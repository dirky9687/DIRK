#pragma once
#include <Arduino.h>
#include <SD.h>
#include <time.h>
#include <Preferences.h>
#include "../storage/StorageManager.h"
#include "../targets/TargetManager.h"
#include "../providers/s3/S3WifiProvider.h"
#include "../providers/c5/JanOSAdapter.h"
#include "../providers/ble/BleProvider.h"
#include "../providers/gps/GpsProvider.h"
#include "XpLedger.h"

namespace dirk {

enum class WardriveEventKind : uint8_t {
  None=0, Wifi24, Wifi5, HiddenWifi, OpenWifi, Ble, Distance, Milestone, LevelUp, GpsLost, GpsBack
};

struct WardriveStats {
  uint32_t startedAt=0;
  uint32_t unique24=0;
  uint32_t unique5=0;
  uint32_t uniqueBle=0;
  uint32_t savedWifi=0;
  uint32_t gpsFixes=0;
  uint32_t xp=0;
  uint16_t level=1;
  float distanceM=0;
};

struct WardriveCareerStats {
  uint32_t sessions=0;
  uint32_t wifi24=0;
  uint32_t wifi5=0;
  uint32_t ble=0;
  uint32_t gpsFixes=0;
  uint32_t bestWifiSession=0;
  float distanceM=0;
  float longestSessionM=0;
};

class WardriveManager {
public:
  static constexpr size_t MAX_SEEN_WIFI=256;
  static constexpr size_t MAX_SEEN_BLE=96;
  static constexpr size_t MAX_EXCLUDED=96;
  static constexpr uint16_t MAX_LEVEL=100;

  void begin(StorageManager* s,TargetManager* t,S3WifiProvider* w,JanOSAdapter* j,BleProvider* b,GpsProvider* g){
    storage=s; targets=t; wifi=w; janos=j; ble=b; gps=g;
    prefs.begin("dirk-wd",false);
    careerXp_=prefs.getUInt("xp",0);
    soundEnabled_=prefs.getBool("sound",true);
    career_.sessions=prefs.getUInt("sessions",0);
    career_.wifi24=prefs.getUInt("c24",0);
    career_.wifi5=prefs.getUInt("c5",0);
    career_.ble=prefs.getUInt("cble",0);
    career_.gpsFixes=prefs.getUInt("cgps",0);
    career_.bestWifiSession=prefs.getUInt("bestwifi",0);
    career_.distanceM=prefs.getFloat("cdist",0.0f);
    career_.longestSessionM=prefs.getFloat("longest",0.0f);
    stats_.xp=careerXp_;
    stats_.level=levelForXp(careerXp_);
  }

  bool start(bool use5=true,bool useBle=true,bool requireGps=false){
    startError_="";
    if(active){ startError_="wardrive already active"; return false; }
    if(!storage || !storage->writable()){ startError_="sd not writable"; return false; }
    if(!targets || !wifi || !gps){ startError_="provider unavailable"; return false; }
    want5=use5; wantBle=useBle; gpsRequired=requireGps;

    memset(&stats_,0,sizeof(stats_));
    stats_.xp=careerXp_; stats_.level=levelForXp(careerXp_); stats_.startedAt=millis();
    seenWifiCount=seenBleCount=excludedCount=0;
    xpQueueHead_=xpQueueTail_=xpQueueCount_=0; lastXpClaimAt_=0;
    lastDiscoveryAt=0; lastDiscoveryText="ready"; lastXpGain_=0; eventSerial_=0;
    lastEventKind_=WardriveEventKind::None; lastLevelUpAt_=0; previousLevel_=stats_.level;
    lastWifiScan=lastBleScan=last5Scan=0; lastTrack=0; lastWifiIngestAt_=lastBleIngestAt_=0; wifiIngestCursor_=bleIngestCursor_=0; haveTrack=false; prevTrackAt_=0; nextDistanceXpM_=250.0f;
    gpsWasValid_=false; gpsStateKnown_=false;
    nativeBuffer_=""; bleBuffer_=""; trackBuffer_="";
    nativeBuffer_.reserve(3072); bleBuffer_.reserve(2048); trackBuffer_.reserve(1536);
    lastLogFlush_=millis(); flushSlot_=0;

    makeSessionPath();
    active=true;
    wifi->setWardriveMode(true);
    sessionReady_=false; initStage_=1; lastInitStageAt_=millis();
    lastWifiScan=millis(); lastBleScan=millis(); last5Scan=millis();
    career_.sessions++; careerDirty_=true;
    return true;
  }

