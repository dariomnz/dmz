#pragma once

#include "DMZPCH.hpp"
#include "DMZPCHSymbols.hpp"
#include "lexer/Lexer.hpp"

namespace DMZ {

enum class ResolvedTypeKind {
    Void,
    Number,
    Bool,
    StructDecl,
    Struct,
    Generic,
    Specialized,
    Error,
    ErrorGroup,
    Module,
    Optional,
    Pointer,
    Array,
};

struct ResolvedType {
    ResolvedTypeKind kind;
    SourceLocation location;
    ResolvedType(ResolvedTypeKind kind, SourceLocation location) : kind(kind), location(std::move(location)) {};
    virtual ~ResolvedType() = default;

    virtual bool equal(const ResolvedType &other) const = 0;
    virtual bool compare(const ResolvedType &other) const = 0;
    virtual ptr<ResolvedType> clone() const = 0;
    virtual void dump(size_t level = 0) const = 0;
    virtual std::string to_str() const = 0;
};

struct ResolvedTypeVoid : public ResolvedType {
    ResolvedTypeVoid(SourceLocation location) : ResolvedType(ResolvedTypeKind::Void, std::move(location)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

enum class ResolvedNumberKind { Int, UInt, Float };
struct ResolvedTypeNumber : public ResolvedType {
    ResolvedNumberKind numberKind;
    int bitSize;
    ResolvedTypeNumber(SourceLocation location, ResolvedNumberKind numberKind, int bitSize)
        : ResolvedType(ResolvedTypeKind::Number, std::move(location)), numberKind(numberKind), bitSize(bitSize) {
        if (numberKind == ResolvedNumberKind::Int && bitSize == 1) {
            kind = ResolvedTypeKind::Bool;
        }
    }

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ResolvedTypeBool : public ResolvedTypeNumber {
    ResolvedTypeBool(SourceLocation location) : ResolvedTypeNumber(std::move(location), ResolvedNumberKind::Int, 1) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ResolvedStructDecl;  // Forward declaration
struct ResolvedTypeStructDecl : public ResolvedType {
    ResolvedStructDecl *decl;
    ResolvedTypeStructDecl(SourceLocation location, ResolvedStructDecl *decl)
        : ResolvedType(ResolvedTypeKind::StructDecl, std::move(location)), decl(decl) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
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
};

struct ResolvedErrorDecl;  // Forward declaration
struct ResolvedTypeError : public ResolvedType {
    ResolvedTypeError(SourceLocation location) : ResolvedType(ResolvedTypeKind::Error, std::move(location)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
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
};

struct ResolvedTypeArray : public ResolvedType {
    ptr<ResolvedType> arrayType;
    int arraySize;
    ResolvedTypeArray(SourceLocation location, ptr<ResolvedType> arrayType, int arraySize)
        : ResolvedType(ResolvedTypeKind::Array, std::move(location)),
          arrayType(std::move(arrayType)),
          arraySize(std::move(arraySize)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};
}  // namespace DMZ
