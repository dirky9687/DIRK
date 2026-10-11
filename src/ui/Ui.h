#pragma once
#include <M5Cardputer.h>
#include <WiFi.h>
#include <Preferences.h>
#include "InputManager.h"
#include "UnifiedApView.h"
#include "../targets/TargetManager.h"
#include "../providers/s3/S3WifiProvider.h"
#include "../providers/ble/BleProvider.h"
#include "../providers/gps/GpsProvider.h"
#include "../providers/lora/LoRaProvider.h"
#include "../providers/ir/IrProvider.h"
#include "../blue/BlueGuard.h"
#include "../wardrive/WardriveManager.h"
#include "../providers/c5/JanOSAdapter.h"
#include "../providers/s3/NetworkTools.h"
#include "../storage/StorageManager.h"
#include "../core/CapabilityManager.h"
#include "../core/TeamModeManager.h"
#include "../system/SystemServices.h"
#include "../capture/CaptureManager.h"
#include "../portal/LabPortal.h"
#include "../red/RedAudit.h"
#include "../red/DualBandReview.h"
#include "../red/BlePosture.h"
#include "../red/AttackLearningLab.h"
#include "../assets/DirkMenuMark.h"

namespace dirk {
class Ui {
public:
  void begin(TargetManager* t,S3WifiProvider* w,JanOSAdapter* j,BleProvider* b,GpsProvider* g,LoRaProvider* lr,IrProvider* ip,BlueGuard* bg,WardriveManager* wd,StorageManager* s,
             CapabilityManager* c,TeamModeManager* m,CaptureManager* cap,NetworkTools* nt,SystemServices* sys,LabPortal* lp){
    targets=t; wifi=w; janos=j; ble=b; gps=g; lora=lr; ir=ip; blueGuard=bg; wardrive=wd; storage=s; caps=c; mode=m; captures=cap; nettools=nt; system=sys; portal=lp;
    dirty=true; lastClock=lastAnim=0;
    lastSystemHeaderSerial=system?system->headerSerial():0;
    meshPrefs.begin("dirk-meshct",false);
    loadMeshContacts();
    lastBuddySoundEventSerial=wardrive?wardrive->eventSerial():0;
    lastBuddyLevelSeen=wardrive?wardrive->stats().level:1;
    if(!wdCanvas){
      wdCanvas=new M5Canvas(&M5Cardputer.Display);
      wdCanvas->setColorDepth(16);
      if(!wdCanvas->createSprite(240,117)){ delete wdCanvas; wdCanvas=nullptr; }
    }
    // v0.7.2: composite the animated 111x98 panel off-screen, then transfer
    // one rectangular block. Direct glyph/pixel writes made the rain judder.
    if(!menuCanvas){
      menuCanvas=new M5Canvas(&M5Cardputer.Display);
      menuCanvas->setColorDepth(16);
      if(!menuCanvas->createSprite(111,98)){ delete menuCanvas; menuCanvas=nullptr; }
    }
    buddyBaseVolume=M5Cardputer.Speaker.getVolume();
  }

  void input(const InputEvent& e){
    if(!e.valid()) return;
    if(system->wakeOnInput()){ dirty=true; return; }
    system->touch();

    if(loraTextEntry){ handleLoraText(e); return; }
    if(textEntry){ handleText(e); return; }

    if(screen==0){
      // Retain legacy one-key access to the file categories after placing them
      // under Data Center. Existing operator shortcuts must not be lost.
      int dataShortcut=-1;
      switch(e.action){
        case InputAction::HotCaptures: dataShortcut=0; break;
        case InputAction::HotAudits: dataShortcut=1; break;
        case InputAction::HotSessions: dataShortcut=2; break;
        case InputAction::HotFiles: dataShortcut=3; break;
        default: break;
      }
      int h=hotIndex(e.action);
      if(e.action==InputAction::Character){
        char k=e.ch; if(k>='A'&&k<='Z') k=(char)(k-'A'+'a');
        if(k=='w') h=0; else if(k=='b') h=1; else if(k=='d') h=2; else if(k=='l') h=3;
        else if(k=='i') h=4; else if(k=='c') dataShortcut=0; else if(k=='a') dataShortcut=1;
        else if(k=='s') dataShortcut=2; else if(k=='f') dataShortcut=3; else if(k=='t') h=6;
      }
      if(dataShortcut>=0){ menuSel=5; openDataItem((uint8_t)dataShortcut); }
      else if(h>=0){ menuSel=h; openMenu(); }
      else if(e.action==InputAction::Up) menuSel=(menuSel+6)%7;
      else if(e.action==InputAction::Down) menuSel=(menuSel+1)%7;
      else if(e.action==InputAction::Select) openMenu();
      dirty=true; return;
    }

    if(e.action==InputAction::Back){
      if(screen>=23 && screen<=27) cancelBuddyTone();
      if(screen==8 && filePath!="/dirk"){ browserParent(); return; }
      if(screen==38){ enterScreen(browserReturnScreen); return; }
      if(screen==4 || screen==5 || screen==7 || screen==8){
        if(screen==4 && redHubReturn_){redHubReturn_=false;enterScreen(33);return;}
        enterScreen(6);return;
      }
      if(screen==10){ enterScreen(9); return; }
      if(screen>=22 && screen<=27){ enterScreen(3); return; }
      if(screen==11){ enterScreen(detailReturnScreen_); return; }
      if(screen==28){ enterScreen(30); return; }
      if(screen==29){ enterScreen(34); return; }
      if(screen==33){redHubReturn_=false;enterScreen(1);return;}
      if(screen>=30 && screen<=32){ enterScreen(1); return; }
      if(screen>=34 && screen<=37){ enterScreen(2); return; }
      if(screen==52){ enterScreen(meshProfileReturn); return; }
      if(screen>=41 && screen<=52){ enterScreen(40); return; }
      if(screen==53){ if(blueAlertsReturnSummary_){blueAlertsReturnSummary_=false;enterScreen(67);}else enterScreen(32); return; }
      if(screen==65){redHubReturn_=false; enterScreen(33); return; }
      if(screen==66){ enterScreen(19); return; }
      if(screen==67){ enterScreen(32); return; }
      if(screen==69){ enterScreen(66); return; }
      if(screen==70){ enterScreen(65); return; }
      if(screen==71){ redHubReturn_=false; enterScreen(33); return; }
      if(screen==72){ bleExportConfirm_=false; bleReviewHasSnapshot_=false; enterScreen(37); return; }
      if(screen==73){ redHubReturn_=false; enterScreen(33); return; }
      if(screen>=74 && screen<=77){ enterScreen(73); return; }
      if(screen==68){ enterScreen(15); return; }
      if(screen==64){ if(gps && gps->baud()!=115200u) gps->setBaud(115200u); enterScreen(3); return; }
      if(screen==61 || screen==62 || screen==63){ enterScreen(60); return; }
      if(screen==60){ screen=0; menuSel=4; dirty=true; return; }
      if(screen==40){ screen=0; menuSel=3; dirty=true; return; }
      if(screen==6){ screen=0; menuSel=5; dirty=true; return; }
      if(screen==9){ screen=0; menuSel=6; dirty=true; return; }
      if(screen==3){ screen=0; menuSel=2; dirty=true; return; }
      if(screen==2){ screen=0; menuSel=1; dirty=true; return; }
      if(screen==1){ screen=0; menuSel=0; dirty=true; return; }
      if(screen>=12 && screen<=21){
        if(screen==15 && captures && captures->active()) captures->stop();
        if(screen==20 && portal && portal->running()) portal->stop();
        if(janos && ((screen==12 && !(wardrive && wardrive->running())) || (wifiBand5 && (screen==13 || screen==16)))) janos->stop();
        if(redHubReturn_ && (screen==19 || screen==20 || screen==15 || screen==14 || screen==13)){
          redHubReturn_=false; enterScreen(33);
        }
        else if(screen==19 || screen==20 || screen==15 || screen==14 || screen==21 || screen==32 || screen==33) enterScreen(1);
        else if(screen==17 || screen==18 || screen==13) enterScreen(31);
        else enterScreen(30);
        return;
      }
      screen=0; menuSel=0; dirty=true; return;
    }

    if(screen==1) handleWifiTools(e);
    else if(screen==2) handleBleMenu(e);
    else if(screen==30) handleWifiReconMenu(e);
    else if(screen==31) handleWifiAnalysisMenu(e);
    else if(screen==32) handleWifiBlueMenu(e);
    else if(screen==33) handleWifiRedMenu(e);
    else if(screen==34) handleBle(e);
    else if(screen==35) handleBleAnalysis(e);
    else if(screen==36) handleBleBlue(e);
    else if(screen==37) handleBleRed(e);
    else if(screen==3) handleWardriveMenu(e);
    else if(screen==6) handleDataMenu(e);
    else if(screen==22) handleWardriveSetup(e);
    else if(screen==23) handleWardriveTank(e);
    else if(screen==24) handleWardriveList(e);
    else if(screen==25) handleWardriveMap(e);
    else if(screen==26) handleWardriveStats(e);
    else if(screen==27) handleWardriveProviders(e);
    else if(screen==28) handleSignalTracker(e);
    else if(screen==29) handleBleDetail(e);
    else if(screen==12) handleRecon(e);
    else if(screen==13) handleSpectrum(e);
    else if(screen==14) handleSavedTargets(e);
    else if(screen==15) handleDeauthTool(e);
    else if(screen==16) handleApMonitor(e);
    else if(screen==17) handleProbeMonitor(e);
    else if(screen==18) handleDeauthMonitor(e);
    else if(screen==19) handleNetworkTools(e);
    else if(screen==20) handleBeaconLab(e);
    else if(screen==21) handleDiag5g(e);
    else if(screen==11) handleTargetDetail(e);
    else if(screen==9) handleSettings(e);
    else if(screen==10) handleWifiSettings(e);
    else if(screen==4) handleCaptures(e);
    else if(screen==5) handleAudits(e);
    else if(screen==7) handleSessions(e);
    else if(screen==8) handleFiles(e);
    else if(screen==38) handleBrowserDetail(e);
    else if(screen==40) handleLoRaMenu(e);
    else if(screen==41) handleLoRaChat(e);
    else if(screen==42) handleLoRaNodes(e);
    else if(screen==43) handleLoRaPosition(e);
    else if(screen==44) handleLoRaRange(e);
    else if(screen==45) handleLoRaSettings(e);
    else if(screen==46) handleLoRaDiagnostics(e);
    else if(screen==47) handleMeshtastic(e);
    else if(screen==48) handleMeshNearby(e);
    else if(screen==49) handleMeshContacts(e);
    else if(screen==50) handleMeshChannels(e);
    else if(screen==51) handleMeshNetworks(e);
    else if(screen==52) handleMeshProfile(e);
    else if(screen==53) handleBlueGuardAlerts(e);
    else if(screen==65) handleRedPosture(e);
    else if(screen==66) handleNetworkSurveyResults(e);
    else if(screen==67) handleBlueGuardSummary(e);
    else if(screen==68) handleRedOperationResult(e);
    else if(screen==69) handlePortDetail(e);
    else if(screen==70) handlePostureFinding(e);
    else if(screen==71) handleDualBandReview(e);
    else if(screen==72) handleBlePosture(e);
    else if(screen==73) handleAttackLabMenu(e);
    else if(screen>=74 && screen<=77) handleAttackLabLesson(e);
    else if(screen==60) handleIrMenu(e);
    else if(screen==61) handleIrNec(e);
    else if(screen==62) handleIrFiles(e);
    else if(screen==63) handleIrDiagnostics(e);
    else if(screen==64) handleGpsDiagnostics(e);
    dirty=true;
  }

  bool tickLiveScreen(){
    const uint32_t now=millis();

    // v0.7.15: independent dual-band scan requests + event-driven combined list.
    // Do not touch either provider's scan protocol or blocking/fallback policy.
    if(screen==12){
      if(now-lastUnified24RequestAt_>=3300u){
        lastUnified24RequestAt_=now;
        if(wifi && !wifi->scanning() && !(ble && ble->scanning()))
          wifi->startScanAsync(300,0);
      }
      if(now-lastUnified5RequestAt_>=5500u){
        lastUnified5RequestAt_=now;
        if(janos && !janos->scanning()) janos->startScan5();
      }
      const uint32_t w=wifi?wifi->resultSerial():0;
      const uint32_t c=janos?janos->visualSerial():0;
      if(w!=lastWifiResultSerial || c!=lastC5VisualSerial){
        lastWifiResultSerial=w; lastC5VisualSerial=c;
        unifiedPending_=true;
      }
      if(now-lastUnifiedExpiryAt_>=4000u){lastUnifiedExpiryAt_=now;unifiedPending_=true;}
      if(unifiedPending_ && now-lastUnifiedPaintAt_>=450u){
        unifiedPending_=false; lastUnifiedPaintAt_=now;
        // Snapshot selection by target ID across rebuilds.
        uint32_t selected=0;
        if(const auto* old=unifiedView_.preferred(*targets,unifiedSel_,unifiedPrefer5_)) selected=old->id;
        unifiedView_.build(*targets,now);
        if(selected){
          for(size_t i=0;i<unifiedView_.count();i++){
            const auto* row=unifiedView_.at(i);
            const auto* a=row->band24>=0?targets->at((size_t)row->band24):nullptr;
            const auto* b=row->band5>=0?targets->at((size_t)row->band5):nullptr;
            if((a&&a->id==selected)||(b&&b->id==selected)){unifiedSel_=(uint16_t)i;break;}
          }
        }
        if(unifiedSel_>=unifiedView_.count()) unifiedSel_=unifiedView_.count()?unifiedView_.count()-1:0;
        const uint32_t sig=unifiedView_.signature(*targets);
        if(sig!=lastUnifiedSignature_){lastUnifiedSignature_=sig;return true;}
      }
      return false;
    }

    // FIX8 2.4 GHz live screens: requesting a scan no longer invalidates the TFT.
    // We start scans on schedule, then redraw only when a completed scan publishes
    // data. The AP list (screen 12) is stricter: after its first fresh completion,
    // it redraws only when the provider's meaningful result signature changes.
    if(!wifiBand5 && (screen==12 || screen==13 || screen==16 || screen==28)){
      uint32_t interval=(screen==12)?2500u:3200u;
      if(now-lastWifiRequestAt>=interval) runBandScan(false);

      const uint32_t completed=wifi?wifi->completionSerial():0;
      if(completed!=lastWifiCompletionSerial){
        lastWifiCompletionSerial=completed;
        const uint32_t changed=wifi?wifi->resultSerial():0;
        const bool listChanged=changed!=lastWifiResultSerial;
        if(listChanged) lastWifiResultSerial=changed;

        size_t n=bandTargetCount();
        if(n==0) wifiSel=0; else if(wifiSel>=n) wifiSel=(uint8_t)(n-1);

        if(screen==12){
          if(awaitFreshWifiScan || listChanged){ awaitFreshWifiScan=false; return true; }
          return false;
        }
        if(screen==13){ updateSpectrumFromTargets(); return true; }
        if(screen==16) return true;
        if(screen==28){ selectSignalTarget(); sampleSignalHistory(); return true; }
      }
      return false;
    }

    // C5 scan request and C5 redraw are separate. No unconditional AP-list
    // full-screen repaint on a 500ms timer when the data is unchanged.
    if(wifiBand5 && (screen==12 || screen==13 || screen==16 || screen==28)){
      if(now-lastLiveTick>=600u){
        lastLiveTick=now;
        runBandScan(false);  // adapter enforces one scan + cooldown
        const uint32_t serial=janos?janos->visualSerial():0;
        if(serial!=lastC5VisualSerial){
          lastC5VisualSerial=serial;
          if(screen==13) updateSpectrumFromTargets();
          if(screen==28) sampleSignalHistory();
          return true;
        }
        if(screen==28 && now-lastC5HistoryAt>=3200u){ lastC5HistoryAt=now; sampleSignalHistory(); return true; }
      }
      return false;
    }
    if((screen==17 || screen==18) && now-lastLiveTick>=1500){ lastLiveTick=now; sampleAirNow(); return true; }
    if(screen==21 && now-lastLiveTick>=2500){ lastLiveTick=now; if(janos && !janos->ready()) janos->ping(); return true; }
    if(screen==64 && now-lastLiveTick>=1000u){ lastLiveTick=now; return true; }
    // Tank frames are driven solely by draw(); do not double-invalidate them.
    if(screen==23 && wardrive && wardrive->running()) return false;
    if(screen>=24 && screen<=26 && wardrive && wardrive->running() && now-lastLiveTick>=350){ lastLiveTick=now; return true; }
    // v0.6.1 mesh UI is event driven. Chat/nodes no longer repaint on a fixed
    // 500 ms timer, which was the main source of visible LoRa menu stutter.
    if(screen==41){
      uint32_t serial=lora?lora->messageSerial():0;
      if(serial!=lastLoraMessageSerial){ lastLoraMessageSerial=serial; return true; }
      return false;
    }
    if(screen==42){
      uint32_t serial=lora?lora->nodeSerial():0;
      if(serial!=lastLoraNodeSerial){ lastLoraNodeSerial=serial; return true; }
      return false;
    }
    if(screen==48 || screen==51 || screen==47 || screen==52){
      if((screen==48 || screen==51 || screen==47) && meshAutoDiscover && now-lastMeshScanAt>=5000u){
        lastMeshScanAt=now;
        if(ble && !ble->scanning() && (!wifi || !wifi->scanning())) ble->startScanAsync(2);
      }
      uint32_t ns=lora?lora->nodeSerial():0, bs=ble?ble->resultSerial():0;
      if(ns!=lastLoraNodeSerial || bs!=lastBleMeshSerial){
        lastLoraNodeSerial=ns; lastBleMeshSerial=bs;
        if(screen==52) refreshMeshProfile();
        return true;
      }
      return false;
    }
    // Passive BLE lab has manual scan only. Refresh on completed results;
    // never start another BLE scan from the animation/render loop.
    if(screen==37){
      const uint32_t serial=ble?ble->resultSerial():0;
      if(serial!=lastBleLabSerial_){
        lastBleLabSerial_=serial;
        if(bleRedSel_>=(ble?ble->count():0))bleRedSel_=0;
        return true;
      }
      return false;
    }
    if((screen==32 || screen==67) && now-lastWifiRequestAt>=5000u) runBandScan(false);
    if(screen==68 && redResultPage_==1 && captures && captures->active() && now-lastLiveTick>=1000u){
      lastLiveTick=now;return true;
    }
    if((screen==32 || screen==53 || screen==67) && blueGuard){
      uint32_t bs=blueGuard->compareSerial();
      if(bs!=lastBlueGuardSerial){ lastBlueGuardSerial=bs; return true; }
    }
    if(screen==43 && now-lastLiveTick>=1000u){ lastLiveTick=now; return true; }
    if(screen==46 && now-lastLiveTick>=1200u){ lastLiveTick=now; return true; }
    if((screen==34 || screen==35 || screen==36) && now-lastLiveTick>=2600){
      lastLiveTick=now;
      if(ble && !ble->scanning() && (!wifi || !wifi->scanning())) ble->startScanAsync(1);
      if(bleSel>= (ble?ble->count():0)) bleSel=0;
      return true;
    }
    return false;
  }

  // 0.7.13: Wardrive renders into the long-lived 240x117 general UI sprite.
  // The menu's independent 111x98 panel never changes size. No large sprite
  // is created or destroyed on tank entry/exit (avoids heap churn across runs).

  void draw(){
    if(system) system->setSleepInhibit(wardrive && wardrive->running());
    serviceBuddySound();

    // v0.6.5: header telemetry is still a partial refresh. A clock/battery
    // tick must never invalidate the body of the active screen.
    uint32_t sysHeader=system?system->headerSerial():0;
    const GpsFix curGps=gps?gps->fix():GpsFix{};
    const uint32_t gpsHeaderKey=(curGps.valid?0x10000u:0u)
      | (gps&&gps->present()?0x20000u:0u)
      | (curGps.valid?(uint32_t)curGps.sats:0u);
    const bool gpsHeaderChanged=gpsHeaderKey!=lastGpsHeaderKey_ &&
      (uint32_t)(millis()-lastGpsHeaderAt_)>=850u;
    if(!dirty && (sysHeader!=lastSystemHeaderSerial || gpsHeaderChanged) && !(screen==23 && wardrive && wardrive->running())){
      lastSystemHeaderSerial=sysHeader;
      lastGpsHeaderKey_=gpsHeaderKey; lastGpsHeaderAt_=millis();
      auto& hd=M5Cardputer.Display;
      hd.startWrite(); header(hd,accent()); hd.endWrite();
      return;
    }
    if(dirty){ lastSystemHeaderSerial=sysHeader; lastGpsHeaderKey_=gpsHeaderKey; lastGpsHeaderAt_=millis(); }

    if(dirty){ animPhase=(animPhase+1)%64; lastAnim=millis(); }

    if(!dirty){
      uint32_t now=millis();

      // v0.7.0: keep menu text completely static between input events.
      // Only the right DIRK panel animates, so Matrix motion no longer forces a
      // full 240x117 recomposition every frame. This removes the periodic menu
      // hitch while making the panel animation visibly smoother.
      if(isSectionMenuScreen() && now-lastAnim>=110u){
        lastAnim=now; animPhase=(animPhase+1)%64;
        uint16_t aa=accent();
        if(menuCanvas){
          menuCanvas->fillScreen(TFT_BLACK);
          sectionPanel(*menuCanvas,aa,activeSectionKind(),-126,-19);
          menuCanvas->pushSprite(126,19);
        } else {
          // Low-RAM fallback: retain the original panel draw.
          auto& ad=M5Cardputer.Display;
          ad.startWrite(); sectionPanel(ad,aa,activeSectionKind()); ad.endWrite();
        }
        return;
      }

      // v0.7.8: minimalist Wardrive view. Rank/XP are shown only on the
      // statistics screen; the animation owns y=19..99, the one-line telemetry
      // owns y=100..116. Never repaint one while refreshing the other.
      if(screen==23 && wardrive && wardrive->running() && now-lastLiveTick>=95u){
        if(lastLiveTick && (uint32_t)(now-lastLiveTick)>120u) ++wdLateFrames;
        lastLiveTick=now;
        const uint16_t aa=accent();
        auto& ad=M5Cardputer.Display;
        if(wdCanvas){
          // Reuse the existing UI sprite; transfer only the tank viewport.
          // The tiny Matrix-menu sprite is never reallocated or repurposed.
          wdCanvas->setTextSize(1);
          wdCanvas->setTextWrap(false);
          wdCanvas->fillRect(0,19,240,81,TFT_BLACK);
          wardriveTank(*wdCanvas,aa,true,12);
          ad.setClipRect(0,19,240,81);
          wdCanvas->pushSprite(0,0);
          ad.clearClipRect();
        } else {
          ad.startWrite();
          ad.fillRect(0,19,240,81,TFT_BLACK);
          wardriveTank(ad,aa,true,12);
          ad.endWrite();
        }
        // RF and GPS counters update independently; never repaint the moving
        // tank or animate the footer solely for a telemetry or clock change.
        if(now-lastWdInfoAt>=1000u){
          lastWdInfoAt=now;
          if(wdCanvas){
            drawWardriveCompactStatus(*wdCanvas,aa);
            ad.setClipRect(0,100,240,17);
            wdCanvas->pushSprite(0,0);
            ad.clearClipRect();
          }else{
            ad.startWrite();drawWardriveCompactStatus(ad,aa);ad.endWrite();
          }
        }
        if(sysHeader!=lastSystemHeaderSerial){
          lastSystemHeaderSerial=sysHeader;
          if(wdCanvas){
            header(*wdCanvas,aa);
            ad.setClipRect(0,0,240,19);
            wdCanvas->pushSprite(0,0);
            ad.clearClipRect();
          }else{
            ad.startWrite();header(ad,aa);ad.endWrite();
          }
        }
        return;
      }

      if(tickLiveScreen()) dirty=true;
      if(!dirty) return;
    }

    auto& d=M5Cardputer.Display;
    uint16_t a=accent();
    if(screen==23) bubbleCacheVisible=false;

    // v0.6.5: all normal UI redraws are double-buffered. Previously only
    // wardrive/LoRa used the canvas, so scrolling Wi-Fi/BLE/main menus briefly
    // showed fillScreen(TFT_BLACK) on the physical TFT. The existing 240x117
    // canvas is enough for every header+body screen and avoids extra RAM.
    if(wdCanvas){
      wdCanvas->setTextSize(1);
      wdCanvas->setTextWrap(false);
      wdCanvas->fillScreen(TFT_BLACK);
      renderActive(*wdCanvas,a);
      wdCanvas->pushSprite(0,0);
      footer(d,a);
      dirty=false;
      return;
    }

    d.startWrite();
    d.setTextSize(1); d.setTextWrap(false);
    d.fillRect(0,0,240,117,TFT_BLACK);
    renderActive(d,a);
    d.endWrite();
    footer(d,a);
    dirty=false;
  }

  void renderActive(lgfx::LGFXBase& d,uint16_t a){
    header(d,a);
    switch(screen){
      case 0: home(d,a); break;
      case 1: wifiTools(d,a); break;
      case 2: bleMenu(d,a); break;
      case 30: wifiReconMenu(d,a); break;
      case 31: wifiAnalysisMenu(d,a); break;
      case 32: wifiBlueMenu(d,a); break;
      case 33: wifiRedMenu(d,a); break;
      case 34: bleScreen(d,a); break;
      case 35: bleAnalysis(d,a); break;
      case 36: bleBlue(d,a); break;
      case 37: bleRed(d,a); break;
      case 3: wardriveMenu(d,a); break;
      case 6: dataMenu(d,a); break;
      case 4: capturesScreen(d,a); break;
      case 5: auditsScreen(d,a); break;
      case 7: sessionsScreen(d,a); break;
      case 8: filesScreen(d,a); break;
      case 38: browserDetail(d,a); break;
      case 40: loraMenu(d,a); break;
      case 41: loraChat(d,a); break;
      case 42: loraNodes(d,a); break;
      case 43: loraPosition(d,a); break;
      case 44: loraRange(d,a); break;
      case 45: loraSettings(d,a); break;
      case 46: loraDiagnostics(d,a); break;
      case 47: meshtasticScreen(d,a); break;
      case 48: meshNearbyScreen(d,a); break;
      case 49: meshContactsScreen(d,a); break;
      case 50: meshChannelsScreen(d,a); break;
      case 51: meshNetworksScreen(d,a); break;
      case 52: meshProfileScreen(d,a); break;
      case 53: blueGuardAlerts(d,a); break;
      case 65: redPostureScreen(d,a); break;
      case 66: networkSurveyResults(d,a); break;
      case 67: blueGuardSummary(d,a); break;
      case 68: redOperationResult(d,a); break;
      case 69: portDetail(d,a); break;
      case 70: postureFinding(d,a); break;
      case 71: dualBandReview(d,a); break;
      case 72: blePosture(d,a); break;
      case 73: attackLabMenu(d,a); break;
      case 74: attackLabIdentity(d,a); break;
      case 75: attackLabPmf(d,a); break;
      case 76: attackLabWps(d,a); break;
      case 77: attackLabPlaybook(d,a); break;
      case 60: irMenu(d,a); break;
      case 61: irNec(d,a); break;
      case 62: irFilesScreen(d,a); break;
      case 63: irDiagnostics(d,a); break;
      case 64: gpsDiagnostics(d,a); break;
      case 22: wardriveSetup(d,a); break;
      case 23: wardriveTank(d,a,false,12); break;
      case 24: wardriveList(d,a); break;
      case 25: wardriveMap(d,a); break;
      case 26: wardriveStats(d,a); break;
      case 27: wardriveProviders(d,a); break;
      case 28: signalTracker(d,a); break;
      case 29: bleDetail(d,a); break;
      case 12: wifiRecon(d,a); break;
      case 13: channelSpectrum(d,a); break;
      case 14: savedTargets(d,a); break;
      case 15: deauthTool(d,a); break;
      case 16: apMonitor(d,a); break;
      case 17: probeMonitor(d,a); break;
      case 18: deauthMonitor(d,a); break;
      case 19: networkTools(d,a); break;
      case 20: beaconLab(d,a); break;
      case 21: diag5g(d,a); break;
      case 9: settings(d,a); break;
      case 10: wifiSettings(d,a); break;
      case 11: targetDetail(d,a); break;
      default: moduleScreen(d,a); break;
    }
    if(textEntry) drawTextEntry(d,a);
    if(loraTextEntry) drawLoraTextEntry(d,a);
  }

private:
  TargetManager* targets=nullptr;
  S3WifiProvider* wifi=nullptr;
  JanOSAdapter* janos=nullptr;
  BleProvider* ble=nullptr;
  GpsProvider* gps=nullptr;
  LoRaProvider* lora=nullptr;
  IrProvider* ir=nullptr;
  BlueGuard* blueGuard=nullptr;
  WardriveManager* wardrive=nullptr;
  StorageManager* storage=nullptr;
  CapabilityManager* caps=nullptr;
  TeamModeManager* mode=nullptr;
  CaptureManager* captures=nullptr;
  NetworkTools* nettools=nullptr;
  SystemServices* system=nullptr;
  LabPortal* portal=nullptr;

