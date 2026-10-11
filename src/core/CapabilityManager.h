#pragma once
#include <dirk/types.h>

namespace dirk {
struct Capabilities {
  bool wifi24=false;
  bool wifi5=false;
  bool ble=false;
  bool gps=false;
  bool sd=false;
  bool c5=false;
  bool lora=false;
  bool ir=false;
};

class CapabilityManager {
public:
  void begin(){ caps = {}; }
  const Capabilities& get() const { return caps; }
  Capabilities& mutableCaps(){ return caps; }
private:
  Capabilities caps{};
};
}