  void stop(){
    if(!active) return;
    if(janos && janos->scanning()) janos->stop();
    // Stop radio work first. In particular, do not release Wardrive mode
    // before cancelling the S3 scan: a timeout could otherwise execute the
    // blocking synchronous fallback in a non-Wardrive menu.
    if(wifi){ wifi->cancelWardriveScan(); wifi->setWardriveMode(false); }
    if(wantBle && ble) ble->cancelWardriveScan();
    // Resolve queued discoveries before leaving; persist IDs before XP.
    while(xpQueueCount_) processXpQueue(true);
    xpLedger_.flushAll();
    flushLogs(true);
    if(sessionReady_){ rebuildWigle(); writeStats(); }
    updateCareerRecords();
    saveProgress(true);
    saveCareer(true);
    mirrorProgressToSd();
    active=false;
  }

  void update(){
    if(!active) return;
    const uint32_t now=millis();
    // GPS is already serviced once per core loop in DirkApp::update().
    if(!sessionReady_){ serviceSessionInit(now); return; }

    const bool wifiBusy=wifi && wifi->scanning();
    const bool bleBusy=wantBle && ble && ble->scanning();

    // BLE and S3 Wi-Fi share the same 2.4GHz radio. Let the first Wi-Fi
    // sweep finish before any BLE scan; otherwise discovery can starve.
    if(wantBle && ble && (int32_t)(now-initialWifiPriorityUntil_)>=0 && !bleBusy && !wifiBusy && now-lastBleScan>=6500){
      if(ble->startScanAsync(1)) lastBleScan=now;
    }
    if(wantBle && ble && now-lastBleIngestAt_>=80u){ lastBleIngestAt_=now; ingestBle(4); }

    if(!wifiBusy && !(wantBle && ble && ble->scanning()) && now-lastWifiScan>=3600){
      if(wifi->startScanAsync(300,0)) lastWifiScan=now;
    }
    if(want5 && janos && !janos->scanning() && now-last5Scan>=3200){
      last5Scan=now;
      janos->startScan5();
    }

    if(now-lastWifiIngestAt_>=50u){ lastWifiIngestAt_=now; ingestWifi(4); }
    // XP identity checks are deliberately paced separately from RF ingestion.
    // A new AP must not trigger multiple SD lookups during one video frame.
    if(xpQueueCount_ && now-lastXpClaimAt_>=160u){
      lastXpClaimAt_=now; processXpQueue(false);
    }
    trackGps(now);
    serviceGpsState();
    flushLogs(false);
    if(xpLedger_.pending() && now-lastXpFlushAt_>=
        (xpLedger_.pending()>24u?180u:850u)){
      lastXpFlushAt_=now; xpLedger_.flushOne();
    }
    saveProgress(false);
  }

  bool running() const { return active; }
  bool uses5() const { return want5; }
  bool usesBle() const { return wantBle; }
  bool requiresGps() const { return gpsRequired; }
  const String& startError() const { return startError_; }
  void setUse5(bool v){ want5=v; }
  void setUseBle(bool v){ wantBle=v; }
  void setRequireGps(bool v){ gpsRequired=v; }

  bool soundEnabled() const { return soundEnabled_; }
  void setSoundEnabled(bool v){ if(soundEnabled_==v) return; soundEnabled_=v; prefs.putBool("sound",v); }
  void toggleSound(){ setSoundEnabled(!soundEnabled_); }

  const WardriveStats& stats() const { return stats_; }
  const WardriveCareerStats& career() const { return career_; }
  String sessionPath() const { return String(sessionDir); }
  uint32_t elapsedSeconds() const { return active?(millis()-stats_.startedAt)/1000:0; }
  const String& lastDiscovery() const { return lastDiscoveryText; }
  bool discoveryFlash() const { return active && lastDiscoveryAt && millis()-lastDiscoveryAt<900; }
  uint32_t lastDiscoveryAgeMs() const { return lastDiscoveryAt ? (uint32_t)(millis()-lastDiscoveryAt) : 0xFFFFFFFFu; }
  uint16_t lastXpGain() const { return lastXpGain_; }
  uint32_t eventSerial() const { return eventSerial_; }
  WardriveEventKind lastEventKind() const { return lastEventKind_; }

