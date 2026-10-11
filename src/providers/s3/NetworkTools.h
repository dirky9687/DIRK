#pragma once
#include <WiFi.h>
namespace dirk {
struct PortHit{uint16_t port=0; bool open=false;};
class NetworkTools {
public:
  bool connected()const{return WiFi.status()==WL_CONNECTED;}
  IPAddress ip()const{return WiFi.localIP();}
  IPAddress gateway()const{return WiFi.gatewayIP();}
  IPAddress subnet()const{return WiFi.subnetMask();}
  IPAddress dns()const{return WiFi.dnsIP();}
  int rssi()const{return connected()?WiFi.RSSI():-127;}
  uint8_t channel()const{return connected()?WiFi.channel():0;}
  const PortHit* hits()const{return results;}
  size_t count()const{return N;}
  void scanGateway(){
    static const uint16_t ports[N]={22,23,53,80,443,445,8080,8443};
    IPAddress host=gateway();
    for(size_t i=0;i<N;i++){results[i].port=ports[i]; WiFiClient c; results[i].open=c.connect(host,ports[i],180); c.stop();}
  }
private:
  static constexpr size_t N=8; PortHit results[N]{};
};
}