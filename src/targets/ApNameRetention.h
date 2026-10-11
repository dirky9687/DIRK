#pragma once
#include <string.h>
#include <dirk/types.h>

namespace dirk {
// JanOS sometimes publishes a known BSSID without a broadcast SSID. A later
// placeholder must not erase an *actual* SSID previously received for that
// same BSSID. Do not infer a name from a nearby AP, another BSSID, or 2.4 GHz.
inline bool isUnresolvedC5Name(const char* s) {
  return !s || !s[0] || strcmp(s,"<5G AP>")==0 ||
         strcmp(s,"<hidden>")==0 || strcmp(s,"<unknown>")==0;
}
inline bool retainObservedC5Name(Band band,const char* replacement,const char* previouslySeen) {
  return band==Band::Band5 && isUnresolvedC5Name(replacement) &&
         !isUnresolvedC5Name(previouslySeen);
}
} // namespace dirk
