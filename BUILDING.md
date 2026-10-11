# Building DIRK v0.8.0

## Requirements
- PlatformIO Core or PlatformIO IDE
- ESP32 development toolchain
- M5Stack Cardputer ADV

## Firmware Build

Clone the repository and enter the project directory.

git clone https://github.com/dirky9687/DIRK.git
cd DIRK

Build using:

pio run -e cardputer-adv

Compiled firmware location:

.pio/build/cardputer-adv/firmware.bin

## Dependencies

PlatformIO installs the libraries declared in platformio.ini.

- M5Unified
- M5Cardputer
- TinyGPSPlus
- RadioLib

The build uses platformio/espressif32@6.11.0
and the m5stack-stamps3 board definition.

## Using Modified TinyGPSPlus

To experiment with a modified TinyGPSPlus library:

1. Make a separate working copy of DIRK.
2. Download the TinyGPSPlus source code.
3. Place the modified library in lib/TinyGPSPlus/.
4. Remove mikalhart/TinyGPSPlus from lib_deps
   in your working copy of platformio.ini.
5. Clean and rebuild:

pio run -e cardputer-adv -t clean
pio run -e cardputer-adv

Verify the dependency selection and test the
resulting firmware on compatible hardware.

## Licensing

See LICENSE, THIRD_PARTY_NOTICES.md, and
licenses/LGPL-2.1.txt.

Consult the upstream library licenses and
redistribution requirements before distributing
modified firmware binaries.
