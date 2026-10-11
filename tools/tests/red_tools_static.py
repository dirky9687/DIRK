"""Source-only v0.7.17 Red Team audit guards, not a firmware compile."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
u=(root/'src/ui/Ui.h').read_text()
p=(root/'src/portal/LabPortal.h').read_text()
s=(root/'src/storage/StorageManager.h').read_text()
r=(root/'src/red/RedAudit.h').read_text()
assert 'static constexpr uint8_t N=9;' in u
assert 'target posture / CSV' in u and 'enterScreen(65)' in u
assert 'handleRedPosture(e)' in u and 'case 65: redPostureScreen(d,a);' in u
assert 'mode->permitsActive(t)' in u
assert 'if(t && t->saved && t->authorized)' in u
assert '/dirk/reports/red_posture_' in u and 'auditCsvCell' in u
assert '/dirk/portal/notice.txt' in p and 'escapeHtml(notice)' in p
assert 'server.on("/",HTTP_GET,' in p and 'server.onNotFound' in p
assert 'server.on("/note"' not in p
assert '300000u' in p and 'routesReady' in p
assert 'make("/dirk/portal")' in s
assert '5 GHz data limited' in r and 'Open/unknown on C5' in r
print('PASS v0.7.17 Red Team source guards')
