import {describe, expect, test, vi} from "vitest";
import {definitions} from "../src/devices/aqara_diy";
import {findByDevice} from "../src/index";
import type {Definition, Tz} from "../src/lib/types";
import {mockDevice} from "./utils";

function sensor() {
    return mockDevice({
        modelID: "WSDCGQ01LM-DIY",
        manufacturerID: 0x1037,
        endpoints: [{ID: 1, inputClusterIDs: [0, 1, 0x0402, 0x0405, 0xfc00]}],
    });
}

describe("WSDCGQ01LM custom firmware", () => {
    test("matches only the custom model and exposes firmware configuration", async () => {
        const definition = (await findByDevice(sensor())) as Definition;
        expect(definition.model).toBe("WSDCGQ01LM-DIY");
        expect(definitions[0].zigbeeModel).toEqual(["WSDCGQ01LM-DIY"]);
        expect(definition.toZigbee.some((converter) => converter.key.includes("temperature_offset"))).toBe(true);
        expect(definition.toZigbee.some((converter) => converter.key.includes("battery_report_interval"))).toBe(true);
    });

    test("validates signed offsets and confirms saved settings", async () => {
        const device = sensor();
        const endpoint = device.getEndpoint(1);
        const definition = (await findByDevice(device)) as Definition;
        const converter = definition.toZigbee.find((entry) => entry.key.includes("temperature_offset"));
        endpoint.write = vi.fn().mockResolvedValue(undefined);
        endpoint.read = vi.fn().mockResolvedValue({16: -125});
        const result = await converter.convertSet(endpoint, "temperature_offset", -1.25, {} as Tz.Meta);
        expect(endpoint.write).toHaveBeenCalledWith(0xfc00, {16: {value: -125, type: 0x29}}, {manufacturerCode: 0x1037});
        expect(endpoint.read).toHaveBeenCalledWith(0xfc00, [16], {manufacturerCode: 0x1037});
        expect(result).toEqual({state: {temperature_offset: -1.25}});
        await expect(converter.convertSet(endpoint, "temperature_offset", -10.01, {} as Tz.Meta)).rejects.toThrow("Invalid");
        await expect(converter.convertSet(endpoint, "temperature_offset", 0.001, {} as Tz.Meta)).rejects.toThrow("precision");
        endpoint.read = vi.fn().mockResolvedValue({16: 0});
        await expect(converter.convertSet(endpoint, "temperature_offset", 1, {} as Tz.Meta)).rejects.toThrow("did not persist");
    });
});
