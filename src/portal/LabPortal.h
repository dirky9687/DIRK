#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include "../storage/StorageManager.h"

namespace dirk {
// Isolated, time-bounded local portal demonstration. The only custom content
// is a short escaped TEXT notice from SD (never executable HTML/JS/forms).
// No credential prompts, no POST handlers, no secret collection or redirect.
class LabPortal {
public:
  LabPortal():server(80){}
  bool begin(const char* ssid,StorageManager* storage=nullptr){
    if(active) stop();
    notice="Authorized DIRK wireless lab. No credentials are requested.";
    if(storage && storage->mounted() && SD.exists("/dirk/portal/notice.txt")){
      digitalWrite(StorageManager::LORA_CS,HIGH);
      File f=SD.open("/dirk/portal/notice.txt",FILE_READ);
      if(f && f.size()<=1024 && f.size()>0){
        String text="";text.reserve((size_t)f.size());
        while(f.available() && text.length()<1024)text+=(char)f.read();
        notice=text;
      }
      if(f)f.close();
    }
    if(!WiFi.softAP(ssid))return false;
    if(!routesReady){
    server.on("/",HTTP_GET,[this](){
      String html="<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'></head><body>";
      html+="<h2>DIRK LAB</h2><p>Authorized local wireless demonstration.</p><pre>";
      html+=escapeHtml(notice);
      html+="</pre><p>No logins or passwords are requested or stored.</p></body></html>";
      server.sendHeader("Cache-Control","no-store");
      server.send(200,"text/html; charset=utf-8",html);
    });
    server.onNotFound([this](){server.send(404,"text/plain","DIRK lab: open the root page");});
    routesReady=true;
    }
    server.begin(); active=true; startedAt=millis();return true;
  }
  void update(){
    if(!active)return;
    if((uint32_t)(millis()-startedAt)>=300000u){stop();return;}
    server.handleClient();
  }
  bool running() const { return active; }
  void stop(){if(active){server.stop();WiFi.softAPdisconnect(true);active=false;}}
private:
  static String escapeHtml(const String& input){
    String s="";s.reserve(input.length()+32);
    for(size_t i=0;i<input.length();++i){
      switch(input[i]){
        case '&':s+="&amp;";break;
        case '<':s+="&lt;";break;
        case '>':s+="&gt;";break;
        case '\"':s+="&quot;";break;
        case '\'':s+="&#39;";break;
        default:s+=input[i];break;
      }
    }
    return s;
  }
  WebServer server;
  String notice;
  bool active=false;
  bool routesReady=false;
  uint32_t startedAt=0;
};
}
