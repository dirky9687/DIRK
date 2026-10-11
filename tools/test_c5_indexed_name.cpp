#include <cassert>
#include <cstring>
#include <cstdio>
#include "../src/providers/c5/C5CsvApParser.h"
#include "../src/providers/c5/C5IndexedNameRepair.h"
using namespace dirk;
int main(){
  C5CsvApRecord r{};
  const char* captured="\"33\",\"Demo5G-Sample\",\"\",\"02:11:22:33:44:55\",\"100\",\"WPA2/WPA3 Mixed\",\"-96\",\"5GHz\"";
  assert(parseC5CsvAp(captured,r));
  assert(!strcmp(r.ssid,"33") && r.channel==100 && r.rssi==-96);
  assert(c5RepairIndexedSsid(captured,r));
  assert(!strcmp(r.ssid,"Demo5G-Sample") && r.channel==100 && r.rssi==-96);
  const char* old="\"DemoDualBand\",\"AA:BB:CC:DD:EE:01\",\"36\",\"WPA3\",\"-52\",\"5GHz\"";
  assert(parseC5CsvAp(old,r));
  assert(!c5RepairIndexedSsid(old,r) && !strcmp(r.ssid,"DemoDualBand"));
  const char* numeric="\"33\",\"AA:BB:CC:DD:EE:01\",\"36\",\"WPA3\",\"-52\",\"5GHz\"";
  assert(parseC5CsvAp(numeric,r));
  assert(!c5RepairIndexedSsid(numeric,r) && !strcmp(r.ssid,"33"));
  const char* indexed="\"7\",\"DemoConsole\",\"\",\"AA:BB:CC:DD:EE:02\",\"153\",\"WPA2\",\"-44\",\"5GHz\"";
  assert(parseC5CsvAp(indexed,r));
  assert(c5RepairIndexedSsid(indexed,r) && !strcmp(r.ssid,"DemoConsole") && r.channel==153);
  const char* escaped="\"8\",\"Example \"\"Guest\"\"\",\"\",\"AA:BB:CC:DD:EE:03\",\"44\",\"WPA2\",\"-55\",\"5GHz\"";
  assert(parseC5CsvAp(escaped,r));
  assert(c5RepairIndexedSsid(escaped,r) && !strcmp(r.ssid,"Example \"Guest\""));
  puts("PASS: indexed-SSID repair and backward compatibility");
}
