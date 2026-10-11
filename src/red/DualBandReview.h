#pragma once
#include "RedAudit.h"
#include <string.h>
namespace dirk {
// Evidence for *saved, explicitly authorized* AP observations. SSIDs are
// group labels; BSSIDs remain separate and no shared device is assumed.
inline bool sameDualBandSsid(const WifiTarget& x,const WifiTarget& y){
  return x.saved && x.authorized && y.saved && y.authorized &&
         x.band!=y.band && ((x.band==Band::Band24 && y.band==Band::Band5) ||
         (x.band==Band::Band5 && y.band==Band::Band24)) &&
         x.ssid[0] && strcmp(x.ssid,"<5G AP>") && strcmp(x.ssid,y.ssid)==0;
}
inline RedPosture assessDualBand(const WifiTarget& primary,const WifiTarget* match){
  RedPosture a{};
  if(!match){
    a.add(RedSeverity::Info,"Other band not confirmed","Save and authorize other BSSID");
    a.add(RedSeverity::Info,"No cross-band conclusion","A missing scan is not a flaw");
    return a;
  }
  const WifiTarget& p24=primary.band==Band::Band24?primary:*match;
  const WifiTarget& p5=primary.band==Band::Band5?primary:*match;
  a.add(RedSeverity::Info,"Both bands observed","Separate BSSID per radio retained");
  if(p24.wps) a.add(RedSeverity::Review,"2.4 GHz WPS advertised","Disable WPS if unneeded");
  else a.add(RedSeverity::Info,"2.4 GHz WPS not seen","Absence does not prove disabled");
  if(p24.pmfRequired) a.add(RedSeverity::Info,"2.4 GHz PMF required","Validate with supported clients");
  else if(p24.pmfCapable) a.add(RedSeverity::Review,"2.4 GHz PMF optional","Consider PMF-required mode");
  else a.add(RedSeverity::Info,"2.4 GHz PMF unknown","Check AP management settings");
  // C5's current scan payload does not provide full RSN/WPS/PMF IEs.
  a.add(RedSeverity::Info,"5 GHz RSN detail limited","Check PMF/WPS in router UI");
  if(p24.auth==0 || p24.auth==1)
    a.add(RedSeverity::Review,"2.4 GHz weak/open auth","Verify encryption config");
  if(p5.auth==0)
    a.add(RedSeverity::Info,"5 GHz auth unconfirmed","C5 open/unknown not conclusive");
  // Never claim that SSID equality implies identical security policies.
  return a;
}
} // namespace dirk
