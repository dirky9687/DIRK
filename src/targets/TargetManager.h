#pragma once
#include <dirk/types.h>
#include "ApNameRetention.h"
#include <SD.h>
#include "../storage/StorageManager.h"

namespace dirk {
class TargetManager {
public:
  static constexpr size_t MAX=256;
  void begin(StorageManager* s=nullptr){ storage=s; count_=0; next_=1; }
  void setStorage(StorageManager* s){ storage=s; }

  void clearUnsaved(){
    size_t w=0;
    for(size_t i=0;i<count_;++i) if(items[i].saved) items[w++]=items[i];
    count_=w;
  }

  void clearUnsavedBand(Band band){
    size_t w=0;
    for(size_t i=0;i<count_;++i){
      if(items[i].saved || items[i].band!=band) items[w++]=items[i];
    }
    count_=w;
  }

  WifiTarget* byId(uint32_t id){
    for(size_t i=0;i<count_;++i) if(items[i].id==id) return &items[i];
    return nullptr;
  }

  WifiTarget* observe(const WifiTarget& in){
    for(size_t i=0;i<count_;++i){
      if(macEq(items[i].bssid,in.bssid) && items[i].band==in.band){
        bool saved=items[i].saved, authz=items[i].authorized;
        uint32_t id=items[i].id, first=items[i].firstSeen, beacons=items[i].beaconCount;
        char previousSsid[33]{};
        const bool keepC5Ssid=retainObservedC5Name(in.band,in.ssid,items[i].ssid);
        if(keepC5Ssid) strlcpy(previousSsid,items[i].ssid,sizeof(previousSsid));
        char cipher[16]; char akm[16];
        strlcpy(cipher,items[i].cipher,sizeof(cipher));
        strlcpy(akm,items[i].akm,sizeof(akm));
        bool pmfc=items[i].pmfCapable, pmfr=items[i].pmfRequired, wps=items[i].wps;
        uint16_t beaconInterval=items[i].beaconInterval; uint8_t dtim=items[i].dtimPeriod;
        char country[4]; strlcpy(country,items[i].country,sizeof(country));
        bool ht=items[i].ht,vht=items[i].vht,he=items[i].he,wmm=items[i].wmm;
        items[i]=in; items[i].id=id; items[i].saved=saved; items[i].authorized=authz; items[i].firstSeen=first;
        // Only a same-BSSID 5 GHz refresh may reuse a previously observed name.
        // The new RSSI, timestamp and channel still come from the latest scan.
        if(keepC5Ssid) strlcpy(items[i].ssid,previousSsid,sizeof(items[i].ssid));
        if(!items[i].cipher[0]) strlcpy(items[i].cipher,cipher,sizeof(items[i].cipher));
        if(!items[i].akm[0]) strlcpy(items[i].akm,akm,sizeof(items[i].akm));
        items[i].pmfCapable = items[i].pmfCapable || pmfc;
        items[i].pmfRequired = items[i].pmfRequired || pmfr;
        items[i].wps = items[i].wps || wps;
        if(!items[i].beaconInterval) items[i].beaconInterval=beaconInterval;
        if(!items[i].dtimPeriod) items[i].dtimPeriod=dtim;
        if(!items[i].country[0]) strlcpy(items[i].country,country,sizeof(items[i].country));
        items[i].ht=items[i].ht||ht; items[i].vht=items[i].vht||vht; items[i].he=items[i].he||he; items[i].wmm=items[i].wmm||wmm;
        if(!items[i].beaconCount) items[i].beaconCount=beacons;
        return &items[i];
      }
    }
    if(count_>=MAX) return nullptr;
    items[count_]=in; items[count_].id=next_++; items[count_].firstSeen=millis();
    return &items[count_++];
  }

  size_t count() const { return count_; }
  WifiTarget* at(size_t i){ return i<count_?&items[i]:nullptr; }
  const WifiTarget* at(size_t i) const { return i<count_?&items[i]:nullptr; }

