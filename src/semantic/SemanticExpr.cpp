// #define DEBUG
#include "semantic/Semantic.hpp"

namespace DMZ {

bool op_generate_bool(TokenType op) {
    static const std::unordered_set<TokenType> op_bool = {
        TokenType::op_excla_mark, TokenType::op_less,    TokenType::op_less_eq,
        TokenType::op_more,       TokenType::op_more_eq, TokenType::op_equal,
        TokenType::op_not_equal,  TokenType::pipepipe,   TokenType::ampamp,
    };
    return op_bool.count(op) != 0;
}

std::unique_ptr<ResolvedDeclRefExpr> Sema::resolve_decl_ref_expr(const DeclRefExpr &declRefExpr, bool isCallee) {
    debug_func(declRefExpr.location);
    // Search in the module scope
    ResolvedDecl *decl = lookup(declRefExpr.identifier, ResolvedDeclType::ResolvedDecl).first;
    if (!decl) {
        // Search in the current module
        decl = lookup_in_modules(m_currentModuleID, declRefExpr.identifier, ResolvedDeclType::ResolvedDecl);
    }
    if (!decl) {
        // Search in the imports
        decl = lookup_in_modules(m_currentModuleIDRef, declRefExpr.identifier, ResolvedDeclType::ResolvedDecl);
    }
    if (!decl) {
        dump_scopes();
        return report(declRefExpr.location, "symbol '" + std::string(declRefExpr.identifier) + "' not found");
    }

    if (!isCallee && dynamic_cast<ResolvedFuncDecl *>(decl))
        return report(declRefExpr.location, "expected to call function '" + std::string(declRefExpr.identifier) + "'");

    if (dynamic_cast<ResolvedStructDecl *>(decl))
        return report(declRefExpr.location, "expected an instance of '" + std::string(decl->type.name) + '\'');

    // println("ResolvedDeclRefExpr " << declRefExpr.identifier << " " << decl << " " << decl->identifier);
    varOrReturn(type, resolve_type(decl->type));
    // println(type->to_str());
    return std::make_unique<ResolvedDeclRefExpr>(declRefExpr.location, *decl, *type);
}

std::unique_ptr<ResolvedCallExpr> Sema::resolve_call_expr(const CallExpr &call) {
    debug_func(call.location);
    const ResolvedFuncDecl *parentFunc = nullptr;
    const ResolvedFuncDecl *resolvedFuncDecl = nullptr;
    const ResolvedMemberFunctionDecl *resolvedMemberFuncDecl = nullptr;
    bool isMemberCall = false;
    if (const auto *memberExpr = dynamic_cast<const MemberExpr *>(call.callee.get())) {
        resolvedMemberFuncDecl = static_cast<const ResolvedMemberFunctionDecl *>(
            lookup_in_modules(m_currentModuleIDRef, memberExpr->field, ResolvedDeclType::ResolvedMemberFunctionDecl));
        if (!resolvedMemberFuncDecl) return report(memberExpr->location, "cannot fount struct of member function");
        resolvedFuncDecl = resolvedMemberFuncDecl->function.get();
        parentFunc = resolvedMemberFuncDecl;
        isMemberCall = true;
    } else {
        const auto *dre = dynamic_cast<const DeclRefExpr *>(call.callee.get());
        if (!dre) return report(call.location, "expression cannot be called as a function");

        varOrReturn(resolvedCallee, resolve_decl_ref_expr(*dre, true));
        resolvedFuncDecl = dynamic_cast<const ResolvedFuncDecl *>(&resolvedCallee->decl);
        parentFunc = resolvedFuncDecl;
    }

    if (!resolvedFuncDecl) return report(call.location, "calling non-function symbol");

    if (auto resolvedFunctionDecl = dynamic_cast<const ResolvedFunctionDecl *>(resolvedFuncDecl)) {
        if (call.genericTypes) {
            resolvedFuncDecl = specialize_generic_function(
                *parentFunc, *const_cast<ResolvedFunctionDecl *>(resolvedFunctionDecl), *call.genericTypes);
            if (!resolvedFuncDecl) return nullptr;
        } else {
            if (resolvedFunctionDecl->genericTypes) {
                return report(call.location, "try to call a generic function without specialization");
            }
        }
    }

    size_t call_args_num = call.arguments.size();
    size_t func_args_num = resolvedFuncDecl->params.size();
    if (isMemberCall) func_args_num -= 1;
    bool isVararg = (func_args_num != 0) ? resolvedFuncDecl->params.back()->isVararg : false;
    size_t funcDeclArgs = isVararg ? (func_args_num - 1) : func_args_num;
    if (call_args_num != func_args_num) {
        if (!isVararg || (isVararg && call_args_num < funcDeclArgs))
            return report(call.location, "argument count mismatch in function call");
    }

    std::vector<std::unique_ptr<ResolvedExpr>> resolvedArguments;

    if (const auto *memberExpr = dynamic_cast<const MemberExpr *>(call.callee.get())) {
        if (const auto *declRefExpr = dynamic_cast<const DeclRefExpr *>(memberExpr->base.get())) {
            varOrReturn(resolvedDeclRefExpr, resolve_decl_ref_expr(*declRefExpr));
            auto resolvedRefPtrExpr =
                std::make_unique<ResolvedRefPtrExpr>(resolvedDeclRefExpr->location, std::move(resolvedDeclRefExpr));
            resolvedRefPtrExpr->type = resolvedRefPtrExpr->type.remove_pointer();
            resolvedRefPtrExpr->type.isRef = true;
            resolvedArguments.emplace_back(std::move(resolvedRefPtrExpr));
        } else {
            return report(declRefExpr->location, "unexpected expr in call member function");
        }
    }

    size_t idx = 0;
    if (isMemberCall) idx += 1;
    for (auto &&arg : call.arguments) {
        varOrReturn(resolvedArg, resolve_expr(*arg));
        // Only check until vararg
        if (idx < funcDeclArgs) {
            // Modifi ptr to reference
            if (resolvedFuncDecl->params[idx]->type.isRef && resolvedArg->type.isPointer) {
                resolvedArg->type = resolvedArg->type.remove_pointer();
                resolvedArg->type.isRef = true;
            }

            if (!Type::compare(resolvedFuncDecl->params[idx]->type, resolvedArg->type)) {
                return report(resolvedArg->location, "unexpected type of argument '" + resolvedArg->type.to_str() +
                                                         "' expected '" + resolvedFuncDecl->params[idx]->type.to_str() +
                                                         "'");
            }
            if (resolvedFuncDecl->params[idx]->type.isRef) {
                if (!dynamic_cast<const ResolvedRefPtrExpr *>(resolvedArg.get())) {
                    return report(resolvedArg->location, "expected to reference the value with '&'");
                }
            }
        }
        resolvedArg->set_constant_value(cee.evaluate(*resolvedArg, false));

        ++idx;
        resolvedArguments.emplace_back(std::move(resolvedArg));
    }
    // resolvedFuncDecl->dump();
    // println("param ptr " << resolvedFuncDecl->params[0].get());
    return std::make_unique<ResolvedCallExpr>(call.location, *resolvedFuncDecl, std::move(resolvedArguments));
}

std::unique_ptr<ResolvedExpr> Sema::resolve_expr(const Expr &expr) {
    debug_func(expr.location);
    if (const auto *number = dynamic_cast<const IntLiteral *>(&expr)) {
        return std::make_unique<ResolvedIntLiteral>(number->location, std::stod(std::string(number->value)));
    }
    if (const auto *number = dynamic_cast<const FloatLiteral *>(&expr)) {
        return std::make_unique<ResolvedFloatLiteral>(number->location, std::stod(std::string(number->value)));
    }
    if (const auto *character = dynamic_cast<const CharLiteral *>(&expr)) {
        if (auto c = str_from_source(character->value.substr(1, character->value.size() - 1))) {
            return std::make_unique<ResolvedCharLiteral>(character->location, (*c)[0]);
        } else {
            return report(character->location, "malformed char");
        }
    }
    if (const auto *boolean = dynamic_cast<const BoolLiteral *>(&expr)) {
        return std::make_unique<ResolvedBoolLiteral>(boolean->location, boolean->value == "true");
    }
    if (const auto *str = dynamic_cast<const StringLiteral *>(&expr)) {
        if (auto c = str_from_source(str->value.substr(1, str->value.size() - 2))) {
            return std::make_unique<ResolvedStringLiteral>(str->location, *c);
        } else {
            return report(str->location, "malformed string");
        }
    }
    if (const auto *errRef = dynamic_cast<const ErrDeclRefExpr *>(&expr)) {
        return resolve_err_decl_ref_expr(*errRef);
    }
    if (const auto *declRefExpr = dynamic_cast<const DeclRefExpr *>(&expr)) {
        return resolve_decl_ref_expr(*declRefExpr);
    }
    if (const auto *callExpr = dynamic_cast<const CallExpr *>(&expr)) {
        return resolve_call_expr(*callExpr);
    }
    if (const auto *groupingExpr = dynamic_cast<const GroupingExpr *>(&expr)) {
        return resolve_grouping_expr(*groupingExpr);
    }
    if (const auto *binaryOperator = dynamic_cast<const BinaryOperator *>(&expr)) {
        return resolve_binary_operator(*binaryOperator);
    }
    if (const auto *unaryOperator = dynamic_cast<const UnaryOperator *>(&expr)) {
        return resolve_unary_operator(*unaryOperator);
    }
    if (const auto *refPtrExpr = dynamic_cast<const RefPtrExpr *>(&expr)) {
        return resolve_ref_ptr_expr(*refPtrExpr);
    }
    if (const auto *derefPtrExpr = dynamic_cast<const DerefPtrExpr *>(&expr)) {
        return resolve_deref_ptr_expr(*derefPtrExpr);
    }
    if (const auto *structInstantiation = dynamic_cast<const StructInstantiationExpr *>(&expr)) {
        return resolve_struct_instantiation(*structInstantiation);
    }
    if (const auto *arrayInstantiation = dynamic_cast<const ArrayInstantiationExpr *>(&expr)) {
        return resolve_array_instantiation(*arrayInstantiation);
    }
    if (const auto *assignableExpr = dynamic_cast<const AssignableExpr *>(&expr)) {
        return resolve_assignable_expr(*assignableExpr);
    }
    if (const auto *errUnwrapExpr = dynamic_cast<const ErrUnwrapExpr *>(&expr)) {
        return resolve_err_unwrap_expr(*errUnwrapExpr);
    }
    if (const auto *catchErrExpr = dynamic_cast<const CatchErrExpr *>(&expr)) {
        return resolve_catch_err_expr(*catchErrExpr);
    }
    if (const auto *tryErrExpr = dynamic_cast<const TryErrExpr *>(&expr)) {
        return resolve_try_err_expr(*tryErrExpr);
    }
    if (const auto *modDeclRefExpr = dynamic_cast<const ModuleDeclRefExpr *>(&expr)) {
        return resolve_module_decl_ref_expr(*modDeclRefExpr, ModuleID{});
    }
    expr.dump();
    dmz_unreachable("unexpected expression");
}

std::unique_ptr<ResolvedUnaryOperator> Sema::resolve_unary_operator(const UnaryOperator &unary) {
    debug_func(unary.location);
    varOrReturn(resolvedRHS, resolve_expr(*unary.operand));

    if (resolvedRHS->type.kind == Type::Kind::Void || !Type::compare(Type::builtinBool(), resolvedRHS->type))
        return report(resolvedRHS->location, '\'' + std::string(resolvedRHS->type.name) +
                                                 "' cannot be used as an operand to unary operator '" +
                                                 std::string(get_op_str(unary.op)) + "'");
    auto ret = std::make_unique<ResolvedUnaryOperator>(unary.location, unary.op, std::move(resolvedRHS));
    if (op_generate_bool(unary.op)) {
        ret->type = Type::builtinBool();
    }
    if (unary.op == TokenType::amp) {
        ret->type = ret->type.pointer();
    }
    return ret;
}

std::unique_ptr<ResolvedRefPtrExpr> Sema::resolve_ref_ptr_expr(const RefPtrExpr &refPtrExpr) {
    debug_func(refPtrExpr.location);
    varOrReturn(resolvedExpr, resolve_expr(*refPtrExpr.expr));
    return std::make_unique<ResolvedRefPtrExpr>(refPtrExpr.location, std::move(resolvedExpr));
}

std::unique_ptr<ResolvedDerefPtrExpr> Sema::resolve_deref_ptr_expr(const DerefPtrExpr &derefPtrExpr) {
    debug_func(derefPtrExpr.location);
    varOrReturn(resolvedExpr, resolve_expr(*derefPtrExpr.expr));
    return std::make_unique<ResolvedDerefPtrExpr>(derefPtrExpr.location, std::move(resolvedExpr));
}

std::unique_ptr<ResolvedBinaryOperator> Sema::resolve_binary_operator(const BinaryOperator &binop) {
    debug_func(binop.location);
    varOrReturn(resolvedLHS, resolve_expr(*binop.lhs));
    varOrReturn(resolvedRHS, resolve_expr(*binop.rhs));

    if (resolvedLHS->type.kind != Type::Kind::Int && resolvedLHS->type.kind != Type::Kind::UInt &&
        resolvedLHS->type.kind != Type::Kind::Float) {
        return report(resolvedLHS->location, '\'' + std::string(resolvedLHS->type.name) +
                                                 "' cannot be used as LHS operand to binary operator");
    }
    if (resolvedRHS->type.kind != Type::Kind::Int && resolvedRHS->type.kind != Type::Kind::UInt &&
        resolvedRHS->type.kind != Type::Kind::Float) {
        return report(resolvedRHS->location, '\'' + std::string(resolvedRHS->type.name) +
                                                 "' cannot be used as RHS operand to binary operator");
    }
    if (!Type::compare(resolvedLHS->type, resolvedRHS->type)) {
        return report(binop.location, "unexpected type " + resolvedLHS->type.to_str() + " in binop");
    }

    auto ret = std::make_unique<ResolvedBinaryOperator>(binop.location, binop.op, std::move(resolvedLHS),
                                                        std::move(resolvedRHS));
    if (op_generate_bool(binop.op)) {
        ret->type = Type::builtinBool();
    }
    return ret;
}

std::unique_ptr<ResolvedGroupingExpr> Sema::resolve_grouping_expr(const GroupingExpr &grouping) {
    debug_func(grouping.location);
    varOrReturn(resolvedExpr, resolve_expr(*grouping.expr));
    return std::make_unique<ResolvedGroupingExpr>(grouping.location, std::move(resolvedExpr));
}

std::unique_ptr<ResolvedAssignableExpr> Sema::resolve_assignable_expr(const AssignableExpr &assignableExpr) {
    debug_func(assignableExpr.location);
    if (const auto *declRefExpr = dynamic_cast<const DeclRefExpr *>(&assignableExpr))
        return resolve_decl_ref_expr(*declRefExpr);

    if (const auto *memberExpr = dynamic_cast<const MemberExpr *>(&assignableExpr))
        return resolve_member_expr(*memberExpr);

    if (const auto *arrayAtExpr = dynamic_cast<const ArrayAtExpr *>(&assignableExpr))
        return resolve_array_at_expr(*arrayAtExpr);

    if (const auto *derefExpr = dynamic_cast<const DerefPtrExpr *>(&assignableExpr))
        return resolve_deref_ptr_expr(*derefExpr);

    assignableExpr.dump();
    dmz_unreachable("unexpected assignable expression");
}

std::unique_ptr<ResolvedMemberExpr> Sema::resolve_member_expr(const MemberExpr &memberExpr) {
    debug_func(memberExpr.location);
    auto resolvedBase = resolve_expr(*memberExpr.base);
    if (!resolvedBase) return nullptr;

    if (resolvedBase->type.kind != Type::Kind::Struct)
        return report(memberExpr.base->location, "cannot access field of '" + resolvedBase->type.to_str() + '\'');

    const auto *st = static_cast<const ResolvedStructDecl *>(
        lookup(resolvedBase->type.name, ResolvedDeclType::ResolvedStructDecl).first);

    if (!st) return report(memberExpr.location, "failed to lookup struct");

    if (resolvedBase->type.genericTypes) {
        st = specialize_generic_struct(*const_cast<ResolvedStructDecl *>(st), *resolvedBase->type.genericTypes);
        if (!st) return report(memberExpr.location, "failed to specialize generic struct");
    }

    const ResolvedFieldDecl *fieldDecl = nullptr;
    for (auto &&field : st->fields) {
        if (field->identifier == memberExpr.field) fieldDecl = field.get();
    }

    if (!fieldDecl)
        return report(memberExpr.location, '\'' + resolvedBase->type.to_str() + "' has no field called '" +
                                               std::string(memberExpr.field) + '\'');

    return std::make_unique<ResolvedMemberExpr>(memberExpr.location, std::move(resolvedBase), *fieldDecl);
}

std::unique_ptr<ResolvedArrayAtExpr> Sema::resolve_array_at_expr(const ArrayAtExpr &arrayAtExpr) {
    debug_func(arrayAtExpr.location);
    auto resolvedBase = resolve_expr(*arrayAtExpr.array);
    if (!resolvedBase) return nullptr;

    if (!resolvedBase->type.isArray && !resolvedBase->type.isPointer) {
        return report(arrayAtExpr.array->location, "cannot access element of '" + resolvedBase->type.to_str() + '\'');
    }

    varOrReturn(index, resolve_expr(*arrayAtExpr.index));

    return std::make_unique<ResolvedArrayAtExpr>(arrayAtExpr.location, std::move(resolvedBase), std::move(index));
}

std::unique_ptr<ResolvedStructInstantiationExpr> Sema::resolve_struct_instantiation(
    const StructInstantiationExpr &structInstantiation) {
    debug_func(structInstantiation.location);
    // TODO: fix
    const auto *st = static_cast<const ResolvedStructDecl *>(
        lookup(structInstantiation.structType.name, ResolvedDeclType::ResolvedStructDecl).first);
    if (!st)
        return report(structInstantiation.location,
                      "'" + std::string(structInstantiation.structType.name) + "' is not a struct type");

    if (st->genericTypes) {
        st = specialize_generic_struct(*const_cast<ResolvedStructDecl *>(st),
                                       *structInstantiation.structType.genericTypes);
        if (!st) return nullptr;
    }

    std::vector<std::unique_ptr<ResolvedFieldInitStmt>> resolvedFieldInits;
    std::map<std::string_view, const ResolvedFieldInitStmt *> inits;

    std::map<std::string_view, const ResolvedFieldDecl *> fields;
    for (auto &&fieldDecl : st->fields) fields[fieldDecl->identifier] = fieldDecl.get();

    bool error = false;
    for (auto &&initStmt : structInstantiation.fieldInitializers) {
        std::string_view id = initStmt->identifier;
        const SourceLocation &loc = initStmt->location;

        if (inits.count(id)) {
            report(loc, "field '" + std::string{id} + "' is already initialized");
            error = true;
            continue;
        }

        const ResolvedFieldDecl *fieldDecl = fields[id];
        if (!fieldDecl) {
            report(loc, "'" + std::string(st->identifier) + "' has no field named '" + std::string{id} + "'");
            error = true;
            continue;
        }

        auto resolvedInitExpr = resolve_expr(*initStmt->initializer);
        if (!resolvedInitExpr) {
            error = true;
            continue;
        }

        if (resolvedInitExpr->type.name != fieldDecl->type.name) {
            report(resolvedInitExpr->location, "'" + std::string(resolvedInitExpr->type.name) +
                                                   "' cannot be used to initialize a field of type '" +
                                                   std::string(fieldDecl->type.name) + "'");
            error = true;
            continue;
        }

        auto init = std::make_unique<ResolvedFieldInitStmt>(loc, *fieldDecl, std::move(resolvedInitExpr));
        inits[id] = resolvedFieldInits.emplace_back(std::move(init)).get();
    }

    for (auto &&fieldDecl : st->fields) {
        if (!inits.count(fieldDecl->identifier)) {
            report(structInstantiation.location,
                   "field '" + std::string(fieldDecl->identifier) + "' is not initialized");
            error = true;
            continue;
        }

        auto &initStmt = inits[fieldDecl->identifier];
        initStmt->initializer->set_constant_value(cee.evaluate(*initStmt->initializer, false));
    }

    if (error) return nullptr;

    return std::make_unique<ResolvedStructInstantiationExpr>(structInstantiation.location, *st,
                                                             std::move(resolvedFieldInits));
}

std::unique_ptr<ResolvedArrayInstantiationExpr> Sema::resolve_array_instantiation(
    const ArrayInstantiationExpr &arrayInstantiation) {
    debug_func(arrayInstantiation.location);
    std::vector<std::unique_ptr<ResolvedExpr>> resolvedinitializers;
    resolvedinitializers.reserve(arrayInstantiation.initializers.size());

    Type type = Type::builtinVoid();
    for (auto &&initializer : arrayInstantiation.initializers) {
        varOrReturn(resolvedExpr, resolve_expr(*initializer));
        auto &resolved = resolvedinitializers.emplace_back(std::move(resolvedExpr));

        resolved->set_constant_value(cee.evaluate(*resolved, false));

        if (type == Type::builtinVoid()) {
            type = resolved->type;
        }

        if (resolved->type != type) {
            return report(initializer->location, "unexpected different types in array instantiation");
        }
    }
    type.isArray = arrayInstantiation.initializers.size();

    return std::make_unique<ResolvedArrayInstantiationExpr>(arrayInstantiation.location, type,
                                                            std::move(resolvedinitializers));
}

std::unique_ptr<ResolvedErrDeclRefExpr> Sema::resolve_err_decl_ref_expr(const ErrDeclRefExpr &errDeclRef) {
    debug_func(errDeclRef.location);
    // Search in the module scope
    ResolvedErrDecl *lookupErr =
        static_cast<ResolvedErrDecl *>(lookup(errDeclRef.identifier, ResolvedDeclType::ResolvedErrDecl).first);
    if (!lookupErr) {
        // Search in the current module
        lookupErr = static_cast<ResolvedErrDecl *>(
            lookup_in_modules(m_currentModuleID, errDeclRef.identifier, ResolvedDeclType::ResolvedErrDecl));
    }
    if (!lookupErr) {
        // Search in the imports
        lookupErr = static_cast<ResolvedErrDecl *>(
            lookup_in_modules(m_currentModuleIDRef, errDeclRef.identifier, ResolvedDeclType::ResolvedErrDecl));
    }

    if (!lookupErr) {
        return report(errDeclRef.location, "err '" + std::string(errDeclRef.identifier) + "' not found");
    }

    return std::make_unique<ResolvedErrDeclRefExpr>(errDeclRef.location, *lookupErr);
}

std::unique_ptr<ResolvedErrUnwrapExpr> Sema::resolve_err_unwrap_expr(const ErrUnwrapExpr &errUnwrapExpr) {
    debug_func(errUnwrapExpr.location);
    varOrReturn(resolvedToUnwrap, resolve_expr(*errUnwrapExpr.errToUnwrap));
    if (!resolvedToUnwrap->type.isOptional)
        return report(errUnwrapExpr.location,
                      "unexpected type to unwrap that is not optional '" + resolvedToUnwrap->type.to_str() + "'");

    Type unwrapType = resolvedToUnwrap->type;
    unwrapType.isOptional = false;

    auto defers = resolve_defer_ref_stmt(false);
    return std::make_unique<ResolvedErrUnwrapExpr>(errUnwrapExpr.location, unwrapType, std::move(resolvedToUnwrap),
                                                   std::move(defers));
}

std::unique_ptr<ResolvedCatchErrExpr> Sema::resolve_catch_err_expr(const CatchErrExpr &catchErrExpr) {
    debug_func(catchErrExpr.location);
    if (catchErrExpr.errTocatch) {
        auto resolvedErr = resolve_expr(*catchErrExpr.errTocatch);
        return std::make_unique<ResolvedCatchErrExpr>(catchErrExpr.location, std::move(resolvedErr), nullptr);
    } else if (catchErrExpr.declaration) {
        auto declaration = resolve_decl_stmt(*catchErrExpr.declaration);
        return std::make_unique<ResolvedCatchErrExpr>(catchErrExpr.location, nullptr, std::move(declaration));
    } else {
        dmz_unreachable("malformed CatchErrExpr");
    }
}

std::unique_ptr<ResolvedTryErrExpr> Sema::resolve_try_err_expr(const TryErrExpr &tryErrExpr) {
    debug_func(tryErrExpr.location);
    if (tryErrExpr.errTotry) {
        auto resolvedErr = resolve_expr(*tryErrExpr.errTotry);
        return std::make_unique<ResolvedTryErrExpr>(tryErrExpr.location, std::move(resolvedErr), nullptr);
    } else if (tryErrExpr.declaration) {
        auto declaration = resolve_decl_stmt(*tryErrExpr.declaration);
        declaration->varDecl->type.isOptional = false;
        return std::make_unique<ResolvedTryErrExpr>(tryErrExpr.location, nullptr, std::move(declaration));
    } else {
        dmz_unreachable("malformed TryErrExpr");
    }
}

std::unique_ptr<ResolvedModuleDeclRefExpr> Sema::resolve_module_decl_ref_expr(const ModuleDeclRefExpr &moduleDeclRef,
                                                                              const ModuleID &prevModuleID) {
    debug_func(moduleDeclRef.location);
    ModuleID currentModuleID = prevModuleID;
    currentModuleID.modules.emplace_back(moduleDeclRef.identifier);
    m_currentModuleIDRef = currentModuleID;
    // Only check imported of the last module
    if (!dynamic_cast<ModuleDeclRefExpr *>(moduleDeclRef.expr.get())) {
        // std::string strToLookUp = currentModuleID.to_string();
        // std::string prevStr = prevModuleID.to_string();
        // auto newSize = currentModuleID.modules.size() > 0 ? strToLookUp.size() - 2 : strToLookUp.size();
        // strToLookUp = strToLookUp.substr(0, newSize);
        auto *importDecl = static_cast<ResolvedImportDecl *>(
            lookup_in_modules(m_currentModuleID, moduleDeclRef.identifier, ResolvedDeclType::ResolvedImportDecl));
        if (!importDecl) {
            // moduleDeclRef.dump();
            return report(moduleDeclRef.location, "module '" + currentModuleID.to_string() + "' not imported");
        }
        if (!importDecl->alias.empty()) {
            m_currentModuleIDRef = importDecl->moduleID;
            m_currentModuleIDRef.modules.emplace_back(importDecl->identifier);
        }
    }

    std::unique_ptr<ResolvedExpr> resolvedExpr;
    if (auto nextmoduleDeclRef = dynamic_cast<ModuleDeclRefExpr *>(moduleDeclRef.expr.get())) {
        resolvedExpr = resolve_module_decl_ref_expr(*nextmoduleDeclRef, currentModuleID);
    } else {
        resolvedExpr = resolve_expr(*moduleDeclRef.expr);
    }
    if (!resolvedExpr) return nullptr;

    return std::make_unique<ResolvedModuleDeclRefExpr>(moduleDeclRef.location, currentModuleID,
                                                       std::move(resolvedExpr));
}
}  // namespace DMZ