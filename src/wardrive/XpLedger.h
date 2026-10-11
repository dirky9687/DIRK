#pragma once
#include <Arduino.h>
#include <SD.h>
#include <string.h>

namespace dirk {
// Exact, persistent contact identity: one byte for radio (1=Wi-Fi,
// 2=BLE) plus the six MAC/BSSID bytes. Avoid SSID-based rewards: different
// APs can broadcast the same SSID, and SSIDs may be hidden or renamed.
// 64 short SD buckets keep reads bounded without a large PSRAM hash table
// (the Cardputer ADV's ESP32-S3FN8 has no PSRAM).
class XpLedger {
public:
  enum class ClaimResult : uint8_t { Repeat, New, Unavailable };
  bool begin(){
    ready_=SD.exists(dir()) || SD.mkdir(dir());
    pendingCount_=0;
    return ready_;
  }
  bool ready() const { return ready_; }
  ClaimResult claim(uint8_t radio,const uint8_t mac[6]){
    if(!ready_ || !mac || (radio!=1 && radio!=2)) return ClaimResult::Unavailable;
    bool empty=true;
    for(int i=0;i<6;i++) if(mac[i]){empty=false;break;}
    if(empty || (radio==1 && (mac[0]&1u))) return ClaimResult::Unavailable;
    uint8_t key[7]{}; key[0]=radio; memcpy(key+1,mac,6);
    for(size_t i=0;i<pendingCount_;i++) if(!memcmp(pending_[i],key,7)) return ClaimResult::Repeat;
    char p[75]; bucketPath(key,p,sizeof(p));
    if(SD.exists(p)){
      File f=SD.open(p,FILE_READ);
      if(!f) return ClaimResult::Unavailable; // fail closed: never award twice on SD errors
      uint8_t current[7];
      while(f.available()>=7){
        if(f.read(current,7)!=7){f.close();return ClaimResult::Unavailable;}
        if(!memcmp(current,key,7)){f.close();return ClaimResult::Repeat;}
      }
      // Partial/corrupt bucket? Do not issue additional XP until recovered.
      if(f.available()!=0){f.close();return ClaimResult::Unavailable;}
      f.close();
    }
    // If writes are lagging, refuse the award instead of risking duplicates.
    if(pendingCount_>=MAX_PENDING) return ClaimResult::Unavailable;
    memcpy(pending_[pendingCount_++],key,7);
    return ClaimResult::New;
  }
  bool flushOne(){
    if(!ready_ || !pendingCount_) return false;
    char p[75]; bucketPath(pending_[0],p,sizeof(p));
    File f=SD.open(p,FILE_APPEND);
    if(!f) return false;
    bool ok=f.write(pending_[0],7)==7;
    f.close();
    if(ok){
      --pendingCount_;
      if(pendingCount_) memmove(pending_,pending_+1,pendingCount_*7u);
    }
    return ok;
  }
  void flushAll(){while(pendingCount_ && flushOne()){} }
  size_t pending() const {return pendingCount_;}
private:
  static const char* dir(){ return "/dirk/wardrive/config/xp-contacts"; }
  static constexpr size_t MAX_PENDING=96;
  uint8_t pending_[MAX_PENDING][7]{};
  size_t pendingCount_=0;
  bool ready_=false;
  static void bucketPath(const uint8_t key[7],char* out,size_t n){
    uint8_t h=0;
    for(uint8_t i=0;i<7;i++) h=(uint8_t)((h*33u)^key[i]);
    snprintf(out,n,"%s/%02X.bin",dir(),(unsigned)(h&63u));
  }
};
}
