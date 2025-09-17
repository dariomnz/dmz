#include "semantic/SemanticSymbols.hpp"

namespace DMZ {

void ResolvedExpr::dump_constant_value(size_t level) const {
    if (value) {
        std::cerr << indent(level) << "| value: " << *value << '\n';
    }
}

void ResolvedDependencies::dump_dependencies(size_t level) const {
    // dump(level, true);
    std::cerr << indent_line(level, 0, true) << symbolName << '\n';
    std::cerr << indent_line(level + 1, 1, false) << "Depends on " << dependsOn.size() << ": [ ";
    for (auto &&dep : dependsOn) {
        if (dep->symbolName.empty()) {
            std::cerr << "id'" << dep->identifier << "' ";
        } else {
            std::cerr << "'" << dep->symbolName << "' ";
        }
    }
    std::cerr << "]\n";
    std::cerr << indent_line(level + 1, 1, false) << "Is used by " << isUsedBy.size() << ": [ ";
    for (auto &&dep : isUsedBy) {
        if (dep->symbolName.empty()) {
            std::cerr << "id'" << dep->identifier << "' ";
        } else {
            std::cerr << "'" << dep->symbolName << "' ";
        }
    }
    std::cerr << "]\n";
}

void ResolvedGenericTypeDecl::dump(size_t level, [[maybe_unused]] bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGenericTypeDecl " << identifier << '\n';
    // if (specializedType) {
    //     std::cerr << indent(level + 1) << "Specialized type ";
    //     (*specializedType).dump();
    //     std::cerr << '\n';
    // }
}

void ResolvedIntLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedIntLiteral:" << type->to_str() << " '" << value << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedFloatLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFloatLiteral:" << type->to_str() << " '" << std::fixed << value << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedCharLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCharLiteral:" << type->to_str() << " '"
              << str_to_source(std::string(1, value)) << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedBoolLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedBoolLiteral:" << type->to_str() << " '" << (value ? "true" : "false")
              << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedStringLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStringLiteral:" << type->to_str() << " '" << str_to_source(value) << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedNullLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedNullLiteral:" << type->to_str() << "\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedSizeofExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSizeofExpr:" << type->to_str() << "\n";
    std::cerr << indent(level + 1) << sizeofType->to_str() << "\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedDeclRefExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeclRefExpr:" << type->to_str() << " " << decl.identifier << '\n';
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedCallExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCallExpr:" << type->to_str() << '\n';
    callee.dump(level + 1, true);

    if (onlySelf) return;
    dump_constant_value(level);

    for (auto &&arg : arguments) arg->dump(level + 1, onlySelf);
}

void ResolvedBlock::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedBlock\n";

    if (onlySelf) return;
    for (auto &&stmt : statements) stmt->dump(level + 1, onlySelf);
    for (auto &&d : defers) d->dump(level + 1, onlySelf);
}

void ResolvedParamDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedParamDecl:";
    if (isVararg) {
        std::cerr << "vararg";
    } else {
        std::cerr << type->to_str();
    }
    std::cerr << " " << identifier << '\n';

    if (onlySelf) return;
}

void ResolvedExternFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedExternFunctionDecl " << identifier << " -> " << type->to_str() << '\n';

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);
}

void ResolvedFunctionDecl::dump(size_t level, bool onlySelf) const {
    if (auto member = dynamic_cast<const ResolvedMemberFunctionDecl *>(this)) {
        if (member->isStatic) {
            std::cerr << indent(level) << "ResolvedStaticMemberFunctionDecl ";
        } else {
            std::cerr << indent(level) << "ResolvedMemberFunctionDecl ";
        }
    } else if (dynamic_cast<const ResolvedTestDecl *>(this)) {
        std::cerr << indent(level) << "ResolvedTestDecl ";
    } else {
        std::cerr << indent(level) << "ResolvedFunctionDecl ";
    }
    std::cerr << identifier << " -> " << type->to_str() << '\n';
    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);

    if (body) body->dump(level + 1, onlySelf);
}

void ResolvedGenericFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGenericFunctionDecl " << identifier << " -> " << type->to_str() << '\n';
    for (auto &&genType : genericTypeDecls) genType->dump(level + 1, onlySelf);

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);

    if (body) body->dump(level + 1, onlySelf);

    for (auto &&func : specializations) {
        func->dump(level + 1, onlySelf);
    }
}

