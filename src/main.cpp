#include <algorithm>
#include <atomic>
#include <chrono>
#include <conio.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "crosshair_renderer.h"
#include "offset_loader.h"
#include "player_monitor.h"

namespace {

std::atomic<bool> g_stop{false};

BOOL WINAPI console_handler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_CLOSE_EVENT || type == CTRL_BREAK_EVENT) {
        g_stop.store(true);
        return TRUE;
    }
    return FALSE;
}

void print_banner() {
    std::printf("==============================================\n");
    std::printf("  Zetian External Health Monitor + Crosshair\n");
    std::printf("  External only - no injection, no writes\n");
    std::printf("==============================================\n");
}

std::string exe_directory() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0) {
        return {};
    }
    return std::filesystem::path(buffer).parent_path().string();
}

std::string find_offsets_path(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument != "--debug" && argument != "--probe"
            && argument != "--test-death") {
            return argument;
        }
    }

    const std::string exe_dir = exe_directory();
    const std::string cwd = std::filesystem::current_path().string();

    const std::string candidates[] = {
        "offsets.json",
        (std::filesystem::path(exe_dir) / "offsets.json").string(),
        (std::filesystem::path(exe_dir) / ".." / "offsets.json").string(),
        (std::filesystem::path(exe_dir) / ".." / ".." / "offsets.json").string(),
        (std::filesystem::path(cwd) / "offsets.json").string(),
    };

    for (const std::string& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return "offsets.json";
}

std::string installed_roblox_version() {
    wchar_t buffer[MAX_PATH]{};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
    if (length == 0) {
        return {};
    }
    const std::filesystem::path versions_dir =
        std::filesystem::path(buffer) / L"Roblox" / L"Versions";
    std::error_code ec;
    std::string best;
    std::filesystem::file_time_type best_time{};
    for (const auto& entry : std::filesystem::directory_iterator(versions_dir, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_directory(ec)) {
            continue;
        }
        const auto exe = entry.path() / L"RobloxPlayerBeta.exe";
        if (!std::filesystem::exists(exe, ec)) {
            continue;
        }
        const auto time = std::filesystem::last_write_time(entry, ec);
        if (best.empty() || time > best_time) {
            best = entry.path().filename().string();
            best_time = time;
        }
    }
    return best;
}

bool same_version(const std::string& lhs, const std::string& rhs) {
    auto lower = [](std::string text) {
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    };
    return lower(lhs) == lower(rhs);
}

int fatal_error(const std::string& message) {
    std::printf("[error] %s\n", message.c_str());
    MessageBoxA(nullptr, message.c_str(), "ZetianExternal - Fatal Error",
                MB_OK | MB_ICONERROR);
    std::printf("\nPress any key to exit...\n");
    while (true) {
        if (_kbhit() != 0) {
            break;
        }
        Sleep(50);
    }
    return 1;
}

void print_probe(const zext::RobloxProbe& probe) {
    std::printf("[probe] DataModel=0x%llX class='%s'\n",
                static_cast<unsigned long long>(probe.data_model),
                probe.data_model_class.c_str());
    for (const auto& strategy : probe.children) {
        std::printf("[probe]   children offset=0x%zX stride=%zu -> %zu valid",
                    strategy.offset, strategy.stride, strategy.count);
        for (const std::string& klass : strategy.classes) {
            std::printf(" %s", klass.c_str());
        }
        std::printf("\n");
    }
    for (const auto& name : probe.names) {
        std::printf("[probe]   name offset=0x%zX mode='%s' -> '%s'\n",
                    name.offset, name.mode.c_str(), name.value.c_str());
    }
    std::printf("[probe] Players=0x%llX LocalPlayer=0x%llX Character=0x%llX "
                "Humanoid=0x%llX\n",
                static_cast<unsigned long long>(probe.players),
                static_cast<unsigned long long>(probe.local_player),
                static_cast<unsigned long long>(probe.character),
                static_cast<unsigned long long>(probe.humanoid));
    std::printf("[probe] health=%.1f max=%.1f\n", probe.health, probe.max_health);
}

} // namespace

int run(int argc, char** argv);

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetConsoleTitleW(L"ZetianExternal - Health Monitor + Crosshair");
    print_banner();

    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        return fatal_error(std::string("Unexpected error: ") + error.what());
    } catch (...) {
        return fatal_error("Unexpected fatal error.");
    }
}

