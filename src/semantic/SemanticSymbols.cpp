#include "semantic/SemanticSymbols.hpp"

namespace DMZ {

void ResolvedExpr::dump_constant_value(size_t level) const {
    if (value) {
        std::cerr << indent(level) << "| value: ";
        std::visit(overload{
                       [](char val) { std::cerr << str_to_source(std::string(1, val)); },
                       [](auto val) { std::cerr << val; },
                   },
                   *value);
        std::cerr << '\n';
    }
}

void ResolvedIntLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedIntLiteral: '" << value << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedCharLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCharLiteral: '" << str_to_source(std::string(1, value)) << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedStringLiteral::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStringLiteral: '" << str_to_source(value) << "'\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedDeclRefExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeclRefExpr: " << decl.identifier << ":";
    type.dump();
    std::cerr << '\n';
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedCallExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCallExpr: " << callee.identifier << ":";
    type.dump();
    std::cerr << '\n';

    if (onlySelf) return;
    dump_constant_value(level);

    for (auto &&arg : arguments) arg->dump(level + 1);
}

void ResolvedBlock::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedBlock\n";

    if (onlySelf) return;
    for (auto &&stmt : statements) stmt->dump(level + 1);
}

void ResolvedParamDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedParamDecl: " << identifier << ":";
    if (isVararg) {
        std::cerr << "vararg";
    } else {
        type.dump();
    }
    std::cerr << '\n';
    if (onlySelf) return;
}

void ResolvedExternFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedExternFunctionDecl: " << identifier << " -> ";
    type.dump();
    std::cerr << '\n';

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1);
}

void ResolvedFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFunctionDecl: " << identifier << " -> ";
    type.dump();
    std::cerr << '\n';

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1);

    body->dump(level + 1);
}

void ResolvedReturnStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedReturnStmt\n";

    if (onlySelf) return;
    if (expr) expr->dump(level + 1);
}

void ResolvedBinaryOperator::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedBinaryOperator: '" << get_op_str(op) << '\'' << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    lhs->dump(level + 1);
    rhs->dump(level + 1);
}

void ResolvedUnaryOperator::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedUnaryOperator: '" << get_op_str(op) << '\'' << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    operand->dump(level + 1);
}

void ResolvedGroupingExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGroupingExpr:\n";
    if (onlySelf) return;
    dump_constant_value(level);

    expr->dump(level + 1);
}

void ResolvedIfStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedIfStmt\n";

    if (onlySelf) return;
    condition->dump(level + 1);
    trueBlock->dump(level + 1);
    if (falseBlock) falseBlock->dump(level + 1);
}

void ResolvedWhileStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedWhileStmt\n";

    if (onlySelf) return;
    condition->dump(level + 1);
    body->dump(level + 1);
}

void ResolvedVarDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedVarDecl" << (isMutable ? "" : " const") << ": " << identifier << ':';
    type.dump();
    std::cerr << '\n';
    if (onlySelf) return;
    if (initializer) initializer->dump(level + 1);
}

void ResolvedDeclStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeclStmt:\n";
    if (onlySelf) return;
    varDecl->dump(level + 1);
}

void ResolvedAssignment::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedAssignment:\n";
    if (onlySelf) return;
    assignee->dump(level + 1);
    expr->dump(level + 1);
}

void ResolvedFieldDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFieldDecl: " << identifier << ':';
    type.dump();
    std::cerr << '\n';
    if (onlySelf) return;
}

void ResolvedStructDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStructDecl: " << identifier << ':' << '\n';

    if (onlySelf) return;
    for (auto &&field : fields) field->dump(level + 1);
}

void ResolvedMemberExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedMemberExpr: " << field.identifier << ':';
    type.dump();
    std::cerr << '\n';

    if (onlySelf) return;
    base->dump(level + 1);
}

void ResolvedFieldInitStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFieldInitStmt: " << field.identifier << ':';
    field.type.dump();
    std::cerr << '\n';

    if (onlySelf) return;
    initializer->dump(level + 1);
}

void ResolvedStructInstantiationExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStructInstantiationExpr:\n";

    if (onlySelf) return;
    for (auto &&field : fieldInitializers) field->dump(level + 1);
}

void ResolvedDeferStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeferStmt:\n";
    if (onlySelf) return;
    block->dump(level + 1);
}

void ResolvedErrDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedErrDecl: " << identifier << '\n';
    if (onlySelf) return;
}

void ResolvedErrDeclRefExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedErrDeclRefExpr: " << decl.identifier << '\n';
    if (onlySelf) return;
}

void ResolvedErrGroupDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedErrGroupDecl: " << '\n';

    if (onlySelf) return;
    for (auto &&err : errs) err->dump(level + 1);
}

void ResolvedErrUnwrapExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedErrUnwrapExpr: ";
    type.dump();
    std::cerr << '\n';

    if (onlySelf) return;
    errToUnwrap->dump(level + 1);
}

void ResolvedCatchErrExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCatchErrExpr: ";
    type.dump();
    std::cerr << '\n';

    if (onlySelf) return;
    if (declaration) declaration->dump(level + 1);
    if (errToCatch) errToCatch->dump(level + 1);
}
}  // namespace DMZ