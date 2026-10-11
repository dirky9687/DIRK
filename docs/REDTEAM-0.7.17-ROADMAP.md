# DIRK 0.7.17 — Red Team expansion, first implementation

Hardware limitations and permission boundaries:
- S3 Wi-Fi radio is 2.4 GHz only. The MonsterC5 radio supplies 5 GHz discovery; no 5 GHz injection or packet capture is claimed.
- Testing and capture require explicit permission from the owner/operator. A saved/authorized flag is an interface safeguard, not legal authorization.
- Preserve C5 scan/CSV repair (v0.7.16 FIX2), GPS, BLE, Wardrive + Buddy XP, and live UI redraw controls.

## Added now

1. **Wi-Fi > Authorized Tests > Passive Posture + SD Report**.
   - Saves an assessment of any saved/authorized BSSID.
   - Up/Down cycles only saved authorized targets, Tab exports to `/dirk/reports/red_posture_<BSSID>.csv`.
   - Findings use observed auth mode, channel, RSSI, WPS and PMF where supported.
   - C5 5 GHz PMF/WPS are treated as UNKNOWN, never assumed safe/unsafe.
   - Does not send frames, associate, or probe the network.
2. **Wi-Fi > Portal / Beacon Lab > Captive Portal Lab**.
   - Optional SD `/dirk/portal/notice.txt` (UTF-8 text, max 1024 bytes) changes the displayed notice.
   - Text is HTML-escaped; no login form, password collection, scripts, or POST endpoint.
   - Auto stops after five minutes or when leaving menu; web server is local softAP only.
   - This is an **AP-hosted demonstration page**, NOT a DNS-captive redirect / Karma attack.

## Already present in 0.7.16 (not rewritten)

- Red Team saved-target authorization flow, bounded 2.4 GHz management-frame tests,
  EAPOL/PMKID monitor and raw single-target PCAP; network tools ping/limited gateway ports;
  4x lab SSID beacon demonstration (5 s), 5 GHz C5 discovery.
- Passive AP scanner, spectrum and beacon IE inspection, probe/deauth monitors, BLE advertisements,
  GPS Wardrive.

## Follow-on, in separately hardware-tested steps

- BLE passive target inventory/export and controlled GATT service enumeration when supported and authorized;
  avoid unknown/unsafe writes, disruptive BLE stress testing, or crash payloads.
- Bounded authenticated LAN service inventory with clear host/port limits and rate caps.
- Lab-only AP template presets and optional safe scripted training portal flows (no credential collection).
- Red Team report summary and ZIP-friendly test case bundles via SD.

## Hardware validation checklist

1. With no authorized targets: posture screen refuses to assess/export.
2. Save and authorize your own AP. Red menu > Passive Posture. Confirm auth + band are accurate; Up/Down changes among authorized saved BSSIDs only.
3. Tab exports CSV. Check `/dirk/reports/red_posture_<BSSID>.csv` on the SD card.
4. Place `/dirk/portal/notice.txt` with sample text containing `<script>` as a literal string. Confirm it renders as text and does not execute.
5. Portal lab auto-stops at 5 minutes or on Back. It does not prompt for passwords.
6. Confirm combined AP scanner correctly shows dual-band indicators; 5G Diagnostics displays names; GPS, Wardrive 2.4/5 and Buddy animate normally.

NOTE: Native regression tests pass, but full PlatformIO and Cardputer hardware compilation/testing must be performed on the target cyberdeck. Keep FIX2 as the known-working rollback build.
