# Firmware source of truth

All future work on this firmware belongs in this repository, including source,
Zigbee2MQTT support, tests, build tooling and documentation. Do not maintain a
second editable firmware implementation in the parent home-automation project.

Keep public documentation in English and describe the current firmware without
internal experiment/version labels. Preserve third-party notices. Do not commit
NXP SDK files, compiler binaries, generated firmware or local installation data.

Run the host tests and cross-compile changes when the local SDK/toolchain are
available. Distinguish simulated tests, successful builds and physical validation.
Do not flash hardware, change a live Zigbee2MQTT installation or send live MQTT
commands as part of a software test without the user's explicit instruction.
