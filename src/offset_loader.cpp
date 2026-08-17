#include "offset_loader.h"

#include <stdexcept>
#include <string>

#include "json_parser.h"

namespace zext {

namespace {

const JsonValue& require_group(const JsonValue& root, const std::string& group) {
    const JsonValue* offsets = root.find("Offsets");
    if (offsets == nullptr || !offsets->is_object()) {
        throw std::runtime_error("offsets file is missing the 'Offsets' object");
    }
    const JsonValue* entry = offsets->find(group);
    if (entry == nullptr || !entry->is_object()) {
        throw std::runtime_error("offsets file is missing the '" + group + "' group");
    }
    return *entry;
}

std::int64_t require_offset(const JsonValue& root, const std::string& group,
                            const std::string& key) {
    const JsonValue& entry = require_group(root, group);
    const JsonValue* value = entry.find(key);
    if (value == nullptr || !value->is_number()) {
        throw std::runtime_error("offsets file is missing required offset '"
                                 + group + "." + key + "'");
    }
    const std::int64_t parsed = value->as_int64();
    if (parsed < 0) {
        throw std::runtime_error("offsets file contains a negative offset '"
                                 + group + "." + key + "'");
    }
    return parsed;
}

std::int64_t optional_offset(const JsonValue& root, const std::string& group,
                             const std::string& key, std::int64_t fallback) {
    const JsonValue* offsets = root.find("Offsets");
    if (offsets == nullptr || !offsets->is_object()) {
        return fallback;
    }
    const JsonValue* entry = offsets->find(group);
    if (entry == nullptr || !entry->is_object()) {
        return fallback;
    }
    const JsonValue* value = entry->find(key);
    if (value == nullptr || !value->is_number()) {
        return fallback;
    }
    return value->as_int64();
}

} // namespace

RobloxOffsets load_offsets(const std::string& path) {
    JsonValue root;
    try {
        root = parse_json_file(path);
    } catch (const JsonParseError& error) {
        throw std::runtime_error("failed to parse '" + path + "': " + error.what());
    }

    RobloxOffsets offsets;

    const JsonValue* version = root.find("Roblox Version");
    if (version != nullptr && version->is_string()) {
        offsets.roblox_version = version->as_string();
    }

    offsets.visual_engine_pointer = static_cast<uintptr_t>(
        require_offset(root, "VisualEngine", "Pointer"));
    offsets.fake_data_model_pointer = static_cast<uintptr_t>(
        require_offset(root, "FakeDataModel", "Pointer"));
    offsets.fake_data_model_real_data_model = static_cast<std::size_t>(
        require_offset(root, "FakeDataModel", "RealDataModel"));
    offsets.visual_engine_fake_data_model = static_cast<std::size_t>(
        require_offset(root, "VisualEngine", "FakeDataModel"));

    offsets.instance_children_start = static_cast<std::size_t>(
        require_offset(root, "Instance", "ChildrenStart"));
    offsets.instance_children_end_delta = static_cast<std::size_t>(
        require_offset(root, "Instance", "ChildrenEnd"));
    offsets.instance_name = static_cast<std::size_t>(
        require_offset(root, "Instance", "Name"));
    offsets.instance_name_container = static_cast<std::size_t>(
        optional_offset(root, "Instance", "NameContainer", 0));
    offsets.instance_class_descriptor = static_cast<std::size_t>(
        require_offset(root, "Instance", "ClassDescriptor"));
    offsets.instance_class_name = static_cast<std::size_t>(
        require_offset(root, "Instance", "ClassName"));

    offsets.player_local_player = static_cast<std::size_t>(
        require_offset(root, "Player", "LocalPlayer"));
    offsets.player_model_instance = static_cast<std::size_t>(
        require_offset(root, "Player", "ModelInstance"));

    offsets.humanoid_health = static_cast<std::size_t>(
        require_offset(root, "Humanoid", "Health"));
    offsets.humanoid_max_health = static_cast<std::size_t>(
        require_offset(root, "Humanoid", "MaxHealth"));

    return offsets;
}

} // namespace zext
