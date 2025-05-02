#include "AST.hpp"

#include <iostream>

namespace C {

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

void ResolvedNumberLiteral::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedNumberLiteral: '" << value << "'\n";
}

void ResolvedDeclRefExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedDeclRefExpr: " << decl.identifier << '\n';
}

void ResolvedCallExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ResolvedCallExpr: " << callee.identifier << '\n';

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
}  // namespace C