  bool shouldSave(uint32_t id) const {
    for(size_t i=0;i<excludedCount;i++) if(excluded[i]==id) return false;
    return true;
  }
  void toggleSave(uint32_t id){
    for(size_t i=0;i<excludedCount;i++) if(excluded[i]==id){ excluded[i]=excluded[--excludedCount]; return; }
    if(excludedCount<MAX_EXCLUDED) excluded[excludedCount++]=id;
  }

  bool providerConfigured(const char* name) const {
    String key=readProvider(name); return key.length()>3;
  }
  String providerHint(const char* name) const {
    String v=readProvider(name); if(!v.length()) return "not configured";
    return String("configured • ")+String(v.length())+" chars";
  }

  const char* rankTitle() const { return titleForLevel(stats_.level); }
  const char* tierTitle() const { return tierForLevel(stats_.level); }
  bool levelUpFlash() const { return active && lastLevelUpAt_ && millis()-lastLevelUpAt_<3600; }
  uint16_t previousLevel() const { return previousLevel_; }
  uint32_t levelFloorXp() const { return xpForLevel(stats_.level); }
  uint32_t nextLevelXp() const { return stats_.level>=MAX_LEVEL ? stats_.xp : xpForLevel((uint16_t)(stats_.level+1)); }
  uint8_t xpPercentToNext() const {
    if(stats_.level>=MAX_LEVEL) return 100;
    uint32_t lo=xpForLevel(stats_.level), hi=xpForLevel((uint16_t)(stats_.level+1));
    if(hi<=lo || stats_.xp<=lo) return 0;
    uint32_t p=((stats_.xp-lo)*100u)/(hi-lo);
    return (uint8_t)min((uint32_t)100,p);
  }

private:
  StorageManager* storage=nullptr; TargetManager* targets=nullptr; S3WifiProvider* wifi=nullptr;
  JanOSAdapter* janos=nullptr; BleProvider* ble=nullptr; GpsProvider* gps=nullptr;
  bool active=false,want5=true,wantBle=true,gpsRequired=false,soundEnabled_=true;
  uint32_t initialWifiPriorityUntil_=0;
  String startError_;
  WardriveStats stats_{}; WardriveCareerStats career_{};
  uint32_t lastWifiScan=0,lastBleScan=0,last5Scan=0,lastTrack=0,lastDiscoveryAt=0,lastWifiIngestAt_=0,lastBleIngestAt_=0;
  size_t wifiIngestCursor_=0,bleIngestCursor_=0;
  uint32_t seenWifi[MAX_SEEN_WIFI]{}; GpsFix seenWifiFix[MAX_SEEN_WIFI]{}; size_t seenWifiCount=0;
  char seenBle[MAX_SEEN_BLE][18]{}; size_t seenBleCount=0;
  uint32_t excluded[MAX_EXCLUDED]{}; size_t excludedCount=0;
  char sessionDir[96]{}; String lastDiscoveryText;
  uint16_t lastXpGain_=0; uint32_t eventSerial_=0; uint32_t lastLevelUpAt_=0; uint16_t previousLevel_=1;
  WardriveEventKind lastEventKind_=WardriveEventKind::None;
  bool haveTrack=false; double prevLat=0,prevLon=0; uint32_t prevTrackAt_=0; float nextDistanceXpM_=250.0f;
  bool gpsWasValid_=false,gpsStateKnown_=false;
  Preferences prefs; uint32_t careerXp_=0,lastProgressSave_=0; bool progressDirty_=false,careerDirty_=false;
  bool sessionReady_=false; uint8_t initStage_=0; uint32_t lastInitStageAt_=0;
  String nativeBuffer_,bleBuffer_,trackBuffer_; uint32_t lastLogFlush_=0; uint8_t flushSlot_=0;
  XpLedger xpLedger_;
  struct XpContact { uint8_t radio=0,band=0,flags=0; uint8_t mac[6]{}; };
  static constexpr size_t XP_QUEUE_CAP=128;
  XpContact xpQueue_[XP_QUEUE_CAP]{};
  size_t xpQueueHead_=0,xpQueueTail_=0,xpQueueCount_=0;
  uint32_t lastXpClaimAt_=0,lastXpFlushAt_=0;
  void queueXp(uint8_t radio,uint8_t band,const uint8_t mac[6],uint8_t flags){
    if(xpQueueCount_==XP_QUEUE_CAP) return; // no XP on overflow, never double-credit
    XpContact& c=xpQueue_[xpQueueHead_];
    c.radio=radio; c.band=band; c.flags=flags;
    memcpy(c.mac,mac,6);
    xpQueueHead_=(xpQueueHead_+1u)%XP_QUEUE_CAP; ++xpQueueCount_;
  }
  void processXpQueue(bool force){
    if(!xpQueueCount_) return;
    XpContact c=xpQueue_[xpQueueTail_];
    xpQueueTail_=(xpQueueTail_+1u)%XP_QUEUE_CAP; --xpQueueCount_;
    // Only grant the XP after the persistent ledger accepts the new contact.
    // When storage is unavailable, a possible duplicate never grants XP.
    const XpLedger::ClaimResult result=xpLedger_.claim(c.radio,c.mac);
    if(result!=XpLedger::ClaimResult::New) return;
    uint32_t before=stats_.xp;
    String text; WardriveEventKind kind;
    if(c.radio==1){
      if(c.band==5){career_.wifi5++;addXp(4);text="5g contact";kind=WardriveEventKind::Wifi5;}
      else {career_.wifi24++;addXp(2);text="new ap";kind=WardriveEventKind::Wifi24;}
      if(c.flags&1u){text="ghost contact";kind=WardriveEventKind::HiddenWifi;addXp(2);}
      if(c.flags&2u){text="open signal";kind=WardriveEventKind::OpenWifi;}
      const uint32_t total=career_.wifi24+career_.wifi5;
      if(total && (total%25u)==0u){addXp(10);text=String(total)+" lifetime contacts";kind=WardriveEventKind::Milestone;}
    }else{
      career_.ble++;addXp(1);text="ble contact";kind=WardriveEventKind::Ble;
      if((career_.ble%25u)==0u){addXp(6);text=String(career_.ble)+" lifetime ble";kind=WardriveEventKind::Milestone;}
    }
    careerDirty_=true;
    noteEvent(kind,text,(uint16_t)(stats_.xp-before));
    // Normal runtime flush is paced. On shutdown, write through immediately.
    if(force) xpLedger_.flushOne();
  }

