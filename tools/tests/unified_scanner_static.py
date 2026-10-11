from pathlib import Path
root=Path(__file__).resolve().parents[2]
u=(root/'src/ui/Ui.h').read_text()
app=(root/'src/core/DirkApp.cpp').read_text()
assert '#include "UnifiedApView.h"' in u
assert 'if(screen==12){' in u
assert 'wifi->startScanAsync(300,0)' in u
assert 'janos->startScan5()' in u
assert 'unifiedView_.build(*targets,now)' in u
assert 'unifiedView_.count24()' in u and 'unifiedView_.count5()' in u
assert 'p24?0xC800:0x39E7' in u and 'p5?0xC800:0x39E7' in u
assert 'd.print(auth)' in u and '24:%d 5:%d dBm' in u
assert 'detailReturnScreen_=12' in u and 'detailReturnScreen_=14' in u
assert 'SavedOnly' in (root/'tools/tests/unified_ap_view_test.cpp').read_text()
assert 'dirk v0.7.16' in app or 'dirk v0.7.17' in app or 'dirk v0.7.18' in app or 'dirk v0.7.19' in app or 'dirk v0.8.0' in app
print('PASS: unified scanner navigation, dual scheduling, display, and version guards')
