#pragma once
#include "RedAudit.h"
namespace dirk {
// Advertisement-only observations. These are configuration/privacy review
// hints, not proof of GATT permissions, pairing security or exploitability.
template <class Adv> inline RedPosture assessBlePosture(const Adv& t){
  RedPosture a{};
  if(t.named) a.add(RedSeverity::Info,"Name broadcast observed","Review naming privacy policy");
  else a.add(RedSeverity::Info,"No name advertised","Identity cannot be inferred");
  if(t.ibeacon) a.add(RedSeverity::Review,"iBeacon format observed","Review beacon ID exposure");
  if(t.eddystone) a.add(RedSeverity::Review,"Eddystone observed","Review beacon configuration");
  if(t.meshtastic || t.meshcore)
    a.add(RedSeverity::Info,"Mesh service indicated","Validate owner and radio setup");
  if(t.manufacturerId || t.manufacturerDataBytes)
    a.add(RedSeverity::Info,"Manufacturer data seen","May reveal vendor; review use");
  if(t.service[0]) a.add(RedSeverity::Info,"Service UUID announced","Only a partial service sample");
  if(t.hasTxPower) a.add(RedSeverity::Info,"TX power advertised","Compare expected signal range");
  a.add(RedSeverity::Info,"GATT security untested","No connection, read or write made");
  return a;
}
} // namespace dirk
