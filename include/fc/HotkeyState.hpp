#pragma once

namespace fc {
// One transition on the rising edge, without depending on window messages.
// Reacquiring focus primes the state: a key held while Alt-Tabbing cannot open
// the menu accidentally. Call only from the render thread under the UI mutex.
class HotkeyState {
public:
    bool Update(bool focused, bool down) noexcept {
        if (!focused) {
            focused_ = false;
            down_ = false;
            return false;
        }
        if (!focused_) {
            focused_ = true;
            down_ = down;
            return false;
        }
        const bool pressed = down && !down_;
        down_ = down;
        return pressed;
    }
private:
    bool focused_{};
    bool down_{};
};
}
