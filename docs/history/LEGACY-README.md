**Current source test:** DIRK v0.8.0 Attack Learning Labs. See `V0.8.0-ATTACK-LEARNING-LABS-NOTES.md` for the new Red Operations labs, limits and build instructions.

# DIRK v0.7.19 - Red & Bluetooth Expansion (SOURCE TEST)

**New:** authorized dual-band Wi-Fi policy comparison with SD CSV export;
read-only BLE advertisement findings and confirmed owned-device SD CSV export;
clearer interpretation of management-frame TX results.

See `V0.7.19-RED-BLE-EXPANSION-NOTES.md` for menus, tests and safeguards.

**NOT hardware-tested here. Keep v0.7.18 FIX1 on SD as rollback.**

---

# DIRK v0.7.19 - Red & Bluetooth Expansion (SOURCE TEST)

**New:** authorized dual-band Wi-Fi policy comparison with SD CSV export;
read-only BLE advertisement findings and confirmed owned-device SD CSV export;
clearer interpretation of management-frame TX results.

See `V0.7.19-RED-BLE-EXPANSION-NOTES.md` for menus, tests and safeguards.

**NOT hardware-tested here. Keep v0.7.18 FIX1 on SD as rollback.**

---

# DIRK v0.7.18 FIX1 — Footer layout correction (SOURCE TEST)

Small display-only correction based on hardware photographs. See `V0.7.18-FIX1-FOOTER-NOTES.txt`. No new tools or radio/protocol changes.

---

# DIRK v0.7.18 — Red Operations UI Pass 2 (SOURCE TEST)

Built on the working v0.7.17 Red Team Pass 1 source. Red Operations are now
organized as a task-oriented hub; Blue Guard, gateway survey, validation and
posture findings have dedicated results/detail views instead of inline results
under menus. No MonsterC5, scan-provider, Wardrive, capture-engine or packet
transmission code was changed. See `V0.7.18-RED-UI-PASS2-NOTES.txt` and
`docs/REDTEAM-0.7.18-UI-ROADMAP.md` for exact changes and hardware checks.

**This ZIP is a source-test build, not compiled or hardware-verified here.**

---

# DIRK v0.7.17 — Red Team Pass 1 (SOURCE TEST)

This release builds on the user-confirmed v0.7.16 FIX2 dual-band behavior.
See `V0.7.17-RED-TEAM-PASS1-NOTES.txt` and `docs/REDTEAM-0.7.17-ROADMAP.md` for additions, limits, tests, and build instructions.
**Compile and test on Cardputer ADV before relying on this build.**

---

# DIRK v0.7.16 — Dual-Band AP Scan Clarity + 5 GHz Verification (SOURCE TEST)

**Base:** user-supplied and working `DIRK-v0.7.15-UNIFIED-AP-STEP2-TEST(1).zip`.
This is a focused diagnostic / display revision. It is not a replacement
MonsterC5 firmware and does not change the JanOS command/CSV parser logic.

## Changes since 0.7.15

1. Wi-Fi > Recon > Dual-band AP Scanner **never presents a radio-band choice**.
   Its menu heading says `AUTO 2.4+5`. Both S3 2.4 GHz and MonsterC5 5 GHz
   scans already start automatically; left/right INSIDE the combined scanner
   only chooses which discovered BSSID to inspect, not what gets scanned.
   The bottom hint has been corrected to reflect that behavior.
2. Removed misleading band choices from Recon > Saved Targets (all bands)
   and Probe Monitor (2.4 GHz only). The old band choice is still available
   for AP Monitor and Signal Tracker, which are genuinely band-specific.
3. TargetManager now remembers an SSID **previously received from a 5 GHz
   BSSID** when that *same* BSSID later arrives as unnamed/`<5G AP>` in a C5
   scan. Previously the real name could be overwritten and disappear from
   the cross-band SSID grouping. No inference is made from the 2.4 GHz AP,
   a different BSSID, or router marketing/specifications. A never-named
   5 GHz AP remains unnamed, so the red indicator is never fabricated.
4. Wi-Fi > 5G Diagnostics now has **5 pages**. Page 5 shows the actual
   5 GHz SSIDs received in the last 60 seconds, including channel and RSSI.
   Use Up/Down to scroll the received C5 networks; `2.4 exact:YES/NO`
   reports whether an equally named 2.4 GHz AP was also seen in the last
   15 seconds. `name len` helps locate unanticipated string differences.
   Tab requests another C5 scan; left/right changes diagnostic pages.
