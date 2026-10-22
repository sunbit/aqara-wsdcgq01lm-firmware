# Configuration

The firmware stores its settings in the sensor's non-volatile memory. They survive
battery removal, reboot, firmware updates with the same record format, and the
five-second button re-pair operation.

Use the matching [external converter](../zigbee2mqtt/wsdcgq01lm-diy.mjs). Settings
appear under the device's **Exposes** tab in Zigbee2MQTT and support MQTT `/set`
and `/get`. This converter requires firmware implementing the attributes below.

## Settings

| Zigbee2MQTT property | Default | Allowed range | Unit |
| --- | ---: | ---: | --- |
| `temperature_offset` | 0 | -10 to +10, step 0.01 | °C |
| `humidity_offset` | 0 | -20 to +20, step 0.01 | percentage points RH |
| `temperature_report_delta` | 0.1 | 0.01 to 10, step 0.01 | °C |
| `humidity_report_delta` | 1 | 0.01 to 100, step 0.01 | percentage points RH |
| `measurement_interval` | 60 | 5 to 3600, whole numbers | seconds |
| `temperature_report_interval` | 300 | 5 to 86400, whole numbers | seconds |
| `humidity_report_interval` | 600 | 5 to 86400, whole numbers | seconds |
| `battery_report_interval` | 3600 | 5 to 86400, whole numbers | seconds |
| `temperature_min_report_interval` | 0 | 0 to 3600, whole numbers | seconds |
| `humidity_min_report_interval` | 0 | 0 to 3600, whole numbers | seconds |

Offsets are added to a valid measurement **before** updating the Zigbee attributes
and comparing report thresholds. Humidity is clamped to 0–100% RH and temperature
to the advertised -40–125 °C range. Offsets do not make the sensor more accurate
than its hardware specifications.

The stock device's `temperature_calibration` and `humidity_calibration` options
are corrections applied by Zigbee2MQTT. These remain separate from the new
firmware-backed offsets. Leave those host-side calibration options at zero when
using the sensor offsets, unless you deliberately want both corrections added.

## When reports are sent

- Each measurement compares the corrected value with the last submitted report.
  A difference at least as large as the configured delta sends a new report,
  provided the minimum interval has elapsed.
- The temperature/humidity report intervals are heartbeats. They send unchanged
  readings too and take precedence over the minimum intervals.
- Battery voltage and percentage are measured and sent at the battery interval,
  on join and on a short button press.
- A short press sends fresh temperature, humidity and battery values even when
  they are unchanged. It bypasses the delta/minimum limits and flashes once.
- Changing a setting requests fresh reports. The measurement interval takes
  effect after the already scheduled wake, which may still use the old interval.

Heartbeats are evaluated at measurement times. If an interval is not an exact
multiple of `measurement_interval`, it is reached at the next measurement; e.g.
a 90-second heartbeat with 60-second measurements is sent after about 120 seconds.
Use multiples of the measurement interval for predictable deadlines. An interval
shorter than the measurement interval cannot create extra measurements.

Elapsed-time tracking includes normal awake time and retained-RAM sleep. A manual
refresh restarts that value's report age at the actual refresh time. Radio or I2C
failures can delay delivery; a failed measurement never generates a cached reading.
Discrete measurements cannot guarantee observing a short-lived 0.1 °C excursion
between samples. Smaller deltas, shorter measurement intervals and shorter
heartbeats generally consume more battery.

## Writing settings

For a device named `my_sensor`, publish to `zigbee2mqtt/my_sensor/set`:

```json
{"temperature_offset": -0.25, "humidity_offset": 1.5}
```

For the default temperature behavior:

```json
{"temperature_report_delta": 0.1, "measurement_interval": 60, "temperature_report_interval": 300}
```

Read one setting through `zigbee2mqtt/my_sensor/get`:

```json
{"temperature_offset": ""}
```

This is a sleeping battery device. Send a request and briefly press the original
PCB button to wake it if the request times out. Do not hold the button for five
seconds while configuring it. The converter reads each setting back after a write
and reports an error if the saved value differs. Zigbee2MQTT startup only reads
settings and never overwrites them with defaults.

## Re-pair button

Hold the original PCB button for **5 seconds**. The LED then flashes **3 times**.
After the final flash, the firmware clears the saved Zigbee network, reboots and
starts network steering. Open permit-join on the target coordinator first.
The offsets, reporting settings and factory IEEE address are preserved. A button
still held during reboot is ignored until released, preventing a reset loop.

A coordinator-issued factory reset/leave follows the existing factory-reset path
and clears the configuration as well as network records. Neither operation writes
IEEE/OTP identity registers. A failed LED timer cancels the physical re-pair action
rather than erasing the network without completing the confirmation.

## ZCL interface

Endpoint 1, cluster `0xfc00`, manufacturer-specific header `0x1037` inherited
from the base firmware. This is an experimental interface, not an assigned new
manufacturer identity. Keep the exact `WSDCGQ01LM-DIY` model fingerprint.

| Attribute | Property | ZCL type | Wire unit |
| --- | --- | --- | --- |
| `0x0010` | `temperature_offset` | int16 (`0x29`) | 0.01 °C |
| `0x0011` | `humidity_offset` | int16 (`0x29`) | 0.01% RH |
| `0x0012` | `temperature_report_delta` | uint16 (`0x21`) | 0.01 °C |
| `0x0013` | `humidity_report_delta` | uint16 (`0x21`) | 0.01% RH |
| `0x0014` | `measurement_interval` | uint16 (`0x21`) | seconds |
| `0x0015` | `temperature_report_interval` | uint32 (`0x23`) | seconds |
| `0x0016` | `humidity_report_interval` | uint32 (`0x23`) | seconds |
| `0x0017` | `battery_report_interval` | uint32 (`0x23`) | seconds |
| `0x0018` | `temperature_min_report_interval` | uint16 (`0x21`) | seconds |
| `0x0019` | `humidity_min_report_interval` | uint16 (`0x21`) | seconds |

These settings control application reporting. Standard ZCL Configure Reporting
requests do not change this application policy; use the exposed properties.
Configuration writes are stored only when values change. Invalid ranges are
rejected. The old two-byte configuration record is migrated without changing the
network-format marker, so upgrading compatible firmware preserves pairing.
Unknown/truncated configuration records fall back to defaults.
