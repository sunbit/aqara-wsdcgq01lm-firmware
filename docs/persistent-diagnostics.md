# Persistent battery diagnostics

This optional, temporary image runs a fixed set of 16 SHT30 checks after one
short press of the original button. It saves the completed results in the
sensor's nonvolatile memory, then sends them over Zigbee. If radio delivery
fails on the battery, the saved results can be recovered later on external
power without repeating the measurements.

The implementation has host-test coverage and cross-compiles for JN5169 in
release and debug profiles. Physical operation of this suite remains unverified.
It is a diagnostic tool, not a fix or a battery-life test. It deliberately
pauses normal temperature/humidity reporting and disables the five-second
re-pair gesture. Restore a normal firmware image after collecting results.

## What it checks

The baseline uses DIO14 SCL, DIO15 SDA, address `0x44`, internal pullups,
100 kHz nominal SCL, 100 µs line settling, low repeatability (`0x2416`) and a
20 ms conversion wait. Each row below changes the stated setting relative to
that baseline. The tests run in this order; recovery commands can affect later
tests, so this is not a set of independent laboratory experiments.

| Case | Check |
| --- | --- |
| 1 | Baseline, LED off |
| 2 | Baseline, original LED on during the voltage snapshot and measurement |
| 3 | 50 kHz nominal SCL |
| 4 | 25 kHz nominal SCL |
| 5 | 12.5 kHz nominal SCL |
| 6 | 5 ms line settling |
| 7 | 20 ms line settling |
| 8 | MCU internal I²C pullups off |
| 9 | Address `0x45` on DIO14/15 |
| 10 | Documented alternate I²C route DIO16/17, address `0x44` |
| 11 | DIO16/17, address `0x45` |
| 12 | Nine released-high/actively-low SCL clocks, then baseline measurement |
| 13 | SHT30 soft reset `0x30a2`, 2 ms wait, then baseline measurement |
| 14 | Medium repeatability (`0x240b`) |
| 15 | High repeatability (`0x2400`) |
| 16 | Final baseline, LED off |

Each result records the transaction stage, failure reason, idle SCL/SDA levels,
six received bytes, both CRC results, actual selected settings and an MCU
supply-voltage snapshot. It also records whether a requested interface clear or
soft reset completed. A successful measurement does not imply that its preceding
recovery command succeeded; inspect both fields.

These are representative checks, not every possible combination. Unknown sensor
power/reset GPIOs are never driven. The ADC snapshot measures the MCU supply,
not SHT30 VDD, and does not capture transient voltage dips. If every known route
still NACKs, software cannot establish sensor power, reset or PCB continuity.