  uint8_t screen=0,menuSel=0,dataSel=0,settingSel=0,wifiSel=0,targetActionSel=0,reportStatus=0,wifiToolSel=0,bleSel=0,redToolSel=0,labSel=0;
  uint8_t loraMenuSel=0,loraNodeSel=0,loraSettingSel=0,meshtasticSel=0;
  enum MeshProto : uint8_t { MeshDirk=0, MeshMeshtastic=1, MeshCore=2 };
  struct MeshNearbyItem { uint8_t proto=0; char key[32]{}; char name[28]{}; int16_t rssi=-127; float snr=0; uint32_t lastSeen=0; bool hasPosition=false; double lat=0,lon=0; bool favorite=false; };
  struct MeshContact { uint8_t proto=0; char key[32]{}; char name[28]{}; };
  static constexpr uint8_t MAX_MESH_CONTACTS=20;
  Preferences meshPrefs; MeshContact meshContacts[MAX_MESH_CONTACTS]{}; uint8_t meshContactCount=0;
  uint8_t meshNearbySel=0,meshContactSel=0,meshChannelSel=0,meshNetworkSel=0,meshProfileReturn=48;
  uint32_t lastLoraMessageSerial=0,lastLoraNodeSerial=0,lastBleMeshSerial=0,lastMeshScanAt=0,lastBleLabSerial_=0;
  bool meshAutoDiscover=true;
  MeshNearbyItem meshProfile{};
  uint8_t wifiCategorySel=0,wifiSubSel=0,bleCategorySel=0,blueGuardSel=0,blueAlertSel=0;
  uint8_t irMenuSel=0,irFileSel=0; uint16_t irAddress=0x00; uint8_t irCommand=0x00;
  struct IrFileEntry { char name[40]{}; char path[96]{}; }; static constexpr uint8_t MAX_IR_FILES=20; IrFileEntry irFiles[MAX_IR_FILES]{}; uint8_t irFileCount=0; String irUiStatus;
  uint8_t wdMenuSel=0,wdSetupSel=0,wdListSel=0,gpsDiagPage=0; bool wdUse5=true,wdUseBle=true,wdRequireGps=false;
  uint32_t detailTargetId=0; uint8_t detailPage=0,detailReturnScreen_=30;
  uint8_t monitorChannel=6; WifiAirStats airStats{}; bool airSampleValid=false,netScanDone=false,beaconRan=false;
  uint32_t redStatusUntil=0; String redStatus;
  bool redHubReturn_=false,blueAlertsReturnSummary_=false;
  uint8_t networkResultSel_=0,blueSummaryPage_=0,redResultPage_=0,redResultOp_=0;
  uint8_t postureFindingSel_=0;
  uint8_t attackLabSel_=0,attackLabPage_=0,attackWpsSample_=0;
  uint16_t attackTargetSel_=0;
  String attackLabStatus_; uint32_t attackLabStatusUntil_=0;
  uint8_t dualReviewPage_=0,bleReviewFinding_=0;
  uint16_t dualReviewSel_=0,bleRedSel_=0;
  bool bleExportConfirm_=false, bleReviewHasSnapshot_=false;
  BleTarget bleReviewSnapshot_{};
  String redExtraStatus_; uint32_t redExtraStatusUntil_=0;
  uint32_t redResultTargetId_=0;
  String redResultText_,blueGuardNotice_;
  bool wifiBand5=false;
  bool dirty=true,textEntry=false,loraTextEntry=false;
  uint16_t redAuditSel_=0;
  uint8_t redAuditFindingOffset_=0;
  String redAuditMessage_;
  uint32_t redAuditMessageUntil_=0;
  uint32_t lastClock=0,lastAnim=0,lastLiveTick=0,lastWdInfoAt=0,wdLateFrames=0,lastWifiRequestAt=0;
  mutable size_t wdDensity_=0;
  mutable uint32_t wdDensityAt_=0;
  mutable bool wdDensityValid_=false;
  uint32_t lastSystemHeaderSerial=0,lastBlueGuardSerial=0;
  uint32_t lastGpsHeaderKey_=0,lastGpsHeaderAt_=0;
  uint32_t lastWifiCompletionSerial=0,lastWifiResultSerial=0;
  bool awaitFreshWifiScan=false;
  UnifiedApView unifiedView_{};
  uint16_t unifiedSel_=0;
  bool unifiedPrefer5_=false,unifiedPending_=false;
  uint32_t lastUnified24RequestAt_=0,lastUnified5RequestAt_=0,lastUnifiedPaintAt_=0,lastUnifiedExpiryAt_=0,lastUnifiedSignature_=0;
  M5Canvas* wdCanvas=nullptr;
  M5Canvas* menuCanvas=nullptr;
  uint32_t lastC5VisualSerial=0,lastC5HistoryAt=0; uint8_t diag5Page=0;
  uint16_t diag5Sel_=0;
  uint8_t buddyBaseVolume=0; bool buddyAudioTouched=false;
  uint8_t animPhase=0;
  uint8_t spectrum24[14]{};
  uint8_t spectrum5[12]{};
  int8_t signalHistory[36]{}; uint8_t signalHistCount=0, signalHistPos=0;
  uint32_t signalTargetId=0;
  uint32_t reconEpoch=0;
  String pendingSsid,password;
  String loraCompose; bool loraComposeDirect=false; uint32_t loraComposeTarget=0;
  String wdStartStatus;

  struct BrowserEntry { char name[48]{}; char path[128]{}; uint32_t size=0; bool dir=false; };
  static constexpr uint8_t MAX_BROWSER=36;
  BrowserEntry browser[MAX_BROWSER]{}; uint8_t browserCount=0,browserSel=0,browserReturnScreen=0;
  String filePath="/dirk"; String browserTitle; bool browserDeleteConfirm=false;
  String bubbleCacheText; int bubbleCacheX=-1,bubbleCacheY=-1,bubbleCacheW=0,bubbleCacheH=0; bool bubbleCacheVisible=false;
  bool buddyGpsWarningShown_=false;
  uint32_t buddyGpsWarningUntil_=0;
  uint32_t buddyChatterKey=0; uint8_t buddyLastPick=255; String buddyLine1,buddyLine2,buddyMeta;
  uint32_t lastBuddySoundEventSerial=0,buddyToneNextAt=0; uint16_t lastBuddyLevelSeen=1; uint8_t buddyTonePattern=0,buddyToneStep=0;

  uint16_t accent() const { return mode->mode()==TeamMode::Red?0xF800:0x04BF; }
  uint16_t accentDark() const { return mode->mode()==TeamMode::Red?0x7800:0x0210; }

  void distanceTextTo(char* out,size_t n,float meters) const {
    if(!out || !n) return; bool miles=system && system->cfg().distanceMiles;
    if(miles){ float mi=meters/1609.344f; if(mi<0.10f) snprintf(out,n,"%dft",(int)(meters*3.28084f)); else snprintf(out,n,"%.2fmi",mi); return; }
    if(meters<1000.0f) snprintf(out,n,"%dm",(int)meters); else snprintf(out,n,"%.2fkm",meters/1000.0f);
  }
  String distanceText(float meters) const { char b[18]{}; distanceTextTo(b,sizeof(b),meters); return String(b); }

  bool buddyScreenActive() const {
    return screen>=23 && screen<=27;
  }
  void cancelBuddyTone(){
    buddyTonePattern=0; buddyToneStep=0; buddyToneNextAt=0;
    if(buddyAudioTouched){
      M5Cardputer.Speaker.stop();
      M5Cardputer.Speaker.setVolume(buddyBaseVolume);
      buddyAudioTouched=false;
    }
  }
  void queueBuddyTone(uint8_t pattern){
    if(!wardrive || !wardrive->running() || !buddyScreenActive() || !wardrive->soundEnabled()) return;
    // Level-up wins over ordinary discovery pips. Other events may replace a
    // still-pending discovery chirp, but no sound path ever blocks the UI.
    if(buddyTonePattern==3 && pattern!=3) return;
    buddyTonePattern=pattern; buddyToneStep=0; buddyToneNextAt=0;
  }

  void serviceBuddySound(){
    if(!wardrive){ return; }
    const uint32_t serial=wardrive->eventSerial();
    const uint16_t level=wardrive->stats().level;
    if(!wardrive->running() || !wardrive->soundEnabled() || !buddyScreenActive()){
      cancelBuddyTone();
      lastBuddySoundEventSerial=serial; lastBuddyLevelSeen=level; return;
    }
    if(level>lastBuddyLevelSeen){ queueBuddyTone(3); lastBuddyLevelSeen=level; lastBuddySoundEventSerial=serial; }
    else if(serial!=lastBuddySoundEventSerial){
      lastBuddySoundEventSerial=serial;
      switch(wardrive->lastEventKind()){
        case WardriveEventKind::Wifi5: queueBuddyTone(2); break;
        case WardriveEventKind::Milestone: queueBuddyTone(4); break;
        case WardriveEventKind::GpsLost: queueBuddyTone(5); break;
        case WardriveEventKind::GpsBack: queueBuddyTone(6); break;
        default: queueBuddyTone(1); break;
      }
    }
    if(!buddyTonePattern) return;
    uint32_t now=millis(); if(buddyToneNextAt && (int32_t)(now-buddyToneNextAt)<0) return;
    uint16_t freq=0,dur=38,gap=24; uint8_t count=1;
    switch(buddyTonePattern){
      case 1: count=1; freq=1420; dur=32; break;                         // ordinary contact
      case 2: count=2; freq=buddyToneStep?1760:1320; dur=34; break;     // 5 GHz / notable
      case 3: count=4; { static const uint16_t f[]={880,1175,1480,1760}; freq=f[min((uint8_t)3,buddyToneStep)]; dur=48; gap=28; } break;
      case 4: count=3; { static const uint16_t f[]={1100,1450,1960}; freq=f[min((uint8_t)2,buddyToneStep)]; dur=42; } break;
      case 5: count=2; freq=buddyToneStep?520:760; dur=55; gap=35; break;
      case 6: count=2; freq=buddyToneStep?1220:720; dur=45; break;
      default: buddyTonePattern=0; return;
    }
    // Keep the original volume as our reference so toggling views never
    // repeatedly halves it. M5Unified volume range is 0..255.
    if(!buddyAudioTouched){
      buddyBaseVolume=M5Cardputer.Speaker.getVolume();
      M5Cardputer.Speaker.setVolume((uint8_t)(buddyBaseVolume/2u));
      buddyAudioTouched=true;
    }
    M5Cardputer.Speaker.tone(freq,dur);
    buddyToneStep++;
    if(buddyToneStep>=count){ buddyTonePattern=0; buddyToneStep=0; buddyToneNextAt=0; }
    else buddyToneNextAt=now+dur+gap;
  }

  int hotIndex(InputAction a) const {
    switch(a){
      case InputAction::HotWifi: case InputAction::HotNetwork: return 0;
      case InputAction::HotBle: return 1;
      case InputAction::HotWardrive: return 2;
      case InputAction::HotCaptures: case InputAction::HotAudits:
      case InputAction::HotSessions: case InputAction::HotFiles: return 5;
      case InputAction::HotSettings: return 6;
      default: return -1;
    }
  }

  void openDataItem(uint8_t which){
    dataSel=(uint8_t)(which%4);
    menuSel=5;
    switch(dataSel){
      case 0: loadBrowser("/dirk/captures","captures",false); enterScreen(4); break;
      case 1: loadBrowser("/dirk/targets","audits",false,".txt"); enterScreen(5); break;
      case 2: loadBrowser("/dirk/wardrive/sessions","sessions",true); enterScreen(7); break;
      default: filePath="/dirk"; loadBrowser(filePath.c_str(),"files",false); enterScreen(8); break;
    }
  }
  void openMenu(){
    static const uint8_t screens[]={1,2,3,40,60,6,9};
    uint8_t next=screens[menuSel<7?menuSel:0];
    if(next==1){ wifiToolSel=0; wifiCategorySel=0; wifiSubSel=0; wifiSel=0; }
    if(next==2){ bleSel=0; bleCategorySel=0; }
    if(next==3){ wdMenuSel=0; }
    if(next==40){ loraMenuSel=0; }
    if(next==60){ irMenuSel=0; refreshIrFiles(); }
    if(next==6){ dataSel=0; }
    if(next==9) settingSel=0;
    enterScreen(next);
  }

  void enterScreen(uint8_t next){
    if(next<23 || next>27) cancelBuddyTone();
    screen=next;
    lastC5VisualSerial=janos?janos->visualSerial():0;
    lastLiveTick=0; lastWdInfoAt=0; lastWifiRequestAt=0;
    lastWifiCompletionSerial=wifi?wifi->completionSerial():0;
    lastWifiResultSerial=wifi?wifi->resultSerial():0;
    awaitFreshWifiScan=false;
    if(screen==12){
      reconEpoch=millis(); wifiSel=0; unifiedSel_=0; unifiedPrefer5_=false;
      unifiedView_.build(*targets,millis());
      lastUnifiedSignature_=unifiedView_.signature(*targets);
      unifiedPending_=false;lastUnifiedPaintAt_=millis();
      lastUnified24RequestAt_=lastUnified5RequestAt_=lastUnifiedExpiryAt_=millis();
      if(wifi && !wifi->scanning() && !(ble && ble->scanning())) wifi->startScanAsync(300,0);
      if(janos && !janos->scanning()) janos->startScan5();
    }
    else if(screen==13 || screen==16){ runBandScan(false); }
    if(screen==13){ memset(spectrum24,0,sizeof(spectrum24)); memset(spectrum5,0,sizeof(spectrum5)); updateSpectrumFromTargets(); }
    if(screen==17 || screen==18){ sampleAirNow(); }
    if(screen==21 && janos){ janos->ping(); }
    if(screen==32){ wifiBand5=false; if(blueGuard){ blueGuard->compareNow(); lastBlueGuardSerial=blueGuard->compareSerial(); } runBandScan(false); }
    if(screen==62) refreshIrFiles();
    if(screen==2 && ble && ble->count()==0 && (!wifi || !wifi->scanning())){ ble->startScanAsync(1); bleSel=0; }
    if(screen==37) lastBleLabSerial_=ble?ble->resultSerial():0;
    if(screen==28){ signalHistCount=0; signalHistPos=0; signalTargetId=0; runBandScan(false); selectSignalTarget(); sampleSignalHistory(); }
    if(screen==23 && wardrive){
      lastBuddySoundEventSerial=wardrive->eventSerial(); lastBuddyLevelSeen=wardrive->stats().level;
      buddyTonePattern=0; buddyToneStep=0; buddyChatterKey=0; buddyLine1=""; buddyLine2=""; buddyMeta="";
      buddyGpsWarningShown_=false; buddyGpsWarningUntil_=0;
    }
    if(screen>=41 && screen<=52){
      lastLoraMessageSerial=lora?lora->messageSerial():0;
      lastLoraNodeSerial=lora?lora->nodeSerial():0;
      lastBleMeshSerial=ble?ble->resultSerial():0;
    }
    if(screen==48 || screen==51 || screen==47){
      lastMeshScanAt=millis();
      if(meshAutoDiscover && ble && !ble->scanning() && (!wifi || !wifi->scanning())) ble->startScanAsync(2);
    }
    dirty=true;
  }

  template <typename Gfx>
  void drawStatusIcon(Gfx& d,int x,int y,uint8_t kind,bool on,uint16_t a){
    uint16_t c=on?a:0x4208;
    if(kind==0){ // Wi-Fi arcs
      d.drawCircle(x+5,y+5,5,c); d.fillRect(x,y,11,5,TFT_BLACK);
      d.drawCircle(x+5,y+5,3,c); d.fillRect(x+2,y+2,7,3,TFT_BLACK); d.fillCircle(x+5,y+7,1,c);
    } else if(kind==1){ // C5 lightning
      d.drawLine(x+6,y,x+2,y+5,c); d.drawLine(x+2,y+5,x+6,y+5,c); d.drawLine(x+6,y+5,x+3,y+10,c);
    } else if(kind==2){ // BLE rune
      d.drawLine(x+5,y,x+5,y+10,c); d.drawLine(x+5,y,x+9,y+3,c); d.drawLine(x+9,y+3,x+2,y+8,c); d.drawLine(x+2,y+2,x+9,y+7,c);
    } else if(kind==3){ // GPS pin
      d.drawCircle(x+5,y+4,3,c); d.drawLine(x+3,y+6,x+5,y+10,c); d.drawLine(x+7,y+6,x+5,y+10,c); d.fillCircle(x+5,y+4,1,c);
    } else { // SD
      d.drawRect(x+2,y,7,10,c); d.drawLine(x+3,y+2,x+8,y+2,c); d.fillRect(x+4,y+6,3,2,c);
    }
  }

  template <typename Gfx>
  void header(Gfx& d,uint16_t a){
    d.fillRect(0,0,240,18,TFT_BLACK); d.drawFastHLine(0,17,240,a);
    drawStatusIcon(d,3,3,0,caps->get().wifi24,a);
    d.setTextColor(caps->get().wifi24?a:0x4208,TFT_BLACK); d.setCursor(16,5); d.print("2.4");
    drawStatusIcon(d,39,3,1,caps->get().wifi5,a); d.setCursor(51,5); d.print("5g");
    drawStatusIcon(d,67,3,2,caps->get().ble,a); d.setCursor(79,5); d.print("ble");
    const GpsFix liveGps=gps?gps->fix():GpsFix{};
    drawStatusIcon(d,104,3,3,liveGps.valid,a);
    d.setCursor(116,5);
    if(liveGps.valid) d.printf("g%u",(unsigned)liveGps.sats);
    else d.print(gps&&gps->present()?"gps":"g--");
    drawStatusIcon(d,140,3,4,caps->get().sd,a); d.setCursor(152,5); d.print("sd");

    // Compact clock leaves a fixed battery block at the far right.
    String tm=system?system->headerClockText():String("--:--");
    d.setTextColor(0xC618,TFT_BLACK);
    int tx=202-d.textWidth(tm); if(tx<165) tx=165;
    d.setCursor(tx,5); d.print(tm);

    int bp=system?system->batteryPercent():-1;
    const int bx=204,by=5;
    d.drawRect(bx,by,7,7,bp>=0?a:0x4208); d.fillRect(bx+7,by+2,2,3,bp>=0?a:0x4208);
    if(bp>=0){
      int fill=(bp*5+99)/100; if(fill>0) d.fillRect(bx+1,by+1,fill,5,a);
    }
    String bat=bp>=0?String(bp)+"%":String("--%");
    d.setTextColor(bp>=0?TFT_WHITE:0x7BEF,TFT_BLACK); d.setCursor(239-d.textWidth(bat),5); d.print(bat);
  }

  void footer2(lgfx::LGFXBase& d,const char* left,const char* right){
    // Hard-bounded single-line footer. Text wrapping is disabled at the display level.
    d.setTextWrap(false);
    d.setTextColor(0xC618,TFT_BLACK);
    if(left){ d.setCursor(5,123); d.print(left); }
    if(right){ int rx=235-d.textWidth(right); if(rx<126) rx=126; d.setCursor(rx,123); d.print(right); }
  }

  void footer(lgfx::LGFXBase& d,uint16_t a){
    d.drawFastHLine(0,117,240,a); d.fillRect(0,118,240,17,TFT_BLACK);
    if(screen==0) footer2(d,wardrive&&wardrive->running()?"WD BG ACTIVE":"arrows nav","ent open");
    else if(screen==1) footer2(d,"up/dn scroll","ent open");
    else if(screen==2) footer2(d,"up/dn scroll","ent open");
    else if(screen==6) footer2(d,"up/dn select","ent open");
    else if(screen==4||screen==5||screen==7||screen==8) footer2(d,"up/dn browse","ent open");
    else if(screen==38) footer2(d,"tab delete","bk back");
    else if(screen>=30 && screen<=33) footer2(d,"up/dn select","ent open");
    else if(screen==34) footer2(d,"tab scan","ent detail");
    else if(screen==37) footer2(d,"tab scan | ENT inspect","bk BLE");
    else if(screen>=35 && screen<=36) footer2(d,"live passive","bk back");
    else if(screen==3) footer2(d,wardrive&&wardrive->running()?"WD ACTIVE: end run":"ent open/toggle","bk home");
    else if(screen==22) footer2(d,"l/r toggle","ent start");
    else if(screen==23) footer2(d,"l/r view","ent list");
    else if(screen==24) footer2(d,"ent save","l/r view");
    else if(screen==25||screen==26) footer2(d,"l/r view","bk menu");
    else if(screen==27) footer2(d,"sd config","bk menu");
    else if(screen==28) footer2(d,"up/dn target","bk back");
    else if(screen==29) footer2(d,"live passive","bk back");
    else if(screen==12) footer2(d,"TAB both | L/R inspect","ENT detail");
    else if(screen==13) footer2(d,"auto spectrum","bk back");
    else if(screen==14) footer2(d,"ent inspect","bk back");
    else if(screen==15) footer2(d,"ent run/stop","bk back");
    else if(screen==65) footer2(d,"L/R page | TAB CSV","ENT finding");
    else if(screen==70) footer2(d,"L/R findings","bk posture");
    else if(screen==71) footer2(d,"up/dn AP | TAB CSV","L/R page");
    else if(screen==72) footer2(d,"L/R finding | TAB CSV","bk BLE lab");
    else if(screen==73) footer2(d,"up/dn select","ent lesson");
    else if(screen==74) footer2(d,"up/dn AP | TAB CSV","L/R page");
    else if(screen==75) footer2(d,"up/dn AP | L/R page","SIM only");
    else if(screen==76) footer2(d,"up/dn sample","L/R page");
    else if(screen==77) footer2(d,"L/R step","bk labs");
    else if(screen==66) footer2(d,"ENT port | TAB rescan","bk list");
    else if(screen==67) footer2(d,"L/R pages | TAB scan","ENT alerts");
    else if(screen==68) footer2(d,"L/R detail","bk actions");
    else if(screen==69) footer2(d,"read-only info","bk results");
    else if(screen==16) footer2(d,"auto monitor","bk back");
    else if(screen==17||screen==18) footer2(d,"l/r ch auto","bk back");
    else if(screen==19) footer2(d,"ent scan | tab results","bk back");
    else if(screen==20) footer2(d,"ent 5s burst","bk back");
    else if(screen==21) footer2(d,"ping / tab scan","bk back");
    else if(screen==11) footer2(d,"l/r page tab inspect","ent save");
    else if(screen==40) footer2(d,"up/dn scroll","ent open");
    else if(screen==41) footer2(d,"type / ent compose","bk back");
    else if(screen==42) footer2(d,"up/dn nodes","bk back");
    else if(screen==43) footer2(d,"ent send pos","l/r beacon");
    else if(screen==44) footer2(d,"ent range ping","bk back");
    else if(screen==45) footer2(d,"l/r change","bk back");
    else if(screen==46) footer2(d,"live diagnostics","bk back");
    else if(screen==47) footer2(d,"tab ble scan","bk back");
    else if(screen==48) footer2(d,"f friend tab scan","ent profile");
    else if(screen==49) footer2(d,"f remove","ent profile");
    else if(screen==50) footer2(d,"up/dn channel","ent open");
    else if(screen==51) footer2(d,"tab scan","ent action");
    else if(screen==52) footer2(d,"f friend tab scan","ent open");
    else if(screen==53) footer2(d,"up/dn alerts","bk guard");
    else if(screen==60) footer2(d,"up/dn select","ent open");
    else if(screen==61) footer2(d,"l/r command tab addr","ent send");
    else if(screen==62) footer2(d,"tab refresh","ent replay");
    else if(screen==63) footer2(d,"built-in tx","bk ir menu");
    else if(screen==64) footer2(d,"l/r pages; TAB baud","bk wardrive");
    else footer2(d,"ent select","bk back");
  }

  void matrixRain(lgfx::LGFXBase& d,uint16_t a){
    // Matrix-style falling character streams. Phase increases move each head DOWN.
    static const char glyphs[]="0123456789abcdefghijklmnopqrstuvwxyz@#$%&*+=<>?/[]{}";
    static constexpr int G=(int)sizeof(glyphs)-1;
    uint16_t mid=mode->mode()==TeamMode::Red?0xA800:0x04BF;
    uint16_t dim=mode->mode()==TeamMode::Red?0x3000:0x0128;
    for(int col=0,x=118;x<238;x+=9,++col){
      int speed=2+((col*17+3)%5); int offset=(col*29 + col*col*7)%131;
      int headY=20 + (((int)animPhase*speed + offset) % 124);
      for(int trail=0;trail<10;trail++){
        int y=headY-trail*8;
        if(y<21 || y>112) continue;
        int gi=(col*17 + trail*11 + animPhase*(1+(col%3)))%G;
        d.setTextColor(trail==0?a:(trail<4?mid:dim),TFT_BLACK);
        d.setCursor(x,y); d.print(glyphs[gi]);
      }
    }
    d.drawFastVLine(116,21,94,accentDark());
  }

  void menuIcon(lgfx::LGFXBase& d,int kind,int x,int y,uint16_t c,bool selected){
    // v0.6.5: compact pictograms are intentionally silhouette-like so they
    // remain recognizable on the 240x135 panel instead of looking like glyphs.
    uint16_t k=selected?TFT_BLACK:c;
    switch(kind){
      case 0: // Wi-Fi: stacked radio arcs + node
        d.drawLine(x+1,y+4,x+6,y+1,k); d.drawLine(x+6,y+1,x+11,y+4,k);
        d.drawLine(x+3,y+7,x+6,y+5,k); d.drawLine(x+6,y+5,x+9,y+7,k);
        d.fillCircle(x+6,y+10,1,k); break;
      case 1: // Bluetooth rune
        d.drawFastVLine(x+6,y,13,k); d.drawLine(x+6,y,x+11,y+4,k); d.drawLine(x+11,y+4,x+3,y+10,k);
        d.drawLine(x+3,y+3,x+11,y+9,k); d.drawLine(x+11,y+9,x+6,y+13,k); break;
      case 2: // Wardrive: tiny vehicle
        d.drawRect(x+1,y+5,11,5,k); d.drawLine(x+3,y+5,x+5,y+2,k); d.drawLine(x+5,y+2,x+9,y+2,k); d.drawLine(x+9,y+2,x+11,y+5,k);
        d.fillCircle(x+3,y+11,1,k); d.fillCircle(x+10,y+11,1,k); break;
      case 3: // LoRa: mast and RF waves
        d.drawFastVLine(x+6,y+3,10,k); d.fillCircle(x+6,y+2,1,k); d.drawLine(x+4,y+5,x+2,y+3,k); d.drawLine(x+8,y+5,x+10,y+3,k);
        d.drawLine(x+3,y+8,x,y+6,k); d.drawLine(x+9,y+8,x+12,y+6,k); d.drawFastHLine(x+3,y+12,7,k); break;
      case 9: // Infrared: emitter + radiating beam
        d.fillCircle(x+2,y+6,2,k); d.drawFastVLine(x+1,y+3,7,k);
        d.drawLine(x+5,y+4,x+8,y+2,k); d.drawLine(x+5,y+8,x+8,y+10,k); d.drawFastHLine(x+5,y+6,7,k); break;
      case 4: // Captures: packet/crosshair
        d.drawRect(x+1,y+2,11,9,k); d.drawCircle(x+6,y+6,3,k); d.drawFastHLine(x+4,y+6,5,k); d.drawFastVLine(x+6,y+4,5,k); break;
      case 5: // Audits: shield + check
        d.drawLine(x+6,y,x+11,y+2,k); d.drawLine(x+11,y+2,x+10,y+9,k); d.drawLine(x+10,y+9,x+6,y+13,k);
        d.drawLine(x+6,y+13,x+2,y+9,k); d.drawLine(x+2,y+9,x+1,y+2,k); d.drawLine(x+1,y+2,x+6,y,k);
        d.drawLine(x+3,y+6,x+5,y+8,k); d.drawLine(x+5,y+8,x+9,y+4,k); break;
      case 6: // Sessions: log sheet
        d.drawRect(x+2,y+1,9,11,k); d.drawFastHLine(x+4,y+4,5,k); d.drawFastHLine(x+4,y+7,5,k); d.drawFastHLine(x+4,y+10,4,k); break;
      case 7: // Files: folder
        d.drawRect(x+1,y+4,11,8,k); d.drawLine(x+1,y+4,x+1,y+2,k); d.drawLine(x+1,y+2,x+5,y+2,k); d.drawLine(x+5,y+2,x+7,y+4,k); break;
      default: // Settings: gear-like hub
        d.drawCircle(x+6,y+6,4,k); d.fillCircle(x+6,y+6,1,k); d.drawFastHLine(x,y+6,3,k); d.drawFastHLine(x+10,y+6,3,k);
        d.drawFastVLine(x+6,y,3,k); d.drawFastVLine(x+6,y+10,3,k); d.drawLine(x+2,y+2,x+4,y+4,k); d.drawLine(x+8,y+8,x+10,y+10,k); break;
    }
  }

