const names = ['baseline_led_off', 'baseline_led_on', 'bus_50khz', 'bus_25khz',
    'bus_12_5khz', 'settle_5ms', 'settle_20ms', 'internal_pullups_off',
    'address_45', 'route_16_17_address_44', 'route_16_17_address_45',
    'interface_clear', 'soft_reset', 'medium_repeatability',
    'high_repeatability', 'final_baseline'];
const stages = ['bus-settle', 'write-address', 'command-msb', 'command-lsb',
    'command-wait', 'read-address', 'read-data', 'complete'];
const faults = ['none', 'nack', 'tick-timeout', 'transfer-timeout', 'arbitration-lost', 'invalid-command'];
function crc(bytes) {
    let value = 0xff;
    for (const byte of bytes) {
        value ^= byte;
        for (let n = 0; n < 8; ++n) value = ((value << 1) ^ ((value & 0x80) ? 0x31 : 0)) & 0xff;
    }
    return value;
}
export function decodeDiagnosticPacket(value) {
    if (!(Buffer.isBuffer(value) || value instanceof Uint8Array || Array.isArray(value))) return null;
    const b = Buffer.from(value);
    if (b.length !== 24 || b[0] !== 0xd1 || b[1] !== 1 || !(b[6] < 16 || b[6] === 255)) return null;
    const common = {protocol: 1, run_id: b.readUInt32LE(2), persisted: !!(b[7] & 1),
        suite_finished: !!(b[7] & 2), restored_from_memory: !!(b[7] & 4), packet_hex: b.toString('hex')};
    if (b[6] === 255) {
        if (b[8] !== 16) return null;
        return {...common, type: 'summary', case_count: b[8], checksum: b.readUInt32LE(12)};
    }
    const row = b.subarray(8), flags = row[3], raw = row.subarray(6, 12);
    if (row[0] !== b[6] + 1) return null;
    const received = !!(flags & 1);
    const crcT = received && raw[2] === crc(raw.subarray(0, 2));
    const crcH = received && raw[5] === crc(raw.subarray(3, 5));
    const crcFlagsMatch = crcT === !!(flags & 2) && crcH === !!(flags & 4);
    return {...common, type: 'case', case_id: row[0], name: names[b[6]],
        stage: stages[row[1]] ?? 'unknown', fault: faults[row[2]] ?? 'unknown',
        received, crc_temperature_ok: crcT, crc_humidity_ok: crcH, crc_flags_match: crcFlagsMatch,
        passed: received && crcT && crcH && crcFlagsMatch,
        idle_scl: !!(flags & 8), idle_sda: !!(flags & 16),
        control_ok: (row[12] & 12) ? !!(flags & 32) : null,
        mcu_voltage_mv: (flags & 64) ? row.readUInt16LE(4) : null,
        led_load: !!(flags & 128), route: (row[12] & 128) ? [16, 17] : [14, 15],
        address: row[15], nominal_bus_hz: 3200000 / (row[14] + 1), settle_us: row[13] * 100,
        raw_hex: raw.toString('hex'),
        temperature: crcT && crcFlagsMatch ? -45 + 175 * raw.readUInt16BE(0) / 65535 : null,
        humidity: crcH && crcFlagsMatch ? 100 * raw.readUInt16BE(3) / 65535 : null};
}

// External extension: publish outside the ordinary entity-state path. In
// particular, never merge cached environmental values into diagnostic traffic.
export default class PersistentDiagnostics {
    constructor(zigbee, mqtt, _state, _publishEntityState, eventBus) {
        Object.assign(this, {mqtt, eventBus});
    }
    start() {this.eventBus.onDeviceMessage(this, (data) => this.receive(data));}
    stop() {this.eventBus.removeListeners(this);}
    async receive(data) {
        if (![0xfc01, '64513'].includes(data.cluster) || !['attributeReport', 'readResponse'].includes(data.type) ||
            data.device?.zh?.modelID !== 'WSDCGQ01LM-DIY') return;
        const result = decodeDiagnosticPacket(data.data?.[0]);
        const name = data.device.name ?? data.device.ieeeAddr;
        if (!result || typeof name !== 'string' || !name) return;
        await this.mqtt.publish(`diagnostics/wsdcgq01lm/${encodeURIComponent(name)}`,
            JSON.stringify({diagnostic_run_id: result.run_id, diagnostic_result: JSON.stringify(result)}),
            {clientOptions: {retain: false, qos: 1}});
    }
}
