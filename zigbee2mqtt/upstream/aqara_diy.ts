import type {Zcl} from "zigbee-herdsman";
import * as m from "../lib/modernExtend";
import type {DefinitionWithExtend, ModernExtend} from "../lib/types";
import {assertEndpoint, assertNumber} from "../lib/utils";

const cluster = 0xfc00;
const options = {manufacturerCode: 0x1037};
interface FirmwareSetting {
    name: string;
    id: number;
    type: Zcl.DataType;
    scale: number;
    min: number;
    max: number;
    step: number;
    unit: string;
    description: string;
}
const settings: FirmwareSetting[] = [
    {
        name: "temperature_offset",
        id: 0x10,
        type: 0x29,
        scale: 100,
        min: -10,
        max: 10,
        step: 0.01,
        unit: "°C",
        description: "Temperature offset stored and applied in the sensor",
    },
    {
        name: "humidity_offset",
        id: 0x11,
        type: 0x29,
        scale: 100,
        min: -20,
        max: 20,
        step: 0.01,
        unit: "%",
        description: "Humidity offset stored and applied in the sensor",
    },
    {
        name: "temperature_report_delta",
        id: 0x12,
        type: 0x21,
        scale: 100,
        min: 0.01,
        max: 10,
        step: 0.01,
        unit: "°C",
        description: "Temperature change since the last submitted report",
    },
    {
        name: "humidity_report_delta",
        id: 0x13,
        type: 0x21,
        scale: 100,
        min: 0.01,
        max: 100,
        step: 0.01,
        unit: "%",
        description: "Humidity change since the last submitted report",
    },
    {
        name: "measurement_interval",
        id: 0x14,
        type: 0x21,
        scale: 1,
        min: 5,
        max: 3600,
        step: 1,
        unit: "s",
        description: "Time between measurements; changes apply after the already scheduled wake",
    },
    {
        name: "temperature_report_interval",
        id: 0x15,
        type: 0x23,
        scale: 1,
        min: 5,
        max: 86400,
        step: 1,
        unit: "s",
        description: "Temperature heartbeat; evaluated at measurement times",
    },
    {
        name: "humidity_report_interval",
        id: 0x16,
        type: 0x23,
        scale: 1,
        min: 5,
        max: 86400,
        step: 1,
        unit: "s",
        description: "Humidity heartbeat; evaluated at measurement times",
    },
    {
        name: "battery_report_interval",
        id: 0x17,
        type: 0x23,
        scale: 1,
        min: 5,
        max: 86400,
        step: 1,
        unit: "s",
        description: "Battery report interval; evaluated at measurement times",
    },
    {
        name: "temperature_min_report_interval",
        id: 0x18,
        type: 0x21,
        scale: 1,
        min: 0,
        max: 3600,
        step: 1,
        unit: "s",
        description: "Minimum time between change reports; heartbeat and manual refresh take precedence",
    },
    {
        name: "humidity_min_report_interval",
        id: 0x19,
        type: 0x21,
        scale: 1,
        min: 0,
        max: 3600,
        step: 1,
        unit: "s",
        description: "Minimum time between change reports; heartbeat and manual refresh take precedence",
    },
];

function firmwareSetting(setting: FirmwareSetting): ModernExtend {
    const extend = m.numeric({
        name: setting.name,
        cluster,
        attribute: {ID: setting.id, type: setting.type},
        description: setting.description,
        scale: setting.scale,
        valueMin: setting.min,
        valueMax: setting.max,
        valueStep: setting.step,
        unit: setting.unit,
        access: "ALL",
        reporting: false,
        entityCategory: "config",
        zigbeeCommandOptions: options,
    });
    // Batch startup reads with the manufacturer header in the device configure.
    extend.configure = [];
    const converter = extend.toZigbee[0];
    converter.convertSet = async (entity, key, value) => {
        assertEndpoint(entity);
        assertNumber(value, key);
        if (!Number.isFinite(value) || value < setting.min || value > setting.max) throw new Error(`Invalid ${key}`);
        const raw = Math.round(value * setting.scale);
        if (Math.abs(value * setting.scale - raw) > 1e-6) throw new Error(`Invalid precision for ${key}`);
        await entity.write(cluster, {[setting.id]: {value: raw, type: setting.type}}, options);
        // Firmware rolls back in RAM on a PDM save failure. The NXP SDK may
        // still acknowledge the write, so confirm persistence by reading back.
        const readback = await entity.read(cluster, [setting.id], options);
        const persisted = readback[setting.id];
        if (persisted !== raw) throw new Error(`Sensor did not persist ${key}; read it again`);
        assertNumber(persisted, key);
        return {state: {[key]: persisted / setting.scale}};
    };
    return extend;
}

export const definitions: DefinitionWithExtend[] = [
    {
        zigbeeModel: ["WSDCGQ01LM-DIY"],
        model: "WSDCGQ01LM-DIY",
        vendor: "DIY",
        description: "Configurable WSDCGQ01LM temperature and humidity firmware",
        extend: [
            m.temperature({reporting: false}),
            m.humidity({reporting: false}),
            m.battery({voltage: true, percentageReporting: false, voltageReporting: false}),
            ...settings.map(firmwareSetting),
        ],
        configure: async (device, coordinatorEndpoint) => {
            const endpoint = device.getEndpoint(1);
            await endpoint.bind("genPowerCfg", coordinatorEndpoint);
            for (let start = 0; start < settings.length; start += 4) {
                await endpoint.read(
                    cluster,
                    settings.slice(start, start + 4).map((setting) => setting.id),
                    options,
                );
            }
        },
    },
];
