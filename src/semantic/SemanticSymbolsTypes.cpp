#include "semantic/SemanticSymbolsTypes.hpp"

#include "DMZPCH.hpp"
#include "Utils.hpp"
#include "driver/Driver.hpp"

namespace DMZ {

bool ResolvedType::generate_struct() const {
    return kind == ResolvedTypeKind::Struct || kind == ResolvedTypeKind::Optional || kind == ResolvedTypeKind::Slice;
}

bool ResolvedTypeVoid::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeVoid " << location);
    if (other.kind == ResolvedTypeKind::Void) {
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeVoid::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeVoid " << location);
    if (equal(other)) return debug_ret(true);
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

bool ResolvedTypeNumber::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeNumber " << location);
    if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(&other)) {
        return debug_ret(numberKind == numType->numberKind && bitSize == numType->bitSize);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeNumber::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeNumber " << location);
    if (equal(other)) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::Number || other.kind == ResolvedTypeKind::Bool) {
        // TODO think if is ok to ignore size
        // return debug_ret(numberKind == numType->numberKind);
        return debug_ret(true);
    }
    return debug_ret(false);
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

ptr<ResolvedType> ResolvedTypeNumber::isize(SourceLocation location) {
    return makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::Int, Driver::instance().ptrBitSize());
}

bool ResolvedTypeBool::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeBool " << location);
    if (other.kind == ResolvedTypeKind::Bool) {
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeBool::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeBool " << location);
    if (equal(other)) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::Error || other.kind == ResolvedTypeKind::Pointer) {
        return debug_ret(true);
    } else if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(&other)) {
        return debug_ret(numType->numberKind == ResolvedNumberKind::Int ||
                         numType->numberKind == ResolvedNumberKind::UInt);
    } else {
        return debug_ret(false);
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
        return debug_ret(decl == strType->decl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeStructDecl::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeStructDecl " << location);
    if (equal(other)) return debug_ret(true);

    return debug_ret(false);
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
        if (auto genStru = dynamic_cast<const ResolvedGenericStructDecl *>(decl)) {
            out << ResolvedGenericTypeDecl::generic_types_to_str(genStru->genericTypeDecls);
        }
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
        return debug_ret(decl == strType->decl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeStruct::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeStruct " << location);
    if (equal(other)) return debug_ret(true);
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
    std::stringstream out;
    if (decl->symbolName.empty()) {
        out << decl->identifier;
        if (auto genStru = dynamic_cast<const ResolvedGenericStructDecl *>(decl)) {
            out << ResolvedGenericTypeDecl::generic_types_to_str(genStru->genericTypeDecls);
        }
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
        return debug_ret(decl == genType->decl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeGeneric::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeGeneric " << location);
    if (equal(other)) return debug_ret(true);
    return debug_ret(false);
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
    debug_func("ResolvedTypeSpecialized " << location);
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
    if (other.kind == ResolvedTypeKind::Error) {
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeError::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeError " << location);
    if (equal(other)) return debug_ret(true);
    return debug_ret(false);
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
        return debug_ret(decl == egType->decl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeErrorGroup::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeErrorGroup " << location);
    if (equal(other)) return debug_ret(true);
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
    debug_func("ResolvedTypeModule " << location);
    if (auto modType = dynamic_cast<const ResolvedTypeModule *>(&other)) {
        return debug_ret(moduleDecl == modType->moduleDecl);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeModule::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeModule " << location);
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
    debug_func("ResolvedTypeOptional " << location);
    if (auto optType = dynamic_cast<const ResolvedTypeOptional *>(&other)) {
        return debug_ret(optionalType->equal(*optType->optionalType));
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeOptional::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeOptional " << location);
    if (other.kind == ResolvedTypeKind::Error) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
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

bool ResolvedTypePointer::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypePointer " << location);
    if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(&other)) {
        return debug_ret(pointerType->equal(*ptrType->pointerType));
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypePointer::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypePointer " << location);
    if (equal(other)) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);

    if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(&other)) {
        if (pointerType->compare(*ptrType->pointerType)) return debug_ret(true);
        if (pointerType->kind == ResolvedTypeKind::Void) return debug_ret(true);
        if (ptrType->pointerType->kind == ResolvedTypeKind::Void) return debug_ret(true);
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

ptr<ResolvedType> ResolvedTypePointer::opaquePtr(SourceLocation location) {
    return makePtr<ResolvedTypePointer>(location, makePtr<ResolvedTypeVoid>(location));
}

bool ResolvedTypeSlice::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeSlice " << location);
    if (auto ptrType = dynamic_cast<const ResolvedTypeSlice *>(&other)) {
        return debug_ret(sliceType->equal(*ptrType->sliceType));
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeSlice::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeSlice " << location);
    if (equal(other)) return debug_ret(true);
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

bool ResolvedTypeRange::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeRange " << location);
    if (other.kind == kind) {
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeRange::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeRange " << location);
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
    debug_func("ResolvedTypeArray " << location);
    if (auto arrType = dynamic_cast<const ResolvedTypeArray *>(&other)) {
        return debug_ret(arraySize == arrType->arraySize && arrayType->equal(*arrType->arrayType));
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeArray::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeArray " << location);
    if (equal(other)) return debug_ret(true);
    if (other.kind == ResolvedTypeKind::DefaultInit) return debug_ret(true);
    return debug_ret(false);
}

ptr<ResolvedType> ResolvedTypeArray::clone() const {
    debug_func("ResolvedTypeArray " << location);
    return makePtr<ResolvedTypeArray>(location, arrayType->clone(), arraySize);
}

void ResolvedTypeArray::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedTypeArray " << to_str() << "\n";
}

std::string ResolvedTypeArray::to_str() const { return arrayType->to_str() + "[" + std::to_string(arraySize) + "]"; }

bool ResolvedTypeFunction::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeFunction " << location);
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
    debug_func("ResolvedTypeFunction " << location);
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

std::string ResolvedTypeFunction::to_str() const {
    std::stringstream out;
    out << "fn(";
    for (size_t i = 0; i < paramsTypes.size(); i++) {
        out << paramsTypes[i]->to_str();
        if (i != paramsTypes.size() - 1) {
            out << ", ";
        }
    }
    out << ")->";
    out << returnType->to_str();
    return out.str();
}

bool ResolvedTypeVarArg::equal(const ResolvedType &other) const {
    debug_func("ResolvedTypeVarArg " << location);
    if (other.kind == ResolvedTypeKind::VarArg) {
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeVarArg::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeVarArg " << location);
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
    debug_func("ResolvedTypeDefaultInit " << location);
    if (other.kind == ResolvedTypeKind::DefaultInit) {
        return debug_ret(true);
    } else {
        return debug_ret(false);
    }
}

bool ResolvedTypeDefaultInit::compare(const ResolvedType &other) const {
    debug_func("ResolvedTypeDefaultInit " << location);
    if (equal(other)) return debug_ret(true);
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
}  // namespace DMZ
