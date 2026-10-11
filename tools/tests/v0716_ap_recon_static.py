"""v0.7.16 source guards for the AP scanner and independent C5 diagnostics.
These do NOT replace a PlatformIO compile or real dual-band RF testing.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
ui=(root/'src/ui/Ui.h').read_text()
tm=(root/'src/targets/TargetManager.h').read_text()
app=(root/'src/core/DirkApp.cpp').read_text()
wd=(root/'src/wardrive/WardriveManager.h').read_text()
assert 'if(screen==12){' in ui and 'wifi->startScanAsync(300,0)' in ui and 'janos->startScan5()' in ui
assert 'AUTO 2.4+5' in ui and '(wifiSubSel==2 || wifiSubSel==3)' in ui
assert 'diag5Count(now)' in ui and 'diag5Exact24Match' in ui
assert 'diag5Page==4' in ui and '%5u' in ui
assert 'strcmp(t->ssid,other->ssid)==0' in ui
assert 'retainObservedC5Name(in.band,in.ssid,items[i].ssid)' in tm
assert 'if(keepC5Ssid) strlcpy(items[i].ssid,previousSsid' in tm
assert 'dirk v0.7.16' in app or 'dirk v0.7.17' in app or 'dirk v0.7.18' in app or 'dirk v0.7.19' in app or 'dirk v0.8.0' in app
assert 'DIRK-v0.7.16' in wd
print('PASS: v0.7.16 automatic dual-band recon, C5 name retention and diagnostics guards')
