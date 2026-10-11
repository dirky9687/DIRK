"""Offline source-regression checks (not a PlatformIO or hardware test)."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
ui=(root/'src/ui/Ui.h').read_text()
wd=(root/'src/wardrive/WardriveManager.h').read_text()
wf=(root/'src/providers/s3/S3WifiProvider.h').read_text()
ble=(root/'src/providers/ble/BleProvider.h').read_text()
tests=[
 ('menu has End Session', '"end session"' in ui),
 ('menu uses 8 rows', 'N=8' in ui and 'i<8' in ui),
 ('menu item calls stop', 'wdMenuSel==1 && wardrive && wardrive->running()' in ui and 'wardrive->stop();' in ui),
 ('menu stop clears buddy tone', ui.index('cancelBuddyTone();\n        wardrive->stop();')>=0),
 ('back preserves session', 'if(screen>=22 && screen<=27){ enterScreen(3); return; }' in ui),
 ('home background running marker', 'WD BG ACTIVE' in ui),
 ('wardrive stop cancels wifi', 'wifi->cancelWardriveScan(); wifi->setWardriveMode(false)' in wd),
 ('wardrive stop cancels BLE', 'ble->cancelWardriveScan()' in wd),
 ('wardrive stop cancels C5 scan', 'janos->stop()' in wd),
 ('wardrive stop drains xp', 'while(xpQueueCount_) processXpQueue(true);' in wd),
 ('wardrive stop persists ledger', 'xpLedger_.flushAll()' in wd),
 ('wardrive stop finalizes SD', 'flushLogs(true)' in wd and 'rebuildWigle(); writeStats();' in wd),
 ('wardrive stop ends active state', 'active=false;' in wd),
 ('s3 scan cancelled before wardrive flag cleared',wd.index('cancelWardriveScan();')<wd.index('wifi->setWardriveMode(false);',wd.index('void stop()'))),
 ('s3 cancels ESP32 async scan', 'esp_wifi_scan_stop();' in wf),
 ('s3 drops stale results and becomes ready', 'resultReady=false;' in wf[wf.index('void cancelWardriveScan()'):wf.index('void setWardriveMode')] and 'state=Availability::Ready;' in wf[wf.index('void cancelWardriveScan()'):wf.index('void setWardriveMode')]),
 ('BLE stops active scan', 'scanner->stop();' in ble and 'scanner->clearResults();' in ble),
]
for name,ok in tests: print(('PASS' if ok else 'FAIL')+': '+name)
if not all(x[1] for x in tests):raise SystemExit('source check failed')
print(f'ALL {len(tests)} source checks passed (hardware behavior untested)')
