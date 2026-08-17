#include "player_monitor.h"

#include <cmath>
#include <sstream>

#include "process.h"

namespace zext {

namespace {

const wchar_t kRobloxExe[] = L"RobloxPlayerBeta.exe";

} // namespace

PlayerMonitor::PlayerMonitor(const RobloxOffsets& offsets)
    : offsets_(offsets) {}

PlayerMonitor::~PlayerMonitor() {
    stop();
}

void PlayerMonitor::start() {
    if (thread_.joinable()) {
        return;
    }
    stop_.store(false);
    thread_ = std::thread(&PlayerMonitor::run, this);
}

void PlayerMonitor::stop() {
    stop_.store(true);
    if (thread_.joinable()) {
        thread_.join();
    }
}

void PlayerMonitor::set_poll_interval(std::chrono::milliseconds interval) {
    poll_interval_ = interval;
}

void PlayerMonitor::set_probe_enabled(bool enabled) {
    probe_enabled_.store(enabled);
}

void PlayerMonitor::set_on_status(StatusCallback callback) {
    on_status_ = std::move(callback);
}

void PlayerMonitor::set_on_health(HealthCallback callback) {
    on_health_ = std::move(callback);
}

void PlayerMonitor::set_on_death(DeathCallback callback) {
    on_death_ = std::move(callback);
}

void PlayerMonitor::set_on_respawn(RespawnCallback callback) {
    on_respawn_ = std::move(callback);
}

void PlayerMonitor::set_on_probe(ProbeCallback callback) {
    on_probe_ = std::move(callback);
}

void PlayerMonitor::run() {
    HANDLE process = nullptr;
    uintptr_t base = 0;
    DWORD pid = 0;
    std::string current_status;
    float last_health = -1.0f;
    bool previous_alive = false;
    bool ever_alive = false;
    auto last_health_report = std::chrono::steady_clock::now() - std::chrono::seconds(2);
    auto last_probe_report = std::chrono::steady_clock::now();

    RobloxState state(offsets_);

    auto emit_status = [&](const std::string& message) {
        if (message != current_status) {
            current_status = message;
            if (on_status_) {
                on_status_(message);
            }
        }
    };

    while (!stop_.load()) {
        const auto tick_start = std::chrono::steady_clock::now();

        if (process == nullptr || base == 0) {
            pid = find_process_pid(kRobloxExe);
            if (pid == 0) {
                emit_status("Waiting for Roblox process (RobloxPlayerBeta.exe)...");
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                continue;
            }
            process = open_process_by_pid(pid);
            if (process == nullptr) {
                emit_status("Found Roblox process but failed to open a read handle");
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                continue;
            }
            base = get_module_base(pid, kRobloxExe);
            if (base == 0) {
                CloseHandle(process);
                process = nullptr;
                emit_status("Failed to resolve the Roblox module base address");
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                continue;
            }
            state.set_process(process, base);
            death_detector_.reset();
            last_health = -1.0f;
            std::ostringstream attached_message;
            attached_message << "Attached to Roblox process (pid " << pid
                             << ") at base 0x" << std::hex << base;
            emit_status(attached_message.str());
        }

        uintptr_t data_model = 0;
        uintptr_t players = 0;
        uintptr_t local_player = 0;
        uintptr_t character = 0;
        uintptr_t humanoid = 0;
        float health = 0.0f;
        float max_health = 0.0f;

        bool alive = false;

        if (!state.get_data_model(data_model)) {
            emit_status("Waiting for local player... (DataModel not resolved - "
                        "offsets likely stale)");
        } else if (!state.find_child_by_class(data_model, "Players", players)) {
            emit_status("Waiting for local player... (Players service not found)");
        } else if (!state.get_local_player(players, local_player)) {
            emit_status("Waiting for local player... (LocalPlayer pointer invalid)");
        } else if (!state.get_character(local_player, character)
                   || !state.get_humanoid(character, humanoid)
                   || !state.read_health(humanoid, health, max_health)) {
            emit_status("Player is dead - waiting for respawn");
        } else {
            alive = health > 0.001f;
            emit_status("Monitoring local player health");
        }

        const bool died = death_detector_.update(alive, health);
        if (died && on_death_) {
            on_death_();
        }

        if (alive) {
            if (!previous_alive && ever_alive && on_respawn_) {
                on_respawn_();
            }
            ever_alive = true;
        }
        previous_alive = alive;

        if (alive) {
            const auto now = std::chrono::steady_clock::now();
            const bool changed = std::fabs(health - last_health) >= 0.5f;
            if (changed || now - last_health_report >= std::chrono::seconds(1)) {
                last_health = health;
                last_health_report = now;
                if (on_health_) {
                    on_health_(health, max_health);
                }
            }
        }

        if (probe_enabled_.load() && on_probe_) {
            const auto now = std::chrono::steady_clock::now();
            if (now - last_probe_report >= std::chrono::seconds(3)) {
                last_probe_report = now;
                on_probe_(state.run_probe());
            }
        }

        if (!is_process_running(process)) {
            CloseHandle(process);
            process = nullptr;
            base = 0;
            death_detector_.reset();
            emit_status("Roblox process exited - reconnecting");
        }

        const auto elapsed = std::chrono::steady_clock::now() - tick_start;
        if (elapsed < poll_interval_) {
            std::this_thread::sleep_for(poll_interval_ - elapsed);
        }
    }

    if (process != nullptr) {
        CloseHandle(process);
    }
}

} // namespace zext
