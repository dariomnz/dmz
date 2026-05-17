// #define DEBUG
#include "semantic/SemanticSymbolsTypes.hpp"

#include <iostream>

#include "Debug.hpp"
#include "Utils.hpp"
#include "codegen/CodegenUtils.hpp"
#include "semantic/SemanticSymbols.hpp"

struct ResolvedExpr;
namespace DMZ {

bool ResolvedType::generate_struct() const {
    return kind == ResolvedTypeKind::Struct || kind == ResolvedTypeKind::StructDecl ||
           kind == ResolvedTypeKind::Union || kind == ResolvedTypeKind::UnionDecl ||
           kind == ResolvedTypeKind::Optional || kind == ResolvedTypeKind::Slice;
}

bool ResolvedTypeVoid::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeVoid " << to_str() << " " << other.to_str() << " " << location);
    if (other.kind == ResolvedTypeKind::Void) {
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeVoid::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeVoid " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeVoid::clone() const {
    debug_func("ResolvedTypeVoid " << location);
    return makePtr<ResolvedTypeVoid>(location);
}

void ResolvedTypeVoid::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeVoid " << to_str() << "\n";
}

std::string ResolvedTypeVoid::to_str() const { return "void"; }

bool ResolvedTypeType::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeType " << to_str() << " " << other.to_str() << " " << location);
    return debug_ret(other.kind == ResolvedTypeKind::Type);
}

bool ResolvedTypeType::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeType " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeType::clone() const {
    debug_func("ResolvedTypeType " << location);
    return makePtr<ResolvedTypeType>(location);
}

void ResolvedTypeType::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeType " << to_str() << "\n";
}

std::string ResolvedTypeType::to_str() const { return "type"; }

bool ResolvedTypeNumber::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeNumber " << to_str() << " " << other.to_str() << " " << location);
    if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(&other)) {
        return debug_ret(numberKind == numType->numberKind && bitSize == numType->bitSize);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeNumber::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeNumber " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::Pointer && isPlatformSize) {
        return debug_ret(true);
    }
    if (other.kind == ResolvedTypeKind::Enum) {
        return debug_ret(true);
    }

    if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(&other)) {
        if (numberKind == numType->numberKind) {
            return debug_ret(numType->bitSize <= bitSize);
        }
        if (numberKind == ResolvedNumberKind::Int && numType->numberKind == ResolvedNumberKind::UInt) {
            return debug_ret(numType->bitSize < bitSize);
        }
    }

    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeNumber::clone() const {
    debug_func("ResolvedTypeNumber " << location);
    return makePtr<ResolvedTypeNumber>(location, numberKind, bitSize, isPlatformSize);
}

void ResolvedTypeNumber::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeNumber " << to_str() << "\n";
}

std::string ResolvedTypeNumber::to_str() const {
    if (isPlatformSize) {
        if (numberKind == ResolvedNumberKind::Int) return "isize";
        if (numberKind == ResolvedNumberKind::UInt) return "usize";
    }
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

ptr<ResolvedType> ResolvedTypeNumber::isize(SourceLocation location) {
    return makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::Int, CodegenUtils::ptrBitSize(), true);
}

ptr<ResolvedType> ResolvedTypeNumber::usize(SourceLocation location) {
    return makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::UInt, CodegenUtils::ptrBitSize(), true);
}

ptr<ResolvedType> ResolvedTypeNumber::u8(SourceLocation location) {
    return makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::UInt, 8, false);
}

bool ResolvedTypeBool::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeBool " << to_str() << " " << other.to_str() << " " << location);
    if (other.kind == ResolvedTypeKind::Bool) {
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeBool::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeBool " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::Error || other.kind == ResolvedTypeKind::Number ||
        other.kind == ResolvedTypeKind::Pointer) {
        return debug_ret(true);
    }
    return debug_ret(ResolvedTypeNumber::compare(other));
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
    debug_func("ResolvedTypeStructDecl " << to_str() << " " << other.to_str() << " " << location);
    if (auto strType = dynamic_cast<const ResolvedTypeStructDecl *>(&other)) {
        return debug_ret(decl == strType->decl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeStructDecl::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeStructDecl " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeStructDecl::clone() const {
    debug_func("ResolvedTypeStructDecl " << location);
    return makePtr<ResolvedTypeStructDecl>(location, decl);
}

void ResolvedTypeStructDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeStructDecl " << to_str() << "\n";

    if (ownedDecl) ownedDecl->dump(level + 1);
}

