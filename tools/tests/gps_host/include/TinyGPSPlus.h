#pragma once
#include <cstdint>
#include <Arduino.h>
struct MockVal {
 bool isValid() const {return false;}
 uint32_t age() const {return 0xFFFFFFFFu;}
 double meters() const {return 0.0;}
 double hdop() const {return 0.0;}
 double kmph() const {return 0.0;}
 double deg() const {return 0.0;}
 unsigned value() const {return 0;}
};
struct MockLocation: MockVal {double lat() const{return 0;} double lng() const{return 0;}};
struct MockDate: MockVal {int year() const{return 2026;} unsigned month() const{return 10;} unsigned day() const{return 8;}};
struct MockTime: MockVal {int hour() const{return 0;} int minute() const{return 0;} int second() const{return 0;}};
class TinyGPSPlus {
public:
 MockLocation location; MockVal altitude,hdop,speed,course,satellites; MockDate date; MockTime time;
 bool encode(char c){if(c=='\n')++passed_;return false;}
 uint32_t passedChecksum()const{return passed_;}
 uint32_t failedChecksum()const{return 0;}
private: uint32_t passed_=0;
};
