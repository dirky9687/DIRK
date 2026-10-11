#pragma once
#include <ctype.h>
namespace dirk {
// UART-only 5GHz observer for JanOS/projectZero on the MonsterC5.
// The ESP32-S3 itself cannot scan 5 GHz; we only publish results supplied
// by the C5. Keep scanning, parsing and UI refresh separate.
class JanOSAdapter {
public:
  void begin(C5Transport* t,TargetManager* tm){
    transport=t; targets=tm; ready_=false; scanning_=false;
    parsed_=0; rawLines_=unparsed_=0; visualSerial_=0;
    noMac_=badChannel_=badRssi_=0;
    lastLine_=""; lastEvent_="idle"; scanStarted=0;
    scanStartParsed_=0; fallbackSent_=false; sawScanReply_=false;
  }
  void ping(){ if(transport){ lastEvent_="ping"; transport->sendLine("ping"); visualSerial_++; } }
  void probe(){ ping(); }
  bool startScan5(){
    if(!transport || scanning_)return false;
    const uint32_t now=millis();
    // A complete dual-band scan can take several seconds. Calls from the
    // wardrive and Wi-Fi menus must not restart it every 0.5-3 seconds.
    if(scanStarted && (uint32_t)(now-scanStarted)<11000u) return false;
    scanStarted=now; scanStartParsed_=parsed_; fallbackSent_=false;
    sawScanReply_=false; scanning_=true; lastEvent_="scan_networks";
    // JanOS CLI command. Some forks use `scan`, handled only as fallback.
    transport->sendLine("scan_networks"); visualSerial_++;
    return true;
  }
  void stop(){
    if(!transport || !scanning_)return;
    transport->sendLine("stop"); scanning_=false; lastEvent_="stop"; visualSerial_++;
  }
  void update(){
    if(!transport)return;
    transport->update(); String line;
    uint8_t budget=12;
    while(budget-- && transport->readLine(line)){
      rawLines_++; lastLine_=line;
      // UART traffic alone does not confirm the JanOS protocol.
      // ready_ is set only by a known handshake, scan event or valid AP.
      parseLine(line);
    }
    if(!scanning_)return;
    const uint32_t age=millis()-scanStarted;
    if(!fallbackSent_ && age>=6500u && parsed_==scanStartParsed_ && !sawScanReply_){
      sendFallback();
    }
    if(age>(fallbackSent_?22000u:16000u)){
      scanning_=false; lastEvent_="scan timeout"; visualSerial_++;
    }
  }
  bool available()const{return transport&&transport->everDetected();}
  bool ready()const{return ready_;}
  bool scanning()const{return scanning_;}
  uint32_t parsed()const{return parsed_;}
  uint32_t rawLines()const{return rawLines_;}
  uint32_t unparsed()const{return unparsed_;}
  uint32_t noMac()const{return noMac_;}
  uint32_t badChannel()const{return badChannel_;}
  uint32_t badRssi()const{return badRssi_;}
  uint32_t visualSerial()const{return visualSerial_;}
  const String& lastLine()const{return lastLine_;}
  const String& lastEvent()const{return lastEvent_;}
  const C5Transport* transportInfo() const { return transport; }
private:
  C5Transport* transport=nullptr; TargetManager* targets=nullptr;
  bool ready_=false,scanning_=false,fallbackSent_=false,sawScanReply_=false;
  uint32_t parsed_=0, rawLines_=0, unparsed_=0, scanStarted=0, scanStartParsed_=0, visualSerial_=0;
  uint32_t noMac_=0,badChannel_=0,badRssi_=0;
  String lastLine_,lastEvent_;

