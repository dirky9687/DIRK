#include <cassert>
#include <cstring>
#include "../../src/red/BlePosture.h"
using namespace dirk;
struct MockAdv{
 bool named=false,ibeacon=false,eddystone=false,meshtastic=false,meshcore=false,hasTxPower=false;
 unsigned manufacturerId=0,manufacturerDataBytes=0;
 char service[40]{};
};
static bool has(const RedPosture& p,const char* frag){for(unsigned i=0;i<p.count;i++)if(strstr(p.findings[i].title,frag))return true;return false;}
int main(){
 MockAdv a{};
 RedPosture p=assessBlePosture(a);
 assert(has(p,"No name") && has(p,"GATT security untested"));
 assert(!has(p,"iBeacon"));
 a.ibeacon=true; a.eddystone=true;a.manufacturerId=76;a.manufacturerDataBytes=24;
 a.named=true;a.hasTxPower=true;std::strcpy(a.service,"180A");
 p=assessBlePosture(a);
 assert(has(p,"iBeacon")&&has(p,"Eddystone")&&has(p,"Manufacturer"));
 assert(has(p,"Service UUID")&&has(p,"TX power")&&p.count<=8);
 return 0;
}