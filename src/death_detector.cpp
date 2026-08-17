#include "death_detector.h"

namespace zext {

bool DeathDetector::update(bool alive, float health) {
    const bool dead = !alive || health <= 0.0f;
    if (dead) {
        const bool triggered = was_alive_;
        was_alive_ = false;
        return triggered;
    }
    was_alive_ = true;
    return false;
}

void DeathDetector::reset() {
    was_alive_ = false;
}

} // namespace zext
