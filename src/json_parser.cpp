#include "json_parser.h"

#include <cmath>
#include <fstream>
#include <sstream>

namespace zext {

JsonValue JsonValue::null() {
    return JsonValue();
}

JsonValue JsonValue::boolean(bool value) {
    JsonValue result;
    result.type_ = JsonType::Bool;
    result.bool_ = value;
    return result;
}

JsonValue JsonValue::number(double value) {
    JsonValue result;
    result.type_ = JsonType::Number;
    result.number_ = value;
    return result;
}

JsonValue JsonValue::string(std::string value) {
    JsonValue result;
    result.type_ = JsonType::String;
    result.string_ = std::move(value);
    return result;
}

bool JsonValue::as_bool() const {
    if (type_ != JsonType::Bool) {
        throw std::runtime_error("JSON value is not a boolean");
    }
    return bool_;
}

double JsonValue::as_number() const {
    if (type_ != JsonType::Number) {
        throw std::runtime_error("JSON value is not a number");
    }
    return number_;
}

std::int64_t JsonValue::as_int64() const {
    return static_cast<std::int64_t>(as_number());
}

const std::string& JsonValue::as_string() const {
    if (type_ != JsonType::String) {
        throw std::runtime_error("JSON value is not a string");
    }
    return string_;
}

const std::vector<JsonValue>& JsonValue::as_array() const {
    if (type_ != JsonType::Array) {
        throw std::runtime_error("JSON value is not an array");
    }
    return array_;
}

const std::vector<std::pair<std::string, JsonValue>>& JsonValue::as_object() const {
    if (type_ != JsonType::Object) {
        throw std::runtime_error("JSON value is not an object");
    }
    return object_;
}

const JsonValue* JsonValue::find(const std::string& key) const {
    if (type_ != JsonType::Object) {
        return nullptr;
    }
    for (const auto& entry : object_) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

void JsonValue::push_back(JsonValue value) {
    if (type_ != JsonType::Array) {
        type_ = JsonType::Array;
    }
    array_.push_back(std::move(value));
}

void JsonValue::insert(std::string key, JsonValue value) {
    if (type_ != JsonType::Object) {
        type_ = JsonType::Object;
    }
    object_.emplace_back(std::move(key), std::move(value));
}

namespace {

class JsonParser {
public:
    explicit JsonParser(const std::string& text)
        : text_(text) {}

    JsonValue parse() {
        skip_whitespace();
        JsonValue value = parse_value();
        skip_whitespace();
        if (pos_ != text_.size()) {
            fail("unexpected trailing content");
        }
        return value;
    }

private:
    const std::string& text_;
    std::size_t pos_ = 0;

    [[noreturn]] void fail(const std::string& message) const {
        std::size_t line = 1;
        std::size_t column = 1;
        for (std::size_t i = 0; i < pos_ && i < text_.size(); ++i) {
            if (text_[i] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        std::ostringstream stream;
        stream << "JSON parse error at line " << line << ", column " << column
               << ": " << message;
        throw JsonParseError(stream.str());
    }

    char peek() const {
        if (pos_ >= text_.size()) {
            fail("unexpected end of input");
        }
        return text_[pos_];
    }

    char take() {
        char c = peek();
        ++pos_;
        return c;
    }

    void skip_whitespace() {
        while (pos_ < text_.size()) {
            char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool consume_literal(const char* literal) {
        std::size_t length = std::char_traits<char>::length(literal);
        if (text_.compare(pos_, length, literal) == 0) {
            pos_ += length;
            return true;
        }
        return false;
    }

    JsonValue parse_value() {
        skip_whitespace();
        char c = peek();
        switch (c) {
        case '{':
            return parse_object();
        case '[':
            return parse_array();
        case '"':
            return JsonValue::string(parse_string());
        case 't':
            if (consume_literal("true")) {
                return JsonValue::boolean(true);
            }
            fail("expected 'true'");
        case 'f':
            if (consume_literal("false")) {
                return JsonValue::boolean(false);
            }
            fail("expected 'false'");
        case 'n':
            if (consume_literal("null")) {
                return JsonValue::null();
            }
            fail("expected 'null'");
        default:
            if (c == '-' || (c >= '0' && c <= '9')) {
                return JsonValue::number(parse_number());
            }
            fail("unexpected character");
        }
    }

    JsonValue parse_object() {
        take();
        JsonValue object;
        skip_whitespace();
        if (peek() == '}') {
            take();
            return object;
        }
        while (true) {
            skip_whitespace();
            if (peek() != '"') {
                fail("expected object key string");
            }
            std::string key = parse_string();
            skip_whitespace();
            if (take() != ':') {
                fail("expected ':' after object key");
            }
            JsonValue value = parse_value();
            object.insert(std::move(key), std::move(value));
            skip_whitespace();
            char c = take();
            if (c == ',') {
                continue;
            }
            if (c == '}') {
                break;
            }
            fail("expected ',' or '}' in object");
        }
        return object;
    }

    JsonValue parse_array() {
        take();
        JsonValue array;
        skip_whitespace();
        if (peek() == ']') {
            take();
            return array;
        }
        while (true) {
            array.push_back(parse_value());
            skip_whitespace();
            char c = take();
            if (c == ',') {
                continue;
            }
            if (c == ']') {
                break;
            }
            fail("expected ',' or ']' in array");
        }
        return array;
    }

    std::string parse_string() {
        if (take() != '"') {
            fail("expected string");
        }
        std::string result;
        while (true) {
            char c = take();
            if (c == '"') {
                break;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                fail("unescaped control character in string");
            }
            if (c == '\\') {
                char escape = take();
                switch (escape) {
                case '"':
                    result.push_back('"');
                    break;
                case '\\':
                    result.push_back('\\');
                    break;
                case '/':
                    result.push_back('/');
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                case 'u': {
                    unsigned int codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        char hex = take();
                        codepoint <<= 4;
                        if (hex >= '0' && hex <= '9') {
                            codepoint |= static_cast<unsigned int>(hex - '0');
                        } else if (hex >= 'a' && hex <= 'f') {
                            codepoint |= static_cast<unsigned int>(hex - 'a' + 10);
                        } else if (hex >= 'A' && hex <= 'F') {
                            codepoint |= static_cast<unsigned int>(hex - 'A' + 10);
                        } else {
                            fail("invalid hex digit in \\u escape");
                        }
                    }
                    append_utf8(result, codepoint);
                    break;
                }
                default:
                    fail("invalid escape sequence");
                }
            } else {
                result.push_back(c);
            }
        }
        return result;
    }

    static void append_utf8(std::string& out, unsigned int codepoint) {
        if (codepoint <= 0x7F) {
            out.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0xFFFF) {
            out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }

    double parse_number() {
        std::size_t start = pos_;
        if (pos_ < text_.size() && text_[pos_] == '-') {
            ++pos_;
        }
        while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
            ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
                ++pos_;
            }
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
                ++pos_;
            }
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') {
                ++pos_;
            }
        }
        if (pos_ == start) {
            fail("invalid number");
        }
        std::string token = text_.substr(start, pos_ - start);
        double value = 0.0;
        try {
            std::size_t consumed = 0;
            value = std::stod(token, &consumed);
            if (consumed != token.size()) {
                fail("invalid number");
            }
        } catch (const std::exception&) {
            fail("invalid number");
        }
        return value;
    }
};

} // namespace

JsonValue parse_json(const std::string& text) {
    JsonParser parser(text);
    return parser.parse();
}

JsonValue parse_json_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw JsonParseError("unable to open file: " + path);
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    if (!file.eof() && file.fail()) {
        throw JsonParseError("failed to read file: " + path);
    }
    return parse_json(stream.str());
}

} // namespace zext
