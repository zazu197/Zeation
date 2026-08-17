#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace zext {

struct RobloxOffsets {
    std::string roblox_version;

    uintptr_t visual_engine_pointer = 0;
    uintptr_t fake_data_model_pointer = 0;
    std::size_t fake_data_model_real_data_model = 0;
    std::size_t visual_engine_fake_data_model = 0;

    std::size_t instance_children_start = 0;
    std::size_t instance_children_end_delta = 0;
    std::size_t instance_name = 0;
    std::size_t instance_name_container = 0;
    std::size_t instance_class_descriptor = 0;
    std::size_t instance_class_name = 0;

    std::size_t player_local_player = 0;
    std::size_t player_model_instance = 0;

    std::size_t humanoid_health = 0;
    std::size_t humanoid_max_health = 0;
};

RobloxOffsets load_offsets(const std::string& path);

} // namespace zext
