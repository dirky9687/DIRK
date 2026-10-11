#include "DirkApp.h"
#include "../assets/DirkMenuMark.h"

namespace dirk {
namespace {
static const char MATRIX_GLYPHS[] = "0123456789abcdefghijklmnopqrstuvwxyz@#$%&*+=<>?/[]{}";
static constexpr size_t MATRIX_GLYPH_COUNT = sizeof(MATRIX_GLYPHS)-1;
}

void DirkApp::drawSplashMatrix(uint8_t phase){
  auto& d=M5Cardputer.Display;
  const uint16_t head=team.mode()==TeamMode::Red?0xF800:0x07FF;
  const uint16_t mid =team.mode()==TeamMode::Red?0xA800:0x04BF;
  const uint16_t dim =team.mode()==TeamMode::Red?0x3000:0x0128;

  // Preserve the static logo/dirk rectangle. Only clear the exposed matrix
  // background so boot animation never visibly erases/repaints the artwork.
  d.fillRect(0,0,240,31,TFT_BLACK);
  d.fillRect(0,31,36,60,TFT_BLACK);
  d.fillRect(200,31,40,60,TFT_BLACK);
  d.fillRect(0,91,240,26,TFT_BLACK);

  for(int col=0,x=4;x<238;x+=12,++col){
    int speed=1+((col*13+5)%4); int offset=(col*37 + col*col*7)%151;
    int headY=((int)phase*speed + offset) % 151 - 14;
    for(int trail=0;trail<7;++trail){
      int y=headY-trail*9;
      if(y<0 || y>109) continue;
      // Never paint into the protected foreground rectangle.
      if(x>=36 && x<200 && y>=31 && y<91) continue;
      size_t gi=(size_t)(col*17 + trail*11 + phase*(1+(col%3))) % MATRIX_GLYPH_COUNT;
      d.setTextColor(trail==0?head:(trail<4?mid:dim),TFT_BLACK);
      d.setCursor(x,y);
      d.print(MATRIX_GLYPHS[gi]);
    }
  }
}

void DirkApp::drawSplashArtwork(){
  auto& d=M5Cardputer.Display;
  const bool blue=(team.mode()==TeamMode::Blue);
  const int outW=156, outH=51;
  const int ox=(240-outW)/2, oy=35;
  for(int dy=0; dy<outH; ++dy){
    const int sy=(dy*DIRK_MENU_MARK_H)/outH;
    for(int dx=0; dx<outW; ++dx){
      const int sx=(dx*DIRK_MENU_MARK_W)/outW;
      uint16_t px=pgm_read_word(&DIRK_MENU_MARK[sy*DIRK_MENU_MARK_W+sx]);
      if(px==TFT_BLACK) continue;
      if(blue){
        uint8_t r5=(px>>11)&0x1F, g6=(px>>5)&0x3F, b5=px&0x1F;
        if(r5>8 && r5>(g6>>1)+3 && r5>b5+3){
          uint8_t bv=(uint8_t)min(31,(int)r5+3);
          uint8_t gv=(uint8_t)min(63,(int)r5*2+8);
          px=(uint16_t)((gv<<5)|bv);
        }
      }
      d.drawPixel(ox+dx,oy+dy,px);
    }
  }

  if(!blue){
    const uint16_t steel=0xC618, edge=0xFFFF, shade=0x632C, grip=0x2104, blood=0xF800;
    const int cy=61;
    const int pomX=42, gripX=48, guardX=72, bladeX=78, tipX=190;
    d.fillCircle(pomX,cy,5,grip); d.drawCircle(pomX,cy,5,steel);
    d.fillRoundRect(gripX,cy-5,24,10,2,grip); d.drawRoundRect(gripX,cy-5,24,10,2,steel);
    for(int x=50;x<70;x+=5){ d.drawLine(x,cy-4,x+4,cy+4,0x7BEF); d.drawLine(x+4,cy-4,x,cy+4,0x7BEF); }
    d.fillRect(guardX,cy-14,5,29,steel);
    d.fillTriangle(guardX-4,cy-14,guardX+9,cy-14,guardX+2,cy-9,edge);
    d.fillTriangle(guardX-4,cy+14,guardX+9,cy+14,guardX+2,cy+9,shade);
    d.fillTriangle(bladeX,cy-6,tipX,cy,bladeX,cy+6,steel);
    d.fillTriangle(bladeX,cy,tipX,cy,bladeX,cy+6,shade);
    d.drawLine(bladeX+2,cy-5,tipX-2,cy,edge);
    d.drawLine(bladeX+2,cy+5,tipX-2,cy,0x8410);
    d.drawFastHLine(bladeX+4,cy,tipX-bladeX-8,0xA514);
    d.drawFastHLine(112,cy+1,31,blood);
    d.fillCircle(146,cy+2,1,blood); d.drawFastVLine(146,cy+3,3,blood);
  }
}

void DirkApp::drawSplashBase(){
  auto& d=M5Cardputer.Display;
  if(!splashArtworkDrawn){
    d.fillScreen(TFT_BLACK);
    drawSplashMatrix(splashPhase++);
    drawSplashArtwork();
    splashArtworkDrawn=true;
  }else{
    drawSplashMatrix(splashPhase++);
  }

  uint16_t a=team.mode()==TeamMode::Red?0xF800:0x04BF;
  d.fillRect(0,118,240,17,TFT_BLACK);
  d.drawFastHLine(0,117,240,a);
  d.setTextWrap(false);
  d.setTextSize(1); d.setTextColor(0x7BEF,TFT_BLACK);
  d.setCursor(5,123); d.print("dirk v0.8.0");
}

void DirkApp::splashCheck(const char* name,bool ok,int y){
  (void)y; auto& d=M5Cardputer.Display;
  uint16_t a=team.mode()==TeamMode::Red?0xF800:0x04BF;
  // The artwork remains static; only the exposed matrix background advances.
  // This keeps motion without the full-screen flash seen on real hardware.
  for(uint8_t frame=0;frame<2;frame++){
    drawSplashBase();
    d.fillRect(82,118,158,17,TFT_BLACK);
    d.setTextSize(1); d.setTextColor(0xBDF7,TFT_BLACK); d.setCursor(86,123); d.print(name);
    d.setTextColor(ok?a:0x7BEF,TFT_BLACK); d.setCursor(220,123); d.print(ok?"OK":"--");
    delay(48);
  }
}

bool DirkApp::begin(){
  auto cfg=M5.config();
  M5Cardputer.begin(cfg,false);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextWrap(false);

  system.begin(); events.begin(); capabilities.begin(); team.begin();
  targets.begin(&storage);
  drawSplashBase(); delay(350);

  bool kb=input.begin(); splashCheck("KEYBOARD",kb,0);

  wifi.begin();
  capabilities.mutableCaps().wifi24=(wifi.availability()==Availability::Ready);
  splashCheck("2.4G WIFI",capabilities.get().wifi24,0);

  ble.begin();
  capabilities.mutableCaps().ble=(ble.availability()==Availability::Ready);
  splashCheck("BLE",capabilities.get().ble,0);

  bool sd=storage.begin();
  capabilities.mutableCaps().sd=sd;
  if(sd) targets.loadSaved();
  blueGuard.begin(&storage,&targets);
  splashCheck(sd?"SD WRITE":"SD ERROR",sd,0);

  bool irOk=ir.begin(&storage);
  capabilities.mutableCaps().ir=irOk;
  splashCheck(irOk?"IR TX":"IR --",irOk,0);

  // Opening the GPS UART is not proof of satellite reception. Avoid the
  // old 180 ms boot probe: NMEA typically arrives roughly once per second.
  bool gpsUartStarted=gps.begin();
  gps.update();
  capabilities.mutableCaps().gps=gps.present();
  splashCheck("GPS UART INIT",gpsUartStarted,0);

  bool lr=lora.begin(&storage,&gps);
  capabilities.mutableCaps().lora=lr;
  splashCheck(lr?"LORA SX1262":"LORA --",lr,0);

  // MonsterC5 on Cardputer ADV Grove/HY2.0-4P: RX GPIO1, TX GPIO2.
  // GPS remains isolated on Serial2 GPIO15/GPIO13 via the LoRa/GPS Cap.
  c5.begin(Serial1,1,2,115200);
  janos.begin(&c5,&targets);
  janos.ping();
  uint32_t probeStart=millis();
  while(millis()-probeStart<1100){ janos.update(); gps.update(); input.update(); delay(2); }
  capabilities.mutableCaps().c5=c5.detected();
  capabilities.mutableCaps().wifi5=janos.ready();
  splashCheck(janos.ready()?"JANOS / 5G":"MONSTERC5",janos.ready()||c5.detected(),0);

  splashCheck("DIRK CORE",true,0);
  auto& d=M5Cardputer.Display; uint16_t a=team.mode()==TeamMode::Red?0xF800:0x04BF;
  d.fillRect(82,118,158,17,TFT_BLACK); d.setTextColor(a,TFT_BLACK); d.setCursor(147,123); d.print("system ready"); delay(300);

  wardrive.begin(&storage,&targets,&wifi,&janos,&ble,&gps); captures.begin(&storage,&team);
  ui.begin(&targets,&wifi,&janos,&ble,&gps,&lora,&ir,&blueGuard,&wardrive,&storage,&capabilities,&team,&captures,&nettools,&system,&portal);
  ui.draw(); system.autoConnect();
  return true;
}

void DirkApp::update(){
  M5Cardputer.update(); input.update();
  while(input.available()) ui.input(input.read());
  // FIX8: service S3 Wi-Fi scan phases and publish completed results on the
  // normal Arduino task before wardrive/UI consume the refreshed target set.
  wifi.update();
  wifi.pollScan(targets);
  blueGuard.update(wifi.resultSerial());
  janos.update();
  gps.update();
  time_t gpsUtc=0;
  if(gps.utcEpoch(gpsUtc)){
    GpsFix gf=gps.fix();
    system.updateGpsTime(gpsUtc,gf.valid,gf.lat,gf.lon);
  }
  lora.update(); wardrive.update();
  capabilities.mutableCaps().c5=c5.detected();
  capabilities.mutableCaps().wifi5=janos.ready();
  capabilities.mutableCaps().gps=gps.present();
  capabilities.mutableCaps().lora=lora.ready();
  system.update(); portal.update(); ui.draw(); delay(1);
}
}
