#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

namespace dirk {
class StorageManager {
public:
  static constexpr int SD_SCK=40;
  static constexpr int SD_MISO=39;
  static constexpr int SD_MOSI=14;
  static constexpr int SD_CS=12;
  static constexpr int LORA_CS=5;

  bool begin(){
    mounted_=writable_=false;
    lastError_="";
    pinMode(LORA_CS,OUTPUT);
    digitalWrite(LORA_CS,HIGH); // Cap LoRa shares SPI; keep it deselected.
    pinMode(SD_CS,OUTPUT);
    digitalWrite(SD_CS,HIGH);

    SPI.begin(SD_SCK,SD_MISO,SD_MOSI,SD_CS);
    if(!SD.begin(SD_CS,SPI,25000000)){
      lastError_="SD.begin failed";
      return false;
    }
    if(SD.cardType()==CARD_NONE){
      lastError_="no SD card";
      SD.end();
      return false;
    }
    mounted_=true;
    make("/dirk"); make("/dirk/targets"); make("/dirk/sessions");
    make("/dirk/captures"); make("/dirk/wardrives"); make("/dirk/reports");
    make("/dirk/wardrive"); make("/dirk/wardrive/sessions"); make("/dirk/wardrive/config");
    make("/dirk/lora"); make("/dirk/lora/config"); make("/dirk/lora/history");
    make("/dirk/blue"); make("/dirk/ir"); make("/dirk/portal");
    writable_=selfTest();
    if(!writable_) lastError_="write/read self-test failed";
    return mounted_ && writable_;
  }

  bool mounted() const { return mounted_; }
  bool writable() const { return mounted_ && writable_; }
  const String& lastError() const { return lastError_; }

  bool selfTest(){
    if(!mounted_) return false;
    const char* path="/dirk/.sdtest";
    if(SD.exists(path)) SD.remove(path);
    File f=SD.open(path,FILE_WRITE);
    if(!f) return false;
    const char token[]="DIRK-SD-OK";
    size_t n=f.write((const uint8_t*)token,sizeof(token)-1);
    f.flush(); f.close();
    if(n!=sizeof(token)-1) return false;
    f=SD.open(path,FILE_READ);
    if(!f) return false;
    char buf[16]{};
    size_t r=f.readBytes(buf,sizeof(token)-1);
    f.close();
    bool ok=r==sizeof(token)-1 && memcmp(buf,token,sizeof(token)-1)==0;
    SD.remove(path);
    return ok;
  }

  bool append(const char* path,const String& s){
    if(!writable()) return false;
    digitalWrite(LORA_CS,HIGH);
    File f=SD.open(path,FILE_APPEND); if(!f) return false;
    bool ok=f.print(s)>0; f.flush(); f.close(); return ok;
  }

  bool write(const char* path,const uint8_t* data,size_t n){
    if(!writable()) return false;
    digitalWrite(LORA_CS,HIGH);
    File f=SD.open(path,FILE_WRITE); if(!f) return false;
    size_t w=f.write(data,n); f.flush(); f.close(); return w==n;
  }

  bool overwrite(const char* path,const uint8_t* data,size_t n){
    if(!writable()) return false;
    digitalWrite(LORA_CS,HIGH);
    if(SD.exists(path)) SD.remove(path);
    File f=SD.open(path,FILE_WRITE); if(!f) return false;
    size_t w=f.write(data,n); f.flush(); f.close();
    if(w!=n) return false;
    File verify=SD.open(path,FILE_READ);
    if(!verify) return false;
    bool ok=(size_t)verify.size()==n;
    verify.close();
    return ok;
  }

  bool exists(const char* p) const { return mounted_ && SD.exists(p); }

private:
  bool mounted_=false,writable_=false;
  String lastError_;
  void make(const char* p){ if(!SD.exists(p)) SD.mkdir(p); }
};
}
