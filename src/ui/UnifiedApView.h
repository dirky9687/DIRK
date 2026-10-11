#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <dirk/types.h>

namespace dirk {
// Display-only, bounded grouping of observed BSSIDs. The underlying target
// store, scans, CSV parser, saved targets and XP accounting remain unchanged.
class UnifiedApView {
public:
  static constexpr size_t MAX_ROWS=256;
  struct Row {
    int16_t primary=-1;
    int16_t band24=-1;
    int16_t band5=-1;
    uint32_t newest=0;
  };
  template <typename TargetSource>
  void build(const TargetSource& source,uint32_t now) {
    count_=0; aps24_=aps5_=0;
    for(size_t i=0;i<source.count() && i<MAX_ROWS;i++) {
      const WifiTarget* t=source.at(i);
      if(!t || !t->lastSeen) continue;
      const uint32_t age=(uint32_t)(now-t->lastSeen);
      if(t->band==Band::Band24) { if(age>15000u) continue; ++aps24_; }
      else if(t->band==Band::Band5) { if(age>60000u) continue; ++aps5_; }
      else continue;
      // Placeholder/hidden SSIDs are not names: they must never merge across
      // unrelated BSSIDs just because their placeholder strings match.
      const bool named=actualName(t->ssid);
      size_t group=count_;
      if(named) {
        for(size_t r=0;r<count_;r++) {
          const WifiTarget* first=source.at((size_t)rows_[r].primary);
          if(first && actualName(first->ssid) && strcmp(first->ssid,t->ssid)==0) {
            group=r;break;
          }
        }
      }
      if(group==count_) {
        if(count_>=MAX_ROWS) break;
        rows_[count_]=Row{};
        rows_[count_].primary=(int16_t)i;
        rows_[count_].newest=t->lastSeen;
        ++count_;
      }
      Row& row=rows_[group];
      int16_t& slot=(t->band==Band::Band5)?row.band5:row.band24;
      const WifiTarget* old=slot<0?nullptr:source.at((size_t)slot);
      if(!old || t->rssi>old->rssi) slot=(int16_t)i;
      if((int32_t)(t->lastSeen-row.newest)>0) row.newest=t->lastSeen;
      // Keep a usable SSID for the label even when earliest observation had
      // no name. BSSID-specific details remain on their existing screen.
      if(row.primary<0) row.primary=(int16_t)i;
    }
    // Stable first-observed ordering avoids jumps while new C5 rows arrive.
  }
  size_t count() const { return count_; }
  size_t count24() const { return aps24_; }
  size_t count5() const { return aps5_; }
  const Row* at(size_t i) const { return i<count_?&rows_[i]:nullptr; }
  static bool actualName(const char* name){
    return name && name[0] && strcmp(name,"<hidden>")!=0 &&
      strcmp(name,"<5G AP>")!=0 && strcmp(name,"<unknown>")!=0;
  }
  template <typename TargetSource>
  const WifiTarget* preferred(const TargetSource& source,size_t row,bool five) const {
    const Row* r=at(row);
    if(!r) return nullptr;
    int16_t idx=five?r->band5:r->band24;
    if(idx<0) idx=five?r->band24:r->band5;
    return idx<0?nullptr:source.at((size_t)idx);
  }
  // Lightweight screen signature, independent of JanOS parser/scan counters.
  template <typename TargetSource>
  uint32_t signature(const TargetSource& source) const {
    uint32_t h=2166136261u;
    auto add=[&](uint32_t value) {h^=value;h*=16777619u;};
    add((uint32_t)count_); add((uint32_t)aps24_); add((uint32_t)aps5_);
    for(size_t i=0;i<count_;i++){
      const Row& r=rows_[i];
      const int16_t indices[3]={r.primary,r.band24,r.band5};
      for(int16_t idx : indices) {
        if(idx<0){ add(0xFFu);continue; }
        const WifiTarget* t=source.at((size_t)idx);
        if(!t)continue;
        add(t->id); add((uint32_t)(uint8_t)t->rssi);
        add((uint32_t)t->auth); add((uint32_t)t->band);
      }
    }
    return h;
  }
private:
  Row rows_[MAX_ROWS]{};
  size_t count_=0,aps24_=0,aps5_=0;
};
} // namespace dirk
