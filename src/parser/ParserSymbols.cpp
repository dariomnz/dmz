#include "parser/ParserSymbols.hpp"

namespace DMZ {

void Type::dump() const { std::cerr << to_str(); }

std::string Type::to_str() const {
    std::stringstream out;
    if (isRef==Ref::Ref) out << "&";
    if (isRef==Ref::ParamRef) out << "i&";
    out << name;
    if (isArray) {
        out << "[";
        if (*isArray != 0) {
            out << *isArray;
        }
        out << "]";
    }
    return out.str();
}

void FunctionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "FunctionDecl: " << identifier << " -> ";
    type.dump();
    std::cerr << '\n';

    for (auto &&param : params) param->dump(level + 1);

    body->dump(level + 1);
}

void ExternFunctionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ExternFunctionDecl: " << identifier << " -> ";
    type.dump();
    std::cerr << '\n';

    for (auto &&param : params) param->dump(level + 1);
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

void DeclRefExpr::dump(size_t level) const {
    std::cerr << indent(level) << "DeclRefExpr: ";
    std::cerr << identifier << '\n';
}

void CallExpr::dump(size_t level) const {
    std::cerr << indent(level) << "CallExpr:\n";

    callee->dump(level + 1);

    for (auto &&arg : arguments) arg->dump(level + 1);
}

void ParamDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ParamDecl: " << identifier << ':';
    if (isVararg) {
        std::cerr << "vararg";
    } else {
        type.dump();
    }
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
}  // namespace DMZ