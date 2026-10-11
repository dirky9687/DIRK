#include "InputManager.h"

namespace dirk {

// Values used by the M5Cardputer key map for non-printing keys.
static constexpr uint8_t K_FN        = 0xFF;
static constexpr uint8_t K_SHIFT     = 0x81;
static constexpr uint8_t K_TAB       = 0x2B;
static constexpr uint8_t K_ENTER     = 0x28;
static constexpr uint8_t K_BACKSPACE = 0x2A;

bool InputManager::begin() {
    head_ = tail_ = count_ = 0;
    fn_ = shift_ = false;
    ready_ = false;

    // Use M5Cardputer's bundled TCA8418 driver. Its constructor is already
    // bound to the Cardputer ADV internal I2C bus/address (0x34).
    if (!tca_.begin()) return false;
    if (!tca_.matrix(7, 8)) return false;
    tca_.flush();

    // Poll the FIFO directly. This intentionally avoids depending on GPIO11
    // interrupt delivery, matching the proven polling approach used by Launcher.
    ready_ = true;
    return true;
}

void InputManager::emit(InputAction action, char ch) {
    if (count_ >= Q) return;
    q_[tail_].action = action;
    q_[tail_].ch = ch;
    tail_ = (uint8_t)((tail_ + 1u) % Q);
    ++count_;
}

InputEvent InputManager::next() {
    InputEvent ev;
    if (!count_) return ev;
    ev = q_[head_];
    head_ = (uint8_t)((head_ + 1u) % Q);
    --count_;
    return ev;
}

bool InputManager::decodePhysical(uint8_t raw, uint8_t& row, uint8_t& col, bool& pressed) {
    // M5Cardputer's ADV reader and Launcher's tested ADV implementation both
    // treat bit 7 as key state and the lower 7 bits as the 1-based TCA key ID.
    pressed = (raw & 0x80u) != 0;
    uint8_t key = raw & 0x7Fu;
    if (!key) return false;

    // Same transform as Launcher's mapRawKeyToPhysical().
    const uint8_t u = key % 10u;  // 1..8
    const uint8_t t = key / 10u;  // 0..6
    if (u < 1u || u > 8u || t > 6u) return false;
    const uint8_t u0 = u - 1u;
    row = u0 & 0x03u;
    col = (uint8_t)((t << 1u) | (u0 >> 2u));
    return row < 4u && col < 14u;
}

char InputManager::baseKey(uint8_t row, uint8_t col) {
    // Mirrors the base layer in the installed M5Cardputer Keyboard.h.
    static const uint8_t map[4][14] = {
        {'`','1','2','3','4','5','6','7','8','9','0','-','=',K_BACKSPACE},
        {K_TAB,'q','w','e','r','t','y','u','i','o','p','[',']','\\'},
        {K_FN,K_SHIFT,'a','s','d','f','g','h','j','k','l',';','\'',K_ENTER},
        {0x80,0x00,0x82,'z','x','c','v','b','n','m',',','.','/',' '}
    };
    return (char)map[row][col];
}

void InputManager::handleRawEvent(uint8_t raw) {
    uint8_t row = 0, col = 0;
    bool pressed = false;
    if (!decodePhysical(raw, row, col, pressed)) return;

    const uint8_t key = (uint8_t)baseKey(row, col);

    // Track modifiers on both press and release.
    if (key == K_FN)    { fn_ = pressed; return; }
    if (key == K_SHIFT) { shift_ = pressed; return; }

    // DIRK emits actions on key-down only.
    if (!pressed) return;

    // Hardware-tested Launcher navigation mapping for Cardputer ADV:
    // ; = up, , = left/previous, . = down, / = right/next.
    if (key == ';') { emit(InputAction::Up); return; }
    if (key == ',') { emit(InputAction::Left); return; }
    if (key == '.') { emit(InputAction::Down); return; }
    if (key == '/') { emit(InputAction::Right); return; }
    if (key == K_ENTER) { emit(InputAction::Select); return; }
    if (key == '`' || key == K_BACKSPACE) { emit(InputAction::Back); return; }
    if (key == K_TAB) { emit(InputAction::Tab); return; }

    // Preserve the rest of DIRK's keyboard functionality. Fn combinations are
    // reserved for navigation/function behavior and are not inserted as text.
    if (fn_) return;
    if (key >= 32u && key <= 126u) {
        char ch = (char)key;
        if (shift_ && ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
        emit(InputAction::Character, ch);
    }
}

void InputManager::update() {
    if (!ready_) return;

    // Drain the hardware FIFO every frame; no IRQ flag is required.
    uint8_t guard = 32;
    while (guard-- && tca_.available()) {
        handleRawEvent(tca_.getEvent());
    }
}

} // namespace dirk