  static double degToRad(double d){ return d*0.017453292519943295; }
  static float distMeters(double aLat,double aLon,double bLat,double bLon){
    const double R=6371000.0; double p1=degToRad(aLat),p2=degToRad(bLat),dp=degToRad(bLat-aLat),dl=degToRad(bLon-aLon);
    double h=sin(dp/2)*sin(dp/2)+cos(p1)*cos(p2)*sin(dl/2)*sin(dl/2);
    return (float)(R*2.0*atan2(sqrt(h),sqrt(1.0-h)));
  }
  static uint32_t freqFor(const WifiTarget& t){
    if(t.band==Band::Band24) return t.channel==14?2484:(2407u+5u*t.channel);
    if(t.channel>=1) return 5000u+5u*t.channel; return 0;
  }
  static String csvSafe(const char* s){ String x=s?s:""; x.replace("\"","'"); return x; }
  static String macText(const Mac& m){ char b[18]; snprintf(b,sizeof(b),"%02X:%02X:%02X:%02X:%02X:%02X",m.b[0],m.b[1],m.b[2],m.b[3],m.b[4],m.b[5]); return String(b); }

  void makeSessionPath(){
    time_t now=time(nullptr); struct tm tmv{};
    if(now>1700000000 && localtime_r(&now,&tmv)) snprintf(sessionDir,sizeof(sessionDir),"/dirk/wardrive/sessions/%04d%02d%02d_%02d%02d%02d",tmv.tm_year+1900,tmv.tm_mon+1,tmv.tm_mday,tmv.tm_hour,tmv.tm_min,tmv.tm_sec);
    else snprintf(sessionDir,sizeof(sessionDir),"/dirk/wardrive/sessions/boot_%010lu",(unsigned long)millis());
  }
  bool ensureDirs(){
    const char* dirs[]={"/dirk/wardrive","/dirk/wardrive/sessions","/dirk/wardrive/config"};
    for(auto d:dirs) if(!SD.exists(d) && !SD.mkdir(d)) return false;
    if(!SD.exists(sessionDir) && !SD.mkdir(sessionDir)) return false;
    return true;
  }
  String path(const char* f) const { return String(sessionDir)+"/"+f; }
  void serviceSessionInit(uint32_t now){
    if(now-stats_.startedAt<220 || now-lastInitStageAt_<70) return;
    lastInitStageAt_=now;
    switch(initStage_){
      case 1:
        if(!ensureDirs()){ active=false; if(wifi) wifi->setWardriveMode(false); initStage_=0; return; }
        // Parent directories must exist before creating the lifetime XP log.
        xpLedger_.begin();
        initStage_=2; break;
      case 2:
        storage->append(path("wigle.csv").c_str(),"WigleWifi-1.6,appRelease=DIRK-v0.7.16,model=CardputerADV,device=DIRK,board=ESP32S3,brand=M5Stack\n");
        storage->append(path("wigle.csv").c_str(),"MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type\n");
        initStage_=3; break;
      case 3:
        storage->append(path("ble.csv").c_str(),"address,name,rssi,lat,lon,sats,seen_ms\n");
        storage->append(path("track.csv").c_str(),"seen_ms,lat,lon,alt,hdop,sats\n");
        initStage_=4; break;
      case 4:
        storage->append(path("dirk.jsonl").c_str(),"{\"event\":\"session_start\",\"version\":\"0.7.16\"}\n");
        sessionReady_=true; initStage_=0;
        // First radio pass is 2.4 GHz only; BLE joins after one full sweep.
        lastWifiScan=now-4000; lastBleScan=now;
        initialWifiPriorityUntil_=now+11000u;
        last5Scan=now-3500;
        break;
      default: break;
    }
  }
  bool wifiSeen(uint32_t id) const { for(size_t i=0;i<seenWifiCount;i++) if(seenWifi[i]==id) return true; return false; }
  bool bleSeen(const char* a) const { for(size_t i=0;i<seenBleCount;i++) if(!strcasecmp(seenBle[i],a)) return true; return false; }

