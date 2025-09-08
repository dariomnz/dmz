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

ptr<ResolvedDeclRefExpr> Sema::resolve_decl_ref_expr(const DeclRefExpr &declRefExpr, bool isCallee, bool isStructInst) {
    debug_func(declRefExpr.location);
    // Search in the module scope
    ResolvedDecl *decl = lookup(declRefExpr.location, declRefExpr.identifier, ResolvedDeclType::ResolvedDecl);
    if (!decl) {
#ifdef DEBUG
        dump_scopes();
#endif
        return report(declRefExpr.location, "symbol '" + declRefExpr.identifier + "' not found");
    }

    if (!isCallee && dynamic_cast<ResolvedFuncDecl *>(decl))
        return report(declRefExpr.location, "expected to call function '" + declRefExpr.identifier + "'");

    if (!isStructInst && dynamic_cast<ResolvedStructDecl *>(decl)) {
        return report(declRefExpr.location, "expected an instance of '" + decl->type->to_str() + '\'');
    }

    // println("ResolvedDeclRefExpr " << declRefExpr.identifier << " " << decl << " " << decl->identifier);
    auto resolvedDeclRefExpr = makePtr<ResolvedDeclRefExpr>(declRefExpr.location, *decl, decl->type->clone());

    resolvedDeclRefExpr->set_constant_value(cee.evaluate(*resolvedDeclRefExpr, false));

    return resolvedDeclRefExpr;
}

