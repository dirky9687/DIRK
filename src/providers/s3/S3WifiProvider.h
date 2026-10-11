#pragma once
#include <WiFi.h>
#include <esp_wifi.h>
#include "../../targets/TargetManager.h"

namespace dirk {
class S3WifiProvider {
public:
  void begin(){ WiFi.mode(WIFI_STA); state=Availability::Ready; instance=this; }
  Availability availability() const { return state; }

  // FIX8 primary path: preserve FIX7's proven radio-reset sequence, but perform
  // its timing as a millis()-driven state machine. The actual ESP32 scan is
  // started asynchronously from the normal Arduino loop task (never a worker
  // task), then scanComplete() is polled by update().
  bool startScanAsync(uint16_t maxMsPerChannel=300,uint8_t channel=0){
    if(state==Availability::Busy || resultReady || scanPhase!=ScanPhase::Idle) return false;
    scanMaxMs_=maxMsPerChannel?maxMsPerChannel:300;
    scanChannel_=channel;
    lastScanRaw_=WIFI_SCAN_RUNNING;
    fallbackLastScan_=false;
    state=Availability::Busy;

    // Same ownership/reset semantics as FIX7, minus blocking delay().
    esp_wifi_set_promiscuous(false);
    WiFi.scanDelete();
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false,false);
    scanPhase=ScanPhase::DisconnectWait;
    phaseAt_=millis();
    return true;
  }

  // Service the non-blocking reset + async scan from DirkApp::update(), which
  // executes on the normal Arduino task. A failed/aborted async scan falls back
  // to the exact synchronous FIX7 scanner so discovery reliability is retained.
  void update(){
    if(scanPhase==ScanPhase::Idle) return;
    const uint32_t now=millis();
    switch(scanPhase){
      case ScanPhase::DisconnectWait:
        if(now-phaseAt_>=100){ WiFi.mode(WIFI_OFF); scanPhase=ScanPhase::OffWait; phaseAt_=now; }
        break;
      case ScanPhase::OffWait:
        if(now-phaseAt_>=60){ WiFi.mode(WIFI_STA); scanPhase=ScanPhase::StaWait; phaseAt_=now; }
        break;
      case ScanPhase::StaWait:
        if(now-phaseAt_>=120){
          const int rc=WiFi.scanNetworks(true,true,false,scanMaxMs_,scanChannel_);
          if(rc==WIFI_SCAN_FAILED){ runSynchronousFallback(); break; }
          if(rc>=0){ stageResults(rc); break; } // defensive: some cores can finish immediately
          scanPhase=ScanPhase::AsyncRunning;
          asyncStartedAt_=now;
        }
        break;
      case ScanPhase::AsyncRunning: {
        const int n=WiFi.scanComplete();
        if(n>=0){ stageResults(n); break; }
        if(n==WIFI_SCAN_FAILED || now-asyncStartedAt_>asyncTimeoutMs()){
          runSynchronousFallback();
        }
        break;
      }
      case ScanPhase::Idle: break;
    }
  }

  // Exact FIX7 synchronous scanner retained as a public diagnostic/fallback path.
  int scan(TargetManager& tm){
    if(state==Availability::Busy || resultReady) return WIFI_SCAN_RUNNING;
    fallbackLastScan_=false;
    state=Availability::Busy;
    prepareForScanBlocking();
    const int n=WiFi.scanNetworks(false,true,false,300);
    lastScanRaw_=n; lastScanAt_=millis();
    return finishScan(tm,n);
  }

  // Publish staged async results into TargetManager. This is intentionally cheap
  // and is called once per main loop; it never blocks on RF work.
  int pollScan(TargetManager& tm){
    if(!resultReady) return 0;
    const uint8_t n=pendingCount;
    for(uint8_t i=0;i<n;i++){ pending[i].lastSeen=millis(); tm.observe(pending[i]); }
    const int raw=pendingRaw_;
    completionSerial_++;
    if(pendingChanged_) resultSerial_++;
    pendingCount=0; pendingRaw_=0; resultReady=false; pendingChanged_=false;
    return raw<0?raw:(int)n;
  }

