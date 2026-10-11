#include <cassert>
#include <cstdio>
#include <cstring>
#include "../../src/ui/UnifiedApView.h"
using namespace dirk;
struct FakeTargets {
  WifiTarget items[20]{}; size_t n=0;
  size_t count() const{return n;}
  const WifiTarget* at(size_t i) const{return i<n?&items[i]:nullptr;}
  void add(const char* ssid,Band band,uint8_t lastByte,int rssi,uint32_t seen,uint8_t auth=3){
    WifiTarget& t=items[n++];strncpy(t.ssid,ssid,32);
    t.band=band;t.bssid.b[5]=lastByte;t.id=(uint32_t)n;
    t.rssi=(int8_t)rssi;t.lastSeen=seen;t.auth=auth;
  }
};
int main(){
  FakeTargets src;
  src.add("DemoDualBand",Band::Band24,1,-58,100000);
  src.add("DemoDualBand",Band::Band5,2,-72,100000);
  src.add("DemoConsole",Band::Band5,3,-43,100000);
  src.add("DemoConsole",Band::Band24,4,-82,100000);
  src.add("DemoDualBand",Band::Band5,5,-49,100000); // same-band BSSID: stronger wins
  src.add("<5G AP>",Band::Band5,6,-69,100000);
  src.add("<5G AP>",Band::Band5,7,-71,100000); // no accidental hidden grouping
  src.add("stale",Band::Band24,8,-51,1000);
  src.add("SavedOnly",Band::Band5,9,-50,0);
  UnifiedApView view;view.build(src,101000);
  assert(view.count()==4);assert(view.count24()==2);assert(view.count5()==5);
  assert(view.at(0)->band24==0 && view.at(0)->band5==4);
  assert(view.preferred(src,0,true)->id==5);
  assert(view.preferred(src,0,false)->id==1);
  assert(view.at(1)->band24==3 && view.at(1)->band5==2);
  assert(view.at(2)->band5==5 && view.at(3)->band5==6);
  const auto sig=view.signature(src);
  src.items[4].rssi=-48;
  view.build(src,101000);assert(view.signature(src)!=sig);
  // No new packets: expired 2.4GHz results fall off, 5GHz persist longer.
  view.build(src,120000);assert(view.count24()==0 && view.count5()==5);
  view.build(src,180001);assert(view.count()==0);
  puts("PASS: unified dual-band grouping, band indicators, RSSI, hidden IDs, expiry, signature");
}