  void noteEvent(WardriveEventKind kind,const String& text,uint16_t xpGain){
    lastEventKind_=kind; lastDiscoveryText=text; lastDiscoveryAt=millis(); lastXpGain_=xpGain; eventSerial_++;
  }

  void ingestWifi(uint8_t budget){
    // In v0.7.4 every 50ms we scanned ALL targets from index zero, then
    // linearly rechecked the seen list. This became O(targets*seen) per tick.
    // Process at most 16 entries and advance a persistent round-robin cursor;
    // new APs remain discoverable without long UI stalls.
    GpsFix g=gps->fix(); uint8_t added=0;
    const size_t total=targets->count();
    if(!total){ wifiIngestCursor_=0; return; }
    if(wifiIngestCursor_>=total) wifiIngestCursor_=0;
    const size_t maxInspect=total<16u?total:16u;
    for(size_t inspected=0;inspected<maxInspect && added<budget;++inspected){
      const size_t i=wifiIngestCursor_++;
      if(wifiIngestCursor_>=total) wifiIngestCursor_=0;
      WifiTarget* t=targets->at(i); if(!t || (t->band!=Band::Band24 && t->band!=Band::Band5) || wifiSeen(t->id)) continue;
      if(t->lastSeen<stats_.startedAt) continue;
      if(seenWifiCount>=MAX_SEEN_WIFI) break;
      seenWifi[seenWifiCount]=t->id; seenWifiFix[seenWifiCount]=g; seenWifiCount++; added++;
      if(t->band==Band::Band5) stats_.unique5++;
      else stats_.unique24++;
      // Session counts/logging continue for repeat sightings; discovery XP
      // is decided by MAC against the on-SD lifetime registry, not target ID.
      const uint8_t flags=(!t->ssid[0] || !strcmp(t->ssid,"<hidden>")?1u:0u)
                         | (t->auth==WIFI_AUTH_OPEN?2u:0u);
      queueXp(1,t->band==Band::Band5?5:24,t->bssid.b,flags);
      if(shouldSave(t->id)){
        appendNative(*t,g);
        if(g.valid) stats_.savedWifi++;
      }
    }
  }
  void appendNative(const WifiTarget& t,const GpsFix& g){
    nativeBuffer_ += String("{\"type\":\"wifi\",\"band\":")+String(t.band==Band::Band5?5:24)+",\"bssid\":\""+macText(t.bssid)+"\",\"ssid\":\""+csvSafe(t.ssid)+"\",\"ch\":"+String(t.channel)+",\"rssi\":"+String(t.rssi)+",\"lat\":"+(g.valid?String(g.lat,6):"null")+",\"lon\":"+(g.valid?String(g.lon,6):"null")+"}\n";
  }
  void ingestBle(uint8_t budget){
    if(!ble) return; GpsFix g=gps->fix(); uint8_t added=0;
    const size_t total=ble->count();
    if(!total){ bleIngestCursor_=0; return; }
    if(bleIngestCursor_>=total) bleIngestCursor_=0;
    const size_t maxInspect=total<12u?total:12u;
    for(size_t inspected=0;inspected<maxInspect && added<budget;++inspected){
      const size_t i=bleIngestCursor_++;
      if(bleIngestCursor_>=total) bleIngestCursor_=0;
      const BleTarget* t=ble->at(i); if(!t || bleSeen(t->address)) continue;
      if(seenBleCount>=MAX_SEEN_BLE) break;
      strlcpy(seenBle[seenBleCount++],t->address,sizeof(seenBle[0])); added++;
      stats_.uniqueBle++;
      // BLE identity is tracked separately from Wi-Fi BSSIDs, also lifelong.
      uint8_t bleMac[6]{};
      unsigned parts[6]{};
      if(sscanf(t->address,"%x:%x:%x:%x:%x:%x",&parts[0],&parts[1],&parts[2],&parts[3],&parts[4],&parts[5])==6){
        for(int k=0;k<6;k++) bleMac[k]=(uint8_t)parts[k];
        queueXp(2,0,bleMac,0);
      }
      bleBuffer_ += String(t->address)+",\""+csvSafe(t->name)+"\","+String(t->rssi)+","+(g.valid?String(g.lat,6):"")+","+(g.valid?String(g.lon,6):"")+","+String(g.sats)+","+String(millis())+"\n";
    }
  }
  void trackGps(uint32_t now){
    if(now-lastTrack<1000) return; lastTrack=now; GpsFix g=gps->fix(); if(!g.valid) return;
    stats_.gpsFixes++; career_.gpsFixes++; careerDirty_=true;
    if(haveTrack){
      float d=distMeters(prevLat,prevLon,g.lat,g.lon);
      float dt=prevTrackAt_ ? (now-prevTrackAt_)/1000.0f : 1.0f;
      float speedMps=g.speedKph/3.6f;
      float maxStep=max(18.0f,speedMps*dt*2.2f+10.0f);
      bool quality=(g.sats>=4) && (g.hdop<=0.0f || g.hdop<8.0f);
      if(quality && d>=1.25f && d<=maxStep){ stats_.distanceM+=d; career_.distanceM+=d; }
    }
    if(stats_.distanceM>=nextDistanceXpM_){
      uint32_t before=stats_.xp; addXp(8);
      noteEvent(WardriveEventKind::Distance,String((int)nextDistanceXpM_)+"m recon",(uint16_t)(stats_.xp-before));
      nextDistanceXpM_+=250.0f;
    }
    prevLat=g.lat; prevLon=g.lon; prevTrackAt_=now; haveTrack=true;
    trackBuffer_ += String(now)+","+String(g.lat,6)+","+String(g.lon,6)+","+String(g.alt,1)+","+String(g.hdop,1)+","+String(g.sats)+"\n";
  }

