from pathlib import Path
r=Path(__file__).resolve().parents[2]
u=(r/'src/ui/Ui.h').read_text()
b=(r/'src/providers/ble/BleProvider.h').read_text()
assert 'case 71: dualBandReview(d,a);' in u and 'handleDualBandReview(e)' in u
assert 'case 72: blePosture(d,a);' in u and 'handleBlePosture(e)' in u
assert 'if(screen==71){ redHubReturn_=false; enterScreen(33); return; }' in u
assert 'if(screen==72){ bleExportConfirm_=false;' in u
assert 'sameDualBandSsid(t,*x)' in u and 't && t->saved && t->authorized' in u
assert 'bleReviewSnapshot_=*selected' in u and 'const BleTarget* t=&bleReviewSnapshot_' in u
assert 'Owned device? TAB to export' in u and 'bleExportConfirm_=false' in u
assert 'SD.open(path,FILE_WRITE)' in u and '/dirk/reports/ble_posture_' in u
assert 'if(screen==37){' in u and 'lastBleLabSerial_' in u
assert 'manufacturerDataBytes' in b and 'serviceDataBytes' in b
assert 'd.setCursor(7,105)' in u and 'd.setCursor(7,108)' in u
assert not any(x in u for x in ['BLEClient::connect','writeValue('])
print('PASS: v0.7.19 Red/BLE navigation, authorization, bounds, reporting')
