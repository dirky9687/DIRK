#pragma once
#include <M5Cardputer.h>
#include "EventBus.h"
#include "CapabilityManager.h"
#include "TeamModeManager.h"
#include "../storage/StorageManager.h"
#include "../targets/TargetManager.h"
#include "../providers/s3/S3WifiProvider.h"
#include "../providers/s3/NetworkTools.h"
#include "../providers/ble/BleProvider.h"
#include "../providers/c5/C5Transport.h"
#include "../providers/c5/JanOSAdapter.h"
#include "../providers/gps/GpsProvider.h"
#include "../providers/lora/LoRaProvider.h"
#include "../providers/ir/IrProvider.h"
#include "../blue/BlueGuard.h"
#include "../wardrive/WardriveManager.h"
#include "../capture/CaptureManager.h"
#include "../audit/AuditManager.h"
#include "../portal/LabPortal.h"
#include "../ui/InputManager.h"
#include "../ui/Ui.h"
#include "../system/SystemServices.h"

namespace dirk {
class DirkApp {
public:
  bool begin();
  void update();
private:
  void drawSplashBase();
  void drawSplashMatrix(uint8_t phase);
  void drawSplashArtwork();
  void splashCheck(const char* name,bool ok,int y);
  uint8_t splashPhase=0;
  bool splashArtworkDrawn=false;
  EventBus events;
  CapabilityManager capabilities;
  TeamModeManager team;
  StorageManager storage;
  TargetManager targets;
  S3WifiProvider wifi;
  NetworkTools nettools;
  BleProvider ble;
  C5Transport c5;
  JanOSAdapter janos;
  GpsProvider gps;
  LoRaProvider lora;
  IrProvider ir;
  BlueGuard blueGuard;
  WardriveManager wardrive;
  CaptureManager captures;
  AuditManager audits;
  LabPortal portal;
  InputManager input;
  Ui ui;
  SystemServices system;
};
}
