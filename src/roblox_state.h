#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "offset_loader.h"
#include "process.h"

namespace zext {

struct ChildrenStrategyResult {
    std::size_t offset = 0;
    std::size_t stride = 0;
    std::size_t count = 0;
    std::vector<std::string> classes;
};

struct NameStrategyResult {
    std::size_t offset = 0;
    std::string mode;
    std::string value;
};

struct RobloxProbe {
    uintptr_t data_model = 0;
    std::string data_model_class;
    std::vector<ChildrenStrategyResult> children;
    std::vector<NameStrategyResult> names;
    uintptr_t players = 0;
    uintptr_t local_player = 0;
    uintptr_t character = 0;
    uintptr_t humanoid = 0;
    float health = 0.0f;
    float max_health = 0.0f;
};

class RobloxState {
public:
    explicit RobloxState(const RobloxOffsets& offsets);

    void set_process(HANDLE process, uintptr_t base_address);
    bool attached() const;

    bool get_data_model(uintptr_t& out);
    bool find_child_by_class(uintptr_t instance, const std::string& klass,
                             uintptr_t& out);
    bool get_local_player(uintptr_t players, uintptr_t& out);
    bool get_character(uintptr_t player, uintptr_t& out);
    bool get_humanoid(uintptr_t character, uintptr_t& out);
    bool read_health(uintptr_t humanoid, float& health, float& max_health);

    std::vector<uintptr_t> get_children(uintptr_t instance);
    std::string get_class_name(uintptr_t instance);
    std::string get_name(uintptr_t instance);

    RobloxProbe run_probe();

private:
    bool read_ptr(uintptr_t address, uintptr_t& out) const;
    bool resolve_data_model(uintptr_t fake_data_model, uintptr_t& out);
    std::string class_name_of(uintptr_t instance);

    RobloxOffsets offsets_;
    HANDLE process_ = nullptr;
    uintptr_t base_ = 0;
};

} // namespace zext