  void serviceGpsState(){
    if(!gps) return; bool valid=gps->fix().valid;
    if(!gpsStateKnown_){ gpsStateKnown_=true; gpsWasValid_=valid; return; }
    if(valid==gpsWasValid_) return;
    gpsWasValid_=valid;
    noteEvent(valid?WardriveEventKind::GpsBack:WardriveEventKind::GpsLost,valid?"nav green":"nav went dark",0);
  }

  static uint32_t xpForLevel(uint16_t level){
    if(level<=1) return 0;
    if(level>MAX_LEVEL) level=MAX_LEVEL;
    uint32_t n=(uint32_t)(level-1);
    return 20u*n + 6u*n*n + (n*n*n)/2u;
  }
  static uint16_t levelForXp(uint32_t xp){
    uint16_t level=1;
    for(uint16_t l=2;l<=MAX_LEVEL;l++){ if(xp>=xpForLevel(l)) level=l; else break; }
    return level;
  }
  static const char* tierForLevel(uint16_t lvl){
    static const char* tiers[]={"B00T CAMP","F13LD 0PS","S1GN4L C0RPS","GH0ST D1V","BL4CK N3T","CYB3R C0M","SP3CTRUM CMD","SH4D0W C0RPS","R00T AUTH","1337 C0MM4ND"};
    if(lvl<1) lvl=1; if(lvl>100) lvl=100; return tiers[(lvl-1)/10];
  }
  static const char* titleForLevel(uint16_t lvl){
    static const char* titles[]={
      "n00b Recruit","Packet Private","Beacon Grunt","Signal Scout","Recon Cadet","MAC Tracker","Channel Runner","Probe Trooper","RF Specialist","Packet Corporal",
      "Cipher Sergeant","Beacon Sergeant","Packet Sapper","Recon Sergeant","Signal Lieutenant","RF Lieutenant","Spectrum Officer","Probe Captain","Packet Captain","Recon 0p3r4t0r",
      "Signal Gunner","Beacon Ranger","Channel Sergeant","RF Pathfinder","Spectrum Scout","Packet Warden","Ether Lieutenant","Frequency Captain","Signal Major","S1GN4L Commander",
      "Ghost Lieutenant","Null Captain","Shadow Operator","Phantom Major","R00T Major","Kernel Colonel","Ghost Colonel","Null Commander","0wn4g3 Commander","GH0ST C0MM4ND",
      "Blacknet Scout","Cipher Hunter","Packet Raider","Shadow Sapper","Null Ranger","Darknet Major","Ghost Marshal","Blacknet Colonel","Cipher Commander","BL4CK N3T 0P",
      "Cyber Lieutenant","Packet Strategist","Recon Major","Signal Commander","Kernel Brigadier","Cyber Colonel","Network General","Ether Commander","Command Root","CYB3R C0MM4ND",
      "Spectrum Lt","RF Captain","Channel Major","Spectrum Major","Ether Colonel","RF Commander","Signal General","Spectrum General","Ether Marshal","SP3CTRUM C0MM4ND",
      "Shadow Lieutenant","Ghost Captain","Phantom Colonel","Night Operator","Shadow Major","Null Marshal","Ghost General","Phantom General","Shadow Commander","SH4D0W C0MM4ND",
      "Root Marshal","Kernel Marshal","Zero-Day General","Ether General","Packet Admiral","Spectrum Admiral","Shadow Supreme","R00T Supreme","Lord of Ether","R00T AUTH0R1TY",
      "1337 Operator","1337 Commander","Elite Root","Zero-Day Marshal","Ether Warlord","Spectrum Overlord","Ghost in Command","R00T Overlord","L0RD 0F TH3 L4N3S","1337 SUPR3M3"
    };
    if(lvl<1) lvl=1; if(lvl>100) lvl=100; return titles[lvl-1];
  }
  void addXp(uint16_t n){
    uint16_t before=stats_.level;
    stats_.xp+=n; careerXp_=stats_.xp; stats_.level=levelForXp(stats_.xp); progressDirty_=true;
    if(stats_.level>before){ previousLevel_=before; lastLevelUpAt_=millis(); lastEventKind_=WardriveEventKind::LevelUp; }
  }
  void saveProgress(bool force){
    if(!progressDirty_ && !force) return;
    uint32_t now=millis(); if(!force && now-lastProgressSave_<20000) return;
    prefs.putUInt("xp",careerXp_); prefs.putUShort("level",levelForXp(careerXp_));
    lastProgressSave_=now; progressDirty_=false;
  }
  void saveCareer(bool force){
    if(!careerDirty_ && !force) return;
    prefs.putUInt("sessions",career_.sessions); prefs.putUInt("c24",career_.wifi24); prefs.putUInt("c5",career_.wifi5);
    prefs.putUInt("cble",career_.ble); prefs.putUInt("cgps",career_.gpsFixes); prefs.putUInt("bestwifi",career_.bestWifiSession);
    prefs.putFloat("cdist",career_.distanceM); prefs.putFloat("longest",career_.longestSessionM); careerDirty_=false;
  }
  void updateCareerRecords(){
    uint32_t wifiTotal=stats_.unique24+stats_.unique5;
    if(wifiTotal>career_.bestWifiSession) career_.bestWifiSession=wifiTotal;
    if(stats_.distanceM>career_.longestSessionM) career_.longestSessionM=stats_.distanceM;
    careerDirty_=true;
  }
  void mirrorProgressToSd(){
    if(!storage || !storage->writable()) return;
    if(!SD.exists("/dirk/wardrive")) SD.mkdir("/dirk/wardrive");
    if(!SD.exists("/dirk/wardrive/config")) SD.mkdir("/dirk/wardrive/config");
    File f=SD.open("/dirk/wardrive/config/progression.json",FILE_WRITE);
    if(f){ f.print(String("{\"xp\":")+String(careerXp_)+",\"level\":"+String(levelForXp(careerXp_))+",\"tier\":\""+tierForLevel(levelForXp(careerXp_))+"\",\"title\":\""+titleForLevel(levelForXp(careerXp_))+"\"}\n"); f.close(); }
  }