std::string ResolvedTypeStructDecl::to_str() const {
    if (!decl) return "unknown";
    return decl->name();
}

bool ResolvedTypeStructDecl::is_generic() const {
    if (decl->isGenericVisiting) return debug_ret(false);
    decl->isGenericVisiting = true;
    for (auto &&field : decl->fields) {
        if (field->type->is_generic()) {
            decl->isGenericVisiting = false;
            return debug_ret(true);
        }
    }
    decl->isGenericVisiting = false;
    return debug_ret(false);
}

bool ResolvedTypeStruct::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeStruct " << to_str() << " " << other.to_str() << " " << location);
    if (auto strType = dynamic_cast<const ResolvedTypeStruct *>(&other)) {
        return debug_ret(decl == strType->decl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeStruct::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeStruct " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeStruct::clone() const {
    debug_func("ResolvedTypeStruct " << location);
    return makePtr<ResolvedTypeStruct>(location, decl);
}

void ResolvedTypeStruct::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeStruct " << to_str() << "\n";
}

std::string ResolvedTypeStruct::to_str() const {
    if (!decl) return "unknown{}";
    return decl->name() + "{}";
}

bool ResolvedTypeStruct::is_generic() const {
    if (decl->isGenericVisiting) return debug_ret(false);
    decl->isGenericVisiting = true;
    for (auto &&field : decl->fields) {
        if (field->type->is_generic()) {
            decl->isGenericVisiting = false;
            return debug_ret(true);
        }
    }
    decl->isGenericVisiting = false;
    return debug_ret(false);
}

ResolvedTypeUnionDecl::ResolvedTypeUnionDecl(SourceLocation location, ResolvedUnionDecl *decl)
    : ResolvedTypeStructDecl(std::move(location), decl) {
    kind = ResolvedTypeKind::UnionDecl;
}

ResolvedUnionDecl *ResolvedTypeUnionDecl::unionDecl() const { return static_cast<ResolvedUnionDecl *>(decl); }

bool ResolvedTypeUnionDecl::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeUnionDecl " << to_str() << " " << other.to_str() << " " << location);
    if (auto strType = dynamic_cast<const ResolvedTypeUnionDecl *>(&other)) {
        return debug_ret(decl == strType->decl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeUnionDecl::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeUnionDecl " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeUnionDecl::clone() const {
    debug_func("ResolvedTypeUnionDecl " << location);
    return makePtr<ResolvedTypeUnionDecl>(location, unionDecl());
}

void ResolvedTypeUnionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeUnionDecl " << to_str() << "\n";

    if (ownedDecl) ownedDecl->dump(level + 1);
}

std::string ResolvedTypeUnionDecl::to_str() const { return decl->name(); }

ResolvedTypeUnion::ResolvedTypeUnion(SourceLocation location, ResolvedUnionDecl *decl)
    : ResolvedTypeStruct(std::move(location), decl) {
    kind = ResolvedTypeKind::Union;
}

ResolvedUnionDecl *ResolvedTypeUnion::unionDecl() const { return static_cast<ResolvedUnionDecl *>(decl); }

bool ResolvedTypeUnion::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeUnion " << to_str() << " " << other.to_str() << " " << location);
    if (auto strType = dynamic_cast<const ResolvedTypeUnion *>(&other)) {
        return debug_ret(decl == strType->decl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeUnion::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeUnion " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeUnion::clone() const {
    debug_func("ResolvedTypeUnion " << location);
    return makePtr<ResolvedTypeUnion>(location, unionDecl());
}

void ResolvedTypeUnion::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeUnion " << to_str() << "\n";
}

std::string ResolvedTypeUnion::to_str() const {
    if (!decl) return "unknown{}";
    return decl->name() + "{}";
}

ResolvedTypeEnumDecl::ResolvedTypeEnumDecl(SourceLocation location, ResolvedEnumDecl *decl)
    : ResolvedTypeStructDecl(std::move(location), decl) {
    kind = ResolvedTypeKind::EnumDecl;
}

