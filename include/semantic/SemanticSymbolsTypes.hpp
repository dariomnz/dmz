#pragma once

#include "DMZPCH.hpp"
#include "DMZPCHSymbols.hpp"
#include "lexer/Lexer.hpp"

namespace DMZ {

struct ResolvedType {
    SourceLocation location;
    ResolvedType(SourceLocation location) : location(std::move(location)) {};
    virtual ~ResolvedType() = default;

    virtual bool equal(const ResolvedType &other) const = 0;
    virtual bool compare(const ResolvedType &other) const = 0;
    virtual ptr<ResolvedType> clone() const = 0;
    virtual void dump(size_t level = 0) const = 0;
    virtual std::string to_str() const = 0;
};

struct ResolvedTypeVoid : public ResolvedType {
    ResolvedTypeVoid(SourceLocation location) : ResolvedType(std::move(location)) {}

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
        : ResolvedType(std::move(location)), numberKind(numberKind), bitSize(bitSize) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ResolvedTypeBool : public ResolvedTypeNumber {
    ResolvedTypeBool(SourceLocation location) : ResolvedTypeNumber(location, ResolvedNumberKind::Int, 1) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ResolvedStructDecl;  // Forward declaration
struct ResolvedTypeStruct : public ResolvedType {
    ResolvedStructDecl *decl;
    ResolvedTypeStruct(SourceLocation location, ResolvedStructDecl *decl)
        : ResolvedType(std::move(location)), decl(decl) {}

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
        : ResolvedType(std::move(location)), decl(decl) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ResolvedTypeSpecialized : public ResolvedType {
    ptr<ResolvedType> baseType;
    std::vector<ptr<ResolvedType>> specializedTypes;
    ResolvedTypeSpecialized(SourceLocation location, ptr<ResolvedType> baseType,
                            std::vector<ptr<ResolvedType>> specializedTypes)
        : ResolvedType(std::move(location)),
          baseType(std::move(baseType)),
          specializedTypes(std::move(specializedTypes)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ResolvedErrorDecl;  // Forward declaration
struct ResolvedTypeError : public ResolvedType {
    ResolvedTypeError(SourceLocation location) : ResolvedType(std::move(location)) {}

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
        : ResolvedType(std::move(location)), decl(decl) {}

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
        : ResolvedType(std::move(location)), moduleDecl(moduleDecl) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ResolvedTypeOptional : public ResolvedType {
    ptr<ResolvedType> optionalType;
    ResolvedTypeOptional(SourceLocation location, ptr<ResolvedType> optionalType)
        : ResolvedType(std::move(location)), optionalType(std::move(optionalType)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};

struct ResolvedTypePointer : public ResolvedType {
    ptr<ResolvedType> pointerType;
    ResolvedTypePointer(SourceLocation location, ptr<ResolvedType> pointerType)
        : ResolvedType(std::move(location)), pointerType(std::move(pointerType)) {}

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
        : ResolvedType(std::move(location)), arrayType(std::move(arrayType)), arraySize(std::move(arraySize)) {}

    bool equal(const ResolvedType &other) const override;
    bool compare(const ResolvedType &other) const override;
    ptr<ResolvedType> clone() const override;
    void dump(size_t level = 0) const override;
    std::string to_str() const override;
};
}  // namespace DMZ