The commands and nominal clock formula follow the
[Sensirion SHT3x-DIS datasheet](https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf)
and [NXP JN516x peripherals guide](https://www.nxp.com/docs/en/user-guide/JN-UG-3087.pdf).

## Build

Use the external SDK/toolchain setup in [building](building.md), including the
battery-voltage attribute fix, and a separate build directory:

```sh
cmake -S . -B build-persistent \
  -DCMAKE_TOOLCHAIN_FILE=toolchain/CMakeToolchain.cmake \
  -DTARGET=WSDCGQ01LM_DEV -DENABLE_DEBUG=OFF \
  -DSENSOR_PERSISTENT_DIAGNOSTICS=ON
cmake --build build-persistent -j1
```

Keep `SENSOR_LED_DIAGNOSTICS=OFF` and
`SENSOR_I2C_DIAGNOSTIC_PROFILE=normal`; the suite selects its own settings.
The image is `build-persistent/firmwares/WSDCGQ01LM_DEV.bin`. Build a separate
debug image with `ENABLE_DEBUG=ON` if UART output is needed.

## Receive results without changing normal measurements

Keep the normal `WSDCGQ01LM-DIY` converter installed. Add
[`persistent-diagnostics.mjs`](../zigbee2mqtt/diagnostics/persistent-diagnostics.mjs)
as an **external extension**, following the
[official Zigbee2MQTT guide](https://www.zigbee2mqtt.io/advanced/more/external_extensions.html).
It listens passively for the diagnostic cluster and does not read, write, bind,
reconfigure or control the sensor. It does not replace the normal converter.

The extension publishes nonretained QoS 1 JSON at:

```text
<base_topic>/diagnostics/wsdcgq01lm/<URL-encoded friendly_name>
```

The payload contains `diagnostic_run_id` and a JSON string `diagnostic_result`.
That string includes the raw `packet_hex`, decoded result and persistence flags.
No normal entity-state publication is made: cached temperature/humidity fields
must not be presented as fresh telemetry by diagnostic traffic. Check any
installation-specific wildcard subscribers before loading the extension.
It is tested against the Zigbee2MQTT 2.13.0 extension API in host simulations;
live reception still requires validation. The temporary unknown cluster may
also appear in Zigbee2MQTT debug logs.

Start a passive subscription to the exact diagnostic topic **before** the test.
Capture one JSON object per line as `{"topic":"...","payload":{...}}`.
Once collected, assemble and verify it locally:

```sh
python3 scripts/collect-diagnostics.py \
  --input capture.jsonl --output diagnostic-results.json \
  --topic 'zigbee2mqtt/diagnostics/wsdcgq01lm/your-device'
```

The collector never connects to a broker or sends commands. It groups by topic
and run ID, tolerates duplicates/reordering, lists missing cases and recomputes
the checksum of the complete saved record. `complete: true` means all 16 results
match the summary checksum; it does not mean every sensor test passed.

## Battery test and later recovery

1. Save the normal firmware image and its checksum. Flash the diagnostic image
   using the [normal wiring and flashing procedure](flashing.md). Preserve the
   existing network state; no re-pair is needed for an already joined sensor.
2. Disconnect the fixture and **all adapter wires** before inserting the cell.
   Never connect the cell and adapter supply at the same time. Leave the board
   battery-powered for 30 seconds before starting the test.
3. Short-press and release the original PCB button **once**. The normal feedback
   blink confirms the press. The suite starts after 500 ms, runs each case with
   a 250 ms gap and saves the completed lot. Leave it untouched for at least
   30 seconds, and preferably three minutes for the repeated radio bursts.
   The test's LED-on case is not a numerical blink code.
4. Check the receiver. There are three report rounds: summary, 16 cases, summary.
   Packets are spaced 500 ms apart; later rounds begin on scheduled wakeups.
   Local submission retries and repeated rounds improve delivery but do not
   guarantee reception. `persisted: true` requires a successful save and readback.
5. If reception failed, remove the cell, connect only the verified external
   3.3 V supply and GND, leave BOOT open and boot the **same diagnostic image**.
   Do **not** press the original button. It automatically reloads and replays
   the saved battery results, marked `restored_from_memory: true`. It sends no
   new SHT30 commands during that replay. UART wires are unnecessary.
6. A new short press deliberately starts a new run and replaces the previous
   saved lot only after completion. Use this for a separate external-power
   comparison after the battery lot has been captured. Keep the two captures.
7. Restore the normal firmware and remove only this diagnostic extension when
   finished. Keep other extensions, the normal converter and saved network
   settings intact. Do not clear retained device/discovery topics.

The suite is also able to collect results before joining; transmission waits
until the application is joined. A device without saved network state may need
normal joining arrangements. This image's long press cannot erase a network.

## Storage and protocol

A complete lot occupies 272 bytes: a 16-byte versioned header and 16 rows of
16 bytes. It uses separate PDM record `0x000b`; configuration and network records
are not intentionally erased. There is one save operation per completed run,
followed by readback comparison, rather than a save after each test. Do not
repeatedly run this image to assess battery endurance or EEPROM lifetime.

Interrupted tests are not saved or labelled complete. The previous completed
record remains available after a reboot unless a later save failed or storage
was erased/damaged. A failed save/readback can still produce RAM results with
`persisted: false`; those results are not guaranteed to survive power loss.

Endpoint 1 sends attribute `0x0000` of private cluster `0xfc01` as a 24-byte ZCL
octet string. The attribute uses a regular ZCL header because the SDK's report
helper does not support manufacturer-specific attributes. Packet bytes are:

| Bytes | Meaning |
| --- | --- |
| 0–1 | Marker `d1`, protocol version `01` |
| 2–5 | Run ID, little-endian |
| 6 | Case index 0–15, or `ff` for a summary |
| 7 | Bits 0: saved/readback verified; 1: suite finished; 2: restored from PDM |
| 8–23 | Result row; summary instead contains case count at 8 and checksum at 12–15 |

The saved record uses the FNV-1a 32-bit checksum with its checksum field zeroed
during calculation. This detects incomplete/corrupted captures; it is not a
cryptographic authenticity mechanism. Network security remains Zigbee's normal
network security. Run IDs increment from the latest saved run, so clearing
storage or losing an unsaved run can reuse an ID; keep captures separate across
storage resets.

See [`diagnostic_suite.c`](../firmwares/common/src/diagnostic_suite.c) for the
byte layout and fixed case table, and the receiver for decoded field names.
Normal firmware does not use this record. A programmer that explicitly erases
PDM/EEPROM can remove it; preserve captured results before reflashing.
