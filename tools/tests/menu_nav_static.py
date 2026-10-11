"""DIRK v0.7.14 menu-navigation source guard, not an embedded compilation."""
from pathlib import Path

root=Path(__file__).resolve().parents[2]
ui=(root/'src/ui/Ui.h').read_text()
checks={
    'seven root categories': 'static const uint8_t screens[]={1,2,3,40,60,6,9};' in ui,
    'all home labels': all(x in ui for x in ['"[w]ifi"','"[b]luetooth"','"war[d]rive"','"[l]ora / mesh"','"[i]nfrared"','"data center"','"sys[t]em"']),
    'menu navigation wraps across seven': 'menuSel=(menuSel+6)%7' in ui and 'menuSel=(menuSel+1)%7' in ui,
    'data submenu renders': 'case 6: dataMenu(d,a); break;' in ui,
    'data submenu handles input': 'else if(screen==6) handleDataMenu(e);' in ui,
    'all four data destinations': all(s in ui for s in ['enterScreen(4); break;', 'enterScreen(5); break;', 'enterScreen(7); break;', 'enterScreen(8); break;']),
    'data screen Back handled': 'if(screen==6){ screen=0; menuSel=5;' in ui,
    'data browsers Back to data': 'if(screen==4 || screen==5 || screen==7 || screen==8){' in ui and 'enterScreen(6);return;' in ui,
    'system screen Back to system': 'if(screen==9){ screen=0; menuSel=6;' in ui,
    'settings Wi-Fi connection back to system': 'if(screen==10){ enterScreen(9); return; }' in ui,
    'legacy captured files shortcuts': all(s in ui for s in ['case InputAction::HotCaptures: dataShortcut=0;', 'case InputAction::HotAudits: dataShortcut=1;', 'case InputAction::HotSessions: dataShortcut=2;', 'case InputAction::HotFiles: dataShortcut=3;']),
    'file locations unchanged': all(s in ui for s in ['"/dirk/captures"','"/dirk/targets"','"/dirk/wardrive/sessions"']),
    'Wi-Fi duplicate capture removed': '"recon","analysis","capture"' not in ui and 'uint8_t wifiCategoryCount() const { return 6; }' in ui,
    'Red authorized tools preserved': 'else if(red && wifiCategorySel==4){ redHubReturn_=false; enterScreen(33); }' in ui,
    'Red portal preserved': 'case 6: beaconRan=false; labSel=0; enterScreen(20); break;' in ui,
    '5G diagnostics reachable': 'else { wifiBand5=true; enterScreen(21); }' in ui,
    'Wardrive end session preserved': 'wdMenuSel==1 && wardrive && wardrive->running()' in ui and 'wardrive->stop();' in ui,
    'Wardrive background badge preserved': 'WD BG ACTIVE' in ui,
    'matrix canvas preserved': 'menuCanvas->pushSprite(126,19);' in ui,
}
for name,ok in checks.items():
    print(('PASS' if ok else 'FAIL')+': '+name)
assert all(checks.values()), 'Menu navigation static check failed'
print(f'PASS: {len(checks)} menu navigation guards')
