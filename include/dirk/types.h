#pragma once
#include <Arduino.h>
#include <cstring>

namespace dirk {
enum class TeamMode : uint8_t { Blue=0, Red=1 };
enum class Band : uint8_t { Unknown=0, Band24=1, Band5=2 };
enum class Availability : uint8_t { Unknown=0, Unavailable, Initializing, Ready, Busy, Error };

struct Mac { uint8_t b[6]{}; };
inline bool macEq(const Mac& a,const Mac& b){ return memcmp(a.b,b.b,6)==0; }

struct WifiTarget {
  uint32_t id=0;
  Mac bssid{};
  char ssid[33]{};
  int8_t rssi=-127;
  uint8_t channel=0;
  Band band=Band::Unknown;
  uint8_t auth=0;
  char cipher[16]{};
  char akm[16]{};
  bool pmfCapable=false;
  bool pmfRequired=false;
  bool wps=false;
  uint16_t beaconInterval=0;
  uint8_t dtimPeriod=0;
  char country[4]{};
  bool ht=false;
  bool vht=false;
  bool he=false;
  bool wmm=false;
  uint32_t beaconCount=0;
  bool saved=false;
  bool authorized=false;
  uint32_t firstSeen=0;
  uint32_t lastSeen=0;
};

struct WifiAirStats {
  uint32_t beacons=0;
  uint32_t probes=0;
  uint32_t deauth=0;
  uint32_t disassoc=0;
  uint32_t data=0;
  uint16_t uniqueStations=0;
};

struct GpsFix {
  bool valid=false;
  double lat=0;
  double lon=0;
  float alt=0;
  float hdop=0;
  float speedKph=0;
  float courseDeg=0;
  uint8_t sats=0;
  uint32_t at=0;
};
}
