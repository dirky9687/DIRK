#pragma once
#include <Arduino.h>
namespace dirk {
class AuditManager {
public:
  bool isSupportedCapture(const char* path) const {
    return path && (strstr(path,".pcap") || strstr(path,".pcapng"));
  }
};
}
