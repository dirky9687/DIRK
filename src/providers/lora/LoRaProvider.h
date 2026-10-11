#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <M5Unified.h>
#include <Preferences.h>
#include <dirk/types.h>
#include "utility/PI4IOE5V6408_Class.hpp"
#include "../../storage/StorageManager.h"
#include "../gps/GpsProvider.h"

namespace dirk {

enum class LoRaRegion : uint8_t { US915=0, EU868=1, AU915=2 };
enum class LoRaPreset : uint8_t { Balanced=0, LongRange=1, Fast=2 };

struct LoRaMessage {
  uint32_t from=0;
  uint32_t to=0;
  uint32_t seq=0;
  uint32_t at=0;
  int16_t rssi=-127;
  float snr=0;
  bool outgoing=false;
  char text[112]{};
};

struct LoRaNode {
  uint32_t id=0;
  char name[18]{};
  int16_t rssi=-127;
  float snr=0;
  uint32_t lastSeen=0;
  bool hasPosition=false;
  double lat=0;
  double lon=0;
};

class LoRaProvider {
public:
  static constexpr uint8_t MAX_MESSAGES=24;
  static constexpr uint8_t MAX_NODES=24;

  bool begin(StorageManager* s,GpsProvider* g){
    storage=s; gps=g; state_=Availability::Initializing; lastError_="";
    prefs.begin("dirk-lora",false);
    loadSettings();
    nodeId_=(uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFu);
    snprintf(nodeName_,sizeof(nodeName_),"dirk-%04lx",(unsigned long)(nodeId_ & 0xFFFFu));

    // Cardputer ADV Cap LoRa-1262 RF path. M5Cardputer.begin() has already
    // initialized the internal I2C bus on GPIO8/9, so only probe the expander.
    if(!ioe.begin()){
      lastError_="PI4IOE5V6408 not found";
      state_=Availability::Unavailable;
      return false;
    }
    ioe.setDirection(0,true);
    ioe.setHighImpedance(0,false);
    ioe.digitalWrite(0,true); // enable SX1262 RF antenna switch

    pinMode(12,OUTPUT); digitalWrite(12,HIGH); // keep microSD deselected during radio init
    pinMode(5,OUTPUT); digitalWrite(5,HIGH);

    int16_t rc=applyRadioConfig(true);
    if(rc!=RADIOLIB_ERR_NONE){
      lastRadioCode_=rc;
      lastError_=String("SX1262 init ")+String(rc);
      state_=Availability::Error;
      return false;
    }
    radio.setPacketReceivedAction(&LoRaProvider::rxThunk);
    rc=radio.startReceive();
    if(rc!=RADIOLIB_ERR_NONE){
      lastRadioCode_=rc; lastError_=String("RX start ")+String(rc); state_=Availability::Error; return false;
    }
    state_=Availability::Ready;
    lastNodeBeacon_=millis();
    if(storage && storage->writable() && !storage->exists("/dirk/lora/chat.csv"))
      storage->append("/dirk/lora/chat.csv","seen_ms,direction,node_id,rssi,snr,text\n");
    logLine(String("# DIRK LoRa started node=")+hexId(nodeId_)+"\n");
    return true;
  }

  void update(){
    if(state_!=Availability::Ready && state_!=Availability::Busy) return;
    if(rxFlag_){
      rxFlag_=false;
      String packet;
      int16_t rc=radio.readData(packet);
      lastRadioCode_=rc;
      if(rc==RADIOLIB_ERR_NONE){
        lastRssi_=radio.getRSSI();
        lastSnr_=radio.getSNR();
        handlePacket(packet,lastRssi_,lastSnr_);
      }
      radio.startReceive();
      state_=Availability::Ready;
    }

    const uint32_t now=millis();
    if(now-lastNodeBeacon_>60000u){ lastNodeBeacon_=now; sendNodeBeacon(); }
    if(positionBeacon_ && gps && gps->fixed() && now-lastPositionBeacon_>30000u){ lastPositionBeacon_=now; sendPosition(); }
  }

