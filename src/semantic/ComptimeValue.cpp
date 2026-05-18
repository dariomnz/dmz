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
            } else if constexpr (std::is_same_v<T, Union>) {
                Union u{arg.activeTag, arg.activeFieldName, nullptr};
                if (arg.payload) u.payload = makePtr<ComptimeValue>(*arg.payload);
                this->value = std::move(u);
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

bool ComptimeValue::Slice::operator==(const Slice& other) const {
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

bool ComptimeValue::Simd::operator==(const Simd& other) const {
    if (elements.size() != other.elements.size()) return false;
    for (size_t i = 0; i < elements.size(); ++i) {
        if (!(elements[i] == other.elements[i])) return false;
    }
    return true;
}

bool ComptimeValue::Union::operator==(const Union& other) const {
    if (activeTag != other.activeTag) return false;
    if (activeFieldName != other.activeFieldName) return false;
    if (!payload && !other.payload) return true;
    if (!payload || !other.payload) return false;
    return *payload == *other.payload;
}

bool ComptimeValue::operator==(const ComptimeValue& other) const {
    if (value.index() != other.value.index()) return false;
    if (isType()) {
        auto t1 = std::get<ptr<ResolvedType>>(value).get();
        auto t2 = std::get<ptr<ResolvedType>>(other.value).get();
        if (!t1 || !t2) return t1 == t2;
        return t1->equal(*t2);
    }
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
    } else if (isSlice()) {
        std::cerr << "slice";
    } else if (isSimd()) {
        std::cerr << "simd";
    } else if (isStruct()) {
        std::cerr << "struct";
    } else if (isUnion()) {
        std::cerr << "union";
    } else {
        std::cerr << "unknown";
    }
    std::cerr << ": " << *this << "\n";
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
        for (auto&& elem : cv.getArray().elements) {
            if (!first) os << ", ";
            os << elem;
            first = false;
        }
        os << '}';
    } else if (cv.isSlice()) {
        os << '[';
        bool first = true;
        for (auto&& elem : cv.getSlice().elements) {
            if (!first) os << ", ";
            os << elem;
            first = false;
        }
        os << ']';
    } else if (cv.isSimd()) {
        auto& simd = cv.getSimd();
        os << "simd(";
        bool first = true;
        for (auto&& elem : simd.elements) {
            if (!first) os << ", ";
            os << elem;
            first = false;
        }
        os << ')';
    } else if (cv.isStruct()) {
        os << '{';
        bool first = true;
        for (auto&& [key, val] : cv.getStruct().fields) {
            if (!first) os << ", ";
            os << key << ": " << val;
            first = false;
        }
        os << '}';
    } else if (cv.isUnion()) {
        auto& u = cv.getUnion();
        os << "#" << u.activeFieldName << "{";
        if (u.payload) os << *u.payload;
        os << "}";
    } else if (cv.isType())
        os << cv.getType()->to_str();
    else
        os << "unknown";
    return os;
}
}  // namespace DMZ