ResolvedEnumDecl *ResolvedTypeEnumDecl::enumDecl() const { return static_cast<ResolvedEnumDecl *>(decl); }

bool ResolvedTypeEnumDecl::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeEnumDecl " << to_str() << " " << other.to_str() << " " << location);
    if (auto strType = dynamic_cast<const ResolvedTypeEnumDecl *>(&other)) {
        return debug_ret(decl == strType->decl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeEnumDecl::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeEnumDecl " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeEnumDecl::clone() const {
    debug_func("ResolvedTypeEnumDecl " << location);
    return makePtr<ResolvedTypeEnumDecl>(location, enumDecl());
}

void ResolvedTypeEnumDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeEnumDecl " << to_str() << "\n";

    if (ownedDecl) ownedDecl->dump(level + 1);
}

std::string ResolvedTypeEnumDecl::to_str() const { return decl->name(); }

ResolvedTypeEnum::ResolvedTypeEnum(SourceLocation location, ResolvedEnumDecl *decl)
    : ResolvedTypeStruct(std::move(location), decl) {
    kind = ResolvedTypeKind::Enum;
}

ResolvedEnumDecl *ResolvedTypeEnum::enumDecl() const { return static_cast<ResolvedEnumDecl *>(decl); }

bool ResolvedTypeEnum::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeEnum " << to_str() << " " << other.to_str() << " " << location);
    if (auto strType = dynamic_cast<const ResolvedTypeEnum *>(&other)) {
        return debug_ret(decl == strType->decl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeEnum::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeEnum " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit || other.kind == ResolvedTypeKind::Number) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeEnum::clone() const {
    debug_func("ResolvedTypeEnum " << location);
    return makePtr<ResolvedTypeEnum>(location, enumDecl());
}

void ResolvedTypeEnum::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeEnum " << to_str() << "\n";
}

std::string ResolvedTypeEnum::to_str() const {
    if (!decl) return "unknown{}";
    return decl->name() + "{}";
}

bool ResolvedTypeSpecialized::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeSpecialized " << to_str() << " " << other.to_str() << " " << location);
    if (auto specType = dynamic_cast<const ResolvedTypeSpecialized *>(&other)) {
        if (specializedTypes.size() != specType->specializedTypes.size()) return debug_ret(false);

        for (size_t i = 0; i < specializedTypes.size(); i++) {
            if (!specializedTypes[i]->equal(*specType->specializedTypes[i])) return debug_ret(false);
        }

        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeSpecialized::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeSpecialized " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    return debug_ret(false);
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
    out << "(";
    for (size_t i = 0; i < specializedTypes.size(); i++) {
        out << specializedTypes[i]->to_str();
        if (i != specializedTypes.size() - 1) {
            out << ", ";
        }
    }
    out << ")";
    return out.str();
}

bool ResolvedTypeSpecialized::is_generic() const {
    for (auto &&specType : specializedTypes) {
        if (specType->is_generic()) {
            return debug_ret(true);
        }
    }
    return debug_ret(false);
}

bool ResolvedTypeError::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeError " << to_str() << " " << other.to_str() << " " << location);
    if (other.kind == ResolvedTypeKind::Error) {
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeError::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeError " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeError::clone() const {
    debug_func("ResolvedTypeError " << location);
    return makePtr<ResolvedTypeError>(location);
}

void ResolvedTypeError::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeError " << to_str() << "\n";
}

std::string ResolvedTypeError::to_str() const { return "err"; }