  // Called on an explicit Wardrive End Session. Cancel an in-flight async
  // scan before turning Wardrive mode off, otherwise a failed scan may run
  // the blocking FIX7 fallback after the user has already left the screen.
  void cancelWardriveScan(){
    if(scanPhase==ScanPhase::AsyncRunning) esp_wifi_scan_stop();
    if(scanPhase!=ScanPhase::Idle || resultReady || state==Availability::Busy){
      WiFi.scanDelete();
      scanPhase=ScanPhase::Idle;
      resultReady=false;
      pendingCount=0; pendingRaw_=0; pendingChanged_=false;
      state=Availability::Ready;
    }
  }
  void setWardriveMode(bool enabled){ wardriveMode_=enabled; }
  bool scanning() const { return resultReady || state==Availability::Busy || scanPhase!=ScanPhase::Idle; }
  uint8_t scanChannel() const { return scanChannel_; }
  int lastScanCode() const { return lastScanRaw_; }
  uint32_t lastScanAt() const { return lastScanAt_; }
  uint32_t completionSerial() const { return completionSerial_; }
  uint32_t resultSerial() const { return resultSerial_; }
  bool usedFallbackLastScan() const { return fallbackLastScan_; }

  bool inspect(WifiTarget& t,uint32_t listenMs=900){
    if(t.band!=Band::Band24 || t.channel<1 || t.channel>14) return false;
    inspectTarget=&t; inspectFound=false;
    WiFi.mode(WIFI_STA);
    if(esp_wifi_set_channel(t.channel,WIFI_SECOND_CHAN_NONE)!=ESP_OK) return false;
    wifi_promiscuous_filter_t filter{};
    filter.filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT;
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&S3WifiProvider::rxThunk);
    if(esp_wifi_set_promiscuous(true)!=ESP_OK) return false;
    uint32_t start=millis();
    while(millis()-start<listenMs && !inspectFound){ delay(4); }
    esp_wifi_set_promiscuous(false);
    inspectTarget=nullptr;
    return inspectFound;
  }

  bool sampleAir(uint8_t channel,uint32_t listenMs,WifiAirStats& out){
    if(channel<1 || channel>14) return false;
    stats={}; uniqueCount=0; statsMode=true;
    WiFi.mode(WIFI_STA);
    if(esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE)!=ESP_OK){ statsMode=false; return false; }
    wifi_promiscuous_filter_t filter{};
    filter.filter_mask=WIFI_PROMIS_FILTER_MASK_MGMT|WIFI_PROMIS_FILTER_MASK_DATA;
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&S3WifiProvider::rxThunk);
    if(esp_wifi_set_promiscuous(true)!=ESP_OK){ statsMode=false; return false; }
    uint32_t start=millis();
    while(millis()-start<listenMs){ delay(4); }
    esp_wifi_set_promiscuous(false);
    stats.uniqueStations=uniqueCount;
    out=stats; statsMode=false;
    return true;
  }

  bool beaconLabBurst(uint32_t durationMs=15000){
    static const char* names[]={"DIRK-LAB-01","DIRK-LAB-02","DIRK-LAB-03","DIRK-LAB-04"};
    WiFi.mode(WIFI_STA);
    uint8_t channel=6;
    if(esp_wifi_set_channel(channel,WIFI_SECOND_CHAN_NONE)!=ESP_OK) return false;
    uint32_t start=millis(); uint16_t seq=0;
    while(millis()-start<durationMs){
      for(int i=0;i<4;i++){
        uint8_t frame[128]{};
        size_t n=buildBeacon(frame,sizeof(frame),names[i],i,channel,seq++);
        if(n) esp_wifi_80211_tx(WIFI_IF_STA,frame,n,false);
        delay(55);
      }
    }
    return true;
  }

