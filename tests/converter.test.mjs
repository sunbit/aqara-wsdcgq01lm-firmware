import test from 'node:test';
import assert from 'node:assert/strict';
import definition from '../zigbee2mqtt/wsdcgq01lm-diy.mjs';
const config = definition.fromZigbee.find((entry) => entry.cluster === 0xfc00);
const control = definition.toZigbee[0];
const options = {manufacturerCode: 0x1037};

test('exact custom model and distinct firmware-backed calibration controls', () => {
    assert.deepEqual(definition.zigbeeModel, ['WSDCGQ01LM-DIY']);
    assert.equal(definition.exposes.length, 14);
    const temperature = definition.exposes.find((entry) => entry.property === 'temperature_offset');
    assert.equal(temperature.access, 7);
    assert.equal(temperature.value_min, -10);
    assert.equal(temperature.value_max, 10);
    assert.equal(temperature.value_step, 0.01);
});
test('read-response decoding preserves signed offsets and scales centi units', () => {
    assert.deepEqual(config.convert(null, {data: {16: -125, 17: 250, 18: 10, 20: 60, 21: 300}}), {
        temperature_offset: -1.25, humidity_offset: 2.5, temperature_report_delta: 0.1,
        measurement_interval: 60, temperature_report_interval: 300,
    });
    assert.deepEqual(config.convert(null, {data: {65535: 42}}), {});
});
test('all settings write correct ZCL types and verify the saved value', async () => {
    const cases = [
        ['temperature_offset', -1.25, 16, -125, 0x29], ['humidity_offset', 2.5, 17, 250, 0x29],
        ['temperature_report_delta', 0.1, 18, 10, 0x21], ['humidity_report_delta', 1, 19, 100, 0x21],
        ['measurement_interval', 120, 20, 120, 0x21], ['temperature_report_interval', 600, 21, 600, 0x23],
        ['humidity_report_interval', 900, 22, 900, 0x23], ['battery_report_interval', 7200, 23, 7200, 0x23],
        ['temperature_min_report_interval', 0, 24, 0, 0x21], ['humidity_min_report_interval', 30, 25, 30, 0x21],
    ];
    assert.deepEqual(control.key, cases.map(([key]) => key));
    for (const [key, value, id, raw, type] of cases) {
        let written = false;
        const entity = {
            write: async (cluster, attrs, actualOptions) => {
                assert.equal(cluster, 0xfc00); assert.deepEqual(actualOptions, options);
                assert.deepEqual(attrs, {[id]: {value: raw, type}}); written = true;
            },
            read: async (cluster, ids, actualOptions) => {
                assert(written); assert.equal(cluster, 0xfc00); assert.deepEqual(ids, [id]);
                assert.deepEqual(actualOptions, options); return {[id]: raw};
            },
        };
        assert.deepEqual(await control.convertSet(entity, key, value), {state: {[key]: value}});
    }
});
test('invalid input never writes; an acknowledged but unsaved write is detected', async () => {
    let writes = 0;
    const entity = {write: async () => {++writes;}, read: async () => ({20: 60})};
    for (const value of [0, 3601, 5.5, NaN, Infinity, '60'])
        await assert.rejects(control.convertSet(entity, 'measurement_interval', value));
    await assert.rejects(control.convertSet(entity, 'unknown', 1));
    assert.equal(writes, 0);
    await assert.rejects(control.convertSet(entity, 'measurement_interval', 120), /did not persist/);
    assert.equal(writes, 1);
});
test('configure only binds/reads; restart does not reset saved preferences', async () => {
    const batches = [], coordinator = {};
    const endpoint = {
        bind: async (cluster, target) => {assert.equal(cluster, 'genPowerCfg'); assert.equal(target, coordinator);},
        read: async (cluster, ids, actualOptions) => {
            assert.equal(cluster, 0xfc00); assert.deepEqual(actualOptions, options);
            assert(ids.length <= 4); batches.push(ids);
        },
    };
    await definition.configure({getEndpoint: (id) => {assert.equal(id, 1); return endpoint;}}, coordinator);
    assert.deepEqual(batches.flat(), Array.from({length: 10}, (_, index) => index + 16));
    await control.convertGet({read: async (cluster, ids) => {assert.equal(cluster, 0xfc00); assert.deepEqual(ids, [16]);}}, 'temperature_offset');
});

test('real herdsman ZCL codec handles manufacturer-specific signed/32-bit settings', async () => {
    const {createRequire} = await import('node:module');
    const require = createRequire(import.meta.url);
    const packageRequire = createRequire(require.resolve('zigbee-herdsman-converters'));
    const {Zcl} = packageRequire('zigbee-herdsman');
    for (const [id, type, value] of [[16, 0x29, -125], [18, 0x21, 10], [21, 0x23, 86400]]) {
        const payload = [{attrId: id, dataType: type, attrData: value}];
        const frame = Zcl.Frame.create(0, 0, true, 0x1037, 1, 'write', 0xfc00, payload, {});
        const buffer = frame.toBuffer();
        const decoded = Zcl.Frame.fromBuffer(0xfc00, Zcl.Header.fromBuffer(buffer), buffer, {});
        assert.equal(decoded.header.manufacturerCode, 0x1037);
        assert.deepEqual(decoded.payload, payload);
        const response = Zcl.Frame.create(0, 1, true, 0x1037, 2, 'readRsp', 0xfc00,
            [{...payload[0], status: 0}], {});
        const responseBuffer = response.toBuffer();
        assert.deepEqual(Zcl.Frame.fromBuffer(0xfc00, Zcl.Header.fromBuffer(responseBuffer), responseBuffer, {}).payload,
            [{...payload[0], status: 0}]);
    }
});
