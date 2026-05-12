#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "UtilsPtr.hpp"

namespace DMZ {

struct ResolvedType;  // Forward declaration

struct ComptimeValue {
    struct Void {
        bool operator==(const Void&) const = default;
    };
    struct Array {
        std::vector<ComptimeValue> elements;
        bool operator==(const Array& other) const;
    };
    struct Struct {
        std::unordered_map<std::string, ComptimeValue> fields;
        bool operator==(const Struct& other) const;
    };

    using ValueVariant = std::variant<Void, int64_t, double, bool, std::string, Array, Struct, ptr<ResolvedType>>;

    ValueVariant value;

    ComptimeValue() : value(Void{}) {}
    ComptimeValue(int64_t i) : value(i) {}
    ComptimeValue(double d) : value(d) {}
    ComptimeValue(bool b) : value(b) {}
    ComptimeValue(std::string s) : value(std::move(s)) {}
    ComptimeValue(const char* s) : value(std::string(s)) {}
    ComptimeValue(Array a) : value(std::move(a)) {}
    ComptimeValue(Struct s) : value(std::move(s)) {}
    ComptimeValue(ptr<ResolvedType> t) : value(std::move(t)) {}

    ComptimeValue(const ComptimeValue& other);
    ComptimeValue& operator=(const ComptimeValue& other);
    ComptimeValue(ComptimeValue&& other) noexcept = default;
    ComptimeValue& operator=(ComptimeValue&& other) noexcept = default;

    bool operator==(const ComptimeValue& other) const;

    bool isVoid() const { return std::holds_alternative<Void>(value); }
    bool isInt() const { return std::holds_alternative<int64_t>(value); }
    bool isFloat() const { return std::holds_alternative<double>(value); }
    bool isBool() const { return std::holds_alternative<bool>(value); }
    bool isString() const { return std::holds_alternative<std::string>(value); }
    bool isArray() const { return std::holds_alternative<Array>(value); }
    bool isStruct() const { return std::holds_alternative<Struct>(value); }
    bool isType() const { return std::holds_alternative<ptr<ResolvedType>>(value); }

    int64_t getInt() const { return std::get<int64_t>(value); }
    double getFloat() const { return std::get<double>(value); }
    bool getBool() const { return std::get<bool>(value); }
    const std::string& getString() const { return std::get<std::string>(value); }
    const Array& getArray() const { return std::get<Array>(value); }
    const Struct& getStruct() const { return std::get<Struct>(value); }
    ptr<ResolvedType> getType() const;

    // Conversion to int64_t for backward compatibility where possible
    std::optional<int64_t> toInt() const {
        if (isInt()) return getInt();
        if (isBool()) return getBool() ? 1 : 0;
        return std::nullopt;
    }

    void dump() const;
    std::string to_str() const;
    friend std::ostream& operator<<(std::ostream& os, const ComptimeValue& cv);
};
}  // namespace DMZ