5. UI and Wardrive file headers/version strings identify v0.7.16.

## What was deliberately NOT touched

- `src/providers/c5/JanOSAdapter.h`, `C5CsvApParser.h`, `C5Transport.h`
- `src/providers/s3/S3WifiProvider.h`, BLE, GPS, LoRa and IR providers
- Wardrive runtime algorithm, Buddy character, XP, sound, scan cadence,
  location logging or C5 scanning/timeout protocol
- Red/Blue Team colors, Matrix animation, previous menu organization

**Warning:** A 5 GHz entry lights up red on a unified SSID row only when
MonsterC5 has actually reported that SSID in a recent 5 GHz AP observation.
Seeing 12 other 5 GHz APs does not prove it has found your two specific ones.
If those names aren't on diagnostics page 5, check C5 scan channel coverage
and the actual 5 GHz SSIDs/channel settings on the test routers next; this
revision intentionally does not guess at supported scan channels.

## Compile on Kali Linux

```bash
source /home/USER/.platformio-venv/bin/activate
unzip -o ~/Downloads/DIRK-v0.7.16-DUALBAND-SSID-DIAG-TEST.zip -d ~/Projects/DIRK-v0.7.16
cd ~/Projects/DIRK-v0.7.16
pio run -e cardputer-adv
```

## SD copy (AFTER a successful compile)

Check `lsblk` to confirm `/dev/sda1` refers to your SD card before running.
The existing `Downloads` folder is required and will not be created:

```bash
cd ~/Projects/DIRK-v0.7.16
findmnt /dev/sda1 >/dev/null || udisksctl mount -b /dev/sda1
SDROOT=$(findmnt -n -o TARGET /dev/sda1)
test -n "$SDROOT" && test -d "$SDROOT/Downloads" && test -f .pio/build/cardputer-adv/firmware.bin && cp -v .pio/build/cardputer-adv/firmware.bin "$SDROOT/Downloads/DIRK-v0.7.16.bin" && sync && cmp -s .pio/build/cardputer-adv/firmware.bin "$SDROOT/Downloads/DIRK-v0.7.16.bin" && echo SUCCESS
```

## Host source tests

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Itools/tests/mocks -Iinclude tools/tests/c5_name_retention_test.cpp -o /tmp/c5names && /tmp/c5names
g++ -std=c++11 -Wall -Wextra -Werror -Itools/tests/mocks -Iinclude tools/tests/unified_ap_view_test.cpp -o /tmp/aptest && /tmp/aptest
g++ -std=c++11 -Wall -Wextra -Werror tools/test_c5_csv_parser.cpp -o /tmp/csvtest && /tmp/csvtest
python3 tools/tests/v0716_ap_recon_static.py
python3 tools/tests/unified_scanner_static.py
python3 tools/tests/menu_nav_static.py
python3 tools/tests/wardrive_end_static.py
```

## Hardware test sequence

1. Verify scanner opens in AUTO 2.4+5 and Left/Right only changes BSSID
   inspect preference. Tab should request both band scans.
2. Wait at least 25 seconds near both known dual-band routers; confirm no
   Wardrive menu/tank regression after leaving the scanner.
3. Note each known SSID and whether both band indicators go red.
4. Navigate Wi-Fi > 5G Diagnostics, press Right four times to reach **5/5**.
   Use Up/Down through all C5-received names and look for the two known SSIDs.
   Take pictures of page 5 for each one, or note if each is entirely missing.
5. If present, record channel, RSSI, `2.4 exact` and `name len` for each one.
   Compare the name with the 2.4 GHz row exactly. If absent, check the router's
   actual 5 GHz SSID and channel (especially DFS) before changing the parser.
6. Confirm both 5 GHz diagnostic pages showing UART scan counts and CSV rows
   remain healthy; check GPS and Buddy XP on a test Wardrive session.

Native tests were executed in the packaging environment. **A PlatformIO
firmware compile and Cardputer ADV hardware/RF test were not performed here**
(PlatformIO dependencies are not installed). Preserve v0.7.15 as the known
working firmware while testing v0.7.16.
