#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <ctype.h>
#include <cstring>
#include <stdlib.h>

namespace dirk {
struct UserSettings {
  uint8_t brightness=85;
  uint8_t dimPercent=25;
  uint16_t dimSeconds=30;
  uint16_t blackSeconds=120;
  bool use24=false;
  int32_t gmtOffset=-18000;      // Manual/fallback UTC offset.
  bool autoTime=true;            // NTP when Wi-Fi is available.
  bool autoTimezone=true;        // Resolve local offset from GNSS position.
  bool distanceMiles=false;
};

class SystemServices {
public:
  void begin(){
    prefs.begin("dirk",false);
    load(); activity=millis(); applyBrightness(settings.brightness);
    restoreAutoZone();
    sampleBattery(true);
  }

  void update(){
    sampleBattery(false);
    updateHeaderSerial();
    if(sleepInhibit){
      if(black||dim){ black=false; dim=false; applyBrightness(settings.brightness); }
      activity=millis(); return;
    }
    uint32_t idle=(millis()-activity)/1000;
    if(settings.blackSeconds && idle>=settings.blackSeconds){
      if(!black){ M5Cardputer.Display.setBrightness(0); black=true; dim=false; }
    } else if(settings.dimSeconds && idle>=settings.dimSeconds){
      if(!dim){ applyBrightness((settings.brightness*settings.dimPercent)/100); dim=true; }
    }
  }

  bool wakeOnInput(){
    bool was=black||dim;
    activity=millis(); black=false; dim=false;
    applyBrightness(settings.brightness);
    return was;
  }
  void touch(){ activity=millis(); }
  void setSleepInhibit(bool on){
    // Wardrive calls this every loop. Avoid reprogramming the backlight on every
    // frame: only update brightness on the transition into inhibited sleep.
    if(on!=sleepInhibit){
      sleepInhibit=on;
      if(on){ activity=millis(); black=false; dim=false; applyBrightness(settings.brightness); }
    } else if(on){ activity=millis(); }
  }
  bool sleepInhibited() const { return sleepInhibit; }
  UserSettings& cfg(){ return settings; }
  const UserSettings& cfg() const { return settings; }
  void applyNow(){ applyBrightness(settings.brightness); }

  void save(){
    prefs.putUChar("bright",settings.brightness);
    prefs.putUChar("dimPct",settings.dimPercent);
    prefs.putUShort("dimSec",settings.dimSeconds);
    prefs.putUShort("blkSec",settings.blackSeconds);
    prefs.putBool("24h",settings.use24);
    prefs.putInt("gmt",settings.gmtOffset);
    prefs.putBool("ntp",settings.autoTime);
    prefs.putBool("tzAuto",settings.autoTimezone);
    prefs.putBool("distMi",settings.distanceMiles);
    ++statusSerial;
  }

  bool connectWifi(const String& ssid,const String& pass,uint32_t timeout=12000){
    WiFi.mode(WIFI_STA); WiFi.begin(ssid.c_str(),pass.c_str());
    uint32_t started=millis();
    while(WiFi.status()!=WL_CONNECTED && millis()-started<timeout){
      delay(25); M5Cardputer.update();
    }
    if(WiFi.status()==WL_CONNECTED){
      prefs.putString("ssid",ssid); prefs.putString("pass",pass);
      if(settings.autoTime) syncTime();
      return true;
    }
    return false;
  }
  bool autoConnect(){
    String s=prefs.getString("ssid","");
    if(!s.length()) return false;
    return connectWifi(s,prefs.getString("pass",""),5000);
  }
  void forgetWifi(){
    WiFi.disconnect(true,true); prefs.remove("ssid"); prefs.remove("pass");
  }
  bool connected() const { return WiFi.status()==WL_CONNECTED; }
  String connectedSsid() const { return connected()?WiFi.SSID():String(); }
  IPAddress ip() const { return WiFi.localIP(); }

  // Keep the ESP32 system clock in UTC. Local conversion is handled below so
  // GNSS and NTP can safely share the same clock source.
  void syncTime(){ configTime(0,0,"pool.ntp.org","time.nist.gov"); }

