#include <cassert>
#include <cstring>
#include "../../src/red/DualBandReview.h"
using namespace dirk;
static bool has(const RedPosture& p,const char* frag){for(unsigned i=0;i<p.count;i++)if(strstr(p.findings[i].title,frag))return true;return false;}
int main(){
 WifiTarget a{}, b{};
 a.saved=a.authorized=b.saved=b.authorized=true;
 a.band=Band::Band24; b.band=Band::Band5;
 std::strcpy(a.ssid,"DemoServer"); std::strcpy(b.ssid,"DemoServer");
 assert(sameDualBandSsid(a,b));
 a.wps=true;a.pmfCapable=true;
 RedPosture p=assessDualBand(a,&b);
 assert(has(p,"Both bands"));assert(has(p,"WPS"));assert(has(p,"PMF optional"));
 assert(has(p,"RSN detail limited"));assert(has(p,"auth unconfirmed"));
 assert(!has(p,"5 GHz weak"));
 b.authorized=false;assert(!sameDualBandSsid(a,b));
 b.authorized=true;std::strcpy(b.ssid,"Other");assert(!sameDualBandSsid(a,b));
 p=assessDualBand(a,nullptr);assert(has(p,"not confirmed"));assert(!has(p,"WPS advertised"));
 return 0;
}