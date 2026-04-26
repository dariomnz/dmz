#ifdef DEBUG_SEMANTIC
#ifndef DEBUG
#define DEBUG
#endif
#endif
#include "Debug.hpp"
#include "Utils.hpp"
#include "codegen/CodegenUtils.hpp"
#include "parser/ParserSymbols.hpp"
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
        auto genericType = makePtr<ResolvedTypeGeneric>(loc, nullptr);
        return makePtr<ResolvedTypeExpr>(loc, genericType->clone());
    };
    if (fnName == "@call") {
        auto genericType = makePtr<ResolvedTypeGeneric>(loc, nullptr);
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
        auto genericType = makePtr<ResolvedTypeGeneric>(loc, nullptr);
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
        auto genericType = makePtr<ResolvedTypeGeneric>(loc, nullptr);
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
        auto genericType = makePtr<ResolvedTypeGeneric>(loc, nullptr);
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
        auto genericType = makePtr<ResolvedTypeGeneric>(loc, nullptr);
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
        auto fnType =
            makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes),
                                          makePtr<ResolvedTypeSimd>(loc, makePtr<ResolvedTypeVoid>(loc), nullptr, 0));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@simdIota") {
        std::vector<ptr<ResolvedParamDecl>> params;
        std::vector<ptr<ResolvedType>> paramsTypes;
        auto fnType =
            makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes),
                                          makePtr<ResolvedTypeSimd>(loc, makePtr<ResolvedTypeVoid>(loc), nullptr, 0));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        return &funcDecl;
    } else if (fnName == "@errorTrace") {
        auto genericType = makePtr<ResolvedTypeGeneric>(loc, nullptr);
        std::vector<ptr<ResolvedParamDecl>> params;
        std::vector<ptr<ResolvedType>> paramsTypes;
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), std::move(genericType));
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
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
        } else if (fnParam->type->kind != ResolvedTypeKind::Generic) {
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
        } else if (argsParam->type->kind != ResolvedTypeKind::Generic) {
            return report(argsParam->location, "@call: args is not a tuple");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(fnParam->type->clone());
        paramsTypes.emplace_back(argsParam->type->clone());
        auto retReturnType =
            fnType ? fnType->returnType->clone() : makePtr<ResolvedTypeGeneric>(call.location, nullptr);
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
        auto returnType = makePtr<ResolvedTypePointer>(
            call.location, makePtr<ResolvedTypeUnion>(call.location, typeInfoDecl->unionDecl()));
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

        call.set_constant_value(hasMethod ? 1 : 0);
        auto retType = makePtr<ResolvedTypeBool>(call.location);
        call.type = retType->clone();

        std::vector<ptr<ResolvedType>> params;
        params.emplace_back(structTypeExpr->type->clone());
        params.emplace_back(methodNameExpr->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(params), call.type->clone());
        if (!ensure_fully_resolved(*structTypeExpr->type)) return nullptr;
        return ret;
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
        auto retType = makePtr<ResolvedTypeSimd>(call.location, value->type->clone(), nullptr, 0);
        call.type = retType->clone();

        std::vector<ptr<ResolvedType>> params;
        params.emplace_back(value->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(params), call.type->clone());
        return ret;
    } else if (resolvedCallee.identifier == "@simdIota") {
        auto retType = makePtr<ResolvedTypeSimd>(call.location, ResolvedTypeNumber::usize(call.location), nullptr, 0);
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
    }
    return report(call.location, "unknown builtin function");
}
}  // namespace DMZ
