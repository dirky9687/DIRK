#pragma once
#include <dirk/types.h>
#include <string.h>

namespace dirk {
// Passive assessment of evidence actually observed for a single BSSID.
// A missing 5 GHz RSN/PMF detail is UNKNOWN, not proof of a vulnerability.
enum class RedSeverity : uint8_t { Info=0, Review=1, High=2 };
struct RedFinding { RedSeverity severity=RedSeverity::Info; const char* title=""; const char* advice=""; };
struct RedPosture {
  RedFinding findings[8]{};
  uint8_t count=0;
  void add(RedSeverity sev,const char* title,const char* advice){
    if(count<8){ findings[count].severity=sev; findings[count].title=title; findings[count].advice=advice; ++count; }
  }
};
inline RedPosture assessRedPosture(const WifiTarget& t){
  RedPosture a{};
  // ESP32 Arduino wifi_auth_mode_t assignments: OPEN=0, WEP=1,
  // WPA_PSK=2, WPA2_PSK=3, WPA_WPA2_PSK=4, WPA3_PSK=6.
  // MonsterC5 currently reports only coarse auth data; refrain from
  // assuming PMF, WPS or cipher information on that radio.
  if(t.auth==0 && t.band==Band::Band5)
    a.add(RedSeverity::Info,"Open/unknown on C5","Check 5 GHz security on AP");
  else if(t.auth==0) a.add(RedSeverity::High,"Open network observed","Review isolation and encryption");
  else if(t.auth==1) a.add(RedSeverity::High,"Legacy WEP indicated","Replace with WPA2/3");
  else if(t.auth==2 || t.auth==4) a.add(RedSeverity::Review,"Legacy WPA indicated","Prefer WPA2-AES/WPA3");
  else if(t.auth==3) a.add(RedSeverity::Info,"WPA2 observed","Review migration to WPA3");
  else if(t.auth==6 || t.auth==7) a.add(RedSeverity::Info,"WPA3 mode observed","Verify client configuration");
  else a.add(RedSeverity::Info,"Security mode uncertain","Confirm AP settings directly");
  if(t.band==Band::Band24){
    if(t.wps) a.add(RedSeverity::Review,"WPS present","Disable if unnecessary");
    if(!t.pmfRequired && t.pmfCapable) a.add(RedSeverity::Info,"PMF optional","Consider requiring PMF");
    // A missing PMF IE is NOT treated as evidence PMF is unsupported.
  } else if(t.band==Band::Band5){
    a.add(RedSeverity::Info,"5 GHz data limited","Verify PMF/WPS on router");
  }
  if(t.rssi<-78) a.add(RedSeverity::Info,"Weak received signal","Check placement/coverage");
  if(!t.ssid[0] || strcmp(t.ssid,"<5G AP>")==0)
    a.add(RedSeverity::Info,"SSID not advertised","Identify AP via BSSID");
  return a;
}
inline const char* redSeverityName(RedSeverity s){
  return s==RedSeverity::High?"HIGH":(s==RedSeverity::Review?"REVIEW":"INFO");
}
} // namespace dirk