void ResolvedGenericFunctionDecl::dump_dependencies(size_t level) const {
    ResolvedDependencies::dump_dependencies(level);
    for (auto &&function : specializations) function->dump_dependencies(level + 1);
}

void ResolvedMemberFunctionDecl::dump(size_t level, bool onlySelf) const {
    ResolvedFunctionDecl::dump(level, onlySelf);
    if (onlySelf) return;
}

void ResolvedSpecializedFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSpecializedFunctionDecl " << identifier << specializedTypes->to_str()
              << " -> " << type->to_str() << '\n';

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);

    body->dump(level + 1, onlySelf);
}

void ResolvedReturnStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedReturnStmt\n";

    if (onlySelf) return;
    if (expr) expr->dump(level + 1, onlySelf);
    for (auto &&d : defers) d->dump(level + 1, onlySelf);
}

void ResolvedBinaryOperator::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedBinaryOperator:" << type->to_str() << " '" << get_op_str(op) << '\'' << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    lhs->dump(level + 1, onlySelf);
    rhs->dump(level + 1, onlySelf);
}

void ResolvedUnaryOperator::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedUnaryOperator:" << type->to_str() << " '" << get_op_str(op) << '\'' << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    operand->dump(level + 1, onlySelf);
}

void ResolvedRefPtrExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedRefPtrExpr:" << type->to_str() << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    expr->dump(level + 1, onlySelf);
}

void ResolvedDerefPtrExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDerefPtrExpr:" << type->to_str() << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    expr->dump(level + 1, onlySelf);
}

void ResolvedGroupingExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGroupingExpr:" << type->to_str() << "\n";
    if (onlySelf) return;
    dump_constant_value(level);

    expr->dump(level + 1, onlySelf);
}

void ResolvedIfStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedIfStmt\n";

    if (onlySelf) return;
    condition->dump(level + 1, onlySelf);
    trueBlock->dump(level + 1, onlySelf);
    if (falseBlock) falseBlock->dump(level + 1, onlySelf);
}

void ResolvedWhileStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedWhileStmt\n";

    if (onlySelf) return;
    condition->dump(level + 1, onlySelf);
    body->dump(level + 1, onlySelf);
}

void ResolvedCaseStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCaseStmt\n";

    if (onlySelf) return;
    condition->dump(level + 1, onlySelf);
    block->dump(level + 1, onlySelf);
}

void ResolvedSwitchStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSwitchStmt\n";

    if (onlySelf) return;
    condition->dump(level + 1, onlySelf);

    for (auto &&c : cases) {
        c->dump(level + 1, onlySelf);
    }
    std::cerr << indent(level + 1) << "ElseBlock\n";
    elseBlock->dump(level + 2, onlySelf);
}

void ResolvedVarDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedVarDecl:" << (isMutable ? "" : "const ") << type->to_str() << " "
              << identifier << '\n';
    if (onlySelf) return;
    if (initializer) initializer->dump(level + 1, onlySelf);
}

void ResolvedDeclStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeclStmt\n";
    if (onlySelf) return;
    varDecl->dump(level + 1, onlySelf);
}

void ResolvedAssignment::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedAssignment\n";
    if (onlySelf) return;
    assignee->dump(level + 1, onlySelf);
    expr->dump(level + 1, onlySelf);
}

void ResolvedFieldDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFieldDecl:" << type->to_str() << " " << identifier << '\n';
    if (onlySelf) return;
}

void ResolvedStructDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStructDecl " << (isPacked ? "packed " : "") << type->to_str() << '\n';

    if (onlySelf) return;
    for (auto &&field : fields) field->dump(level + 1, onlySelf);
    for (auto &&function : functions) function->dump(level + 1, onlySelf);
}

void ResolvedStructDecl::dump_dependencies(size_t level) const {
    ResolvedDependencies::dump_dependencies(level);
    for (auto &&function : functions) function->dump_dependencies(level + 1);
}

void ResolvedGenericStructDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGenericStructDecl " << (isPacked ? "packed " : "") << type->to_str() << '\n';
    for (auto &&genType : genericTypeDecls) genType->dump(level + 1, onlySelf);

    if (onlySelf) return;
    for (auto &&field : fields) field->dump(level + 1, onlySelf);
    for (auto &&function : functions) function->dump(level + 1, onlySelf);
    for (auto &&spec : specializations) spec->dump(level + 1, onlySelf);
}

