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

void ResolvedIntLiteral::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedIntLiteral: '" << value << "'\n";
    dump_constant_value(level);
}

void ResolvedCharLiteral::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedCharLiteral: '" << str_to_source(std::string(1, value)) << "'\n";
    dump_constant_value(level);
}

void ResolvedStringLiteral::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedStringLiteral: '" << str_to_source(value) << "'\n";
    dump_constant_value(level);
}

void ResolvedDeclRefExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedDeclRefExpr: " << decl.identifier << ":";
    type.dump();
    std::cerr << '\n';
    dump_constant_value(level);
}

void ResolvedCallExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedCallExpr: " << callee.identifier << ":";
    type.dump();
    std::cerr << '\n';

    dump_constant_value(level);

    for (auto &&arg : arguments) arg->dump(level + 1);
}

void ResolvedBlock::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedBlock\n";

    for (auto &&stmt : statements) stmt->dump(level + 1);
}

void ResolvedParamDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedParamDecl: " << identifier << ":";
    if (isVararg) {
        std::cerr << "vararg";
    } else {
        type.dump();
    }
    std::cerr << '\n';
}

void ResolvedExternFunctionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedExternFunctionDecl: " << identifier << " -> ";
    type.dump();
    std::cerr << '\n';

    for (auto &&param : params) param->dump(level + 1);
}

void ResolvedFunctionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedFunctionDecl: " << identifier << " -> ";
    type.dump();
    std::cerr << '\n';

    for (auto &&param : params) param->dump(level + 1);

    body->dump(level + 1);
}

void ResolvedReturnStmt::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedReturnStmt\n";

    if (expr) expr->dump(level + 1);
}

void ResolvedBinaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedBinaryOperator: '" << get_op_str(op) << '\'' << '\n';
    dump_constant_value(level);

    lhs->dump(level + 1);
    rhs->dump(level + 1);
}

void ResolvedUnaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedUnaryOperator: '" << get_op_str(op) << '\'' << '\n';
    dump_constant_value(level);

    operand->dump(level + 1);
}

void ResolvedGroupingExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedGroupingExpr:\n";
    dump_constant_value(level);

    expr->dump(level + 1);
}

void ResolvedIfStmt::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedIfStmt\n";

    condition->dump(level + 1);
    trueBlock->dump(level + 1);
    if (falseBlock) falseBlock->dump(level + 1);
}

void ResolvedWhileStmt::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedWhileStmt\n";

    condition->dump(level + 1);
    body->dump(level + 1);
}

void ResolvedVarDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedVarDecl" << (isMutable ? "" : " const") << ": " << identifier << ':';
    type.dump();
    std::cerr << '\n';
    if (initializer) initializer->dump(level + 1);
}

void ResolvedDeclStmt::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedDeclStmt:\n";
    varDecl->dump(level + 1);
}

void ResolvedAssignment::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedAssignment:\n";
    assignee->dump(level + 1);
    expr->dump(level + 1);
}

void ResolvedFieldDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedFieldDecl: " << identifier << ':';
    type.dump();
    std::cerr << '\n';
}

void ResolvedStructDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedStructDecl: " << identifier << ':' << '\n';

    for (auto &&field : fields) field->dump(level + 1);
}

void ResolvedMemberExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedMemberExpr: " << field.identifier << ':';
    type.dump();
    std::cerr << '\n';

    base->dump(level + 1);
}

void ResolvedFieldInitStmt::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedFieldInitStmt: " << field.identifier << ':';
    field.type.dump();
    std::cerr << '\n';

    initializer->dump(level + 1);
}

void ResolvedStructInstantiationExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedStructInstantiationExpr:\n";

    for (auto &&field : fieldInitializers) field->dump(level + 1);
}
}  // namespace DMZ