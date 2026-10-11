#pragma once
#include <Arduino.h>
namespace dirk {
class C5Transport {
public:
  bool begin(HardwareSerial& port=Serial1,int rx=1,int tx=2,uint32_t baud=115200){
    serial=&port; rxPin_=rx; txPin_=tx; baud_=baud;
    serial->begin(baud,SERIAL_8N1,rx,tx);
    lastRx=0; everRx=false; pos=0; head=tail=count=0; droppedLines_=overlongLines_=0; discarding_=false;
    rxBytes_=txBytes_=rxLines_=txLines_=0; lastSent_=""; lastLine_="";
    return true;
  }
  void update(){
    if(!serial)return;
    // Bound UART handling per loop; leave cycles for the screen and GPS.
    const uint32_t started=micros(); unsigned handled=0;
    while(serial->available() && handled<768 && (uint32_t)(micros()-started)<1600u){
      char c=(char)serial->read(); ++handled; lastRx=millis(); everRx=true; rxBytes_++;
      if(c=='\n'||c=='\r'){
        if(!discarding_ && pos){ cur[pos]=0; push(cur); lastLine_=cur; rxLines_++; }
        pos=0; discarding_=false;
      } else if(!discarding_){
        if(pos<sizeof(cur)-1) cur[pos++]=c;
        else { ++overlongLines_; pos=0; discarding_=true; }
      }
    }
  }
  bool detected()const{return serial&&everRx&&(millis()-lastRx)<12000;}
  bool everDetected()const{return serial&&everRx;}
  bool hasLine()const{return count>0;}
  bool readLine(String& out){ if(!count)return false; out=lines[tail]; tail=(tail+1)%Q; count--; return true; }
  void sendLine(const char* s){
    if(serial&&s){ serial->print(s); serial->print("\r\n"); txBytes_+=strlen(s)+2; txLines_++; lastSent_=s; }
  }
  uint32_t droppedLines() const { return droppedLines_; }
  uint32_t overlongLines() const { return overlongLines_; }
  uint32_t rxBytes() const { return rxBytes_; }
  uint32_t txBytes() const { return txBytes_; }
  uint32_t rxLines() const { return rxLines_; }
  uint32_t txLines() const { return txLines_; }
  const String& lastSent() const { return lastSent_; }
  const String& lastLine() const { return lastLine_; }
  uint32_t baud() const { return baud_; }
  int rxPin() const { return rxPin_; }
  int txPin() const { return txPin_; }
private:
  static constexpr uint8_t Q=32;
  HardwareSerial* serial=nullptr; char cur[768]{}; size_t pos=0; bool discarding_=false; String lines[Q];
  uint8_t head=0,tail=0,count=0; bool everRx=false; uint32_t lastRx=0; uint32_t baud_=115200;
  int rxPin_=1,txPin_=2;
  uint32_t rxBytes_=0,txBytes_=0,rxLines_=0,txLines_=0,droppedLines_=0,overlongLines_=0; String lastSent_,lastLine_;
  void push(const char* s){ if(count==Q){tail=(tail+1)%Q;count--;droppedLines_++;} lines[head]=s; head=(head+1)%Q; count++; }
};
}
