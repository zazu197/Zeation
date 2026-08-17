#pragma once

namespace zext {

class DeathDetector {
public:
    bool update(bool alive, float health);
    void reset();

private:
    bool was_alive_ = false;
};

} // namespace zext
