#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ {

bool ResolvedTypeVoid::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeVoid " << location);
    if (dynamic_cast<const ResolvedTypeVoid *>(&other)) {
        return true;
    } else {
        return false;
    }
}

bool ResolvedTypeVoid::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeVoid " << location);
    if (equal(other)) return true;
    return false;
}

ptr<ResolvedType> ResolvedTypeVoid::clone() const {
    debug_func("ResolvedTypeVoid " << location);
    return makePtr<ResolvedTypeVoid>(location);
}

void ResolvedTypeVoid::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeVoid " << to_str() << "\n";
}

std::string ResolvedTypeVoid::to_str() const { return "void"; }

bool ResolvedTypeNumber::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeNumber " << location);
    if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(&other)) {
        return numberKind == numType->numberKind && bitSize == numType->bitSize;
    } else {
        return false;
    }
}

bool ResolvedTypeNumber::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeNumber " << location);
    if (equal(other)) return true;

    if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(&other)) {
        // TODO think if is ok to ignore size
        // return numberKind == numType->numberKind;
        return true;
    }
    return false;
}

ptr<ResolvedType> ResolvedTypeNumber::clone() const {
    debug_func("ResolvedTypeNumber " << location);
    return makePtr<ResolvedTypeNumber>(location, numberKind, bitSize);
}

void ResolvedTypeNumber::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeNumber " << to_str() << "\n";
}

std::string ResolvedTypeNumber::to_str() const {
    std::stringstream out;
    switch (numberKind) {
        case ResolvedNumberKind::Int:
            out << "i";
            break;
        case ResolvedNumberKind::UInt:
            out << "u";
            break;
        case ResolvedNumberKind::Float:
            out << "f";
            break;
    }

    out << bitSize;
    return out.str();
}

bool ResolvedTypeBool::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeBool " << location);
    if (dynamic_cast<const ResolvedTypeBool *>(&other)) {
        return true;
    } else {
        return false;
    }
}

bool ResolvedTypeBool::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeBool " << location);
    if (equal(other)) return true;

    if (dynamic_cast<const ResolvedTypeError *>(&other) || dynamic_cast<const ResolvedTypePointer *>(&other)) {
        return true;
    } else if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(&other)) {
        return numType->numberKind == ResolvedNumberKind::Int || numType->numberKind == ResolvedNumberKind::UInt;
    } else {
        return false;
    }
}

ptr<ResolvedType> ResolvedTypeBool::clone() const {
    debug_func("ResolvedTypeBool " << location);
    return makePtr<ResolvedTypeBool>(location);
}

void ResolvedTypeBool::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeBool " << to_str() << "\n";
}

std::string ResolvedTypeBool::to_str() const { return "bool"; }

bool ResolvedTypeStructDecl::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeStructDecl " << location);
    if (auto strType = dynamic_cast<const ResolvedTypeStructDecl *>(&other)) {
        return decl == strType->decl;
    } else {
        return false;
    }
}

bool ResolvedTypeStructDecl::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeStructDecl " << location);
    if (equal(other)) return true;

    return false;
}

ptr<ResolvedType> ResolvedTypeStructDecl::clone() const {
    debug_func("ResolvedTypeStructDecl " << location);
    return makePtr<ResolvedTypeStructDecl>(location, decl);
}

void ResolvedTypeStructDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeStructDecl " << to_str() << "\n";
}

std::string ResolvedTypeStructDecl::to_str() const {
    std::stringstream out;
    if (decl->symbolName.empty()) {
        out << decl->identifier;
        if (auto speStru = dynamic_cast<const ResolvedSpecializedStructDecl *>(decl)) {
            out << speStru->specializedTypes->to_str();
        }
    } else {
        out << decl->symbolName;
    }
    return out.str();
}

bool ResolvedTypeStruct::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeStruct " << location);
    if (auto strType = dynamic_cast<const ResolvedTypeStruct *>(&other)) {
        return decl == strType->decl;
    } else {
        return false;
    }
}

