#pragma once
#include <Arduino.h>
#include <SD.h>
#include "../storage/StorageManager.h"
#include "../targets/TargetManager.h"

namespace dirk {

class BlueGuard {
public:
  enum class AlertKind : uint8_t { NewAp, MissingAp, SecurityChanged, ChannelChanged, PmfChanged, SsidChanged, KnownSsidNewBssid };
  struct Alert {
    AlertKind kind=AlertKind::NewAp;
    char ssid[33]{};
    Mac bssid{};
    Band band=Band::Unknown;
    uint8_t oldChannel=0,newChannel=0;
    uint8_t oldAuth=0,newAuth=0;
  };

  void begin(StorageManager* s,TargetManager* t){ storage=s; targets=t; loadBaseline(); }
  bool hasBaseline() const { return baselineCount_>0; }
  size_t baselineCount() const { return baselineCount_; }
  size_t alertCount() const { return alertCount_; }
  uint16_t newCount() const { return newCount_; }
  uint16_t missingCount() const { return missingCount_; }
  uint16_t changedCount() const { return changedCount_; }
  uint16_t cloneCount() const { return cloneCount_; }
  uint32_t compareSerial() const { return compareSerial_; }
  const Alert* alertAt(size_t i) const { return i<alertCount_?&alerts_[i]:nullptr; }

  const char* kindText(AlertKind k) const {
    switch(k){
      case AlertKind::NewAp:return "new ap";
      case AlertKind::MissingAp:return "missing";
      case AlertKind::SecurityChanged:return "security changed";
      case AlertKind::ChannelChanged:return "channel changed";
      case AlertKind::PmfChanged:return "pmf changed";
      case AlertKind::SsidChanged:return "ssid changed";
      case AlertKind::KnownSsidNewBssid:return "known ssid/new bssid";
    }
    return "change";
  }

  bool saveBaseline(){
    if(!storage || !storage->writable() || !targets) return false;
    if(SD.exists(pathTmp())) SD.remove(pathTmp());
    File f=SD.open(pathTmp(),FILE_WRITE); if(!f) return false;
    f.println("bssid,ssid,band,channel,auth,pmfc,pmfr,wps");
    size_t wrote=0; const uint32_t now=millis();
    for(size_t i=0;i<targets->count() && wrote<MAX_BASE;i++){
      const WifiTarget* t=targets->at(i); if(!t || !fresh(*t,now)) continue;
      char mac[18]; macText(t->bssid,mac,sizeof(mac));
      String ss=t->ssid; ss.replace(",","_");
      f.printf("%s,%s,%u,%u,%u,%u,%u,%u\n",mac,ss.c_str(),(unsigned)t->band,t->channel,t->auth,
        t->pmfCapable?1:0,t->pmfRequired?1:0,t->wps?1:0);
      wrote++;
    }
    f.flush(); f.close();
    if(!wrote){ SD.remove(pathTmp()); return false; }
    if(SD.exists(path())) SD.remove(path());
    if(!SD.rename(pathTmp(),path())) return false;
    bool ok=loadBaseline();
    if(ok) compareNow();
    return ok;
  }

  bool loadBaseline(){
    baselineCount_=0;
    if(!storage || !storage->mounted() || !SD.exists(path())) return false;
    File f=SD.open(path(),FILE_READ); if(!f) return false;
    bool first=true;
    while(f.available() && baselineCount_<MAX_BASE){
      String line=f.readStringUntil('\n'); line.trim();
      if(first){ first=false; continue; }
      if(!line.length()) continue;
      String fld[8]; int start=0;
      for(int i=0;i<7;i++){
        int p=line.indexOf(',',start); if(p<0){ start=-1; break; }
        fld[i]=line.substring(start,p); start=p+1;
      }
      if(start<0) continue; fld[7]=line.substring(start);
      BaselineEntry b{}; unsigned m[6];
      if(sscanf(fld[0].c_str(),"%x:%x:%x:%x:%x:%x",&m[0],&m[1],&m[2],&m[3],&m[4],&m[5])!=6) continue;
      for(int j=0;j<6;j++) b.bssid.b[j]=(uint8_t)m[j];
      strlcpy(b.ssid,fld[1].c_str(),sizeof(b.ssid)); b.band=(Band)fld[2].toInt(); b.channel=(uint8_t)fld[3].toInt(); b.auth=(uint8_t)fld[4].toInt();
      b.pmfCapable=fld[5].toInt()!=0; b.pmfRequired=fld[6].toInt()!=0; b.wps=fld[7].toInt()!=0;
      baseline_[baselineCount_++]=b;
    }
    f.close(); return baselineCount_>0;
  }

  void update(uint32_t wifiResultSerial){
    if(wifiResultSerial==lastWifiResultSerial_) return;
    lastWifiResultSerial_=wifiResultSerial;
    if(hasBaseline()) compareNow();
  }

