#include "roblox_state.h"

#include <cmath>
#include <cstring>

namespace zext {

namespace {

constexpr std::size_t kMaxChildren = 64;
constexpr std::size_t kMaxContainerSpan = 0x10000;
constexpr std::size_t kSharedPtrStride = 16;

bool plausible_class_name(const std::string& name) {
    if (name.empty() || name.size() > 64) {
        return false;
    }
    for (unsigned char c : name) {
        const bool printable = c >= 0x20 && c < 0x7F;
        if (!printable) {
            return false;
        }
    }
    return true;
}

} // namespace

RobloxState::RobloxState(const RobloxOffsets& offsets)
    : offsets_(offsets) {}

void RobloxState::set_process(HANDLE process, uintptr_t base_address) {
    process_ = process;
    base_ = base_address;
}

bool RobloxState::attached() const {
    return process_ != nullptr && base_ != 0;
}

bool RobloxState::read_ptr(uintptr_t address, uintptr_t& out) const {
    return read_pointer(process_, address, out);
}

std::string RobloxState::class_name_of(uintptr_t instance) {
    if (instance == 0) {
        return {};
    }
    uintptr_t class_descriptor = 0;
    if (!read_ptr(instance + offsets_.instance_class_descriptor, class_descriptor)
        || class_descriptor == 0) {
        return {};
    }
    uintptr_t class_name_ptr = 0;
    if (!read_ptr(class_descriptor + offsets_.instance_class_name, class_name_ptr)
        || class_name_ptr == 0) {
        return {};
    }
    return read_string(process_, class_name_ptr, 128);
}

std::string RobloxState::get_class_name(uintptr_t instance) {
    return class_name_of(instance);
}

std::string RobloxState::get_name(uintptr_t instance) {
    if (instance == 0) {
        return {};
    }
    uintptr_t name_ptr = 0;
    if (!read_ptr(instance + offsets_.instance_name, name_ptr) || name_ptr == 0) {
        return {};
    }
    return read_string(process_, name_ptr, 128);
}

bool RobloxState::resolve_data_model(uintptr_t fake_data_model, uintptr_t& out) {
    if (fake_data_model == 0) {
        return false;
    }
    uintptr_t real = 0;
    if (!read_ptr(fake_data_model + offsets_.fake_data_model_real_data_model, real)
        || real == 0) {
        return false;
    }
    const std::string klass = class_name_of(real);
    if (klass == "DataModel" || klass == "DataModelGame") {
        out = real;
        return true;
    }
    return false;
}

bool RobloxState::get_data_model(uintptr_t& out) {
    if (!attached()) {
        return false;
    }

    uintptr_t fake = 0;
    if (read_ptr(base_ + offsets_.fake_data_model_pointer, fake)
        && resolve_data_model(fake, out)) {
        return true;
    }

    uintptr_t visual_engine = 0;
    if (read_ptr(base_ + offsets_.visual_engine_pointer, visual_engine)
        && visual_engine != 0) {
        uintptr_t fake_from_ve = 0;
        if (read_ptr(visual_engine + offsets_.visual_engine_fake_data_model,
                     fake_from_ve)
            && resolve_data_model(fake_from_ve, out)) {
            return true;
        }
    }

    return false;
}

std::vector<uintptr_t> RobloxState::get_children(uintptr_t instance) {
    std::vector<uintptr_t> children;

    uintptr_t container = 0;
    if (instance == 0
        || !read_ptr(instance + offsets_.instance_children_start, container)
        || container == 0) {
        return children;
    }

    uintptr_t begin = 0;
    uintptr_t end = 0;
    if (!read_ptr(container, begin)
        || !read_ptr(container + offsets_.instance_children_end_delta, end)) {
        return children;
    }
    if (begin == 0 || end < begin || end - begin > kMaxContainerSpan) {
        return children;
    }

    for (uintptr_t cursor = begin;
         cursor < end && children.size() < kMaxChildren;
         cursor += kSharedPtrStride) {
        uintptr_t child = 0;
        if (read_ptr(cursor, child) && child != 0) {
            children.push_back(child);
        }
    }

    return children;
}

bool RobloxState::find_child_by_class(uintptr_t instance, const std::string& klass,
                                      uintptr_t& out) {
    for (uintptr_t child : get_children(instance)) {
        if (class_name_of(child) == klass) {
            out = child;
            return true;
        }
    }
    return false;
}

bool RobloxState::get_local_player(uintptr_t players, uintptr_t& out) {
    if (players == 0) {
        return false;
    }
    return read_ptr(players + offsets_.player_local_player, out) && out != 0;
}

bool RobloxState::get_character(uintptr_t player, uintptr_t& out) {
    if (player == 0) {
        return false;
    }
    return read_ptr(player + offsets_.player_model_instance, out) && out != 0;
}

bool RobloxState::get_humanoid(uintptr_t character, uintptr_t& out) {
    return find_child_by_class(character, "Humanoid", out);
}

bool RobloxState::read_health(uintptr_t humanoid, float& health, float& max_health) {
    if (humanoid == 0) {
        return false;
    }
    if (!read_float(process_, humanoid + offsets_.humanoid_health, health)) {
        return false;
    }
    max_health = 100.0f;
    float max = 0.0f;
    if (read_float(process_, humanoid + offsets_.humanoid_max_health, max)
        && std::isfinite(max) && max > 0.0f) {
        max_health = max;
    }
    return std::isfinite(health);
}

RobloxProbe RobloxState::run_probe() {
    RobloxProbe probe;

    if (!attached() || !get_data_model(probe.data_model)) {
        return probe;
    }

    probe.data_model_class = class_name_of(probe.data_model);

    for (std::size_t offset : {offsets_.instance_children_start,
                               offsets_.instance_name_container}) {
        if (offset == 0) {
            continue;
        }
        for (std::size_t stride : {std::size_t{8}, kSharedPtrStride}) {
            ChildrenStrategyResult result;
            result.offset = offset;
            result.stride = stride;

            uintptr_t container = 0;
            uintptr_t begin = 0;
            uintptr_t end = 0;
            if (!read_ptr(probe.data_model + offset, container) || container == 0
                || !read_ptr(container, begin)
                || !read_ptr(container + offsets_.instance_children_end_delta, end)
                || begin == 0 || end < begin || end - begin > kMaxContainerSpan) {
                probe.children.push_back(std::move(result));
                continue;
            }

            for (uintptr_t cursor = begin;
                 cursor < end && result.count < kMaxChildren;
                 cursor += stride) {
                uintptr_t child = 0;
                if (!read_ptr(cursor, child) || child == 0) {
                    continue;
                }
                std::string klass = class_name_of(child);
                if (!plausible_class_name(klass)) {
                    continue;
                }
                ++result.count;
                if (result.classes.size() < 16) {
                    result.classes.push_back(std::move(klass));
                }
            }

            probe.children.push_back(std::move(result));
        }
    }

    std::vector<std::pair<std::size_t, std::string>> name_offsets = {
        {offsets_.instance_name, "qword-then-string"},
        {offsets_.instance_name_container, "qword-then-string"},
        {offsets_.instance_name, "raw-bytes"},
        {offsets_.instance_name_container, "raw-bytes"},
        {0x98, "qword-then-string"},
    };

    for (const auto& [offset, mode] : name_offsets) {
        if (offset == 0) {
            continue;
        }
        NameStrategyResult result;
        result.offset = offset;
        result.mode = mode;

        if (mode == std::string("raw-bytes")) {
            char raw[32]{};
            if (read_memory(process_, probe.data_model + offset, raw, sizeof(raw))) {
                const std::size_t length = strnlen(raw, sizeof(raw));
                result.value.assign(raw, length);
            }
        } else {
            uintptr_t name_ptr = 0;
            if (read_ptr(probe.data_model + offset, name_ptr) && name_ptr != 0) {
                result.value = read_string(process_, name_ptr, 128);
            }
        }

        probe.names.push_back(std::move(result));
    }

    if (find_child_by_class(probe.data_model, "Players", probe.players)) {
        get_local_player(probe.players, probe.local_player);
        if (probe.local_player != 0) {
            get_character(probe.local_player, probe.character);
            if (probe.character != 0) {
                get_humanoid(probe.character, probe.humanoid);
                if (probe.humanoid != 0) {
                    read_health(probe.humanoid, probe.health, probe.max_health);
                }
            }
        }
    }

    return probe;
}

} // namespace zext
