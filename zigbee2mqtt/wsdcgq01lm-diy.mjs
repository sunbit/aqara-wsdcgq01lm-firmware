import * as fz from 'zigbee-herdsman-converters/converters/fromZigbee';
import * as exposes from 'zigbee-herdsman-converters/lib/exposes';
import * as reporting from 'zigbee-herdsman-converters/lib/reporting';
const e = exposes.presets;
const cluster = 0xfc00;
const options = {manufacturerCode: 0x1037};
// Offsets are applied in the sensor; these are distinct from Z2M's host-side
// temperature_calibration and humidity_calibration options.
const settings = [
    {name: 'temperature_offset', id: 0x10, type: 0x29, scale: 100, min: -10, max: 10, step: 0.01, unit: '°C', description: 'Temperature offset stored and applied in the sensor'},
    {name: 'humidity_offset', id: 0x11, type: 0x29, scale: 100, min: -20, max: 20, step: 0.01, unit: '%', description: 'Humidity offset stored and applied in the sensor'},
    {name: 'temperature_report_delta', id: 0x12, type: 0x21, scale: 100, min: 0.01, max: 10, step: 0.01, unit: '°C', description: 'Temperature change since the last submitted report'},
    {name: 'humidity_report_delta', id: 0x13, type: 0x21, scale: 100, min: 0.01, max: 100, step: 0.01, unit: '%', description: 'Humidity change since the last submitted report'},
    {name: 'measurement_interval', id: 0x14, type: 0x21, scale: 1, min: 5, max: 3600, step: 1, unit: 's', description: 'Time between measurements; changes apply after the already scheduled wake'},
    {name: 'temperature_report_interval', id: 0x15, type: 0x23, scale: 1, min: 5, max: 86400, step: 1, unit: 's', description: 'Temperature heartbeat; evaluated at measurement times'},
    {name: 'humidity_report_interval', id: 0x16, type: 0x23, scale: 1, min: 5, max: 86400, step: 1, unit: 's', description: 'Humidity heartbeat; evaluated at measurement times'},
    {name: 'battery_report_interval', id: 0x17, type: 0x23, scale: 1, min: 5, max: 86400, step: 1, unit: 's', description: 'Battery report interval; evaluated at measurement times'},
    {name: 'temperature_min_report_interval', id: 0x18, type: 0x21, scale: 1, min: 0, max: 3600, step: 1, unit: 's', description: 'Minimum time between change reports; heartbeat and manual refresh take precedence'},
    {name: 'humidity_min_report_interval', id: 0x19, type: 0x21, scale: 1, min: 0, max: 3600, step: 1, unit: 's', description: 'Minimum time between change reports; heartbeat and manual refresh take precedence'},
];

const configuration = {
    cluster,
    type: ['attributeReport', 'readResponse'],
    convert: (_model, msg) => {
        const state = {};
        for (const setting of settings) {
            if (msg.data[setting.id] !== undefined) state[setting.name] = msg.data[setting.id] / setting.scale;
        }
        return state;
    },
};

const configureSetting = {
    key: settings.map((setting) => setting.name),
    convertSet: async (entity, key, value) => {
        const setting = settings.find((entry) => entry.name === key);
        if (!setting || typeof value !== 'number' || !Number.isFinite(value) || value < setting.min || value > setting.max)
            throw new Error(`Invalid ${key}`);
        const raw = Math.round(value * setting.scale);
        if (Math.abs(value * setting.scale - raw) > 1e-6) throw new Error(`Invalid precision for ${key}`);
        await entity.write(cluster, {[setting.id]: {value: raw, type: setting.type}}, options);
        // The NXP SDK can acknowledge a write even if the persistence callback
        // fails. Firmware rolls back in RAM; confirm the actual persisted value.
        const readback = await entity.read(cluster, [setting.id], options);
        if (readback[setting.id] !== raw) throw new Error(`Sensor did not persist ${key}; read it again`);
        return {state: {[key]: readback[setting.id] / setting.scale}};
    },
    convertGet: async (entity, key) => {
        const setting = settings.find((entry) => entry.name === key);
        if (!setting) throw new Error(`Unknown setting ${key}`);
        await entity.read(cluster, [setting.id], options);
    },
};

export default {
    zigbeeModel: ['WSDCGQ01LM-DIY'],
    model: 'WSDCGQ01LM-DIY',
    vendor: 'DIY',
    description: 'Configurable WSDCGQ01LM temperature and humidity firmware',
    fromZigbee: [fz.temperature, fz.humidity, fz.battery, configuration],
    toZigbee: [configureSetting],
    exposes: [e.temperature(), e.humidity(), e.battery(), e.battery_voltage(),
        ...settings.map((setting) => e.numeric(setting.name, exposes.access.ALL)
            .withUnit(setting.unit).withValueMin(setting.min).withValueMax(setting.max)
            .withValueStep(setting.step).withDescription(setting.description).withCategory('config'))],
    configure: async (device, coordinatorEndpoint) => {
        const endpoint = device.getEndpoint(1);
        await reporting.bind(endpoint, coordinatorEndpoint, ['genPowerCfg']);
        // Read only; never overwrite settings when Zigbee2MQTT restarts.
        // Keep each response below the sleepy end device's APDU capacity.
        for (let start = 0; start < settings.length; start += 4)
            await endpoint.read(cluster, settings.slice(start, start + 4).map((setting) => setting.id), options);
    },
};
