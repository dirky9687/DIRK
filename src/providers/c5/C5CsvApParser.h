#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

namespace dirk {
// JanOS scan_networks emits CSV-style rows in some releases:
// "SSID","BSSID","channel","auth","RSSI","band".
// Decode independently of Arduino so host regression tests can exercise
// the exact firmware parser. Do not mistake non-AP UART messages for rows.
struct C5CsvApRecord {
  char ssid[33]{};
  char bssid[18]{};
  char security[24]{};
  int channel = -1;
  int rssi = -200;
};

inline bool c5IsHex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
inline bool c5IsMac(const char* s) {
  if (strlen(s) != 17) return false;
  const char sep = s[2];
  if (sep != ':' && sep != '-') return false;
  for (int i=0; i<17; i++) {
    if ((i+1)%3 == 0) { if (s[i] != sep) return false; }
    else if (!c5IsHex(s[i])) return false;
  }
  return true;
}
inline bool c5Num(const char* s, int& out) {
  if (!s || !*s) return false;
  char* end=nullptr;
  const long value=strtol(s,&end,10);
  while (end && isspace((unsigned char)*end)) ++end;
  if (end==s || !end || *end || value < -1000 || value > 10000) return false;
  out=(int)value;return true;
}
inline void c5Copy(char* out, size_t capacity, const char* src) {
  if (!capacity) return;
  size_t i=0;while (i+1<capacity && src[i]) {out[i]=src[i];++i;}
  out[i]='\0';
}
inline bool parseC5CsvAp(const char* line, C5CsvApRecord& out) {
  if (!line) return false;
  // Small bounded local decode, no heap allocation or unbounded fields.
  constexpr int MAX_FIELDS=14;
  constexpr size_t FIELD_CAP=80;
  char fields[MAX_FIELDS][FIELD_CAP]{};
  int count=0;
  bool quoted=false;
  size_t n=0;
  bool sawComma=false;
  for (const char* p=line;;++p) {
    char c=*p;
    if (!c || (c==',' && !quoted)) {
      if (count>=MAX_FIELDS) return false;
      fields[count][n]='\0';
      // Trim spaces outside quotes for normal JanOS CSV columns.
      size_t start=0;
      while (fields[count][start] && isspace((unsigned char)fields[count][start])) ++start;
      size_t end=strlen(fields[count]);
      while (end>start && isspace((unsigned char)fields[count][end-1])) --end;
      fields[count][end]='\0';
      if(start) memmove(fields[count],fields[count]+start,end-start+1);
      ++count;n=0;
      if (!c) break;
      sawComma=true;
      continue;
    }
    if (c=='"') {
      if (quoted && p[1]=='"') { // RFC 4180 escaped quote
        if(n+1<FIELD_CAP) fields[count][n++]=c;
        ++p;
      } else quoted=!quoted;
      continue;
    }
    if (count>=MAX_FIELDS) return false;
    if (n+1<FIELD_CAP) fields[count][n++]=c;
    else return false; // Don't silently accept truncated MAC or channel.
  }
  if (quoted || !sawComma || count<4) return false;

  int macIdx=-1;
  for(int i=0;i<count;i++) if(c5IsMac(fields[i])) {macIdx=i;break;}
  if(macIdx<0) return false;
  int ch=-1, chIdx=-1, rssi=-200, rssiIdx=-1;
  for(int i=macIdx+1;i<count;i++) {
    int v=0;
    if (!c5Num(fields[i],v)) continue;
    if (chIdx<0 && v>=1 && v<=177) {ch=v;chIdx=i;continue;}
    if (chIdx>=0 && rssiIdx<0 && v>=-127 && v<=0) {rssi=v;rssiIdx=i;break;}
  }
  if(chIdx<0 || rssiIdx<0) return false;

  // Check explicit band label if present; channels and band must agree.
  for(int i=0;i<count;i++) {
    const char* f=fields[i];
    if(strstr(f,"2.4GHz") || strstr(f,"2.4ghz") || strstr(f,"2GHz") || strstr(f,"2ghz")) {
      if (ch>14) return false;
    }
    if(strstr(f,"5GHz") || strstr(f,"5ghz")) {
      if (ch<=14) return false;
    }
    if(strstr(f,"6GHz") || strstr(f,"6ghz")) return false;
  }
  out=C5CsvApRecord{};
  // SSID appears before BSSID in JanOS scan rows; optional extra
  // fields (vendor, hidden-SSID marker, etc.) can precede the BSSID.
  if(macIdx>0) c5Copy(out.ssid,sizeof(out.ssid),fields[0]);
  c5Copy(out.bssid,sizeof(out.bssid),fields[macIdx]);
  if(chIdx+1<count && chIdx+1!=rssiIdx)
    c5Copy(out.security,sizeof(out.security),fields[chIdx+1]);
  out.channel=ch;out.rssi=rssi;
  return true;
}
} // namespace dirk