  void drawLargeSectionIcon(lgfx::LGFXBase& d,int kind,int cx,int cy,uint16_t c){
    // Large panel icons use independent geometry rather than scaling the tiny
    // glyphs so Wi-Fi/BLE/wardrive/LoRa are obvious at a glance.
    switch(kind){
      case 0: // Wi-Fi
        d.drawLine(cx-15,cy-8,cx,cy-16,c); d.drawLine(cx,cy-16,cx+15,cy-8,c);
        d.drawLine(cx-10,cy-2,cx,cy-8,c); d.drawLine(cx,cy-8,cx+10,cy-2,c);
        d.drawLine(cx-5,cy+4,cx,cy,c); d.drawLine(cx,cy,cx+5,cy+4,c); d.fillCircle(cx,cy+9,2,c); break;
      case 1: // Bluetooth
        d.drawFastVLine(cx,cy-16,33,c); d.drawLine(cx,cy-16,cx+11,cy-7,c); d.drawLine(cx+11,cy-7,cx-8,cy+8,c);
        d.drawLine(cx-8,cy-8,cx+11,cy+7,c); d.drawLine(cx+11,cy+7,cx,cy+16,c); break;
      case 2: // Wardrive car
        d.drawRoundRect(cx-17,cy-6,34,15,3,c); d.drawLine(cx-11,cy-6,cx-6,cy-14,c); d.drawLine(cx-6,cy-14,cx+7,cy-14,c); d.drawLine(cx+7,cy-14,cx+13,cy-6,c);
        d.fillCircle(cx-10,cy+10,3,c); d.fillCircle(cx+11,cy+10,3,c); d.drawFastHLine(cx-5,cy-2,11,c); break;
      case 3: // LoRa tower
        d.drawFastVLine(cx,cy-14,31,c); d.fillCircle(cx,cy-15,2,c); d.drawLine(cx-3,cy-9,cx-9,cy-14,c); d.drawLine(cx+3,cy-9,cx+9,cy-14,c);
        d.drawLine(cx-6,cy-3,cx-15,cy-10,c); d.drawLine(cx+6,cy-3,cx+15,cy-10,c); d.drawLine(cx-8,cy+15,cx,cy+2,c); d.drawLine(cx+8,cy+15,cx,cy+2,c); d.drawFastHLine(cx-11,cy+15,23,c); break;
      case 9: // Infrared emitter
        d.fillCircle(cx-13,cy,4,c); d.drawFastVLine(cx-16,cy-8,17,c);
        d.drawLine(cx-6,cy-7,cx+2,cy-13,c); d.drawFastHLine(cx-5,cy,18,c); d.drawLine(cx-6,cy+7,cx+2,cy+13,c);
        d.drawLine(cx+7,cy-8,cx+15,cy-13,c); d.drawLine(cx+7,cy+8,cx+15,cy+13,c); break;
      case 4: // Captures
        d.drawRoundRect(cx-15,cy-12,30,24,3,c); d.drawCircle(cx,cy,7,c); d.drawFastHLine(cx-10,cy,21,c); d.drawFastVLine(cx,cy-9,19,c); d.fillCircle(cx,cy,2,c); break;
      case 5: // Audits shield
        d.drawLine(cx,cy-16,cx+14,cy-11,c); d.drawLine(cx+14,cy-11,cx+11,cy+7,c); d.drawLine(cx+11,cy+7,cx,cy+16,c);
        d.drawLine(cx,cy+16,cx-11,cy+7,c); d.drawLine(cx-11,cy+7,cx-14,cy-11,c); d.drawLine(cx-14,cy-11,cx,cy-16,c);
        d.drawLine(cx-7,cy,cx-2,cy+5,c); d.drawLine(cx-2,cy+5,cx+8,cy-6,c); break;
      case 6: // Sessions/log
        d.drawRoundRect(cx-12,cy-15,24,30,3,c); d.drawFastHLine(cx-7,cy-7,14,c); d.drawFastHLine(cx-7,cy,14,c); d.drawFastHLine(cx-7,cy+7,11,c); break;
      case 7: // Files/folder
        d.drawRoundRect(cx-17,cy-9,34,23,3,c); d.drawLine(cx-17,cy-9,cx-17,cy-14,c); d.drawLine(cx-17,cy-14,cx-4,cy-14,c); d.drawLine(cx-4,cy-14,cx+2,cy-9,c); break;
      default: // Settings gear
        d.drawCircle(cx,cy,10,c); d.drawCircle(cx,cy,3,c); d.drawFastHLine(cx-17,cy,8,c); d.drawFastHLine(cx+10,cy,8,c);
        d.drawFastVLine(cx,cy-17,8,c); d.drawFastVLine(cx,cy+10,8,c); d.drawLine(cx-12,cy-12,cx-7,cy-7,c); d.drawLine(cx+7,cy+7,cx+12,cy+12,c);
        d.drawLine(cx+12,cy-12,cx+7,cy-7,c); d.drawLine(cx-7,cy+7,cx-12,cy+12,c); break;
    }
  }

  int menuVisualKind(uint8_t i) const {
    static const uint8_t iconIds[]={0,1,2,3,9,4,8};
    return iconIds[i<7?i:0];
  }

  void home(lgfx::LGFXBase& d,uint16_t a){
    static const char* items[]={"[w]ifi","[b]luetooth","war[d]rive","[l]ora / mesh","[i]nfrared","data center","sys[t]em"};
    d.fillRect(0,19,124,98,TFT_BLACK);
    const int first=menuSel>5?menuSel-5:0;
    for(int row=0;row<6;row++){
      const int i=first+row; if(i>=7) break; const int y=24+row*15; const bool on=i==menuSel;
      if(on) d.fillRoundRect(2,y-2,120,13,2,a);
      menuIcon(d,menuVisualKind((uint8_t)i),5,y-2,a,on);
      d.setTextColor(on?TFT_BLACK:0xC618,on?a:TFT_BLACK); d.setCursor(22,y); d.print(items[i]);
    }
    sectionPanel(d,a,menuVisualKind(menuSel));
  }

  void drawMenuMarkTransparent(lgfx::LGFXBase& d,int ox,int oy){
    const bool blue=(mode->mode()==TeamMode::Blue);
    for(int y=0;y<DIRK_MENU_MARK_H;y++){
      for(int x=0;x<DIRK_MENU_MARK_W;x++){
        uint16_t px=pgm_read_word(&DIRK_MENU_MARK[y*DIRK_MENU_MARK_W+x]);
        if(px==TFT_BLACK) continue;
        if(blue){
          uint8_t r5=(px>>11)&0x1F, g6=(px>>5)&0x3F, b5=px&0x1F;
          if(r5>8 && r5>(g6>>1)+3 && r5>b5+3){
            uint8_t bv=r5; uint8_t gv=(uint8_t)min(63,(int)r5*2);
            px=(uint16_t)((gv<<5)|bv);
          }
        }
        d.drawPixel(ox+x,oy+y,px);
      }
    }
  }

  void drawPanelMatrix(lgfx::LGFXBase& d,uint16_t a,int dx=0,int dy=0){
    const int x0=130+dx, y0=57+dy, w=103, h=58;
    d.fillRect(x0,y0,w,h,TFT_BLACK);
    static const char glyphs[]="0123456789abcdefghijklmnopqrstuvwxyz@#$%&*+=<>?/[]{}";
    static constexpr int G=(int)sizeof(glyphs)-1;
    uint16_t mid=mode->mode()==TeamMode::Red?0xA800:0x04BF;
    uint16_t dim=mode->mode()==TeamMode::Red?0x3000:0x0128;
    for(int col=0,x=x0+2;x<x0+w-4;x+=9,++col){
      int speed=2+((col*11+5)%4); int offset=(col*23 + col*col*3)%(h+31);
      int headY=y0 + (((int)animPhase*speed + offset) % (h+31));
      for(int trail=0;trail<7;trail++){
        int y=headY-trail*8;
        if(y<y0 || y>y0+h-7) continue;
        int gi=(col*19 + trail*13 + animPhase*(1+(col%2)))%G;
        d.setTextColor(trail==0?a:(trail<3?mid:dim),TFT_BLACK);
        d.setCursor(x,y); d.print(glyphs[gi]);
      }
    }
  }

  void sectionPanel(lgfx::LGFXBase& d,uint16_t a,int kind,int dx=0,int dy=0){
    d.fillRect(126+dx,19+dy,111,98,TFT_BLACK);
    d.drawRoundRect(126+dx,20+dy,110,96,4,accentDark());
    d.fillRect(128+dx,22+dy,106,34,TFT_BLACK);
    drawMenuMarkTransparent(d,130+dx,22+dy);
    drawPanelMatrix(d,a,dx,dy);

    const int cx=181+dx, cy=87+dy;
    uint8_t p=(uint8_t)(animPhase%12);
    uint8_t pulse=(uint8_t)(20 + (p<6?p:11-p));
    d.drawCircle(cx,cy,pulse,accentDark());
    if((animPhase&1)==0) d.drawCircle(cx,cy,pulse+2,a);
    drawLargeSectionIcon(d,kind,cx,cy,a);
  }

  bool isSectionMenuScreen() const {
    return screen==0 || screen==1 || screen==2 || screen==3 || screen==6 || screen==40 || screen==60;
  }
  int activeSectionKind() const {
    if(screen==0) return menuVisualKind(menuSel);
    if(screen==1) return 0;
    if(screen==2) return 1;
    if(screen==3) return 2;
    if(screen==6) return 4;
    if(screen==40) return 3;
    if(screen==60) return 9;
    return 8;
  }

  bool targetMatchesBand(const WifiTarget* t) const {
    if(!t) return false;
    return wifiBand5 ? (t->band==Band::Band5) : (t->band==Band::Band24);
  }
  size_t bandTargetCount() const {
    size_t n=0; for(size_t i=0;i<targets->count();++i) if(targetMatchesBand(targets->at(i))) n++; return n;
  }
  int bandIndexAt(size_t ordinal) const {
    size_t n=0; for(size_t i=0;i<targets->count();++i){ auto* t=targets->at(i); if(targetMatchesBand(t)){ if(n==ordinal) return (int)i; n++; } } return -1;
  }
  bool reconTargetVisible(const WifiTarget* t) const {
    if(!targetMatchesBand(t) || !t->lastSeen) return false;
    const uint32_t age=(uint32_t)(millis()-t->lastSeen);
    // A C5 scan spans several seconds: don't hide its most recent valid APs
    // just because the operator re-entered recon between 5GHz sweeps.
    if(wifiBand5) return age<45000u;
    return t->lastSeen>=reconEpoch && age<12000u;
  }
  size_t reconTargetCount() const {
    size_t n=0; for(size_t i=0;i<targets->count();++i) if(reconTargetVisible(targets->at(i))) n++; return n;
  }
  int reconIndexAt(size_t ordinal) const {
    size_t n=0; for(size_t i=0;i<targets->count();++i){ auto* t=targets->at(i); if(reconTargetVisible(t)){ if(n==ordinal) return (int)i; n++; } } return -1;
  }

  bool runBandScan(bool resetIndex=true){
    if(resetIndex) wifiSel=0;
    if(wifiBand5){
      if(janos && !janos->scanning()){
        // Retain previously seen C5 APs across scan gaps. Clearing here made
        // networks blink out when the module missed even one scan cycle.
        janos->startScan5();
        return true;
      }
      return false;
    }

    // FIX8: never overlap BLE with the shared S3 Wi-Fi radio. startScanAsync()
    // only arms the millis()-based reset/scan state machine; it does not block.
    if(!wifi || wifi->scanning() || (ble && ble->scanning())) return false;
    if(wifi->startScanAsync(300,0)){
      lastWifiRequestAt=millis();
      return true;
    }
    return false;
  }

  void updateSpectrumFromTargets(){
    if(wifiBand5){
      static const uint8_t chans[]={36,40,44,48,52,56,60,64,100,104,108,112};
      for(int k=0;k<12;k++){
        int strongest=-127,cnt=0; for(size_t i=0;i<targets->count();++i){ auto*t=targets->at(i); if(t&&t->band==Band::Band5&&t->channel==chans[k]){cnt++; if(t->rssi>strongest) strongest=t->rssi; }}
        uint8_t targetH = cnt ? (uint8_t)constrain(strongest+100,6,52) : 0;
        spectrum5[k] = targetH > spectrum5[k] ? targetH : (spectrum5[k] > 2 ? spectrum5[k]-2 : 0);
      }
    } else {
      for(int c=0;c<14;c++){
        int strongest=-127,cnt=0; for(size_t i=0;i<targets->count();++i){ auto*t=targets->at(i); if(t&&t->band==Band::Band24&&t->channel==c+1){cnt++; if(t->rssi>strongest) strongest=t->rssi; }}
        uint8_t targetH = cnt ? (uint8_t)constrain(strongest+100,6,54) : 0;
        spectrum24[c] = targetH > spectrum24[c] ? targetH : (spectrum24[c] > 2 ? spectrum24[c]-2 : 0);
      }
    }
  }

  uint8_t wifiCategoryCount() const { return 6; }
  uint8_t bleCategoryCount() const { return mode->mode()==TeamMode::Red?4:3; }
  const char* wifiCategoryName(uint8_t i) const {
    static const char* blue[]={"recon","analysis","network tools","defensive monitor","saved targets","5g / c5 diagnostics"};
    static const char* red[]={"recon","analysis","network survey","blue guard","red operations","5g / c5 diagnostics"};
    return mode->mode()==TeamMode::Red?red[i]:blue[i];
  }
  void handleWifiTools(const InputEvent& e){
    uint8_t N=wifiCategoryCount();
    if(e.action==InputAction::Up) wifiCategorySel=(wifiCategorySel+N-1)%N;
    else if(e.action==InputAction::Down) wifiCategorySel=(wifiCategorySel+1)%N;
    else if(e.action==InputAction::Select){
      wifiSubSel=0; const bool red=mode->mode()==TeamMode::Red;
      if(wifiCategorySel==0) enterScreen(30);
      else if(wifiCategorySel==1) enterScreen(31);
      else if(wifiCategorySel==2){ enterScreen(19); }
      else if(wifiCategorySel==3) enterScreen(32);
      else if(red && wifiCategorySel==4){ redHubReturn_=false; enterScreen(33); }
      else if(!red && wifiCategorySel==4){ wifiSel=0; enterScreen(14); }
      else { wifiBand5=true; enterScreen(21); }
    }
  }
  void wifiTools(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("wifi // toolkit");
    static const char* blueShort[]={"recon","analysis","network tools","defensive","saved targets","5g diagnostics"};
    static const char* redShort[]={"recon","analysis","net survey","blue guard","red operations","5g diagnostics"};
    uint8_t N=wifiCategoryCount(); uint8_t first=wifiCategorySel>5?wifiCategorySel-5:0;
    for(uint8_t row=0;row<6 && first+row<N;row++){
      uint8_t i=first+row; int y=40+row*13; bool on=i==wifiCategorySel;
      if(on)d.fillRoundRect(4,y-2,116,12,2,a);
      d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(9,y); d.print(mode->mode()==TeamMode::Red?redShort[i]:blueShort[i]);
    }
    if(first>0){d.setTextColor(a,TFT_BLACK);d.setCursor(114,27);d.print("^");}
    if(first+6<N){d.setTextColor(a,TFT_BLACK);d.setCursor(114,108);d.print("v");}
    sectionPanel(d,a,0);
  }

  void handleWifiReconMenu(const InputEvent& e){
    static constexpr uint8_t N=5;
    // Show a band choice only for the single-band monitor/tracker tools.
    // The dual scanner, saved targets and 2.4-only probe monitor do not use it.
    if((wifiSubSel==2 || wifiSubSel==3) &&
       (e.action==InputAction::Left || e.action==InputAction::Right)){
      wifiBand5=!wifiBand5; wifiSel=0; return;
    }
    if(e.action==InputAction::Up) wifiSubSel=(wifiSubSel+N-1)%N; else if(e.action==InputAction::Down) wifiSubSel=(wifiSubSel+1)%N;
    else if(e.action==InputAction::Select){
      if(wifiSubSel==0) enterScreen(12); else if(wifiSubSel==1){wifiSel=0;enterScreen(14);} else if(wifiSubSel==2) enterScreen(16); else if(wifiSubSel==3) enterScreen(28); else { airSampleValid=false; enterScreen(17); }
    }
  }
  void wifiReconMenu(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("wifi // recon");
    if(wifiSubSel==0 || wifiSubSel==1 || wifiSubSel==4){
      d.setTextColor(a,TFT_BLACK); d.setCursor(126,23);
      d.print(wifiSubSel==0?"AUTO 2.4+5":(wifiSubSel==1?"ALL BANDS":"2.4 ONLY"));
    } else {
      d.setCursor(102,23); d.setTextColor(!wifiBand5?TFT_BLACK:0x7BEF,!wifiBand5?a:TFT_BLACK); d.print(" 2.4 "); d.setCursor(143,23); d.setTextColor(wifiBand5?TFT_BLACK:0x7BEF,wifiBand5?a:TFT_BLACK); d.print(" 5g ");
    }
    static const char* rows[]={"dual-band AP scanner","saved targets","ap monitor","signal tracker","probe monitor"};
    for(int i=0;i<5;i++){ int y=43+i*14; bool on=i==wifiSubSel; if(on)d.fillRoundRect(4,y-2,228,12,2,a); d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(9,y); d.print(rows[i]); }
  }

  void handleWifiAnalysisMenu(const InputEvent& e){
    static constexpr uint8_t N=4;
    if(e.action==InputAction::Left || e.action==InputAction::Right){ wifiBand5=!wifiBand5; wifiSel=0; return; }
    if(e.action==InputAction::Up) wifiSubSel=(wifiSubSel+N-1)%N; else if(e.action==InputAction::Down) wifiSubSel=(wifiSubSel+1)%N;
    else if(e.action==InputAction::Select){ if(wifiSubSel==0) enterScreen(13); else if(wifiSubSel==1){airSampleValid=false;enterScreen(18);} else if(wifiSubSel==2) enterScreen(32); else {wifiBand5=true;enterScreen(21);} }
  }
  void wifiAnalysisMenu(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("wifi // analysis");
    static const char* rows[]={"channel spectrum","deauth/disassoc monitor","security posture","5g diagnostics"};
    for(int i=0;i<4;i++){ int y=43+i*16; bool on=i==wifiSubSel; if(on)d.fillRoundRect(4,y-2,228,13,2,a); d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(9,y); d.print(rows[i]); }
  }