  Availability availability() const { return state_; }
  bool ready() const { return state_==Availability::Ready || state_==Availability::Busy; }
  const String& lastError() const { return lastError_; }
  int16_t lastRadioCode() const { return lastRadioCode_; }
  int16_t lastRssi() const { return lastRssi_; }
  float lastSnr() const { return lastSnr_; }
  uint32_t nodeId() const { return nodeId_; }
  const char* nodeName() const { return nodeName_; }
  uint32_t txCount() const { return txCount_; }
  uint32_t rxCount() const { return rxCount_; }
  uint32_t relayCount() const { return relayCount_; }
  uint32_t droppedCount() const { return droppedCount_; }
  uint32_t messageSerial() const { return messageSerial_; }
  uint32_t nodeSerial() const { return nodeSerial_; }
  size_t messageCount() const { return msgCount_; }
  const LoRaMessage* messageAt(size_t ordinal) const {
    if(ordinal>=msgCount_) return nullptr;
    uint8_t idx=(uint8_t)((msgHead_+MAX_MESSAGES-msgCount_+ordinal)%MAX_MESSAGES);
    return &messages_[idx];
  }
  size_t nodeCount() const { return nodeCount_; }
  const LoRaNode* nodeAt(size_t i) const { return i<nodeCount_?&nodes_[i]:nullptr; }

  bool sendChat(const String& text,uint8_t ttl=2){
    String clean=sanitize(text); if(!clean.length()) return false;
    uint32_t seq=++seq_;
    String p=buildPacket('C',seq,ttl,clean);
    if(!sendRaw(p)) return false;
    rememberPacket(nodeId_,seq);
    pushMessage(nodeId_,0,seq,clean,true,0,0);
    logChat("tx",nodeId_,clean,0,0);
    return true;
  }

  bool sendDirect(uint32_t dest,const String& text,uint8_t ttl=2){
    if(!dest || dest==nodeId_) return false;
    String clean=sanitize(text); if(!clean.length()) return false;
    uint32_t seq=++seq_;
    String payload=hexId(dest)+","+clean;
    if(!sendRaw(buildPacket('D',seq,ttl,payload))) return false;
    rememberPacket(nodeId_,seq);
    pushMessage(nodeId_,dest,seq,clean,true,0,0);
    logChat("tx-dm",dest,clean,0,0);
    return true;
  }

  bool sendPosition(uint8_t ttl=2){
    if(!gps || !gps->fixed()) return false;
    GpsFix f=gps->fix();
    String payload=String(f.lat,6)+","+String(f.lon,6)+","+String(f.alt,0)+","+String(f.sats);
    uint32_t seq=++seq_;
    if(!sendRaw(buildPacket('P',seq,ttl,payload))) return false;
    rememberPacket(nodeId_,seq);
    return true;
  }

  bool sendNodeBeacon(uint8_t ttl=1){
    uint32_t seq=++seq_;
    if(!sendRaw(buildPacket('N',seq,ttl,String(nodeName_)))) return false;
    rememberPacket(nodeId_,seq);
    return true;
  }

  bool sendRangePing(){
    uint32_t seq=++seq_;
    rangeSent_++; lastRangeSeq_=seq; lastRangeAt_=millis();
    if(!sendRaw(buildPacket('R',seq,0,String("ping,")+String(rangeSent_)))) return false;
    rememberPacket(nodeId_,seq);
    return true;
  }

  uint32_t rangeSent() const { return rangeSent_; }
  uint32_t rangeReplies() const { return rangeReplies_; }
  int16_t rangeLastRssi() const { return rangeLastRssi_; }
  float rangeLastSnr() const { return rangeLastSnr_; }

  void setPositionBeacon(bool on){ positionBeacon_=on; prefs.putBool("pos",positionBeacon_); }
  bool positionBeacon() const { return positionBeacon_; }
  void setRelay(bool on){ relayEnabled_=on; prefs.putBool("relay",relayEnabled_); }
  bool relayEnabled() const { return relayEnabled_; }
  void setHopLimit(uint8_t h){ hopLimit_=constrain((int)h,0,5); prefs.putUChar("hops",hopLimit_); }
  uint8_t hopLimit() const { return hopLimit_; }
  void setTxPower(int8_t p){ txPower_=constrain((int)p,2,22); prefs.putChar("txp",txPower_); if(ready()) radio.setOutputPower(txPower_); }
  int8_t txPower() const { return txPower_; }
  LoRaRegion region() const { return region_; }
  LoRaPreset preset() const { return preset_; }
  const char* regionName() const { switch(region_){ case LoRaRegion::EU868:return "eu868"; case LoRaRegion::AU915:return "au915"; default:return "us915"; } }
  const char* presetName() const { switch(preset_){ case LoRaPreset::LongRange:return "long"; case LoRaPreset::Fast:return "fast"; default:return "balanced"; } }
  float frequency() const { return frequencyFor(region_); }

