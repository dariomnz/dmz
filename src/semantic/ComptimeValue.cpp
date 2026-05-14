#include "semantic/ComptimeValue.hpp"

#include <iostream>
#include <sstream>

#include "Utils.hpp"
#include "semantic/SemanticSymbols.hpp"
#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ {

ComptimeValue::ComptimeValue(const ComptimeValue& other) {
    std::visit(
        [this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, ptr<ResolvedType>>) {
                if (arg)
                    this->value = arg->clone();
                else
                    this->value = ptr<ResolvedType>(nullptr);
            } else {
                this->value = arg;
            }
        },
        other.value);
}

ComptimeValue& ComptimeValue::operator=(const ComptimeValue& other) {
    if (this != &other) {
        ComptimeValue temp(other);
        std::swap(value, temp.value);
    }
    return *this;
}

ptr<ResolvedType> ComptimeValue::getType() const { return std::get<ptr<ResolvedType>>(value)->clone(); }

bool ComptimeValue::Array::operator==(const Array& other) const {
    if (elements.size() != other.elements.size()) return false;
    for (size_t i = 0; i < elements.size(); ++i) {
        if (!(elements[i] == other.elements[i])) return false;
    }
    return true;
}

bool ComptimeValue::Struct::operator==(const Struct& other) const {
    if (fields.size() != other.fields.size()) return false;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (fields[i].first != other.fields[i].first) return false;
        if (!(fields[i].second == other.fields[i].second)) return false;
    }
    return true;
}

bool ComptimeValue::operator==(const ComptimeValue& other) const {
    if (value.index() != other.value.index()) return false;
    return value == other.value;
}

void ComptimeValue::dump() const {
    std::cerr << "ComptimeValue ";
    if (isVoid()) {
        std::cerr << "void";
    } else if (isInt()) {
        std::cerr << "int";
    } else if (isFloat()) {
        std::cerr << "float";
    } else if (isBool()) {
        std::cerr << "bool";
    } else if (isString()) {
        std::cerr << "string";
    } else if (isArray()) {
        std::cerr << "array";
    } else if (isStruct()) {
        std::cerr << "struct";
    } else {
        std::cerr << "unknown";
    }
    std::cerr << ": " << to_str() << "\n";
}

std::string ComptimeValue::to_str() const {
    std::stringstream ss;
    ss << *this;
    return ss.str();
}

std::ostream& operator<<(std::ostream& os, const ComptimeValue& cv) {
    if (cv.isVoid())
        os << "void";
    else if (cv.isInt())
        os << cv.getInt();
    else if (cv.isFloat())
        os << cv.getFloat();
    else if (cv.isBool())
        os << (cv.getBool() ? "true" : "false");
    else if (cv.isString())
        os << '\'' << str_to_source(cv.getString()) << '\'';
    else if (cv.isArray()) {
        os << '{';
        bool first = true;
        for (auto &&elem : cv.getArray().elements) {
            if (!first) os << ", ";
            os << elem;
            first = false;
        }
        os << '}';
    } else if (cv.isStruct()) {
        os << '{';
        bool first = true;
        for (auto &&[key, val] : cv.getStruct().fields) {
            if (!first) os << ", ";
            os << key << ": " << val;
            first = false;
        }
        os << '}';
    }
    else if (cv.isType())
        os << cv.getType()->to_str();
    else
        os << "unknown";
    return os;
}
}  // namespace DMZ
