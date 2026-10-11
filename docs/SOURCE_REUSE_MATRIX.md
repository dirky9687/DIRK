# Source / Reuse Matrix

DIRK is open source. Before public release, retain upstream notices required by each dependency/source.

- M5Stack M5Cardputer / M5Unified: Cardputer ADV display and TCA8418 keyboard API reference.
- M5Stack / Adafruit TCA8418: keyboard controller behavior.
- M5 Launcher: Cardputer ADV navigation and device-management behavior reference.
- C5Lab MonsterC5 / JanOS ecosystem: MonsterC5 architecture and UART integration reference.
- Cardputer ADV GPS projects: LoRa/GPS Cap UART and bus-contention behavior reference.
- TinyGPSPlus: NMEA parsing dependency.
- Arduino-ESP32: Wi-Fi, Preferences, WebServer, time/NTP and SD APIs.

No claim is made that mere attribution replaces license obligations. Preserve required license texts/notices for any code copied or substantially adapted before publishing DIRK.

## v0.3.0 ADV input / UI implementation references
- M5Stack M5Cardputer (MIT): physical 4x14 key map and Cardputer ADV TCA8418 architecture.
- bmorcelli/Launcher (MIT): Cardputer ADV TCA8418 address/pins, raw-event remap formula, and proven navigation convention (`; , . /`, Enter, backtick/Backspace). DIRK implements its own compact polling reader rather than copying Launcher's application framework.
- C5Lab/M5MonsterC5-CardputerADV + projectZero/JanOS (MIT): architectural reference for the Cardputer app + ESP32-C5 companion split. DIRK does not claim 5 GHz capability until C5 UART activity is observed.
- GeneralDussDuss/poseidon: visual/interaction reference only for keyboard-first firmware and ambient themed motion; no source copied into DIRK UI.