bool ResolvedTypeStruct::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeStruct " << location);
    if (equal(other)) return true;

    return false;
}

ptr<ResolvedType> ResolvedTypeStruct::clone() const {
    debug_func("ResolvedTypeStruct " << location);
    return makePtr<ResolvedTypeStruct>(location, decl);
}

void ResolvedTypeStruct::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeStruct " << to_str() << "\n";
}

std::string ResolvedTypeStruct::to_str() const {
    std::stringstream out;
    if (decl->symbolName.empty()) {
        out << decl->identifier;
        if (auto speStru = dynamic_cast<const ResolvedSpecializedStructDecl *>(decl)) {
            out << speStru->specializedTypes->to_str();
        }
    } else {
        out << decl->symbolName;
    }
    return out.str() + "{}";
}

bool ResolvedTypeGeneric::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeGeneric " << location);
    if (auto genType = dynamic_cast<const ResolvedTypeGeneric *>(&other)) {
        return decl == genType->decl;
    } else {
        return false;
    }
}

bool ResolvedTypeGeneric::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeGeneric " << location);
    if (equal(other)) return true;
    return false;
}

ptr<ResolvedType> ResolvedTypeGeneric::clone() const {
    debug_func("ResolvedTypeGeneric " << location);
    return makePtr<ResolvedTypeGeneric>(location, decl);
}

void ResolvedTypeGeneric::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeGeneric " << to_str() << "\n";
}

std::string ResolvedTypeGeneric::to_str() const { return decl->name(); }

bool ResolvedTypeSpecialized::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeSpecialized " << location);
    if (auto specType = dynamic_cast<const ResolvedTypeSpecialized *>(&other)) {
        if (specializedTypes.size() != specType->specializedTypes.size()) return false;

        for (size_t i = 0; i < specializedTypes.size(); i++) {
            if (!specializedTypes[i]->equal(*specType->specializedTypes[i])) return false;
        }

        return true;
    } else {
        return false;
    }
}

bool ResolvedTypeSpecialized::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeSpecialized " << location);
    if (equal(other)) return true;
    return false;
}

ptr<ResolvedType> ResolvedTypeSpecialized::clone() const {
    debug_func("ResolvedTypeSpecialized " << location);
    std::vector<ptr<ResolvedType>> specTypes;
    specTypes.reserve(specializedTypes.size());
    for (auto &&t : specializedTypes) {
        specTypes.emplace_back(t->clone());
    }
    return makePtr<ResolvedTypeSpecialized>(location, std::move(specTypes));
}

void ResolvedTypeSpecialized::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeSpecialized " << to_str() << "\n";
}

std::string ResolvedTypeSpecialized::to_str() const {
    if (specializedTypes.size() == 0) return "";
    std::stringstream out;
    out << "<";
    for (size_t i = 0; i < specializedTypes.size(); i++) {
        out << specializedTypes[i]->to_str();
        if (i != specializedTypes.size() - 1) {
            out << ", ";
        }
    }
    out << ">";
    return out.str();
}

bool ResolvedTypeError::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeError " << location);
    if (dynamic_cast<const ResolvedTypeError *>(&other)) {
        return true;
    } else {
        return false;
    }
}

bool ResolvedTypeError::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeError " << location);
    if (equal(other)) return true;
    return false;
}

ptr<ResolvedType> ResolvedTypeError::clone() const {
    debug_func("ResolvedTypeError " << location);
    return makePtr<ResolvedTypeError>(location);
}

void ResolvedTypeError::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeError " << to_str() << "\n";
}

std::string ResolvedTypeError::to_str() const { return "error"; }

bool ResolvedTypeErrorGroup::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeErrorGroup " << location);
    if (auto egType = dynamic_cast<const ResolvedTypeErrorGroup *>(&other)) {
        return decl == egType->decl;
    } else {
        return false;
    }
}

bool ResolvedTypeErrorGroup::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeErrorGroup " << location);
    if (equal(other)) return true;
    return false;
}

