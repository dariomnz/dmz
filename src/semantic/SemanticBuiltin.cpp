#ifdef DEBUG_SEMANTIC
#ifndef DEBUG
#define DEBUG
#endif
#endif
#include "Debug.hpp"
#include "Utils.hpp"
#include "codegen/CodegenUtils.hpp"
#include "parser/ParserSymbols.hpp"
#include "semantic/ComptimeValue.hpp"
#include "semantic/Semantic.hpp"
#include "semantic/SemanticSymbols.hpp"

// #define DEBUG_SCOPES
// #ifdef DEBUG
// #define DEBUG_SCOPES
// #endif

namespace DMZ {

ResolvedBuiltinFunctionDecl *Sema::resolve_builtin_function_symbol(const DeclRefExpr &declRefExpr,
                                                                   const std::string &fnName) {
    debug_func(fnName);
    auto it = m_funcBuiltins.find(fnName);
    if (it == m_funcBuiltins.end()) {
        return report(declRefExpr.location, "Builtin function " + fnName + " not found");
    }
    if (it->second != nullptr) {
        debug_msg("found");
        return it->second;
    }

    SourceLocation loc = SourceLocation::builtin();
    auto genericTypeExpr = [&]() {
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);
        return makePtr<ResolvedTypeExpr>(loc, genericType->clone());
    };
    if (fnName == "@call") {
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "fn", genericTypeExpr(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "args", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), genericType->clone());
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        debug_msg("created @call");
        return &funcDecl;
    } else if (fnName == "@atomicLoad") {
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "atomicPtr", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), genericType->clone());
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        debug_msg("created @atomicLoad");
        return &funcDecl;
    } else if (fnName == "@atomicStore") {
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "atomicPtr", genericTypeExpr(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "value", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), genericType->clone());
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        debug_msg("created @atomicStore");
        return &funcDecl;
    } else if (fnName == "@atomicCmpExS" || fnName == "@atomicCmpExW") {
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "atomicPtr", genericTypeExpr(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "expected", genericTypeExpr(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "desired", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        paramsTypes.emplace_back(params[2]->type->clone());
        auto fnType =
            makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), makePtr<ResolvedTypeBool>(loc));
        if (fnName == "@atomicCmpExS") {
            static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
            it->second = &funcDecl;
        } else if (fnName == "@atomicCmpExW") {
            static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
            it->second = &funcDecl;
        }
        debug_msg("created " + fnName);
        return it->second;
    } else if (fnName == "@atomicRmw") {
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "atom", genericTypeExpr(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(
            loc, "op", makePtr<ResolvedTypeExpr>(loc, makePtr<ResolvedTypeEnum>(loc, nullptr)), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "value", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        paramsTypes.emplace_back(params[2]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), genericType->clone());
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        debug_msg("created @atomicRmw");
        return &funcDecl;
    } else if (fnName == "@sizeof") {
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "type", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes),
                                                    makePtr<ResolvedTypeNumber>(loc, ResolvedNumberKind::UInt, 64));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@typeof") {
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(
            makePtr<ResolvedParamDecl>(loc, "expr", ResolvedTypeExpr::fromType(genericType->clone()), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto fnType =
            makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), makePtr<ResolvedTypeType>(loc));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@typeid") {
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "expr", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes),
                                                    makePtr<ResolvedTypeNumber>(loc, ResolvedNumberKind::UInt, 64));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@typeinfo") {
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "type", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes),
                                                    makePtr<ResolvedTypePointer>(loc, makePtr<ResolvedTypeVoid>(loc)));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@hasMethod") {
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "type", genericTypeExpr(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "method", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        auto fnType =
            makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), makePtr<ResolvedTypeBool>(loc));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@simd") {
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(
            loc, "type", ResolvedTypeExpr::fromType(makePtr<ResolvedTypeType>(loc)), false, true));
        params.emplace_back(makePtr<ResolvedParamDecl>(
            loc, "size", ResolvedTypeExpr::fromType(makePtr<ResolvedTypeAnyType>(loc)), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        auto fnType =
            makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), makePtr<ResolvedTypeType>(loc));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        debug_msg("created @simd");
        return &funcDecl;
    } else if (fnName == "@simdSize") {
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "type", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes),
                                                    makePtr<ResolvedTypeNumber>(loc, ResolvedNumberKind::UInt, 64));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@simdSplat") {
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "value", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes),
                                                    makePtr<ResolvedTypeSimd>(loc, makePtr<ResolvedTypeVoid>(loc), 0));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@simdIota") {
        std::vector<ptr<ResolvedParamDecl>> params;
        std::vector<ptr<ResolvedType>> paramsTypes;
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes),
                                                    makePtr<ResolvedTypeSimd>(loc, makePtr<ResolvedTypeVoid>(loc), 0));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@errorTrace") {
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);
        std::vector<ptr<ResolvedParamDecl>> params;
        std::vector<ptr<ResolvedType>> paramsTypes;
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), std::move(genericType));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@testNum") {
        std::vector<ptr<ResolvedParamDecl>> params;
        std::vector<ptr<ResolvedType>> paramsTypes;
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes),
                                                    ResolvedTypeNumber::usize(SourceLocation::builtin()));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@testRun") {
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(
            loc, "test", ResolvedTypeExpr::fromType(ResolvedTypeNumber::usize(SourceLocation::builtin())), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(
            loc, nullptr, std::move(paramsTypes),
            makePtr<ResolvedTypeOptional>(SourceLocation::builtin(),
                                          makePtr<ResolvedTypeVoid>(SourceLocation::builtin())));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@testName") {
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(
            loc, "test", ResolvedTypeExpr::fromType(ResolvedTypeNumber::usize(SourceLocation::builtin())), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(
            loc, nullptr, std::move(paramsTypes),
            makePtr<ResolvedTypeSlice>(SourceLocation::builtin(), ResolvedTypeNumber::u8(SourceLocation::builtin())));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@simdLoad") {
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "ptr", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes),
                                                    makePtr<ResolvedTypeSimd>(loc, makePtr<ResolvedTypeVoid>(loc), 0));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@simdStore") {
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "ptr", genericTypeExpr(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "val", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        auto fnType =
            makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), makePtr<ResolvedTypeVoid>(loc));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@simdSelect") {
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "a", genericTypeExpr(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "b", genericTypeExpr(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "mask", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        paramsTypes.emplace_back(params[2]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes),
                                                    makePtr<ResolvedTypeSimd>(loc, makePtr<ResolvedTypeVoid>(loc), 0));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@simdReduce") {
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "val", genericTypeExpr(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(
            loc, "op", makePtr<ResolvedTypeExpr>(loc, makePtr<ResolvedTypeEnum>(loc, nullptr)), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes),
                                                    makePtr<ResolvedTypeAnyType>(loc));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@compileError" || fnName == "@compileLog") {
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);  // Placeholder for return type
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(
            loc, "message", makePtr<ResolvedTypeExpr>(loc, makePtr<ResolvedTypeVarArg>(loc)), false, true));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto fnType =
            makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), makePtr<ResolvedTypeVoid>(loc));
        if (fnName == "@compileError") {
            static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
            it->second = &funcDecl;
            debug_msg("created @compileError");
            return &funcDecl;
        } else if (fnName == "@compileLog") {
            static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
            it->second = &funcDecl;
            debug_msg("created @compileLog");
            return &funcDecl;
        }
    } else if (fnName == "@asm") {
        auto stringPtr = makePtr<ResolvedTypePointer>(loc, ResolvedTypeNumber::u8(loc));
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(
            makePtr<ResolvedParamDecl>(loc, "asmCode", ResolvedTypeExpr::fromType(stringPtr->clone()), false));
        params.emplace_back(
            makePtr<ResolvedParamDecl>(loc, "constraints", ResolvedTypeExpr::fromType(stringPtr->clone()), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(
            loc, "args", makePtr<ResolvedTypeExpr>(loc, makePtr<ResolvedTypeVarArg>(loc)), false, true));

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        paramsTypes.emplace_back(params[2]->type->clone());

        auto fnType =
            makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), makePtr<ResolvedTypeVoid>(loc));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@ptrCast") {
        // @ptrCast(ptr) -> *TargetType  (target inferred from context)
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(
            loc, "ptr", ResolvedTypeExpr::fromType(ResolvedTypePointer::opaquePtr(loc)), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), genericType->clone());
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        debug_msg("created @ptrCast");
        return &funcDecl;
    } else if (fnName == "@intCast" || fnName == "@floatCast") {
        // @intCast(val) -> TargetType  (target inferred from context)
        // @floatCast(val) -> TargetType (target inferred from context)
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(
            loc, "val", ResolvedTypeExpr::fromType(makePtr<ResolvedTypeNumber>(loc, ResolvedNumberKind::Int, 32)),
            false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), genericType->clone());
        if (fnName == "@intCast") {
            static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
            it->second = &funcDecl;
            debug_msg("created @intCast");
            return &funcDecl;
        } else if (fnName == "@floatCast") {
            static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
            it->second = &funcDecl;
            debug_msg("created @floatCast");
            return &funcDecl;
        }
    } else if (fnName == "@sqrt") {
        // @sqrt(value) -> value  (target inferred from context)
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "value", genericTypeExpr(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto genericType = makePtr<ResolvedTypeAnyType>(loc);
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), genericType->clone());
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        debug_msg("created @sqrt");
        return &funcDecl;
    }
    debug_msg("return null");
    return nullptr;
}

