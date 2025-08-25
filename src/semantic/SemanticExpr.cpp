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
    ResolvedDecl *decl = lookup(declRefExpr.identifier, ResolvedDeclType::ResolvedDecl);
    if (!decl) {
        // dump_scopes();
        return report(declRefExpr.location, "symbol '" + std::string(declRefExpr.identifier) + "' not found");
    }

    if (!isCallee && dynamic_cast<ResolvedFuncDecl *>(decl))
        return report(declRefExpr.location, "expected to call function '" + std::string(declRefExpr.identifier) + "'");

    if (dynamic_cast<ResolvedStructDecl *>(decl))
        return report(declRefExpr.location, "expected an instance of '" + std::string(decl->type.name) + '\'');

    // println("ResolvedDeclRefExpr " << declRefExpr.identifier << " " << decl << " " << decl->identifier);
    varOrReturn(type, resolve_type(decl->type));
    auto resolvedDeclRefExpr = std::make_unique<ResolvedDeclRefExpr>(declRefExpr.location, *decl, *type);

    resolvedDeclRefExpr->set_constant_value(cee.evaluate(*resolvedDeclRefExpr, false));
    // println(type->to_str());
    return resolvedDeclRefExpr;
}

std::unique_ptr<ResolvedCallExpr> Sema::resolve_call_expr(const CallExpr &call) {
    debug_func(call.location);
    const ResolvedFuncDecl *resolvedFuncDecl = nullptr;
    bool isMemberCall = false;
    if (const auto *memberExpr = dynamic_cast<const MemberExpr *>(call.callee.get())) {
        auto resolvedMemberExpr = resolve_member_expr(*memberExpr);
        if (!resolvedMemberExpr) return nullptr;

        resolvedFuncDecl = dynamic_cast<const ResolvedFuncDecl *>(&resolvedMemberExpr->member);
        if (auto resolvedMemberFuncDecl = dynamic_cast<const ResolvedMemberFunctionDecl *>(resolvedFuncDecl)) {
            // resolvedFuncDecl = resolvedMemberFuncDecl->function.get();
            isMemberCall = true;
        }
    } else if (const auto *memberExpr = dynamic_cast<const SelfMemberExpr *>(call.callee.get())) {
        auto resolvedMemberExpr = resolve_self_member_expr(*memberExpr);
        if (!resolvedMemberExpr) return nullptr;

        resolvedFuncDecl = dynamic_cast<const ResolvedFuncDecl *>(&resolvedMemberExpr->member);

        if (auto resolvedMemberFuncDecl = dynamic_cast<const ResolvedMemberFunctionDecl *>(resolvedFuncDecl)) {
            // resolvedFuncDecl = resolvedMemberFuncDecl->function.get();
            isMemberCall = true;
        }
    } else {
        const auto *dre = dynamic_cast<const DeclRefExpr *>(call.callee.get());
        if (!dre) return report(call.location, "expression cannot be called as a function");

        varOrReturn(resolvedCallee, resolve_decl_ref_expr(*dre, true));
        resolvedFuncDecl = dynamic_cast<const ResolvedFuncDecl *>(&resolvedCallee->decl);
    }

    if (!resolvedFuncDecl) return report(call.location, "calling non-function symbol");

    if (auto resolvedFunctionDecl = dynamic_cast<const ResolvedGenericFunctionDecl *>(resolvedFuncDecl)) {
        if (call.genericTypes) {
            resolvedFuncDecl = specialize_generic_function(
                *const_cast<ResolvedGenericFunctionDecl *>(resolvedFunctionDecl), *call.genericTypes);
            if (!resolvedFuncDecl) return nullptr;
        } else {
            if (dynamic_cast<const ResolvedGenericFunctionDecl *>(resolvedFuncDecl)) {
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
        if (!isVararg || (isVararg && call_args_num < funcDeclArgs)) {
            // resolvedFuncDecl->dump();
            // println("call_args_num " << call_args_num << " func_args_num " << func_args_num);
            return report(call.location, "argument count mismatch in function call, expected " +
                                             std::to_string(func_args_num) + " actual " +
                                             std::to_string(call_args_num));
        }
    }

    std::vector<std::unique_ptr<ResolvedExpr>> resolvedArguments;

    if (const auto *memberExpr = dynamic_cast<const MemberExpr *>(call.callee.get())) {
        varOrReturn(resolvedBase, resolve_expr(*memberExpr->base));
        if (resolvedBase->type.kind == Type::Kind::Struct) {
            auto resolvedRefPtrExpr =
                std::make_unique<ResolvedRefPtrExpr>(resolvedBase->location, std::move(resolvedBase));
            resolvedArguments.emplace_back(std::move(resolvedRefPtrExpr));
        }
    }

    size_t idx = 0;
    if (isMemberCall) idx += 1;
    for (auto &&arg : call.arguments) {
        varOrReturn(resolvedArg, resolve_expr(*arg));
        // Only check until vararg
        if (idx < funcDeclArgs) {
            // Modifi ptr to reference
            // if (resolvedFuncDecl->params[idx]->type.isRef && resolvedArg->type.isPointer) {
            //     resolvedArg->type = resolvedArg->type.remove_pointer();
            //     resolvedArg->type.isRef = true;
            // }

            if (!Type::compare(resolvedFuncDecl->params[idx]->type, resolvedArg->type)) {
                return report(resolvedArg->location, "unexpected type of argument '" + resolvedArg->type.to_str() +
                                                         "' expected '" + resolvedFuncDecl->params[idx]->type.to_str() +
                                                         "'");
            }
            // if (resolvedFuncDecl->params[idx]->type.isRef) {
            //     if (!dynamic_cast<const ResolvedRefPtrExpr *>(resolvedArg.get())) {
            //         return report(resolvedArg->location, "expected to reference the value with '&'");
            //     }
            // }
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
    if (const auto *catchErrorExpr = dynamic_cast<const CatchErrorExpr *>(&expr)) {
        return resolve_catch_error_expr(*catchErrorExpr);
    }
    if (const auto *tryErrorExpr = dynamic_cast<const TryErrorExpr *>(&expr)) {
        return resolve_try_error_expr(*tryErrorExpr);
    }
    if (const auto *orelseExpr = dynamic_cast<const OrElseErrorExpr *>(&expr)) {
        return resolve_orelse_error_expr(*orelseExpr);
    }
    if (const auto *importExpr = dynamic_cast<const ImportExpr *>(&expr)) {
        return resolve_import_expr(*importExpr);
    }
    if (const auto *errorGroupExprDecl = dynamic_cast<const ErrorGroupExprDecl *>(&expr)) {
        return resolve_error_group_expr_decl(*errorGroupExprDecl);
    }
    expr.dump();
    dmz_unreachable("unexpected expression");
}

std::unique_ptr<ResolvedUnaryOperator> Sema::resolve_unary_operator(const UnaryOperator &unary) {
    debug_func(unary.location);
    varOrReturn(resolvedRHS, resolve_expr(*unary.operand));

    if (resolvedRHS->type.kind == Type::Kind::Void ||
        (unary.op == TokenType::op_excla_mark && !Type::compare(Type::builtinBool(), resolvedRHS->type)))
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

    if (const auto *memberExpr = dynamic_cast<const SelfMemberExpr *>(&assignableExpr))
        return resolve_self_member_expr(*memberExpr);

    if (const auto *arrayAtExpr = dynamic_cast<const ArrayAtExpr *>(&assignableExpr))
        return resolve_array_at_expr(*arrayAtExpr);

    if (const auto *derefExpr = dynamic_cast<const DerefPtrExpr *>(&assignableExpr))
        return resolve_deref_ptr_expr(*derefExpr);

    assignableExpr.dump();
    dmz_unreachable("unexpected assignable expression");
}

std::unique_ptr<ResolvedMemberExpr> Sema::resolve_member_expr(const MemberExpr &memberExpr) {
    debug_func(memberExpr.location);
    const ResolvedDecl *decl = nullptr;
    auto resolvedBase = resolve_expr(*memberExpr.base);
    if (!resolvedBase) return nullptr;
    // resolvedBase->dump();
    // println("Type " << resolvedBase->type.to_str() << " type");
    // println("Type " << resolvedBase->type.decl << " type");
    // println("Type " << resolvedBase->type.decl->location << " type");
    if (resolvedBase->type.kind == Type::Kind::Struct) {
        const DMZ::ResolvedStructDecl *st = nullptr;
        if (!resolvedBase->type.decl) {
            st = cast_lookup(resolvedBase->type.name, ResolvedStructDecl);
        } else {
            st = dynamic_cast<ResolvedStructDecl *>(resolvedBase->type.decl);
        }

        if (!st) return report(memberExpr.location, "failed to lookup struct " + resolvedBase->type.name);

        // if (resolvedBase->type.genericTypes) {
        //     st = specialize_generic_struct(*const_cast<ResolvedStructDecl *>(st),
        //     *resolvedBase->type.genericTypes); if (!st) return report(memberExpr.location, "failed to specialize
        //     generic struct");
        // }

        decl = cast_lookup_in_struct(*st, memberExpr.field, ResolvedDecl);
        if (!decl)
            return report(memberExpr.location, "struct \'" + resolvedBase->type.to_str() + "' has no member called '" +
                                                   std::string(memberExpr.field) + '\'');

    } else if (resolvedBase->type.kind == Type::Kind::Module) {
        auto md = resolvedBase->type.decl;
        if (!md) return report(resolvedBase->location, "expected not null the decl in type to be a module decl");
        auto moduleDecl = dynamic_cast<ResolvedModuleDecl *>(md);
        if (!moduleDecl) return report(resolvedBase->location, "expected the decl in type to be a module decl");

        decl = cast_lookup_in_module(*moduleDecl, memberExpr.field, ResolvedDecl);
        if (!decl)
            return report(memberExpr.location, "module \'" + resolvedBase->type.to_str() + "' has no member called '" +
                                                   std::string(memberExpr.field) + '\'');

    } else if (resolvedBase->type.kind == Type::Kind::ErrorGroup) {
        auto egd = resolvedBase->type.decl;
        if (!egd) return report(resolvedBase->location, "expected not null the decl in type to be a error group decl");
        auto errorGroupDecl = dynamic_cast<ResolvedErrorGroupExprDecl *>(egd);
        if (!errorGroupDecl)
            return report(resolvedBase->location, "expected the decl in type to be a error group decl");
        // println("Error group declaration size " << errorGroupDecl->errors.size());
        for (auto &&err : errorGroupDecl->errors) {
            // println("Err " << err->identifier << " field " << memberExpr.field);
            if (err->identifier == memberExpr.field) {
                decl = err.get();
                break;
            }
        }
        if (!decl)
            return report(memberExpr.location,
                          "error group has no member called '" + std::string(memberExpr.field) + '\'');
    } else {
        return report(memberExpr.base->location, "cannot access member of '" + resolvedBase->type.to_str() + '\'');
    }
    return std::make_unique<ResolvedMemberExpr>(memberExpr.location, std::move(resolvedBase), *decl);
}

std::unique_ptr<ResolvedSelfMemberExpr> Sema::resolve_self_member_expr(const SelfMemberExpr &memberExpr) {
    if (!m_currentStruct) return report(memberExpr.location, "unexpected use of self member outside a struct");
    auto decl = cast_lookup_in_struct(*m_currentStruct, memberExpr.field, ResolvedDecl);
    if (!decl)
        return report(memberExpr.location, "struct \'" + m_currentStruct->type.to_str() +
                                               "' has no self member called '" + std::string(memberExpr.field) + '\'');
    if (!m_currentFunction) return report(memberExpr.location, "internal error resolve_self_member_expr");

    if (m_currentFunction->params.size() == 0)
        return report(memberExpr.location, "internal error resolve_self_member_expr params");
    auto &param = m_currentFunction->params[0];
    auto baseRef = std::make_unique<ResolvedDeclRefExpr>(param->location, *param, param->type);

    return std::make_unique<ResolvedSelfMemberExpr>(memberExpr.location, std::move(baseRef), *decl);
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
    const auto *st = cast_lookup(structInstantiation.structType.name, ResolvedStructDecl);
    if (!st)
        return report(structInstantiation.location,
                      "'" + structInstantiation.structType.name + "' is not a struct type");

    if (auto genStruct = dynamic_cast<const ResolvedGenericStructDecl *>(st)) {
        if (genStruct->genericTypeDecls.size() != 0) {
            if (!structInstantiation.structType.genericTypes)
                return report(structInstantiation.location,
                              "'" + st->identifier + "' is a generic and need specialization");
            st = specialize_generic_struct(*const_cast<ResolvedGenericStructDecl *>(genStruct),
                                           *structInstantiation.structType.genericTypes);
            if (!st) return nullptr;
        }
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

std::unique_ptr<ResolvedCatchErrorExpr> Sema::resolve_catch_error_expr(const CatchErrorExpr &catchErrorExpr) {
    debug_func(catchErrorExpr.location);
    if (catchErrorExpr.errorToCatch) {
        auto resolvedErr = resolve_expr(*catchErrorExpr.errorToCatch);
        return std::make_unique<ResolvedCatchErrorExpr>(catchErrorExpr.location, std::move(resolvedErr), nullptr);
    } else if (catchErrorExpr.declaration) {
        auto declaration = resolve_decl_stmt(*catchErrorExpr.declaration);
        return std::make_unique<ResolvedCatchErrorExpr>(catchErrorExpr.location, nullptr, std::move(declaration));
    } else {
        dmz_unreachable("malformed CatchErrorExpr");
    }
}

std::unique_ptr<ResolvedTryErrorExpr> Sema::resolve_try_error_expr(const TryErrorExpr &tryErrorExpr) {
    debug_func(tryErrorExpr.location);
    varOrReturn(resolvedErr, resolve_expr(*tryErrorExpr.errorToTry));
    if (!resolvedErr->type.isOptional) return report(resolvedErr->location, "expect error union when using try");
    auto defers = resolve_defer_ref_stmt(false, true);
    return std::make_unique<ResolvedTryErrorExpr>(tryErrorExpr.location, std::move(resolvedErr), std::move(defers));
}

std::unique_ptr<ResolvedOrElseErrorExpr> Sema::resolve_orelse_error_expr(const OrElseErrorExpr &orelseExpr) {
    debug_func(orelseExpr.location);
    varOrReturn(resolvedErr, resolve_expr(*orelseExpr.errorToOrElse));
    if (!resolvedErr->type.isOptional) return report(resolvedErr->location, "expect error union when using try");
    varOrReturn(resolvedOrelse, resolve_expr(*orelseExpr.orElseExpr));

    if (!Type::compare(resolvedErr->type.withoutOptional(), resolvedOrelse->type)) {
        return report(orelseExpr.location, "unexpected mismatch of types in orelse expresion '" +
                                               resolvedErr->type.withoutOptional().to_str() + "' and '" +
                                               resolvedOrelse->type.to_str() + "'");
    }
    return std::make_unique<ResolvedOrElseErrorExpr>(orelseExpr.location, std::move(resolvedErr),
                                                     std::move(resolvedOrelse));
}

std::unique_ptr<ResolvedImportExpr> Sema::resolve_import_expr(const ImportExpr &importExpr) {
    debug_func(importExpr.location);

    auto it = m_modules_for_import.find(importExpr.identifier);
    if (it == m_modules_for_import.end()) {
        return report(importExpr.location, "module '" + std::string(importExpr.identifier) + "' not found");
    }

    auto im = (*it).second;

    add_dependency(im);

    return std::make_unique<ResolvedImportExpr>(importExpr.location, *im);
}
}  // namespace DMZ