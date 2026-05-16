#pragma once

#include "Utils.hpp"
#include "UtilsPtr.hpp"

namespace DMZ {

enum class ResolvedTypeKind {
    Type,
    Void,
    Number,
    Bool,
    StructDecl,
    Struct,
    UnionDecl,
    Union,
    EnumDecl,
    Enum,
    Generic,
    Specialized,
    Error,
    ErrorGroup,
    Module,
    Optional,
    Pointer,
    Slice,
    Range,
    Array,
    Simd,
    Function,
    VarArg,
    DefaultInit,
    ComptimeValue,
    AnyType,
};

struct ResolvedType {
    ResolvedTypeKind kind;
    SourceLocation location;

    ResolvedType(ResolvedTypeKind kind, SourceLocation location) : kind(kind), location(std::move(location)) {}
    virtual ~ResolvedType() = default;

    virtual bool equal(const ResolvedType &other) const = 0;
    virtual bool compare(const ResolvedType &other) const = 0;
    virtual ptr<ResolvedType> clone() const = 0;
    virtual void dump(size_t level = 0) const = 0;
    virtual std::string to_str() const = 0;

    virtual bool is_generic() const { return false; };
    bool generate_struct() const;
    virtual std::string_view className() const { return type_name<decltype(*this)>(); }
};

struct ResolvedTypeVoid : public ResolvedType {
    ResolvedTypeVoid(SourceLocation location) : ResolvedType(ResolvedTypeKind::Void, std::move(location)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeType : public ResolvedType {
    ResolvedTypeType(SourceLocation location) : ResolvedType(ResolvedTypeKind::Type, std::move(location)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
};

enum class ResolvedNumberKind { Int, UInt, Float };
struct ResolvedTypeNumber : public ResolvedType {
    ResolvedNumberKind numberKind;
    int bitSize;
    bool isPlatformSize;
    ResolvedTypeNumber(SourceLocation location, ResolvedNumberKind numberKind, int bitSize, bool isPlatformSize = false)
        : ResolvedType(ResolvedTypeKind::Number, std::move(location)),
          numberKind(numberKind),
          bitSize(bitSize),
          isPlatformSize(isPlatformSize) {
        if (numberKind == ResolvedNumberKind::Int && bitSize == 1) {
            kind = ResolvedTypeKind::Bool;
        }
    }

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();

    static ptr<ResolvedType> isize(SourceLocation location);
    static ptr<ResolvedType> usize(SourceLocation location);
    static ptr<ResolvedType> u8(SourceLocation location);
};

struct ResolvedTypeBool : public ResolvedTypeNumber {
    ResolvedTypeBool(SourceLocation location) : ResolvedTypeNumber(std::move(location), ResolvedNumberKind::Int, 1) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
};

struct ResolvedStructDecl;  // Forward declaration
struct ResolvedUnionDecl;   // Forward declaration
struct ResolvedEnumDecl;    // Forward declaration

struct ResolvedTypeStructDecl : public ResolvedType {
    ResolvedStructDecl *ownedDecl = nullptr;
    ResolvedStructDecl *decl;
    ResolvedTypeStructDecl(SourceLocation location, ResolvedStructDecl *decl)
        : ResolvedType(ResolvedTypeKind::StructDecl, std::move(location)), decl(decl) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    bool is_generic() const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeStruct : public ResolvedType {
    ResolvedStructDecl *decl;
    ResolvedTypeStruct(SourceLocation location, ResolvedStructDecl *decl)
        : ResolvedType(ResolvedTypeKind::Struct, std::move(location)), decl(decl) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    bool is_generic() const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeUnionDecl : public ResolvedTypeStructDecl {
    // Implementation in cpp
    ResolvedTypeUnionDecl(SourceLocation location, ResolvedUnionDecl *decl);

    ResolvedUnionDecl *unionDecl() const;

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    bool is_generic() const override { return false; }
    DMZ_TYPE_NAME();
};

struct ResolvedTypeUnion : public ResolvedTypeStruct {
    // Implementation in cpp
    ResolvedTypeUnion(SourceLocation location, ResolvedUnionDecl *decl);

    ResolvedUnionDecl *unionDecl() const;

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    bool is_generic() const override { return false; }
    DMZ_TYPE_NAME();
};

struct ResolvedTypeEnumDecl : public ResolvedTypeStructDecl {
    // Implementation in cpp
    ResolvedTypeEnumDecl(SourceLocation location, ResolvedEnumDecl *decl);

    ResolvedEnumDecl *enumDecl() const;

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    bool is_generic() const override { return false; }
    DMZ_TYPE_NAME();
};

struct ResolvedTypeEnum : public ResolvedTypeStruct {
    // Implementation in cpp
    ResolvedTypeEnum(SourceLocation location, ResolvedEnumDecl *decl);

    ResolvedEnumDecl *enumDecl() const;

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    bool is_generic() const override { return false; }
    DMZ_TYPE_NAME();
};

struct ResolvedGenericTypeDecl;  // Forward declaration
struct ResolvedTypeGeneric : public ResolvedType {
    ResolvedGenericTypeDecl *decl;
    ResolvedTypeGeneric(SourceLocation location, ResolvedGenericTypeDecl *decl)
        : ResolvedType(ResolvedTypeKind::Generic, std::move(location)), decl(decl) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
    bool is_generic() const override;
};

struct ResolvedTypeSpecialized : public ResolvedType {
    std::vector<ptr<ResolvedType>> specializedTypes;
    ResolvedTypeSpecialized(SourceLocation location, std::vector<ptr<ResolvedType>> specializedTypes)
        : ResolvedType(ResolvedTypeKind::Specialized, std::move(location)),
          specializedTypes(std::move(specializedTypes)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
    bool is_generic() const override;
};

struct ResolvedErrorDecl;  // Forward declaration
struct ResolvedTypeError : public ResolvedType {
    ResolvedTypeError(SourceLocation location) : ResolvedType(ResolvedTypeKind::Error, std::move(location)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
};

struct ResolvedErrorGroupExprDecl;  // Forward declaration
struct ResolvedTypeErrorGroup : public ResolvedType {
    ResolvedErrorGroupExprDecl *decl;
    ResolvedTypeErrorGroup(SourceLocation location, ResolvedErrorGroupExprDecl *decl)
        : ResolvedType(ResolvedTypeKind::ErrorGroup, std::move(location)), decl(decl) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
};

struct ResolvedModuleDecl;  // Forward declaration
struct ResolvedTypeModule : public ResolvedType {
    ResolvedModuleDecl *moduleDecl;
    ResolvedTypeModule(SourceLocation location, ResolvedModuleDecl *moduleDecl)
        : ResolvedType(ResolvedTypeKind::Module, std::move(location)), moduleDecl(moduleDecl) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeOptional : public ResolvedType {
    ptr<ResolvedType> optionalType;
    ResolvedTypeOptional(SourceLocation location, ptr<ResolvedType> optionalType)
        : ResolvedType(ResolvedTypeKind::Optional, std::move(location)), optionalType(std::move(optionalType)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
    bool is_generic() const override;

    static ptr<ResolvedType> voidOptional(SourceLocation location);
};

struct ResolvedTypePointer : public ResolvedType {
    ptr<ResolvedType> pointerType;
    ResolvedTypePointer(SourceLocation location, ptr<ResolvedType> pointerType)
        : ResolvedType(ResolvedTypeKind::Pointer, std::move(location)), pointerType(std::move(pointerType)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
    bool is_generic() const override;

    static ptr<ResolvedType> opaquePtr(SourceLocation location);
};

struct ResolvedTypeSlice : public ResolvedType {
    ptr<ResolvedType> sliceType;
    ResolvedTypeSlice(SourceLocation location, ptr<ResolvedType> sliceType)
        : ResolvedType(ResolvedTypeKind::Slice, std::move(location)), sliceType(std::move(sliceType)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
    bool is_generic() const override;
};

struct ResolvedTypeRange : public ResolvedType {
    ResolvedTypeRange(SourceLocation location) : ResolvedType(ResolvedTypeKind::Range, std::move(location)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
};

struct ResolvedExpr;  // Forward declaration
struct ResolvedTypeArray : public ResolvedType {
    ptr<ResolvedType> arrayType;
    ptr<ResolvedExpr> arraySizeExpr;
    int arraySize;
    ResolvedTypeArray(SourceLocation location, ptr<ResolvedType> arrayType, ptr<ResolvedExpr> arraySizeType,
                      int arraySize)
        : ResolvedType(ResolvedTypeKind::Array, std::move(location)),
          arrayType(std::move(arrayType)),
          arraySizeExpr(std::move(arraySizeType)),
          arraySize(arraySize) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
    bool is_generic() const override;
};

struct ResolvedTypeSimd : public ResolvedType {
    ptr<ResolvedType> simdType;
    int simdSize;
    ResolvedTypeSimd(SourceLocation location, ptr<ResolvedType> vectorType, int vectorSize)
        : ResolvedType(ResolvedTypeKind::Simd, std::move(location)),
          simdType(std::move(vectorType)),
          simdSize(vectorSize) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
    bool is_generic() const override;
};

struct ResolvedFuncDecl;  // Forward declaration
struct ResolvedTypeFunction : public ResolvedType {
    ResolvedFuncDecl *fnDecl;
    std::vector<ptr<ResolvedType>> paramsTypes;
    ptr<ResolvedType> returnType;

    ResolvedTypeFunction(SourceLocation location, ResolvedFuncDecl *fnDecl, std::vector<ptr<ResolvedType>> paramsTypes,
                         ptr<ResolvedType> returnType)
        : ResolvedType(ResolvedTypeKind::Function, std::move(location)),
          fnDecl(fnDecl),
          paramsTypes(std::move(paramsTypes)),
          returnType(std::move(returnType)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
    std::string to_str_with_params(bool with_params = true) const;

    bool is_generic() const override;
};

struct ResolvedTypeVarArg : public ResolvedType {
    ResolvedTypeVarArg(SourceLocation location) : ResolvedType(ResolvedTypeKind::VarArg, std::move(location)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeDefaultInit : public ResolvedType {
    ResolvedTypeDefaultInit(SourceLocation location)
        : ResolvedType(ResolvedTypeKind::DefaultInit, std::move(location)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
};

struct ComptimeValue;  // Forward declaration
struct ResolvedTypeComptimeValue : public ResolvedType {
    ptr<ComptimeValue> value;
    ResolvedTypeComptimeValue(SourceLocation location, ptr<ComptimeValue> value)
        : ResolvedType(ResolvedTypeKind::ComptimeValue, std::move(location)), value(std::move(value)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
};

struct ResolvedTypeAnyType : public ResolvedType {
    ResolvedTypeAnyType(SourceLocation location) : ResolvedType(ResolvedTypeKind::AnyType, std::move(location)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
    DMZ_TYPE_NAME();
    bool is_generic() const override { return true; }
};
}  // namespace DMZ
