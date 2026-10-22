import * as fz from 'zigbee-herdsman-converters/converters/fromZigbee';
import * as exposes from 'zigbee-herdsman-converters/lib/exposes';
import * as reporting from 'zigbee-herdsman-converters/lib/reporting';
const e = exposes.presets;

// Experimental technical model fingerprint; existing friendly_name/Homie labels
// are preserved independently. Never match the stock lumi.sensor_ht model.
export default {
    zigbeeModel: ['WSDCGQ01LM-DIY'],
    model: 'WSDCGQ01LM-DIY',
    vendor: 'DIY',
    description: 'Experimental WSDCGQ01LM temperature and humidity firmware',
    fromZigbee: [fz.temperature, fz.humidity, fz.battery],
    toZigbee: [],
    exposes: [e.temperature(), e.humidity(), e.battery(), e.battery_voltage()],
    configure: async (device, coordinatorEndpoint) => {
        const endpoint = device.getEndpoint(1);
        // Sensor reports directly to coordinator EP1; binding is needed for
        // inherited battery reporting. Temperature delta is fixed in firmware.
        await reporting.bind(endpoint, coordinatorEndpoint, ['genPowerCfg']);
    },
};
