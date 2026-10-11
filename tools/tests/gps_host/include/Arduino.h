#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <string>
#include <deque>
constexpr int OUTPUT=1, HIGH=1, SERIAL_8N1=0;
inline uint32_t testMs=0, testUs=0;
inline uint32_t millis(){ return testMs; }
inline uint32_t micros(){ return ++testUs; }
inline void pinMode(int,int){}
inline void digitalWrite(int,int){}
struct MockSerial {
  std::deque<char> q;
  void setRxBufferSize(size_t n){ bufferSize=n; }
  void begin(int baud,int,int,int){ lastBaud=baud; }
  void end(){ q.clear(); }
  int available(){return (int)q.size();}
  int read(){char c=q.front();q.pop_front();return c;}
  void feed(const std::string& s){for(char c:s)q.push_back(c);}
  size_t bufferSize=0;
  uint32_t lastBaud=0;
};
inline MockSerial Serial2;