  bool setRegion(LoRaRegion r){ region_=r; prefs.putUChar("region",(uint8_t)region_); return reconfigure(); }
  bool setPreset(LoRaPreset p){ preset_=p; prefs.putUChar("preset",(uint8_t)preset_); return reconfigure(); }

private:
  StorageManager* storage=nullptr;
  Preferences prefs;
  GpsProvider* gps=nullptr;
  Availability state_=Availability::Unknown;
  String lastError_;

  Module module{5,4,3,6};
  SX1262 radio{&module};
  m5::PI4IOE5V6408_Class ioe{0x43,400000,&m5::In_I2C};
  static volatile bool rxFlag_;

  uint32_t nodeId_=0,seq_=0;
  char nodeName_[18]{};
  LoRaRegion region_=LoRaRegion::US915;
  LoRaPreset preset_=LoRaPreset::Balanced;
  int8_t txPower_=17;
  uint8_t hopLimit_=2;
  bool positionBeacon_=true,relayEnabled_=true;
  int16_t lastRadioCode_=0,lastRssi_=-127;
  float lastSnr_=0;
  uint32_t txCount_=0,rxCount_=0,relayCount_=0,droppedCount_=0;
  uint32_t messageSerial_=0,nodeSerial_=0;
  uint32_t lastNodeBeacon_=0,lastPositionBeacon_=0;

  LoRaMessage messages_[MAX_MESSAGES]{}; uint8_t msgHead_=0,msgCount_=0;
  LoRaNode nodes_[MAX_NODES]{}; uint8_t nodeCount_=0;
  uint64_t seen_[32]{}; uint8_t seenHead_=0;
  uint32_t rangeSent_=0,rangeReplies_=0,lastRangeSeq_=0,lastRangeAt_=0;
  int16_t rangeLastRssi_=-127; float rangeLastSnr_=0;

  void loadSettings(){
    uint8_t r=prefs.getUChar("region",(uint8_t)LoRaRegion::US915);
    uint8_t p=prefs.getUChar("preset",(uint8_t)LoRaPreset::Balanced);
    if(r>2) r=(uint8_t)LoRaRegion::US915;
    if(p>2) p=(uint8_t)LoRaPreset::Balanced;
    region_=(LoRaRegion)r; preset_=(LoRaPreset)p;
    txPower_=constrain((int)prefs.getChar("txp",17),2,22);
    hopLimit_=constrain((int)prefs.getUChar("hops",2),0,5);
    relayEnabled_=prefs.getBool("relay",true);
    positionBeacon_=prefs.getBool("pos",true);
  }

  static void IRAM_ATTR rxThunk();

  static float frequencyFor(LoRaRegion r){
    switch(r){ case LoRaRegion::EU868:return 868.0f; case LoRaRegion::AU915:return 915.2f; default:return 915.0f; }
  }

  void presetParams(float& bw,uint8_t& sf,uint8_t& cr,uint16_t& pre) const {
    if(preset_==LoRaPreset::LongRange){ bw=125.0f; sf=11; cr=5; pre=16; }
    else if(preset_==LoRaPreset::Fast){ bw=250.0f; sf=7; cr=5; pre=8; }
    else { bw=125.0f; sf=9; cr=5; pre=12; }
  }

