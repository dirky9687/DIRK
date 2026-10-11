"""v0.8.0 UI routing, no-new-TX, and footer bounds; source checks only."""
from pathlib import Path
r=Path(__file__).resolve().parents[2]
u=(r/'src/ui/Ui.h').read_text()
h=(r/'src/red/AttackLearningLab.h').read_text()
assert 'static constexpr uint8_t N=9;' in u
assert '"attack learning labs"' in u and 'case 2: attackLabSel_=0; enterScreen(73);' in u
assert 'if(screen==73){ redHubReturn_=false; enterScreen(33); return; }' in u
assert 'if(screen>=74 && screen<=77){ enterScreen(73); return; }' in u
for n,token in [(73,'attackLabMenu'),(74,'attackLabIdentity'),(75,'attackLabPmf'),(76,'attackLabWps'),(77,'attackLabPlaybook')]:
    assert f'case {n}: {token}(d,a); break;' in u
for key in ['collectLookalikes','demoWpsChecksum','observedPmf','exportIdentityLesson']:
    assert key in u
assert '!t.saved || !t.authorized' in u and 'mode->permitsActive(t)' in u
assert 'PASSIVE / OFFLINE / NO FRAME TX' in u and 'SIMULATION: NO RF sent' in u
assert '5G auth/PMF: not verified' in u
for forbidden in ['deauthBurst','disassocBurst','WiFi.begin(','WiFi.softAP(','esp_wifi_80211_tx','startScanAsync']:
    assert forbidden not in h
assert 'd.setCursor(6,108)' in u
assert 'dirk v0.8.0' in (r/'src/core/DirkApp.cpp').read_text()
print('PASS: v0.8.0 labs/menu/authorization/passivity/boot label')