ptr<ResolvedTypeFunction> Sema::resolve_builtin_function_expr(ResolvedExpr &call,
                                                              ResolvedBuiltinFunctionDecl &resolvedCallee,
                                                              std::vector<ptr<ResolvedExpr>> &resolvedArguments) {
    // Return real builtin function type to check params afterwards
    if (resolvedCallee.identifier == "@call") {
        auto &fnParam = resolvedArguments[0];
        auto &argsParam = resolvedArguments[1];

        // fn is a pointer to a function
        ptr<ResolvedTypeFunction> fnType = nullptr;
        if (auto *fnPtrType = dynamic_cast<const ResolvedTypePointer *>(fnParam->type.get())) {
            if (auto *paramFnType = dynamic_cast<const ResolvedTypeFunction *>(fnPtrType->pointerType.get())) {
                fnType = castPtr<ResolvedTypeFunction>(paramFnType->clone());
            } else {
                return report(fnParam->location, "@call: fn is not a pointer to a function");
            }
        } else if (fnParam->type->kind != ResolvedTypeKind::AnyType) {
            return report(fnParam->location, "@call: fn is not a pointer to a function");
        }
        // args is a touple with exactly the same number of params than the function
        ptr<ResolvedTypeStruct> argsTupleType = nullptr;
        if (auto *tupleType = dynamic_cast<const ResolvedTypeStruct *>(argsParam->type.get())) {
            if (tupleType->decl && tupleType->decl->isTuple) {
                if (fnType) {
                    if (tupleType->decl->fields.size() == fnType->paramsTypes.size()) {
                        if (auto *struInit = dynamic_cast<ResolvedStructInstantiationExpr *>(argsParam.get())) {
                            for (size_t i = 0; i < struInit->fieldInitializers.size(); i++) {
                                if (!perform_implicit_cast(struInit->fieldInitializers[i]->initializer,
                                                           *fnType->paramsTypes[i]))
                                    return nullptr;
                            }
                        }
                        argsTupleType = castPtr<ResolvedTypeStruct>(tupleType->clone());
                    } else {
                        return report(argsParam->location, "@call: tuple size does not match function params size");
                    }
                } else {
                    argsTupleType = castPtr<ResolvedTypeStruct>(tupleType->clone());
                }
            } else {
                return report(argsParam->location, "@call: args is not a tuple");
            }
        } else if (argsParam->type->kind != ResolvedTypeKind::AnyType) {
            return report(argsParam->location, "@call: args is not a tuple");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(fnParam->type->clone());
        paramsTypes.emplace_back(argsParam->type->clone());
        auto retReturnType =
            fnType ? fnType->returnType->clone() : makePtr<ResolvedTypeAnyType>(call.location);
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                                 std::move(retReturnType));
        call.type = ret->clone();
        return ret;
    } else if (resolvedCallee.identifier == "@atomicLoad") {
        auto &atomicPtrParam = resolvedArguments[0];
        auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(atomicPtrParam->type.get());
        if (!ptrType) {
            return report(atomicPtrParam->location,
                          "expected pointer type for @atomicLoad, actual '" + atomicPtrParam->type->to_str() + "'");
        }
        if (ptrType->pointerType->kind != ResolvedTypeKind::Number &&
            ptrType->pointerType->kind != ResolvedTypeKind::Pointer) {
            return report(ptrType->location,
                          "atomic load operation only supported for numeric or pointer types, actual '" +
                              ptrType->pointerType->to_str() + "'");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(atomicPtrParam->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                                 ptrType->pointerType->clone());
        call.type = ret->returnType->clone();
        return ret;
    } else if (resolvedCallee.identifier == "@atomicStore") {
        auto &atomicPtrParam = resolvedArguments[0];
        auto &valueParam = resolvedArguments[1];
        auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(atomicPtrParam->type.get());
        if (!ptrType) {
            return report(atomicPtrParam->location,
                          "expected pointer type for @atomicStore, actual '" + atomicPtrParam->type->to_str() + "'");
        }
        if (ptrType->pointerType->kind != ResolvedTypeKind::Number &&
            ptrType->pointerType->kind != ResolvedTypeKind::Pointer) {
            return report(ptrType->location,
                          "atomic store operation only supported for numeric or pointer types, actual '" +
                              ptrType->pointerType->to_str() + "'");
        }
        if (!perform_implicit_cast(valueParam, *ptrType->pointerType)) return nullptr;
        if (!valueParam->type->compare(*ptrType->pointerType)) {
            return report(valueParam->location, "cannot store '" + valueParam->type->to_str() + "' into '" +
                                                    ptrType->pointerType->to_str() + "'");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(atomicPtrParam->type->clone());
        paramsTypes.emplace_back(valueParam->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                                 makePtr<ResolvedTypeVoid>(call.location));
        call.type = ret->returnType->clone();
        return ret;
    } else if (resolvedCallee.identifier == "@atomicCmpExS" || resolvedCallee.identifier == "@atomicCmpExW") {
        auto &atomicPtrParam = resolvedArguments[0];
        auto &expectedParam = resolvedArguments[1];
        auto &desiredParam = resolvedArguments[2];
        auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(atomicPtrParam->type.get());
        if (!ptrType) {
            return report(atomicPtrParam->location, "expected pointer type for " + resolvedCallee.identifier +
                                                        ", actual '" + atomicPtrParam->type->to_str() + "'");
        }
        if (ptrType->pointerType->kind != ResolvedTypeKind::Number &&
            ptrType->pointerType->kind != ResolvedTypeKind::Pointer) {
            return report(ptrType->location,
                          "atomic compare-exchange operation only supported for numeric or pointer types, actual '" +
                              ptrType->pointerType->to_str() + "'");
        }
        if (!perform_implicit_cast(expectedParam, *ptrType->pointerType)) return nullptr;
        if (!perform_implicit_cast(desiredParam, *ptrType->pointerType)) return nullptr;

        if (!expectedParam->type->compare(*ptrType->pointerType)) {
            return report(expectedParam->location, "expected '" + ptrType->pointerType->to_str() +
                                                       "' for second parameter of " + resolvedCallee.identifier +
                                                       ", actual '" + expectedParam->type->to_str() + "'");
        }
        if (!desiredParam->type->compare(*ptrType->pointerType)) {
            return report(desiredParam->location, "expected '" + ptrType->pointerType->to_str() +
                                                      "' for third parameter of " + resolvedCallee.identifier +
                                                      ", actual '" + desiredParam->type->to_str() + "'");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(atomicPtrParam->type->clone());
        paramsTypes.emplace_back(expectedParam->type->clone());
        paramsTypes.emplace_back(desiredParam->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                                 makePtr<ResolvedTypeBool>(call.location));
        call.type = ret->returnType->clone();
        return ret;
    } else if (resolvedCallee.identifier == "@atomicRmw") {
        auto &atomicPtrParam = resolvedArguments[0];
        auto &opParam = resolvedArguments[1];
        auto &valueParam = resolvedArguments[2];

        auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(atomicPtrParam->type.get());
        if (!ptrType) {
            return report(atomicPtrParam->location,
                          "expected pointer type for @atomicRmw, actual '" + atomicPtrParam->type->to_str() + "'");
        }

        ImportExpr atomicImportExpr(SourceLocation::builtin(), "atomic");
        auto atomic_import_expr = resolve_import_expr(atomicImportExpr);
        if (!atomic_import_expr) return {};

        auto atomicOpDecl = lookup_in_module(call.location, atomic_import_expr->moduleDecl, "AtomicOp");
        if (atomicOpDecl) {
            if (auto enumDecl = dynamic_cast<ResolvedTypeEnumDecl *>(atomicOpDecl->type.get())) {
                if (!perform_implicit_cast(opParam, *makePtr<ResolvedTypeEnum>(call.location, enumDecl->enumDecl()))) {
                    return nullptr;
                }
            }
        } else {
            return report(opParam->location, "AtomicOp not found in std/atomic.dmz");
        }

        if (ptrType->pointerType->kind != ResolvedTypeKind::Number &&
            ptrType->pointerType->kind != ResolvedTypeKind::Pointer) {
            std::string opStr = "unknown";
            if (auto autoMember = dynamic_cast<ResolvedAutoMemberExpr *>(opParam.get())) {
                opStr = autoMember->field;
            }
            return report(call.location, "atomic RMW operation '" + opStr +
                                             "' only supported for numeric or pointer types, actual '" +
                                             ptrType->pointerType->to_str() + "'");
        }

        if (!perform_implicit_cast(valueParam, *ptrType->pointerType)) return nullptr;
        if (!valueParam->type->compare(*ptrType->pointerType)) {
            return report(valueParam->location, "cannot RMW '" + valueParam->type->to_str() + "' into '" +
                                                    ptrType->pointerType->to_str() + "'");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(atomicPtrParam->type->clone());
        paramsTypes.emplace_back(opParam->type->clone());
        paramsTypes.emplace_back(valueParam->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                                 ptrType->pointerType->clone());
        call.type = ret->returnType->clone();
        return ret;
    } else if (resolvedCallee.identifier == "@sizeof") {
        auto &typeExpr = resolvedArguments[0];
        auto retType = makePtr<ResolvedTypeNumber>(call.location, ResolvedNumberKind::UInt, 64, true);
        call.type = retType->clone();

        std::vector<ptr<ResolvedType>> params;
        params.emplace_back(typeExpr->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(params), call.type->clone());
        if (!ensure_fully_resolved(*typeExpr->type)) return nullptr;
        return ret;
    } else if (resolvedCallee.identifier == "@typeof") {
        if (resolvedArguments.empty()) return report(call.location, "expected argument for @typeof");
        auto &arg = resolvedArguments[0];
        ptr<ResolvedType> resultType = nullptr;
        if (arg->type->kind == ResolvedTypeKind::Type) {
            if (auto comptimeVal = arg->get_constant_value()) {
                if (comptimeVal->isType()) {
                    resultType = comptimeVal->getType();
                }
            }
            if (!resultType) {
                if (auto typeExpr = dynamic_cast<ResolvedTypeExpr *>(arg.get())) {
                    resultType = typeExpr->resolvedType->clone();
                }
            }
        } else {
            resultType = arg->type->clone();
        }

        if (!resultType) return report(arg->location, "could not determine type");

        call.type = makePtr<ResolvedTypeType>(call.location);
        call.set_constant_value(ComptimeValue(resultType->clone()));

        std::vector<ptr<ResolvedType>> params;
        params.emplace_back(arg->type->clone());
        return makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(params), call.type->clone());
    } else if (resolvedCallee.identifier == "@typeid") {
        auto &expr = resolvedArguments[0];
        auto retType = makePtr<ResolvedTypeNumber>(call.location, ResolvedNumberKind::Int, 32);
        call.type = retType->clone();

        std::vector<ptr<ResolvedType>> params;
        params.emplace_back(expr->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(params), call.type->clone());
        if (!ensure_fully_resolved(*expr->type)) return nullptr;
        return ret;
    } else if (resolvedCallee.identifier == "@typeinfo") {
        auto &expr = resolvedArguments[0];

        ImportExpr typesImportExpr(SourceLocation::builtin(), "types");
        auto types_import_expr = resolve_import_expr(typesImportExpr);
        if (!types_import_expr) return nullptr;

        std::string targetUnionName = "TypeInfo";
        ResolvedTypeUnionDecl *typeInfoDecl = nullptr;
        for (auto &decl : types_import_expr->moduleDecl.declarations) {
            if (decl->identifier == targetUnionName) {
                if (!ensure_fully_resolved(*decl)) return nullptr;
                if (auto unionDeclRef = dynamic_cast<ResolvedTypeUnionDecl *>(decl->type.get())) {
                    typeInfoDecl = unionDeclRef;
                    break;
                }
            }
        }
        if (!typeInfoDecl) {
            return report(call.location, "could not find " + targetUnionName + " in types.dmz");
        }
        auto returnType = makePtr<ResolvedTypeUnion>(call.location, typeInfoDecl->unionDecl());
        call.type = returnType->clone();

        std::vector<ptr<ResolvedType>> params;
        params.emplace_back(expr->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(params), call.type->clone());
        if (!ensure_fully_resolved(*expr->type)) return nullptr;
        return ret;
    } else if (resolvedCallee.identifier == "@hasMethod") {
        auto &structTypeExpr = resolvedArguments[0];
        auto &methodNameExpr = resolvedArguments[1];
        auto methodNameLit = dynamic_cast<ResolvedStringLiteral *>(methodNameExpr.get());
        if (!methodNameLit)
            return report(methodNameExpr->location, "expected string literal for method name in @hasMethod");
        std::string methodName = methodNameLit->value;

        ResolvedType *baseType = structTypeExpr->type.get();
        if (auto ptrType = dynamic_cast<ResolvedTypePointer *>(baseType)) {
            baseType = ptrType->pointerType.get();
        }

        bool hasMethod = false;
        ResolvedStructDecl *structToSearch = nullptr;
        if (auto struDeclType = dynamic_cast<ResolvedTypeStructDecl *>(baseType)) {
            structToSearch = struDeclType->decl;
        } else if (auto struType = dynamic_cast<ResolvedTypeStruct *>(baseType)) {
            structToSearch = struType->decl;
        }

        if (structToSearch) {
            if (!ensure_struct_funcs_resolved(*structToSearch)) return nullptr;
            for (auto &&func : structToSearch->functions_strs) {
                if (func == methodName) {
                    hasMethod = true;
                    break;
                }
            }
        }

        call.set_constant_value(ComptimeValue((int64_t)(hasMethod ? 1 : 0)));
        auto retType = makePtr<ResolvedTypeBool>(call.location);
        call.type = retType->clone();

        std::vector<ptr<ResolvedType>> params;
        params.emplace_back(structTypeExpr->type->clone());
        params.emplace_back(methodNameExpr->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(params), call.type->clone());
        if (!ensure_fully_resolved(*structTypeExpr->type)) return nullptr;
        return ret;
    } else if (resolvedCallee.identifier == "@simd") {
        return castPtr<ResolvedTypeFunction>(resolvedCallee.getFnType()->clone());
    } else if (resolvedCallee.identifier == "@simdSize") {
        auto &typeExpr = resolvedArguments[0];
        auto retType = ResolvedTypeNumber::usize(call.location);
        call.type = retType->clone();

        std::vector<ptr<ResolvedType>> params;
        params.emplace_back(typeExpr->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(params), call.type->clone());
        if (!ensure_fully_resolved(*typeExpr->type)) return nullptr;
        return ret;
    } else if (resolvedCallee.identifier == "@simdSplat") {
        auto &value = resolvedArguments[0];
        auto retType = makePtr<ResolvedTypeSimd>(call.location, value->type->clone(), 0);
        call.type = retType->clone();

        std::vector<ptr<ResolvedType>> params;
        params.emplace_back(value->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(params), call.type->clone());
        return ret;
    } else if (resolvedCallee.identifier == "@simdIota") {
        auto retType = makePtr<ResolvedTypeSimd>(call.location, ResolvedTypeNumber::usize(call.location), 0);
        call.type = retType->clone();

        std::vector<ptr<ResolvedType>> params;
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(params), call.type->clone());
        return ret;
    } else if (resolvedCallee.identifier == "@errorTrace") {
        ImportExpr builtinImportExpr(SourceLocation::builtin(), "builtin");
        auto builtin_import_expr = resolve_import_expr(builtinImportExpr);
        if (!builtin_import_expr) return {};

        std::string errorTraceType = "ErrorTrace";
        ResolvedTypeStructDecl *typeInfoDecl = nullptr;
        for (auto &decl : builtin_import_expr->moduleDecl.declarations) {
            if (decl->identifier == errorTraceType) {
                if (!ensure_fully_resolved(*decl)) return nullptr;
                if (auto unionDeclRef = dynamic_cast<ResolvedTypeStructDecl *>(decl->type.get())) {
                    typeInfoDecl = unionDeclRef;
                    break;
                }
            }
        }
        if (!typeInfoDecl) {
            return report(call.location, "could not find " + errorTraceType + " in types.dmz");
        }
        call.type = makePtr<ResolvedTypeStruct>(call.location, typeInfoDecl->decl);

        std::vector<ptr<ResolvedType>> params;
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(params), call.type->clone());
        return ret;
    } else if (resolvedCallee.identifier == "@testNum") {
        call.set_constant_value(ComptimeValue((int64_t)m_tests.size()));
        auto ret = dynamic_cast<ResolvedTypeFunction *>(resolvedCallee.type.get());
        if (!ret) dmz_unreachable(call.location, "not function type " + resolvedCallee.type->to_str());
        return castPtr<ResolvedTypeFunction>(ret->clone());
    } else if (resolvedCallee.identifier == "@testRun") {
        auto ret = dynamic_cast<ResolvedTypeFunction *>(resolvedCallee.type.get());
        if (!ret) dmz_unreachable(call.location, "not function type " + resolvedCallee.type->to_str());
        // Check if the first parameter is const value
        if (!resolvedArguments.empty() && !resolvedArguments[0]->get_constant_value().has_value()) {
            return report(call.location, "expected const value for test name");
        }
        return castPtr<ResolvedTypeFunction>(ret->clone());
    } else if (resolvedCallee.identifier == "@testName") {
        auto ret = dynamic_cast<ResolvedTypeFunction *>(resolvedCallee.type.get());
        if (!ret) dmz_unreachable(call.location, "not function type " + resolvedCallee.type->to_str());
        // Check if the first parameter is const value
        if (!resolvedArguments.empty() && !resolvedArguments[0]->get_constant_value().has_value()) {
            return report(call.location, "expected const value for test name");
        }
        return castPtr<ResolvedTypeFunction>(ret->clone());
    } else if (resolvedCallee.identifier == "@simdLoad") {
        if (resolvedArguments.empty()) return report(call.location, "expected argument for @simdLoad");
        auto &ptrParam = resolvedArguments[0];
        auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(ptrParam->type.get());
        if (!ptrType) {
            return report(ptrParam->location, "expected pointer type for @simdLoad");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(ptrParam->type->clone());
        return makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                             call.type->clone());
    } else if (resolvedCallee.identifier == "@simdStore") {
        if (resolvedArguments.size() != 2) return report(call.location, "expected 2 arguments for @simdStore");
        auto &valParam = resolvedArguments[0];
        auto &ptrParam = resolvedArguments[1];
        auto *simdType = dynamic_cast<const ResolvedTypeSimd *>(valParam->type.get());
        if (!simdType) return report(valParam->location, "expected SIMD type for @simdStore");

        auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(ptrParam->type.get());
        if (!ptrType) return report(ptrParam->location, "expected pointer type for @simdStore");

        if (!perform_implicit_cast(ptrParam, *makePtr<ResolvedTypePointer>(call.location, simdType->simdType->clone())))
            return nullptr;

        call.type = makePtr<ResolvedTypeVoid>(call.location);
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(valParam->type->clone());
        paramsTypes.emplace_back(ptrParam->type->clone());
        return makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                             call.type->clone());
    } else if (resolvedCallee.identifier == "@simdSelect") {
        if (resolvedArguments.size() != 3) return report(call.location, "expected 3 arguments for @simdSelect");
        auto &aParam = resolvedArguments[0];
        auto &bParam = resolvedArguments[1];
        auto &maskParam = resolvedArguments[2];

        auto *simdType = dynamic_cast<const ResolvedTypeSimd *>(aParam->type.get());
        if (!simdType) return report(aParam->location, "expected SIMD type for @simdSelect");

        if (!perform_implicit_cast(bParam, *simdType)) return nullptr;

        auto maskType =
            makePtr<ResolvedTypeSimd>(call.location, makePtr<ResolvedTypeBool>(call.location), simdType->simdSize);
        if (!perform_implicit_cast(maskParam, *maskType)) return nullptr;

        call.type = simdType->clone();
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(aParam->type->clone());
        paramsTypes.emplace_back(bParam->type->clone());
        paramsTypes.emplace_back(maskParam->type->clone());
        return makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                             call.type->clone());
    } else if (resolvedCallee.identifier == "@simdReduce") {
        if (resolvedArguments.size() != 2) return report(call.location, "expected 2 arguments for @simdReduce");
        auto &valParam = resolvedArguments[0];
        auto &opParam = resolvedArguments[1];

        auto *simdType = dynamic_cast<const ResolvedTypeSimd *>(valParam->type.get());
        if (!simdType) return report(valParam->location, "expected SIMD type for @simdReduce");

        ImportExpr simdImportExpr(SourceLocation::builtin(), "simd");
        auto simd_import_expr = resolve_import_expr(simdImportExpr);
        if (!simd_import_expr) return {};

        auto simdReduceKindDecl = lookup_in_module(call.location, simd_import_expr->moduleDecl, "SimdReduceKind");
        if (simdReduceKindDecl) {
            if (auto enumDecl = dynamic_cast<ResolvedTypeEnumDecl *>(simdReduceKindDecl->type.get())) {
                if (!perform_implicit_cast(opParam, *makePtr<ResolvedTypeEnum>(call.location, enumDecl->enumDecl()))) {
                    return nullptr;
                }
            }
        } else {
            return report(opParam->location, "SimdReduceKind not found in std/simd.dmz");
        }

        auto elementType = simdType->simdType.get();
        auto numType = dynamic_cast<const ResolvedTypeNumber *>(elementType);
        if (!numType && elementType->kind != ResolvedTypeKind::AnyType) {
            return report(call.location, "reduction operations only supported for numeric vector elements");
        }

        std::string opStr = "unknown";
        if (auto autoMember = dynamic_cast<ResolvedAutoMemberExpr *>(opParam.get())) {
            opStr = autoMember->field;
        }

        if (opStr == "Sub") {
            return report(call.location, "Sub reduction is not supported; no vector subtract-reduce in hardware");
        }
        if (opStr == "And" || opStr == "Or" || opStr == "Xor") {
            if (numType && numType->numberKind == ResolvedNumberKind::Float) {
                return report(call.location, "bitwise reduction only supported for integer vectors");
            }
        }

        call.type = simdType->simdType->clone();
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(valParam->type->clone());
        paramsTypes.emplace_back(opParam->type->clone());
        return makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                             call.type->clone());
    } else if (resolvedCallee.identifier == "@compileError") {
        // @compileError(...) all args need to be comptime values
        if (resolvedArguments.size() == 0) {
            return report(call.location, "@compileError expects at least one argument.");
        }
        std::stringstream errorMsg;
        for (auto &arg : resolvedArguments) {
            if (auto comptimeVal = arg->get_constant_value()) {
                errorMsg << comptimeVal.value() << " ";
            } else {
                if (!(m_currentFunction && dynamic_cast<ResolvedGenericFunctionDecl *>(m_currentFunction))) {
                    return report(arg->location, "@compileError expects all arguments to be comptime values");
                }
            }
        }
        if (!(m_currentFunction && dynamic_cast<ResolvedGenericFunctionDecl *>(m_currentFunction))) {
            return report(call.location, errorMsg.str(), ReportLevel::Error);
        }
        if (resolvedCallee.type->kind != ResolvedTypeKind::Function) {
            dmz_unreachable(call.location, "@compileError expects a function type");
        }
        return castPtr<ResolvedTypeFunction>(resolvedCallee.type->clone());
    } else if (resolvedCallee.identifier == "@compileLog") {
        // @compileLog(...) all args need to be comptime values
        if (resolvedArguments.size() == 0) {
            return report(call.location, "@compileLog expects at least one argument.");
        }
        std::stringstream errorMsg;
        for (auto &arg : resolvedArguments) {
            if (auto comptimeVal = arg->get_constant_value()) {
                errorMsg << comptimeVal.value() << " ";
            } else {
                if (!(m_currentFunction && dynamic_cast<ResolvedGenericFunctionDecl *>(m_currentFunction))) {
                    return report(arg->location, "@compileLog expects all arguments to be comptime values");
                }
            }
        }
        if (!(m_currentFunction && dynamic_cast<ResolvedGenericFunctionDecl *>(m_currentFunction))) {
            report(call.location, errorMsg.str(), ReportLevel::Info);
        }
        if (resolvedCallee.type->kind != ResolvedTypeKind::Function) {
            dmz_unreachable(call.location, "@compileLog expects a function type");
        }
        return castPtr<ResolvedTypeFunction>(resolvedCallee.type->clone());
    } else if (resolvedCallee.identifier == "@asm") {
        if (resolvedArguments.size() < 2)
            return report(call.location, "@asm expects at least 2 arguments: (AsmCode, Constraints, ...Args)");

        auto &asmCodeExpr = resolvedArguments[0];
        if (!dynamic_cast<ResolvedStringLiteral *>(asmCodeExpr.get())) {
            return report(asmCodeExpr->location, "@asm first argument must be a string literal");
        }

        auto &constraintsExpr = resolvedArguments[1];
        if (!dynamic_cast<ResolvedStringLiteral *>(constraintsExpr.get())) {
            return report(constraintsExpr->location, "@asm second argument must be a string literal");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        for (auto &arg : resolvedArguments) {
            paramsTypes.emplace_back(arg->type->clone());
        }
        auto fnType = dynamic_cast<const ResolvedTypeFunction *>(call.type.get());
        if (!fnType) {
            dmz_unreachable(call.location, "@asm: call.type is not a function type");
        }
        return makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                             fnType->returnType->clone());
    } else if (resolvedCallee.identifier == "@ptrCast") {
        if (resolvedArguments.size() != 1) {
            return report(call.location, "@ptrCast expects exactly 1 argument: (ptr)");
        }
        auto &ptrArg = resolvedArguments[0];

        // Arg must be a pointer
        auto srcPtrType = dynamic_cast<const ResolvedTypePointer *>(ptrArg->type.get());
        if (!srcPtrType) {
            return report(ptrArg->location,
                          "@ptrCast: argument must be a pointer, got '" + ptrArg->type->to_str() + "'");
        }

        // Return type is opaque pointer
        auto returnType = ResolvedTypePointer::opaquePtr(call.location);
        call.type = returnType->clone();

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(ptrArg->type->clone());
        return makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                             std::move(returnType));
    } else if (resolvedCallee.identifier == "@intCast") {
        if (resolvedArguments.size() != 1) {
            return report(call.location, "@intCast expects exactly 1 argument: (val)");
        }
        auto &valArg = resolvedArguments[0];

        // Arg must be a number, bool, or enum
        if (valArg->type->kind != ResolvedTypeKind::Number && valArg->type->kind != ResolvedTypeKind::Bool &&
            valArg->type->kind != ResolvedTypeKind::Enum && valArg->type->kind != ResolvedTypeKind::AnyType) {
            return report(valArg->location, "@intCast: argument must be numeric, got '" + valArg->type->to_str() + "'");
        }

        // Return type is generic number for now, will be inferred in perform_implicit_cast
        auto returnType = makePtr<ResolvedTypeAnyType>(call.location);
        call.type = returnType->clone();

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(valArg->type->clone());
        return makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                             std::move(returnType));
    } else if (resolvedCallee.identifier == "@floatCast") {
        if (resolvedArguments.size() != 1) {
            return report(call.location, "@floatCast expects exactly 1 argument: (val)");
        }
        auto &valArg = resolvedArguments[0];

        // Arg must be a number
        if (valArg->type->kind != ResolvedTypeKind::Number && valArg->type->kind != ResolvedTypeKind::AnyType) {
            return report(valArg->location,
                          "@floatCast: argument must be numeric, got '" + valArg->type->to_str() + "'");
        }

        // Return type is generic for now, will be inferred in perform_implicit_cast
        auto returnType = makePtr<ResolvedTypeAnyType>(call.location);
        call.type = returnType->clone();

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(valArg->type->clone());
        return makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                             std::move(returnType));
    } else if (resolvedCallee.identifier == "@sqrt") {
        if (resolvedArguments.size() != 1) {
            return report(call.location, "@sqrt expects exactly 1 argument: (value)");
        }
        auto &valArg = resolvedArguments[0];

        // Arg must be a float
        bool isCorrectType = true;
        ptr<ResolvedType> returnType = nullptr;
        if (auto numType = dynamic_cast<ResolvedTypeNumber *>(valArg->type.get())) {
            if (numType->numberKind != ResolvedNumberKind::Float) {
                isCorrectType = false;
            }
            returnType = numType->clone();
        } else if (auto simdType = dynamic_cast<ResolvedTypeSimd *>(valArg->type.get())) {
            if (auto numType = dynamic_cast<ResolvedTypeNumber *>(simdType->simdType.get())) {
                if (numType->numberKind != ResolvedNumberKind::Float) {
                    isCorrectType = false;
                }
                returnType = simdType->clone();
            } else {
                isCorrectType = false;
            }
        }
        if (!isCorrectType) {
            return report(valArg->location,
                          "@sqrt: argument must be floating or simd, got '" + valArg->type->to_str() + "'");
        }

        call.type = returnType->clone();

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(valArg->type->clone());
        return makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                             std::move(returnType));
    }
    return report(call.location, "unknown builtin function");
}
}  // namespace DMZ
