#pragma once
#include <dirk/types.h>
#include <cstring>
#include <cstdint>

namespace dirk {
// Passive SSID impersonation *clues*: another BSSID on the same SSID is not
// proof of an evil twin. Mesh and multiband APs routinely share their SSID.
struct LookalikeClues {
  uint16_t sameName=0;
  uint16_t sameBand=0;
  uint16_t otherBand=0;
  uint16_t sameBandAuthDiff=0;
  uint16_t otherBandAuthUnknown=0;
  uint16_t sameBandChannelDiff=0;
};
inline bool usableObservedName(const WifiTarget& t){
  return t.ssid[0] && std::strcmp(t.ssid,"<5G AP>")!=0 &&
         std::strcmp(t.ssid,"<hidden>")!=0;
}
inline void learnFromPeer(LookalikeClues& r,const WifiTarget& focus,const WifiTarget& peer){
  if(!usableObservedName(focus) || !usableObservedName(peer) ||
     std::strcmp(focus.ssid,peer.ssid)!=0 ||
     (macEq(focus.bssid,peer.bssid) && focus.band==peer.band)) return;
  ++r.sameName;
  if(peer.band==focus.band){
    ++r.sameBand;
    // On the S3 radio the auth code is observed; skip ambiguous C5 detail.
    if(focus.band==Band::Band24 && focus.auth!=peer.auth) ++r.sameBandAuthDiff;
    if(focus.channel && peer.channel && focus.channel!=peer.channel)
      ++r.sameBandChannelDiff;
  } else {
    ++r.otherBand;
    if(peer.band==Band::Band5 || focus.band==Band::Band5) ++r.otherBandAuthUnknown;
  }
}

enum class PmfLesson : uint8_t { Required, Optional, Unknown };
inline PmfLesson observedPmf(const WifiTarget& t){
  // C5 records lack reliable PMF RSN IEs: never infer a 5 GHz failure.
  if(t.band!=Band::Band24) return PmfLesson::Unknown;
  if(t.pmfRequired) return PmfLesson::Required;
  if(t.pmfCapable) return PmfLesson::Optional;
  return PmfLesson::Unknown;
}
inline const char* pmfLessonName(PmfLesson v){
  return v==PmfLesson::Required?"REQUIRED":(v==PmfLesson::Optional?"OPTIONAL":"UNKNOWN");
}
// Entirely offline WPS PIN check-digit teaching example, NEVER an AP query.
// Only synthetic seven-digit inputs are used by the UI, no radio TX or guesses.
inline uint8_t demoWpsChecksum(uint32_t pin7){
  uint32_t p=pin7, sum=0;
  while(p){sum+=3u*(p%10u);p/=10u;sum+=(p%10u);p/=10u;}
  return static_cast<uint8_t>((10u-(sum%10u))%10u);
}
inline uint32_t demoWpsPin(uint32_t pin7){return pin7*10u+demoWpsChecksum(pin7);}
} // namespace dirk