  int16_t applyRadioConfig(bool first){
    float bw; uint8_t sf,cr; uint16_t pre; presetParams(bw,sf,cr,pre);
    if(first){
      int16_t rc=radio.begin(frequencyFor(region_),bw,sf,cr,0x34,txPower_,pre,3.0,true);
      if(rc!=RADIOLIB_ERR_NONE) return rc;
      radio.setCurrentLimit(140);
      return RADIOLIB_ERR_NONE;
    }
    int16_t rc=radio.standby(); if(rc!=RADIOLIB_ERR_NONE) return rc;
    if((rc=radio.setFrequency(frequencyFor(region_)))!=RADIOLIB_ERR_NONE) return rc;
    if((rc=radio.setBandwidth(bw))!=RADIOLIB_ERR_NONE) return rc;
    if((rc=radio.setSpreadingFactor(sf))!=RADIOLIB_ERR_NONE) return rc;
    if((rc=radio.setCodingRate(cr))!=RADIOLIB_ERR_NONE) return rc;
    if((rc=radio.setOutputPower(txPower_))!=RADIOLIB_ERR_NONE) return rc;
    if((rc=radio.setPreambleLength(pre))!=RADIOLIB_ERR_NONE) return rc;
    return radio.startReceive();
  }

  bool reconfigure(){
    if(!ready()) return false;
    int16_t rc=applyRadioConfig(false); lastRadioCode_=rc;
    if(rc!=RADIOLIB_ERR_NONE){ lastError_=String("radio cfg ")+String(rc); state_=Availability::Error; return false; }
    return true;
  }

  String sanitize(String s) const {
    s.trim();
    s.replace("|","/"); s.replace("\n"," "); s.replace("\r"," ");
    if(s.length()>104) s=s.substring(0,104);
    return s;
  }

  static String hexId(uint32_t v){ char b[10]; snprintf(b,sizeof(b),"%08lx",(unsigned long)v); return String(b); }
  String buildPacket(char type,uint32_t seq,uint8_t ttl,const String& payload) const {
    return String("D1|")+hexId(nodeId_)+"|"+String(seq)+"|"+String(ttl)+"|"+String(type)+"|"+payload;
  }

  bool sendRaw(const String& packet){
    if(!ready()) return false;
    state_=Availability::Busy;
    digitalWrite(12,HIGH);
    int16_t rc=radio.transmit((uint8_t*)packet.c_str(),packet.length());
    lastRadioCode_=rc;
    if(rc==RADIOLIB_ERR_NONE) txCount_++; else lastError_=String("TX ")+String(rc);
    radio.startReceive();
    state_=Availability::Ready;
    return rc==RADIOLIB_ERR_NONE;
  }

  bool parseHeader(const String& p,uint32_t& src,uint32_t& seq,uint8_t& ttl,char& type,String& payload){
    if(!p.startsWith("D1|")) return false;
    int a=p.indexOf('|',3); if(a<0)return false;
    int b=p.indexOf('|',a+1); if(b<0)return false;
    int c=p.indexOf('|',b+1); if(c<0)return false;
    int d=p.indexOf('|',c+1); if(d<0)return false;
    String ssrc=p.substring(3,a); src=(uint32_t)strtoul(ssrc.c_str(),nullptr,16);
    seq=(uint32_t)p.substring(a+1,b).toInt(); ttl=(uint8_t)p.substring(b+1,c).toInt();
    String ts=p.substring(c+1,d); if(!ts.length()) return false; type=ts[0]; payload=p.substring(d+1); return true;
  }

  uint64_t keyFor(uint32_t src,uint32_t seq) const { return ((uint64_t)src<<32)|seq; }
  bool seenBefore(uint32_t src,uint32_t seq) const { uint64_t k=keyFor(src,seq); for(uint8_t i=0;i<32;i++)if(seen_[i]==k)return true; return false; }
  void rememberPacket(uint32_t src,uint32_t seq){ seen_[seenHead_++%32]=keyFor(src,seq); }

  LoRaNode* upsertNode(uint32_t id,int16_t rssi,float snr){
    for(uint8_t i=0;i<nodeCount_;i++) if(nodes_[i].id==id){
      bool changed=(nodes_[i].rssi!=rssi) || (fabsf(nodes_[i].snr-snr)>0.1f);
      nodes_[i].rssi=rssi; nodes_[i].snr=snr; nodes_[i].lastSeen=millis();
      if(changed) nodeSerial_++;
      return &nodes_[i];
    }
    uint8_t idx=nodeCount_<MAX_NODES?nodeCount_++:(uint8_t)(id%MAX_NODES);
    nodes_[idx]={}; nodes_[idx].id=id; snprintf(nodes_[idx].name,sizeof(nodes_[idx].name),"node-%04lx",(unsigned long)(id&0xFFFFu));
    nodes_[idx].rssi=rssi; nodes_[idx].snr=snr; nodes_[idx].lastSeen=millis(); nodeSerial_++; return &nodes_[idx];
  }

