#include <cassert>
#include <cstring>
#include "../../src/red/AttackLearningLab.h"
using namespace dirk;
int main(){
  WifiTarget a{},b{};
  a.band=b.band=Band::Band24;
  a.auth=3;b.auth=0;
  a.channel=1;b.channel=11;
  a.bssid.b[0]=1; b.bssid.b[0]=2;
  strcpy(a.ssid,"MyLab");strcpy(b.ssid,"MyLab");
  LookalikeClues x{};learnFromPeer(x,a,a);assert(x.sameName==0);
  learnFromPeer(x,a,b);
  assert(x.sameName==1 && x.sameBand==1 && x.sameBandAuthDiff==1 && x.sameBandChannelDiff==1);
  WifiTarget c=b;c.band=Band::Band5;c.auth=0;
  learnFromPeer(x,a,c);assert(x.sameName==2 && x.otherBand==1 && x.otherBandAuthUnknown==1);
  strcpy(c.ssid,"<5G AP>");learnFromPeer(x,a,c);assert(x.sameName==2);
  strcpy(c.ssid,"Other");learnFromPeer(x,a,c);assert(x.sameName==2);
  a.pmfRequired=true; a.pmfCapable=false;assert(observedPmf(a)==PmfLesson::Required);
  a.pmfRequired=false;a.pmfCapable=true;assert(observedPmf(a)==PmfLesson::Optional);
  a.pmfCapable=false;assert(observedPmf(a)==PmfLesson::Unknown);
  a.band=Band::Band5;a.pmfRequired=true;assert(observedPmf(a)==PmfLesson::Unknown);
  for(uint32_t pin=0;pin<10000000;pin+=173331){
    uint32_t full=demoWpsPin(pin);
    assert(full/10==pin && full%10==demoWpsChecksum(pin));
    uint32_t v=full, sum=0;
    while(v){sum+=(v%10u);v/=10u;sum+=3u*(v%10u);v/=10u;}
    assert(sum%10u==0);
  }
  return 0;
}
