#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ {

bool ResolvedTypeVoid::equal(const ResolvedType &other) const { dmz_unreachable("TODO"); }
bool ResolvedTypeVoid::compare(const ResolvedType &other) const { dmz_unreachable("TODO"); }

ptr<ResolvedType> ResolvedTypeVoid::clone() const { return makePtr<ResolvedTypeVoid>(location); }

void ResolvedTypeVoid::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeVoid " << to_str() << "\n";
}

std::string ResolvedTypeVoid::to_str() const { return "void"; }

bool ResolvedTypeNumber::equal(const ResolvedType &other) const {
    if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(&other)) {
        return numberKind == numType->numberKind && bitSize == numType->bitSize;
    } else {
        return false;
    }

    dump();
    other.dump();
    dmz_unreachable("TODO");
}
bool ResolvedTypeNumber::compare(const ResolvedType &other) const {
    if (equal(other)) return true;

    if (dynamic_cast<const ResolvedTypePointer *>(&other)) {
        return false;
    }
    dump();
    other.dump();
    dmz_unreachable("TODO");
}

ptr<ResolvedType> ResolvedTypeNumber::clone() const {
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

bool ResolvedTypeStruct::equal(const ResolvedType &other) const {
    if (auto strType = dynamic_cast<const ResolvedTypeStruct *>(&other)) {
        return decl == strType->decl;
    } else {
        return false;
    }
}

bool ResolvedTypeStruct::compare(const ResolvedType &other) const {
    if (equal(other)) return true;

    dump();
    other.dump();
    dmz_unreachable("TODO");
}

ptr<ResolvedType> ResolvedTypeStruct::clone() const { return makePtr<ResolvedTypeStruct>(location, decl); }

void ResolvedTypeStruct::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeStruct " << to_str() << "\n";
}

std::string ResolvedTypeStruct::to_str() const {
    std::stringstream out;
    out << decl->name();
    if (auto speStru = dynamic_cast<const ResolvedSpecializedStructDecl *>(decl)) {
        out << speStru->specializedTypes->to_str();
    }
    return out.str();
}

bool ResolvedTypeGeneric::equal(const ResolvedType &other) const { dmz_unreachable("TODO"); }
bool ResolvedTypeGeneric::compare(const ResolvedType &other) const { dmz_unreachable("TODO"); }
ptr<ResolvedType> ResolvedTypeGeneric::clone() const { dmz_unreachable("TODO"); }
void ResolvedTypeGeneric::dump(size_t level) const { dmz_unreachable("TODO"); }
std::string ResolvedTypeGeneric::to_str() const { dmz_unreachable("TODO"); }

bool ResolvedTypeSpecialized::equal(const ResolvedType &other) const { dmz_unreachable("TODO"); }
bool ResolvedTypeSpecialized::compare(const ResolvedType &other) const { dmz_unreachable("TODO"); }
ptr<ResolvedType> ResolvedTypeSpecialized::clone() const { dmz_unreachable("TODO"); }
void ResolvedTypeSpecialized::dump(size_t level) const { dmz_unreachable("TODO"); }
std::string ResolvedTypeSpecialized::to_str() const { dmz_unreachable("TODO"); }

bool ResolvedTypeError::equal(const ResolvedType &other) const { dmz_unreachable("TODO"); }
bool ResolvedTypeError::compare(const ResolvedType &other) const { dmz_unreachable("TODO"); }
ptr<ResolvedType> ResolvedTypeError::clone() const { dmz_unreachable("TODO"); }
void ResolvedTypeError::dump(size_t level) const { dmz_unreachable("TODO"); }
std::string ResolvedTypeError::to_str() const { dmz_unreachable("TODO"); }

bool ResolvedTypeErrorGroup::equal(const ResolvedType &other) const {
    if (auto egType = dynamic_cast<const ResolvedTypeErrorGroup *>(&other)) {
        return decl == egType->decl;
    } else {
        return false;
    }
}

bool ResolvedTypeErrorGroup::compare(const ResolvedType &other) const {
    if (equal(other)) return true;
    return false;
}

ptr<ResolvedType> ResolvedTypeErrorGroup::clone() const { return makePtr<ResolvedTypeErrorGroup>(location, decl); }
void ResolvedTypeErrorGroup::dump(size_t level) const { dmz_unreachable("TODO"); }
std::string ResolvedTypeErrorGroup::to_str() const { dmz_unreachable("TODO"); }

bool ResolvedTypeModule::equal(const ResolvedType &other) const {
    if (auto modType = dynamic_cast<const ResolvedTypeModule *>(&other)) {
        return moduleDecl == modType->moduleDecl;
    } else {
        return false;
    }
}

bool ResolvedTypeModule::compare(const ResolvedType &other) const {
    if (equal(other)) return true;
    return false;
}

ptr<ResolvedType> ResolvedTypeModule::clone() const { return makePtr<ResolvedTypeModule>(location, moduleDecl); }

void ResolvedTypeModule::dump(size_t level) const { dmz_unreachable("TODO"); }
std::string ResolvedTypeModule::to_str() const { dmz_unreachable("TODO"); }

bool ResolvedTypeOptional::equal(const ResolvedType &other) const { dmz_unreachable("TODO"); }
bool ResolvedTypeOptional::compare(const ResolvedType &other) const { dmz_unreachable("TODO"); }
ptr<ResolvedType> ResolvedTypeOptional::clone() const { dmz_unreachable("TODO"); }
void ResolvedTypeOptional::dump(size_t level) const { dmz_unreachable("TODO"); }
std::string ResolvedTypeOptional::to_str() const { dmz_unreachable("TODO"); }

bool ResolvedTypePointer::equal(const ResolvedType &other) const {
    if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(&other)) {
        return pointerType->equal(*ptrType->pointerType);
    } else {
        return false;
    }
}

bool ResolvedTypePointer::compare(const ResolvedType &other) const {
    if (equal(other)) return true;

    dump();
    other.dump();
    dmz_unreachable("TODO");
}

ptr<ResolvedType> ResolvedTypePointer::clone() const {
    return makePtr<ResolvedTypePointer>(location, pointerType->clone());
}

void ResolvedTypePointer::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypePointer " << to_str() << "\n";
}

std::string ResolvedTypePointer::to_str() const { return "*" + pointerType->to_str(); }

bool ResolvedTypeArray::equal(const ResolvedType &other) const { dmz_unreachable("TODO"); }
bool ResolvedTypeArray::compare(const ResolvedType &other) const { dmz_unreachable("TODO"); }
ptr<ResolvedType> ResolvedTypeArray::clone() const { dmz_unreachable("TODO"); }
void ResolvedTypeArray::dump(size_t level) const { dmz_unreachable("TODO"); }
std::string ResolvedTypeArray::to_str() const { dmz_unreachable("TODO"); }
}  // namespace DMZ
