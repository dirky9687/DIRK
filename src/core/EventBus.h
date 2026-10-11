#pragma once
#include <Arduino.h>

namespace dirk {
enum class EventType:uint16_t {
  Ready=1, TargetSeen, TargetSaved, StorageChanged, ModeChanged,
  OperationChanged, C5Line, GpsFix, Warning, Error
};
struct Event { EventType type=EventType::Ready; uint32_t a=0,b=0,at=0; };

class EventBus {
public:
  bool begin(){ head=tail=count=0; return true; }
  bool publish(Event e){
    if(count>=MAX) return false;
    e.at=millis(); q[tail]=e; tail=(tail+1)%MAX; ++count; return true;
  }
  bool pop(Event& e){
    if(!count) return false;
    e=q[head]; head=(head+1)%MAX; --count; return true;
  }
private:
  static constexpr uint8_t MAX=32;
  Event q[MAX]{};
  uint8_t head=0,tail=0,count=0;
};
}
