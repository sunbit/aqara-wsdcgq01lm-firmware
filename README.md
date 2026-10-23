# Aqara WSDCGQ01LM 0.1 °C firmware

Experimental firmware for the round Aqara WSDCGQ01LM temperature and humidity sensor with the NXP JN5169 and Sensirion SHT30. It reports temperature changes at 0.1 °C resolution, keeps the original PCB button useful, and blinks the original LED when the button is pressed.

The included Zigbee2MQTT converter matches **custom `WSDCGQ01LM-DIY` firmware only**. Unmodified WSDCGQ01LM sensors continue to use their stock device definition.

## Status

The reporting path, short-button readings and LED feedback have been confirmed on a pilot sensor. Persistent configuration and the five-second re-pair gesture have host-test coverage; their physical validation is still pending. Battery-powered runtime and long-term stability also need verification, so treat the project as experimental.

A battery-only measurement failure was reproduced on the pilot's earlier fixed-policy build: button presses still sent battery reports, but fresh temperature/humidity returned only with external power. The current configurable build has not yet been physically tested. See [battery troubleshooting](docs/troubleshooting.md) before deploying to more sensors.

By default, the current firmware samples once per minute, reports temperature changes of 0.1 °C and sends a temperature heartbeat every five minutes. Offsets, change thresholds, measurement frequency and report intervals are configurable and saved in the sensor. Hold the original button for five seconds to re-pair, confirmed by three LED flashes. See [configuration](docs/configuration.md) for all settings and [development](docs/development.md) for test coverage and remaining physical checks. Discrete samples cannot guarantee observing every short-lived temperature excursion.

## Hardware

See [hardware and wiring](docs/hardware.md) for the observed board revision, GPIO assignments, UART wiring, boot switch, reset button, and power notes.

## Flashing

See [flashing instructions](docs/flashing.md). Flashing requires a 3.3 V USB-UART adapter, physical access to the board pads, and a JN5169-compatible programmer. This repository does not include a programmer binary, the NXP SDK, or the compiler toolchain.

An optional [persistent diagnostic image](docs/persistent-diagnostics.md) runs
16 battery tests, saves the completed lot and reports it over a separate
Zigbee/MQTT channel. It is a temporary investigation tool with physical
validation pending; normal firmware behavior is unchanged by default.

## Build

The application source and CMake project are included. Obtain the NXP JN516x ZigBee 3.0 SDK and the BA2/JN51xx compiler toolchain separately, under their own terms, then install them locally as described in [building](docs/building.md). Do not commit those third-party files or generated build products.

## Zigbee2MQTT

The optional external converter is in [`zigbee2mqtt/`](zigbee2mqtt/). It recognizes the distinct model string `WSDCGQ01LM-DIY` and decodes standard temperature, humidity, and battery reports. It has been tested with Zigbee2MQTT 2.13.0 and `zigbee-herdsman-converters` 26.90.0. See [Zigbee2MQTT support](docs/zigbee2mqtt.md).

## License and third-party code

The firmware application is derived from [`mgavryliuk/zcf-jn5169-ed-switches`](https://github.com/mgavryliuk/zcf-jn5169-ed-switches), pinned at commit `0bc0fbbc437e749aed406a6acbf388383313f2e5`, and is distributed under GPL-3.0 as shown in [LICENSE.md](LICENSE.md). Original copyright and license notices in source files are retained. The Zigbee2MQTT converter has a separate MIT license in [`zigbee2mqtt/LICENSE`](zigbee2mqtt/LICENSE).

NXP SDK source, libraries, configuration tools, and the BA2 compiler are not included. NXP notices in the source remain in force; users must obtain and use NXP materials under NXP's terms. The separate Python flasher used during development is also not included here. See [third-party notices](THIRD_PARTY_NOTICES.md).

## Contributions

Please include the exact board revision, firmware version, build settings, Zigbee2MQTT version, and whether a report came from a battery-powered sensor or a bench supply. Do not post IEEE addresses, broker credentials, private MQTT topics, or personal installation details.
