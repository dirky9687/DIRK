#pragma once
#include <Arduino.h>
#include <utility/Adafruit_TCA8418/Adafruit_TCA8418.h>

namespace dirk {

enum class InputAction : uint8_t {
    None, Up, Down, Left, Right, Select, Back, Tab, Save, Help, Quick,
    Home, Pause, Menu, Character,
    // Keep the v0.3.2 public UI contract intact. These are consumed by Ui::hotIndex().
    HotWifi, HotBle, HotWardrive, HotCaptures, HotAudits,
    HotNetwork, HotSessions, HotFiles, HotSettings
};

struct InputEvent {
    InputAction action = InputAction::None;
    char ch = 0;
    bool valid() const { return action != InputAction::None; }
};

class InputManager {
public:
    bool begin();
    void update();
    bool available() const { return count_ != 0; }
    InputEvent next();
    // v0.3.2 DirkApp uses read(); keep it as the queue-consumer API.
    InputEvent read() { return next(); }
    bool keyboardReady() const { return ready_; }

private:
    static constexpr uint8_t Q = 24;
    InputEvent q_[Q];
    uint8_t head_ = 0, tail_ = 0, count_ = 0;
    bool ready_ = false;
    bool fn_ = false;
    bool shift_ = false;
    Adafruit_TCA8418 tca_;

    void emit(InputAction action, char ch = 0);
    void handleRawEvent(uint8_t raw);
    static bool decodePhysical(uint8_t raw, uint8_t& row, uint8_t& col, bool& pressed);
    static char baseKey(uint8_t row, uint8_t col);
};

} // namespace dirk