  void handleWifiBlueMenu(const InputEvent& e){
    static constexpr uint8_t N=4;
    if(e.action==InputAction::Up) blueGuardSel=(blueGuardSel+N-1)%N;
    else if(e.action==InputAction::Down) blueGuardSel=(blueGuardSel+1)%N;
    else if(e.action==InputAction::Select){
      if(blueGuardSel==0){
        runBandScan(false);
        if(blueGuard) blueGuard->compareNow();
        blueGuardNotice_="Scan requested; updating on results";
        blueSummaryPage_=0; enterScreen(67);
      }
      else if(blueGuardSel==1){
        const bool ok=blueGuard&&blueGuard->saveBaseline();
        blueGuardNotice_=ok?"Baseline saved to SD":"Baseline save failed";
        blueSummaryPage_=0; enterScreen(67);
      }
      else if(blueGuardSel==2){ if(blueGuard) blueGuard->compareNow(); blueAlertSel=0; blueAlertsReturnSummary_=false; enterScreen(53); }
      else { airSampleValid=false; sampleAirNow(); blueGuardNotice_="Passive air sample refreshed";blueSummaryPage_=2;enterScreen(67); }
    }
    else if(e.action==InputAction::Tab){ runBandScan(false); }
  }
  void wifiBlueMenu(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("blue // guard tools");
    d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,37);
    d.printf("Baseline: %s",blueGuard&&blueGuard->hasBaseline()?"saved":"not saved");
    static const char* rows[]={"scan + results","save current baseline","view change alerts","passive air sample"};
    for(uint8_t i=0;i<4;i++){
      int y=54+i*14;const bool on=i==blueGuardSel;
      if(on)d.fillRoundRect(4,y-2,230,12,2,a);
      d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK);
      d.setCursor(9,y);d.print(rows[i]);
    }
  }
  void handleBlueGuardSummary(const InputEvent& e){
    if(e.action==InputAction::Left) blueSummaryPage_=(blueSummaryPage_+2)%3;
    else if(e.action==InputAction::Right) blueSummaryPage_=(blueSummaryPage_+1)%3;
    else if(e.action==InputAction::Tab){runBandScan(false);if(blueGuard)blueGuard->compareNow();blueGuardNotice_="Scan requested; updating on results";}
    else if(e.action==InputAction::Select){blueAlertSel=0;blueAlertsReturnSummary_=true;enterScreen(53);}
  }
  void blueGuardSummary(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.printf("blue guard // results %u/3",(unsigned)(blueSummaryPage_+1));
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    const bool baseline=blueGuard&&blueGuard->hasBaseline();
    if(blueSummaryPage_==0){
      d.setCursor(7,39);d.printf("Baseline APs    %u",baseline?(unsigned)blueGuard->baselineCount():0u);
      d.setCursor(7,54);d.printf("New observations %u",blueGuard?(unsigned)blueGuard->newCount():0u);
      d.setCursor(7,69);d.printf("Missing          %u",blueGuard?(unsigned)blueGuard->missingCount():0u);
      d.setCursor(7,84);d.printf("Changed          %u",blueGuard?(unsigned)blueGuard->changedCount():0u);
      d.setCursor(7,99);d.printf("Known SSID/new BSSID %u",blueGuard?(unsigned)blueGuard->cloneCount():0u);
    }else if(blueSummaryPage_==1){
      size_t open=0,hidden=0,pmf=0,total=0;
      const uint32_t now=millis();
      for(size_t i=0;targets && i<targets->count();i++){
        const WifiTarget* t=targets->at(i);
        if(!t||!t->lastSeen||(uint32_t)(now-t->lastSeen)>30000u)continue;
        ++total; if((wifi_auth_mode_t)t->auth==WIFI_AUTH_OPEN)++open;
        if(!t->ssid[0])++hidden; if(t->pmfCapable||t->pmfRequired)++pmf;
      }
      d.setCursor(7,39);d.printf("Recent AP records %u",(unsigned)total);
      d.setCursor(7,54);d.printf("Open networks    %u",(unsigned)open);
      d.setCursor(7,69);d.printf("Hidden SSIDs     %u",(unsigned)hidden);
      d.setCursor(7,84);d.printf("PMF observed     %u",(unsigned)pmf);
      d.setCursor(7,99);d.print("Observations, not vulnerabilities");
    }else{
      d.setCursor(7,39);d.printf("Deauth seen %lu",(unsigned long)airStats.deauth);
      d.setCursor(7,54);d.printf("Disassoc    %lu",(unsigned long)airStats.disassoc);
      d.setCursor(7,69);d.printf("Alert rows  %u",blueGuard?(unsigned)blueGuard->alertCount():0u);
      d.setCursor(7,84);d.print(baseline?"Comparison uses saved baseline":"Save a baseline to compare");
      d.setCursor(7,99);d.print("TAB refresh | ENT inspect alerts");
    }
    if(blueGuardNotice_.length()){
      d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(7,111);
      d.print(blueGuardNotice_.substring(0,37));
    }
  }

  void handleBlueGuardAlerts(const InputEvent& e){
    size_t n=blueGuard?blueGuard->alertCount():0; if(e.action==InputAction::Up && n) blueAlertSel=(blueAlertSel+n-1)%n; else if(e.action==InputAction::Down && n) blueAlertSel=(blueAlertSel+1)%n; else if(e.action==InputAction::Tab && blueGuard) blueGuard->compareNow();
  }
  void blueGuardAlerts(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("blue guard // changes");
    size_t n=blueGuard?blueGuard->alertCount():0; d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(204,23); d.printf("%u",(unsigned)n);
    if(!blueGuard || !blueGuard->hasBaseline()){ d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,48); d.print("no baseline saved"); d.setCursor(8,62); d.print("save one from blue guard first"); return; }
    if(!n){ d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,48); d.print("no changes against baseline"); return; }
    size_t first=blueAlertSel>4?blueAlertSel-4:0;
    for(size_t r=0;r<6 && first+r<n;r++){ const BlueGuard::Alert* al=blueGuard->alertAt(first+r); if(!al)continue; int y=39+r*13; bool on=(first+r)==blueAlertSel; if(on)d.fillRoundRect(3,y-2,234,12,2,a); d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(6,y);
      String label=String(blueGuard->kindText(al->kind))+" "+String(al->ssid); if(label.length()>31)label=label.substring(0,31); d.print(label);
    }
  }

  void handleWifiRedMenu(const InputEvent& e){
    static constexpr uint8_t N=9;
    if(e.action==InputAction::Up) wifiSubSel=(wifiSubSel+N-1)%N;
    else if(e.action==InputAction::Down) wifiSubSel=(wifiSubSel+1)%N;
    else if(e.action==InputAction::Select){
      // Preserve Back to this hub even when an operation has its own native screen.
      redHubReturn_=true;
      switch(wifiSubSel){
        case 0: redAuditSel_=0; redAuditFindingOffset_=0; redAuditMessage_=""; enterScreen(65); break;
        case 1: dualReviewSel_=0; dualReviewPage_=0; enterScreen(71); break;
        case 2: attackLabSel_=0; enterScreen(73); break;
        case 3: redToolSel=0; enterScreen(15); break;
        case 4: enterScreen(19); break;
        case 5: wifiBand5=false; enterScreen(13); break;
        case 6: beaconRan=false; labSel=0; enterScreen(20); break;
        case 7: wifiSel=0; enterScreen(14); break;
        default: loadBrowser("/dirk/captures","captures",false); browserReturnScreen=33; enterScreen(4); break;
      }
    }
  }
  void wifiRedMenu(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("red // operations");
    if(mode->mode()!=TeamMode::Red){
      d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(8,48);d.print("locked: enable red mode in settings");return;
    }
    static const char* rows[]={
      "target posture / CSV", "dual-band policy review", "attack learning labs",
      "authorized validation", "gateway service survey", "RF channel spectrum",
      "beacon / portal lab", "saved targets", "capture evidence"
    };
    const uint8_t first=wifiSubSel>5?wifiSubSel-5:0;
    for(uint8_t i=first;i<9 && i<first+6;i++){
      const int y=39+(i-first)*13; const bool on=i==wifiSubSel;
      if(on)d.fillRoundRect(4,y-2,231,12,2,a);
      d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK);
      d.setCursor(9,y);d.print(rows[i]);
    }

  }

  // v0.8.0 classroom: passive observations + offline exercises only.
  // All screens 73-77 are independent of scan and capture state machines.
  void handleAttackLabMenu(const InputEvent& e){
    if(e.action==InputAction::Up) attackLabSel_=(attackLabSel_+3)%4;
    else if(e.action==InputAction::Down) attackLabSel_=(attackLabSel_+1)%4;
    else if(e.action==InputAction::Select && mode && mode->mode()==TeamMode::Red){
      attackLabPage_=0; attackTargetSel_=0; attackWpsSample_=0;
      attackLabStatus_="";
      enterScreen((uint8_t)(74+attackLabSel_));
    }
  }
  void attackLabMenu(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("red // learning labs");
    if(!mode || mode->mode()!=TeamMode::Red){
      d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,48);d.print("enable red mode first");return;
    }
    static const char* rows[]={
      "SSID impersonation clues", "PMF spoofing tabletop",
      "WPS PIN checksum demo", "authorized test playbook"
    };
    for(uint8_t i=0;i<4;i++){
      int y=43+i*16;bool sel=i==attackLabSel_;
      if(sel)d.fillRoundRect(4,y-2,232,14,2,a);
      d.setTextColor(sel?TFT_BLACK:TFT_WHITE,sel?a:TFT_BLACK);
      d.setCursor(8,y);d.print(rows[i]);
    }
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(6,108);d.print("PASSIVE / OFFLINE / NO FRAME TX");
  }
  void handleAttackLabLesson(const InputEvent& e){
    if(screen==74 || screen==75){
      size_t n=redAuthorizedCount();
      if(e.action==InputAction::Up && n)attackTargetSel_=(attackTargetSel_+n-1)%n;
      if(e.action==InputAction::Down && n)attackTargetSel_=(attackTargetSel_+1)%n;
    } else if(screen==76){
      if(e.action==InputAction::Up) attackWpsSample_=(attackWpsSample_+2)%3;
      if(e.action==InputAction::Down) attackWpsSample_=(attackWpsSample_+1)%3;
    }
    const uint8_t pages=screen==77?4:3;
    if(e.action==InputAction::Left)attackLabPage_=(attackLabPage_+pages-1)%pages;
    if(e.action==InputAction::Right)attackLabPage_=(attackLabPage_+1)%pages;
    if(screen==74 && e.action==InputAction::Tab){
      const size_t n=redAuthorizedCount();
      const WifiTarget* t=n?redAuthorizedAt(attackTargetSel_%n):nullptr;
      bool ok=t && exportIdentityLesson(*t);
      attackLabStatus_=ok?"Report saved to SD":"SD write / auth failed";
      attackLabStatusUntil_=millis()+2500u;
    }
  }
  const WifiTarget* attackLabTarget() const {
    const size_t n=redAuthorizedCount();
    return n?redAuthorizedAt(attackTargetSel_%n):nullptr;
  }
  LookalikeClues collectLookalikes(const WifiTarget& t) const {
    LookalikeClues r{};
    if(!targets) return r;
    for(size_t i=0;i<targets->count();++i){
      const WifiTarget* p=targets->at(i);
      if(p)learnFromPeer(r,t,*p);
    }
    return r;
  }
  bool exportIdentityLesson(const WifiTarget& t){
    if(!storage || !storage->writable() || !t.saved || !t.authorized ||
       !mode || !mode->permitsActive(t)) return false;
    const LookalikeClues x=collectLookalikes(t);
    char path[100];
    snprintf(path,sizeof(path),"/dirk/reports/identity_lab_%02X%02X%02X%02X%02X%02X.csv",
      t.bssid.b[0],t.bssid.b[1],t.bssid.b[2],t.bssid.b[3],t.bssid.b[4],t.bssid.b[5]);
    String r="ssid,band,peers_same_ssid,peers_same_band,peers_other_band,auth_differences_24,channel_differences,conclusion\n";
    r+=auditCsvCell(t.ssid)+","+String(t.band==Band::Band5?"5":"2.4")+","+
      String(x.sameName)+","+String(x.sameBand)+","+String(x.otherBand)+","+
      String(x.sameBandAuthDiff)+","+String(x.sameBandChannelDiff)+","+
      auditCsvCell("Observation only; matching SSIDs or differences do not prove impersonation")+"\n";
    return storage->overwrite(path,(const uint8_t*)r.c_str(),r.length());
  }
  void attackLabIdentity(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("lab // ssid impersonation");
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(201,23);d.printf("%u/3",(unsigned)(attackLabPage_+1));
    const WifiTarget* t=attackLabTarget();
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    if(!t){
      d.setCursor(7,47);d.print("Save + authorize your own AP");
      d.setCursor(7,60);d.print("under Wi-Fi > saved targets.");return;
    }
    const LookalikeClues x=collectLookalikes(*t);
    d.setCursor(6,38);d.printf("%-24.24s %s",t->ssid,t->band==Band::Band5?"5G":"2G");
    if(attackLabPage_==0){
      d.setCursor(6,53);d.printf("Other BSSIDs same name: %u",(unsigned)x.sameName);
      d.setCursor(6,67);d.printf("Same band: %u  other: %u",(unsigned)x.sameBand,(unsigned)x.otherBand);
      d.setCursor(6,83);d.print("Exercise: compare router BSSIDs");
      d.setCursor(6,99);d.print("A shared SSID is often normal.");
    }else if(attackLabPage_==1){
      d.setCursor(6,53);d.printf("2.4 auth differences: %u",(unsigned)x.sameBandAuthDiff);
      d.setCursor(6,67);d.printf("Same-band channel diffs: %u",(unsigned)x.sameBandChannelDiff);
      d.setCursor(6,83);d.print("5G auth/PMF: not verified");
      d.setCursor(6,99);d.print("Flag for review, NOT an attack.");
    }else{
      d.setCursor(6,52);d.print("Confirm AP MACs in router UI.");
      d.setCursor(6,66);d.print("Compare actual security policy.");
      d.setCursor(6,80);d.print("Check for authorized extenders.");
      d.setCursor(6,94);d.print("TAB: save observation to SD.");
    }
    if(attackLabStatus_.length() && (int32_t)(attackLabStatusUntil_-millis())>0){
      d.fillRect(4,103,232,12,TFT_BLACK);
      d.setTextColor(a,TFT_BLACK);d.setCursor(6,104);d.print(attackLabStatus_.substring(0,37));
    }
  }
  void attackLabPmf(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("lab // PMF frame defense");
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(201,23);d.printf("%u/3",(unsigned)(attackLabPage_+1));
    const WifiTarget* t=attackLabTarget();
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    if(!t){d.setCursor(6,46);d.print("Save + authorize your AP first.");return;}
    const PmfLesson p=observedPmf(*t);
    d.setCursor(6,39);d.printf("%-23.23s  %s",t->ssid,t->band==Band::Band5?"5G":"2G");
    d.setTextColor(a,TFT_BLACK);d.setCursor(6,53);d.printf("Observed PMF: %s",pmfLessonName(p));
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    if(attackLabPage_==0){
      d.setCursor(6,69);d.print("Scenario: forged disconnect");
      d.setCursor(6,84);d.print("Would a protected client");
      d.setCursor(6,98);d.print("reject the unprotected frame?");
    }else if(attackLabPage_==1){
      d.setCursor(6,69);
      if(p==PmfLesson::Required)d.print("Protected sessions: reject");
      else if(p==PmfLesson::Optional)d.print("Client negotiation matters");
      else d.print("Insufficient RSN evidence");
      d.setCursor(6,84);d.print("SIMULATION: NO RF sent");
      d.setCursor(6,98);d.print("No observed client response.");
    }else{
      d.setCursor(6,68);d.print("Use a test client + router");
      d.setCursor(6,82);d.print("logs for real validation.");
      d.setCursor(6,96);d.print("Optional != proven vulnerable.");
    }
  }
  void attackLabWps(lgfx::LGFXBase& d,uint16_t a){
    static constexpr uint32_t samples[]={1234567u,2468135u,7301492u};
    const uint32_t pin7=samples[attackWpsSample_%3];
    const uint8_t checksum=demoWpsChecksum(pin7);
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("lab // WPS checksum");
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(201,23);d.printf("%u/3",(unsigned)(attackLabPage_+1));
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    if(attackLabPage_==0){
      d.setCursor(6,42);d.print("SYNTHETIC EXAMPLE (OFFLINE)");
      d.setCursor(6,58);d.printf("Seven digits: %07lu",(unsigned long)pin7);
      d.setCursor(6,72);d.printf("Check digit : %u",(unsigned)checksum);
      d.setCursor(6,87);d.printf("Eight digits: %08lu",(unsigned long)demoWpsPin(pin7));
      d.setCursor(6,103);d.print("No AP PIN read or attempted");
    }else if(attackLabPage_==1){
      d.setCursor(6,44);d.print("The 8th digit checks the PIN.");
      d.setCursor(6,59);d.print("It is not independent entropy.");
      d.setCursor(6,75);d.print("Learning: configuration risk");
      d.setCursor(6,90);d.print("does not imply compromise.");
    }else{
      d.setCursor(6,44);d.print("Protect your router:");
      d.setCursor(6,59);d.print("Disable WPS PIN if possible.");
      d.setCursor(6,74);d.print("Enforce rate limits / lockout.");
      d.setCursor(6,89);d.print("Prefer WPA2-AES / WPA3.");
      d.setCursor(6,103);d.print("No WPS attempts performed.");
    }
  }
  void attackLabPlaybook(lgfx::LGFXBase& d,uint16_t a){
    static const char* titles[]={"1. Authorize","2. Baseline","3. Exercise","4. Verify"};
    static const char* l1[]={"Save each permitted BSSID.","Log AP channels and policy.","Choose a bounded lab test.","Compare settings after test."};
    static const char* l2[]={"Record owner + time window.","Note scan quality and limits.","Use controlled test devices.","Review AP + client logs."};
    static const char* l3[]={"Never assume SSID = owner.","Keep 5G unknowns as unknown.","Avoid disrupting bystanders.","Document unknowns, not claims."};
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("lab // assessment playbook");
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(201,23);d.printf("%u/4",(unsigned)(attackLabPage_+1));
    d.setTextColor(a,TFT_BLACK);d.setCursor(7,42);d.print(titles[attackLabPage_]);
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    d.setCursor(7,59);d.print(l1[attackLabPage_]);
    d.setCursor(7,76);d.print(l2[attackLabPage_]);
    d.setCursor(7,93);d.print(l3[attackLabPage_]);
  }

  // Red Team: strictly passive, scoped to operator-authorized saved BSSIDs.
  // No new radio commands, disruptive frames, authentication attempts or probes.
  size_t redAuthorizedCount() const {
    size_t n=0;
    if(!targets) return 0;
    for(size_t i=0;i<targets->count();++i){
      const WifiTarget* t=targets->at(i);
      if(t && t->saved && t->authorized) n++;
    }
    return n;
  }
  const WifiTarget* redAuthorizedAt(size_t ordinal) const {
    if(!targets) return nullptr;
    for(size_t i=0;i<targets->count();++i){
      const WifiTarget* t=targets->at(i);
      if(t && t->saved && t->authorized){
        if(ordinal==0) return t;
        --ordinal;
      }
    }
    return nullptr;
  }
  static String auditCsvCell(const char* p){
    String s="\"";
    if(p) for(const char* q=p;*q;++q){
      if(*q=='\"') s+="\"\"";
      else if(*q=='\r' || *q=='\n') s+=' ';
      else s+=*q;
    }
    s+='\"';
    return s;
  }
  bool exportRedPosture(const WifiTarget& t){
    if(!storage || !storage->writable() || !mode || !mode->permitsActive(t)) return false;
    const RedPosture a=assessRedPosture(t);
    char path[96]{};
    snprintf(path,sizeof(path),"/dirk/reports/red_posture_%02X%02X%02X%02X%02X%02X.csv",
      t.bssid.b[0],t.bssid.b[1],t.bssid.b[2],t.bssid.b[3],t.bssid.b[4],t.bssid.b[5]);
    digitalWrite(StorageManager::LORA_CS,HIGH);
    if(SD.exists(path)) SD.remove(path);
    File f=SD.open(path,FILE_WRITE);
    if(!f) return false;
    f.println("ssid,bssid,band,channel,rssi_dbm,auth_code,severity,finding,recommendation");
    char mac[18]{};
    snprintf(mac,sizeof(mac),"%02X:%02X:%02X:%02X:%02X:%02X",
      t.bssid.b[0],t.bssid.b[1],t.bssid.b[2],t.bssid.b[3],t.bssid.b[4],t.bssid.b[5]);
    for(uint8_t i=0;i<a.count;++i){
      const RedFinding& x=a.findings[i];
      String line=auditCsvCell(t.ssid)+","+auditCsvCell(mac)+","+
        String(t.band==Band::Band5?"5":"2.4")+","+String(t.channel)+","+
        String(t.rssi)+","+String(t.auth)+","+
        auditCsvCell(redSeverityName(x.severity))+","+auditCsvCell(x.title)+","+
        auditCsvCell(x.advice)+"\n";
      if(f.print(line)!=line.length()){ f.close(); SD.remove(path); return false; }
    }
    f.flush();f.close();
    File verify=SD.open(path,FILE_READ);
    const bool ok=verify && verify.size()>80;
    if(verify) verify.close();
    return ok;
  }
  void handleRedPosture(const InputEvent& e){
    const size_t n=redAuthorizedCount();
    if(e.action==InputAction::Up && n){ redAuditSel_=(uint16_t)((redAuditSel_+n-1)%n); redAuditFindingOffset_=0; }
    else if(e.action==InputAction::Down && n){ redAuditSel_=(uint16_t)((redAuditSel_+1)%n); redAuditFindingOffset_=0; }
    else if((e.action==InputAction::Left || e.action==InputAction::Right) && n){
      const WifiTarget* t=redAuthorizedAt(redAuditSel_%n);
      if(t){const RedPosture p=assessRedPosture(*t);
        const uint8_t pages=(p.count+2)/3;
        if(pages){ const uint8_t page=(uint8_t)(redAuditFindingOffset_/3);
          redAuditFindingOffset_=(uint8_t)(3*((page+(e.action==InputAction::Right?1:pages-1))%pages));
        }
      }
    }
    else if(e.action==InputAction::Select && n){
      const WifiTarget* t=redAuthorizedAt(redAuditSel_%n);
      if(t){
        const RedPosture p=assessRedPosture(*t);
        if(p.count){postureFindingSel_=redAuditFindingOffset_<p.count?redAuditFindingOffset_:0;enterScreen(70);}
      }
    }
    else if(e.action==InputAction::Tab && n){
      const WifiTarget* t=redAuthorizedAt(redAuditSel_%n);
      redAuditMessage_=t&&exportRedPosture(*t)?"CSV saved to /dirk/reports":"SD report failed";
      redAuditMessageUntil_=millis()+3200u;
    }
  }
  void redPostureScreen(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("red // target posture");
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    const size_t n=redAuthorizedCount();
    if(!n){
      d.setCursor(7,43);d.print("No authorized saved target.");
      d.setCursor(7,58);d.print("Save & authorize your own AP");
      d.setCursor(7,72);d.print("from Wi-Fi > saved targets.");return;
    }
    if(redAuditSel_>=n) redAuditSel_=0;
    const WifiTarget* t=redAuthorizedAt(redAuditSel_);
    if(!t)return;
    d.setCursor(5,37);d.printf("%u/%u %-29.29s",(unsigned)(redAuditSel_+1),(unsigned)n,t->ssid[0]?t->ssid:"<hidden>");
    d.setCursor(5,49);d.printf("%s CH%u RSSI %d auth:%u",t->band==Band::Band5?"5 GHz":"2.4 GHz",(unsigned)t->channel,(int)t->rssi,(unsigned)t->auth);
    const RedPosture posture=assessRedPosture(*t);
    // Fixed footer starts at y=117. Keep all content in the 18..116 body.
    // The footer already advertises L/R and CSV, so move page counts into
    // the title instead of drawing an overlapping help line at y=111.
    d.setTextColor(a,TFT_BLACK);
    d.setCursor(195,23);
    d.printf("%u/%u",(unsigned)(1+redAuditFindingOffset_/3),(unsigned)((posture.count+2)/3));
    for(uint8_t i=0;i<3 && (int)(redAuditFindingOffset_+i)<posture.count;i++){
      const RedFinding& f=posture.findings[redAuditFindingOffset_+i];
      const int y=62+(int)i*14; // last row y=90, clear of status/footer
      d.setCursor(5,y);
      d.setTextColor(f.severity==RedSeverity::High?0xF800:(f.severity==RedSeverity::Review?0xFFE0:a),TFT_BLACK);
      d.printf("%s: %-29.29s",redSeverityName(f.severity),f.title);
    }
    if(redAuditMessage_.length() && (int32_t)(redAuditMessageUntil_-millis())>0){
      d.setTextColor(0x7BEF,TFT_BLACK);
      d.setCursor(5,105); // transient CSV save result, above footer line
      d.print(redAuditMessage_.substring(0,35));
    }
  }

  void handlePostureFinding(const InputEvent& e){
    const size_t n=redAuthorizedCount();
    if(!n)return;
    const WifiTarget* t=redAuthorizedAt(redAuditSel_%n);if(!t)return;
    const RedPosture p=assessRedPosture(*t);if(!p.count)return;
    if(e.action==InputAction::Left)postureFindingSel_=(postureFindingSel_+p.count-1)%p.count;
    else if(e.action==InputAction::Right)postureFindingSel_=(postureFindingSel_+1)%p.count;
  }
  void postureFinding(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("red // finding detail");
    const size_t n=redAuthorizedCount();if(!n)return;
    const WifiTarget* t=redAuthorizedAt(redAuditSel_%n);if(!t)return;
    const RedPosture p=assessRedPosture(*t);if(!p.count)return;
    if(postureFindingSel_>=p.count)postureFindingSel_=0;
    const RedFinding& f=p.findings[postureFindingSel_];
    d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,38);
    d.printf("%.29s",t->ssid[0]?t->ssid:"<hidden>");
    d.setCursor(7,53);d.printf("Finding %u/%u | %s",(unsigned)(postureFindingSel_+1),(unsigned)p.count,redSeverityName(f.severity));
    d.setTextColor(a,TFT_BLACK);d.setCursor(7,71);d.print(String(f.title).substring(0,37));
    d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,89);d.print("Recommendation:");
    d.setCursor(7,103);d.print(String(f.advice).substring(0,37));
  }

  void selectSignalTarget(){
    size_t n=bandTargetCount();
    if(!n){ signalTargetId=0; return; }
    if(wifiSel>=n) wifiSel=0;
    int idx=reconIndexAt(wifiSel); auto* t=idx>=0?targets->at(idx):nullptr;
    signalTargetId=t?t->id:0;
  }

  void sampleSignalHistory(){
    if(!signalTargetId) selectSignalTarget();
    auto* t=targets->byId(signalTargetId);
    if(!t || !targetMatchesBand(t)){ selectSignalTarget(); t=targets->byId(signalTargetId); }
    if(!t) return;
    signalHistory[signalHistPos]=t->rssi;
    signalHistPos=(uint8_t)((signalHistPos+1)%36);
    if(signalHistCount<36) signalHistCount++;
  }

  void handleSignalTracker(const InputEvent& e){
    size_t n=bandTargetCount();
    if(e.action==InputAction::Up && n){ wifiSel=(wifiSel+n-1)%n; signalTargetId=0; signalHistCount=0; selectSignalTarget(); }
    else if(e.action==InputAction::Down && n){ wifiSel=(wifiSel+1)%n; signalTargetId=0; signalHistCount=0; selectSignalTarget(); }
    else if(e.action==InputAction::Tab){ runBandScan(false); selectSignalTarget(); sampleSignalHistory(); }
  }

  void signalTracker(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print(wifiBand5?"signal tracker // 5g":"signal tracker // 2.4g");
    auto* t=signalTargetId?targets->byId(signalTargetId):nullptr;
    if(!t){ d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,48); d.print("waiting for visible ap..."); return; }
    d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(7,38); d.printf("%-20.20s %ddbm",t->ssid[0]?t->ssid:"<hidden>",t->rssi);
    d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(7,51); d.printf("ch%u  %s  live history",t->channel,t->band==Band::Band5?"5g":"2.4");
    const int x0=8,y0=64,w=224,h=42;
    d.drawRect(x0,y0,w,h,accentDark());
    for(int db=-90;db<=-50;db+=20){ int yy=y0+h-2-(db+100)*(h-4)/60; d.drawFastHLine(x0+1,yy,w-2,accentDark()); }
    if(signalHistCount>1){
      for(int i=1;i<signalHistCount;i++){
        int ia=(signalHistPos+36-signalHistCount+i-1)%36, ib=(signalHistPos+36-signalHistCount+i)%36;
        int xa=x0+2+(i-1)*(w-5)/35, xb=x0+2+i*(w-5)/35;
        int ya=y0+h-3-constrain((int)(signalHistory[ia]+100),0,60)*(h-6)/60;
        int yb=y0+h-3-constrain((int)(signalHistory[ib]+100),0,60)*(h-6)/60;
        d.drawLine(xa,ya,xb,yb,a);
      }
    }
  }

  size_t savedCount() const {
    size_t n=0; for(size_t i=0;i<targets->count();++i){ auto* t=targets->at(i); if(t && t->saved) n++; } return n;
  }

  int savedIndexAt(size_t ordinal) const {
    size_t n=0; for(size_t i=0;i<targets->count();++i){ auto* t=targets->at(i); if(t && t->saved){ if(n==ordinal) return (int)i; n++; } } return -1;
  }

  void handleSavedTargets(const InputEvent& e){
    size_t n=savedCount();
    if(e.action==InputAction::Up && n) wifiSel=(wifiSel+n-1)%n;
    else if(e.action==InputAction::Down && n) wifiSel=(wifiSel+1)%n;
    else if(e.action==InputAction::Select && n){ int idx=savedIndexAt(wifiSel); if(idx>=0){ auto* t=targets->at(idx); if(t){ detailTargetId=t->id; detailPage=0; reportStatus=0;detailReturnScreen_=14; screen=11; } } }
  }

  void savedTargets(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("saved targets");
    size_t n=savedCount();
    if(!n){ d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(8,48); d.print("no saved aps in this session."); d.setCursor(8,62); d.print("inspect an ap and save to sd."); return; }
    size_t start=wifiSel>4?wifiSel-4:0;
    for(size_t r=0;r<6 && start+r<n;r++){
      int idx=savedIndexAt(start+r); auto* t=idx>=0?targets->at(idx):nullptr; if(!t) continue;
      int y=39+r*13; bool on=(start+r)==wifiSel; if(on)d.fillRoundRect(3,y-2,234,12,2,a);
      d.setTextColor(TFT_WHITE,on?a:TFT_BLACK); d.setCursor(6,y);
      d.printf("* %-17.17s CH%2u %4d",t->ssid[0]?t->ssid:"<hidden>",t->channel,t->rssi);
    }
  }

  void handleSpectrum(const InputEvent& e){
    if(e.action==InputAction::Tab){ runBandScan(false); updateSpectrumFromTargets(); }
  }

  void channelSpectrum(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print(wifiBand5?"channel spectrum // 5g":"channel spectrum // 2.4g");
    if(wifiBand5){
      static const uint8_t chans[]={36,40,44,48,52,56,60,64,100,104,108,112};
      int base=105,maxH=52;
      for(int k=0;k<12;k++){
        int x=4+k*19,h=spectrum5[k]?spectrum5[k]:1; d.drawRect(x,base-maxH,14,maxH,accentDark()); if(spectrum5[k]) d.fillRect(x+2,base-h,10,h,a);
        d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(x,108); d.print(chans[k]);
      }
      d.setCursor(5,39); d.print(janos&&janos->scanning()?"JanOS scan running...":"continuous passive refresh");
      d.setCursor(5,48); d.printf("parsed:%u  raw:%lu",(unsigned)janos->parsed(),(unsigned long)janos->rawLines());
      return;
    }
    int base=105, maxH=54;
    for(int c=0;c<14;c++){
      int x=5+c*16; int h=spectrum24[c]?spectrum24[c]:1;
      d.drawRect(x,base-maxH,12,maxH,accentDark()); if(spectrum24[c]) d.fillRect(x+2,base-h,8,h,a);
      d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(x+(c<9?4:1),108); d.print(c+1);
    }
    d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(5,39); d.print("continuous passive analyzer with decay");
  }

  void handleDeauthTool(const InputEvent& e){
    static constexpr uint8_t N=5; WifiTarget* t=firstAuthorizedTarget();
    if(e.action==InputAction::Up) redToolSel=(redToolSel+N-1)%N;
    else if(e.action==InputAction::Down) redToolSel=(redToolSel+1)%N;
    else if(e.action==InputAction::Select && t && captures && mode->mode()==TeamMode::Red){
      if(redToolSel==0){ bool ok=captures->deauthBurst(*t,3); redStatus=ok?"3 TX requests accepted":"TX rejected or unavailable"; }
      else if(redToolSel==1){ bool ok=captures->disassocBurst(*t,3); redStatus=ok?"3 TX requests accepted":"TX rejected or unavailable"; }
      else if(redToolSel==2){ if(captures->active()){captures->stop();redStatus="capture stopped / saved";} else redStatus=captures->start(*t,CaptureKind::Handshake)?"handshake/eapol capture running":"capture start failed"; }
      else if(redToolSel==3){ if(captures->active()){captures->stop();redStatus="capture stopped / saved";} else redStatus=captures->start(*t,CaptureKind::Pmkid)?"passive pmkid watch running":"capture start failed"; }
      else { if(captures->active()){captures->stop();redStatus="capture stopped / saved";} else redStatus=captures->start(*t,CaptureKind::Raw)?"target raw pcap running":"capture start failed"; }
      redStatusUntil=millis()+2200;
      redResultText_=redStatus;
      redResultOp_=redToolSel;
      redResultTargetId_=t->id;
      redResultPage_=0;
      enterScreen(68);
    }
  }

  WifiTarget* firstAuthorizedTarget(){
    for(size_t i=0;i<targets->count();++i){ auto* t=targets->at(i); if(t && t->saved && t->authorized) return t; } return nullptr;
  }

  void deauthTool(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("wifi // authorized tests");
    d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(7,36);
    if(mode->mode()!=TeamMode::Red){ d.print("locked: enable red mode in settings"); return; }
    WifiTarget* t=firstAuthorizedTarget();
    if(!t){ d.print("locked: no authorized saved target"); d.setCursor(7,49); d.print("save + authorize your own ap first"); return; }
    d.printf("%-20.20s ch%u",t->ssid[0]?t->ssid:"<hidden>",t->channel);
    static const char* ops[]={"deauth validation x3","disassoc validation x3","handshake / eapol","pmkid passive watch","raw target pcap"};
    uint8_t first=redToolSel>3?redToolSel-3:0;
    for(uint8_t row=0;row<4 && first+row<5;row++){uint8_t i=first+row;int y=51+row*13;bool on=i==redToolSel;if(on)d.fillRoundRect(4,y-2,228,12,2,a);d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK);d.setCursor(9,y);d.print(ops[i]);}
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(7,109);
    d.print("ENT opens operation result page");
  }

  void handleRedOperationResult(const InputEvent& e){
    if(e.action==InputAction::Left || e.action==InputAction::Right) redResultPage_^=1u;
  }
  void redOperationResult(lgfx::LGFXBase& d,uint16_t a){
    static const char* ops[]={"deauth validation","disassoc validation","handshake / eapol","pmkid passive watch","raw target pcap"};
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.printf("red // action result %u/2",(unsigned)(redResultPage_+1));
    d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,39);
    d.printf("Operation: %s",ops[redResultOp_<5?redResultOp_:0]);
    const WifiTarget* t=targets?targets->byId(redResultTargetId_):nullptr;
    if(t){
      d.setCursor(7,53);d.printf("Target: %-28.28s",t->ssid[0]?t->ssid:"<hidden>");
      d.setCursor(7,67);d.printf("%s | ch%u | %d dBm",t->band==Band::Band5?"5 GHz":"2.4 GHz",(unsigned)t->channel,(int)t->rssi);
    }else{d.setCursor(7,56);d.print("Selected target no longer listed");}
    if(redResultPage_==0){
      d.setCursor(7,84);d.print("Result:");
      d.setCursor(7,99);d.print(redResultText_.substring(0,37));
    }else{
      // Result page 2 is compacted to avoid the fixed y=117 footer.
      d.setCursor(7,79);d.printf("Capture: %s",captures&&captures->active()?"running":"stopped");
      d.setCursor(7,92);d.printf("Packets:%lu EAPOL:%lu PMKID:%lu",
        captures?(unsigned long)captures->packets():0ul,
        captures?(unsigned long)captures->eapolFrames():0ul,
        captures?(unsigned long)captures->pmkids():0ul);
      d.setCursor(7,105);d.print("TX status != client effect");
    }
  }

  // Unified scanner: 2.4 GHz and 5 GHz observations share a display-only
  // SSID group. Each original BSSID remains in TargetManager for Wardrive/XP.
  void handleRecon(const InputEvent& e){
    const size_t n=unifiedView_.count();
    if(e.action==InputAction::Up && n) unifiedSel_=(uint16_t)((unifiedSel_+n-1)%n);
    else if(e.action==InputAction::Down && n) unifiedSel_=(uint16_t)((unifiedSel_+1)%n);
    else if(e.action==InputAction::Left) unifiedPrefer5_=false;
    else if(e.action==InputAction::Right) unifiedPrefer5_=true;
    else if(e.action==InputAction::Select && n){
      const auto* t=unifiedView_.preferred(*targets,unifiedSel_,unifiedPrefer5_);
      if(!t) return;
      detailTargetId=t->id; detailPage=0;
      // The unified view returns a const observation, but inspect() updates
      // the live target. Resolve its stable ID to a writable TargetManager
      // entry; never cast away const or inspect a C5/5 GHz observation.
      if(t->band==Band::Band24 && wifi){
        if(auto* live=targets->byId(t->id)) wifi->inspect(*live,700);
      }
      targetActionSel=0;reportStatus=0;detailReturnScreen_=12;screen=11;
    }
    else if(e.action==InputAction::Tab){
      if(wifi && !wifi->scanning() && !(ble && ble->scanning())) wifi->startScanAsync(300,0);
      if(janos && !janos->scanning()) janos->startScan5();
      lastUnified24RequestAt_=lastUnified5RequestAt_=millis();
    }
    else if(e.action==InputAction::Save && n){
      const auto* t=unifiedView_.preferred(*targets,unifiedSel_,unifiedPrefer5_);
      if(t) targets->save(t->id,t->authorized);
    }
  }

  static const char* shortAuth(uint8_t auth){
    switch((wifi_auth_mode_t)auth){
      case WIFI_AUTH_OPEN:return "OPEN";
      case WIFI_AUTH_WEP:return "WEP";
      case WIFI_AUTH_WPA_PSK:return "WPA";
      case WIFI_AUTH_WPA2_PSK:return "WPA2";
      case WIFI_AUTH_WPA_WPA2_PSK:return "WPA1/2";
      case WIFI_AUTH_WPA2_ENTERPRISE:return "ENTER";
#ifdef WIFI_AUTH_WPA3_PSK
      case WIFI_AUTH_WPA3_PSK:return "WPA3";
#endif
#ifdef WIFI_AUTH_WPA2_WPA3_PSK
      case WIFI_AUTH_WPA2_WPA3_PSK:return "WPA2/3";
#endif
      default:return "SEC";
    }
  }

  void wifiRecon(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("ap scanner // dual band");
    const size_t n=unifiedView_.count();
    d.setTextColor(0xA514,TFT_BLACK);d.setCursor(5,35);
    d.printf("2.4:%u  5:%u   %u SSIDs",(unsigned)unifiedView_.count24(),
      (unsigned)unifiedView_.count5(),(unsigned)n);
    if(!n){
      d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,62);
      if(wifi && wifi->scanning()) d.print("scanning 2.4 GHz...");
      else if(janos && janos->scanning()) d.print("scanning 5 GHz...");
      else d.print("waiting for AP discoveries");
      d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(7,77);d.print("Tab: scan both radios");
      return;
    }
    size_t first=(unifiedSel_>3)?unifiedSel_-3:0;
    if(first+4>n && n>4)first=n-4;
    for(size_t row=0;row<4 && first+row<n;row++){
      const auto* group=unifiedView_.at(first+row);
      if(!group)continue;
      const auto* p24=group->band24>=0?targets->at((size_t)group->band24):nullptr;
      const auto* p5=group->band5>=0?targets->at((size_t)group->band5):nullptr;
      const auto* rep=targets->at((size_t)group->primary);
      if(!rep)continue;
      const int y=47+(int)row*18;const bool on=(first+row)==unifiedSel_;
      if(on)d.fillRoundRect(2,y-2,236,18,2,accentDark());
      d.setTextColor(TFT_WHITE,on?accentDark():TFT_BLACK);
      d.setCursor(5,y);
      d.printf("%-26.26s",UnifiedApView::actualName(rep->ssid)?rep->ssid:"<hidden AP>");
      // Red means observed on that band, grey means not observed. These are
      // measured statuses, not bands claimed by the router or inferred from SSID.
      d.fillRoundRect(171,y-1,33,9,2,p24?0xC800:0x39E7);
      d.fillRoundRect(207,y-1,29,9,2,p5?0xC800:0x39E7);
      d.setTextColor(TFT_WHITE);d.setCursor(175,y);d.print("2.4");
      d.setCursor(215,y);d.print("5");
      if(on){
        const bool inspect5=p5 && (unifiedPrefer5_ || !p24);
        d.drawRoundRect(inspect5?206:170,y-2,inspect5?31:35,11,2,TFT_WHITE);
      }
      d.setTextColor(0xBDF7,on?accentDark():TFT_BLACK);
      d.setCursor(7,y+9);
      if(p24 && p5)d.printf("24:%d 5:%d dBm",(int)p24->rssi,(int)p5->rssi);
      else if(p24)d.printf("24:%d dBm",(int)p24->rssi);
      else if(p5)d.printf("5:%d dBm",(int)p5->rssi);
      const char* auth=shortAuth((p5?p5:p24)->auth);
      if(p24 && p5 && p24->auth!=p5->auth)auth="MIX";
      d.setTextColor(0xBDF7,on?accentDark():TFT_BLACK);
      d.setCursor(194,y+9);d.print(auth);
    }
  }

  const char* authName(uint8_t auth) const {
    switch((wifi_auth_mode_t)auth){
      case WIFI_AUTH_OPEN: return "open";
      case WIFI_AUTH_WEP: return "WEP";
      case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
      case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
      case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
      case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
#ifdef WIFI_AUTH_WPA3_PSK
      case WIFI_AUTH_WPA3_PSK: return "WPA3-SAE";
#endif
#ifdef WIFI_AUTH_WPA2_WPA3_PSK
      case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
#endif
      default: return "SECURED";
    }
  }

  bool saveTargetReport(WifiTarget* t){
    if(!t || !storage || !storage->writable()) return false;
    char path[80];
    snprintf(path,sizeof(path),"/dirk/targets/AP_%02X%02X%02X%02X%02X%02X.txt",
      t->bssid.b[0],t->bssid.b[1],t->bssid.b[2],t->bssid.b[3],t->bssid.b[4],t->bssid.b[5]);
    String r;
    r += "DIRK AP INSPECTION\n==================\n";
    r += "SSID: "; r += (t->ssid[0]?t->ssid:"<hidden>"); r += "\n";
    char line[128];
    snprintf(line,sizeof(line),"BSSID: %02X:%02X:%02X:%02X:%02X:%02X\n",
      t->bssid.b[0],t->bssid.b[1],t->bssid.b[2],t->bssid.b[3],t->bssid.b[4],t->bssid.b[5]); r+=line;
    snprintf(line,sizeof(line),"Band: %s\nChannel: %u\nRSSI: %d dBm\nSecurity: %s\n",
      t->band==Band::Band5?"5 GHz":"2.4 GHz",
      t->channel,t->rssi,authName(t->auth)); r+=line;
    r += "Cipher: "; r += (t->cipher[0]?t->cipher:"unknown"); r += "\nAKM: "; r += (t->akm[0]?t->akm:"unknown"); r += "\nPMF: "; r += (t->pmfRequired?"required":(t->pmfCapable?"capable":"not observed")); r += "\nWPS: "; r += (t->wps?"yes":"no"); r += "\nAccess: "; r += ((wifi_auth_mode_t)t->auth==WIFI_AUTH_OPEN?"open":"PASSWORD PROTECTED"); r += "\n";
    snprintf(line,sizeof(line),"Beacon interval: %u TU\nDTIM period: %u\nCountry: %s\nPHY: %s%s%s\nWMM: %s\nBeacon observations: %lu\n",
      t->beaconInterval,t->dtimPeriod,t->country[0]?t->country:"unknown",t->ht?"11n ":"",t->vht?"11ac ":"",t->he?"11ax":"",t->wmm?"yes":"not observed",(unsigned long)t->beaconCount); r+=line;
    r += "Source: passive Wi-Fi scan / management-frame metadata\n";
    bool ok=storage->overwrite(path,(const uint8_t*)r.c_str(),r.length());
    if(ok) targets->save(t->id,t->authorized);
    return ok;
  }

  void handleTargetDetail(const InputEvent& e){
    auto* t=targets->byId(detailTargetId); if(!t){ screen=12; return; }
    if(e.action==InputAction::Left || e.action==InputAction::Right){ detailPage^=1u; }
    else if(e.action==InputAction::Tab && t->band==Band::Band24 && wifi){ wifi->inspect(*t,900); }
    else if(e.action==InputAction::Select || (e.action==InputAction::Character && (e.ch=='s'||e.ch=='S'))){
      reportStatus=saveTargetReport(t)?1:2;
    } else if(e.action==InputAction::Character && (e.ch=='a'||e.ch=='A')){
      // Authorization is explicit and only meaningful for a saved target.
      if(t->saved){ t->authorized=!t->authorized; targets->save(t->id,t->authorized); }
    }
  }

  void targetDetail(lgfx::LGFXBase& d,uint16_t a){
    auto* t=targets->byId(detailTargetId); if(!t){ d.setCursor(8,30); d.print("target unavailable"); return; }
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print(detailPage?"ap inspect // beacon IEs":"ap inspect // identity");
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    if(!detailPage){
      d.setCursor(6,36); d.printf("SSID   %-25.25s",t->ssid[0]?t->ssid:"<hidden>");
      d.setCursor(6,47); d.printf("BSSID  %02X:%02X:%02X:%02X:%02X:%02X",t->bssid.b[0],t->bssid.b[1],t->bssid.b[2],t->bssid.b[3],t->bssid.b[4],t->bssid.b[5]);
      d.setCursor(6,58); d.printf("BAND   %s   CH %u",t->band==Band::Band5?"5 GHz":"2.4 GHz",t->channel);
      d.setCursor(6,69); d.printf("RSSI   %d dBm   AUTH %s",t->rssi,authName(t->auth));
      d.setCursor(6,80); d.printf("CIPHER %-8s AKM %s",t->cipher[0]?t->cipher:"?",t->akm[0]?t->akm:"?");
      d.setCursor(6,91); d.printf("PMF %s  WPS %s",t->pmfRequired?"REQ":(t->pmfCapable?"CAP":"--"),t->wps?"YES":"NO");
      d.setTextColor(t->authorized?a:0x7BEF,TFT_BLACK); d.setCursor(6,103); d.print(t->authorized?"AUTHORIZED TEST TARGET":(t->saved?"SAVED TARGET":"passive scan metadata"));
    } else {
      d.setCursor(6,38); d.printf("beacon interval  %u TU",t->beaconInterval);
      d.setCursor(6,51); d.printf("DTIM period      %u",t->dtimPeriod);
      d.setCursor(6,64); d.printf("country          %s",t->country[0]?t->country:"--");
      d.setCursor(6,77); d.print("PHY              "); if(t->ht)d.print("11n "); if(t->vht)d.print("11ac "); if(t->he)d.print("11ax "); if(!t->ht&&!t->vht&&!t->he)d.print("not observed");
      d.setCursor(6,90); d.printf("WMM %-3s   beacons %lu",t->wmm?"yes":"--",(unsigned long)t->beaconCount);
      d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(6,104); d.print(t->band==Band::Band24?"tab refreshes passive beacon inspect":"C5 metadata only on 5 GHz");
    }
    d.fillRoundRect(171,21,64,12,2,reportStatus==1?0x07E0:(reportStatus==2?0xF800:accentDark()));
    d.setTextColor(TFT_WHITE,reportStatus==1?0x07E0:(reportStatus==2?0xF800:accentDark())); d.setCursor(176,23); d.print(reportStatus==1?"saved":(reportStatus==2?"save fail":"ent save"));
  }


  void handleApMonitor(const InputEvent& e){
    size_t n=bandTargetCount();
    if(e.action==InputAction::Up&&n)wifiSel=(wifiSel+n-1)%n;
    else if(e.action==InputAction::Down&&n)wifiSel=(wifiSel+1)%n;
    else if(e.action==InputAction::Tab)runBandScan(false);
    else if(e.action==InputAction::Select&&n){ int idx=bandIndexAt(wifiSel); auto*t=idx>=0?targets->at(idx):nullptr; if(t&&t->band==Band::Band24)wifi->inspect(*t,600); }
  }
  void apMonitor(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print(wifiBand5?"ap monitor // 5g":"ap monitor // 2.4g");
    size_t n=bandTargetCount(); if(!n){d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(8,47);d.print(wifiBand5?"waiting for c5 5 ghz ap...":"scanning 2.4 ghz...");return;}
    int idx=bandIndexAt(wifiSel<n?wifiSel:0); auto*t=idx>=0?targets->at(idx):nullptr; if(!t)return;
    d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,42);d.printf("%-24.24s",t->ssid[0]?t->ssid:"<hidden>");
    d.setCursor(7,57);d.printf("CH %u  RSSI %d dBm",t->channel,t->rssi);
    int w=constrain((int)t->rssi+100,1,100); d.drawRect(7,72,202,15,accentDark());d.fillRect(9,74,w*2,11,a);
    d.setCursor(7,93);d.printf("Beacons %lu  %s",(unsigned long)t->beaconCount,authName(t->auth));
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(7,106);d.print("auto refresh  enter passive inspect");
  }

  void sampleAirNow(){ airSampleValid=wifi->sampleAir(monitorChannel,1200,airStats); }
  void handleProbeMonitor(const InputEvent&e){
    if(e.action==InputAction::Left&&monitorChannel>1){ monitorChannel--; sampleAirNow(); }
    else if(e.action==InputAction::Right&&monitorChannel<14){ monitorChannel++; sampleAirNow(); }
    else if(e.action==InputAction::Select||e.action==InputAction::Tab)sampleAirNow();
  }
  void probeMonitor(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("probe monitor // passive");
    d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,43);d.printf("Channel %u",monitorChannel);
    d.setCursor(7,59);d.printf("Probe requests  %lu",(unsigned long)airStats.probes);
    d.setCursor(7,74);d.printf("Unique stations %u",airStats.uniqueStations);
    d.setCursor(7,89);d.printf("Data frames     %lu",(unsigned long)airStats.data);
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(7,106);d.print("auto 1.2s passive samples until back");
  }

  void handleDeauthMonitor(const InputEvent&e){
    if(e.action==InputAction::Left&&monitorChannel>1){ monitorChannel--; sampleAirNow(); }
    else if(e.action==InputAction::Right&&monitorChannel<14){ monitorChannel++; sampleAirNow(); }
    else if(e.action==InputAction::Select||e.action==InputAction::Tab)sampleAirNow();
  }
  void deauthMonitor(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("deauth monitor // passive");
    d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,43);d.printf("Channel %u",monitorChannel);
    d.setCursor(7,59);d.printf("Deauth frames   %lu",(unsigned long)airStats.deauth);
    d.setCursor(7,74);d.printf("Disassoc frames %lu",(unsigned long)airStats.disassoc);
    d.setCursor(7,89);d.printf("Unique stations %u",airStats.uniqueStations);
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(7,106);d.print("auto 1.2s passive samples until back");
  }

  // A diagnostic list of *received* 5 GHz SSIDs. This is intentionally
  // independent of the combined view so we can distinguish a missed scan
  // from an SSID-merging problem on the actual Cardputer ADV hardware.
  bool diag5Visible(const WifiTarget* t,uint32_t now) const {
    return t && t->band==Band::Band5 && t->lastSeen &&
      (uint32_t)(now-t->lastSeen)<=60000u;
  }
  size_t diag5Count(uint32_t now) const {
    size_t n=0;
    for(size_t i=0;i<targets->count();++i) if(diag5Visible(targets->at(i),now)) ++n;
    return n;
  }
  const WifiTarget* diag5TargetAt(size_t ordinal,uint32_t now) const {
    for(size_t i=0;i<targets->count();++i){
      const auto* t=targets->at(i);
      if(diag5Visible(t,now)){
        if(!ordinal) return t;
        --ordinal;
      }
    }
    return nullptr;
  }
  bool diag5Exact24Match(const WifiTarget* t,uint32_t now) const {
    if(!t || !UnifiedApView::actualName(t->ssid)) return false;
    for(size_t i=0;i<targets->count();++i){
      const auto* other=targets->at(i);
      if(other && other->band==Band::Band24 && other->lastSeen &&
         (uint32_t)(now-other->lastSeen)<=15000u &&
         strcmp(t->ssid,other->ssid)==0) return true;
    }
    return false;
  }
  void handleDiag5g(const InputEvent&e){
    wifiBand5=true;
    if(e.action==InputAction::Select && janos) janos->ping();
    else if(e.action==InputAction::Tab && janos) janos->startScan5();
    else if(e.action==InputAction::Right) diag5Page=(uint8_t)((diag5Page+1u)%5u);
    else if(e.action==InputAction::Left) diag5Page=(uint8_t)((diag5Page+4u)%5u);
    else if(diag5Page==4 && e.action==InputAction::Up){
      const size_t n=diag5Count(millis());
      if(n) diag5Sel_=(uint16_t)((diag5Sel_+n-1u)%n);
    }
    else if(diag5Page==4 && e.action==InputAction::Down){
      const size_t n=diag5Count(millis());
      if(n) diag5Sel_=(uint16_t)((diag5Sel_+1u)%n);
    }
  }
  void diag5g(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23);
    d.printf("5g diagnostics // %u/5",(unsigned)diag5Page+1);
    const C5Transport* tr=janos?janos->transportInfo():nullptr;
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    if(diag5Page==0){
      d.setCursor(5,38); d.printf("UART RX%d TX%d %lu",tr?tr->rxPin():-1,tr?tr->txPin():-1,(unsigned long)(tr?tr->baud():0));
      d.setCursor(5,49); d.printf("C5:%s JanOS:%s",tr&&tr->everDetected()?"RX":"NO RX",janos&&janos->ready()?"READY":"UNKNOWN");
      d.setCursor(5,60); d.printf("Scan:%s TX:%lu RX:%lu",janos&&janos->scanning()?"RUN":"IDLE",(unsigned long)(tr?tr->txLines():0),(unsigned long)(tr?tr->rxLines():0));
      d.setCursor(5,71); d.printf("5g:%lu  2g:%lu  reject:%lu",(unsigned long)(janos?janos->parsed():0),(unsigned long)(janos?janos->filtered24():0),(unsigned long)(janos?janos->unparsed():0));
      d.setCursor(5,82); d.printf("RXbytes:%lu  drop:%lu",(unsigned long)(tr?tr->rxBytes():0),(unsigned long)(tr?tr->droppedLines():0));
      d.setCursor(5,93); d.printf("long:%lu late:%lu",(unsigned long)(tr?tr->overlongLines():0),(unsigned long)wdLateFrames);
      d.setCursor(5,104); d.print("TAB scan   arrows: pages");
    }else if(diag5Page==1){
      d.setCursor(5,38); d.printf("noMAC:%lu badCH:%lu badRSSI:%lu",(unsigned long)(janos?janos->noMac():0),(unsigned long)(janos?janos->badChannel():0),(unsigned long)(janos?janos->badRssi():0));
      d.setCursor(5,49); d.printf("starts:%lu done:%lu",(unsigned long)(janos?janos->scanStarts():0),(unsigned long)(janos?janos->scanCompletions():0));
      d.setCursor(5,60); d.print("event: "); d.print(janos?janos->lastEvent().substring(0,32):String("-"));
      d.setCursor(5,71); d.print("last command:");
      d.setCursor(5,82); d.print(tr?tr->lastSent().substring(0,37):String("-"));
      d.setCursor(5,93); d.printf("CSV rows decoded: %lu",(unsigned long)(janos?janos->csvParsed():0));
      d.setCursor(5,104); d.print("arrows: raw C5 records");
    }else if(diag5Page==4){
      const uint32_t now=millis();
      const size_t count=diag5Count(now);
      if(diag5Sel_>=count)diag5Sel_=count?count-1:0;
      d.setCursor(5,36);d.printf("C5 received: %u (last 60s)",(unsigned)count);
      if(!count){
        d.setCursor(7,61);d.print("No fresh 5 GHz records");
        d.setCursor(7,76);d.print("TAB: request 5 GHz scan");
      }else{
        size_t first=diag5Sel_>3?diag5Sel_-3:0;
        if(first+4>count && count>4)first=count-4;
        for(size_t r=0;r<4 && first+r<count;r++){
          const auto* t=diag5TargetAt(first+r,now);if(!t)continue;
          const int y=45+(int)r*13;const bool selected=first+r==diag5Sel_;
          if(selected)d.fillRoundRect(2,y-2,236,12,2,accentDark());
          d.setTextColor(selected?TFT_WHITE:0xBDF7,selected?accentDark():TFT_BLACK);
          d.setCursor(5,y);
          d.printf("%-23.23s CH%3u %4d",UnifiedApView::actualName(t->ssid)?t->ssid:"<unnamed>",
             (unsigned)t->channel,(int)t->rssi);
        }
        const auto* picked=diag5TargetAt(diag5Sel_,now);
        if(picked){
          const bool match=diag5Exact24Match(picked,now);
          d.setTextColor(a,TFT_BLACK);d.setCursor(5,98);
          d.printf("2.4 exact: %s   name len:%u",match?"YES":"NO",(unsigned)strlen(picked->ssid));
        }
      }
      d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(5,109);d.print("UP/DN AP  L/R page  TAB scan");
    }else{
      String value=janos ? (diag5Page==2 ? janos->lastCandidate() : janos->lastLine()) : String();
      d.setCursor(5,37); d.print(diag5Page==2?"Last AP candidate (raw):":"Last UART line (raw):");
      if(!value.length()){ d.setCursor(5,54); d.print("no matching UART text yet"); }
      else{
        // Render a full 180-char sample in five readable slices.
        for(int r=0;r<5;r++){
          int idx=r*35;
          if(idx>=value.length())break;
          d.setCursor(5,49+r*11);
          d.print(value.substring(idx,idx+35));
        }
      }
      d.setCursor(5,105); d.print("arrows: pages | TAB scan");
    }
  }

  void handleNetworkTools(const InputEvent&e){
    if(e.action==InputAction::Select && nettools && nettools->connected()){
      nettools->scanGateway();netScanDone=true;networkResultSel_=0;enterScreen(66);
    }else if(e.action==InputAction::Tab && netScanDone){networkResultSel_=0;enterScreen(66);}
  }
  void networkTools(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("network survey // gateway");
    if(!nettools||!nettools->connected()){
      d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,47);
      d.print("Not connected to Wi-Fi."); d.setCursor(7,62);
      d.print("Connect via Settings > Wi-Fi.");return;
    }
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    d.setCursor(7,39);d.printf("Local IP  %s",nettools->ip().toString().c_str());
    d.setCursor(7,54);d.printf("Gateway   %s",nettools->gateway().toString().c_str());
    d.setCursor(7,69);d.printf("DNS       %s",nettools->dns().toString().c_str());
    d.setCursor(7,84);d.printf("CH %u  RSSI %d dBm",(unsigned)nettools->channel(),nettools->rssi());
    d.setTextColor(a,TFT_BLACK);d.setCursor(7,105);
    d.print(netScanDone?"ENT rescan | TAB previous results":"ENT: bounded common-port survey");
  }
  void handleNetworkSurveyResults(const InputEvent& e){
    const size_t n=nettools?nettools->count():0;
    if(e.action==InputAction::Up && n)networkResultSel_=(networkResultSel_+n-1)%n;
    else if(e.action==InputAction::Down && n)networkResultSel_=(networkResultSel_+1)%n;
    else if(e.action==InputAction::Tab && nettools && nettools->connected()){
      nettools->scanGateway();netScanDone=true;networkResultSel_=0;
    }
    else if(e.action==InputAction::Select && n)enterScreen(69);
  }
  void networkSurveyResults(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("gateway survey // TCP results");
    if(!nettools||!netScanDone){ d.setCursor(7,45);d.print("No survey yet");return; }
    const char* services[]={"SSH","Telnet","DNS","HTTP","HTTPS","SMB","HTTP-alt","HTTPS-alt"};
    size_t open=0;for(size_t i=0;i<nettools->count();i++)if(nettools->hits()[i].open)open++;
    d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,37);
    d.printf("GW %s | %u/%u open",nettools->gateway().toString().c_str(),(unsigned)open,(unsigned)nettools->count());
    const size_t n=nettools->count();
    if(n && networkResultSel_>=n)networkResultSel_=0;
    const size_t first=networkResultSel_>4?networkResultSel_-4:0;
    for(size_t i=first;i<n && i<first+5;i++){
      const PortHit& hit=nettools->hits()[i];
      const int y=51+(int)(i-first)*12;const bool selected=i==networkResultSel_;
      if(selected)d.fillRoundRect(4,y-2,231,12,2,a);
      d.setTextColor(selected?TFT_BLACK:TFT_WHITE,selected?a:TFT_BLACK);
      d.setCursor(7,y);d.printf("%-5u %-10.10s %s",(unsigned)hit.port,services[i<8?i:7],hit.open?"OPEN":"closed/no reply");
    }
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(7,113);
    d.print("TCP connection test; no service ID");
  }


  void handlePortDetail(const InputEvent& e){ (void)e; }
  void portDetail(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("gateway survey // port detail");
    if(!nettools||!netScanDone||networkResultSel_>=nettools->count())return;
    const PortHit& hit=nettools->hits()[networkResultSel_];
    const char* hints[]={"Secure remote shell","Unencrypted remote terminal","Name resolution / DNS","Web service / HTTP",
                         "Encrypted web / HTTPS","SMB file sharing","Alternate HTTP web","Alternate HTTPS web"};
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    d.setCursor(7,41);d.printf("TCP port: %u",(unsigned)hit.port);
    d.setCursor(7,56);d.printf("Connection: %s",hit.open?"accepted":"no connection");
    d.setCursor(7,73);d.print(hints[networkResultSel_<8?networkResultSel_:7]);
    d.setCursor(7,92);d.print("Typical use only; service unknown.");
    d.setCursor(7,108);d.print("No version or vulnerability scan.");
  }

  void handleBeaconLab(const InputEvent&e){
    if(mode->mode()!=TeamMode::Red) return;
    if(e.action==InputAction::Up) labSel=(labSel+1)%2;
    else if(e.action==InputAction::Down) labSel=(labSel+1)%2;
    else if(e.action==InputAction::Select){
      if(labSel==0){
        if(wardrive && wardrive->running()) {redStatus="end Wardrive first";redStatusUntil=millis()+2400u;}
        else beaconRan=wifi->beaconLabBurst(5000);
      }
      else if(portal){
        if(portal->running()) portal->stop();
        else if((wardrive && wardrive->running()) || (captures && captures->active()) || (wifi && wifi->scanning())){
          redStatus="wait/end scan, cap, Wardrive";redStatusUntil=millis()+2400u;
        } else if(!portal->begin("DIRK-LAB-PORTAL",storage)){
          redStatus="portal start failed";redStatusUntil=millis()+2400u;
        }
      }
    }
  }
  void beaconLab(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("wifi // authorized lab");
    d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,39);
    if(mode->mode()!=TeamMode::Red){d.print("locked: enable red mode in settings");return;}
    const char* rows[]={"bounded beacon burst","captive portal lab"};
    for(int i=0;i<2;i++){int y=52+i*18;bool on=i==labSel;if(on)d.fillRoundRect(4,y-2,228,14,2,a);d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK);d.setCursor(9,y);d.print(rows[i]);}
    d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(7,91); d.print("beacon: 4 lab SSIDs / 5s max");
    d.setCursor(7,103); d.print(portal&&portal->running()?"portal active: DIRK-LAB-PORTAL":"portal: non-secret test form only");
  }

  void handleBleMenu(const InputEvent& e){
    uint8_t N=bleCategoryCount();
    if(e.action==InputAction::Up) bleCategorySel=(bleCategorySel+N-1)%N; else if(e.action==InputAction::Down) bleCategorySel=(bleCategorySel+1)%N;
    else if(e.action==InputAction::Select){ if(bleCategorySel==0) enterScreen(34); else if(bleCategorySel==1) enterScreen(35); else if(bleCategorySel==2) enterScreen(36); else enterScreen(37); }
  }
  void bleMenu(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("bluetooth // tools");
    const char* blue[]={"recon / explore","analysis","defensive"};
    const char* red[]={"recon / explore","analysis","defensive","authorized lab"};
    uint8_t N=bleCategoryCount();
    for(uint8_t i=0;i<N;i++){int y=43+i*17;bool on=i==bleCategorySel;if(on)d.fillRoundRect(4,y-2,116,14,2,a);d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK);d.setCursor(9,y);d.print(mode->mode()==TeamMode::Red?red[i]:blue[i]);}
    sectionPanel(d,a,1);
  }

  void handleBle(const InputEvent& e){
    size_t n=ble?ble->count():0;
    if(e.action==InputAction::Tab){ if(ble && !ble->scanning() && (!wifi || !wifi->scanning())) ble->startScanAsync(1); bleSel=0; }
    else if(e.action==InputAction::Up && n) bleSel=(bleSel+n-1)%n;
    else if(e.action==InputAction::Down && n) bleSel=(bleSel+1)%n;
    else if(e.action==InputAction::Select && n) enterScreen(29);
  }
  void bleScreen(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("ble explorer // passive live");
    size_t n=ble?ble->count():0; d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(197,23); d.printf("%u",(unsigned)n);
    if(!n){ d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,45); d.print("scanning ble advertisements..."); d.setCursor(8,59); d.print("no connection or transmission."); return; }
    size_t start=bleSel>4?bleSel-4:0;
    for(size_t r=0;r<6 && start+r<n;r++){ auto* t=ble->at(start+r); int y=39+r*13; bool on=(start+r)==bleSel; if(on)d.fillRoundRect(3,y-2,234,12,2,a); d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(6,y); d.printf("%-17.17s %4d",t?t->name:"",t?t->rssi:-127); if(t){ int bars=constrain((t->rssi+100)/12,0,4); for(int b=0;b<bars;b++) d.fillRect(208+b*5,y+6-b*2,3,2+b*2,on?TFT_BLACK:a); }}
  }
  void handleBleDetail(const InputEvent& e){ size_t n=ble?ble->count():0; if(e.action==InputAction::Up && n) bleSel=(bleSel+n-1)%n; else if(e.action==InputAction::Down && n) bleSel=(bleSel+1)%n; else if(e.action==InputAction::Tab){ if(ble && !ble->scanning() && (!wifi || !wifi->scanning())) ble->startScanAsync(1); if(bleSel>=ble->count()) bleSel=0; } }
  void bleDetail(lgfx::LGFXBase& d,uint16_t a){
    size_t n=ble?ble->count():0; d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("ble target // advertisement"); if(!n){ d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,48); d.print("no ble target selected"); return; } if(bleSel>=n) bleSel=0; const BleTarget* t=ble->at(bleSel); if(!t) return;
    d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,37); d.printf("name %.24s",t->name); d.setCursor(8,49); d.printf("addr %s",t->address); d.setCursor(8,61); d.printf("rssi %d",t->rssi); if(t->hasTxPower)d.printf("  tx %d",t->txPower);
    d.setCursor(8,73); d.printf("mfg  0x%04X",t->manufacturerId); if(t->hasAppearance)d.printf("  app %u",t->appearance);
    d.setCursor(8,85); String tags; if(t->ibeacon)tags+="iBeacon "; if(t->eddystone)tags+="Eddystone "; if(t->meshtastic)tags+="Meshtastic "; if(t->meshcore)tags+="MeshCore "; if(!tags.length())tags="generic advertisement"; d.print(tags.substring(0,35));
    d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(8,98); String svc=t->service[0]?String(t->service):String("service uuid not advertised"); if(svc.length()>36)svc=svc.substring(0,36); d.print(svc);
    int w=constrain((t->rssi+100)*2,0,120); d.drawRect(8,109,124,5,accentDark()); d.fillRect(10,110,w,2,a); d.setCursor(141,107); d.print("passive only");
  }
  void handleBleAnalysis(const InputEvent& e){ if(e.action==InputAction::Select || e.action==InputAction::Tab){ if(ble&&!ble->scanning()&&(!wifi||!wifi->scanning()))ble->startScanAsync(1); } }
  void bleAnalysis(lgfx::LGFXBase& d,uint16_t a){
    size_t n=ble?ble->count():0,named=0,strong=0,mfg=0,tx=0; int best=-127,worst=0; for(size_t i=0;i<n;i++){auto*t=ble->at(i);if(!t)continue;if(t->named)named++;if(t->rssi>-60)strong++;if(t->manufacturerId)mfg++;if(t->hasTxPower)tx++;if(t->rssi>best)best=t->rssi;if(i==0||t->rssi<worst)worst=t->rssi;}
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("ble // advertisement analyzer");d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(8,41);d.printf("devices %u  named %u",(unsigned)n,(unsigned)named);d.setCursor(8,55);d.printf("manufacturer data %u",(unsigned)mfg);d.setCursor(8,69);d.printf("tx power advertised %u",(unsigned)tx);d.setCursor(8,83);d.printf("strong %u  best %d",(unsigned)strong,best);d.setCursor(8,97);d.printf("weak %d dbm",n?worst:-127);d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(8,109);d.print("passive advertisements only");
  }
  void handleBleBlue(const InputEvent& e){ handleBleAnalysis(e); }
  void bleBlue(lgfx::LGFXBase& d,uint16_t a){
    size_t n=ble?ble->count():0,strong=0,unnamed=0,ibe=0,edd=0,mesh=0; for(size_t i=0;i<n;i++){auto*t=ble->at(i);if(!t)continue;if(t->rssi>-55)strong++;if(!t->named)unnamed++;if(t->ibeacon)ibe++;if(t->eddystone)edd++;if(t->meshtastic||t->meshcore)mesh++;}
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("ble // defensive monitor");d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(8,41);d.printf("observed %u  close %u",(unsigned)n,(unsigned)strong);d.setCursor(8,55);d.printf("unnamed %u  mesh radios %u",(unsigned)unnamed,(unsigned)mesh);d.setCursor(8,69);d.printf("iBeacon %u  Eddystone %u",(unsigned)ibe,(unsigned)edd);d.setCursor(8,83);d.print("no connect / no GATT probing");d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(8,103);d.print("tab/enter refreshes passive scan");
  }
  const WifiTarget* dualMatchAt(const WifiTarget& t) const {
    if(!targets)return nullptr;
    const WifiTarget* best=nullptr;
    for(size_t i=0;i<targets->count();++i){
      const WifiTarget* x=targets->at(i);
      if(x && sameDualBandSsid(t,*x) && (!best || x->rssi>best->rssi))best=x;
    }
    return best;
  }
  bool exportDualBand(const WifiTarget& t,const WifiTarget* other){
    if(!storage || !storage->writable() || !mode || !mode->permitsActive(t))return false;
    const RedPosture p=assessDualBand(t,other);
    char path[94];snprintf(path,sizeof(path),"/dirk/reports/dual_policy_%02X%02X%02X%02X%02X%02X.csv",
      t.bssid.b[0],t.bssid.b[1],t.bssid.b[2],t.bssid.b[3],t.bssid.b[4],t.bssid.b[5]);
    digitalWrite(StorageManager::LORA_CS,HIGH);
    if(SD.exists(path))SD.remove(path);
    File f=SD.open(path,FILE_WRITE);if(!f)return false;
    f.println("ssid,primary_band,other_band_confirmed,severity,finding,recommendation");
    bool ok=true;
    for(uint8_t i=0;i<p.count;i++){
      const RedFinding& x=p.findings[i];
      String line=auditCsvCell(t.ssid)+","+String(t.band==Band::Band5?"5":"2.4")+","+
        String(other?"yes":"no")+","+auditCsvCell(redSeverityName(x.severity))+","+
        auditCsvCell(x.title)+","+auditCsvCell(x.advice)+"\n";
      if(f.print(line)!=line.length()){ok=false;break;}
    }
    f.flush();f.close();if(!ok){SD.remove(path);return false;}
    File v=SD.open(path,FILE_READ);ok=v&&v.size()>80;
    if(v)v.close();return ok;
  }
  void handleDualBandReview(const InputEvent& e){
    size_t n=redAuthorizedCount();if(!n)return;
    if(e.action==InputAction::Up){dualReviewSel_=(dualReviewSel_+n-1)%n;dualReviewPage_=0;}
    else if(e.action==InputAction::Down){dualReviewSel_=(dualReviewSel_+1)%n;dualReviewPage_=0;}
    else if(e.action==InputAction::Left || e.action==InputAction::Right){
      const WifiTarget* t=redAuthorizedAt(dualReviewSel_%n);
      if(t && assessDualBand(*t,dualMatchAt(*t)).count>3)dualReviewPage_=(dualReviewPage_+1)%2;
      else dualReviewPage_=0;
    }
    else if(e.action==InputAction::Tab){
      const WifiTarget* t=redAuthorizedAt(dualReviewSel_%n);
      redExtraStatus_=(t && exportDualBand(*t,dualMatchAt(*t)))?"CSV saved /dirk/reports":"SD export failed";
      redExtraStatusUntil_=millis()+3200;
    }
  }
  void dualBandReview(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("red // dual-band policy");
    size_t n=redAuthorizedCount();d.setTextColor(TFT_WHITE,TFT_BLACK);
    if(!n){d.setCursor(7,45);d.print("No authorized saved APs");return;}
    if(dualReviewSel_>=n)dualReviewSel_=0;
    const WifiTarget* t=redAuthorizedAt(dualReviewSel_);if(!t)return;
    const WifiTarget* other=dualMatchAt(*t);RedPosture p=assessDualBand(*t,other);
    d.setCursor(7,37);d.printf("%u/%u  %.26s",(unsigned)(dualReviewSel_+1),(unsigned)n,t->ssid);
    d.setCursor(7,51);d.printf("%s ch%u | other band %s",t->band==Band::Band5?"5G":"2.4G",(unsigned)t->channel,other?"seen":"unknown");
    const uint8_t start=dualReviewPage_?3:0;
    for(uint8_t i=0;i<3 && start+i<p.count;i++){
      const RedFinding& x=p.findings[start+i];
      d.setTextColor(x.severity==RedSeverity::Review?0xFFE0:a,TFT_BLACK);
      d.setCursor(7,65+i*14);d.print(String(x.title).substring(0,36));
    }
    if((int32_t)(redExtraStatusUntil_-millis())>0){
      d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(7,108);d.print(redExtraStatus_.substring(0,36));
    }
  }

  void requestBleLabScan(){
    if(ble && !ble->scanning() && !(wifi&&wifi->scanning()) && !(wardrive&&wardrive->running()))
      ble->startScanAsync(2);
  }
  void handleBleRed(const InputEvent& e){
    if(!mode || mode->mode()!=TeamMode::Red) return;
    const size_t n=ble?ble->count():0;
    if(e.action==InputAction::Tab) requestBleLabScan();
    else if(e.action==InputAction::Up && n) bleRedSel_=(bleRedSel_+n-1)%n;
    else if(e.action==InputAction::Down && n) bleRedSel_=(bleRedSel_+1)%n;
    else if(e.action==InputAction::Select && n){
      const BleTarget* selected=ble->at(bleRedSel_);
      if(!selected)return;
      bleReviewSnapshot_=*selected;bleReviewHasSnapshot_=true;
      bleReviewFinding_=0;bleExportConfirm_=false;enterScreen(72);
    }
  }
  void bleRed(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("ble // owned-device assessment");
    if(!mode || mode->mode()!=TeamMode::Red){d.setCursor(7,45);d.print("Red mode required");return;}
    size_t n=ble?ble->count():0;
    if(!n){d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,46);d.print("TAB scan advertisements");
      d.setCursor(7,62);d.print("Select only your own devices");return;}
    if(bleRedSel_>=n) bleRedSel_=0;
    size_t first=bleRedSel_>4?bleRedSel_-4:0;
    for(size_t r=0;r<5 && first+r<n;++r){
      auto* t=ble->at(first+r);if(!t)continue;
      int y=40+(int)r*14;bool on=(first+r)==bleRedSel_;
      if(on)d.fillRoundRect(3,y-2,234,13,2,a);
      d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK);
      d.setCursor(7,y);d.printf("%-25.25s %d",t->name,t->rssi);
    }
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(7,110);d.print("Passive scan; no BLE connection");
  }
  bool exportBlePosture(const BleTarget& t){
    if(!storage || !storage->writable() || !mode || mode->mode()!=TeamMode::Red) return false;
    char hex[13]{};unsigned k=0;
    for(const char* p=t.address;*p && k<12;++p){
      if((*p>='0'&&*p<='9')||(*p>='a'&&*p<='f')||(*p>='A'&&*p<='F'))hex[k++]=*p;
    }
    if(k!=12)return false;
    char path[90];snprintf(path,sizeof(path),"/dirk/reports/ble_posture_%s.csv",hex);
    const RedPosture report=assessBlePosture(t);
    digitalWrite(StorageManager::LORA_CS,HIGH);
    // Explicit local report only. Never export manufacturer payload bytes.
    if(SD.exists(path))SD.remove(path);
    File f=SD.open(path,FILE_WRITE);if(!f)return false;
    f.println("address,name,rssi,service_uuid,manufacturer_id,mfg_bytes,service_bytes,severity,finding,recommendation");
    bool ok=true;
    for(uint8_t i=0;i<report.count;i++){
      const RedFinding& x=report.findings[i];
      String line=auditCsvCell(t.address)+","+auditCsvCell(t.name)+","+String(t.rssi)+","+
        auditCsvCell(t.service)+","+String(t.manufacturerId)+","+
        String(t.manufacturerDataBytes)+","+String(t.serviceDataBytes)+","+
        auditCsvCell(redSeverityName(x.severity))+","+auditCsvCell(x.title)+","+auditCsvCell(x.advice)+"\n";
      if(f.print(line)!=line.length()){ok=false;break;}
    }
    f.flush();f.close();
    if(!ok){SD.remove(path);return false;}
    File check=SD.open(path,FILE_READ);ok=check&&check.size()>110;
    if(check)check.close();return ok;
  }
  void handleBlePosture(const InputEvent& e){
    if(!mode || mode->mode()!=TeamMode::Red)return;
    if(!bleReviewHasSnapshot_){bleExportConfirm_=false;return;}
    const BleTarget* t=&bleReviewSnapshot_;
    const RedPosture p=assessBlePosture(*t);
    if(e.action==InputAction::Left && p.count){bleReviewFinding_=(bleReviewFinding_+p.count-1)%p.count;bleExportConfirm_=false;}
    else if(e.action==InputAction::Right && p.count){bleReviewFinding_=(bleReviewFinding_+1)%p.count;bleExportConfirm_=false;}
    else if(e.action==InputAction::Tab){
      if(!bleExportConfirm_){bleExportConfirm_=true;redExtraStatus_="Owned device? TAB to export";}
      else {redExtraStatus_=exportBlePosture(*t)?"Report saved /dirk/reports":"SD report failed";bleExportConfirm_=false;}
      redExtraStatusUntil_=millis()+3500u;
    }
  }
  void blePosture(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("red // BLE advertisement audit");
    if(!bleReviewHasSnapshot_)return;
    const BleTarget* t=&bleReviewSnapshot_;
    const RedPosture p=assessBlePosture(*t);if(!p.count)return;
    if(bleReviewFinding_>=p.count)bleReviewFinding_=0;
    const RedFinding& f=p.findings[bleReviewFinding_];
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    d.setCursor(7,38);d.printf("%.29s %d dBm",t->name,(int)t->rssi);
    d.setCursor(7,51);d.printf("%.17s | %u/%u",t->address,(unsigned)(bleReviewFinding_+1),(unsigned)p.count);
    d.setTextColor(f.severity==RedSeverity::Review?0xFFE0:a,TFT_BLACK);
    d.setCursor(7,67);d.print(String(f.title).substring(0,36));
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    d.setCursor(7,85);d.print(String(f.advice).substring(0,36));
    if((int32_t)(redExtraStatusUntil_-millis())>0){
      d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(7,105);d.print(redExtraStatus_.substring(0,36));
    }
  }

  void handleWardriveMenu(const InputEvent& e){
    static constexpr uint8_t N=8;
    if(e.action==InputAction::Up) wdMenuSel=(wdMenuSel+N-1)%N;
    else if(e.action==InputAction::Down) wdMenuSel=(wdMenuSel+1)%N;
    else if(e.action==InputAction::Left || e.action==InputAction::Right){
      if(wdMenuSel==6 && wardrive) wardrive->toggleSound();
    } else if(e.action==InputAction::Select){
      if(wdMenuSel==0){ if(wardrive&&wardrive->running()) enterScreen(23); else enterScreen(22); }
      else if(wdMenuSel==1 && wardrive && wardrive->running()){
        // Explicitly finish the session without rebooting or leaving the menu.
        cancelBuddyTone();
        wardrive->stop();
        // Immediately invalidate the old Wardrive animation/telemetry timing.
        // The next normal menu repaint starts fresh; no active scene can tick.
        lastLiveTick=0; lastWdInfoAt=0; wdDensityValid_=false;
        lastBuddySoundEventSerial=wardrive->eventSerial();
        wdMenuSel=0;
      }
      else if(wdMenuSel==2) enterScreen(24);
      else if(wdMenuSel==3) enterScreen(25);
      else if(wdMenuSel==4) enterScreen(26);
      else if(wdMenuSel==5) enterScreen(27);
      else if(wdMenuSel==6 && wardrive) wardrive->toggleSound();
      else if(wdMenuSel==7){ gpsDiagPage=0; enterScreen(64); }
    }
  }

  void wardriveMenu(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("wardrive // hub");
    static const char* rows[]={"start / resume","end session","live discoveries","grid map","operator dossier","providers","buddy sound","gps diagnostics"};
    const bool active=wardrive && wardrive->running();
    for(int i=0;i<8;i++){
      // Eight rows fit fully above the 117px footer, without touching the
      // animated right panel or the previously fixed menu rendering path.
      int y=36+i*10; const bool on=i==wdMenuSel;
      if(on)d.fillRoundRect(4,y-2,116,10,2,a);
      d.setTextColor(on?TFT_BLACK:((i==1&&!active)?0x7BEF:TFT_WHITE),on?a:TFT_BLACK);
      d.setCursor(9,y); d.print(rows[i]);
      if(i==0 && active){ d.setCursor(110,y); d.print("*"); }
      if(i==6 && wardrive){ d.setCursor(105,y); d.print(wardrive->soundEnabled()?"on":"off"); }
    }
    sectionPanel(d,a,2);
  }

  void handleGpsDiagnostics(const InputEvent& e){
    if(e.action==InputAction::Left || e.action==InputAction::Right ||
       e.action==InputAction::Select){
      gpsDiagPage=(gpsDiagPage+1)%2;
    } else if(e.action==InputAction::Tab && gps && !(wardrive && wardrive->running())){
      // Diagnostic override only: a receiver previously reconfigured by
      // another firmware may be operating at 9600 instead of 115200.
      // A proven 115200-baud fix must not be destroyed by a diagnostic key.
      // 9600 is for a no-fix hardware probe only; Back restores 115200.
      if(gps->baud()==115200u && gps->fixed()) return;
      gps->setBaud(gps->baud()==115200u?9600u:115200u);
      gpsDiagPage=0;
    }
  }

  void gpsDiagnostics(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);
    d.setCursor(5,23); d.printf("gps // diagnostics %u/2",(unsigned)(gpsDiagPage+1));
    if(!gps){ d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(5,51); d.print("GPS provider unavailable"); return; }
    GpsFix f=gps->fix();
    d.setTextColor(TFT_WHITE,TFT_BLACK);
    if(gpsDiagPage==0){
      const char* state=!gps->everReceived()?"NO UART DATA":
        (f.valid?"POSITION FIX":(gps->checksumOk()?"NMEA / NO FIX":"UART / NO VALID NMEA"));
      d.setCursor(5,37); d.printf("status: %s",state);
      d.setCursor(5,50); d.printf("RX15 TX13 @ %lu",(unsigned long)gps->baud());
      d.setCursor(5,63); d.printf("RX bytes: %lu",(unsigned long)gps->rxBytes());
      d.setCursor(5,76); d.print("last byte: ");
      if(!gps->everReceived()) d.print("never");
      else d.printf("%lu sec ago",(unsigned long)(gps->lastRxAgeMs()/1000u));
      d.setCursor(5,89); d.printf("sats: %u    hdop: %.1f",(unsigned)f.sats,f.hdop);
      d.setCursor(5,102); d.printf("NMEA: %lu  valid: %lu",(unsigned long)gps->nmeaLines(),(unsigned long)gps->checksumOk());
    } else {
      d.setCursor(5,37); d.printf("good CS:%lu  bad CS:%lu",(unsigned long)gps->checksumOk(),(unsigned long)gps->checksumBad());
      d.setCursor(5,50); d.print("fix age: ");
      if(gps->locationAgeMs()==0xFFFFFFFFu) d.print("never");
      else d.printf("%lu sec",(unsigned long)(gps->locationAgeMs()/1000u));
      d.setCursor(5,63); d.print("last raw NMEA:");
      const char* raw=gps->lastNmea();
      if(*raw){
        d.setCursor(5,75); d.printf("%.37s",raw);
        if(strlen(raw)>37u){d.setCursor(5,87);d.printf("%.37s",raw+37);}
      } else { d.setCursor(5,75); d.print("(none received)"); }
      d.setTextColor(a,TFT_BLACK); d.setCursor(5,105);
      d.print(f.valid?"fix acquired":"TAB baud | outdoor sky test");
    }
  }

  void handleWardriveSetup(const InputEvent& e){
    static constexpr uint8_t N=4;
    if(e.action==InputAction::Up) wdSetupSel=(wdSetupSel+N-1)%N;
    else if(e.action==InputAction::Down) wdSetupSel=(wdSetupSel+1)%N;
    else if(e.action==InputAction::Left || e.action==InputAction::Right || e.action==InputAction::Select){
      if(wdSetupSel==0) wdUse5=!wdUse5;
      else if(wdSetupSel==1) wdUseBle=!wdUseBle;
      else if(wdSetupSel==2) wdRequireGps=!wdRequireGps;
      else if(wdSetupSel==3 && e.action==InputAction::Select && wardrive){
        bool use5=wdUse5 && janos;
        if(wardrive->start(use5,wdUseBle,wdRequireGps)){ wdStartStatus=""; enterScreen(23); }
        else wdStartStatus=wardrive->startError();
      }
    }
  }

  void wardriveSetup(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("new wardrive // setup");
    const char* names[]={"5g monsterc5","ble discovery","require gps fix","start run"};
    for(int i=0;i<4;i++){ int y=42+i*17; bool on=i==wdSetupSel; if(on)d.fillRoundRect(4,y-2,228,14,2,a); d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(9,y); d.print(names[i]); d.setCursor(185,y);
      if(i==0) d.print(wdUse5?(janos&&janos->available()?"on":"probe"):"off");
      else if(i==1) d.print(wdUseBle?"on":"off"); else if(i==2) d.print(wdRequireGps?"yes":"no");
    }
    if(wdStartStatus.length()){ d.setTextColor(0xF800,TFT_BLACK); d.setCursor(6,108); d.print(wdStartStatus.substring(0,36)); }
  }

  template <typename Gfx>
  void wdHeader(Gfx& d,uint16_t a){
    auto st=wardrive->stats(); GpsFix g=gps?gps->fix():GpsFix{};
    // A satellite count is not a fix flag. Always state the GNSS state.
    const char* gpsState=g.valid?"FIX":
      (gps&&gps->present()?"WAIT":
      (gps&&gps->everReceived()?(gps->lastRxAgeMs()<8000u?"RAW":"STALE"):"NO RX"));
    d.fillRect(0,19,240,17,TFT_BLACK); d.setTextColor(a,TFT_BLACK); d.setCursor(3,23);
    if(wardrive->uses5()) d.printf("G:%s/%u 2:%lu 5:%lu B:%lu",gpsState,g.sats,(unsigned long)st.unique24,(unsigned long)st.unique5,(unsigned long)st.uniqueBle);
    else d.printf("G:%s/%u 2:%lu 5:-- B:%lu",gpsState,g.sats,(unsigned long)st.unique24,(unsigned long)st.uniqueBle);
  }

  void handleWardriveTank(const InputEvent& e){
    if(e.action==InputAction::Left) enterScreen(26);
    else if(e.action==InputAction::Right || e.action==InputAction::Select) enterScreen(24);
    else if(e.action==InputAction::Tab && wardrive){ wardrive->stop(); enterScreen(3); }
  }

  struct BuddyPose { int x=8; int y=58; bool right=true; bool turning=false; uint8_t bump=0; };
  enum class BuddyFace : uint8_t { Happy, Excited, Serious, Surprised, Sad, Sleepy, Smug };

  BuddyPose buddyPose() const {
    // Continuous patrol animation uses only millis(); RF/SD work never advances
    // or stalls the vehicle state.
    const uint32_t tick=millis()/50u;
    const uint16_t p=(uint16_t)(tick%144u);
    const uint16_t travel=(p<72u)?p:(143u-p);
    static const int8_t bumps[]={0,-1,-2,-1,0,1,0,-1,-3,-2,-1,0,1,2,1,0};
    BuddyPose b;
    b.x=3 + (int)((travel*124u)/71u);
    b.bump=(uint8_t)(tick&15u);
    b.y=58 + bumps[b.bump];
    b.right=(p<72u);
    b.turning=(travel<4u || travel>68u);
    return b;
  }

  size_t freshWardriveWifiCount() const {
    // Do not rescan the whole target table for every ~90ms sprite frame.
    const uint32_t now=millis();
    if(wdDensityValid_ && (uint32_t)(now-wdDensityAt_)<950u) return wdDensity_;
    wdDensityAt_=now; wdDensityValid_=true; wdDensity_=0;
    if(!targets) return 0;
    for(size_t i=0;i<targets->count();++i){
      const WifiTarget* t=targets->at(i); if(!t) continue;
      if((t->band==Band::Band24 || t->band==Band::Band5) && t->lastSeen && (uint32_t)(now-t->lastSeen)<9000u) ++wdDensity_;
    }
    return wdDensity_;
  }

  BuddyFace buddyFace(bool event,bool gpsOk,size_t density) const {
    if(!gpsOk) return BuddyFace::Sad;
    if(wardrive && wardrive->levelUpFlash()) return BuddyFace::Excited;
    if(event) return density>=24?BuddyFace::Surprised:BuddyFace::Excited;
    if(density>=28) return BuddyFace::Serious;
    if(wardrive && wardrive->lastDiscoveryAgeMs()>45000u) return BuddyFace::Sleepy;
    if(wardrive && wardrive->stats().level>=71 && ((millis()/5000u)&1u)) return BuddyFace::Smug;
    return BuddyFace::Happy;
  }

  const char* buddyFaceText(BuddyFace f) const {
    switch(f){
      case BuddyFace::Excited:return "(^o^)";
      case BuddyFace::Serious:return "(-_-)";
      case BuddyFace::Surprised:return "(O.O)";
      case BuddyFace::Sad:return "(;_;)";
      case BuddyFace::Sleepy:return "(-.-)";
      case BuddyFace::Smug:return "(^_~)";
      default:return "(^.^)";
    }
  }

  template <typename Gfx>
  void drawAsciiBuddy(Gfx& d,uint16_t a,const BuddyPose& b,bool event,bool gpsOk,size_t density,int yOffset=0){
    const uint32_t tick=millis()/50u; const bool tread=(tick&1u)!=0;
    BuddyFace face=buddyFace(event,gpsOk,density);
    const int recoil=event?2:0; const int yy=b.y-recoil-yOffset;

    // v0.7.0 buddy: a tiny cat driver actually sits above the hatch. Only the
    // eyes/mouth change, keeping expressions cheap enough for the live view.
    int hx=b.right?b.x+22:b.x+44;
    d.setTextColor(a,TFT_BLACK); d.setCursor(hx,yy-15); d.print("/\\_/\\");
    d.setCursor(hx,yy-7); d.print(buddyFaceText(face));

    static const char* rightTop="  ___|_|___==> ";
    static const char* rightTurn="  ___|_|__=>   ";
    static const char* rightHull=" /_[_______]_\\ ";
    static const char* rightTreadA=" \\(o)o(o)o(o)/ ";
    static const char* rightTreadB=" \\o(o)o(o)o(/ ";
    static const char* rightBase="   \\_______/   ";
    static const char* leftTop=" <==___|_|___   ";
    static const char* leftTurn="   <=__|_|___   ";
    static const char* leftHull=" /_[_______]_\\ ";
    static const char* leftTreadA=" \\(o)o(o)o(o)/ ";
    static const char* leftTreadB=" \\o(o)o(o)o(/ ";
    static const char* leftBase="   \\_______/   ";

    int bodyY=yy+2;
    d.setCursor(b.x,bodyY); d.print(b.right?(b.turning?rightTurn:rightTop):(b.turning?leftTurn:leftTop));
    d.setCursor(b.x,bodyY+8); d.print(b.right?rightHull:leftHull);
    d.setCursor(b.x,bodyY+16); d.print(b.right?(tread?rightTreadB:rightTreadA):(tread?leftTreadB:leftTreadA));
    d.setCursor(b.x,bodyY+24); d.print(b.right?rightBase:leftBase);

    // Discovery recoil/spark. The expression does the personality work; this
    // small flash is just visual punctuation.
    if(event){
      int sx=b.right?b.x+91:b.x-3; int sy=bodyY+1;
      if(sx>1 && sx<238){ d.drawPixel(sx,sy,TFT_WHITE); d.drawFastHLine(sx-2,sy,5,a); d.drawFastVLine(sx,sy-2,5,a); }
    }
    int antennaX=b.right?b.x+35:b.x+55; int pulseY=yy-18-(int)(tick%3u);
    if(antennaX>1 && antennaX<239){ d.drawPixel(antennaX,pulseY,a); if((tick&3u)==0){ d.drawPixel(antennaX-1,pulseY-1,TFT_WHITE); d.drawPixel(antennaX+1,pulseY-1,TFT_WHITE); } }
  }

  bool buddyBubbleVisible(bool event,bool gpsOk) const {
    // Never make a missing fix monopolize Buddy's chatter. Once acknowledged,
    // stay quiet until a fix or a genuine discovery event changes the state.
    if(event || (wardrive&&wardrive->levelUpFlash())) return true;
    if(!wardrive) return false;
    if(!gpsOk) return !buddyGpsWarningShown_ ||
      (buddyGpsWarningUntil_ && (int32_t)(millis()-buddyGpsWarningUntil_)<0);

    // Idle chatter is deliberately intermittent; quiet space is part of the
    // personality and stops the bubble from obscuring the scene constantly.
    uint32_t slot=(wardrive->elapsedSeconds()/6u + wardrive->stats().level)%10u;
    return slot<6u;
  }

  void splitBubbleText(const String& src,String& l1,String& l2,uint8_t maxChars=18) const {
    String ss=src; ss.trim();
    if(ss.length()<=maxChars){ l1=ss; l2=""; return; }
    int cut=maxChars;
    while(cut>7 && ss.charAt(cut)!=' ') cut--;
    if(cut<=7) cut=maxChars;
    l1=ss.substring(0,cut); l1.trim();
    l2=ss.substring(cut); l2.trim();
    if(l2.length()>maxChars) l2=l2.substring(0,maxChars);
  }

  const char* tierIdleQuip(uint8_t tier,uint8_t pick) const {
    static const char* q[10][8]={
      {"fresh beacon!","contact!","rf ahead!","got another one","w00t. signal!","new box found","rolling for packets","is ch6 always busy?"},
      {"contact acquired","new mac in the ao","marking position","fresh sector","recon moving","signal on scope","field ops online","keep rolling"},
      {"signal is screaming","upper band lit","spicy rf today","spectrum rush hour","channel soup","nice clean beacon","rf weather: busy","antenna approves"},
      {"fresh mac on scope","n3w n0d3","s3ct0r pwned","rf l00t acquired","ghost on the wire","grid mapped","quietly 0wned","packet trail found"},
      {"well well well...","that is interesting","somebody is busy","mark that one","another router joins","black net awake","intel secured","suspiciously loud"},
      {"sector secured","intel logged","recon proceeding","signature confirmed","command approves","ao mapped","packet count rising","mission nominal"},
      {"the ether provides","wall of rf","tx power enjoyer","signal: very hot","5ghz woke up","spectrum crowded","rf artillery ahead","airwaves saturated"},
      {"noted.","another one.","grid owned.","contact fading","n3w t3rr1t0ry","busy airspace","ghost traffic","keep moving."},
      {"expected.","mark it.","r3c0rd next?","another sector falls","rf dominance","operator steady","ether secured","routine recon."},
      {"interesting.","archive it.","1337 find pending","s3ct0r pwned. next.","the spectrum remembers","command online","root the map","mission continues."}
    };
    tier=min((uint8_t)9,tier); return q[tier][pick&7u];
  }

  const char* eventQuip(WardriveEventKind kind,uint8_t pick) const {
    static const char* wifi24[]={"fresh beacon","new contact","packet territory","new mac in the ao","signal acquired","another ap","rf contact","beacon locked"};
    static const char* wifi5[]={"five-g contact","upper band lit","5ghz acquired","high-band traffic","five-g on scope","upper spectrum awake","clean 5g hit","high band contact"};
    static const char* hidden[]={"ghost contact","hidden beacon","ssid went dark","anonymous signal","ghost on scope","quiet one found"};
    static const char* openq[]={"open signal","open network noted","no lock on that one","open beacon logged","security: wide open","mark the open one"};
    static const char* bleq[]={"ble contact","tiny radio found","bluetooth ping","ble on scope","short-range contact","new ble critter","advertisement logged","ble says hello"};
    static const char* distq[]={"new territory","sector secured","grid mapped","s3ct0r pwned","another square down","keep rolling","road recon logged","distance banked"};
    static const char* mileq[]={"target-rich sector","contact swarm","rf l00t secured","that is a haul","spectrum jackpot","ao is crawling","big recon pull","command noticed"};
    static const char* lostq[]={"nav went dark","gps smoke break","lost the map","nav offline","position ghosted","gps wandered off"};
    static const char* backq[]={"nav green","position reacquired","back on the map","gps is back","navigation restored","grid lock"};
    pick&=7u;
    switch(kind){
      case WardriveEventKind::Wifi5:return wifi5[pick];
      case WardriveEventKind::HiddenWifi:return hidden[pick%6u];
      case WardriveEventKind::OpenWifi:return openq[pick%6u];
      case WardriveEventKind::Ble:return bleq[pick];
      case WardriveEventKind::Distance:return distq[pick];
      case WardriveEventKind::Milestone:return mileq[pick];
      case WardriveEventKind::GpsLost:return lostq[pick%6u];
      case WardriveEventKind::GpsBack:return backq[pick%6u];
      default:return wifi24[pick];
    }
  }

  void updateBuddyBubbleCache(bool event,bool gpsOk){
    if(!wardrive) return;
    bool levelUp=wardrive->levelUpFlash();
    uint32_t idleSlot=wardrive->elapsedSeconds()/6u;
    if(gpsOk) buddyGpsWarningShown_=false;
    // Keep the first missing-GPS warning briefly, then suppress repeats.
    if(!gpsOk && !event && !levelUp && !buddyGpsWarningShown_){
      buddyGpsWarningShown_=true;
      buddyGpsWarningUntil_=millis()+3500u;
    }
    uint32_t key=(levelUp?0x80000000u:0u) ^ (event?wardrive->eventSerial():idleSlot) ^ ((uint32_t)gpsOk<<29) ^ ((uint32_t)wardrive->stats().level<<16);
    if(key==buddyChatterKey && buddyLine1.length()) return;
    buddyChatterKey=key; buddyMeta=""; String text;
    if(levelUp){
      text=String("pr0m0t3d: ")+wardrive->rankTitle();
      buddyMeta=wardrive->tierTitle();
    } else if(event){
      uint8_t pick=(uint8_t)((wardrive->eventSerial()*7u + wardrive->stats().xp + wardrive->stats().level*3u)&7u);
      if(pick==buddyLastPick) pick=(pick+3u)&7u; buddyLastPick=pick;
      text=eventQuip(wardrive->lastEventKind(),pick);
      if(wardrive->lastXpGain()) buddyMeta=String("+")+String(wardrive->lastXpGain())+" xp";
    } else if(!gpsOk){
      text="gps wandered off";
    } else {
      uint8_t tier=(uint8_t)min((uint16_t)9,(uint16_t)((wardrive->stats().level-1)/10));
      uint8_t pick=(uint8_t)((idleSlot*5u + wardrive->stats().xp/17u + tier*3u)&7u);
      if(pick==buddyLastPick) pick=(pick+1u)&7u; buddyLastPick=pick;
      text=tierIdleQuip(tier,pick);
    }
    splitBubbleText(text,buddyLine1,buddyLine2,16);
  }

  template <typename Gfx>
  void drawBuddyBubble(Gfx& d,uint16_t a,const BuddyPose& b,bool event,bool gpsOk,int yOffset=0){
    bool visible=buddyBubbleVisible(event,gpsOk);
    if(!visible){ bubbleCacheVisible=false; return; }
    updateBuddyBubbleCache(event,gpsOk);

    const bool bubbleLeft=b.x>92;
    uint8_t maxLen=buddyLine1.length(); if(buddyLine2.length()>maxLen) maxLen=buddyLine2.length(); if(buddyMeta.length()>maxLen) maxLen=buddyMeta.length();
    int bw=constrain((int)maxLen*6+12,54,110);
    int lines=1+(buddyLine2.length()?1:0)+(buddyMeta.length()?1:0); int bh=10+lines*9;
    int bx=bubbleLeft?3:237-bw; int by=45-yOffset; if(!bubbleLeft && bx<127) bx=127;
    // Bubble is deliberately last in the layer order and capped above the road
    // HUD, so mortar/spark/tank animations can never clip through the text.
    if(by+bh>98-yOffset) by=98-yOffset-bh;
    bubbleCacheVisible=true; bubbleCacheX=bx; bubbleCacheY=by; bubbleCacheW=bw; bubbleCacheH=bh;
    uint16_t bubbleBg=0x0841;
    d.fillRoundRect(bx,by,bw,bh,4,bubbleBg); d.drawRoundRect(bx,by,bw,bh,4,a);
    int tailY=by+bh/2;
    if(bubbleLeft) d.fillTriangle(bx+bw,tailY-3,bx+bw+7,tailY,bx+bw,tailY+3,a);
    else d.fillTriangle(bx,tailY-3,bx-7,tailY,bx,tailY+3,a);
    d.setTextColor(TFT_WHITE,bubbleBg); int yy=by+5; d.setCursor(bx+6,yy); d.print(buddyLine1); yy+=9;
    if(buddyLine2.length()){ d.setCursor(bx+6,yy); d.print(buddyLine2); yy+=9; }
    if(buddyMeta.length()){ d.setTextColor(a,bubbleBg); d.setCursor(bx+6,yy); d.print(buddyMeta); }
  }

  template <typename Gfx>
  void drawWardriveCompactStatus(Gfx& d,uint16_t a){
    if(!wardrive) return;
    // One status row: session-unique 2.4 GHz, 5 GHz and BLE contacts + GPS.
    // Preserve the session counters and discovery logic; this is display-only.
    const auto& st=wardrive->stats();
    const GpsFix fix=gps?gps->fix():GpsFix{};
    const char* gpsState=fix.valid?"F":(gps && gps->everReceived()?"W":"-");
    char line[64];
    char band5[14];
    char bleCount[14];
    if(wardrive->uses5()) snprintf(band5,sizeof(band5),"%lu",(unsigned long)st.unique5);
    else snprintf(band5,sizeof(band5),"--");
    if(wardrive->usesBle()) snprintf(bleCount,sizeof(bleCount),"%lu",(unsigned long)st.uniqueBle);
    else snprintf(bleCount,sizeof(bleCount),"--");
    // BLE is intentionally explicit, rather than the ambiguous 'B:'.
    // Counts are session unique, not numbers of scan callbacks or repeat XP.
    snprintf(line,sizeof(line),"2.4:%lu 5:%s BLE:%s GPS:%s%u",
             (unsigned long)st.unique24,band5,bleCount,gpsState,
             (unsigned)(fix.valid?fix.sats:0u));
    if(d.textWidth(line)>232){
      snprintf(line,sizeof(line),"2:%lu 5:%s BLE:%s G:%s%u",
               (unsigned long)st.unique24,band5,bleCount,gpsState,
               (unsigned)(fix.valid?fix.sats:0u));
    }
    if(d.textWidth(line)>232){
      // Very large counters: prefer an explicit BLE label to clipped telemetry.
      snprintf(line,sizeof(line),"2:%lu 5:%s BLE:%s",
               (unsigned long)st.unique24,band5,bleCount);
    }
    d.fillRect(0,100,240,17,TFT_BLACK);
    d.drawFastHLine(0,100,240,accentDark());
    d.setTextColor(a,TFT_BLACK);
    d.setCursor(4,104);
    d.print(line);
  }

  template <typename Gfx>
  void drawIncomingMortars(Gfx& d,uint16_t a,size_t density,int yOffset=0){
    if(density<10) return;
    uint8_t intensity=density>=30?3:(density>=18?2:1); uint32_t tick=millis()/75u;
    for(uint8_t i=0;i<intensity;i++){
      uint32_t phase=(tick+i*19u)%64u; int x=28+(int)((i*71u + tick*3u)%188u);
      if(phase<46u){ int y=45+(int)phase-yOffset; d.drawLine(x+4,y-5,x,y,a); d.drawPixel(x+5,y-6,TFT_WHITE); }
      else if(phase<53u){ int y=92-yOffset; d.drawFastHLine(x-4,y,9,a); d.drawFastVLine(x,y-4,9,TFT_WHITE); d.drawLine(x-3,y-3,x+3,y+3,a); d.drawLine(x+3,y-3,x-3,y+3,a); }
    }
  }

  template <typename Gfx>
  void drawWardriveWorld(Gfx& d,uint16_t a,bool movingRight,int yOffset=0){
    // Time-based three-layer parallax. This intentionally keeps moving every
    // frame so the buddy reads as travelling over terrain, not hovering over it.
    const uint32_t tick=millis()/50u;
    const int dir=movingRight?-1:1;
    const int farOff=(dir*(int)tick)%40;
    const int midOff=(dir*(int)tick*3)%48;
    const int nearOff=(dir*(int)tick*7)%42;

    // Far radio skyline.
    for(int i=-2;i<10;i++){
      int x=i*40+farOff;
      int h=4+((i*11+(int)(tick/8))&7);
      d.drawFastVLine(x,77-h-yOffset,h,accentDark());
      if((i&1)==0) d.drawPixel(x,75-h-yOffset,a);
    }

    // Midground hills/bumps move visibly underneath the chassis.
    int px=-48+midOff;
    int lastX=px,lastY=91-yOffset;
    for(int n=0;n<9;n++){
      int x=px+n*32;
      int y=88-yOffset + ((n+(tick/6))%3)*3;
      d.drawLine(lastX,lastY,x,y,accentDark());
      d.drawLine(x,y,x+16,91+(n&1)*2-yOffset,a);
      lastX=x+16; lastY=91+(n&1)*2-yOffset;
    }
    d.drawFastHLine(0,96-yOffset,240,accentDark());

    // Fast foreground road/tread marks.
    for(int x=-42+nearOff;x<282;x+=42){
      d.drawFastHLine(x,103-yOffset,18,a);
      d.drawFastHLine(x+23,106-yOffset,10,0x7BEF);
    }
    // Passing rocks and signs reinforce travel direction.
    int landmark=(dir*(int)tick*4 + 300)%330-35;
    if(landmark>=0 && landmark<238){
      if(((tick/40)&1)==0){
        d.drawFastVLine(landmark,79-yOffset,17,0x7BEF); d.drawFastHLine(landmark-4,80-yOffset,9,0x7BEF); d.drawPixel(landmark+5,82-yOffset,a);
      }else{
        d.fillTriangle(landmark,95-yOffset,landmark+4,88-yOffset,landmark+8,95-yOffset,accentDark());
      }
    }
  }

  template <typename Gfx>
  void wardriveTank(Gfx& d,uint16_t a,bool motionOnly=false,int yOffset=0){
    if(!wardrive||!wardrive->running()){ d.setTextColor(a,TFT_BLACK); d.setCursor(5,28); d.print("wardrive stopped"); d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,50); d.print("return to wardrive menu to start"); return; }

    GpsFix fix=gps?gps->fix():GpsFix{};
    bool event=wardrive->discoveryFlash();
    BuddyPose pose=buddyPose(); size_t density=freshWardriveWifiCount();

    drawWardriveWorld(d,a,pose.right,yOffset);
    drawIncomingMortars(d,a,density,yOffset);
    drawAsciiBuddy(d,a,pose,event,fix.valid,density,yOffset);
    drawBuddyBubble(d,a,pose,event,fix.valid,yOffset);

    if(!motionOnly) drawWardriveCompactStatus(d,a);
  }

  size_t wdTargetCount() const { size_t n=0; for(size_t i=0;i<targets->count();++i){ auto*t=targets->at(i); if(t&&(t->band==Band::Band24||t->band==Band::Band5)) n++; } return n; }
  int wdTargetAt(size_t ord) const { size_t n=0; for(size_t i=0;i<targets->count();++i){ auto*t=targets->at(i); if(t&&(t->band==Band::Band24||t->band==Band::Band5)){ if(n==ord)return (int)i; n++; }} return -1; }
  void handleWardriveList(const InputEvent& e){
    size_t n=wdTargetCount(); if(e.action==InputAction::Up&&n)wdListSel=(wdListSel+n-1)%n; else if(e.action==InputAction::Down&&n)wdListSel=(wdListSel+1)%n;
    else if(e.action==InputAction::Select&&n&&wardrive){ int idx=wdTargetAt(wdListSel); if(idx>=0)wardrive->toggleSave(targets->at(idx)->id); }
    else if(e.action==InputAction::Left) enterScreen(23); else if(e.action==InputAction::Right) enterScreen(25);
  }
  void wardriveList(lgfx::LGFXBase& d,uint16_t a){
    if(wardrive&&wardrive->running()) wdHeader(d,a); else { d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("wardrive // discoveries"); }
    size_t n=wdTargetCount(); size_t first=wdListSel>4?wdListSel-4:0;
    for(size_t r=0;r<6&&first+r<n;r++){ int idx=wdTargetAt(first+r); auto*t=idx>=0?targets->at(idx):nullptr; if(!t)continue; int y=39+r*13; bool on=first+r==wdListSel; if(on)d.fillRoundRect(3,y-2,234,12,2,a); d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(5,y); bool save=!wardrive||wardrive->shouldSave(t->id); d.printf("%c %-14.14s %s %4d",save?'*':'-',t->ssid[0]?t->ssid:"<hidden>",t->band==Band::Band5?"5g":"24",t->rssi); }
  }
  void handleWardriveMap(const InputEvent& e){ if(e.action==InputAction::Left)enterScreen(24); else if(e.action==InputAction::Right)enterScreen(26); }
  void wardriveMap(lgfx::LGFXBase& d,uint16_t a){
    if(wardrive&&wardrive->running())wdHeader(d,a); d.setTextColor(a,TFT_BLACK); d.setCursor(5,40); d.print("grid map // breadcrumb");
    for(int x=8;x<232;x+=24)d.drawFastVLine(x,53,52,accentDark()); for(int y=53;y<106;y+=13)d.drawFastHLine(8,y,224,accentDark());
    GpsFix g=gps?gps->fix():GpsFix{}; if(g.valid){ int px=120+(int)(g.lon*1000)%90,py=79+(int)(g.lat*1000)%20; px=constrain(px,15,225);py=constrain(py,58,101); d.fillCircle(px,py,3,a); }
    d.setTextColor(0xC618,TFT_BLACK); d.setCursor(9,107); if(g.valid)d.printf("%.5f %.5f sats:%u",g.lat,g.lon,g.sats);else d.print("waiting for gps fix");
  }
  void handleWardriveStats(const InputEvent& e){ if(e.action==InputAction::Left)enterScreen(25); else if(e.action==InputAction::Right)enterScreen(23); }
  void wardriveStats(lgfx::LGFXBase& d,uint16_t a){
    auto st=wardrive?wardrive->stats():WardriveStats{}; WardriveCareerStats cs=wardrive?wardrive->career():WardriveCareerStats{};
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("operator dossier // wardrive");
    d.setTextColor(TFT_WHITE,TFT_BLACK); String title=wardrive?String(wardrive->rankTitle()):String("n00b Recruit"); if(title.length()>23)title=title.substring(0,23);
    d.setCursor(7,38); d.printf("L%u %s",(unsigned)st.level,title.c_str());
    d.setTextColor(a,TFT_BLACK); d.setCursor(7,50); if(wardrive)d.print(wardrive->tierTitle());
    d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(7,62); if(wardrive && st.level<100)d.printf("xp %lu / %lu",(unsigned long)st.xp,(unsigned long)wardrive->nextLevelXp());else d.printf("xp %lu  prestige",(unsigned long)st.xp);
    d.setCursor(7,74); d.printf("career ap %lu  ble %lu",(unsigned long)(cs.wifi24+cs.wifi5),(unsigned long)cs.ble);
    d.setCursor(7,86); d.print("career "); d.print(distanceText(cs.distanceM)); d.printf("  runs %lu",(unsigned long)cs.sessions);
    d.setCursor(7,98); d.printf("this run ap %lu ble %lu",(unsigned long)(st.unique24+st.unique5),(unsigned long)st.uniqueBle);
    d.setCursor(7,110); d.print("run "); d.print(distanceText(st.distanceM)); d.printf("  best ap %lu",(unsigned long)cs.bestWifiSession);
  }
  void handleWardriveProviders(const InputEvent& e){ (void)e; }
  void wardriveProviders(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("wardrive // providers"); d.setTextColor(TFT_WHITE,TFT_BLACK);
    d.setCursor(8,42);d.print("wigle      ");d.print(wardrive&&wardrive->providerConfigured("wigle_token")?"configured":"not set");
    d.setCursor(8,57);d.print("wdgwars    ");d.print(wardrive&&wardrive->providerConfigured("wdgwars_key")?"configured":"not set");
    d.setCursor(8,72);d.print("wardrivego export: wigle csv"); d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(8,91);d.print("sd: /dirk/wardrive/config/"); d.setCursor(8,104);d.print("providers.ini (portal ui next pass)");
  }

  void loadBrowser(const char* path,const char* title,bool dirsOnly=false,const char* ext=nullptr){
    browserCount=0; browserSel=0; browserDeleteConfirm=false; browserTitle=title?title:"files";
    if(!storage || !storage->mounted()) return;
    File dir=SD.open(path); if(!dir || !dir.isDirectory()){ if(dir) dir.close(); return; }
    File f=dir.openNextFile();
    while(f && browserCount<MAX_BROWSER){
      String nm=f.name(); int slash=nm.lastIndexOf('/'); if(slash>=0) nm=nm.substring(slash+1); bool isDir=f.isDirectory(); bool keep=!dirsOnly || isDir;
      if(ext && !isDir){ String low=nm; low.toLowerCase(); String ex=ext; ex.toLowerCase(); keep=low.endsWith(ex); }
      if(keep){
        BrowserEntry &e=browser[browserCount++];
        String full=String(path); if(!full.endsWith("/")) full+="/"; full+=nm;
        strlcpy(e.name,nm.c_str(),sizeof(e.name)); strlcpy(e.path,full.c_str(),sizeof(e.path)); e.size=(uint32_t)f.size(); e.dir=isDir;
      }
      f.close(); f=dir.openNextFile();
    }
    dir.close();
  }

  void drawBrowserList(lgfx::LGFXBase& d,uint16_t a,const char* title){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print(title);
    d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(200,23); d.printf("%u",(unsigned)browserCount);
    if(!storage||!storage->mounted()){ d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,48); d.print("sd card not mounted"); return; }
    if(!browserCount){ d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,48); d.print("no entries"); return; }
    uint8_t first=browserSel>5?browserSel-5:0;
    for(uint8_t row=0;row<6 && first+row<browserCount;row++){
      uint8_t i=first+row; int y=39+row*13; bool on=i==browserSel; BrowserEntry &e=browser[i];
      if(on)d.fillRoundRect(3,y-2,234,12,2,a); d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(6,y);
      String n=e.name; if(n.length()>23)n=n.substring(0,23); d.print(e.dir?"[d] ":"    "); d.print(n);
      if(!e.dir){ String sz=e.size>1024?String(e.size/1024)+"k":String(e.size)+"b"; d.setCursor(235-d.textWidth(sz),y); d.print(sz); }
    }
    if(first>0){d.setTextColor(a,TFT_BLACK);d.setCursor(229,26);d.print("^");}
    if(first+6<browserCount){d.setTextColor(a,TFT_BLACK);d.setCursor(229,108);d.print("v");}
  }

  void browserMove(const InputEvent& e){
    if(e.action==InputAction::Up && browserCount) browserSel=(browserSel+browserCount-1)%browserCount;
    else if(e.action==InputAction::Down && browserCount) browserSel=(browserSel+1)%browserCount;
  }
  void openBrowserEntry(uint8_t returnScreen){
    if(!browserCount) return; BrowserEntry &e=browser[browserSel];
    if(e.dir && returnScreen==8){ filePath=e.path; loadBrowser(filePath.c_str(),"files",false); return; }
    browserReturnScreen=returnScreen; browserDeleteConfirm=false; enterScreen(38);
  }
  void browserParent(){
    int slash=filePath.lastIndexOf('/'); if(slash<=0){ filePath="/dirk"; } else { filePath=filePath.substring(0,slash); if(filePath.length()<5) filePath="/dirk"; }
    loadBrowser(filePath.c_str(),"files",false); dirty=true;
  }
  bool isTextFile(const String& p) const { String q=p; q.toLowerCase(); return q.endsWith(".txt")||q.endsWith(".json")||q.endsWith(".jsonl")||q.endsWith(".csv")||q.endsWith(".ini")||q.endsWith(".log"); }

  void handleDataMenu(const InputEvent& e){
    if(e.action==InputAction::Up) dataSel=(dataSel+3)%4;
    else if(e.action==InputAction::Down) dataSel=(dataSel+1)%4;
    else if(e.action==InputAction::Select) openDataItem(dataSel);
  }
  void dataMenu(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("data // center");
    static const char* rows[]={"captures", "audit reports", "wd sessions", "sd files"};
    for(uint8_t i=0;i<4;i++){
      int y=42+i*17; const bool on=i==dataSel;
      if(on) d.fillRoundRect(4,y-2,116,14,2,a);
      d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK);
      d.setCursor(9,y); d.print(rows[i]);
    }
    sectionPanel(d,a,4);
  }

  void handleCaptures(const InputEvent& e){ browserMove(e); if(e.action==InputAction::Select) openBrowserEntry(4); else if(e.action==InputAction::Tab) loadBrowser("/dirk/captures","captures",false); }
  void capturesScreen(lgfx::LGFXBase& d,uint16_t a){ drawBrowserList(d,a,"captures // saved pcaps"); }
  void handleAudits(const InputEvent& e){ browserMove(e); if(e.action==InputAction::Select) openBrowserEntry(5); else if(e.action==InputAction::Tab) loadBrowser("/dirk/targets","audits",false,".txt"); }
  void auditsScreen(lgfx::LGFXBase& d,uint16_t a){ drawBrowserList(d,a,"audits // target reports"); if(!browserCount){d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(8,65);d.print("save report from target detail");} }
  void handleSessions(const InputEvent& e){ browserMove(e); if(e.action==InputAction::Select) openBrowserEntry(7); else if(e.action==InputAction::Tab) loadBrowser("/dirk/wardrive/sessions","sessions",true); }
  void sessionsScreen(lgfx::LGFXBase& d,uint16_t a){ drawBrowserList(d,a,"sessions // wardrive runs"); }
  void handleFiles(const InputEvent& e){ browserMove(e); if(e.action==InputAction::Select) openBrowserEntry(8); else if(e.action==InputAction::Tab) loadBrowser(filePath.c_str(),"files",false); }
  void filesScreen(lgfx::LGFXBase& d,uint16_t a){ String t="files // "+filePath; if(t.length()>34)t="files // ..."+t.substring(t.length()-24); drawBrowserList(d,a,t.c_str()); }

  void handleBrowserDetail(const InputEvent& e){
    if(!browserCount) return; BrowserEntry &be=browser[browserSel];
    if(e.action==InputAction::Tab && !be.dir){
      if(!browserDeleteConfirm){ browserDeleteConfirm=true; }
      else { SD.remove(be.path); browserDeleteConfirm=false; uint8_t ret=browserReturnScreen; if(ret==4)loadBrowser("/dirk/captures","captures",false); else if(ret==5)loadBrowser("/dirk/targets","audits",false,".txt"); else if(ret==7)loadBrowser("/dirk/wardrive/sessions","sessions",true); else loadBrowser(filePath.c_str(),"files",false); enterScreen(ret); }
    }
  }
  void browserDetail(lgfx::LGFXBase& d,uint16_t a){
    if(!browserCount)return; BrowserEntry &be=browser[browserSel];
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("file // detail");
    d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(7,39);String n=be.name;if(n.length()>34)n=n.substring(0,34);d.print(n);
    d.setCursor(7,51);d.printf("%s  %lu bytes",be.dir?"folder":"file",(unsigned long)be.size);
    String p=be.path;if(p.length()>36)p="..."+p.substring(p.length()-33);d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(7,63);d.print(p);
    if(be.dir && browserReturnScreen==7){
      String sp=String(be.path)+"/stats.json"; if(SD.exists(sp)){File f=SD.open(sp);if(f){String line=f.readStringUntil('\n');f.close();if(line.length()>38)line=line.substring(0,38);d.setCursor(7,79);d.print(line);}}
      String wp=String(be.path)+"/wigle.csv"; d.setCursor(7,93); d.print(SD.exists(wp)?"wigle.csv ready":"no wigle.csv");
    } else if(!be.dir && isTextFile(be.path)){
      File f=SD.open(be.path); int y=77; if(f){for(int row=0;row<3&&f.available();row++){String line=f.readStringUntil('\n');line.trim();if(line.length()>36)line=line.substring(0,36);d.setCursor(7,y);d.print(line);y+=11;}f.close();}
    } else if(!be.dir){ d.setCursor(7,82); d.print("binary capture / export on sd"); }
    if(browserDeleteConfirm){ d.setTextColor(0xF800,TFT_BLACK); d.setCursor(7,105); d.print("tab again confirms delete"); }
  }


  // ---------------------------------------------------------------------------
  // Infrared subsystem (v0.6.5)
  // Cardputer ADV provides a built-in IR emitter on GPIO44. Receive/analyze
  // requires an external receiver; this module intentionally does not pretend
  // the onboard hardware can receive IR.
  // ---------------------------------------------------------------------------
  void refreshIrFiles(){
    irFileCount=0; if(!storage || !storage->mounted() || !SD.exists("/dirk/ir")) return;
    File root=SD.open("/dirk/ir"); if(!root || !root.isDirectory()){ if(root)root.close(); return; }
    File f=root.openNextFile();
    while(f && irFileCount<MAX_IR_FILES){
      if(!f.isDirectory()){
        String full=f.name(); String low=full; low.toLowerCase();
        if(low.endsWith(".ir")){
          String base=full; int slash=base.lastIndexOf('/'); if(slash>=0)base=base.substring(slash+1);
          strlcpy(irFiles[irFileCount].name,base.c_str(),sizeof(irFiles[irFileCount].name));
          String path=full.startsWith("/")?full:String("/dirk/ir/")+base;
          strlcpy(irFiles[irFileCount].path,path.c_str(),sizeof(irFiles[irFileCount].path)); irFileCount++;
        }
      }
      f.close(); f=root.openNextFile();
    }
    root.close(); if(irFileSel>=irFileCount)irFileSel=0;
  }

  void handleIrMenu(const InputEvent& e){
    static constexpr uint8_t N=3;
    if(e.action==InputAction::Up) irMenuSel=(irMenuSel+N-1)%N; else if(e.action==InputAction::Down) irMenuSel=(irMenuSel+1)%N;
    else if(e.action==InputAction::Select){ static const uint8_t screens[]={61,62,63}; enterScreen(screens[irMenuSel]); }
  }
  template <typename Gfx>
  void irMenu(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("infrared // tools");
    static const char* rows[]={"NEC transmitter","SD .ir remotes","diagnostics"};
    for(uint8_t i=0;i<3;i++){ int y=43+i*17; bool on=i==irMenuSel; if(on)d.fillRoundRect(4,y-2,116,14,2,a); d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(9,y); d.print(rows[i]); }
    sectionPanel(d,a,9);
  }

  void handleIrNec(const InputEvent& e){
    if(e.action==InputAction::Left) irCommand--; else if(e.action==InputAction::Right) irCommand++;
    else if(e.action==InputAction::Up) irCommand=(uint8_t)(irCommand+16); else if(e.action==InputAction::Down) irCommand=(uint8_t)(irCommand-16);
    else if(e.action==InputAction::Tab) irAddress=(uint16_t)((irAddress+1u)&0x00FFu);
    else if(e.action==InputAction::Select && ir){ bool ok=ir->sendNec(irAddress,irCommand); irUiStatus=ok?"NEC frame sent":ir->lastStatus(); }
  }
  void irNec(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("infrared // NEC transmit");
    d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,43); d.printf("TX GPIO 44   %s",ir&&ir->ready()?"ready":"unavailable");
    d.setCursor(8,59); d.printf("address  0x%02X",(unsigned)(irAddress&0xFFu));
    d.setCursor(8,74); d.printf("command  0x%02X",(unsigned)irCommand);
    d.setCursor(8,89); d.print("carrier  38 kHz / NEC");
    d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(8,104); d.print(irUiStatus.length()?irUiStatus.substring(0,35):"point emitter at your own device");
  }

  void handleIrFiles(const InputEvent& e){
    if(e.action==InputAction::Up && irFileCount) irFileSel=(irFileSel+irFileCount-1)%irFileCount;
    else if(e.action==InputAction::Down && irFileCount) irFileSel=(irFileSel+1)%irFileCount;
    else if(e.action==InputAction::Tab) refreshIrFiles();
    else if(e.action==InputAction::Select && irFileCount && ir){ String sig; bool ok=ir->sendFlipperFirst(irFiles[irFileSel].path,&sig); irUiStatus=ok?(String("sent ")+sig):ir->lastStatus(); }
  }
  void irFilesScreen(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("infrared // SD remotes"); d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(205,23); d.printf("%u",irFileCount);
    if(!storage||!storage->mounted()){ d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,48); d.print("SD unavailable"); return; }
    if(!irFileCount){ d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,45); d.print("no .ir files in /dirk/ir"); d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(8,61); d.print("supports first NEC or raw signal"); return; }
    uint8_t first=irFileSel>4?irFileSel-4:0;
    for(uint8_t r=0;r<5 && first+r<irFileCount;r++){ uint8_t i=first+r; int y=39+r*13; bool on=i==irFileSel; if(on)d.fillRoundRect(3,y-2,234,12,2,a); d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(7,y); String nm=irFiles[i].name; if(nm.length()>32)nm=nm.substring(0,32); d.print(nm); }
    d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(7,106); d.print(irUiStatus.length()?irUiStatus.substring(0,36):"Flipper-style parsed NEC + raw");
  }

  void handleIrDiagnostics(const InputEvent& e){ (void)e; }
  void irDiagnostics(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("infrared // diagnostics"); d.setTextColor(TFT_WHITE,TFT_BLACK);
    d.setCursor(8,42); d.printf("built-in TX   GPIO44  %s",ir&&ir->ready()?"OK":"--");
    d.setCursor(8,57); d.printf("carrier       %lu Hz",(unsigned long)(ir?ir->carrierHz():0));
    d.setCursor(8,72); d.printf("duty          %u%%",(unsigned)(ir?ir->dutyPercent():0));
    d.setCursor(8,87); d.printf("frames sent   %lu",(unsigned long)(ir?ir->sentCount():0));
    d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(8,103); d.print("IR receive requires external RX hardware");
  }


  // ---------------------------------------------------------------------------
  // LoRa / mesh subsystem (v0.6.5)
  // ---------------------------------------------------------------------------
  void handleLoRaMenu(const InputEvent& e){
    static constexpr uint8_t N=9;
    if(e.action==InputAction::Up) loraMenuSel=(loraMenuSel+N-1)%N;
    else if(e.action==InputAction::Down) loraMenuSel=(loraMenuSel+1)%N;
    else if(e.action==InputAction::Select){
      static const uint8_t screens[]={48,41,49,50,43,51,44,45,46};
      enterScreen(screens[loraMenuSel]);
    }
  }

  template <typename Gfx>
  void loraMenu(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("lora // mesh");
    static const char* rows[]={"nearby mesh","chats / inbox","contacts","channels","positions","networks","range test","radio settings","diagnostics"};
    const uint8_t N=9; uint8_t first=loraMenuSel>5?loraMenuSel-5:0;
    for(uint8_t r=0;r<6 && first+r<N;r++){
      uint8_t i=first+r; int y=40+r*13; bool on=i==loraMenuSel;
      if(on)d.fillRoundRect(4,y-2,116,12,2,a);
      d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(9,y); d.print(rows[i]);
    }
    if(first>0){d.setTextColor(a,TFT_BLACK);d.setCursor(114,27);d.print("^");}
    if(first+6<N){d.setTextColor(a,TFT_BLACK);d.setCursor(114,108);d.print("v");}
    sectionPanel(d,a,3);
  }

  void handleLoRaChat(const InputEvent& e){
    if(e.action==InputAction::Select){ loraCompose=""; loraComposeDirect=false; loraComposeTarget=0; loraTextEntry=true; return; }
    if(e.action==InputAction::Character){ loraCompose=""; loraComposeDirect=false; loraComposeTarget=0; loraCompose+=e.ch; loraTextEntry=true; return; }
    if(e.action==InputAction::Tab && lora) lora->sendNodeBeacon();
  }

  String shortNode(uint32_t id) const { char b[7]; snprintf(b,sizeof(b),"%04lx",(unsigned long)(id&0xFFFFu)); return String(b); }

  template <typename Gfx>
  void loraChat(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("lora // chats / inbox");
    if(!lora || !lora->ready()){
      d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,47); d.print("sx1262 unavailable");
      d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(8,62); d.print("check cap + antenna + diagnostics"); return;
    }
    size_t n=lora->messageCount();
    if(!n){ d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,48); d.print("no dirk mesh messages yet"); d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(8,64); d.print("type or enter to compose"); }
    size_t start=n>5?n-5:0; int y=38;
    for(size_t i=start;i<n;i++){
      const LoRaMessage* m=lora->messageAt(i); if(!m) continue;
      bool mine=m->outgoing; String who; if(mine) who=m->to?String("me>")+shortNode(m->to):"me"; else who=m->to?shortNode(m->from)+">me":shortNode(m->from); String txt=m->text;
      if(txt.length()>25) txt=txt.substring(0,25);
      String shown=String("D ")+txt; int w=min(222, max(66, d.textWidth(shown)+12)); int x=mine?235-w:5;
      d.fillRoundRect(x,y-2,w,14,3,mine?accentDark():0x1082); d.drawRoundRect(x,y-2,w,14,3,mine?a:0x7BEF);
      d.setTextColor(TFT_WHITE,mine?accentDark():0x1082); d.setCursor(x+5,y); d.print(shown);
      d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(mine?202:7,y+13); d.print(who);
      y+=18; if(y>105) break;
    }
  }

  void handleLoraText(const InputEvent& e){
    if(e.action==InputAction::Back){
      if(loraCompose.length()) loraCompose.remove(loraCompose.length()-1); else { loraTextEntry=false; loraComposeDirect=false; loraComposeTarget=0; }
    } else if(e.action==InputAction::Select){
      if(loraCompose.length() && lora){
        if(loraComposeDirect && loraComposeTarget) lora->sendDirect(loraComposeTarget,loraCompose,lora->hopLimit());
        else lora->sendChat(loraCompose,lora->hopLimit());
        lastLoraMessageSerial=lora->messageSerial();
      }
      loraCompose=""; loraTextEntry=false; loraComposeDirect=false; loraComposeTarget=0;
    } else if(e.action==InputAction::Character && e.ch>=32 && e.ch<=126 && loraCompose.length()<100){
      loraCompose+=e.ch;
    }
    dirty=true;
  }

  template <typename Gfx>
  void drawLoraTextEntry(Gfx& d,uint16_t a){
    d.fillRoundRect(7,69,226,43,5,0x1082); d.drawRoundRect(7,69,226,43,5,a);
    d.setTextColor(a,0x1082); d.setCursor(13,75); if(loraComposeDirect){ d.print("direct > "); d.print(shortNode(loraComposeTarget)); } else d.print("dirk mesh broadcast");
    String line=loraCompose; if(line.length()>33) line="..."+line.substring(line.length()-30);
    d.setTextColor(TFT_WHITE,0x1082); d.setCursor(13,89); d.print(line); d.print("_");
    d.setTextColor(0xBDF7,0x1082); d.setCursor(13,102); d.print("enter send   bksp edit");
  }

  void handleLoRaNodes(const InputEvent& e){
    size_t n=lora?lora->nodeCount():0;
    if(e.action==InputAction::Up && n) loraNodeSel=(loraNodeSel+n-1)%n;
    else if(e.action==InputAction::Down && n) loraNodeSel=(loraNodeSel+1)%n;
    else if(e.action==InputAction::Tab && lora) lora->sendNodeBeacon();
  }

  template <typename Gfx>
  void loraNodes(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("lora // nodes");
    size_t n=lora?lora->nodeCount():0; d.setCursor(202,23); d.printf("%u",(unsigned)n);
    if(!n){d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(8,48);d.print("waiting for mesh nodes...");return;}
    if(loraNodeSel>=n)loraNodeSel=0; size_t first=loraNodeSel>4?loraNodeSel-4:0;
    for(size_t r=0;r<6&&first+r<n;r++){
      auto* node=lora->nodeAt(first+r); if(!node)continue; int y=39+r*13; bool on=first+r==loraNodeSel;
      if(on)d.fillRoundRect(3,y-2,234,12,2,a); d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(6,y);
      d.printf("%-15.15s %4d %4.1f",node->name,node->rssi,node->snr);
      if(node->hasPosition){ d.setCursor(221,y); d.print("+"); }
    }
  }

  void handleLoRaPosition(const InputEvent& e){
    if(!lora)return;
    if(e.action==InputAction::Left || e.action==InputAction::Right) lora->setPositionBeacon(!lora->positionBeacon());
    else if(e.action==InputAction::Select) lora->sendPosition(lora->hopLimit());
  }

  template <typename Gfx>
  void loraPosition(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("lora // position");
    GpsFix f=gps?gps->fix():GpsFix{}; d.setTextColor(TFT_WHITE,TFT_BLACK);
    d.setCursor(8,42); d.printf("gps %s  sats %u",f.valid?"fix":"--",f.sats);
    d.setCursor(8,57); if(f.valid)d.printf("lat %.6f",f.lat); else d.print("lat --");
    d.setCursor(8,72); if(f.valid)d.printf("lon %.6f",f.lon); else d.print("lon --");
    d.setCursor(8,87); d.printf("beacon %s  hops %u",lora&&lora->positionBeacon()?"on":"off",lora?lora->hopLimit():0);
    d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(8,104); d.print("enter sends position now");
  }

  void handleLoRaRange(const InputEvent& e){ if(e.action==InputAction::Select && lora) lora->sendRangePing(); }
  template <typename Gfx>
  void loraRange(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("lora // range test"); d.setTextColor(TFT_WHITE,TFT_BLACK);
    if(!lora||!lora->ready()){d.setCursor(8,48);d.print("radio unavailable");return;}
    d.setCursor(8,44); d.printf("pings sent     %lu",(unsigned long)lora->rangeSent());
    d.setCursor(8,59); d.printf("replies        %lu",(unsigned long)lora->rangeReplies());
    d.setCursor(8,74); d.printf("last rssi      %d dbm",lora->rangeLastRssi());
    d.setCursor(8,89); d.printf("last snr       %.1f db",lora->rangeLastSnr());
    d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(8,104); d.print("enter sends one bounded ping");
  }

  void handleLoRaSettings(const InputEvent& e){
    static constexpr uint8_t N=6; if(!lora)return;
    if(e.action==InputAction::Up) loraSettingSel=(loraSettingSel+N-1)%N;
    else if(e.action==InputAction::Down) loraSettingSel=(loraSettingSel+1)%N;
    else if(e.action==InputAction::Left || e.action==InputAction::Right || e.action==InputAction::Select){
      int dir=e.action==InputAction::Left?-1:1;
      if(loraSettingSel==0){ int v=((int)lora->region()+dir+3)%3; lora->setRegion((LoRaRegion)v); }
      else if(loraSettingSel==1){ int v=((int)lora->preset()+dir+3)%3; lora->setPreset((LoRaPreset)v); }
      else if(loraSettingSel==2) lora->setTxPower(lora->txPower()+dir);
      else if(loraSettingSel==3) lora->setHopLimit((uint8_t)constrain((int)lora->hopLimit()+dir,0,5));
      else if(loraSettingSel==4) lora->setRelay(!lora->relayEnabled());
      else if(loraSettingSel==5) lora->setPositionBeacon(!lora->positionBeacon());
    }
  }

  template <typename Gfx>
  void loraSettings(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("lora // radio settings");
    const char* names[]={"region","modem preset","tx power","hop limit","mesh relay","gps beacon"};
    String vals[]={lora?lora->regionName():"--",lora?lora->presetName():"--",lora?String(lora->txPower())+"dbm":"--",lora?String(lora->hopLimit()):"--",lora&&lora->relayEnabled()?"on":"off",lora&&lora->positionBeacon()?"on":"off"};
    for(int i=0;i<6;i++){int y=39+i*13;bool on=i==loraSettingSel;if(on)d.fillRoundRect(3,y-2,234,12,2,a);d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK);d.setCursor(7,y);d.print(names[i]);d.setCursor(233-d.textWidth(vals[i]),y);d.print(vals[i]);}
  }

  void handleLoRaDiagnostics(const InputEvent& e){ if(e.action==InputAction::Select && lora) lora->sendNodeBeacon(); }
  template <typename Gfx>
  void loraDiagnostics(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("lora // diagnostics"); d.setTextColor(TFT_WHITE,TFT_BLACK);
    d.setCursor(8,40); d.printf("sx1262 %s  code %d",lora&&lora->ready()?"ready":"--",lora?lora->lastRadioCode():-999);
    d.setCursor(8,54); d.printf("%s %.3f mhz %s",lora?lora->regionName():"--",lora?lora->frequency():0,lora?lora->presetName():"--");
    d.setCursor(8,68); d.printf("tx %lu  rx %lu  relay %lu",lora?(unsigned long)lora->txCount():0,lora?(unsigned long)lora->rxCount():0,lora?(unsigned long)lora->relayCount():0);
    d.setCursor(8,82); d.printf("last %d dbm  snr %.1f",lora?lora->lastRssi():-127,lora?lora->lastSnr():0);
    if(lora && lora->lastError().length()){String er=lora->lastError();if(er.length()>34)er=er.substring(0,34);d.setCursor(8,96);d.print(er);}
    d.setTextColor(0xFBE0,TFT_BLACK); d.setCursor(8,108); d.print("antenna must be attached before tx");
  }

  size_t meshtasticAdvertCount() const { size_t n=0;if(!ble)return 0;for(size_t i=0;i<ble->count();i++){auto*t=ble->at(i);if(t&&t->meshtastic)n++;}return n; }
  const BleTarget* meshtasticAdvertAt(size_t ord) const { size_t n=0;if(!ble)return nullptr;for(size_t i=0;i<ble->count();i++){auto*t=ble->at(i);if(t&&t->meshtastic){if(n==ord)return t;n++;}}return nullptr; }

  void handleMeshtastic(const InputEvent& e){
    if(e.action==InputAction::Up) meshtasticSel=(meshtasticSel+1)%2;
    else if(e.action==InputAction::Down) meshtasticSel=(meshtasticSel+1)%2;
    else if(e.action==InputAction::Tab || (e.action==InputAction::Select && meshtasticSel==1)){
      if(ble && !ble->scanning() && (!wifi || !wifi->scanning())) ble->startScanAsync(2);
    }
  }

  template <typename Gfx>
  void meshtasticScreen(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("lora // meshtastic");
    const char* rows[]={"native upstream engine","companion node discovery"};
    for(int i=0;i<2;i++){int y=40+i*16;bool on=i==meshtasticSel;if(on)d.fillRoundRect(4,y-2,228,13,2,a);d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK);d.setCursor(9,y);d.print(rows[i]);}
    d.setTextColor(0x7BEF,TFT_BLACK);
    if(meshtasticSel==0){
      d.setCursor(8,77); d.print("full mesh/routing/pki uses the"); d.setCursor(8,89); d.print("included meshtastic-adv sidecar"); d.setCursor(8,101); d.print("build script in tools/");
    }else{
      size_t n=meshtasticAdvertCount(); d.setCursor(8,77); d.printf("meshtastic ble nodes: %u",(unsigned)n);
      const BleTarget* t=n?meshtasticAdvertAt(0):nullptr; d.setCursor(8,89); if(t)d.printf("%-21.21s %ddbm",t->name,t->rssi); else d.print("tab scans standard mesh service");
      d.setCursor(8,101); d.print("full companion chat: sidecar mode");
    }
  }

  const char* meshProtoName(uint8_t p) const { return p==MeshMeshtastic?"meshtastic":(p==MeshCore?"meshcore":"dirk mesh"); }
  char meshProtoLetter(uint8_t p) const { return p==MeshMeshtastic?'M':(p==MeshCore?'C':'D'); }

  String cleanMeshField(String v) const {
    v.replace("|","/"); v.replace("\n"," "); v.replace("\r"," ");
    if(v.length()>27) v=v.substring(0,27);
    return v;
  }

  void loadMeshContacts(){
    meshContactCount=0;
    meshAutoDiscover=meshPrefs.getBool("auto",true);
    String raw=meshPrefs.getString("contacts","");
    int pos=0;
    while(pos<(int)raw.length() && meshContactCount<MAX_MESH_CONTACTS){
      int nl=raw.indexOf('\n',pos); if(nl<0) nl=raw.length();
      String line=raw.substring(pos,nl); pos=nl+1;
      int a=line.indexOf('|'), b=line.indexOf('|',a+1);
      if(a<=0 || b<=a) continue;
      MeshContact &c=meshContacts[meshContactCount++]; c={};
      c.proto=(uint8_t)line.substring(0,a).toInt();
      strlcpy(c.key,line.substring(a+1,b).c_str(),sizeof(c.key));
      strlcpy(c.name,line.substring(b+1).c_str(),sizeof(c.name));
    }
  }

  void saveMeshContacts(){
    String raw;
    for(uint8_t i=0;i<meshContactCount;i++){
      raw+=String(meshContacts[i].proto)+"|"+String(meshContacts[i].key)+"|"+cleanMeshField(meshContacts[i].name)+"\n";
    }
    meshPrefs.putString("contacts",raw);
    if(storage && storage->writable()) storage->overwrite("/dirk/lora/contacts.csv",(const uint8_t*)raw.c_str(),raw.length());
  }

  int meshContactIndex(const char* key) const {
    if(!key || !*key) return -1;
    for(uint8_t i=0;i<meshContactCount;i++) if(strcmp(meshContacts[i].key,key)==0) return i;
    return -1;
  }
  bool meshIsFavorite(const char* key) const { return meshContactIndex(key)>=0; }

  void toggleMeshFavorite(const MeshNearbyItem& item){
    int idx=meshContactIndex(item.key);
    if(idx>=0){
      for(uint8_t i=(uint8_t)idx;i+1<meshContactCount;i++) meshContacts[i]=meshContacts[i+1];
      if(meshContactCount) meshContactCount--;
    }else if(meshContactCount<MAX_MESH_CONTACTS){
      MeshContact &c=meshContacts[meshContactCount++]; c={}; c.proto=item.proto;
      strlcpy(c.key,item.key,sizeof(c.key)); strlcpy(c.name,item.name,sizeof(c.name));
    }
    saveMeshContacts();
  }

  size_t meshNearbyCount() const {
    size_t n=lora?lora->nodeCount():0;
    if(ble) for(size_t i=0;i<ble->count();i++){ const BleTarget* t=ble->at(i); if(t && (t->meshtastic||t->meshcore)) n++; }
    return n;
  }

  bool meshNearbyAt(size_t ord,MeshNearbyItem& out) const {
    out={}; size_t dn=lora?lora->nodeCount():0;
    if(ord<dn){
      const LoRaNode* n=lora->nodeAt(ord); if(!n)return false;
      out.proto=MeshDirk; snprintf(out.key,sizeof(out.key),"D:%08lx",(unsigned long)n->id);
      strlcpy(out.name,n->name,sizeof(out.name)); out.rssi=n->rssi; out.snr=n->snr; out.lastSeen=n->lastSeen;
      out.hasPosition=n->hasPosition; out.lat=n->lat; out.lon=n->lon; out.favorite=meshIsFavorite(out.key); return true;
    }
    ord-=dn;
    if(ble){
      for(size_t i=0;i<ble->count();i++){
        const BleTarget* t=ble->at(i); if(!t || (!t->meshtastic && !t->meshcore)) continue;
        if(ord--==0){
          out.proto=t->meshtastic?MeshMeshtastic:MeshCore;
          snprintf(out.key,sizeof(out.key),"%c:%s",meshProtoLetter(out.proto),t->address);
          const char* fallback=t->meshtastic?"meshtastic node":"meshcore node";
          strlcpy(out.name,t->named?t->name:fallback,sizeof(out.name)); out.rssi=t->rssi; out.lastSeen=t->lastSeen;
          out.favorite=meshIsFavorite(out.key); return true;
        }
      }
    }
    return false;
  }

  bool meshFindLiveByKey(const char* key,MeshNearbyItem& out) const {
    size_t n=meshNearbyCount();
    for(size_t i=0;i<n;i++){ MeshNearbyItem x; if(meshNearbyAt(i,x) && strcmp(x.key,key)==0){out=x;return true;} }
    return false;
  }

  void refreshMeshProfile(){
    if(!meshProfile.key[0]) return;
    MeshNearbyItem live;
    if(meshFindLiveByKey(meshProfile.key,live)) meshProfile=live;
    meshProfile.favorite=meshIsFavorite(meshProfile.key);
  }

  size_t meshProtoBleCount(uint8_t proto) const {
    size_t n=0; if(!ble)return 0;
    for(size_t i=0;i<ble->count();i++){ const BleTarget* t=ble->at(i); if(!t)continue; if(proto==MeshMeshtastic&&t->meshtastic)n++; if(proto==MeshCore&&t->meshcore)n++; }
    return n;
  }

  void requestMeshBleScan(){
    lastMeshScanAt=millis();
    if(ble && !ble->scanning() && (!wifi || !wifi->scanning())) ble->startScanAsync(2);
  }

  void handleMeshNearby(const InputEvent& e){
    size_t n=meshNearbyCount();
    if(e.action==InputAction::Up && n) meshNearbySel=(meshNearbySel+n-1)%n;
    else if(e.action==InputAction::Down && n) meshNearbySel=(meshNearbySel+1)%n;
    else if(e.action==InputAction::Tab) requestMeshBleScan();
    else if(e.action==InputAction::Character && (e.ch=='f'||e.ch=='F') && n){ MeshNearbyItem x; if(meshNearbyAt(meshNearbySel,x)) toggleMeshFavorite(x); }
    else if(e.action==InputAction::Select && n){ if(meshNearbyAt(meshNearbySel,meshProfile)){ meshProfileReturn=48; enterScreen(52); } }
  }

  template <typename Gfx>
  void meshNearbyScreen(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK); d.setCursor(5,23); d.print("lora // nearby mesh");
    size_t n=meshNearbyCount(); d.setCursor(207,23); d.printf("%u",(unsigned)n);
    if(!n){ d.setTextColor(TFT_WHITE,TFT_BLACK); d.setCursor(8,47); d.print("listening for mesh peers..."); d.setTextColor(0x7BEF,TFT_BLACK); d.setCursor(8,62); d.print("tab scans meshtastic + meshcore ble"); return; }
    if(meshNearbySel>=n)meshNearbySel=0; size_t first=meshNearbySel>5?meshNearbySel-5:0;
    for(size_t r=0;r<6 && first+r<n;r++){
      MeshNearbyItem x; if(!meshNearbyAt(first+r,x))continue; int y=39+r*13; bool on=first+r==meshNearbySel;
      if(on)d.fillRoundRect(3,y-2,234,12,2,a); d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK); d.setCursor(6,y);
      d.printf("%c%c %-19.19s %4d",meshProtoLetter(x.proto),x.favorite?'*':' ',x.name,x.rssi);
    }
  }

  void handleMeshContacts(const InputEvent& e){
    if(e.action==InputAction::Up && meshContactCount) meshContactSel=(meshContactSel+meshContactCount-1)%meshContactCount;
    else if(e.action==InputAction::Down && meshContactCount) meshContactSel=(meshContactSel+1)%meshContactCount;
    else if(e.action==InputAction::Select && meshContactCount){
      MeshContact &c=meshContacts[meshContactSel]; meshProfile={}; meshProfile.proto=c.proto; strlcpy(meshProfile.key,c.key,sizeof(meshProfile.key)); strlcpy(meshProfile.name,c.name,sizeof(meshProfile.name)); refreshMeshProfile(); meshProfileReturn=49; enterScreen(52);
    }else if(e.action==InputAction::Character && (e.ch=='f'||e.ch=='F') && meshContactCount){
      MeshContact c=meshContacts[meshContactSel]; MeshNearbyItem x{};x.proto=c.proto;strlcpy(x.key,c.key,sizeof(x.key));strlcpy(x.name,c.name,sizeof(x.name));toggleMeshFavorite(x); if(meshContactSel>=meshContactCount&&meshContactCount)meshContactSel=meshContactCount-1;
    }
  }

  template <typename Gfx>
  void meshContactsScreen(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("lora // contacts");d.setCursor(207,23);d.printf("%u",meshContactCount);
    if(!meshContactCount){d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(8,48);d.print("no saved mesh contacts");d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(8,64);d.print("press f on nearby mesh to friend");return;}
    size_t first=meshContactSel>5?meshContactSel-5:0;
    for(size_t r=0;r<6&&first+r<meshContactCount;r++){ MeshContact &c=meshContacts[first+r]; MeshNearbyItem live{}; bool online=meshFindLiveByKey(c.key,live); int y=39+r*13;bool on=first+r==meshContactSel;if(on)d.fillRoundRect(3,y-2,234,12,2,a);d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK);d.setCursor(6,y);d.printf("%c* %-20.20s %s",meshProtoLetter(c.proto),c.name,online?"+":"-"); }
  }

  void handleMeshChannels(const InputEvent& e){
    static constexpr uint8_t N=3; if(e.action==InputAction::Up)meshChannelSel=(meshChannelSel+N-1)%N;else if(e.action==InputAction::Down)meshChannelSel=(meshChannelSel+1)%N;else if(e.action==InputAction::Select){ if(meshChannelSel==0)enterScreen(41); else if(meshChannelSel==1)enterScreen(47); else enterScreen(51); }
  }
  template <typename Gfx>
  void meshChannelsScreen(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("lora // channels");
    const char* rows[]={"D  #dirk-broadcast","M  meshtastic channels","C  meshcore channels"};
    for(int i=0;i<3;i++){int y=42+i*18;bool on=i==meshChannelSel;if(on)d.fillRoundRect(4,y-2,228,14,2,a);d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK);d.setCursor(9,y);d.print(rows[i]);}
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(8,101);d.print("external channels use companion adapters");
  }

  void handleMeshNetworks(const InputEvent& e){
    static constexpr uint8_t N=5;
    if(e.action==InputAction::Up)meshNetworkSel=(meshNetworkSel+N-1)%N;else if(e.action==InputAction::Down)meshNetworkSel=(meshNetworkSel+1)%N;
    else if(e.action==InputAction::Tab)requestMeshBleScan();
    else if(e.action==InputAction::Select){
      if(meshNetworkSel==0){meshAutoDiscover=!meshAutoDiscover;meshPrefs.putBool("auto",meshAutoDiscover);}
      else if(meshNetworkSel==2)enterScreen(47);
      else if(meshNetworkSel==3)requestMeshBleScan();
      else if(meshNetworkSel==4)requestMeshBleScan();
    }
  }
  template <typename Gfx>
  void meshNetworksScreen(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("lora // networks");
    String rows[]={String("auto discover      ")+(meshAutoDiscover?"on":"off"),String("dirk mesh          ")+(lora&&lora->ready()?"active":"--"),String("meshtastic ble     ")+String(meshProtoBleCount(MeshMeshtastic)),String("meshcore ble       ")+String(meshProtoBleCount(MeshCore)),String("scan companion radios")};
    for(int i=0;i<5;i++){int y=39+i*14;bool on=i==meshNetworkSel;if(on)d.fillRoundRect(4,y-2,228,13,2,a);d.setTextColor(on?TFT_BLACK:TFT_WHITE,on?a:TFT_BLACK);d.setCursor(8,y);d.print(rows[i]);}
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(8,108);d.print("D direct | M/C companion transports");
  }

  void handleMeshProfile(const InputEvent& e){
    if(e.action==InputAction::Character && (e.ch=='f'||e.ch=='F')){toggleMeshFavorite(meshProfile);meshProfile.favorite=meshIsFavorite(meshProfile.key);}
    else if(e.action==InputAction::Tab)requestMeshBleScan();
    else if(e.action==InputAction::Select){
      if(meshProfile.proto==MeshDirk){ loraCompose=""; loraComposeDirect=true; loraComposeTarget=(uint32_t)strtoul(meshProfile.key+2,nullptr,16); loraTextEntry=true; }
      else if(meshProfile.proto==MeshMeshtastic) enterScreen(47);
      else enterScreen(51);
    }
  }
  template <typename Gfx>
  void meshProfileScreen(Gfx& d,uint16_t a){
    d.setTextColor(a,TFT_BLACK);d.setCursor(5,23);d.print("mesh // profile");d.setTextColor(TFT_WHITE,TFT_BLACK);
    d.setCursor(8,40);d.printf("%c %s %s",meshProtoLetter(meshProfile.proto),meshProfile.name,meshProfile.favorite?"*":"");
    String k=meshProfile.key;if(k.length()>32)k=k.substring(0,32);d.setTextColor(0xBDF7,TFT_BLACK);d.setCursor(8,54);d.print(k);
    d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(8,69);d.printf("rssi %d",meshProfile.rssi);if(meshProfile.proto==MeshDirk)d.printf("  snr %.1f",meshProfile.snr);
    d.setCursor(8,83);d.printf("network %s",meshProtoName(meshProfile.proto));
    d.setTextColor(0x7BEF,TFT_BLACK);d.setCursor(8,98);
    if(meshProfile.proto==MeshDirk)d.print("enter composes direct message");
    else if(meshProfile.proto==MeshMeshtastic)d.print("enter opens meshtastic companion");
    else d.print("enter opens meshcore companion path");
    // Friend/scan/open controls live only in the footer to avoid duplicated hints.
  }

  void handleSettings(const InputEvent& e){
    auto& c=system->cfg();
    static constexpr uint8_t SETTINGS_COUNT=10;
    if(e.action==InputAction::Up) settingSel=(settingSel+SETTINGS_COUNT-1)%SETTINGS_COUNT;
    else if(e.action==InputAction::Down) settingSel=(settingSel+1)%SETTINGS_COUNT;
    else if(e.action==InputAction::Select && settingSel==0){ screen=10; scanWifi(); }
    else if(e.action==InputAction::Left || e.action==InputAction::Right){
      int dir=e.action==InputAction::Right?1:-1;
      switch(settingSel){
        case 1: c.brightness=constrain((int)c.brightness+dir*5,10,100); system->applyNow(); break;
        case 2: c.dimPercent=constrain((int)c.dimPercent+dir*5,0,100); break;
        case 3: c.dimSeconds=constrain((int)c.dimSeconds+dir*15,0,600); break;
        case 4: c.blackSeconds=constrain((int)c.blackSeconds+dir*30,0,1800); break;
        case 5: c.use24=!c.use24; break;
        case 6: c.autoTimezone=!c.autoTimezone; break;
        case 7: c.gmtOffset=constrain(c.gmtOffset+dir*900,-43200,50400); break;
        case 8: mode->set(mode->mode()==TeamMode::Blue?TeamMode::Red:TeamMode::Blue,true); break;
        case 9: c.distanceMiles=!c.distanceMiles; break;
      }
      system->save();
    }
  }

  void settings(lgfx::LGFXBase& d,uint16_t a){
    auto& c=system->cfg();
    const char* names[]={"wi-fi connection","brightness","dim level","dim after","black after","clock","timezone","utc offset","team mode","distance units"};
    String vals[]={
      system->connected()?system->connectedSsid():"open",
      String(c.brightness)+"%",String(c.dimPercent)+"%",String(c.dimSeconds)+"s",
      String(c.blackSeconds)+"s",c.use24?"24 hour":"12 hour",
      system->timezoneText(),String((float)c.gmtOffset/3600.0f,2),
      mode->mode()==TeamMode::Red?"red":"blue",c.distanceMiles?"miles":"kilometers"
    };
    int first=settingSel>5?settingSel-5:0;
    for(int row=0;row<6;row++){
      int i=first+row; if(i>=10) break; int y=25+row*15; bool on=i==settingSel;
      if(on) d.fillRoundRect(3,y-2,234,13,3,a);
      d.setTextColor(TFT_WHITE,on?a:TFT_BLACK); d.setCursor(7,y); d.print(names[i]);
      d.setCursor(233-d.textWidth(vals[i]),y); d.print(vals[i]);
    }
  }

  void scanWifi(){ WiFi.mode(WIFI_STA); WiFi.scanDelete(); WiFi.scanNetworks(false,true); wifiSel=0; }

  void handleWifiSettings(const InputEvent& e){
    int n=WiFi.scanComplete(); if(n<0) n=0;
    if(e.action==InputAction::Up && n) wifiSel=(wifiSel+n-1)%n;
    else if(e.action==InputAction::Down && n) wifiSel=(wifiSel+1)%n;
    else if(e.action==InputAction::Tab){ scanWifi(); }
    else if(e.action==InputAction::Select && n){
      pendingSsid=WiFi.SSID(wifiSel);
      wifi_auth_mode_t enc=WiFi.encryptionType(wifiSel);
      if(enc==WIFI_AUTH_OPEN){ system->connectWifi(pendingSsid,""); }
      else { password=""; textEntry=true; }
    }
  }

  void wifiSettings(lgfx::LGFXBase& d,uint16_t a){
    d.setTextColor(a); d.setCursor(5,24);
    d.print(system->connected()?"connected":"wi-fi connection");
    if(system->connected()){
      d.setTextColor(TFT_WHITE); d.setCursor(5,36); d.print(system->connectedSsid());
      d.setCursor(5,48); d.print(system->ip().toString());
    }
    int n=WiFi.scanComplete(); if(n<0)n=0;
    int first=wifiSel>4?wifiSel-4:0;
    for(int row=0;row<5 && first+row<n;row++){
      int i=first+row,y=58+row*11; bool on=i==wifiSel;
      if(on) d.fillRect(3,y-1,234,10,a);
      d.setTextColor(TFT_WHITE,on?a:TFT_BLACK); d.setCursor(6,y);
      String s=WiFi.SSID(i); d.printf("%-20.20s %4d",s.c_str(),WiFi.RSSI(i));
    }
  }

  void handleText(const InputEvent& e){
    if(e.action==InputAction::Back){
      if(password.length()) password.remove(password.length()-1);
      else textEntry=false;
    } else if(e.action==InputAction::Select){
      textEntry=false; system->connectWifi(pendingSsid,password);
    } else if(e.action==InputAction::Character && e.ch>=32 && e.ch<=126 && password.length()<63){
      password+=e.ch;
    }
    dirty=true;
  }

  void drawTextEntry(lgfx::LGFXBase& d,uint16_t a){
    d.fillRoundRect(12,31,216,72,8,0x1082); d.drawRoundRect(12,31,216,72,8,a);
    d.setTextColor(TFT_WHITE,0x1082); d.setCursor(20,41); d.print("wi-fi password");
    d.setCursor(20,55); d.print(pendingSsid);
    String stars; for(size_t i=0;i<password.length();i++) stars+='*';
    d.fillRoundRect(19,68,202,18,3,TFT_BLACK); d.setCursor(24,73); d.print(stars);
    d.setCursor(20,91); d.print("enter connect   bksp delete");
  }

  void moduleScreen(lgfx::LGFXBase& d,uint16_t a){
    static const char* title[]={"","wi-fi","bluetooth","wardrive","captures","audits","network tools","sessions","files","settings"};
    d.setTextColor(a); d.setTextSize(2); d.setCursor(8,29); d.print(title[screen]); d.setTextSize(1);
    d.setTextColor(TFT_WHITE); d.setCursor(8,57); d.print("dirk v0.7.18 subsystem");
    d.setCursor(8,73); d.printf("2.4:%c 5g:%c ble:%c gps:%c sd:%c",
      caps->get().wifi24?'+':'-',caps->get().wifi5?'+':'-',caps->get().ble?'+':'-',
      caps->get().gps?'+':'-',caps->get().sd?'+':'-');
    d.setCursor(8,91);
    if(screen==2) d.print("ble provider ready");
    else if(screen==3) d.print("gps/sd wardrive foundation ready");
    else if(screen==4) d.print("authorized capture boundary ready");
    else d.print("module framework ready for beta");
  }
};
}
