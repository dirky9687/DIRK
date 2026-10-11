DIRK — Red & Blue Team Firmware

Dual-Band Reconnaissance | Wireless Security | Field Operations

DIRK is an open-source wireless reconnaissance and security-testing firmware project developed for the M5Stack Cardputer ADV.

Designed around Red Team and Blue Team operations, DIRK combines dual-band Wi-Fi discovery, Bluetooth analysis, LoRa, infrared, GPS wardriving, and hands-on cybersecurity learning labs in one portable platform.

DIRK takes its name from the double-edged blade, representing both offensive security testing and defensive reconnaissance.

Current Release: v0.8.0 Beta

---

Features

Wi-Fi Reconnaissance

- 2.4 GHz scanning using the Cardputer ADV's ESP32-S3
- 5 GHz discovery through the LAB5 MonsterC5 module
- Unified dual-band SSID scanner
- Signal strength, channel, and security inspection
- Access point analysis and network monitoring

Red Team Operations

- Authorized wireless security-testing tools
- AP security posture assessments
- WPA2/WPA3 configuration analysis
- EAPOL and PMKID monitoring
- Controlled beacon and portal laboratories
- Attack Learning Labs with interactive educational exercises
- Exportable findings and reports

Blue Team Operations

- Wireless environment monitoring
- Baseline comparison and change detection
- Suspicious access point analysis
- Security configuration assessment
- Passive RF monitoring

Bluetooth

- BLE device discovery
- Advertisement and service identification
- Device information and security assessment
- Authorized-device analysis
- Exportable assessment reports

GPS & Wardriving

- GPS integration with the official M5Stack LoRa/GPS Cap
- Dual-band Wi-Fi wardriving
- Network discovery and location logging
- Wardrive progression and animated Buddy companion
- Experience points and unlockable ranks

LoRa & Infrared

- LoRa tools supporting compatible hardware
- Infrared utility and testing features
- Portable field-oriented interface

Availability of individual tools depends on connected hardware, firmware configuration, and radio capabilities.

---

Hardware

Main Device

- M5Stack Cardputer ADV

Expansion Hardware

- LAB5 MonsterC5 — 5 GHz Wi-Fi discovery
- Official M5Stack LoRa/GPS Cap — GPS and compatible LoRa functionality
- MicroSD card — Firmware loading, logs, and exports

The ESP32-S3 provides 2.4 GHz Wi-Fi. The MonsterC5 expansion provides additional 5 GHz discovery capabilities.

---

Installation

DIRK supports firmware installation through a compatible Cardputer firmware launcher such as M5Launcher.

1. Download the compiled DIRK firmware ".bin" from GitHub Releases when available.
2. Copy the BIN file to your microSD card.
3. Insert the card into your Cardputer ADV.
4. Open M5Launcher.
5. Select the DIRK firmware file and flash it.
6. Restart and explore DIRK.

Always use firmware built for the correct hardware model.

---

Source Code

DIRK is intended to be an open-source, community-driven project.

The complete PlatformIO source archive is available in this repository for developers interested in studying, modifying, compiling, or improving the firmware.

The source will also be organized into directly browsable repository folders as the public release is finalized.

Development

DIRK uses the PlatformIO build environment for ESP32 development.

Build target:

"cardputer-adv"

---

Project Status

DIRK v0.8.0 Beta

DIRK is under active development. Some features remain experimental, and not all hardware combinations have been independently tested.

Bug reports, constructive feedback, testing results, and code contributions are welcome.

---

Responsible Use

DIRK is intended for cybersecurity education, wireless research, network diagnostics, and authorized penetration testing.

Use active security-testing features only on devices and networks you own or have explicit permission to assess.

Users are responsible for observing applicable laws, radio regulations, and authorization requirements.

---

License

DIRK is being prepared for release under the GNU General Public License v3.0 or later (GPL-3.0-or-later), subject to final third-party licensing review.

See the project's license documentation for details.

---

DIRK — Two Edges. One Mission.

Reconnaissance. Research. Defense.
