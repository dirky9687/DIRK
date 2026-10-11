# DIRK 0.7.18 Red Operations — UI audit and follow-ups

## Current tested source changes

Red Operations is a task hub rather than a repeated list of features already
accessible elsewhere. Results are separate from action selection. Screens 66–70
are read-only detail/report views except for rerunning the existing bounded
port scan and refreshing the existing Blue Guard comparison.

## Design rules

- Navigation: Back returns to the logical parent, not to an unrelated menu.
- Results: do not mix scan status with action menu rows.
- Display details: show observed evidence and limitations; avoid fabricated
  vulnerability conclusions and false claims of 5GHz security coverage.
- Rate: only update live counters while required (1-second capture count
  refresh) and avoid redraws of unrelated animations.
- Hardware: do not change the MonsterC5 UART protocol, 5GHz firmware/parser,
  wardrive engine, or proven asynchronous ESP32 scan logic.

## Potential follow-up features, subject to hardware scope / authorization

1. Read-only AP capability inspector: richer present/unknown 802.11 IEs,
   stronger 2.4/5 GHz provenance, and per-BSSID detail with SD export.
2. Authorized BLE GATT discovery with explicit device selection and consent,
   read-only descriptors/characteristics and protective timeouts.
3. Authorized local gateway diagnostics: latency, DNS lookup and narrow
   service identification with clear limits and zero authentication attempts.
4. Passive management-frame analytics with per-channel counters and
   comparison against a stored local baseline.
5. Scoped export workflow: named assessment sessions linking authorized
   targets, passive evidence, results, timestamps and SD CSV/JSON output.
6. Separate lab portal customization workflow with strict no-secret forms,
   local-only operation and automatic timeout.

Do not represent all these as implemented in v0.7.18.
