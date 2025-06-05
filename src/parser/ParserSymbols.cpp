#include "parser/ParserSymbols.hpp"

namespace DMZ {

void Type::dump() const { std::cerr << *this; }

std::string Type::to_str() const {
    std::stringstream out;
    out << *this;
    return out.str();
}

std::ostream &operator<<(std::ostream &os, const Type &t) {
    if (t.isRef) os << "&";
    if (t.isPointer)
        for (int i = 0; i < *t.isPointer; i++) os << "*";
    os << t.name;
    if (t.isArray) {
        os << "[";
        if (*t.isArray != 0) {
            os << *t.isArray;
        }
        os << "]";
    }
    if (t.isOptional) os << "?";
    return os;
}

void FunctionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "FunctionDecl " << identifier << " -> " << type << "\n";

    for (auto &&param : params) param->dump(level + 1);

    body->dump(level + 1);
}

void ExternFunctionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ExternFunctionDecl " << identifier << " -> " << type << "\n";

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

void IntLiteral::dump(size_t level) const { std::cerr << indent(level) << "IntLiteral '" << value << "'\n"; }

void FloatLiteral::dump(size_t level) const { std::cerr << indent(level) << "FloatLiteral '" << value << "'\n"; }

void CharLiteral::dump(size_t level) const { std::cerr << indent(level) << "CharLiteral '" << value << "'\n"; }

void BoolLiteral::dump(size_t level) const { std::cerr << indent(level) << "BoolLiteral '" << value << "'\n"; }

void StringLiteral::dump(size_t level) const { std::cerr << indent(level) << "StringLiteral '" << value << "'\n"; }

void DeclRefExpr::dump(size_t level) const {
    std::cerr << indent(level) << "DeclRefExpr ";
    std::cerr << identifier << '\n';
}

void CallExpr::dump(size_t level) const {
    std::cerr << indent(level) << "CallExpr\n";

    callee->dump(level + 1);

    for (auto &&arg : arguments) arg->dump(level + 1);
}

void ParamDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ParamDecl:";
    if (isVararg) {
        std::cerr << "vararg";
    } else {
        std::cerr << type;
    }
    std::cerr << " " << identifier << '\n';
}

void BinaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "BinaryOperator '" << get_op_str(op) << '\'' << '\n';

    lhs->dump(level + 1);
    rhs->dump(level + 1);
}

void UnaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "UnaryOperator '" << get_op_str(op) << '\'' << '\n';

    operand->dump(level + 1);
}

void RefPtrExpr::dump(size_t level) const {
    std::cerr << indent(level) << "RefPtrExpr" << '\n';

    expr->dump(level + 1);
}

void DerefPtrExpr::dump(size_t level) const {
    std::cerr << indent(level) << "DerefPtrExpr" << '\n';

    expr->dump(level + 1);
}

void GroupingExpr::dump(size_t level) const {
    std::cerr << indent(level) << "GroupingExpr\n";

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

void CaseStmt::dump(size_t level) const {
    std::cerr << indent(level) << "CaseStmt\n";

    condition->dump(level + 1);
    block->dump(level + 1);
}

void SwitchStmt::dump(size_t level) const {
    std::cerr << indent(level) << "SwitchStmt\n";

    condition->dump(level + 1);

    for (auto &&c : cases) {
        c->dump(level + 1);
    }
    std::cerr << indent(level + 1) << "ElseBlock\n";
    elseBlock->dump(level + 1);
}

void VarDecl::dump(size_t level) const {
    std::cerr << indent(level) << "VarDecl:" << (isMutable ? "" : "const ");
    if (type) {
        std::cerr << *type;
    }
    std::cerr << " " << identifier << '\n';

    if (initializer) initializer->dump(level + 1);
}

void DeclStmt::dump(size_t level) const {
    std::cerr << indent(level) << "DeclStmt\n";
    varDecl->dump(level + 1);
}

void Assignment::dump(size_t level) const {
    std::cerr << indent(level) << "Assignment\n";
    assignee->dump(level + 1);
    expr->dump(level + 1);
}

void FieldDecl::dump(size_t level) const {
    std::cerr << indent(level) << "FieldDecl:" << type << " " << identifier << '\n';
}

void StructDecl::dump(size_t level) const {
    std::cerr << indent(level) << "StructDecl " << identifier << '\n';

    for (auto &&field : fields) field->dump(level + 1);
}

void MemberExpr::dump(size_t level) const {
    std::cerr << indent(level) << "MemberExpr ." << field << '\n';

    base->dump(level + 1);
}

void ArrayAtExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ArrayAtExpr" << '\n';

    array->dump(level + 1);
    index->dump(level + 1);
}

void StructInstantiationExpr::dump(size_t level) const {
    std::cerr << indent(level) << "StructInstantiationExpr " << identifier << '\n';

    for (auto &&field : fieldInitializers) field->dump(level + 1);
}

void ArrayInstantiationExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ArrayInstantiationExpr " << '\n';

    for (auto &&initializer : initializers) initializer->dump(level + 1);
}

void FieldInitStmt::dump(size_t level) const {
    std::cerr << indent(level) << "FieldInitStmt " << identifier << '\n';
    initializer->dump(level + 1);
}

void DeferStmt::dump(size_t level) const {
    std::cerr << indent(level) << "DeferStmt " << '\n';
    block->dump(level + 1);
}

void ErrDecl::dump(size_t level) const { std::cerr << indent(level) << "ErrDecl " << identifier << '\n'; }

void ErrDeclRefExpr::dump(size_t level) const { std::cerr << indent(level) << "ErrDeclRefExpr " << identifier << '\n'; }

void ErrGroupDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ErrGroupDecl " << '\n';

    for (auto &&err : errs) err->dump(level + 1);
}

void ErrUnwrapExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ErrUnwrapExpr " << '\n';

    errToUnwrap->dump(level + 1);
}

void CatchErrExpr::dump(size_t level) const {
    std::cerr << indent(level) << "CatchErrExpr " << '\n';

    if (declaration) declaration->dump(level + 1);
    if (errTocatch) errTocatch->dump(level + 1);
}

void TryErrExpr::dump(size_t level) const {
    std::cerr << indent(level) << "TryErrExpr " << '\n';

    if (declaration) declaration->dump(level + 1);
    if (errTotry) errTotry->dump(level + 1);
}

void ModuleDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ModuleDecl " << identifier << '\n';

    for (auto &&decl : declarations) decl->dump(level + 1);

    if (nestedModule) nestedModule->dump(level + 1);
}

void ModuleDeclRefExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ModuleDeclRefExpr " << identifier << '\n';

    if (expr) expr->dump(level + 1);
}

void ImportDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ImportDecl " << identifier;
    if (!alias.empty()) std::cerr << " as " << alias;
    std::cerr << '\n';

    if (nestedImport) nestedImport->dump(level + 1);
}

std::string ImportDecl::get_moduleID() const {
    std::string moduleID(identifier);
    const ImportDecl *currentImportDecl = this;
    while (currentImportDecl->nestedImport) {
        currentImportDecl = currentImportDecl->nestedImport.get();
        moduleID += "::";
        moduleID += currentImportDecl->identifier;
    }
    return moduleID;
}
}  // namespace DMZ