# Hardware and wiring

## Tested board

The pilot is a round **WSDCGQ01LM**, PCB marking **LM15-THS3.0**, with an NXP **JN5169** and a Sensirion **SHT30**. Other PCB revisions have not been checked. Confirm the board marking and continuity before applying power or copying the GPIO assignments below.

## Confirmed signal assignments

| Function | JN5169 signal | Evidence |
|---|---:|---|
| Original PCB button | DIO5, active low | Repeated pressed/released transitions observed on the pilot |
| Original LED | DIO9, active low | Three low pulses visibly lit the pilot LED |
| SHT30 I²C clock | DIO14 | Valid SHT30 response on the pilot |
| SHT30 I²C data | DIO15 | Valid SHT30 response on the pilot |
| Boot strap | SPIMISO / DO1 | Board bootloader entry circuit; not a user GPIO |
| Reset | RESETN | Active-low hardware reset |

Do not reuse DIO5, DIO9, DIO14, or DIO15 for other functions. The firmware releases UART0 RTS/CTS so UART0 does not take control of the DIO5 button pin.

## JP1 and USB-UART wiring

The labels below are the board's JP1 signal names. Check your board orientation; do not infer pad order from a photograph.

| Aqara board | USB-UART adapter | Notes |
|---|---|---|
| JP1.1 TXD0 | RX | Sensor UART output to adapter input |
| JP1.2 RXD0 | TX | Adapter output to sensor UART input |
| JP1.3 SPIMISO / DO1 | GND through 4.7 kΩ and S1 switch | Close S1 to enter/hold bootloader mode; open it for normal firmware operation |
| JP1.4 VCC | 3.3 V | Only if powering from the adapter; never connect 5 V |
| JP1.5 RESETN | GND through momentary S2 button | Press and release S2 to reset |
| JP1.6 GND | GND | Common ground |

Adapter DTR and CTS are unused. The 4.7 kΩ resistor and S1 are in series between SPIMISO/DO1 and ground. S2 is a momentary reset switch; it is not the original PCB button.

```text
USB-UART                         Aqara JP1
RX  <--------------------------- JP1.1 TXD0
TX  ---------------------------> JP1.2 RXD0
                                  JP1.3 SPIMISO/DO1 -- 4.7 kΩ -- S1 -- GND
3.3 V -------------------------- JP1.4 VCC   (bench power only)
GND ---------------------------- JP1.6 GND
                                  JP1.5 RESETN -- momentary S2 -- GND

DTR: unused
CTS: unused
```

Do not power the board from both the adapter and a battery at the same time. Remove the battery whenever JP1.4 is powered from the adapter. For battery operation, disconnect adapter VCC and adapter TX; an adapter RX connection can be used for passive UART output if required. The release firmware has no UART logging.

## 3D-printed pogo fixture

A printed clamp can hold spring contacts or fixed pogo pins against JP1 pads, with short Dupont leads to the adapter. Make the fixture keyed so it cannot be placed backwards. Keep the BOOT and RESET switches accessible, insulate adjacent contacts, and avoid applying force to the sensor, crystal, or fine-pitch IC pins. Verify each contact against GND and the board labels before powering it.
