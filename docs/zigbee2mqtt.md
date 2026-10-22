# Zigbee2MQTT

The firmware advertises the Basic-cluster model string `WSDCGQ01LM-DIY`. The external converter in `../zigbee2mqtt/wsdcgq01lm-diy.mjs` matches that exact string, so stock WSDCGQ01LM sensors continue to use their existing definition.

The converter decodes standard temperature, humidity, and battery reports. It also binds the Power Configuration cluster to the coordinator for battery reporting. Temperature resolution and the 0.1 °C report threshold are implemented in the firmware, not in Zigbee2MQTT.

To try the converter, copy it into Zigbee2MQTT's `external_converters` directory and enable external JavaScript converters in the configuration. Restart Zigbee2MQTT and pair a sensor running this custom firmware. External converters are intended for development and testing; see the [official Zigbee2MQTT guide](https://www.zigbee2mqtt.io/advanced/more/external_converters.html).

The current file uses the converter API tested against Zigbee2MQTT 2.13.0 and `zigbee-herdsman-converters` 26.90.0. Before submitting it upstream, update it to the modern `extend` API expected by the current `zigbee-herdsman-converters` project, settle an MIT-compatible redistribution license for this original converter code, then follow Zigbee2MQTT's [support-new-devices guide](https://www.zigbee2mqtt.io/advanced/support-new-devices/01_support_new_devices.html). Device definitions belong in `zigbee-herdsman-converters`, not in the Zigbee2MQTT core repository.
