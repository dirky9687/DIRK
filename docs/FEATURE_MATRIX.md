# DIRK v0.7.0 Beta Feature Matrix

| Area | State |
|---|---|
| Cardputer ADV boot/display | Implemented |
| Double-buffered Blue/Red UI | Implemented |
| TCA8418 keyboard navigation | Implemented / hardware tested lineage |
| Main-menu hotkeys | Implemented |
| 2.4 GHz Wi-Fi async scan + sync fallback | Implemented (FIX8 lineage) |
| MonsterC5 5 GHz transport | Implemented; availability requires C5 traffic |
| Passive Wi-Fi AP metadata | Implemented |
| Beacon IE inspection: PMF/WPS/cipher/AKM/country/DTIM/PHY/WMM | Implemented for 2.4 GHz passive inspect |
| Blue Guard baseline | Implemented on SD |
| Blue Guard change alerts | Implemented; new/missing/security/channel/PMF/SSID/new-BSSID heuristic |
| Channel spectrum / probe / deauth-disassoc passive monitors | Implemented |
| BLE passive explorer | Implemented |
| BLE advertisement analyzer | Implemented; TX power/manufacturer/appearance/service/iBeacon/Eddystone/mesh tags |
| GPS UART + fix parsing | Implemented |
| GPS UTC clock + timezone/DST | Implemented |
| Battery percentage header | Implemented |
| Wardrive + SD sessions | Implemented |
| LoRa / DIRK Mesh | Implemented |
| Meshtastic / MeshCore discovery/contact framework | Implemented companion discovery path |
| Cardputer ADV built-in IR TX (GPIO44) | Implemented |
| NEC IR transmit | Implemented |
| Flipper-style `.ir` NEC/raw replay | Implemented for first supported signal in file |
| Built-in IR receive | Not available in Cardputer ADV hardware; external RX required |
| SD storage | Implemented |
| PCAP writer | Implemented core |
| Active capture authorization gate | Implemented |
| Bounded Red-mode single-target Wi-Fi validation | Implemented |
| Lab portal | Implemented non-secret test portal |