  void compareNow(){
    alertCount_=0; newCount_=missingCount_=changedCount_=cloneCount_=0;
    if(!targets || !baselineCount_){ compareSerial_++; return; }
    const uint32_t now=millis();
    bool bandSeen24=false,bandSeen5=false;
    for(size_t i=0;i<targets->count();i++){
      const WifiTarget* t=targets->at(i); if(!t || !fresh(*t,now)) continue;
      if(t->band==Band::Band24) bandSeen24=true; else if(t->band==Band::Band5) bandSeen5=true;
    }

    for(size_t i=0;i<baselineCount_;i++){
      const BaselineEntry& b=baseline_[i];
      if((b.band==Band::Band24 && !bandSeen24) || (b.band==Band::Band5 && !bandSeen5)) continue;
      const WifiTarget* t=findCurrent(b.bssid,b.band,now);
      if(!t){ addAlert(AlertKind::MissingAp,b.ssid,b.bssid,b.band,b.channel,0,b.auth,0); missingCount_++; continue; }
      if(t->auth!=b.auth){ addAlert(AlertKind::SecurityChanged,t->ssid,t->bssid,t->band,b.channel,t->channel,b.auth,t->auth); changedCount_++; }
      if(t->channel!=b.channel){ addAlert(AlertKind::ChannelChanged,t->ssid,t->bssid,t->band,b.channel,t->channel,b.auth,t->auth); changedCount_++; }
      if(t->pmfCapable!=b.pmfCapable || t->pmfRequired!=b.pmfRequired){ addAlert(AlertKind::PmfChanged,t->ssid,t->bssid,t->band,b.channel,t->channel,b.auth,t->auth); changedCount_++; }
      if(strcmp(t->ssid,b.ssid)!=0){ addAlert(AlertKind::SsidChanged,t->ssid,t->bssid,t->band,b.channel,t->channel,b.auth,t->auth); changedCount_++; }
    }

    for(size_t i=0;i<targets->count();i++){
      const WifiTarget* t=targets->at(i); if(!t || !fresh(*t,now)) continue;
      if(findBaseline(t->bssid,t->band)>=0) continue;
      bool knownSsid=false;
      if(t->ssid[0]){
        for(size_t j=0;j<baselineCount_;j++) if(baseline_[j].band==t->band && strcmp(baseline_[j].ssid,t->ssid)==0){ knownSsid=true; break; }
      }
      addAlert(knownSsid?AlertKind::KnownSsidNewBssid:AlertKind::NewAp,t->ssid,t->bssid,t->band,0,t->channel,0,t->auth);
      newCount_++; if(knownSsid) cloneCount_++;
    }
    compareSerial_++;
  }

private:
  struct BaselineEntry { Mac bssid{}; char ssid[33]{}; Band band=Band::Unknown; uint8_t channel=0,auth=0; bool pmfCapable=false,pmfRequired=false,wps=false; };
  static constexpr size_t MAX_BASE=96,MAX_ALERTS=32;
  static const char* path(){ return "/dirk/blue/baseline.csv"; }
  static const char* pathTmp(){ return "/dirk/blue/baseline.tmp"; }
  StorageManager* storage=nullptr; TargetManager* targets=nullptr;
  BaselineEntry baseline_[MAX_BASE]{}; size_t baselineCount_=0;
  Alert alerts_[MAX_ALERTS]{}; size_t alertCount_=0;
  uint16_t newCount_=0,missingCount_=0,changedCount_=0,cloneCount_=0;
  uint32_t lastWifiResultSerial_=0,compareSerial_=0;

  static bool fresh(const WifiTarget& t,uint32_t now){ return t.lastSeen && (uint32_t)(now-t.lastSeen)<30000u; }
  static void macText(const Mac& m,char* out,size_t n){ snprintf(out,n,"%02X:%02X:%02X:%02X:%02X:%02X",m.b[0],m.b[1],m.b[2],m.b[3],m.b[4],m.b[5]); }
  int findBaseline(const Mac& mac,Band band) const { for(size_t i=0;i<baselineCount_;i++) if(baseline_[i].band==band && macEq(baseline_[i].bssid,mac)) return (int)i; return -1; }
  const WifiTarget* findCurrent(const Mac& mac,Band band,uint32_t now) const {
    if(!targets) return nullptr; for(size_t i=0;i<targets->count();i++){ const WifiTarget* t=targets->at(i); if(t && t->band==band && macEq(t->bssid,mac) && fresh(*t,now)) return t; } return nullptr;
  }
  void addAlert(AlertKind k,const char* ssid,const Mac& mac,Band band,uint8_t oldCh,uint8_t newCh,uint8_t oldAuth,uint8_t newAuth){
    if(alertCount_>=MAX_ALERTS) return; Alert& a=alerts_[alertCount_++]; a.kind=k; strlcpy(a.ssid,ssid&&ssid[0]?ssid:"<hidden>",sizeof(a.ssid)); a.bssid=mac; a.band=band; a.oldChannel=oldCh; a.newChannel=newCh; a.oldAuth=oldAuth; a.newAuth=newAuth;
  }
};

}
