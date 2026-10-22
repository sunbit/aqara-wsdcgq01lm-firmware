# Building

This repository contains application sources, not a complete toolchain. Build products depend on the NXP JN516x SDK and a BA2 compiler. These third-party components are deliberately excluded.

## Required components

- NXP **JN-SW-4170**, ZigBee 3.0 SDK for JN516x.
- A compatible BA2/JN51xx toolchain. The development build used the OpenLumi `BA2-toolchain` release from 2020-12-19.
- CMake 3.28.1 or later and Python 3.

Obtain the NXP SDK from the [NXP JN516x ZigBee page](https://www.nxp.com/pages/jn516x-7x-zigbee-3-0:ZIGBEE-3-0) under NXP's terms. Obtain the compiler from the [OpenLumi BA2-toolchain releases](https://github.com/openlumi/BA2-toolchain/releases) under its license. The SDK and compiler are not bundled here.

## Local setup

1. Install or extract the SDK into a local `sdk/` directory at the repository root. Do not commit it; `sdk/` is ignored by Git.
2. Extract the BA2 toolchain into `toolchain/` so that `toolchain/bin/ba-elf-gcc` exists. Do not commit the compiler binaries.
3. In your local copy of `sdk/Components/ZCL/Clusters/General/Source/PowerConfiguration.c`, change the battery voltage access flags from `E_ZCL_AF_RD` to `E_ZCL_AF_RD|E_ZCL_AF_RP`. The upstream SDK declares battery voltage read-only; this firmware also reports that attribute. Keep the file's NXP notice and make a backup before editing.
4. Configure and build the release:

```sh
cmake -S . -B build-release \
  -DCMAKE_TOOLCHAIN_FILE=toolchain/CMakeToolchain.cmake \
  -DTARGET=WSDCGQ01LM_DEV \
  -DENABLE_DEBUG=OFF
cmake --build build-release -j1
```

The image is written to `build-release/firmwares/WSDCGQ01LM_DEV.bin`. Keep your build log, compiler/SDK versions, and SHA-256 checksum with any firmware you share. A successful local build does not validate operation on a physical sensor.