  void flushLogs(bool force){
    if(!storage || !storage->writable()) return;
    // SD open/write/close causes visible animation hitches on some cards.
    // Reduce ordinary flush frequency, but bound RAM exposure under heavy RF
    // traffic by forcing a flush when any one queue exceeds 4 KiB.
    uint32_t now=millis();
    if(!force && now-lastLogFlush_<2500u &&
       nativeBuffer_.length()<4096u && bleBuffer_.length()<4096u && trackBuffer_.length()<4096u) return;
    lastLogFlush_=now;
    auto flushOne=[&](String& buf,const char* name){ if(!buf.length()) return false; bool ok=storage->append(path(name).c_str(),buf); if(ok) buf=""; return true; };
    if(force){ flushOne(nativeBuffer_,"dirk.jsonl"); flushOne(bleBuffer_,"ble.csv"); flushOne(trackBuffer_,"track.csv"); return; }
    for(uint8_t tries=0;tries<3;tries++){
      uint8_t slot=(uint8_t)((flushSlot_+tries)%3u); bool done=false;
      if(slot==0) done=flushOne(nativeBuffer_,"dirk.jsonl"); else if(slot==1) done=flushOne(bleBuffer_,"ble.csv"); else done=flushOne(trackBuffer_,"track.csv");
      if(done){ flushSlot_=(uint8_t)((slot+1)%3u); break; }
    }
  }

