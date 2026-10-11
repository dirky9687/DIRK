#include <iostream>
#include <cassert>
#include "../../../src/providers/gps/GpsProvider.h"
int main(){
  dirk::GpsProvider g;
  assert(g.begin());
  assert(Serial2.bufferSize==2048);
  assert(!g.present()); assert(!g.everReceived()); assert(!g.fixed());
  Serial2.feed("$GPRMC,123456,V,,,,,\r\n");
  g.update();
  assert(g.everReceived()); assert(g.nmeaLines()==1);
  assert(g.rxBytes()==std::string("$GPRMC,123456,V,,,,,\r\n").size()); assert(g.checksumOk()==1);
  assert(g.present()); assert(!g.fixed());
  assert(std::string(g.lastNmea())=="$GPRMC,123456,V,,,,,");
  testMs=9001;
  assert(!g.present());
  Serial2.feed(std::string(600,'X'));
  const uint32_t prior=g.rxBytes();
  g.update();
  assert(g.rxBytes()-prior<=384);
  g.update();
  assert(g.rxBytes()-prior==600);
  g.setBaud(9600);
  assert(g.baud()==9600 && Serial2.lastBaud==9600);
  assert(!g.everReceived() && !g.present());
  Serial2.feed("$GPRMC,123456,V,,,,,\r\n");g.update();
  assert(g.everReceived() && g.nmeaLines()==1);
  g.setBaud(115200);
  assert(g.baud()==115200 && Serial2.lastBaud==115200);
  std::cout << "PASS: startup status, NMEA capture, fix distinction, freshness timeout, bounded UART, 2KB buffer\n";
}
