"""0.7.18 display/navigation guards, not a firmware compile."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
u=(root/'src/ui/Ui.h').read_text()
checks={
    'red hub accessible':'else if(red && wifiCategorySel==4)' in u and 'enterScreen(33)' in u,
    'dedicated Red operations menu':'red // operations' in u and 'gateway service survey' in u,
    'gateway survey results':'case 66: networkSurveyResults' in u and 'enterScreen(66)' in u,
    'gateway port details':'case 69: portDetail' in u and 'enterScreen(69)' in u,
    'blue guard result pages':'case 67: blueGuardSummary' in u and 'blueSummaryPage_' in u,
    'operation outcomes separate':'case 68: redOperationResult' in u and 'redResultText_=redStatus' in u,
    'posture advice':'case 70: postureFinding' in u and 'Recommendation:' in u,
    'return from detail':'if(screen==70){ enterScreen(65); return; }' in u,
    'return from port':'if(screen==69){ enterScreen(66); return; }' in u,
    'return from Red result':'if(screen==68){ enterScreen(15); return; }' in u,
    'capture exit retained':'if(screen==15 && captures && captures->active()) captures->stop();' in u,
    'no new port scanning scope':'nettools->scanGateway();' in u and 'const char* services[]' in u,
    'no bogus service verification':'No version or vulnerability scan.' in u,
    'automatic dual-band AP unchanged':'unifiedView_.build(*targets,now)' in u and 'janos->startScan5()' in u,
}
for label,ok in checks.items():print(('PASS' if ok else 'FAIL')+': '+label)
assert all(checks.values()),'Red UI guard failed'
print('PASS red UI source checks')
