#pragma once
#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <dirk/types.h>

namespace dirk {
class GpsProvider {
public:
  bool begin(uint32_t baud=115200u){
    // Official ADV LoRa/GPS Cap: receiver TX -> Cardputer GPIO15,
    // receiver RX <- Cardputer GPIO13; ATGM336H default 115200 8N1.
    // Keep the GNSS UART separate from MonsterC5 on Serial1 GPIO1/2.
    pinMode(5,OUTPUT); digitalWrite(5,HIGH); // LoRa SPI chip select idle
    Serial2.setRxBufferSize(2048); // absorb NMEA bursts during Wi-Fi/SD work
    Serial2.begin(baud,SERIAL_8N1,15,13);
    baud_=baud;
    started=true; return true;
  }
  uint32_t baud() const { return baud_; }
  void setBaud(uint32_t baud){
    if(baud!=115200u && baud!=9600u) return;
    if(started) Serial2.end();
    // Reopening the UART is diagnostic, not proof of a satellite fix.
    // Clear stale NMEA position/counts from the previous baud rate.
    gps=TinyGPSPlus();
    rxBytes_=nmeaLines_=lastRxAt_=lastNmeaAt_=0;
    capturePos_=0; capturing_=false;
    lastLine_[0]=0; currentLine_[0]=0;
    begin(baud);
  }
  void update(){
    if(!started) return;
    // Nonblocking bounded drain: do not let UART bursts starve tank animation.
    const uint32_t begun=micros(); unsigned handled=0;
    while(Serial2.available() && handled<1024 && (uint32_t)(micros()-begun)<2300u){
      const char c=(char)Serial2.read(); ++handled;
      ++rxBytes_; lastRxAt_=millis();
      gps.encode(c);
      // Capture one NMEA line for diagnostics, not an unbounded String.
      if(c=='$'){ capturePos_=0; capturing_=true; }
      if(!capturing_) continue;
      if(c=='\n'){
        currentLine_[capturePos_]=0;
        memcpy(lastLine_,currentLine_,capturePos_+1);
        ++nmeaLines_; lastNmeaAt_=millis();
        capturePos_=0; capturing_=false;
      } else if(c!='\r' && (uint8_t)c>=32u && (uint8_t)c<127u){
        if(capturePos_<sizeof(currentLine_)-1) currentLine_[capturePos_++]=c;
      }
    }
  }
  // A successful checksum and recently received bytes indicate a live GNSS
  // receiver. Neither of these implies a satellite position fix.
  bool present() const { return started && gps.passedChecksum()>0 && lastRxAgeMs()<8000u; }
  bool everReceived() const { return rxBytes_!=0; }
  uint32_t rxBytes() const { return rxBytes_; }
  uint32_t nmeaLines() const { return nmeaLines_; }
  uint32_t checksumOk() const { return gps.passedChecksum(); }
  uint32_t checksumBad() const { return gps.failedChecksum(); }
  uint32_t lastRxAgeMs() const { return rxBytes_?millis()-lastRxAt_:0xFFFFFFFFu; }
  uint32_t lastNmeaAgeMs() const { return nmeaLines_?millis()-lastNmeaAt_:0xFFFFFFFFu; }
  uint32_t locationAgeMs() const { return gps.location.isValid()?gps.location.age():0xFFFFFFFFu; }
  uint32_t satellitesAgeMs() const { return gps.satellites.isValid()?gps.satellites.age():0xFFFFFFFFu; }
  uint32_t hdopAgeMs() const { return gps.hdop.isValid()?gps.hdop.age():0xFFFFFFFFu; }
  const char* lastNmea() const { return lastLine_; }
  bool fixed() const { return gps.location.isValid() && gps.location.age()<5000u; }
  GpsFix fix() {
    GpsFix f{};
    f.valid=fixed();
    if(f.valid){ f.lat=gps.location.lat(); f.lon=gps.location.lng(); }
    if(gps.altitude.isValid() && gps.altitude.age()<10000u) f.alt=(float)gps.altitude.meters();
    if(gps.hdop.isValid() && gps.hdop.age()<10000u) f.hdop=(float)gps.hdop.hdop();
    if(gps.speed.isValid() && gps.speed.age()<10000u) f.speedKph=(float)gps.speed.kmph();
    if(gps.course.isValid() && gps.course.age()<10000u) f.courseDeg=(float)gps.course.deg();
    if(gps.satellites.isValid() && gps.satellites.age()<10000u) f.sats=(uint8_t)gps.satellites.value();
    f.at=millis(); return f;
  }
  bool utcEpoch(time_t& out) {
    if(!gps.date.isValid() || !gps.time.isValid()) return false;
    if(gps.date.age()>5000 || gps.time.age()>5000) return false;
    int y=gps.date.year(); unsigned m=gps.date.month(),d=gps.date.day();
    if(y<2024 || m<1 || m>12 || d<1 || d>31) return false;
    int yy=y-(m<=2);
    int era=(yy>=0?yy:yy-399)/400;
    unsigned yoe=(unsigned)(yy-era*400);
    unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
    unsigned doe=yoe*365+yoe/4-yoe/100+doy;
    int64_t days=(int64_t)era*146097+(int64_t)doe-719468;
    out=(time_t)(days*86400LL + (int64_t)gps.time.hour()*3600LL + (int64_t)gps.time.minute()*60LL + gps.time.second());
    return true;
  }
private:
  TinyGPSPlus gps;
  bool started=false;
  uint32_t baud_=115200u;
  uint32_t rxBytes_=0,nmeaLines_=0,lastRxAt_=0,lastNmeaAt_=0;
  char currentLine_[96]{};
  char lastLine_[96]{};
  size_t capturePos_=0;
  bool capturing_=false;
};
}
