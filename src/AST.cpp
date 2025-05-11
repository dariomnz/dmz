#include "AST.hpp"

namespace C {

void Type::dump() const {
    std::cerr << name;
    if (isSlice) std::cerr << "[]";
}

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
    std::cerr << indent(level) << "FunctionDecl: " << identifier << " -> ";
    type.dump();
    std::cerr << '\n';

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

void IntLiteral::dump(size_t level) const { std::cerr << indent(level) << "IntLiteral: '" << value << "'\n"; }

void CharLiteral::dump(size_t level) const { std::cerr << indent(level) << "CharLiteral: '" << value << "'\n"; }

void StringLiteral::dump(size_t level) const { std::cerr << indent(level) << "StringLiteral: '" << value << "'\n"; }

void DeclRefExpr::dump(size_t level) const { std::cerr << indent(level) << "DeclRefExpr: " << identifier << '\n'; }

void CallExpr::dump(size_t level) const {
    std::cerr << indent(level) << "CallExpr:\n";

    callee->dump(level + 1);

    for (auto &&arg : arguments) arg->dump(level + 1);
}

void ParamDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ParamDecl: " << identifier << ':';
    type.dump();
    std::cerr << '\n';
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
    if (type) {
        std::cerr << ':';
        type->dump();
    }
    std::cerr << '\n';

    if (initializer) initializer->dump(level + 1);
}

void DeclStmt::dump(size_t level) const {
    std::cerr << indent(level) << "DeclStmt:\n";
    varDecl->dump(level + 1);
}

void Assignment::dump(size_t level) const {
    std::cerr << indent(level) << "Assignment:\n";
    assignee->dump(level + 1);
    expr->dump(level + 1);
}

void FieldDecl::dump(size_t level) const {
    std::cerr << indent(level) << "FieldDecl: " << identifier << ':';
    type.dump();
    std::cerr << '\n';
}

void StructDecl::dump(size_t level) const {
    std::cerr << indent(level) << "StructDecl: " << identifier << '\n';

    for (auto &&field : fields) field->dump(level + 1);
}

void MemberExpr::dump(size_t level) const {
    std::cerr << indent(level) << "MemberExpr: ." << field << '\n';

    base->dump(level + 1);
}

void StructInstantiationExpr::dump(size_t level) const {
    std::cerr << indent(level) << "StructInstantiationExpr: " << identifier << '\n';

    for (auto &&field : fieldInitializers) field->dump(level + 1);
}

void FieldInitStmt::dump(size_t level) const {
    std::cerr << indent(level) << "FieldInitStmt: " << identifier << '\n';
    initializer->dump(level + 1);
}

void ResolvedIntLiteral::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedIntLiteral: '" << value << "'\n";
    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: ";
        std::visit([](auto &&i) { std::cerr << i; }, *val);
        std::cerr << '\n';
    }
}

void ResolvedCharLiteral::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedCharLiteral: '" << value << "'\n";
    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: ";
        std::visit([](auto &&i) { std::cerr << i; }, *val);
        std::cerr << '\n';
    }
}

void ResolvedStringLiteral::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedStringLiteral: '" << value << "'\n";
    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: ";
        std::visit([](auto &&i) { std::cerr << i; }, *val);
        std::cerr << '\n';
    }
}

void ResolvedDeclRefExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedDeclRefExpr: " << decl.identifier << '\n';
    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: ";
        std::visit([](auto &&i) { std::cerr << i; }, *val);
        std::cerr << '\n';
    }
}

void ResolvedCallExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedCallExpr: " << callee.identifier << '\n';

    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: ";
        std::visit([](auto &&i) { std::cerr << i; }, *val);
        std::cerr << '\n';
    }
    for (auto &&arg : arguments) arg->dump(level + 1);
}

void ResolvedBlock::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedBlock\n";

    for (auto &&stmt : statements) stmt->dump(level + 1);
}

void ResolvedParamDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedParamDecl: " << identifier << ": ";
    type.dump();
    std::cerr << '\n';
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
        std::cerr << indent(level) << "| value: ";
        std::visit([](auto &&i) { std::cerr << i; }, *val);
        std::cerr << '\n';
    }
    lhs->dump(level + 1);
    rhs->dump(level + 1);
}

void ResolvedUnaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedUnaryOperator: '" << get_op_str(op) << '\'' << '\n';

    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: ";
        std::visit([](auto &&i) { std::cerr << i; }, *val);
        std::cerr << '\n';
    }

    operand->dump(level + 1);
}

void ResolvedGroupingExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedGroupingExpr:\n";

    if (auto val = get_constant_value()) {
        std::cerr << indent(level) << "| value: ";
        std::visit([](auto &&i) { std::cerr << i; }, *val);
        std::cerr << '\n';
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
    assignee->dump(level + 1);
    expr->dump(level + 1);
}

void ResolvedFieldDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedFieldDecl: " << identifier << '\n';
}

void ResolvedStructDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedStructDecl: " << identifier << ':' << '\n';

    for (auto &&field : fields) field->dump(level + 1);
}

void ResolvedMemberExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedMemberExpr: " << field.identifier << '\n';

    base->dump(level + 1);
}

void ResolvedFieldInitStmt::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedFieldInitStmt: " << field.identifier << '\n';

    initializer->dump(level + 1);
}

void ResolvedStructInstantiationExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedStructInstantiationExpr:\n";

    for (auto &&field : fieldInitializers) field->dump(level + 1);
}
}  // namespace C