  void sendFallback(){
    if(!transport || fallbackSent_)return;
    transport->sendLine("scan"); fallbackSent_=true;
    lastEvent_="fallback scan"; visualSerial_++;
  }
  static int hexVal(char c){
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
  }
  static bool parseMac(const String& s,Mac& m){
    // Exact six 2-digit octets; sscanf("%x") could consume arbitrary widths
    // and mistakenly turn other UART text into a BSSID.
    for(int p=0;p+16<s.length();p++){
      if(p && hexVal(s[p-1])>=0) continue;
      const char sep=s[p+2];
      if(sep!=':' && sep!='-')continue;
      uint8_t bytes[6]{}; bool valid=true;
      for(int i=0;i<6;i++){
        int a=hexVal(s[p+i*3]), b=hexVal(s[p+i*3+1]);
        if(a<0 || b<0 || (i<5 && s[p+i*3+2]!=sep)){valid=false;break;}
        bytes[i]=(uint8_t)((a<<4)|b);
      }
      if(valid && (p+17==s.length() || hexVal(s[p+17])<0)){
        memcpy(m.b,bytes,6);return true;
      }
    }
    return false;
  }
  static int findNumAfter(const String& s,const char* key,int def){
    int p=s.indexOf(key);if(p<0)return def;
    p+=(int)strlen(key);
    while(p<s.length()&&(s[p]==' '||s[p]=='='||s[p]==':'||s[p]=='['||s[p]=='"'))p++;
    if(p>=s.length() || (s[p]!='-' && (s[p]<'0'||s[p]>'9'))) return def;
    return s.substring(p).toInt();
  }
  static String fieldString(const String& original,const String& low,const char* key){
    // `ssid` is part of `bssid`; match an actual field name, not a substring.
    int p=-1;const int keyLen=(int)strlen(key);int from=0;
    while((p=low.indexOf(key,from))>=0){
      const bool leftOk=(p==0 || !isalnum((unsigned char)low[p-1]));
      const int right=p+keyLen;
      const bool rightOk=(right>=low.length() || !isalnum((unsigned char)low[right]));
      if(leftOk && rightOk) break;
      from=p+keyLen;
    }
    if(p<0)return String();
    int sep=original.indexOf(':',p);if(sep<0)sep=original.indexOf('=',p);if(sep<0)return String();
    int start=sep+1;
    while(start<original.length() && (original[start]==' '||original[start]=='"'))start++;
    int end=start;
    while(end<original.length() && original[end]!=',' && original[end]!='}' && original[end]!='"')end++;
    String out=original.substring(start,end);out.trim();return out;
  }
  static bool valid5(int ch){
    return (ch>=32&&ch<=68) || (ch>=96&&ch<=144) || (ch>=149&&ch<=177);
  }
  void parseLine(String line){
    line.trim();if(!line.length())return;
    String low=line;low.toLowerCase();
    if(low=="pong" || low.indexOf("janos")>=0){
      ready_=true; lastEvent_="pong / janos"; visualSerial_++;return;
    }
    if(low.indexOf("scan_start")>=0 || low.indexOf("scan started")>=0){
      scanning_=true;ready_=true;sawScanReply_=true;lastEvent_="scan started";visualSerial_++;return;
    }
    if(low.indexOf("scan_complete")>=0 || low.indexOf("scan complete")>=0 || low.indexOf("scan_done")>=0){
      ready_=true;sawScanReply_=true;
      if(scanning_ && parsed_==scanStartParsed_ && !fallbackSent_){ sendFallback();return; }
      scanning_=false;lastEvent_="scan complete";visualSerial_++;return;
    }

    Mac mac{};
    if(!parseMac(line,mac)){unparsed_++;noMac_++;return;}
    int ch=findNumAfter(low,"channel",-1);
    if(ch<0)ch=findNumAfter(low,"\"ch\"",-1);
    if(ch<0)ch=findNumAfter(low,"ch:",-1);
    if(ch<0)ch=findNumAfter(low,"ch=",-1);
    if(ch<0)ch=findNumAfter(low,"primary",-1);
    if(ch<0){
      int mhz=findNumAfter(low,"frequency",-1);
      if(mhz<0)mhz=findNumAfter(low,"freq",-1);
      if(mhz>=5000 && mhz<=5900)ch=(mhz-5000)/5;
    }
    if(!valid5(ch)){unparsed_++;badChannel_++;return;}
    int rssi=findNumAfter(low,"rssi",-200);
    if(rssi==-200)rssi=findNumAfter(low,"signal",-200);
    if(rssi==-200 || rssi < -127 || rssi>0){unparsed_++;badRssi_++;return;}

    WifiTarget t{};t.bssid=mac;t.channel=(uint8_t)ch;t.rssi=(int8_t)rssi;
    t.band=Band::Band5;t.lastSeen=millis();
    String ss=fieldString(line,low,"ssid");
    if(ss.length())strlcpy(t.ssid,ss.c_str(),sizeof(t.ssid));
    if(!t.ssid[0])strlcpy(t.ssid,"<5G AP>",sizeof(t.ssid));
    if(low.indexOf("wpa3")>=0){
#ifdef WIFI_AUTH_WPA3_PSK
      t.auth=(uint8_t)WIFI_AUTH_WPA3_PSK;
#else
      t.auth=(uint8_t)WIFI_AUTH_WPA2_PSK;
#endif
    }else if(low.indexOf("wpa2")>=0)t.auth=(uint8_t)WIFI_AUTH_WPA2_PSK;
    else if(low.indexOf("wpa")>=0)t.auth=(uint8_t)WIFI_AUTH_WPA_PSK;
    else if(low.indexOf("open")>=0)t.auth=(uint8_t)WIFI_AUTH_OPEN;
    if(targets && targets->observe(t)){
      ready_=true;parsed_++;sawScanReply_=true;lastEvent_="5g ap parsed";visualSerial_++;
    }
  }
};
}
