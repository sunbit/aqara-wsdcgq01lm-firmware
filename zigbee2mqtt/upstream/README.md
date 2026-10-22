# Prepared upstream integration

These MIT-licensed sources mirror the work branch in
[sunbit/zigbee-herdsman-converters](https://github.com/sunbit/zigbee-herdsman-converters/tree/aqara-wsdcgq01lm-diy).
They keep the integration work alongside the firmware's source of truth.

In a matching converter checkout, copy:

- `aqara_diy.ts` to `src/devices/aqara_diy.ts`.
- `aqara_diy.test.ts` to `test/aqara_diy.test.ts`.

Apply `register.patch` to register the new module in `src/devices/index.ts`.
Do not also keep a duplicate `WSDCGQ01LM-DIY` definition in `lumi.ts`.

The device uses modern extends, with custom read-after-write verification for the
firmware's persistent settings. It matches only `WSDCGQ01LM-DIY`. The branch is
based on converters 26.123.0; the standalone external converter remains compatible
with the pilot's 26.90.0 dependency. Hardware validation is still pending.

Before a PR, run the fork's checks/build/tests and review its current
[AI policy](https://github.com/Koenkk/zigbee-herdsman-converters/blob/master/AI_POLICY.md).
The policy requires maintainer permission before posting an AI-assisted PR,
meaningful human review and disclosure. No upstream PR has been opened.