private:
  enum class ScanPhase : uint8_t { Idle,DisconnectWait,OffWait,StaWait,AsyncRunning };
  Availability state=Availability::Unknown;
  static S3WifiProvider* instance;
  static constexpr uint8_t MAX_PENDING=48;
  ScanPhase scanPhase=ScanPhase::Idle;
  uint32_t phaseAt_=0,asyncStartedAt_=0;
  uint16_t scanMaxMs_=300;
  uint8_t scanChannel_=0;
  bool resultReady=false,pendingChanged_=false,fallbackLastScan_=false,havePublishedSignature_=false,wardriveMode_=false;
  WifiTarget pending[MAX_PENDING]{};
  uint8_t pendingCount=0;
  int pendingRaw_=0;
  int lastScanRaw_=0;
  uint32_t lastScanAt_=0,completionSerial_=0,resultSerial_=0,lastPublishedSignature_=0;
  WifiTarget* inspectTarget=nullptr;
  volatile bool inspectFound=false,statsMode=false;
  WifiAirStats stats{};
  Mac uniqueStations[32]{};
  uint16_t uniqueCount=0;


  uint32_t asyncTimeoutMs() const {
    const uint32_t chans=scanChannel_?1u:14u;
    const uint32_t wanted=(uint32_t)scanMaxMs_*chans+2500u;
    return wanted<2500u?2500u:wanted;
  }

  static uint32_t fnv1a(uint32_t h,const uint8_t* p,size_t n){
    for(size_t i=0;i<n;i++){ h^=p[i]; h*=16777619u; }
    return h;
  }

  uint32_t targetSignature(const WifiTarget& t) const {
    uint32_t h=2166136261u;
    h=fnv1a(h,t.bssid.b,6);
    h=fnv1a(h,(const uint8_t*)t.ssid,strlen(t.ssid));
    h^=t.channel; h*=16777619u; h^=t.auth; h*=16777619u;
    // Ignore tiny RSSI flutter so the AP list redraws only for meaningful changes.
    const int16_t q=(int16_t)((t.rssi>=0?t.rssi+2:t.rssi-2)/4);
    h^=(uint16_t)q; h*=16777619u;
    return h;
  }

  uint32_t pendingSignature(uint8_t count,int raw) const {
    // Order-independent aggregate: scan RSSI ordering can shuffle without the
    // actual AP set changing. Include count/raw so empty/non-empty transitions show.
    uint32_t h=0x9E3779B9u ^ (uint32_t)(raw+3) ^ ((uint32_t)count<<24);
    for(uint8_t i=0;i<count;i++){
      uint32_t x=targetSignature(pending[i]);
      h^=x + 0x9E3779B9u + (x<<6) + (x>>2);
    }
    return h;
  }

  void stageResults(int n){
    uint8_t count=0;
    if(n>0){
      for(int i=0;i<n && count<MAX_PENDING;i++){
        WifiTarget t{};
        strlcpy(t.ssid,WiFi.SSID(i).c_str(),sizeof(t.ssid));
        t.rssi=(int8_t)constrain(WiFi.RSSI(i),-127,0);
        t.channel=(uint8_t)WiFi.channel(i);
        t.band=Band::Band24;
        t.auth=(uint8_t)WiFi.encryptionType(i);
        t.lastSeen=millis();
        const uint8_t* b=WiFi.BSSID(i);
        if(b) memcpy(t.bssid.b,b,6);
        pending[count++]=t;
      }
    }
    const uint32_t sig=pendingSignature(count,n);
    pendingChanged_=!havePublishedSignature_ || sig!=lastPublishedSignature_;
    if(pendingChanged_){ lastPublishedSignature_=sig; havePublishedSignature_=true; }
    pendingCount=count; pendingRaw_=n; resultReady=true;
    lastScanRaw_=n; lastScanAt_=millis();
    WiFi.scanDelete();
    scanPhase=ScanPhase::Idle;
    state=Availability::Ready;
  }

  void prepareForScanBlocking(){
    // Exact FIX7 reset sequence, retained only for the synchronous fallback.
    esp_wifi_set_promiscuous(false);
    WiFi.scanDelete();
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false,false);
    delay(100);
    WiFi.mode(WIFI_OFF);
    delay(60);
    WiFi.mode(WIFI_STA);
    delay(120);
  }

  void runSynchronousFallback(){
    // A failed scan MUST NOT block the tank/UI/GPS loop for seconds. Publish
    // the failure and let Wardrive retry asynchronously on its next scan slot.
    // Keep the hardware-proven FIX7 blocking fallback outside Wardrive only.
    if(wardriveMode_){
      fallbackLastScan_=false;
      stageResults(WIFI_SCAN_FAILED);
      return;
    }
    fallbackLastScan_=true;
    scanPhase=ScanPhase::Idle;
    prepareForScanBlocking();
    const int n=WiFi.scanNetworks(false,true,false,scanMaxMs_,scanChannel_);
    stageResults(n);
  }

  int finishScan(TargetManager& tm,int n){
    if(n<0){ WiFi.scanDelete(); state=Availability::Ready; return n; }
    for(int i=0;i<n;i++){
      WifiTarget t{};
      strlcpy(t.ssid,WiFi.SSID(i).c_str(),sizeof(t.ssid));
      t.rssi=(int8_t)constrain(WiFi.RSSI(i),-127,0);
      t.channel=(uint8_t)WiFi.channel(i);
      t.band=Band::Band24;
      t.auth=(uint8_t)WiFi.encryptionType(i);
      t.lastSeen=millis();
      const uint8_t* b=WiFi.BSSID(i);
      if(b) memcpy(t.bssid.b,b,6);
      tm.observe(t);
    }
    WiFi.scanDelete();
    lastScanRaw_=n; lastScanAt_=millis(); fallbackLastScan_=false;
    state=Availability::Ready;
    return n;
  }

  static void rxThunk(void* buf,wifi_promiscuous_pkt_type_t type){ if(instance) instance->onRx(buf,type); }

  void rememberStation(const uint8_t* mac){
    if(!mac || (mac[0]&1)) return;
    Mac m{}; memcpy(m.b,mac,6);
    for(uint16_t i=0;i<uniqueCount;i++) if(macEq(uniqueStations[i],m)) return;
    if(uniqueCount<32) uniqueStations[uniqueCount++]=m;
  }

  static const char* cipherName(const uint8_t* s){
    if(!s) return "";
    switch(s[3]){
      case 1:return "WEP40";
      case 2:return "TKIP";
      case 4:return "CCMP";
      case 5:return "WEP104";
      case 8:return "GCMP";
      case 9:return "GCMP256";
      case 10:return "CCMP256";
      default:return "OTHER";
    }
  }

  static const char* akmName(const uint8_t* s){
    if(!s) return "";
    switch(s[3]){
      case 1:return "802.1X";
      case 2:return "PSK";
      case 3:return "FT-1X";
      case 4:return "FT-PSK";
      case 8:return "SAE";
      case 9:return "FT-SAE";
      case 12:return "SUITE-B";
      case 18:return "OWE";
      default:return "OTHER";
    }
  }

  void parseIes(WifiTarget& t,const uint8_t* ie,size_t len){
    size_t p=0;
    while(p+2<=len){
      uint8_t id=ie[p], l=ie[p+1]; p+=2;
      if(p+l>len) break;
      const uint8_t* d=ie+p;
      if(id==48 && l>=8){ // RSN
        size_t x=2;
        if(x+4<=l){ strlcpy(t.cipher,cipherName(d+x),sizeof(t.cipher)); x+=4; }
        if(x+2<=l){
          uint16_t pc=d[x]|(d[x+1]<<8); x+=2;
          if(pc && x+4<=l){ strlcpy(t.cipher,cipherName(d+x),sizeof(t.cipher)); }
          x+=4*pc;
        }
        if(x+2<=l){
          uint16_t ac=d[x]|(d[x+1]<<8); x+=2;
          if(ac && x+4<=l) strlcpy(t.akm,akmName(d+x),sizeof(t.akm));
          x+=4*ac;
        }
        if(x+2<=l){
          uint16_t caps=d[x]|(d[x+1]<<8);
          t.pmfRequired=(caps&(1<<6))!=0;
          t.pmfCapable=(caps&(1<<7))!=0;
        }
      } else if(id==5 && l>=2){ // TIM: DTIM count, DTIM period
        t.dtimPeriod=d[1];
      } else if(id==7 && l>=3){ // Country information
        t.country[0]=(char)d[0]; t.country[1]=(char)d[1]; t.country[2]=(char)d[2]; t.country[3]=0;
      } else if(id==45){ // HT capabilities (802.11n)
        t.ht=true;
      } else if(id==191){ // VHT capabilities (802.11ac)
        t.vht=true;
      } else if(id==255 && l>=1 && d[0]==35){ // HE capabilities extension (802.11ax)
        t.he=true;
      } else if(id==221 && l>=4 && d[0]==0x00 && d[1]==0x50 && d[2]==0xF2 && d[3]==0x04){
        t.wps=true;
      } else if(id==221 && l>=5 && d[0]==0x00 && d[1]==0x50 && d[2]==0xF2 && d[3]==0x02){
        t.wmm=true;
      }
      p+=l;
    }
  }

  void onRx(void* buf,wifi_promiscuous_pkt_type_t type){
    if(!buf) return;
    auto* pkt=(wifi_promiscuous_pkt_t*)buf;
    const uint8_t* p=pkt->payload;
    uint16_t n=pkt->rx_ctrl.sig_len; if(n>4) n-=4;
    if(n<24) return;
    uint8_t fc0=p[0];
    uint8_t frameType=fc0&0x0c;
    uint8_t subtype=(fc0>>4)&0x0f;

    if(statsMode){
      if(frameType==0x00){
        if(subtype==8) stats.beacons++;
        else if(subtype==4){ stats.probes++; rememberStation(p+10); }
        else if(subtype==12){ stats.deauth++; rememberStation(p+10); }
        else if(subtype==10){ stats.disassoc++; rememberStation(p+10); }
      } else if(frameType==0x08){ stats.data++; rememberStation(p+10); }
    }

    if(inspectTarget && frameType==0x00 && (subtype==8 || subtype==5)){
      const uint8_t* bssid=p+16;
      if(memcmp(bssid,inspectTarget->bssid.b,6)!=0) return;
      inspectTarget->rssi=(int8_t)constrain((int)pkt->rx_ctrl.rssi,-127,0);
      inspectTarget->beaconCount++;
      if(n>=36) inspectTarget->beaconInterval=(uint16_t)p[32] | ((uint16_t)p[33]<<8);
      if(n>36) parseIes(*inspectTarget,p+36,n-36);
      inspectFound=true;
    }
  }

  size_t buildBeacon(uint8_t* f,size_t cap,const char* ssid,uint8_t idx,uint8_t ch,uint16_t seq){
    size_t sl=strlen(ssid); if(sl>32 || cap<64+sl) return 0;
    size_t p=0;
    f[p++]=0x80; f[p++]=0x00; f[p++]=0; f[p++]=0;
    for(int i=0;i<6;i++) f[p++]=0xff;
    uint8_t mac[6]={0x02,0x44,0x49,0x52,0x4b,(uint8_t)(0x10+idx)};
    memcpy(f+p,mac,6); p+=6; memcpy(f+p,mac,6); p+=6;
    f[p++]=(uint8_t)(seq<<4); f[p++]=(uint8_t)(seq>>4);
    for(int i=0;i<8;i++) f[p++]=0;
    f[p++]=0x64; f[p++]=0x00; f[p++]=0x01; f[p++]=0x04;
    f[p++]=0; f[p++]=(uint8_t)sl; memcpy(f+p,ssid,sl); p+=sl;
    static const uint8_t rates[]={0x82,0x84,0x8b,0x96,0x0c,0x12,0x18,0x24};
    f[p++]=1; f[p++]=sizeof(rates); memcpy(f+p,rates,sizeof(rates)); p+=sizeof(rates);
    f[p++]=3; f[p++]=1; f[p++]=ch;
    return p;
  }
};
inline S3WifiProvider* S3WifiProvider::instance=nullptr;
}
