#include <cassert>
#include <cstdio>
#include "../src/providers/c5/C5CsvApParser.h"
using namespace dirk;
int main(){
  C5CsvApRecord r{};
  assert(parseC5CsvAp("\"A3\",\"\",\"12:EF:3A:01:DC:57\",\"153\",\"WPA2\",\"-93\",\"5GHz\"",r));
  assert(r.channel==153 && r.rssi==-93 && !strcmp(r.ssid,"A3") && !strcmp(r.security,"WPA2"));
  assert(parseC5CsvAp("\"DemoDualBand\",\"AA:BB:CC:DD:EE:01\",\"36\",\"WPA3\",\"-52\",\"5GHz\"",r));
  assert(r.channel==36 && r.rssi==-52 && !strcmp(r.ssid,"DemoDualBand"));
  assert(parseC5CsvAp("\"DemoConsole\",\"AA:BB:CC:DD:EE:02\",\"149\",\"WPA2\",\"-61\",\"5GHz\"",r));
  assert(parseC5CsvAp("\"comma, ssid\",\"AA:BB:CC:DD:EE:03\",\"44\",\"WPA2\",\"-40\",\"5GHz\"",r));
  assert(!strcmp(r.ssid,"comma, ssid"));
  assert(parseC5CsvAp("\"Foo\",\"AA:BB:CC:DD:EE:04\",\"11\",\"WPA2\",\"-70\",\"2.4GHz\"",r));
  assert(r.channel==11);
  assert(!parseC5CsvAp("\"Foo\",\"AA:BB:CC:DD:EE:04\",\"11\",\"WPA2\",\"-70\",\"5GHz\"",r));
  assert(!parseC5CsvAp("\"Foo\",\"AA:BB:CC:DD:EE:04\",\"36\",\"WPA2\",\"-70\",\"6GHz\"",r));
  assert(!parseC5CsvAp("scan complete",r));
  assert(!parseC5CsvAp("\"Foo\",\"notamac\",\"36\",\"WPA2\",\"-70\",\"5GHz\"",r));
  assert(!parseC5CsvAp("\"Foo\",\"AA:BB:CC:DD:EE:04\",\"36\",\"WPA2\",\"garbage\",\"5GHz\"",r));
  puts("PASS: 10 C5 CSV regression cases");
}
