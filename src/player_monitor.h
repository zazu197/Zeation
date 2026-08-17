#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include "death_detector.h"
#include "offset_loader.h"
#include "roblox_state.h"

namespace zext {

class PlayerMonitor {
public:
    using StatusCallback = std::function<void(const std::string&)>;
    using HealthCallback = std::function<void(float health, float max_health)>;
    using DeathCallback = std::function<void()>;
    using RespawnCallback = std::function<void()>;
    using ProbeCallback = std::function<void(const RobloxProbe&)>;

    explicit PlayerMonitor(const RobloxOffsets& offsets);
    ~PlayerMonitor();

    void start();
    void stop();

    void set_poll_interval(std::chrono::milliseconds interval);
    void set_probe_enabled(bool enabled);
    void set_on_status(StatusCallback callback);
    void set_on_health(HealthCallback callback);
    void set_on_death(DeathCallback callback);
    void set_on_respawn(RespawnCallback callback);
    void set_on_probe(ProbeCallback callback);

private:
    void run();

    RobloxOffsets offsets_;
    std::chrono::milliseconds poll_interval_{100};
    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> probe_enabled_{false};

    StatusCallback on_status_;
    HealthCallback on_health_;
    DeathCallback on_death_;
    RespawnCallback on_respawn_;
    ProbeCallback on_probe_;
    DeathDetector death_detector_;
};

} // namespace zext