bool ResolvedTypeErrorGroup::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeErrorGroup " << to_str() << " " << other.to_str() << " " << location);
    if (auto egType = dynamic_cast<const ResolvedTypeErrorGroup *>(&other)) {
        return debug_ret(decl == egType->decl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeErrorGroup::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeErrorGroup " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    return debug_ret(false);
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
    debug_func("ResolvedTypeModule " << to_str() << " " << other.to_str() << " " << location);
    if (auto modType = dynamic_cast<const ResolvedTypeModule *>(&other)) {
        return debug_ret(moduleDecl == modType->moduleDecl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeModule::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeModule " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    return debug_ret(false);
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
    debug_func("ResolvedTypeOptional " << to_str() << " " << other.to_str() << " " << location);
    if (auto optType = dynamic_cast<const ResolvedTypeOptional *>(&other)) {
        return debug_ret(optionalType->equal(*optType->optionalType));
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeOptional::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeOptional " << to_str() << " " << other.to_str() << " " << location);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit || other.kind == ResolvedTypeKind::Error) return debug_ret(true);
    if (equal(other)) return debug_ret(true);

    if (auto optType = dynamic_cast<const ResolvedTypeOptional *>(&other)) {
        return debug_ret(optionalType->compare(*optType->optionalType));
    } else {
        return debug_ret(optionalType->compare(other));
    }
}

ptr<ResolvedType> ResolvedTypeOptional::clone() const {
    debug_func("ResolvedTypeOptional " << location);
    return makePtr<ResolvedTypeOptional>(location, optionalType->clone());
}

void ResolvedTypeOptional::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeOptional " << to_str() << "\n";
}

std::string ResolvedTypeOptional::to_str() const { return "!" + optionalType->to_str(); }

ptr<ResolvedType> ResolvedTypeOptional::voidOptional(SourceLocation location) {
    return makePtr<ResolvedTypeOptional>(location, makePtr<ResolvedTypeVoid>(location));
}

bool ResolvedTypeOptional::is_generic() const { return debug_ret(optionalType->is_generic()); }

bool ResolvedTypePointer::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypePointer " << to_str() << " " << other.to_str() << " " << location);
    if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(&other)) {
        return debug_ret(pointerType->equal(*ptrType->pointerType));
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypePointer::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypePointer " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);

    if (dynamic_cast<const ResolvedTypeError *>(&other)) {
        if (auto numType = dynamic_cast<ResolvedTypeNumber *>(pointerType.get())) {
            if (numType->numberKind == ResolvedNumberKind::UInt && numType->bitSize == 8) {
                return debug_ret(true);
            }
        }
    }

    if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(&other)) {
        if (numType->isPlatformSize) return debug_ret(true);
    }

    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypePointer::clone() const {
    debug_func("ResolvedTypePointer " << location);
    return makePtr<ResolvedTypePointer>(location, pointerType->clone());
}

void ResolvedTypePointer::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypePointer " << to_str() << "\n";
}

std::string ResolvedTypePointer::to_str() const { return "*" + pointerType->to_str(); }

bool ResolvedTypePointer::is_generic() const { return debug_ret(pointerType->is_generic()); }

ptr<ResolvedType> ResolvedTypePointer::opaquePtr(SourceLocation location) {
    return makePtr<ResolvedTypePointer>(location, makePtr<ResolvedTypeVoid>(location));
}

bool ResolvedTypeSlice::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeSlice " << to_str() << " " << other.to_str() << " " << location);
    if (auto ptrType = dynamic_cast<const ResolvedTypeSlice *>(&other)) {
        return debug_ret(sliceType->equal(*ptrType->sliceType));
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeSlice::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeSlice " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);

    if (auto ptrType = dynamic_cast<const ResolvedTypeSlice *>(&other)) {
        if (sliceType->compare(*ptrType->sliceType)) return debug_ret(true);
    }

    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeSlice::clone() const {
    debug_func("ResolvedTypeSlice " << location);
    return makePtr<ResolvedTypeSlice>(location, sliceType->clone());
}

void ResolvedTypeSlice::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeSlice " << to_str() << "\n";
}

std::string ResolvedTypeSlice::to_str() const { return "[]" + sliceType->to_str(); }

bool ResolvedTypeSlice::is_generic() const { return debug_ret(sliceType->is_generic()); }

bool ResolvedTypeRange::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeRange " << to_str() << " " << other.to_str() << " " << location);
    if (other.kind == kind) {
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeRange::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeRange " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeRange::clone() const {
    debug_func("ResolvedTypeRange " << location);
    return makePtr<ResolvedTypeRange>(location);
}

void ResolvedTypeRange::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeRange " << to_str() << "\n";
}

std::string ResolvedTypeRange::to_str() const { return "range"; }

