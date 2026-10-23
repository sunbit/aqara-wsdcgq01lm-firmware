# Troubleshooting

## Battery reports arrive, but temperature and humidity stop

This was reproduced on an LM15-THS3.0 pilot with the earlier fixed-policy release
build (60-second sampling, 0.1 °C temperature delta, five-minute temperature
heartbeat). The current configurable build has not yet been tested on the board;
do not assume that its additional settings resolve this failure.

Observed behavior:

- With the coin cell, the original button blinked the LED and transmitted battery
  readings, while temperature/humidity stayed at their earlier values.
- Removing and reinserting the cell did not restore fresh measurements.
- With the same firmware, removing the cell and supplying power through JP1 VCC
  and GND restored fresh temperature/humidity. Adapter TX and RX were disconnected.

This confirms that the UART data wires are not required for successful bench
measurements. It does not distinguish a weak cell, battery contact/path issue,
sensor supply/reset problem, or firmware behavior at the cell's supply voltage.
Button-triggered radio traffic does not establish that periodic waking works.

In the installed code, a failed SHT30 transport or CRC check discards the T/H
sample, while the button handler continues with a fresh battery report. This is
consistent with the symptom, but a cluster lookup or report transmission error
must also be distinguished by diagnostics before identifying the cause.

Zigbee2MQTT merges new attributes with cached values. A payload containing T/H
alongside a new battery value does not prove a fresh T/H measurement. Its retained
availability status is also insufficient evidence of continued reports.

### Controlled comparison

1. Keep the same binary and saved network state.
2. Try another known-good CR2032 with all adapter wires disconnected. Record a
   short-button result, then leave the board untouched to check periodic reports.
3. For the external-power comparison, disconnect USB before wiring, remove the
   cell, connect only the verified 3.3 V supply and GND, leave BOOT open, then
   reconnect USB. Never apply both supplies together.
4. Check both a short-button report and unchanged-value temperature heartbeats.
   A brief successful button test does not validate battery life or long-term
   operation.

The [SHT3x-DIS datasheet](https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf)
specifies a 2.15–5.5 V sensor supply. The microcontroller's reported voltage is
not a direct measurement at the SHT30 and does not capture supply dips. Do not
conclude that the sensor requires bench voltage from this observation alone.
Battery percentage is a voltage-based estimate, not a measured charge counter.

If percentage changes while voltage stays at a previous bench-supply value,
check the SDK setup in [building](building.md). A diagnostic build was found to
have omitted the battery-voltage reportable flag in the external SDK. Its LED
transport result remains useful, but its cached MQTT voltage is not evidence of
the actual supply. The build now rejects an SDK missing that flag. Correcting
this build input does not resolve or explain the separate SHT30 address NACK.

### Further diagnostics

If another cell gives the same result, collect the SHT30 transaction stage,
address ACK/NACK, timeout reason, received bytes and both CRC results, together
with cluster lookup/report status. The source already logs transport stages and
CRCs in debug builds; the installed release has no UART output. Additional
diagnostics must preserve the installed behavior so that an unrelated timing or
configuration change does not obscure the comparison.

For passive battery UART observation, use adapter RX connected to board TX and
common GND only, with adapter VCC and TX disconnected. If the fixture prevents
battery insertion, arrange an accessible battery connection first or provide a
diagnostic channel that works on the assembled battery-powered board. A UART
capture taken only on external power cannot explain a battery-only failure.

### Optional LED diagnostic build

Build from this repository with `-DSENSOR_LED_DIAGNOSTICS=ON` and
`-DENABLE_DEBUG=OFF`, using a separate `build-led-diagnostic` directory and the
same external SDK/toolchain arguments as in [building](building.md). Normal
release builds leave this option OFF. This profile does not change I²C speed,
conversion command, conversion wait, bus pullups or recovery policy. It is based
on the current configurable source, rather than an exact copy of the earlier
fixed-policy image; it is a diagnostic candidate, not a validated replacement.

After a **short press and release**, the original 150 ms flash is followed by
one second OFF and a result code. Count the **second group**: each pulse is
300 ms ON, separated by 300 ms OFF. There is no diagnostic group on automatic
samples. Another short press replaces a pending result. Do not hold the button:
the current firmware's five-second re-pair gesture remains active and takes
priority over diagnostics.

| Second-group pulses | Meaning |
| --- | --- |
| 1 | Valid T/H measurement; both forced report submissions accepted locally |
| 2 | SHT30 did not ACK the write address on DIO14/15, address 0x44 |
| 3 | SHT30 did not ACK the read address after the conversion wait on that route |
| 4 | A six-byte reply arrived on that route but at least one CRC failed |
| 5 | Tick-wait or I²C-transfer timeout on that route |
| 6 | Other transport failure on that route, such as command NACK or arbitration loss |
| 7 | Valid measurement, but the T/H application clusters could not be found |
| 8 | Valid measurement, but at least one T/H report submission failed locally |
| 9 | The application does not consider itself joined; no measurement attempted |

Transport codes describe the pilot's confirmed DIO14/15/0x44 route. Failed probes
of alternate routes do not overwrite that result. A valid measurement through
any supported route takes precedence. Code 1 is **not** proof of over-air
delivery; correlate it with received MQTT reports. A code-5 timeout does not
identify whether the sensor supply, bus or MCU timer caused the failure.

The extra flashes keep the CPU awake and consume energy. Do not use this profile
to assess battery consumption. It does not intentionally clear network state or
persist diagnostics. It requires a separate physical flash; compilation and
host tests do not establish that the displayed code is correct on real hardware.
