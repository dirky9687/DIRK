#pragma once
#include <Arduino.h>
#include <string>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <dirk/types.h>

namespace dirk {
struct BleTarget {
  char address[18]{}; char name[28]{}; int rssi=-127; uint32_t lastSeen=0; bool named=false;
  bool meshtastic=false; bool meshcore=false; bool ibeacon=false; bool eddystone=false;
  bool hasTxPower=false; int8_t txPower=0; bool hasAppearance=false; uint16_t appearance=0;
  uint16_t manufacturerId=0; uint8_t manufacturerDataBytes=0,serviceDataBytes=0; char service[40]{};
};
class BleProvider {
public:
  static constexpr size_t MAX=40;
  void begin(){ BLEDevice::init(""); scanner=BLEDevice::getScan(); scanner->setActiveScan(false); scanner->setInterval(100); scanner->setWindow(80); state=Availability::Ready; instance=this; }
  Availability availability() const { return state; }
  size_t scan(uint32_t seconds=3){
    count_=0; if(!scanner) return 0; state=Availability::Busy;
    BLEScanResults found=scanner->start(seconds,false);
    consume(found); state=Availability::Ready; return count_;
  }

  // Async BLE scan used by Wardrive so a one-second BLE dwell no longer freezes
  // the buddy animation or GPS/C5 servicing.
  bool startScanAsync(uint32_t seconds=1){
    if(!scanner || state==Availability::Busy) return false;
    count_=0; state=Availability::Busy;
    bool ok=scanner->start(seconds,&BleProvider::scanCompleteThunk,false);
    if(!ok) state=Availability::Error;
    return ok;
  }
  // Stopping Wardrive must not leave a one-second BLE dwell running while
  // its owner is no longer active. The regular BLE screens may scan again.
  void cancelWardriveScan(){
    if(!scanner || state!=Availability::Busy) return;
    scanner->stop();
    scanner->clearResults();
    state=Availability::Ready;
  }
  bool scanning() const { return state==Availability::Busy; }
  size_t count() const { return count_; }
  uint32_t resultSerial() const { return resultSerial_; }
  const BleTarget* at(size_t i) const { return i<count_?&items[i]:nullptr; }
private:
  BLEScan* scanner=nullptr; Availability state=Availability::Unknown; BleTarget items[MAX]{}; size_t count_=0; uint32_t resultSerial_=0;
  static BleProvider* instance;

  void consume(BLEScanResults& found){
    int n=found.getCount();
    for(int i=0;i<n && count_<MAX;i++){
      items[count_]={};
      BLEAdvertisedDevice d=found.getDevice(i); String a=d.getAddress().toString().c_str(); strlcpy(items[count_].address,a.c_str(),sizeof(items[count_].address));
      items[count_].rssi=d.getRSSI(); items[count_].lastSeen=millis(); if(d.haveName()){ String nm=d.getName().c_str(); strlcpy(items[count_].name,nm.c_str(),sizeof(items[count_].name)); items[count_].named=true; } else strlcpy(items[count_].name,"<unnamed>",sizeof(items[count_].name));
      items[count_].meshtastic=d.isAdvertisingService(BLEUUID("6ba1b218-15a8-461f-9fa8-5dcae273eafd"));
      items[count_].meshcore=d.isAdvertisingService(BLEUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e"));
      items[count_].eddystone=d.isAdvertisingService(BLEUUID((uint16_t)0xFEAA));
      if(d.haveTXPower()){ items[count_].hasTxPower=true; items[count_].txPower=(int8_t)d.getTXPower(); }
      if(d.haveAppearance()){ items[count_].hasAppearance=true; items[count_].appearance=d.getAppearance(); }
      if(d.haveServiceUUID()){ String su=d.getServiceUUID().toString().c_str(); strlcpy(items[count_].service,su.c_str(),sizeof(items[count_].service)); }
      if(d.haveManufacturerData()){
        std::string md=d.getManufacturerData();
        items[count_].manufacturerDataBytes=(uint8_t)(md.size()>255?255:md.size());
        if(md.size()>=2) items[count_].manufacturerId=(uint16_t)(uint8_t)md[0] | ((uint16_t)(uint8_t)md[1]<<8);
        if(md.size()>=4 && (uint8_t)md[0]==0x4C && (uint8_t)md[1]==0x00 && (uint8_t)md[2]==0x02 && (uint8_t)md[3]==0x15) items[count_].ibeacon=true;
      }
      if(d.haveServiceData()){ std::string sd=d.getServiceData(); items[count_].serviceDataBytes=(uint8_t)(sd.size()>255?255:sd.size()); }
      String low=String(items[count_].name); low.toLowerCase();
      if(!items[count_].meshtastic && low.indexOf("meshtastic")>=0) items[count_].meshtastic=true;
      if(!items[count_].meshcore && low.indexOf("meshcore")>=0) items[count_].meshcore=true;
      count_++;
    }
    if(scanner) scanner->clearResults();
    resultSerial_++;
  }
  static void scanCompleteThunk(BLEScanResults found){
    if(!instance) return;
    instance->consume(found);
    instance->state=Availability::Ready;
  }
};
inline BleProvider* BleProvider::instance=nullptr;
}
