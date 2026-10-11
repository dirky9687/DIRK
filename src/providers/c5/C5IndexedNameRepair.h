#pragma once
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include "C5CsvApParser.h"

namespace dirk {
// The JanOS build observed on v0.7.16 hardware uses:
// "row-number","SSID","","BSSID","channel","security","RSSI","5GHz".
// The established CSV parser expects the older SSID-first variant.
// Correct ONLY this proven indexed layout after normal CSV validation;
// leave C5CsvApParser.h, the UART transport and scan lifecycle untouched.
inline bool c5IndexedField(const char*& p, char* dst, size_t capacity) {
  if (!p || !capacity) return false;
  while (*p && isspace(static_cast<unsigned char>(*p))) ++p;
  bool quoted=false;
  size_t n=0;
  for (; *p; ++p) {
    const char c=*p;
    if (c=='"') {
      if (quoted && p[1]=='"') {
        if (n+1>=capacity) return false;
        dst[n++]=c;
        ++p;
      } else {
        quoted=!quoted;
      }
      continue;
    }
    if (!quoted && c==',') { ++p; break; }
    if (n+1>=capacity) return false;
    dst[n++]=c;
  }
  if (quoted) return false;
  dst[n]='\0';
  size_t first=0;
  while (dst[first] && isspace(static_cast<unsigned char>(dst[first]))) ++first;
  size_t last=strlen(dst);
  while (last>first && isspace(static_cast<unsigned char>(dst[last-1]))) --last;
  dst[last]='\0';
  if (first) memmove(dst,dst+first,last-first+1);
  return true;
}

inline bool c5RepairIndexedSsid(const char* line, C5CsvApRecord& record) {
  if (!line || !record.bssid[0]) return false;
  const char* p=line;
  char index[16]{}, name[80]{}, reserved[16]{}, bssid[32]{};
  if (!c5IndexedField(p,index,sizeof(index)) ||
      !c5IndexedField(p,name,sizeof(name)) ||
      !c5IndexedField(p,reserved,sizeof(reserved)) ||
      !c5IndexedField(p,bssid,sizeof(bssid))) return false;
  if (!index[0] || strcmp(index,record.ssid)!=0 || reserved[0] ||
      strcmp(bssid,record.bssid)!=0 || !c5IsMac(bssid) || !name[0] ||
      strlen(name)>32) return false;
  for (const char* q=index; *q; ++q)
    if (!isdigit(static_cast<unsigned char>(*q))) return false;
  c5Copy(record.ssid,sizeof(record.ssid),name);
  return true;
}
} // namespace dirk
