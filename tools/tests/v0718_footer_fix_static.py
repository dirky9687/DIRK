"""Targeted source guard for DIRK 0.7.18 FIX1 footer layout."""
from pathlib import Path
p=Path(__file__).resolve().parents[2]/'src/ui/Ui.h'
s=p.read_text()
a=s[s.index('void redPostureScreen('):s.index('void handlePostureFinding(')]
b=s[s.index('void redOperationResult('):s.index('  // Unified scanner:')]
assert 'd.setCursor(195,23)' in a, 'posture page counter missing'
assert 'const int y=62+(int)i*14' in a, 'posture entries not compacted'
assert 'd.setCursor(5,105)' in a, 'posture feedback too low'
assert 'd.setCursor(5,111)' not in a, 'overlapping guidance still present'
assert 'd.setCursor(7,105)' in b and 'd.setCursor(7,109)' not in b, 'result text at footer'
assert 'footer2(d,"L/R page | TAB CSV","ENT finding")' in s, 'help missing from footer'
print('PASS: Red Team result/detail screens avoid y=117 footer')