  void pushMessage(uint32_t from,uint32_t to,uint32_t seq,const String& text,bool out,int16_t rssi,float snr){
    LoRaMessage& m=messages_[msgHead_]; m={}; m.from=from; m.to=to; m.seq=seq; m.at=millis(); m.outgoing=out; m.rssi=rssi; m.snr=snr;
    strlcpy(m.text,text.c_str(),sizeof(m.text)); msgHead_=(uint8_t)((msgHead_+1)%MAX_MESSAGES); if(msgCount_<MAX_MESSAGES)msgCount_++;
    messageSerial_++;
  }

  void handlePacket(const String& p,int16_t rssi,float snr){
    uint32_t src,seq; uint8_t ttl; char type; String payload;
    if(!parseHeader(p,src,seq,ttl,type,payload)){ droppedCount_++; return; }
    if(src==nodeId_ || seenBefore(src,seq)) return;
    rememberPacket(src,seq); rxCount_++;
    LoRaNode* n=upsertNode(src,rssi,snr);

    if(type=='C'){
      pushMessage(src,0,seq,payload,false,rssi,snr);
      logChat("rx",src,payload,rssi,snr);
    } else if(type=='D'){
      int comma=payload.indexOf(',');
      if(comma>0){
        uint32_t dest=(uint32_t)strtoul(payload.substring(0,comma).c_str(),nullptr,16);
        String body=sanitize(payload.substring(comma+1));
        if(dest==nodeId_){ pushMessage(src,dest,seq,body,false,rssi,snr); logChat("rx-dm",src,body,rssi,snr); }
      }
    } else if(type=='N'){
      String nm=sanitize(payload); if(nm.length() && n){ if(String(n->name)!=nm) nodeSerial_++; strlcpy(n->name,nm.c_str(),sizeof(n->name)); }
    } else if(type=='P'){
      int c1=payload.indexOf(','), c2=payload.indexOf(',',c1+1);
      if(c1>0 && c2>c1 && n){ double la=payload.substring(0,c1).toDouble(), lo=payload.substring(c1+1,c2).toDouble(); if(!n->hasPosition || fabs(n->lat-la)>0.000001 || fabs(n->lon-lo)>0.000001) nodeSerial_++; n->lat=la; n->lon=lo; n->hasPosition=true; }
    } else if(type=='R'){
      // Range pings are answered once with a Q packet; no relaying.
      String reply=String(src,HEX)+","+String(seq);
      uint32_t q=++seq_; sendRaw(buildPacket('Q',q,0,reply)); rememberPacket(nodeId_,q);
    } else if(type=='Q'){
      int comma=payload.indexOf(','); if(comma>0){ uint32_t dest=(uint32_t)strtoul(payload.substring(0,comma).c_str(),nullptr,16); uint32_t orig=(uint32_t)payload.substring(comma+1).toInt(); if(dest==nodeId_ && orig==lastRangeSeq_){ rangeReplies_++; rangeLastRssi_=rssi; rangeLastSnr_=snr; } }
    }

    if(relayEnabled_ && ttl>0 && type!='R' && type!='Q'){
      // Preserve origin/sequence while decrementing TTL. Dedupe prevents loops.
      String rel=String("D1|")+hexId(src)+"|"+String(seq)+"|"+String((uint8_t)(ttl-1))+"|"+String(type)+"|"+payload;
      delay(8+(esp_random()%24));
      if(sendRaw(rel)) relayCount_++;
    }
  }

  void logLine(const String& line){ if(storage && storage->writable()) storage->append("/dirk/lora/events.log",line); }
  void logChat(const char* dir,uint32_t id,const String& text,int16_t rssi,float snr){
    if(!storage || !storage->writable()) return;
    String line=String(millis())+","+dir+","+hexId(id)+","+String(rssi)+","+String(snr,1)+",\""+sanitize(text)+"\"\n";
    storage->append("/dirk/lora/chat.csv",line);
  }
};


}