  // GNSS supplies UTC plus position. GPS itself does not transmit a political
  // timezone, so DIRK resolves a compact offline zone profile from coordinates
  // and caches it. Manual UTC offset remains available for boundary/exceptions.
  void updateGpsTime(time_t utc,bool haveFix,double lat,double lon){
    if(utc>1700000000){
      time_t now=time(nullptr);
      if(lastGpsSyncMs==0 || millis()-lastGpsSyncMs>=60000u || llabs((long long)now-(long long)utc)>2){
        timeval tv{}; tv.tv_sec=utc; tv.tv_usec=0; settimeofday(&tv,nullptr);
        lastGpsSyncMs=millis(); gpsClockValid=true; ++statusSerial;
      }
    }
    if(settings.autoTimezone && haveFix){
      if(!zonePositionValid || fabs(lat-zoneLat)>0.20 || fabs(lon-zoneLon)>0.20 || millis()-lastZoneResolveMs>900000u){
        resolveAutoZone(lat,lon);
        lastZoneResolveMs=millis(); zoneLat=lat; zoneLon=lon; zonePositionValid=true;
      }
    }
  }

  String clockText() const { return formatClock(false); }
  String headerClockText() const { return formatClock(true); }

  String timezoneText() const {
    if(settings.autoTimezone){
      String s="auto "; s+=zoneLabel;
      return s;
    }
    return String("manual ")+offsetText(settings.gmtOffset);
  }

  int32_t effectiveUtcOffset() const {
    time_t now=time(nullptr);
    if(!settings.autoTimezone) return settings.gmtOffset;
    int32_t off=autoBaseOffset;
    if(autoDstRule==DIRK_DST_US && usDstActive(now,autoBaseOffset)) off+=3600;
    else if(autoDstRule==DIRK_DST_EU && euDstActive(now)) off+=3600;
    else if(autoDstRule==DIRK_DST_AU && auDstActive(now,autoBaseOffset)) off+=3600;
    else if(autoDstRule==DIRK_DST_NZ && nzDstActive(now,autoBaseOffset)) off+=3600;
    return off;
  }

  int batteryPercent() const { return batteryPct; }
  bool batteryValid() const { return batteryPct>=0; }
  uint32_t headerSerial() const { return statusSerial; }
  bool gpsClockSynced() const { return gpsClockValid; }

private:
  enum DirkDstRule : uint8_t { DIRK_DST_NONE=0, DIRK_DST_US=1, DIRK_DST_EU=2, DIRK_DST_AU=3, DIRK_DST_NZ=4 };
  Preferences prefs;
  UserSettings settings;
  uint32_t activity=0;
  bool dim=false,black=false,sleepInhibit=false;

  int batteryPct=-1;
  float batterySmooth=-1.0f;
  uint32_t lastBatterySample=0;
  mutable uint32_t statusSerial=1;
  int64_t lastHeaderMinute=-1;
  int lastHeaderBattery=-999;

  uint32_t lastGpsSyncMs=0,lastZoneResolveMs=0;
  bool gpsClockValid=false,zonePositionValid=false;
  double zoneLat=0,zoneLon=0;
  int32_t autoBaseOffset=-18000;
  uint8_t autoDstRule=DIRK_DST_US;
  char zoneLabel[12]="ET";

  void applyBrightness(uint8_t p){
    uint8_t value=(uint8_t)map(constrain((int)p,0,100),0,100,0,255);
    M5Cardputer.Display.setBrightness(value);
  }

  void load(){
    settings.brightness=prefs.getUChar("bright",85);
    settings.dimPercent=prefs.getUChar("dimPct",25);
    settings.dimSeconds=prefs.getUShort("dimSec",30);
    settings.blackSeconds=prefs.getUShort("blkSec",120);
    settings.use24=prefs.getBool("24h",false);
    settings.gmtOffset=prefs.getInt("gmt",-18000);
    settings.autoTime=prefs.getBool("ntp",true);
    settings.autoTimezone=prefs.getBool("tzAuto",true);
    settings.distanceMiles=prefs.getBool("distMi",false);
  }

  void restoreAutoZone(){
    autoBaseOffset=prefs.getInt("tzOff",settings.gmtOffset);
    autoDstRule=prefs.getUChar("tzRule",DIRK_DST_US);
    String l=prefs.getString("tzName","");
    if(l.length()) snprintf(zoneLabel,sizeof(zoneLabel),"%s",l.substring(0,sizeof(zoneLabel)-1).c_str());
  }

  void sampleBattery(bool force){
    uint32_t now=millis();
    if(!force && now-lastBatterySample<1500u) return;
    lastBatterySample=now;
    int raw=(int)M5Cardputer.Power.getBatteryLevel();
    if(raw<0 || raw>100) return;
    if(batterySmooth<0) batterySmooth=(float)raw;
    else batterySmooth=batterySmooth*0.72f+(float)raw*0.28f;
    int next=constrain((int)lroundf(batterySmooth),0,100);
    if(next!=batteryPct){ batteryPct=next; ++statusSerial; }
  }