bool ResolvedTypeArray::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeArray " << to_str() << " " << other.to_str() << " " << location);
    if (auto arrType = dynamic_cast<const ResolvedTypeArray *>(&other)) {
        return debug_ret(arraySize == arrType->arraySize && arrayType->equal(*arrType->arrayType));
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeArray::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeArray " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeArray::clone() const {
    debug_func("ResolvedTypeArray " << location);
    if (!arraySizeExpr) {
        return makePtr<ResolvedTypeArray>(location, arrayType->clone(), nullptr, arraySize);
    } else if (auto sizeExpr = dynamic_cast<const ResolvedDeclRefExpr *>(arraySizeExpr.get())) {
        return makePtr<ResolvedTypeArray>(location, arrayType->clone(),
                                          makePtr<ResolvedDeclRefExpr>(sizeExpr->location, sizeExpr->identifier,
                                                                       sizeExpr->decl, sizeExpr->type->clone()),
                                          arraySize);
    } else if (auto sizeExpr = dynamic_cast<const ResolvedIntLiteral *>(arraySizeExpr.get())) {
        return makePtr<ResolvedTypeArray>(location, arrayType->clone(),
                                          makePtr<ResolvedIntLiteral>(sizeExpr->location, sizeExpr->value), arraySize);
    } else {
        dmz_unreachable(location, "TODO");
    }
}

void ResolvedTypeArray::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeArray " << to_str() << "\n";
}

std::string ResolvedTypeArray::to_str() const { return "[" + std::to_string(arraySize) + "]" + arrayType->to_str(); }

bool ResolvedTypeArray::is_generic() const {
    if (arrayType->is_generic()) return true;
    return arraySize == 0 && arraySizeExpr != nullptr;
}

bool ResolvedTypeSimd::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeSimd " << to_str() << " " << other.to_str() << " " << location);
    if (auto vecType = dynamic_cast<const ResolvedTypeSimd *>(&other)) {
        debug_msg("simdSize: " << simdSize << " " << vecType->simdSize);
        debug_msg("simdType: " << simdType->to_str() << " " << vecType->simdType->to_str());
        return debug_ret(simdSize == vecType->simdSize && simdType->equal(*vecType->simdType));
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeSimd::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeSimd " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.is_generic()) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeSimd::clone() const {
    debug_func("ResolvedTypeSimd " << location);
    return makePtr<ResolvedTypeSimd>(location, simdType->clone(), simdSize);
}

void ResolvedTypeSimd::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeSimd " << to_str() << "\n";
}

std::string ResolvedTypeSimd::to_str() const {
    return "@simd(" + simdType->to_str() + ", " + std::to_string(simdSize) + ")";
}

bool ResolvedTypeSimd::is_generic() const { return debug_ret(simdType->is_generic()); }

bool ResolvedTypeFunction::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeFunction " << to_str() << " " << other.to_str() << " " << location);
    if (auto fnType = dynamic_cast<const ResolvedTypeFunction *>(&other)) {
        if (!returnType->equal(*fnType->returnType)) return debug_ret(false);
        if (paramsTypes.size() != fnType->paramsTypes.size()) return debug_ret(false);
        for (size_t i = 0; i < paramsTypes.size(); i++) {
            if (!paramsTypes[i]->equal(*fnType->paramsTypes[i])) return debug_ret(false);
        }
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeFunction::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeFunction " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    if (auto fnType = dynamic_cast<const ResolvedTypeFunction *>(&other)) {
        if (!returnType->compare(*fnType->returnType)) return debug_ret(false);
        if (paramsTypes.size() != fnType->paramsTypes.size()) return debug_ret(false);
        for (size_t i = 0; i < paramsTypes.size(); i++) {
            if (!paramsTypes[i]->compare(*fnType->paramsTypes[i])) return debug_ret(false);
        }
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

ptr<ResolvedType> ResolvedTypeFunction::clone() const {
    debug_func("ResolvedTypeFunction " << location);
    std::vector<ptr<ResolvedType>> clonedparams;
    clonedparams.reserve(paramsTypes.size());
    for (auto &&param : paramsTypes) {
        clonedparams.emplace_back(param->clone());
    }
    return makePtr<ResolvedTypeFunction>(location, fnDecl, std::move(clonedparams), returnType->clone());
}

void ResolvedTypeFunction::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeFunction " << to_str() << "\n";
}

std::string ResolvedTypeFunction::to_str() const { return to_str_with_params(false); }

