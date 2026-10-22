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
