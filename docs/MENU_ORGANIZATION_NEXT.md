# DIRK menu organization proposal (planning only)

Keep the home screen fast and familiar. Proposed structure (not implemented
in 0.7.12; awaiting user review):

- Wi-Fi: Recon (2.4 / 5), Spectrum/Signal, Saved Targets, Diagnostics, Team tools.
- Bluetooth: Discoveries, Analyzer, Defensive Monitor, Team tools.
- Wardrive: Start/Resume, End Session, Live View, Discoveries, Map, Dossier/XP,
  Providers, Buddy Sound, GPS Diagnostics.
- Mesh/LoRa: Chat, Nearby, Contacts, Networks/Channels, Diagnostics.
- Data: Captures, Reports/Audits, Wardrive Sessions, SD Files.
- Device: Theme/Team, Power, Peripherals, Firmware/About.

Principles:
- One-line labels that fit within the left 116px of the 240px menu.
- Keep high-frequency actions in the top two positions.
- Don't shuffle keyboard shortcuts before publishing a navigation map.
- Don't modify protected v0.7.11 C5, GPS, Wi-Fi, BLE, or XP source just to
  reorganize UI menus.