int run(int argc, char** argv) {
    const std::string offsets_path = find_offsets_path(argc, argv);

    zext::RobloxOffsets offsets;
    try {
        offsets = zext::load_offsets(offsets_path);
    } catch (const std::exception& error) {
        return fatal_error("Failed to load offsets from '" + offsets_path
                           + "':\n" + error.what()
                           + "\n\nMake sure offsets.json sits next to this exe "
                             "or in the working directory, or pass the path:\n"
                             "ZetianExternal.exe <path-to-offsets.json>");
    }

    std::printf("[info] Loaded offsets from '%s'\n", offsets_path.c_str());
    if (!offsets.roblox_version.empty()) {
        const std::string installed = installed_roblox_version();
        std::printf("[info]   Offsets target Roblox version: %s\n",
                    offsets.roblox_version.c_str());
        if (!installed.empty()) {
            std::printf("[info]   Installed Roblox version:    %s\n", installed.c_str());
            if (!same_version(offsets.roblox_version, installed)) {
                std::printf("[warn]  Version mismatch - the offsets file is for a "
                            "different Roblox build.\n");
                std::printf("[warn]  Static pointers will be garbage and the player "
                            "will never resolve.\n");
                std::printf("[warn]  Re-dump current offsets from "
                            "https://offsets.imtheo.lol and replace offsets.json.\n");
            }
        }
    }
    std::printf("[info]   ChildrenStart = 0x%zX  ChildrenEnd = 0x%zX\n",
                offsets.instance_children_start, offsets.instance_children_end_delta);
    std::printf("[info]   Player.LocalPlayer = 0x%zX  ModelInstance = 0x%zX\n",
                offsets.player_local_player, offsets.player_model_instance);
    std::printf("[info]   Humanoid.Health = 0x%zX  MaxHealth = 0x%zX\n",
                offsets.humanoid_health, offsets.humanoid_max_health);

    bool debug = false;
    bool probe = false;
    bool test_death = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--debug") {
            debug = true;
        }
        if (std::string(argv[i]) == "--probe") {
            probe = true;
            debug = true;
        }
        if (std::string(argv[i]) == "--test-death") {
            test_death = true;
        }
    }

    zext::CrosshairRenderer renderer;
    if (!renderer.init(GetModuleHandle(nullptr))) {
        return fatal_error("Failed to initialize the crosshair overlay window.\n"
                           "Another instance may already be running.");
    }

    std::printf("[info] Overlay ready. Press Ctrl+C to quit.\n");
    const std::string initial_style = renderer.request_random_style();
    std::printf("[info] Initial crosshair: %s\n", initial_style.c_str());

    std::thread render_thread([&renderer] { renderer.run(); });

    SetConsoleCtrlHandler(console_handler, TRUE);

    zext::PlayerMonitor monitor(offsets);
    std::atomic<int> death_count{0};

    auto on_death = [&renderer, &death_count]() {
        const int deaths = ++death_count;
        const std::string style = renderer.request_random_style();
        std::printf("[death] Player dead! (#%d) Switching crosshair to: %s\n",
                    deaths, style.c_str());
    };

    monitor.set_probe_enabled(probe);
    monitor.set_on_status([](const std::string& status) {
        std::printf("[status] %s\n", status.c_str());
    });
    monitor.set_on_health([](float health, float max_health) {
        std::printf("[health] Player health: %.1f / %.1f\n", health, max_health);
    });
    monitor.set_on_respawn([]() {
        std::printf("[respawn] Player respawned - monitoring again\n");
    });
    monitor.set_on_death(on_death);
    if (probe) {
        monitor.set_on_probe(print_probe);
    }

    std::thread death_test_thread;
    if (test_death) {
        std::printf("[info] Test mode: simulating a death in 3 seconds...\n");
        death_test_thread = std::thread([on_death]() {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            on_death();
        });
    }

    monitor.start();

    while (!g_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    std::printf("[info] Shutting down...\n");
    monitor.stop();
    if (death_test_thread.joinable()) {
        death_test_thread.join();
    }
    renderer.shutdown();
    render_thread.join();
    std::printf("[info] Done. Total deaths detected: %d\n", death_count.load());
    std::printf("\nPress any key to exit...\n");
    while (true) {
        if (_kbhit() != 0) {
            break;
        }
        Sleep(50);
    }
    return 0;
}