std::string ResolvedTypeFunction::to_str_with_params(bool with_params) const {
    std::stringstream out;
    out << "fn(";

    for (size_t i = 0; i < paramsTypes.size(); i++) {
        if (with_params && fnDecl) {
            if (!fnDecl->params[i]->isVararg) {
                out << fnDecl->params[i]->identifier << ": ";
            }
        }
        out << paramsTypes[i]->to_str();
        if (i != paramsTypes.size() - 1) {
            out << ", ";
        }
    }
    out << ")->";
    out << returnType->to_str();
    return out.str();
}

bool ResolvedTypeFunction::is_generic() const {
    debug_func("ResolvedTypeFunction " << location);

    if (dynamic_cast<const ResolvedGenericFunctionDecl *>(fnDecl)) {
        return debug_ret(true);
    }
    if (auto *spec = dynamic_cast<const ResolvedSpecializedFunctionDecl *>(fnDecl)) {
        return debug_ret(spec->specializedTypes->is_generic());
    }
    return debug_ret(false);
}

bool ResolvedTypeVarArg::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeVarArg " << to_str() << " " << other.to_str() << " " << location);
    if (other.kind == ResolvedTypeKind::VarArg) {
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeVarArg::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeVarArg " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeVarArg::clone() const {
    debug_func("ResolvedTypeVarArg " << location);
    return makePtr<ResolvedTypeVarArg>(location);
}

void ResolvedTypeVarArg::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeVarArg " << to_str() << "\n";
}

std::string ResolvedTypeVarArg::to_str() const { return "..."; }

bool ResolvedTypeDefaultInit::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeDefaultInit " << to_str() << " " << other.to_str() << " " << location);
    if (other.kind == ResolvedTypeKind::DefaultInit) {
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeDefaultInit::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeDefaultInit " << to_str() << " " << other.to_str() << " " << location);
    if (equal(other)) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::Void) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeDefaultInit::clone() const {
    debug_func("ResolvedTypeDefaultInit " << location);
    return makePtr<ResolvedTypeDefaultInit>(location);
}

void ResolvedTypeDefaultInit::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeDefaultInit " << to_str() << "\n";
}

std::string ResolvedTypeDefaultInit::to_str() const { return "{}"; }

bool ResolvedTypeComptimeValue::equal(const ResolvedType &other) const {
    if (auto otherVal = dynamic_cast<const ResolvedTypeComptimeValue *>(&other)) {
        return *value == *otherVal->value;
    }
    return false;
}

bool ResolvedTypeComptimeValue::compare(const ResolvedType &other) const { return equal(other); }

ptr<ResolvedType> ResolvedTypeComptimeValue::clone() const {
    return makePtr<ResolvedTypeComptimeValue>(location, makePtr<ComptimeValue>(*value));
}

void ResolvedTypeComptimeValue::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeComptimeValue " << to_str() << "\n";
}

std::string ResolvedTypeComptimeValue::to_str() const { return value->to_str(); }

bool ResolvedTypeAnyType::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeAnyType " << to_str() << " " << other.to_str() << " " << location);
    if (specialized) return debug_ret(specialized->equal(other));
    if (auto anyType = dynamic_cast<const ResolvedTypeAnyType *>(&other)) {
        if (anyType->specialized) return debug_ret(equal(*anyType->specialized));
        return debug_ret(genericSlot == anyType->genericSlot);
    }
    return debug_ret(false);
}

bool ResolvedTypeAnyType::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeAnyType " << to_str() << " " << other.to_str() << " " << location);
    if (specialized) return debug_ret(specialized->compare(other));
    if (auto anyType = dynamic_cast<const ResolvedTypeAnyType *>(&other)) {
        if (anyType->specialized) return debug_ret(compare(*anyType->specialized));
    }
    return debug_ret(true);
}

ptr<ResolvedType> ResolvedTypeAnyType::clone() const {
    debug_func("ResolvedTypeAnyType " << location);
    if (specialized) return specialized->clone();
    return makePtr<ResolvedTypeAnyType>(location, genericSlot, name);
}

void ResolvedTypeAnyType::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeAnyType " << to_str() << "\n";
}

std::string ResolvedTypeAnyType::to_str() const {
    if (specialized) return specialized->to_str();
    return name.empty() ? "anytype" : name;
}

}  // namespace DMZ
