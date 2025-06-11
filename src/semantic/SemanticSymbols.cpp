#include "semantic/SemanticSymbols.hpp"

namespace DMZ {
std::ostream &operator<<(std::ostream &os, const ModuleID &moduleID) {
    for (auto &&mod : moduleID.modules) {
        os << mod << "::";
    }
    return os;
}

void ModuleID::dump() const { std::cerr << *this; }
std::string ModuleID::to_string() const {
    std::stringstream out;
    out << *this;
    return out.str();
}

void ResolvedExpr::dump_constant_value(size_t level) const {
    if (value) {
        std::cerr << indent(level) << "| value: " << *value << '\n';
    }
}

void ResolvedGenericTypeDecl::dump(size_t level, [[maybe_unused]] bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGenericTypeDecl " << identifier << '\n';
    if (specializedType) {
        std::cerr << indent(level) << "Specialized type";
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
    std::cerr << indent(level) << "ResolvedDeclRefExpr:" << type << " " << decl.moduleID << decl.identifier << '\n';
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedCallExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCallExpr:" << type << '\n';
    callee.dump(level + 1, true);

    if (onlySelf) return;
    dump_constant_value(level);

    for (auto &&arg : arguments) arg->dump(level + 1);
}

void ResolvedBlock::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedBlock\n";

    if (onlySelf) return;
    for (auto &&stmt : statements) stmt->dump(level + 1);
    for (auto &&d : defers) d->dump(level + 1);
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
    std::cerr << indent(level) << "ResolvedExternFunctionDecl " << moduleID << identifier << " -> " << type << '\n';

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1);
}

void ResolvedFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFunctionDecl " << moduleID << identifier << " -> " << type << '\n';
    if (genericTypes) genericTypes->dump(level + 1, onlySelf);

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1);

    if (body) body->dump(level + 1);

    for (auto &&func : specializations) {
        func->dump(level + 1);
    }
}

void ResolvedMemberFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedMemberFunctionDecl\n";
    structDecl.dump(level + 1, true);

    function->dump(level + 1, onlySelf);
    if (onlySelf) return;
}

void ResolvedSpecializedFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSpecializedFunctionDecl " << moduleID << identifier;
    genericTypes.dump();
    std::cerr << " -> " << type << '\n';

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1);

    body->dump(level + 1);
}

void ResolvedReturnStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedReturnStmt\n";

    if (onlySelf) return;
    if (expr) expr->dump(level + 1);
    for (auto &&d : defers) d->dump(level + 1);
}

void ResolvedBinaryOperator::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedBinaryOperator:" << type << " '" << get_op_str(op) << '\'' << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    lhs->dump(level + 1);
    rhs->dump(level + 1);
}

void ResolvedUnaryOperator::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedUnaryOperator:" << type << " '" << get_op_str(op) << '\'' << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    operand->dump(level + 1);
}

void ResolvedRefPtrExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedRefPtrExpr:" << type << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    expr->dump(level + 1);
}

void ResolvedDerefPtrExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDerefPtrExpr:" << type << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    expr->dump(level + 1);
}

void ResolvedGroupingExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGroupingExpr:" << type << "\n";
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

void ResolvedCaseStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCaseStmt\n";

    if (onlySelf) return;
    condition->dump(level + 1);
    block->dump(level + 1);
}

void ResolvedSwitchStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSwitchStmt\n";

    if (onlySelf) return;
    condition->dump(level + 1);

    for (auto &&c : cases) {
        c->dump(level + 1);
    }
    std::cerr << indent(level + 1) << "ElseBlock\n";
    elseBlock->dump(level + 1);
}

void ResolvedVarDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedVarDecl:" << (isMutable ? "" : "const ") << type << " " << identifier
              << '\n';
    if (onlySelf) return;
    if (initializer) initializer->dump(level + 1);
}

void ResolvedDeclStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeclStmt\n";
    if (onlySelf) return;
    varDecl->dump(level + 1);
}

void ResolvedAssignment::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedAssignment\n";
    if (onlySelf) return;
    assignee->dump(level + 1);
    expr->dump(level + 1);
}

void ResolvedFieldDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFieldDecl:" << type << " " << identifier << '\n';
    if (onlySelf) return;
}

void ResolvedStructDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStructDecl:" << type << " " << moduleID << identifier << '\n';

    if (onlySelf) return;
    for (auto &&field : fields) field->dump(level + 1);
}

void ResolvedMemberExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedMemberExpr:" << type << " " << field.identifier << '\n';

    if (onlySelf) return;
    base->dump(level + 1);
}

void ResolvedArrayAtExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedArrayAtExpr:" << type << '\n';

    if (onlySelf) return;
    array->dump(level + 1);
    index->dump(level + 1);
}

void ResolvedFieldInitStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFieldInitStmt:" << field.type << " " << field.identifier << '\n';

    if (onlySelf) return;
    initializer->dump(level + 1);
}

void ResolvedStructInstantiationExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStructInstantiationExpr:" << structDecl.moduleID << structDecl.type << "\n";

    if (onlySelf) return;
    for (auto &&field : fieldInitializers) field->dump(level + 1);
}

void ResolvedArrayInstantiationExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedArrayInstantiationExpr:" << type << "\n";

    if (onlySelf) return;
    for (auto &&initializer : initializers) initializer->dump(level + 1);
}

void ResolvedDeferStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeferStmt\n";
    if (onlySelf) return;
    block->dump(level + 1);
}

void ResolvedDeferRefStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeferRefStmt\n";
    if (onlySelf) return;
    resolvedDefer.block->dump(level + 1);
}

void ResolvedErrDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedErrDecl " << moduleID << identifier << '\n';
    if (onlySelf) return;
}

void ResolvedErrDeclRefExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedErrDeclRefExpr " << decl.moduleID << decl.identifier << '\n';
    if (onlySelf) return;
}

void ResolvedErrGroupDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedErrGroupDecl" << '\n';

    if (onlySelf) return;
    for (auto &&err : errs) err->dump(level + 1);
}

void ResolvedErrUnwrapExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedErrUnwrapExpr:" << type << '\n';

    if (onlySelf) return;
    errToUnwrap->dump(level + 1);
    for (auto &&d : defers) d->dump(level + 1);
}

void ResolvedCatchErrExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCatchErrExpr:" << type << '\n';

    if (onlySelf) return;
    if (declaration) declaration->dump(level + 1);
    if (errToCatch) errToCatch->dump(level + 1);
}

void ResolvedTryErrExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTryErrExpr:" << type << '\n';

    if (onlySelf) return;
    if (declaration) declaration->dump(level + 1);
    if (errToTry) errToTry->dump(level + 1);
}

void ResolvedModuleDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedModuleDecl " << moduleID << identifier << '\n';

    if (nestedModule) nestedModule->dump(level + 1, onlySelf);
    for (auto &&decl : declarations) decl->dump(level + 1, onlySelf);
    if (onlySelf) return;
}

void ResolvedImportDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedImportDecl " << moduleID << identifier;
    if (!alias.empty()) std::cerr << " as " << alias;
    std::cerr << '\n';

    if (onlySelf) return;

    if (nestedImport) nestedImport->dump(level + 1);
}

void ResolvedModuleDeclRefExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedModuleDeclRefExpr " << moduleID << '\n';

    if (onlySelf) return;
    if (expr) expr->dump(level + 1);
}
}  // namespace DMZ