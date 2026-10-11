# Hardware validation notes

Target: M5Stack Cardputer ADV using PlatformIO `m5stack-stamps3`.

GPS/LoRa Cap:
- GPS UART2: RX 15, TX 13, 115200.
- GPIO5 is held HIGH as LoRa NSS while DIRK is using GPS/SD paths to reduce bus contention.

MonsterC5:
- Transport is isolated from JanOS semantics.
- Boot performs passive UART detection only.
- No undocumented active command is transmitted automatically.
- 5 GHz capability is not advertised merely because Serial1 was opened.

Keyboard:
- Navigation first checks standard HID usage codes in `KeysState::hid_keys`.
- Printable fallback mappings are also retained for ADV firmware/library differences.
