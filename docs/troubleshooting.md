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

These are initial checks, not a reason to repeat the same cell or timing tests.
Once the failure is identified as an address NACK, use the decision plan below.

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

### Power path investigation: decide what a test would change

JP1 VCC and the battery positive contact have not been confirmed to be the same
electrical node on this board. The relationship between their ground contacts,
SHT30 VDD and SHT30 nRESET is also not fully mapped. A programming pad is access
to a PCB connection; its name alone does not prove where it joins the battery
supply. The MCU does not inherently know which connector supplies it.

There are several possible arrangements. Both inputs may be wired directly
together, or one may reach a rail through a protection component, switch or
resistive connection. Supplying a downstream rail can bypass that component.
These are circuit possibilities, not identified components on the LM15-THS3.0.
Fixture pressure can also change an intermittent connection independently of
the electrical input used.

Two manufacturer references explain relevant mechanisms:

- Sensirion documents protection diodes on SCL/SDA and warns that a hard power
  reset must also remove voltage from SDA, SCL and ADDR to avoid powering the
  SHT30 through those pins. This makes unintended signal-path powering a real
  possibility, not proof that it occurs here. See sections 3.2 and 4.9 of the
  [SHT3x-DIS datasheet](https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf).
- TI's [TIDA-01575 reference design, section 2.3.5](https://www.ti.com/lit/ug/tidudk5a/tidudk5a.pdf)
  uses a GPIO-controlled load switch and a software power-up sequence for its
  sensor. This is an example of board-specific initialization that firmware
  must implement, not a schematic or GPIO assignment for this Aqara board.

The current [board configuration](../firmwares/WSDCGQ01LM_DEV/device_config.c)
and [sensor driver](../firmwares/common/src/app_sensor.c) contain no dedicated
SHT30 supply-enable or hardware-reset mapping. If this board needs one, a
firmware correction remains possible. Identify the actual connection and
polarity before implementing it; do not search by driving unknown GPIOs.

**First identify the electrical path.** With the battery removed, the USB and
all other supplies disconnected, compare battery-positive to JP1 VCC and
battery-negative to JP1 GND at their accessible contacts. Record settled
resistance in both probe orientations and compare it with the meter-lead
resistance. A brief continuity beep can come from charging a capacitor; it is
not proof of a direct connection. Even a stable low resistance does not exclude
intermittent contact or a voltage drop under load. Do not short a suspected
protection component or connect two supplies together to investigate it.

If more evidence is needed, compare **one regulated source at the same measured
voltage through the two supply entry points**, after identifying their polarity
and valid voltage. Start at 3.0 V, keep the image and mechanical support the
same, disconnect UART and other fixture signals, and leave the cell out.
Power off before moving the source. Measure at the connected board, not only
the supply display. Record a new diagnostic run for each condition; replayed
records are not new measurements. Keep the same current limit and check that
the source stays in voltage regulation.

| Result | What it narrows down | Next action or possible correction |
| --- | --- | --- |
| Same source/voltage works through JP1 but fails through battery contacts | Entry path, ground return or connection pressure; not source voltage alone | Trace the difference. Repair a demonstrated contact fault, or implement a confirmed switch-enable/reset sequence if it is under MCU control. No blind bypass. |
| Same source/voltage works through both inputs, but a cell fails | Cell/holder behavior or load transients remains plausible | Compare the holder contact interface and supply under load. If failure tracks overlapping loads, test measuring before LED/radio activity and restoring normal operation afterward. Change firmware only if the controlled comparison supports it. |
| Both inputs fail at 3.0 V and work at 3.3 V | Voltage sensitivity is real, but its location and cause are still unknown | Investigate actual sensor VDD, reset and bus levels; an MCU ADC snapshot cannot establish them. Do not impose a 3.3 V battery cutoff as a fix. |
| After establishing the previous row, startup at the lower voltage fails but operation continues when lowered after a successful start | Startup/ramp/reset state rather than a simple steady operating limit | Implement the confirmed sensor power/reset order and restore it after sleep. Test cold starts and wakeups separately. |
| Proper sensor supply/reset is independently established but both hardware and GPIO I2C still NACK | Signal integrity, a remaining transport defect or a component fault | Capture SCL/SDA at verified accessible points and compare against an unmodified board before replacing hardware. A logic analyzer cannot establish analog supply stability. |

Do not run a voltage sweep just to find a threshold. Use the 3.0/3.3 V comparison
only when the same-voltage input comparison has not separated the causes; a
threshold alone does not specify a repair. If all input comparisons fail to
separate the causes, stop adding software combinations and obtain an identified
SHT30 supply/reset observation point or a reference-board comparison.

A `write-address` NACK occurs before the measurement command and conversion
wait in [sensor_wire.c](../firmwares/common/src/sensor_wire.c). Increasing that
wait cannot fix this specific failure. A soft-reset command also requires an
addressable sensor. Matching failures with the SI peripheral and GPIO I2C make
a peripheral-only explanation less likely, but neither proves a damaged sensor.

Before accepting a correction, validate cold battery starts, timed wakeups,
fresh button-triggered readings, both measurement CRCs and received reports.
Then check reporting thresholds and battery consumption using a normal release
image. Keep the 0.1 °C reporting objective; do not mask missing measurements with
cached values or treat the diagnostic image as a battery-life test.

### Further diagnostics

For an automated battery-only investigation without UART wiring, see
[persistent diagnostics](persistent-diagnostics.md). The temporary suite saves
16 checks in nonvolatile memory and sends them through a separate Zigbee/MQTT
diagnostic channel, with later replay available on external power.

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

### Comparing bus speed and line settling

Two optional builds investigate write-address NACKs, changing
one parameter per image. Both require `SENSOR_LED_DIAGNOSTICS=ON` and keep the
same LED codes, 0x2416 measurement command and 20 ms conversion wait.

| `SENSOR_I2C_DIAGNOSTIC_PROFILE` | SCL nominal frequency | Bus settle before START |
| --- | --- | --- |
| `normal` (default) | 100 kHz | 100 µs |
| `slow` | 25 kHz | 100 µs |
| `settle` | 100 kHz | 5 ms |

Use a distinct build directory for each image and record its hash. Add
`-DSENSOR_I2C_DIAGNOSTIC_PROFILE=slow` or `=settle` to the documented CMake
configuration arguments. Normal release builds default to `normal`; CMake
rejects a comparison profile unless LED diagnostics are also enabled.

First try `slow` on the battery without adapter wires. Count the second LED
group after a short press and correlate it with MQTT. If it still produces
code 2, try `settle` separately. Neither image is a proven fix. A changed result
helps isolate timing sensitivity; an unchanged NACK does not alone prove a
power or reset fault. No unknown GPIOs are driven to guess sensor power/reset.

The nominal rates use NXP's 16 MHz peripheral-clock prescaler formula in
[JN-UG-3087, section 29.1](https://www.nxp.com/docs/en/user-guide/JN-UG-3087.pdf).
The longer settle applies after configuring the bus on each command; it is not
an assertion about the board's sensor power-up requirements. These profiles
add diagnostic work and must not be used for battery-life measurements.