ptr<ResolvedCallExpr> Sema::resolve_call_expr(const CallExpr &call) {
    debug_func(call.location);
    const ResolvedFuncDecl *resolvedFuncDecl = nullptr;
    bool isMemberCall = false;
    if (const auto *memberExpr = dynamic_cast<const MemberExpr *>(call.callee.get())) {
        auto resolvedMemberExpr = resolve_member_expr(*memberExpr);
        if (!resolvedMemberExpr) return nullptr;

        resolvedFuncDecl = dynamic_cast<const ResolvedFuncDecl *>(&resolvedMemberExpr->member);
        if (dynamic_cast<const ResolvedMemberFunctionDecl *>(resolvedFuncDecl)) {
            // resolvedFuncDecl = resolvedMemberFuncDecl->function.get();
            isMemberCall = true;
        }
    } else if (const auto *memberExpr = dynamic_cast<const SelfMemberExpr *>(call.callee.get())) {
        auto resolvedMemberExpr = resolve_self_member_expr(*memberExpr);
        if (!resolvedMemberExpr) return nullptr;

        resolvedFuncDecl = dynamic_cast<const ResolvedFuncDecl *>(&resolvedMemberExpr->member);

        if (dynamic_cast<const ResolvedMemberFunctionDecl *>(resolvedFuncDecl)) {
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
            varOrReturn(resolvedSpecialized, resolve_specialized_type(call.location, *call.genericTypes));
            resolvedFuncDecl = specialize_generic_function(
                call.location, *const_cast<ResolvedGenericFunctionDecl *>(resolvedFunctionDecl), *resolvedSpecialized);
            if (!resolvedFuncDecl) return nullptr;
        } else {
            return report(call.location, "try to call a generic function without specialization");
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

    std::vector<ptr<ResolvedExpr>> resolvedArguments;

    if (const auto *memberExpr = dynamic_cast<const MemberExpr *>(call.callee.get())) {
        varOrReturn(resolvedBase, resolve_expr(*memberExpr->base));
        if (dynamic_cast<const ResolvedTypeStruct *>(resolvedBase->type.get())) {
            auto resolvedRefPtrExpr = makePtr<ResolvedRefPtrExpr>(resolvedBase->location, std::move(resolvedBase));
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

            if (!resolvedFuncDecl->params[idx]->type->compare(*resolvedArg->type)) {
                return report(resolvedArg->location, "unexpected type of argument '" + resolvedArg->type->to_str() +
                                                         "' expected '" +
                                                         resolvedFuncDecl->params[idx]->type->to_str() + "'");
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
    return makePtr<ResolvedCallExpr>(call.location, *resolvedFuncDecl, std::move(resolvedArguments));
}

ptr<ResolvedExpr> Sema::resolve_expr(const Expr &expr) {
    debug_func(expr.location);
    if (const auto *number = dynamic_cast<const IntLiteral *>(&expr)) {
        return makePtr<ResolvedIntLiteral>(number->location, std::stod(number->value));
    }
    if (const auto *number = dynamic_cast<const FloatLiteral *>(&expr)) {
        return makePtr<ResolvedFloatLiteral>(number->location, std::stod(number->value));
    }
    if (const auto *character = dynamic_cast<const CharLiteral *>(&expr)) {
        if (auto c = str_from_source(character->value.substr(1, character->value.size() - 1))) {
            return makePtr<ResolvedCharLiteral>(character->location, (*c)[0]);
        } else {
            return report(character->location, "malformed char");
        }
    }
    if (const auto *boolean = dynamic_cast<const BoolLiteral *>(&expr)) {
        return makePtr<ResolvedBoolLiteral>(boolean->location, boolean->value == "true");
    }
    if (const auto *str = dynamic_cast<const StringLiteral *>(&expr)) {
        if (auto c = str_from_source(str->value.substr(1, str->value.size() - 2))) {
            return makePtr<ResolvedStringLiteral>(str->location, *c);
        } else {
            return report(str->location, "malformed string");
        }
    }
    if (const auto *null = dynamic_cast<const NullLiteral *>(&expr)) {
        return makePtr<ResolvedNullLiteral>(null->location);
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
    if (const auto *sizeofExpr = dynamic_cast<const SizeofExpr *>(&expr)) {
        return resolve_sizeof_expr(*sizeofExpr);
    }
    expr.dump();
    dmz_unreachable("unexpected expression");
}

ptr<ResolvedUnaryOperator> Sema::resolve_unary_operator(const UnaryOperator &unary) {
    debug_func(unary.location);
    varOrReturn(resolvedRHS, resolve_expr(*unary.operand));

    auto boolType = ResolvedTypeBool{SourceLocation{}};

    if (dynamic_cast<const ResolvedTypeVoid *>(resolvedRHS->type.get()) ||
        (unary.op == TokenType::op_excla_mark && !boolType.compare(*resolvedRHS->type)))
        return report(resolvedRHS->location, '\'' + resolvedRHS->type->to_str() +
                                                 "' cannot be used as an operand to unary operator '" +
                                                 get_op_str(unary.op) + "'");
    ptr<DMZ::ResolvedType> resolvedType = nullptr;
    if (op_generate_bool(unary.op)) {
        resolvedType = boolType.clone();
    } else if (unary.op == TokenType::amp) {
        resolvedType = makePtr<ResolvedTypePointer>(resolvedRHS->type->location, resolvedRHS->type->clone());
    } else {
        resolvedType = resolvedRHS->type->clone();
    }
    return makePtr<ResolvedUnaryOperator>(unary.location, std::move(resolvedType), unary.op, std::move(resolvedRHS));
}

ptr<ResolvedRefPtrExpr> Sema::resolve_ref_ptr_expr(const RefPtrExpr &refPtrExpr) {
    debug_func(refPtrExpr.location);
    varOrReturn(resolvedExpr, resolve_expr(*refPtrExpr.expr));
    return makePtr<ResolvedRefPtrExpr>(refPtrExpr.location, std::move(resolvedExpr));
}

ptr<ResolvedDerefPtrExpr> Sema::resolve_deref_ptr_expr(const DerefPtrExpr &derefPtrExpr) {
    debug_func(derefPtrExpr.location);
    varOrReturn(resolvedExpr, resolve_expr(*derefPtrExpr.expr));
    auto ptrType = dynamic_cast<const ResolvedTypePointer *>(resolvedExpr->type.get());
    if (!ptrType)
        return report(resolvedExpr->location, "expected pointer type, actual '" + resolvedExpr->type->to_str() + "'");
    return makePtr<ResolvedDerefPtrExpr>(derefPtrExpr.location, ptrType->pointerType->clone(), std::move(resolvedExpr));
}

ptr<ResolvedBinaryOperator> Sema::resolve_binary_operator(const BinaryOperator &binop) {
    debug_func(binop.location);
    varOrReturn(resolvedLHS, resolve_expr(*binop.lhs));
    varOrReturn(resolvedRHS, resolve_expr(*binop.rhs));

    if (!dynamic_cast<const ResolvedTypeNumber *>(resolvedLHS->type.get()) &&
        !dynamic_cast<const ResolvedTypePointer *>(resolvedLHS->type.get())) {
        return report(resolvedLHS->location,
                      '\'' + resolvedLHS->type->to_str() + "' cannot be used as LHS operand to binary operator");
    }
    if (!dynamic_cast<const ResolvedTypeNumber *>(resolvedRHS->type.get()) &&
        !dynamic_cast<const ResolvedTypePointer *>(resolvedRHS->type.get())) {
        return report(resolvedRHS->location,
                      '\'' + resolvedRHS->type->to_str() + "' cannot be used as RHS operand to binary operator");
    }
    if (!resolvedLHS->type->compare(*resolvedRHS->type)) {
        return report(binop.location, "unexpected type in binop, expected '" + resolvedLHS->type->to_str() +
                                          "' actual '" + resolvedRHS->type->to_str() + "' ");
    }

    auto ret =
        makePtr<ResolvedBinaryOperator>(binop.location, binop.op, std::move(resolvedLHS), std::move(resolvedRHS));
    if (op_generate_bool(binop.op)) {
        ret->type = makePtr<ResolvedTypeBool>(binop.location);
    }
    return ret;
}

ptr<ResolvedGroupingExpr> Sema::resolve_grouping_expr(const GroupingExpr &grouping) {
    debug_func(grouping.location);
    varOrReturn(resolvedExpr, resolve_expr(*grouping.expr));
    return makePtr<ResolvedGroupingExpr>(grouping.location, std::move(resolvedExpr));
}

ptr<ResolvedAssignableExpr> Sema::resolve_assignable_expr(const AssignableExpr &assignableExpr) {
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

ptr<ResolvedMemberExpr> Sema::resolve_member_expr(const MemberExpr &memberExpr) {
    debug_func(memberExpr.location);
    const ResolvedDecl *decl = nullptr;
    auto resolvedBase = resolve_expr(*memberExpr.base);
    if (!resolvedBase) return nullptr;
    // resolvedBase->dump();
    // println("Type " << resolvedBase->type.to_str() << " type");
    // println("Type " << resolvedBase->type.decl << " type");
    // println("Type " << resolvedBase->type.decl->location << " type");
    ResolvedType *baseType = resolvedBase->type.get();
    // TODO: change acces members
    if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(baseType)) {
        baseType = ptrType->pointerType.get();
    }

    if (auto struType = dynamic_cast<const ResolvedTypeStruct *>(baseType)) {
        const DMZ::ResolvedStructDecl *st = struType->decl;

        if (!st) return report(memberExpr.location, "failed to lookup struct " + resolvedBase->type->to_str());

        // if (resolvedBase->type.genericTypes) {
        //     st = specialize_generic_struct(*const_cast<ResolvedStructDecl *>(st),
        //     *resolvedBase->type.genericTypes); if (!st) return report(memberExpr.location, "failed to specialize
        //     generic struct");
        // }

        decl = cast_lookup_in_struct(*st, memberExpr.field, ResolvedDecl);
        if (!decl)
            return report(memberExpr.location, "struct \'" + resolvedBase->type->to_str() + "' has no member called '" +
                                                   memberExpr.field + '\'');

    } else if (auto modType = dynamic_cast<const ResolvedTypeModule *>(baseType)) {
        auto moduleDecl = modType->moduleDecl;
        if (!moduleDecl)
            return report(resolvedBase->location, "expected not null the decl in type to be a module decl");
        // moduleDecl->dump();
        decl = cast_lookup_in_module(*moduleDecl, memberExpr.field, ResolvedDecl);
        if (!decl)
            return report(memberExpr.location, "module \'" + resolvedBase->type->to_str() + "' has no member called '" +
                                                   memberExpr.field + '\'');

    } else if (auto modType = dynamic_cast<const ResolvedTypeErrorGroup *>(baseType)) {
        auto errorGroupDecl = modType->decl;
        if (!errorGroupDecl)
            return report(resolvedBase->location, "expected not null the decl in type to be a error group decl");
        // println("Error group declaration size " << errorGroupDecl->errors.size());
        for (auto &&err : errorGroupDecl->errors) {
            // println("Err " << err->identifier << " field " << memberExpr.field);
            if (err->identifier == memberExpr.field) {
                decl = err.get();
                break;
            }
        }
        if (!decl) return report(memberExpr.location, "error group has no member called '" + memberExpr.field + '\'');
    } else {
        return report(memberExpr.base->location, "cannot access member of '" + resolvedBase->type->to_str() + '\'');
    }
    return makePtr<ResolvedMemberExpr>(memberExpr.location, std::move(resolvedBase), *decl);
}

ptr<ResolvedSelfMemberExpr> Sema::resolve_self_member_expr(const SelfMemberExpr &memberExpr) {
    if (!m_currentStruct) return report(memberExpr.location, "unexpected use of self member outside a struct");
    auto decl = cast_lookup_in_struct(*m_currentStruct, memberExpr.field, ResolvedDecl);
    if (!decl) {
        m_currentStruct->dump();
        return report(memberExpr.location, "struct \'" + m_currentStruct->type->to_str() +
                                               "' has no self member called '" + memberExpr.field + '\'');
    }
    if (!m_currentFunction) return report(memberExpr.location, "internal error resolve_self_member_expr");

    if (m_currentFunction->params.size() == 0)
        return report(memberExpr.location, "internal error resolve_self_member_expr params");
    auto &param = m_currentFunction->params[0];
    auto baseRef = makePtr<ResolvedDeclRefExpr>(param->location, *param, param->type->clone());

    return makePtr<ResolvedSelfMemberExpr>(memberExpr.location, std::move(baseRef), *decl);
}

ptr<ResolvedArrayAtExpr> Sema::resolve_array_at_expr(const ArrayAtExpr &arrayAtExpr) {
    debug_func(arrayAtExpr.location);
    auto resolvedBase = resolve_expr(*arrayAtExpr.array);
    if (!resolvedBase) return nullptr;

    if (!dynamic_cast<const ResolvedTypeArray *>(resolvedBase->type.get()) &&
        !dynamic_cast<const ResolvedTypePointer *>(resolvedBase->type.get())) {
        return report(arrayAtExpr.array->location, "cannot access element of '" + resolvedBase->type->to_str() + '\'');
    }
    ResolvedType *derefType = nullptr;
    if (auto arrType = dynamic_cast<const ResolvedTypeArray *>(resolvedBase->type.get())) {
        derefType = arrType->arrayType.get();
    } else if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(resolvedBase->type.get())) {
        derefType = ptrType->pointerType.get();
    }

    varOrReturn(index, resolve_expr(*arrayAtExpr.index));

    return makePtr<ResolvedArrayAtExpr>(arrayAtExpr.location, derefType->clone(), std::move(resolvedBase),
                                        std::move(index));
}

ptr<ResolvedStructInstantiationExpr> Sema::resolve_struct_instantiation(
    const StructInstantiationExpr &structInstantiation) {
    debug_func(structInstantiation.location);
    ptr<DMZ::ResolvedExpr> resolvedBase;
    if (auto declRef = dynamic_cast<DeclRefExpr *>(structInstantiation.base.get())) {
        resolvedBase = resolve_decl_ref_expr(*declRef, false, true);
    } else {
        resolvedBase = resolve_expr(*structInstantiation.base);
    }

    if (!resolvedBase) return nullptr;

    if (!dynamic_cast<const ResolvedTypeStruct *>(resolvedBase->type.get())) {
        return report(structInstantiation.base->location, "expected a struct in a struct instantiation");
    }
    auto auxstruType = static_cast<const ResolvedTypeStruct *>(resolvedBase->type.get());
    auto st = auxstruType->decl;
    if (!st) {
        return report(structInstantiation.base->location, "expected a struct in a struct instantiation");
    }

    if (auto genStruct = dynamic_cast<ResolvedGenericStructDecl *>(st)) {
        if (structInstantiation.genericTypes.types.size() == 0)
            return report(structInstantiation.location,
                          "'" + st->identifier + "' is a generic and need specialization");

        varOrReturn(resolvedSpecialized,
                    resolve_specialized_type(structInstantiation.location, structInstantiation.genericTypes));
        st = specialize_generic_struct(structInstantiation.location, *genStruct, *resolvedSpecialized);
        if (!st) return nullptr;
    }

    std::vector<ptr<ResolvedFieldInitStmt>> resolvedFieldInits;
    std::map<std::string_view, const ResolvedFieldInitStmt *> inits;

    std::map<std::string_view, const ResolvedFieldDecl *> fields;
    for (auto &&fieldDecl : st->fields) fields[fieldDecl->identifier] = fieldDecl.get();

    bool error = false;
    for (auto &&initStmt : structInstantiation.fieldInitializers) {
        std::string &id = initStmt->identifier;
        const SourceLocation &loc = initStmt->location;
        debug_msg("Initialice " << id);

        if (inits.count(id)) {
            report(loc, "field '" + id + "' is already initialized");
            error = true;
            continue;
        }

        const ResolvedFieldDecl *fieldDecl = fields[id];
        if (!fieldDecl) {
            report(loc, "'" + st->identifier + "' has no field named '" + id + "'");
            error = true;
            continue;
        }

        auto resolvedInitExpr = resolve_expr(*initStmt->initializer);
        if (!resolvedInitExpr) {
            error = true;
            continue;
        }

        if (!fieldDecl->type->compare(*resolvedInitExpr->type)) {
            report(resolvedInitExpr->location, "'" + resolvedInitExpr->type->to_str() +
                                                   "' cannot be used to initialize a field of type '" +
                                                   fieldDecl->type->to_str() + "'");
            error = true;
            continue;
        }

        auto init = makePtr<ResolvedFieldInitStmt>(loc, *fieldDecl, std::move(resolvedInitExpr));
        inits[id] = resolvedFieldInits.emplace_back(std::move(init)).get();
    }

    for (auto &&fieldDecl : st->fields) {
        if (!inits.count(fieldDecl->identifier)) {
            report(structInstantiation.location, "field '" + fieldDecl->identifier + "' is not initialized");
            error = true;
            continue;
        }

        auto &initStmt = inits[fieldDecl->identifier];
        initStmt->initializer->set_constant_value(cee.evaluate(*initStmt->initializer, false));
    }

    if (error) return nullptr;

    return makePtr<ResolvedStructInstantiationExpr>(structInstantiation.location, *st, std::move(resolvedFieldInits));
}

ptr<ResolvedArrayInstantiationExpr> Sema::resolve_array_instantiation(
    const ArrayInstantiationExpr &arrayInstantiation) {
    debug_func(arrayInstantiation.location);
    std::vector<ptr<ResolvedExpr>> resolvedinitializers;
    resolvedinitializers.reserve(arrayInstantiation.initializers.size());

    ptr<ResolvedType> type = makePtr<ResolvedTypeVoid>(SourceLocation{});
    bool only_first = true;
    for (auto &&initializer : arrayInstantiation.initializers) {
        varOrReturn(resolvedExpr, resolve_expr(*initializer));
        auto &resolved = resolvedinitializers.emplace_back(std::move(resolvedExpr));

        resolved->set_constant_value(cee.evaluate(*resolved, false));

        if (only_first) {
            only_first = false;
            type = resolved->type->clone();
        }

        if (!resolved->type->compare(*type)) {
            return report(initializer->location, "unexpected different types in array instantiation expected '" +
                                                     resolved->type->to_str() + "' actual '" + type->to_str() + "'");
        }
    }
    type = makePtr<ResolvedTypeArray>(SourceLocation{}, std::move(type), arrayInstantiation.initializers.size());

    return makePtr<ResolvedArrayInstantiationExpr>(arrayInstantiation.location, std::move(type),
                                                   std::move(resolvedinitializers));
}

ptr<ResolvedCatchErrorExpr> Sema::resolve_catch_error_expr(const CatchErrorExpr &catchErrorExpr) {
    debug_func(catchErrorExpr.location);
    auto resolvedErr = resolve_expr(*catchErrorExpr.errorToCatch);
    return makePtr<ResolvedCatchErrorExpr>(catchErrorExpr.location, std::move(resolvedErr));
}

ptr<ResolvedTryErrorExpr> Sema::resolve_try_error_expr(const TryErrorExpr &tryErrorExpr) {
    debug_func(tryErrorExpr.location);
    varOrReturn(resolvedErr, resolve_expr(*tryErrorExpr.errorToTry));
    if (!dynamic_cast<const ResolvedTypeOptional *>(resolvedErr->type.get()))
        return report(resolvedErr->location, "expect error union when using try");
    auto optType = static_cast<const ResolvedTypeOptional *>(resolvedErr->type.get());
    auto defers = resolve_defer_ref_stmt(false, true);
    return makePtr<ResolvedTryErrorExpr>(tryErrorExpr.location, optType->optionalType->clone(), std::move(resolvedErr),
                                         std::move(defers));
}

ptr<ResolvedOrElseErrorExpr> Sema::resolve_orelse_error_expr(const OrElseErrorExpr &orelseExpr) {
    debug_func(orelseExpr.location);
    varOrReturn(resolvedErr, resolve_expr(*orelseExpr.errorToOrElse));
    if (!dynamic_cast<const ResolvedTypeOptional *>(resolvedErr->type.get()))
        return report(resolvedErr->location, "expect error union when using orelse");
    varOrReturn(resolvedOrelse, resolve_expr(*orelseExpr.orElseExpr));
    auto resolvedErrOptional = static_cast<const ResolvedTypeOptional *>(resolvedErr->type.get());
    if (!resolvedErrOptional->optionalType->compare(*resolvedOrelse->type)) {
        return report(orelseExpr.location, "unexpected mismatch of types in orelse expresion '" +
                                               resolvedErrOptional->optionalType->to_str() + "' and '" +
                                               resolvedOrelse->type->to_str() + "'");
    }
    return makePtr<ResolvedOrElseErrorExpr>(orelseExpr.location, resolvedErrOptional->optionalType->clone(),
                                            std::move(resolvedErr), std::move(resolvedOrelse));
}

ptr<ResolvedImportExpr> Sema::resolve_import_expr(const ImportExpr &importExpr) {
    debug_func(importExpr.location);

    auto it = m_modules_for_import.find(importExpr.identifier);
    if (it == m_modules_for_import.end()) {
        return report(importExpr.location, "module '" + importExpr.identifier + "' not found");
    }

    auto im = (*it).second;

    add_dependency(im);

    return makePtr<ResolvedImportExpr>(importExpr.location, *im);
}

ptr<ResolvedSizeofExpr> Sema::resolve_sizeof_expr(const SizeofExpr &sizeofExpr) {
    debug_func(sizeofExpr.location);
    auto type = resolve_type(sizeofExpr.sizeofType);
    if (!type)
        return report(sizeofExpr.sizeofType.location, "cannot resolve type '" + sizeofExpr.sizeofType.to_str() + "'");

    return makePtr<ResolvedSizeofExpr>(sizeofExpr.location, std::move(type));
}
}  // namespace DMZ