  bool save(uint32_t id,bool authorized){
    for(size_t i=0;i<count_;++i){
      if(items[i].id==id){
        items[i].saved=true; items[i].authorized=authorized;
        return persist();
      }
    }
    return false;
  }

  bool persist(){
    if(!storage || !storage->writable()) return false;
    const char* tmp="/dirk/targets/saved.tmp";
    const char* dst="/dirk/targets/saved.csv";
    if(SD.exists(tmp)) SD.remove(tmp);
    File f=SD.open(tmp,FILE_WRITE); if(!f) return false;
    f.println("bssid,ssid,rssi,channel,band,auth,cipher,akm,pmfc,pmfr,wps,authorized");
    for(size_t i=0;i<count_;++i){
      const auto& t=items[i]; if(!t.saved) continue;
      char mac[18]; snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",t.bssid.b[0],t.bssid.b[1],t.bssid.b[2],t.bssid.b[3],t.bssid.b[4],t.bssid.b[5]);
      String ss=t.ssid; ss.replace(",","_");
      String ci=t.cipher; ci.replace(",","_");
      String ak=t.akm; ak.replace(",","_");
      f.printf("%s,%s,%d,%u,%u,%u,%s,%s,%u,%u,%u,%u\n",
        mac,ss.c_str(),t.rssi,t.channel,(unsigned)t.band,t.auth,ci.c_str(),ak.c_str(),
        t.pmfCapable?1:0,t.pmfRequired?1:0,t.wps?1:0,t.authorized?1:0);
    }
    f.flush(); f.close();
    if(SD.exists(dst)) SD.remove(dst);
    if(!SD.rename(tmp,dst)) return false;
    File v=SD.open(dst,FILE_READ);
    bool ok=v && v.size()>0; if(v) v.close();
    return ok;
  }

  bool loadSaved(){
    if(!storage || !storage->mounted() || !SD.exists("/dirk/targets/saved.csv")) return false;
    File f=SD.open("/dirk/targets/saved.csv",FILE_READ); if(!f) return false;
    bool first=true; bool any=false;
    while(f.available() && count_<MAX){
      String line=f.readStringUntil('\n'); line.trim();
      if(first){ first=false; continue; }
      if(!line.length()) continue;
      String fld[12]; int start=0;
      for(int i=0;i<11;i++){
        int p=line.indexOf(',',start);
        if(p<0){ fld[i]=line.substring(start); start=line.length(); break; }
        fld[i]=line.substring(start,p); start=p+1;
      }
      fld[11]=line.substring(start);
      WifiTarget t{}; unsigned m[6];
      if(sscanf(fld[0].c_str(),"%x:%x:%x:%x:%x:%x",&m[0],&m[1],&m[2],&m[3],&m[4],&m[5])!=6) continue;
      for(int i=0;i<6;i++) t.bssid.b[i]=(uint8_t)m[i];
      strlcpy(t.ssid,fld[1].c_str(),sizeof(t.ssid));
      t.rssi=(int8_t)fld[2].toInt();
      t.channel=(uint8_t)fld[3].toInt();
      t.band=(Band)fld[4].toInt();
      t.auth=(uint8_t)fld[5].toInt();
      strlcpy(t.cipher,fld[6].c_str(),sizeof(t.cipher));
      strlcpy(t.akm,fld[7].c_str(),sizeof(t.akm));
      t.pmfCapable=fld[8].toInt()!=0;
      t.pmfRequired=fld[9].toInt()!=0;
      t.wps=fld[10].toInt()!=0;
      t.saved=true;
      t.authorized=fld[11].toInt()!=0;
      WifiTarget* p=observe(t);
      if(p){ p->saved=true; p->authorized=t.authorized; any=true; }
    }
    f.close(); return any;
  }

private:
  StorageManager* storage=nullptr;
  WifiTarget items[MAX]{};
  size_t count_=0;
  uint32_t next_=1;
};
}