void ResolvedGenericStructDecl::dump_dependencies(size_t level) const {
    ResolvedDependencies::dump_dependencies(level);
    for (auto &&function : functions) function->dump_dependencies(level + 1);
    for (auto &&spec : specializations) spec->dump_dependencies(level + 1);
}

void ResolvedSpecializedStructDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSpecializedStructDecl " << (isPacked ? "packed " : "") << type->to_str()
              << '\n';

    if (onlySelf) return;
    for (auto &&field : fields) field->dump(level + 1, onlySelf);
    for (auto &&function : functions) function->dump(level + 1, onlySelf);
}

void ResolvedMemberExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedMemberExpr:" << type->to_str() << " " << member.identifier << '\n';

    if (onlySelf) return;
    base->dump(level + 1, onlySelf);
}

void ResolvedSelfMemberExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSelfMemberExpr:" << type->to_str() << " " << member.identifier << '\n';

    if (onlySelf) return;
    base->dump(level + 1, onlySelf);
}

void ResolvedArrayAtExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedArrayAtExpr:" << type->to_str() << '\n';

    if (onlySelf) return;
    array->dump(level + 1, onlySelf);
    index->dump(level + 1, onlySelf);
}

void ResolvedFieldInitStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFieldInitStmt:" << field.type->to_str() << " " << field.identifier << '\n';

    if (onlySelf) return;
    initializer->dump(level + 1, onlySelf);
}

void ResolvedStructInstantiationExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStructInstantiationExpr:" << type->to_str() << "\n";

    if (onlySelf) return;
    for (auto &&field : fieldInitializers) field->dump(level + 1, onlySelf);
}

void ResolvedArrayInstantiationExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedArrayInstantiationExpr:" << type->to_str() << "\n";

    if (onlySelf) return;
    for (auto &&initializer : initializers) initializer->dump(level + 1, onlySelf);
}

void ResolvedDeferStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level);
    if (isErrDefer) {
        std::cerr << "ResolvedErrDeferStmt\n";
    } else {
        std::cerr << "ResolvedDeferStmt\n";
    }
    if (onlySelf) return;
    block->dump(level + 1, onlySelf);
}

void ResolvedDeferRefStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeferRefStmt\n";
    if (onlySelf) return;
    resolvedDefer.block->dump(level + 1, onlySelf);
}

void ResolvedErrorDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedErrorDecl " << identifier << '\n';
    if (onlySelf) return;
}

void ResolvedErrorGroupExprDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedErrorGroupExprDecl" << '\n';

    if (onlySelf) return;
    for (auto &&error : errors) error->dump(level + 1, onlySelf);
}

void ResolvedCatchErrorExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCatchErrorExpr:" << type->to_str() << '\n';

    if (onlySelf) return;
    if (errorToCatch) errorToCatch->dump(level + 1, onlySelf);
}

void ResolvedTryErrorExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTryErrorExpr:" << type->to_str() << '\n';

    if (onlySelf) return;
    if (errorToTry) errorToTry->dump(level + 1, onlySelf);
}

void ResolvedOrElseErrorExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedOrElseErrorExpr:" << type->to_str() << '\n';

    if (onlySelf) return;
    if (errorToOrElse) errorToOrElse->dump(level + 1, onlySelf);
    if (orElseExpr) orElseExpr->dump(level + 1, onlySelf);
}

void ResolvedModuleDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedModuleDecl " << identifier << '\n';
    if (onlySelf) return;
    for (auto &&decl : declarations) decl->dump(level + 1, onlySelf);
}

void ResolvedModuleDecl::dump_dependencies(size_t level) const {
    ResolvedDependencies::dump_dependencies(level);
    for (auto &&decl : declarations) decl->dump_dependencies(level + 1);
}

void ResolvedImportExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedImportExpr " << moduleDecl.name() << '\n';
    if (onlySelf) return;
}

void ResolvedTestDecl::dump(size_t level, bool onlySelf) const { ResolvedFunctionDecl::dump(level, onlySelf); }
}  // namespace DMZ