# Third-party notices

## Firmware base

The firmware application is based on `mgavryliuk/zcf-jn5169-ed-switches`, commit `0bc0fbbc437e749aed406a6acbf388383313f2e5`, licensed under GPL-3.0. The upstream license is included as `LICENSE.md`; original source notices are retained.

## NXP JN516x SDK and tools

The NXP JN-SW-4170 SDK and JN-SW-4141 toolchain are not redistributed in this repository. They are required to build the firmware and must be obtained separately from [NXP's JN516x ZigBee 3.0 page](https://www.nxp.com/pages/jn516x-7x-zigbee-3-0:ZIGBEE-3-0). NXP's release notes identify the supported chips and required toolchain: [JN-RN-0059](https://www.nxp.com/docs/en/supporting-information/JN-RN-0059.pdf).

Several application files originate from NXP sample applications and retain NXP copyright and license headers. Those notices are not replaced by this project's GPL notice. This repository does not grant rights to redistribute NXP SDK files, libraries, or tools. Review NXP's applicable terms before downloading, modifying, building, or redistributing firmware images.

The build used one local SDK source change in `PowerConfiguration.c`: the battery voltage attribute access flag was changed from `E_ZCL_AF_RD` to `E_ZCL_AF_RD|E_ZCL_AF_RP` so the firmware can report a fresh voltage value. Apply this change only to your own SDK installation, retain its NXP notice, and keep a backup of the original file.

## BA2 compiler toolchain

The BA2 compiler binaries are not included. The development build used the OpenLumi BA2 toolchain; obtain it from [OpenLumi's BA2-toolchain releases](https://github.com/openlumi/BA2-toolchain/releases) and follow that project's license and installation instructions.

## JN51xx flasher

The modified `jn51xx_flasher.py` used for the pilot is not included. Its upstream source is [`grafalex82/jn51xx_flasher`](https://github.com/grafalex82/jn51xx_flasher). Check that project's current licensing before redistribution. NXP also publishes the JN51xx Production Flash Programmer (JN-SW-4107) for supported development environments.

## Zigbee2MQTT converter

The converter in `zigbee2mqtt/` is project code and is separately licensed under MIT; see `zigbee2mqtt/LICENSE`. The MIT license of `zigbee-herdsman-converters` does not automatically apply to this project, so the explicit license is included for possible upstream contribution.
