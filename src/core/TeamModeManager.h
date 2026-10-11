#pragma once
#include <dirk/types.h>
#include <Preferences.h>

namespace dirk {
class TeamModeManager {
public:
  void begin(){
    prefs.begin("dirk-team",false);
    mode_ = prefs.getUChar("mode",0)==1 ? TeamMode::Red : TeamMode::Blue;
  }
  TeamMode mode() const { return mode_; }
  bool set(TeamMode m,bool confirmed){
    if(m==TeamMode::Red && !confirmed) return false;
    mode_=m; prefs.putUChar("mode",m==TeamMode::Red?1:0); return true;
  }
  bool permitsActive(const WifiTarget& t) const {
    return mode_==TeamMode::Red && t.saved && t.authorized;
  }
private:
  TeamMode mode_=TeamMode::Blue;
  Preferences prefs;
};
}