  void updateHeaderSerial(){
    time_t now=time(nullptr);
    int64_t minute=(now>1700000000)?(int64_t)(now/60):-1;
    if(minute!=lastHeaderMinute){ lastHeaderMinute=minute; ++statusSerial; }
    if(batteryPct!=lastHeaderBattery){ lastHeaderBattery=batteryPct; ++statusSerial; }
  }

  static int64_t daysFromCivil(int y,unsigned m,unsigned d){
    y-=m<=2;
    const int era=(y>=0?y:y-399)/400;
    const unsigned yoe=(unsigned)(y-era*400);
    const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;
    const unsigned doe=yoe*365+yoe/4-yoe/100+doy;
    return (int64_t)era*146097+(int64_t)doe-719468;
  }
  static int weekday(int y,int m,int d){
    int w=(int)((daysFromCivil(y,(unsigned)m,(unsigned)d)+4)%7); if(w<0) w+=7; return w; // 0=Sun
  }
  static int nthSunday(int y,int m,int n){ int w=weekday(y,m,1); return 1+((7-w)%7)+(n-1)*7; }
  static int lastSunday(int y,int m){
    static const int mdays[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
    int d=mdays[m]; if(m==2 && ((y%4==0&&y%100!=0)||y%400==0)) d=29;
    return d-weekday(y,m,d);
  }

  static time_t utcEpoch(int y,int m,int d,int hour,int minute=0){
    return (time_t)(daysFromCivil(y,(unsigned)m,(unsigned)d)*86400LL + (int64_t)hour*3600LL + (int64_t)minute*60LL);
  }

  static int utcYear(time_t utc){
    struct tm t{}; gmtime_r(&utc,&t); return t.tm_year+1900;
  }

  static bool usDstActive(time_t utc,int32_t baseOffset){
    // US rules since 2007:
    //   starts 02:00 LOCAL STANDARD on the second Sunday in March
    //   ends   02:00 LOCAL DAYLIGHT on the first Sunday in November
    // Convert those local wall-clock instants to UTC explicitly. The previous
    // local-standard comparison kept DST active one hour too long in November.
    int y=utcYear(utc);
    int sd=nthSunday(y,3,2);
    int ed=nthSunday(y,11,1);
    time_t start=utcEpoch(y,3,sd,2) - baseOffset;
    time_t end  =utcEpoch(y,11,ed,2) - (baseOffset+3600);
    return utc>=start && utc<end;
  }

  static bool euDstActive(time_t utc){
    // EU/UK change at 01:00 UTC on the last Sundays of March and October.
    int y=utcYear(utc);
    time_t start=utcEpoch(y,3,lastSunday(y,3),1);
    time_t end=utcEpoch(y,10,lastSunday(y,10),1);
    return utc>=start && utc<end;
  }

  static bool auDstActive(time_t utc,int32_t baseOffset){
    // Eastern/southern Australia profile used by DIRK's compact resolver:
    // first Sunday October 02:00 standard -> first Sunday April 03:00 daylight.
    struct tm t{}; gmtime_r(&utc,&t); int y=t.tm_year+1900,m=t.tm_mon+1;
    int seasonStartYear=(m>=7)?y:y-1;
    int seasonEndYear=seasonStartYear+1;
    time_t start=utcEpoch(seasonStartYear,10,nthSunday(seasonStartYear,10,1),2)-baseOffset;
    time_t end=utcEpoch(seasonEndYear,4,nthSunday(seasonEndYear,4,1),3)-(baseOffset+3600);
    return utc>=start && utc<end;
  }

  static bool nzDstActive(time_t utc,int32_t baseOffset){
    // New Zealand: last Sunday September 02:00 standard ->
    // first Sunday April 03:00 daylight.
    struct tm t{}; gmtime_r(&utc,&t); int y=t.tm_year+1900,m=t.tm_mon+1;
    int seasonStartYear=(m>=7)?y:y-1;
    int seasonEndYear=seasonStartYear+1;
    time_t start=utcEpoch(seasonStartYear,9,lastSunday(seasonStartYear,9),2)-baseOffset;
    time_t end=utcEpoch(seasonEndYear,4,nthSunday(seasonEndYear,4,1),3)-(baseOffset+3600);
    return utc>=start && utc<end;
  }

  static String offsetText(int32_t sec){
    char b[12]; char sign=sec<0?'-':'+'; int v=abs(sec); int h=v/3600,m=(v%3600)/60;
    snprintf(b,sizeof(b),"UTC%c%02d:%02d",sign,h,m); return String(b);
  }

  void setAutoZone(int32_t off,uint8_t rule,const char* label){
    bool changed=off!=autoBaseOffset || rule!=autoDstRule || strncmp(zoneLabel,label,sizeof(zoneLabel))!=0;
    autoBaseOffset=off; autoDstRule=rule; snprintf(zoneLabel,sizeof(zoneLabel),"%s",label);
    if(changed){
      prefs.putInt("tzOff",autoBaseOffset); prefs.putUChar("tzRule",autoDstRule); prefs.putString("tzName",zoneLabel); ++statusSerial;
    }
  }

  void resolveAutoZone(double lat,double lon){
    // Compact offline resolver. It intentionally covers broad political regions
    // and falls back to a longitude-derived 15-minute offset. Manual override is
    // available for boundary areas and jurisdictions with special DST rules.
    if(lat>=18 && lat<=23.5 && lon>=-161.5 && lon<=-154.0){ setAutoZone(-10*3600,DIRK_DST_NONE,"HST"); return; }
    if(lat>=50 && lat<=72 && lon>=-171 && lon<-130){ setAutoZone(-9*3600,DIRK_DST_US,"AKT"); return; }
    if(lat>=24 && lat<=60 && lon>=-130 && lon<=-52){
      if(lon>-67.5) setAutoZone(-4*3600,DIRK_DST_US,"AT");
      else if(lon>-85.0) setAutoZone(-5*3600,DIRK_DST_US,"ET");
      else if(lon>-100.0) setAutoZone(-6*3600,DIRK_DST_US,"CT");
      else if(lon>-115.0) setAutoZone(-7*3600,DIRK_DST_US,"MT");
      else setAutoZone(-8*3600,DIRK_DST_US,"PT");
      return;
    }
    if(lat>=49 && lat<=61 && lon>=-11 && lon<2.5){ setAutoZone(0,DIRK_DST_EU,"UK"); return; }
    if(lat>=35 && lat<=72 && lon>=2.5 && lon<22.5){ setAutoZone(3600,DIRK_DST_EU,"CET"); return; }
    if(lat>=35 && lat<=72 && lon>=22.5 && lon<40){ setAutoZone(7200,DIRK_DST_EU,"EET"); return; }
    if(lat>=26 && lat<=31 && lon>=80 && lon<=89){ setAutoZone(20700,DIRK_DST_NONE,"NPT"); return; }
    if(lat>=6 && lat<=37 && lon>=68 && lon<=98){ setAutoZone(19800,DIRK_DST_NONE,"IST"); return; }
    if(lat>=18 && lat<=54 && lon>=73 && lon<=135){ setAutoZone(28800,DIRK_DST_NONE,"CST"); return; }
    if(lat>=30 && lat<=46 && lon>=128 && lon<=147){ setAutoZone(32400,DIRK_DST_NONE,"JST"); return; }
    if(lat>=-44 && lat<=-10 && lon>=112 && lon<=154){
      if(lon<129) setAutoZone(8*3600,DIRK_DST_NONE,"AWST");
      else if(lon<141) setAutoZone(34200,DIRK_DST_NONE,"ACST");
      else setAutoZone(10*3600,DIRK_DST_AU,"AEST");
      return;
    }
    if(lat>=-48 && lat<=-33 && lon>=165 && lon<=180){ setAutoZone(12*3600,DIRK_DST_NZ,"NZST"); return; }
    int32_t q=(int32_t)lround(lon/3.75); // 15 minutes per 3.75 degrees.
    q=constrain((int)q,-48,56);
    setAutoZone(q*900,DIRK_DST_NONE,"AUTO");
  }

  String formatClock(bool compact) const {
    time_t now=time(nullptr); if(now<1700000000) return "--:--";
    now+=effectiveUtcOffset(); struct tm t{}; gmtime_r(&now,&t);
    char b[16];
    if(settings.use24) strftime(b,sizeof(b),"%H:%M",&t);
    else {
      strftime(b,sizeof(b),compact?"%I:%M%p":"%I:%M %p",&t);
      if(b[0]=='0') memmove(b,b+1,strlen(b));
      if(compact){ size_t n=strlen(b); if(n>=2){ b[n-2]=(char)tolower((unsigned char)b[n-2]); b[n-1]='\0'; } }
    }
    return String(b);
  }
};
}
