#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace zext {

enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
};

class JsonParseError : public std::runtime_error {
public:
    explicit JsonParseError(const std::string& message)
        : std::runtime_error(message) {}
};

class JsonValue {
public:
    JsonValue() = default;

    static JsonValue null();
    static JsonValue boolean(bool value);
    static JsonValue number(double value);
    static JsonValue string(std::string value);

    JsonType type() const { return type_; }
    bool is_null() const { return type_ == JsonType::Null; }
    bool is_bool() const { return type_ == JsonType::Bool; }
    bool is_number() const { return type_ == JsonType::Number; }
    bool is_string() const { return type_ == JsonType::String; }
    bool is_array() const { return type_ == JsonType::Array; }
    bool is_object() const { return type_ == JsonType::Object; }

    bool as_bool() const;
    double as_number() const;
    std::int64_t as_int64() const;
    const std::string& as_string() const;
    const std::vector<JsonValue>& as_array() const;
    const std::vector<std::pair<std::string, JsonValue>>& as_object() const;

    const JsonValue* find(const std::string& key) const;
    void push_back(JsonValue value);
    void insert(std::string key, JsonValue value);

private:
    JsonType type_ = JsonType::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<JsonValue> array_;
    std::vector<std::pair<std::string, JsonValue>> object_;
};

JsonValue parse_json(const std::string& text);
JsonValue parse_json_file(const std::string& path);

} // namespace zext