  void rebuildWigle(){
    String fp=path("wigle.csv");
    if(SD.exists(fp.c_str())) SD.remove(fp.c_str());
    File f=SD.open(fp.c_str(),FILE_WRITE); if(!f) return;
    f.println("WigleWifi-1.6,appRelease=DIRK-v0.7.16,model=CardputerADV,device=DIRK,board=ESP32S3,brand=M5Stack");
    f.println("MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type");
    stats_.savedWifi=0;
    for(size_t i=0;i<seenWifiCount;i++){
      WifiTarget* t=targets?targets->byId(seenWifi[i]):nullptr; const GpsFix& g=seenWifiFix[i];
      if(!t || !g.valid || !shouldSave(t->id)) continue;
      String auth=t->auth==WIFI_AUTH_OPEN?"[OPEN]":"[WPA]";
#ifdef WIFI_AUTH_WPA3_PSK
      if(t->auth==WIFI_AUTH_WPA3_PSK) auth="[WPA3_SAE]";
#endif
      if(t->auth==WIFI_AUTH_WPA2_PSK) auth="[WPA2_PSK]";
      String row=macText(t->bssid)+",\""+csvSafe(t->ssid)+"\","+auth+",,"+String(t->channel)+","+String(freqFor(*t))+","+String(t->rssi)+","+String(g.lat,6)+","+String(g.lon,6)+","+String(g.alt,1)+","+String(g.hdop,1)+",,,WIFI\n";
      f.print(row); stats_.savedWifi++;
    }
    f.flush(); f.close();
  }
  void writeStats(){
    String s="{\"version\":\"0.7.16\",\"wifi24\":"+String(stats_.unique24)+",\"wifi5\":"+String(stats_.unique5)+",\"ble\":"+String(stats_.uniqueBle)+",\"gps_fixes\":"+String(stats_.gpsFixes)+",\"distance_m\":"+String(stats_.distanceM,1)+",\"xp\":"+String(stats_.xp)+",\"level\":"+String(stats_.level)+",\"tier\":\""+tierForLevel(stats_.level)+"\"}\n";
    storage->append(path("stats.json").c_str(),s);
  }
  String readProvider(const char* name) const {
    if(!storage || !storage->mounted()) return String();
    File f=SD.open("/dirk/wardrive/config/providers.ini",FILE_READ); if(!f) return String();
    String prefix=String(name)+"="; String out;
    while(f.available()){ String line=f.readStringUntil('\n'); line.trim(); if(line.startsWith(prefix)){ out=line.substring(prefix.length()); out.trim(); break; } }
    f.close(); return out;
  }
};
}
