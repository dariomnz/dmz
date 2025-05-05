#include "AST.hpp"

namespace C {

[[maybe_unused]] static inline std::string_view get_op_str(TokenType op) {
    if (op == TokenType::op_plus) return "+";
    if (op == TokenType::op_minus) return "-";
    if (op == TokenType::op_mult) return "*";
    if (op == TokenType::op_div) return "/";

    if (op == TokenType::op_not_equal) return "!=";
    if (op == TokenType::op_equal) return "==";
    if (op == TokenType::op_and) return "&&";
    if (op == TokenType::op_or) return "||";
    if (op == TokenType::op_less) return "<";
    if (op == TokenType::op_less_eq) return "<=";
    if (op == TokenType::op_more) return ">";
    if (op == TokenType::op_more_eq) return ">=";
    if (op == TokenType::op_not) return "!";

    llvm_unreachable("unexpected operator");
}

void FunctionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "FunctionDecl: " << identifier << " -> " << type.name << '\n';

    for (auto &&param : params) param->dump(level + 1);

    body->dump(level + 1);
}

void Block::dump(size_t level) const {
    std::cerr << indent(level) << "Block\n";
    for (auto &&stmt : statements) stmt->dump(level + 1);
}

void ReturnStmt::dump(size_t level) const {
    std::cerr << indent(level) << "ReturnStmt\n";

    if (expr) expr->dump(level + 1);
}

void NumberLiteral::dump(size_t level) const { std::cerr << indent(level) << "NumberLiteral: '" << value << "'\n"; }

void DeclRefExpr::dump(size_t level) const { std::cerr << indent(level) << "DeclRefExpr: " << identifier << '\n'; }

void CallExpr::dump(size_t level) const {
    std::cerr << indent(level) << "CallExpr:\n";

    callee->dump(level + 1);

    for (auto &&arg : arguments) arg->dump(level + 1);
}

void ParamDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ParamDecl: " << identifier << ':' << type.name << '\n';
}

void BinaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "BinaryOperator: '" << get_op_str(op) << '\'' << '\n';

    lhs->dump(level + 1);
    rhs->dump(level + 1);
}

void UnaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "UnaryOperator: '" << get_op_str(op) << '\'' << '\n';

    operand->dump(level + 1);
}

void GroupingExpr::dump(size_t level) const {
    std::cerr << indent(level) << "GroupingExpr:\n";

    expr->dump(level + 1);
}

void IfStmt::dump(size_t level) const {
    std::cerr << indent(level) << "IfStmt\n";

    condition->dump(level + 1);
    trueBlock->dump(level + 1);
    if (falseBlock) falseBlock->dump(level + 1);
}

void WhileStmt::dump(size_t level) const {
    std::cerr << indent(level) << "WhileStmt\n";

    condition->dump(level + 1);
    body->dump(level + 1);
}

void VarDecl::dump(size_t level) const {
    std::cerr << indent(level) << "VarDecl" << (isMutable ? "" : " const") << ": " << identifier;
    if (type) std::cerr << ':' << type->name;
    std::cerr << '\n';

    if (initializer) initializer->dump(level + 1);
}

void DeclStmt::dump(size_t level) const {
    std::cerr << indent(level) << "DeclStmt:\n";
    varDecl->dump(level + 1);
}

void Assignment::dump(size_t level) const {
    std::cerr << indent(level) << "Assignment:\n";
    variable->dump(level + 1);
    expr->dump(level + 1);
}

void ResolvedNumberLiteral::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedNumberLiteral: '" << value << "'\n";
    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: " << *val << '\n';
    }
}

void ResolvedDeclRefExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedDeclRefExpr: " << decl.identifier << '\n';
    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: " << *val << '\n';
    }
}

void ResolvedCallExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedCallExpr: " << callee.identifier << '\n';

    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: " << *val << '\n';
    }
    for (auto &&arg : arguments) arg->dump(level + 1);
}

void ResolvedBlock::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedBlock\n";

    for (auto &&stmt : statements) stmt->dump(level + 1);
}

void ResolvedParamDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedParamDecl: " << identifier << ':' << '\n';
}

void ResolvedFunctionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedFunctionDecl: " << identifier << ':' << '\n';

    for (auto &&param : params) param->dump(level + 1);

    body->dump(level + 1);
}

void ResolvedReturnStmt::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedReturnStmt\n";

    if (expr) expr->dump(level + 1);
}

void ResolvedBinaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedBinaryOperator: '" << get_op_str(op) << '\'' << '\n';

    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: " << *val << '\n';
    }
    lhs->dump(level + 1);
    rhs->dump(level + 1);
}

void ResolvedUnaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedUnaryOperator: '" << get_op_str(op) << '\'' << '\n';

    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: " << *val << '\n';
    }

    operand->dump(level + 1);
}

void ResolvedGroupingExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedGroupingExpr:\n";

    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: " << *val << '\n';
    }
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
    std::cerr << indent(level) << "ResolvedVarDecl" << (isMutable ? "" : " const") << ": " << identifier << ':' << '\n';
    if (initializer) initializer->dump(level + 1);
}

void ResolvedDeclStmt::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedDeclStmt:\n";
    varDecl->dump(level + 1);
}

void ResolvedAssignment::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedAssignment:\n";
    variable->dump(level + 1);
    expr->dump(level + 1);
}
}  // namespace C