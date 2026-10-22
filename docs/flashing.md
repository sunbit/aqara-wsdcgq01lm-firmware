# Flashing

These instructions describe the known JN5169 bootloader wiring. Flashing erases the existing firmware and can leave the sensor unusable if power or contacts fail. There is no verified factory image or tested factory rollback for this board.

## Before starting

1. Identify the device and confirm it is the round WSDCGQ01LM with an LM15-THS3.0 board. Do not assume another revision uses the same GPIOs.
2. Use a 3.3 V USB-UART adapter. Confirm its logic levels are 3.3 V; do not use 5 V.
3. Remove the coin cell before connecting adapter VCC.
4. Wire JP1 as shown in [hardware and wiring](hardware.md).
5. Keep the intended firmware image and its SHA-256 checksum together. Never flash a diagnostic or debug image as a release build by mistake.

## Enter the bootloader

1. Close S1 so SPIMISO/DO1 is pulled to GND through 4.7 kΩ.
2. Pulse and release S2 RESET while S1 remains closed.
3. Keep S1 closed while the programmer communicates with the bootloader and writes flash.
4. On a successful write, open S1. Open the serial monitor before pulsing S2 if you are using a debug image.
5. Pulse and release S2 once to start the new firmware.

S1 is a maintained BOOT switch; S2 is a momentary reset switch. The original PCB button does not enter the bootloader.

## Programmer

The development setup used a Python JN51xx bootloader flasher, modified locally to verify the expected IEEE address and firmware hash before writing. That tool is not redistributed in this repository. Obtain a compatible flasher separately and check its license. NXP also provides the JN51xx Production Flash Programmer (JN-SW-4107) for supported Windows environments; consult NXP's documentation for its command syntax.

For a compatible version of the Python flasher, the write command has this form (replace the serial port, IEEE address, checksum, and image path with values for your own sensor):

```sh
python jn51xx_flasher.py \
  --port /dev/cu.usbserial-DEVICE \
  --expected-ieee YOUR_SENSOR_IEEE_ADDRESS \
  --expected-sha256 YOUR_IMAGE_SHA256 \
  write build-release/firmwares/WSDCGQ01LM_DEV.bin
```

Replace `YOUR_SENSOR_IEEE_ADDRESS` with the full IEEE address read from that specific sensor before writing. Calculate the image checksum locally (for example, `shasum -a 256 path/to/image.bin`) and confirm the programmer's options against the version you installed.

Before writing, use the programmer's read-only device identification/configuration function if available. Confirm the JN5169 chip ID and the sensor's factory IEEE address against your own inventory. Never copy the pilot's IEEE address from an example and use it as the expected address for another sensor.

Do not continue if the programmer reports an unexpected chip, IEEE address, protection level, unstable serial response, or checksum mismatch. Do not infer success from a final write status alone; confirm the application starts afterward.

## First boot and Zigbee pairing

The custom firmware starts Zigbee network steering automatically when it boots without saved network state. Open Zigbee2MQTT joining before inserting the battery or resetting the newly flashed sensor. Once joined, network state is saved and later boots try to rejoin automatically. If it misses the join window, reopen joining and reset the sensor to make it try again.

The original PCB button requests a fresh sensor and battery report and blinks the LED. It is not a pairing button.
