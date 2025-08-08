#include "semantic/SemanticSymbols.hpp"

namespace DMZ {

std::ostream &operator<<(std::ostream &os, const ResolvedDeclType &type) {
    if (type == ResolvedDeclType::Module) {
        os << "Module";
    } else if (type == ResolvedDeclType::ResolvedDecl) {
        os << "ResolvedDecl";
    } else if (type == ResolvedDeclType::ResolvedStructDecl) {
        os << "ResolvedStructDecl";
    } else if (type == ResolvedDeclType::ResolvedErrorDecl) {
        os << "ResolvedErrorDecl";
    } else if (type == ResolvedDeclType::ResolvedImportExpr) {
        os << "ResolvedImportExpr";
    } else if (type == ResolvedDeclType::ResolvedModuleDecl) {
        os << "ResolvedModuleDecl";
    } else if (type == ResolvedDeclType::ResolvedMemberFunctionDecl) {
        os << "ResolvedMemberFunctionDecl";
    } else if (type == ResolvedDeclType::ResolvedGenericTypeDecl) {
        os << "ResolvedGenericTypeDecl";
    }
    return os;
}

void ResolvedExpr::dump_constant_value(size_t level) const {
    if (value) {
        std::cerr << indent(level) << "| value: " << *value << '\n';
    }
}

void ResolvedGenericTypeDecl::dump(size_t level, [[maybe_unused]] bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGenericTypeDecl " << identifier << '\n';
    if (specializedType) {
        std::cerr << indent(level) << "Specialized type ";
        (*specializedType).dump();
        std::cerr << '\n';
    }
}

void ResolvedGenericTypesDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGenericTypesDecl\n";
    if (onlySelf) return;
    for (size_t i = 0; i < types.size(); i++) {
        types[i]->dump(level + 1, onlySelf);
    }
}

void ResolvedIntLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedIntLiteral:" << type << " '" << value << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedFloatLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFloatLiteral:" << type << " '" << std::fixed << value << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedCharLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCharLiteral:" << type << " '" << str_to_source(std::string(1, value))
              << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedBoolLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedBoolLiteral:" << type << " '" << (value ? "true" : "false") << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedStringLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStringLiteral:" << type << " '" << str_to_source(value) << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedDeclRefExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeclRefExpr:" << type << " " << decl.identifier << '\n';
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedCallExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCallExpr:" << type << '\n';
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
        std::cerr << type;
    }
    std::cerr << " " << identifier << '\n';

    if (onlySelf) return;
}

void ResolvedExternFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedExternFunctionDecl " << identifier << " -> " << type << '\n';

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);
}

void ResolvedFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFunctionDecl " << identifier << " -> " << type << '\n';
    if (genericTypes) genericTypes->dump(level + 1, onlySelf);

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);

    if (body) body->dump(level + 1, onlySelf);

    for (auto &&func : specializations) {
        func->dump(level + 1, onlySelf);
    }
}

void ResolvedMemberFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedMemberFunctionDecl\n";
    structDecl->dump(level + 1, true);

    function->dump(level + 1, onlySelf);
    if (onlySelf) return;
}

void ResolvedSpecializedFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSpecializedFunctionDecl " << identifier;
    genericTypes.dump();
    std::cerr << " -> " << type << '\n';

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
    std::cerr << indent(level) << "ResolvedBinaryOperator:" << type << " '" << get_op_str(op) << '\'' << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    lhs->dump(level + 1, onlySelf);
    rhs->dump(level + 1, onlySelf);
}

void ResolvedUnaryOperator::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedUnaryOperator:" << type << " '" << get_op_str(op) << '\'' << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    operand->dump(level + 1, onlySelf);
}

void ResolvedRefPtrExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedRefPtrExpr:" << type << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    expr->dump(level + 1, onlySelf);
}

void ResolvedDerefPtrExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDerefPtrExpr:" << type << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    expr->dump(level + 1, onlySelf);
}

void ResolvedGroupingExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGroupingExpr:" << type << "\n";
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
    std::cerr << indent(level) << "ResolvedVarDecl:" << (isMutable ? "" : "const ") << type << " " << identifier
              << '\n';
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
    std::cerr << indent(level) << "ResolvedFieldDecl:" << type << " " << identifier << '\n';
    if (onlySelf) return;
}

void ResolvedStructDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStructDecl " << type << '\n';

    if (onlySelf) return;
    for (auto &&field : fields) field->dump(level + 1, onlySelf);
    for (auto &&function : functions) function->dump(level + 1, onlySelf);
    for (auto &&spec : specializations) spec->dump(level + 1, onlySelf);
}

void ResolvedMemberExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedMemberExpr:" << type << " " << member.identifier << '\n';

    if (onlySelf) return;
    base->dump(level + 1, onlySelf);
}

void ResolvedArrayAtExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedArrayAtExpr:" << type << '\n';

    if (onlySelf) return;
    array->dump(level + 1, onlySelf);
    index->dump(level + 1, onlySelf);
}

void ResolvedFieldInitStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFieldInitStmt:" << field.type << " " << field.identifier << '\n';

    if (onlySelf) return;
    initializer->dump(level + 1, onlySelf);
}

void ResolvedStructInstantiationExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStructInstantiationExpr:" << structDecl.type << "\n";

    if (onlySelf) return;
    for (auto &&field : fieldInitializers) field->dump(level + 1, onlySelf);
}

void ResolvedArrayInstantiationExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedArrayInstantiationExpr:" << type << "\n";

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
    std::cerr << indent(level) << "ResolvedCatchErrorExpr:" << type << '\n';

    if (onlySelf) return;
    if (declaration) declaration->dump(level + 1, onlySelf);
    if (errorToCatch) errorToCatch->dump(level + 1, onlySelf);
}

void ResolvedTryErrorExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTryErrorExpr:" << type << '\n';

    if (onlySelf) return;
    if (errorToTry) errorToTry->dump(level + 1, onlySelf);
}

void ResolvedOrElseErrorExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedOrElseErrorExpr:" << type << '\n';

    if (onlySelf) return;
    if (errorToOrElse) errorToOrElse->dump(level + 1, onlySelf);
    if (orElseExpr) orElseExpr->dump(level + 1, onlySelf);
}

void ResolvedModuleDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedModuleDecl " << identifier << '\n';

    for (auto &&decl : declarations) decl->dump(level + 1, onlySelf);
    if (onlySelf) return;
}

void ResolvedImportExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedImportExpr " << moduleDecl.identifier << '\n';
    if (onlySelf) return;
}

void ResolvedTestDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTestDecl " << identifier << '\n';
    if (onlySelf) return;
    if (testFunction) testFunction->dump(level + 1);
}
}  // namespace DMZ