ptr<ResolvedType> ResolvedTypeErrorGroup::clone() const {
    debug_func("ResolvedTypeErrorGroup " << location);
    return makePtr<ResolvedTypeErrorGroup>(location, decl);
}

void ResolvedTypeErrorGroup::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeErrorGroup " << to_str() << "\n";
}

std::string ResolvedTypeErrorGroup::to_str() const { return "errorGroup"; }

bool ResolvedTypeModule::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeModule " << location);
    if (auto modType = dynamic_cast<const ResolvedTypeModule *>(&other)) {
        return moduleDecl == modType->moduleDecl;
    } else {
        return false;
    }
}

bool ResolvedTypeModule::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeModule " << location);
    if (equal(other)) return true;
    return false;
}

ptr<ResolvedType> ResolvedTypeModule::clone() const {
    debug_func("ResolvedTypeModule " << location);
    return makePtr<ResolvedTypeModule>(location, moduleDecl);
}

void ResolvedTypeModule::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeModule " << to_str() << "\n";
}
std::string ResolvedTypeModule::to_str() const { return moduleDecl->name(); }

bool ResolvedTypeOptional::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeOptional " << location);
    if (auto optType = dynamic_cast<const ResolvedTypeOptional *>(&other)) {
        return optionalType->equal(*optType->optionalType);
    } else {
        return false;
    }
}

bool ResolvedTypeOptional::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeOptional " << location);
    if (dynamic_cast<const ResolvedTypeError *>(&other)) return true;
    if (equal(other)) return true;

    if (auto optType = dynamic_cast<const ResolvedTypeOptional *>(&other)) {
        return optionalType->compare(*optType->optionalType);
    } else {
        return optionalType->compare(other);
    }
}

ptr<ResolvedType> ResolvedTypeOptional::clone() const {
    debug_func("ResolvedTypeOptional " << location);
    return makePtr<ResolvedTypeOptional>(location, optionalType->clone());
}

void ResolvedTypeOptional::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeOptional " << to_str() << "\n";
}

std::string ResolvedTypeOptional::to_str() const { return optionalType->to_str() + "!"; }

bool ResolvedTypePointer::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypePointer " << location);
    if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(&other)) {
        return pointerType->equal(*ptrType->pointerType);
    } else {
        return false;
    }
}

bool ResolvedTypePointer::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypePointer " << location);
    if (equal(other)) return true;

    if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(&other)) {
        if (pointerType->compare(*ptrType->pointerType)) return true;
        if (dynamic_cast<const ResolvedTypeVoid *>(pointerType.get())) return true;
        if (dynamic_cast<const ResolvedTypeVoid *>(ptrType->pointerType.get())) return true;
    }

    return false;
}

ptr<ResolvedType> ResolvedTypePointer::clone() const {
    debug_func("ResolvedTypePointer " << location);
    return makePtr<ResolvedTypePointer>(location, pointerType->clone());
}

void ResolvedTypePointer::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypePointer " << to_str() << "\n";
}

std::string ResolvedTypePointer::to_str() const { return "*" + pointerType->to_str(); }

bool ResolvedTypeArray::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeArray " << location);
    if (auto arrType = dynamic_cast<const ResolvedTypeArray *>(&other)) {
        return arraySize == arrType->arraySize && arrayType->equal(*arrType->arrayType);
    } else {
        return false;
    }
}

bool ResolvedTypeArray::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeArray " << location);
    if (equal(other)) return true;
    return false;

    dump();
    other.dump();
    dmz_unreachable("TODO");
}

ptr<ResolvedType> ResolvedTypeArray::clone() const {
    debug_func("ResolvedTypeArray " << location);
    return makePtr<ResolvedTypeArray>(location, arrayType->clone(), arraySize);
}

void ResolvedTypeArray::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeArray " << to_str() << "\n";
}

std::string ResolvedTypeArray::to_str() const { return arrayType->to_str() + "[" + std::to_string(arraySize) + "]"; }
}  // namespace DMZ
