#include "semantic/ComptimeValue.hpp"

#include <iostream>
#include <sstream>

#include "Utils.hpp"

namespace DMZ {

bool ComptimeValue::Array::operator==(const Array& other) const {
    if (elements.size() != other.elements.size()) return false;
    for (size_t i = 0; i < elements.size(); ++i) {
        if (!elements[i] || !other.elements[i]) {
            if (elements[i] != other.elements[i]) return false;
            continue;
        }
        if (!(*elements[i] == *other.elements[i])) return false;
    }
    return true;
}

bool ComptimeValue::Struct::operator==(const Struct& other) const {
    if (fields.size() != other.fields.size()) return false;
    for (auto const& [key, val] : fields) {
        auto it = other.fields.find(key);
        if (it == other.fields.end()) return false;
        if (!val || !it->second) {
            if (val != it->second) return false;
            continue;
        }
        if (!(*val == *it->second)) return false;
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
    else if (cv.isArray())
        os << "array";
    else if (cv.isStruct())
        os << "struct";
    else
        os << "unknown";
    return os;
}
}  // namespace DMZ
