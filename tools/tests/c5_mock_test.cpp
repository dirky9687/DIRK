// Offline adapter unit test: mocks serial/M5 APIs; not a hardware build.
#include <string>
#include <vector>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <cctype>
#include <cstdlib>
static uint32_t tick=1;
uint32_t millis(){return tick;}
class String {
 std::string s;
 public:
 String()=default;String(const char* c):s(c?c:""){}String(std::string x):s(x){}
 String& operator=(const char* c){s=c?c:"";return *this;}
 int length()const{return (int)s.size();}char operator[](int i)const{return s.at(i);}
 int indexOf(const char* q,int from=0)const{auto p=s.find(q,from);return p==std::string::npos?-1:(int)p;}
 int indexOf(char q,int from=0)const{auto p=s.find(q,from);return p==std::string::npos?-1:(int)p;}
 String substring(int p)const{return String(s.substr(p));}
 String substring(int p,int e)const{return String(s.substr(p,e-p));}
 long toInt()const{return strtol(s.c_str(),nullptr,10);}
 void trim(){auto b=s.find_first_not_of(" \t\n\r");if(b==std::string::npos){s.clear();return;}auto e=s.find_last_not_of(" \t\n\r");s=s.substr(b,e-b+1);}
 void toLowerCase(){std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return tolower(c);});}
 bool operator==(const char* rhs)const{return s==rhs;}bool operator!=(const char* rhs)const{return s!=rhs;}
 const char* c_str()const{return s.c_str();}
};
size_t strlcpy(char* dst,const char* src,size_t size){size_t len=strlen(src);if(size){size_t c=std::min(len,size-1);memcpy(dst,src,c);dst[c]=0;}return len;}
namespace dirk {
 struct Mac{uint8_t b[6]{};};enum class Band:uint8_t{Band24,Band5};
 constexpr uint8_t WIFI_AUTH_WPA2_PSK=3, WIFI_AUTH_WPA_PSK=2, WIFI_AUTH_OPEN=0;
 struct WifiTarget{Mac bssid;uint8_t channel=0;int8_t rssi=0;Band band;uint32_t lastSeen;char ssid[40]{};uint8_t auth=0;};
 class TargetManager{public:std::vector<WifiTarget> items;WifiTarget* observe(const WifiTarget& x){items.push_back(x);return &items.back();}};
 class C5Transport{public:std::vector<String> pending;std::vector<std::string> sent;
  bool everDetected()const{return true;} void update(){}
  bool readLine(String& out){if(pending.empty())return false;out=pending.front();pending.erase(pending.begin());return true;}
  void sendLine(const char* s){sent.push_back(s);}
 };
}
#include "JanOSAdapter_mocked.h"
int main(){
 using namespace dirk;
 C5Transport c;TargetManager targets;JanOSAdapter adapter;adapter.begin(&c,&targets);
 assert(adapter.startScan5());assert(c.sent.back()=="scan_networks");assert(!adapter.startScan5());
 c.pending.push_back("scan_start");
 c.pending.push_back("{\"event\":\"scan_network\",\"bssid\":\"02:11:22:33:44:55\",\"ssid\":\"Our5GHz\",\"channel\":36,\"rssi\":-47,\"auth\":\"wpa2\"}");
 c.pending.push_back("{\"bssid\":\"02:11:22:33:44:66\",\"ssid\":\"WrongBand\",\"channel\":6,\"rssi\":-52}");
 adapter.update();assert(adapter.parsed()==1);assert(targets.items.size()==1);
 assert(targets.items[0].channel==36 && targets.items[0].band==Band::Band5);
 assert(std::string(targets.items[0].ssid)=="Our5GHz");assert(targets.items[0].rssi==-47);
 assert(adapter.badChannel()==1);
 c.pending.push_back("scan_complete");adapter.update();assert(!adapter.scanning());
 tick+=11500;assert(adapter.startScan5());
 c.pending.push_back("OK");adapter.update();tick+=6700;adapter.update();
 assert(adapter.scanning() && c.sent.back()=="scan");
 std::cout<<"PASS C5 parser: 5GHz observation, 2.4GHz rejection, scan lifecycle, fallback\n";
}
