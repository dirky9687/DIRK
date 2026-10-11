#include <cassert>
#include <cstdio>
#include "../../src/targets/ApNameRetention.h"
using namespace dirk;
int main(){
  assert(retainObservedC5Name(Band::Band5,"<5G AP>","DemoServer"));
  assert(retainObservedC5Name(Band::Band5,"","DemoDualBand"));
  assert(retainObservedC5Name(Band::Band5,"<hidden>","DemoConsole"));
  assert(!retainObservedC5Name(Band::Band24,"<5G AP>","DemoServer"));
  assert(!retainObservedC5Name(Band::Band5,"DifferentName","DemoServer"));
  assert(!retainObservedC5Name(Band::Band5,"<5G AP>","<5G AP>"));
  assert(!retainObservedC5Name(Band::Band5,"<5G AP>",""));
  puts("PASS: C5 name retention only for same-BSSID placeholder updates");
}
