# Zigbee2MQTT

The firmware advertises `WSDCGQ01LM-DIY`. The matching external converter is
[`zigbee2mqtt/wsdcgq01lm-diy.mjs`](../zigbee2mqtt/wsdcgq01lm-diy.mjs). Stock
WSDCGQ01LM sensors keep their existing definition and are never matched by it.

The converter exposes temperature, humidity, battery voltage and percentage,
plus the persistent [configuration settings](configuration.md). Measurements use
standard Zigbee attributes; configuration uses the firmware's custom cluster.
Startup binds Power Configuration and reads settings without resetting them.

Install the converter using the [official external-converter guide](https://www.zigbee2mqtt.io/advanced/more/external_converters.html).
Use the Zigbee2MQTT frontend's external-converter editor or your installation's
`external_converters` directory, then reload/restart as described by your version.
Pair the sensor or re-interview it after updating the firmware. Short-press its
button if a sleepy-device request times out. See [configuration](configuration.md)
for calibration, reporting and re-pair behavior.

The converter tests use `zigbee-herdsman-converters` 26.90.0, the API used by the
pilot's Zigbee2MQTT 2.13.0 installation. These are host simulations, including
actual herdsman ZCL frame encoding/decoding; they are not an on-radio validation
of the new settings. The converter has a separate MIT license.

A branch is prepared in [sunbit/zigbee-herdsman-converters](https://github.com/sunbit/zigbee-herdsman-converters/tree/aqara-wsdcgq01lm-diy).
Its modern-extend sources are mirrored in [`zigbee2mqtt/upstream`](../zigbee2mqtt/upstream/)
and tested against converters 26.123.0.
Before proposing upstream support, validate the new firmware on hardware and
adapt the definition to the current upstream conventions. Device definitions
belong in `zigbee-herdsman-converters`, not Zigbee2MQTT core. Follow the official
[support-new-devices guide](https://www.zigbee2mqtt.io/advanced/support-new-devices/01_support_new_devices.html).
