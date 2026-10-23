import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtempSync, readFileSync, writeFileSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
import {createRequire} from 'node:module';
import Extension, {decodeDiagnosticPacket} from '../zigbee2mqtt/diagnostics/persistent-diagnostics.mjs';
const root = fileURLToPath(new URL('../', import.meta.url));
const require = createRequire(import.meta.url);
const packageRequire = createRequire(require.resolve('zigbee-herdsman-converters'));
const {Zcl} = packageRequire('zigbee-herdsman');

function execute(command, args, options = {}) {
    const result = spawnSync(command, args, {encoding: 'utf8', ...options});
    assert.equal(result.status, 0, result.stderr || result.error?.message);
    return result.stdout;
}
function fixture(folder) {
    const binary = join(folder, 'fixture');
    execute('cc', ['-std=c99', '-Wall', '-Wextra', '-Werror', '-I'+join(root,'firmwares/common/include'),
        join(root,'firmwares/common/src/diagnostic_suite.c'), join(root,'firmwares/common/src/sensor_math.c'),
        join(root,'tests/diagnostic_fixture.c'), '-o', binary]);
    return execute(binary, []).trim().split('\n').map((hex) => Buffer.from(hex, 'hex'));
}

test('actual C records survive ZCL reports, isolated extension publication and passive assembly', async () => {
    const folder = mkdtempSync(join(tmpdir(), 'aqara-diagnostic-codec-'));
    try {
        const packets = fixture(folder);
        const lines = [];
        let listener, removed = false;
        const mqtt = {publish: async (topic, payload, options) => {
            assert.equal(topic, 'diagnostics/wsdcgq01lm/test%2Fdevice');
            assert.deepEqual(options, {clientOptions: {retain: false, qos: 1}});
            lines.push(JSON.stringify({topic: 'test/device', payload: JSON.parse(payload)}));
        }};
        const eventBus = {onDeviceMessage: (_owner, callback) => {listener=callback;},
            removeListeners: () => {removed=true;}};
        const extension = new Extension(null, mqtt, null,
            () => {throw new Error('Must not publish normal entity state');}, eventBus);
        extension.start();
        for (const [index, packet] of packets.entries()) {
            const frame = Zcl.Frame.create(0, 1, true, undefined, index, 'report', 0xfc01,
                [{attrId: 0, dataType: 0x41, attrData: packet}], {});
            const bytes = frame.toBuffer();
            const decoded = Zcl.Frame.fromBuffer(0xfc01, Zcl.Header.fromBuffer(bytes), bytes, {});
            // Herdsman emits frame.cluster.name for parsed ZCL messages. For
            // an unknown cluster that is the decimal string, not its numeric ID.
            assert.equal(decoded.cluster.name, '64513');
            await listener({cluster: decoded.cluster.name, type: 'attributeReport',
                device: {name: 'test/device', zh: {modelID: 'WSDCGQ01LM-DIY'}},
                data: {0: decoded.payload[0].attrData}});
        }
        assert.equal(lines.length, packets.length);
        // Retain compatibility with callers that supply the numeric ID.
        await listener({cluster: 0xfc01, type: 'attributeReport',
            device: {name: 'test/device', zh: {modelID: 'WSDCGQ01LM-DIY'}},
            data: {0: packets[0]}});
        assert.equal(lines.length, packets.length + 1);
        const count = lines.length;
        await listener({cluster: 0xfc01, type: 'attributeReport', device: {zh: {modelID: 'stock'}}, data: {0: packets[0]}});
        await listener({cluster: 'msTemperatureMeasurement', type: 'attributeReport', data: {}});
        assert.equal(lines.length, count);
        extension.stop(); assert(removed);
        const input = join(folder,'capture.jsonl'), output = join(folder,'result.json');
        writeFileSync(input, lines.join('\n'));
        execute('python3',[join(root,'scripts/collect-diagnostics.py'),'--input',input,'--output',output,'--topic','test/device']);
        const result = JSON.parse(readFileSync(output,'utf8'));
        assert.equal(result.rejected_lines,0);
        assert.equal(result.runs.length,1);
        const run = result.runs[0];
        assert(run.complete && run.checksum_matches && run.persisted_observed && run.replay_observed);
        assert.deepEqual(run.missing_case_ids,[]);
        assert.deepEqual(run.cases.map((entry) => entry.case_id),Array.from({length:16},(_,n)=>n+1));
        assert(run.cases.every((entry) => entry.passed && entry.mcu_voltage_mv===2800));
        assert.equal(run.cases[1].led_load,true);
        assert.equal(run.cases[4].nominal_bus_hz,12500);
        assert.deepEqual(run.cases[9].route,[16,17]);
        assert(Math.abs(run.cases[0].temperature-25)<0.01);
        assert(lines.every((line) => !Object.hasOwn(JSON.parse(line).payload, 'temperature')));
    } finally {rmSync(folder,{recursive:true,force:true});}
});

test('bad packet shape/version/CRC flags never produce normal environmental values', () => {
    const folder=mkdtempSync(join(tmpdir(),'aqara-diagnostic-invalid-'));
    try {
        const packets=fixture(folder), packet=Buffer.from(packets[1]);
        assert.equal(decodeDiagnosticPacket(packet.subarray(0,23)),null);
        assert.equal(decodeDiagnosticPacket('not bytes'),null);
        packet[1]=2;assert.equal(decodeDiagnosticPacket(packet),null);packet[1]=1;
        packet[14]^=1; // Corrupt raw temperature word while the device flags claim valid CRC.
        const result=decodeDiagnosticPacket(packet);
        assert.equal(result.crc_flags_match,false);
        assert.equal(result.passed,false);
        assert.equal(result.temperature,null);
    } finally {rmSync(folder,{recursive:true,force:true});}
});

test('collector reports incomplete/tampered batches and keeps device topics separate', () => {
    const folder=mkdtempSync(join(tmpdir(),'aqara-diagnostic-partial-'));
    try {
        const packets=fixture(folder);
        const lines=packets.map((packet)=>JSON.stringify({topic:'test/a',payload:{diagnostic_result:JSON.stringify(decodeDiagnosticPacket(packet))}}));
        // Remove a case, keeping the summary and other duplicated messages.
        const incomplete=packets.filter((p)=>p[6]!==3).map((p)=>JSON.stringify({topic:'test/a',payload:{diagnostic_result:JSON.stringify(decodeDiagnosticPacket(p))}}));
        const corrupted=packets.map((p)=>Buffer.from(p));
        const changed=corrupted.find((p)=>p[6]===3);changed[12]^=1;
        const tampered=corrupted.map((p)=>JSON.stringify({topic:'test/b',payload:{diagnostic_result:JSON.stringify(decodeDiagnosticPacket(p))}}));
        const input=join(folder,'capture.jsonl'), output=join(folder,'result.json');
        writeFileSync(input,[...incomplete,...tampered,...lines.map((l)=>l.replace('test/a','test/c'))].join('\n'));
        execute('python3',[join(root,'scripts/collect-diagnostics.py'),'--input',input,'--output',output]);
        const {runs}=JSON.parse(readFileSync(output,'utf8'));
        assert.equal(runs.length,3);
        assert.equal(runs.find((r)=>r.topic==='test/a').complete,false);
        assert.deepEqual(runs.find((r)=>r.topic==='test/a').missing_case_ids,[4]);
        assert.equal(runs.find((r)=>r.topic==='test/b').checksum_matches,false);
        assert.equal(runs.find((r)=>r.topic==='test/c').complete,true);
    } finally {rmSync(folder,{recursive:true,force:true});}
});
