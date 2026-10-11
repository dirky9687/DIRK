#pragma once
#include <Arduino.h>
#include <SD.h>
#include <driver/rmt.h>
#include "../../storage/StorageManager.h"

namespace dirk {

class IrProvider {
public:
  static constexpr gpio_num_t TX_PIN=(gpio_num_t)44;
  bool begin(StorageManager* s=nullptr){ storage=s; ready_=configure(38000,33); return ready_; }
  bool ready() const { return ready_; }
  uint32_t carrierHz() const { return carrierHz_; }
  uint8_t dutyPercent() const { return duty_; }
  const String& lastStatus() const { return lastStatus_; }
  uint32_t sentCount() const { return sentCount_; }

  bool sendNec(uint16_t address,uint8_t command){
    if(!ready_ && !configure(38000,33)) return false;
    rmt_item32_t items[34]{}; size_t n=0;
    setItem(items[n++],9000,4500);
    uint32_t frame;
    if(address<=0xFFu){ uint8_t a=(uint8_t)address; frame=(uint32_t)a | ((uint32_t)(uint8_t)~a<<8); }
    else frame=(uint32_t)address;
    frame |= ((uint32_t)command<<16) | ((uint32_t)(uint8_t)~command<<24);
    for(uint8_t i=0;i<32;i++){
      bool one=(frame>>i)&1u; setItem(items[n++],560,one?1690:560);
    }
    setItem(items[n++],560,1);
    esp_err_t e=rmt_write_items(CH,items,n,true);
    if(e==ESP_OK){ sentCount_++; lastStatus_="NEC sent"; return true; }
    lastStatus_="RMT send failed"; return false;
  }

  bool sendRaw(uint32_t hz,uint8_t duty,const uint16_t* timings,size_t count){
    if(!timings || count<2) return false;
    if(hz<20000 || hz>60000) hz=38000; if(duty<10 || duty>50) duty=33;
    if(!configure(hz,duty)) return false;
    static rmt_item32_t items[256]; size_t n=0;
    for(size_t i=0;i<count && n<256;i+=2){ uint16_t mark=timings[i]; uint16_t space=(i+1<count)?timings[i+1]:1; if(!mark) mark=1; if(!space) space=1; setItem(items[n++],mark,space); }
    esp_err_t e=rmt_write_items(CH,items,n,true);
    if(e==ESP_OK){ sentCount_++; lastStatus_="raw IR sent"; return true; }
    lastStatus_="raw send failed"; return false;
  }

  bool sendFlipperFirst(const char* path,String* signalName=nullptr){
    if(!storage || !storage->mounted() || !path || !SD.exists(path)){ lastStatus_="IR file unavailable"; return false; }
    File f=SD.open(path,FILE_READ); if(!f){ lastStatus_="IR file open failed"; return false; }
    String name,type,protocol,address,command,raw; uint32_t freq=38000; uint8_t duty=33;
    auto tryBlock=[&]()->bool{
      if(type=="parsed" && (protocol=="NEC" || protocol=="NECext")){
        uint32_t a=parseHexBytes(address), c=parseHexBytes(command); bool ok=sendNec((uint16_t)(a&0xFFFFu),(uint8_t)(c&0xFFu)); if(ok && signalName) *signalName=name; return ok;
      }
      if(type=="raw" && raw.length()){
        uint16_t timings[512]{}; size_t cnt=parseTimings(raw,timings,512); bool ok=sendRaw(freq,duty,timings,cnt); if(ok && signalName) *signalName=name; return ok;
      }
      return false;
    };
    while(f.available()){
      String line=f.readStringUntil('\n'); line.trim();
      if(!line.length()) continue;
      if(line=="#"){
        if((type.length() && tryBlock())){ f.close(); return true; }
        name="";type="";protocol="";address="";command="";raw="";freq=38000;duty=33; continue;
      }
      int p=line.indexOf(':'); if(p<0) continue; String key=line.substring(0,p); String val=line.substring(p+1); key.trim(); val.trim();
      if(key=="name") name=val; else if(key=="type") type=val; else if(key=="protocol") protocol=val; else if(key=="address") address=val; else if(key=="command") command=val;
      else if(key=="frequency") freq=(uint32_t)val.toInt(); else if(key=="duty_cycle"){ float d=val.toFloat(); duty=(uint8_t)constrain((int)(d*100.0f+0.5f),10,50); } else if(key=="data") raw=val;
    }
    bool ok=type.length() && tryBlock(); f.close(); if(!ok) lastStatus_="no supported NEC/raw signal"; return ok;
  }

private:
  static constexpr rmt_channel_t CH=RMT_CHANNEL_0;
  StorageManager* storage=nullptr; bool ready_=false,driverInstalled_=false; uint32_t carrierHz_=38000,sentCount_=0; uint8_t duty_=33; String lastStatus_="idle";

  bool configure(uint32_t hz,uint8_t duty){
    if(driverInstalled_ && hz==carrierHz_ && duty==duty_) return true;
    if(driverInstalled_){ rmt_driver_uninstall(CH); driverInstalled_=false; }
    rmt_config_t c{}; c.rmt_mode=RMT_MODE_TX; c.channel=CH; c.gpio_num=TX_PIN; c.mem_block_num=1; c.clk_div=80;
    c.tx_config.loop_en=false; c.tx_config.carrier_en=true; c.tx_config.carrier_freq_hz=hz; c.tx_config.carrier_duty_percent=duty;
    c.tx_config.carrier_level=RMT_CARRIER_LEVEL_HIGH; c.tx_config.idle_level=RMT_IDLE_LEVEL_LOW; c.tx_config.idle_output_en=true;
    if(rmt_config(&c)!=ESP_OK){ lastStatus_="RMT config failed"; return false; }
    if(rmt_driver_install(CH,0,0)!=ESP_OK){ lastStatus_="RMT driver failed"; return false; }
    driverInstalled_=true; carrierHz_=hz; duty_=duty; lastStatus_="IR TX ready"; return true;
  }
  static void setItem(rmt_item32_t& i,uint16_t mark,uint16_t space){ i.level0=1; i.duration0=mark; i.level1=0; i.duration1=space?space:1; }
  static uint32_t parseHexBytes(String s){
    uint32_t out=0; uint8_t shift=0; s.replace("0x",""); s.replace("0X","");
    int start=0; while(start<s.length() && shift<32){ while(start<s.length() && s[start]==' ') start++; if(start>=s.length()) break; int end=s.indexOf(' ',start); if(end<0) end=s.length(); String tok=s.substring(start,end); uint8_t b=(uint8_t)strtoul(tok.c_str(),nullptr,16); out|=((uint32_t)b<<shift); shift+=8; start=end+1; }
    return out;
  }
  static size_t parseTimings(const String& s,uint16_t* out,size_t max){ size_t n=0; int pos=0; while(pos<s.length() && n<max){ while(pos<s.length() && s[pos]==' ') pos++; if(pos>=s.length()) break; int e=s.indexOf(' ',pos); if(e<0)e=s.length(); long v=s.substring(pos,e).toInt(); if(v>0) out[n++]=(uint16_t)min(v,32767L); pos=e+1; } return n; }
};

}
