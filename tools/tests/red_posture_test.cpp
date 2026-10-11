#include <cassert>
#include <cstring>
#include "../../src/red/RedAudit.h"
using namespace dirk;
static bool has(const RedPosture& p,const char* needle){
  for(unsigned i=0;i<p.count;++i) if(strstr(p.findings[i].title,needle))return true;
  return false;
}
int main(){
  WifiTarget open{};open.band=Band::Band24;open.auth=0;open.wps=false;
  RedPosture a=assessRedPosture(open);
  assert(has(a,"Open network") && !has(a,"WPS") && a.findings[0].severity==RedSeverity::High);
  WifiTarget dual{};dual.band=Band::Band5;dual.auth=3;dual.wps=false;
  a=assessRedPosture(dual);
  assert(has(a,"5 GHz data limited") && !has(a,"WPS") && !has(a,"PMF optional"));
  dual.auth=0; a=assessRedPosture(dual);
  assert(has(a,"Open/unknown") && !has(a,"Open network observed"));
  WifiTarget wpa{};wpa.band=Band::Band24;wpa.auth=2;wpa.wps=true;wpa.pmfCapable=true;
  a=assessRedPosture(wpa);
  assert(has(a,"Legacy WPA") && has(a,"WPS") && has(a,"PMF optional"));
  WifiTarget wpa3{};wpa3.band=Band::Band24;wpa3.auth=6;wpa3.pmfRequired=true;
  a=assessRedPosture(wpa3);assert(has(a,"WPA3") && !has(a,"PMF optional"));
  return 0;
}
