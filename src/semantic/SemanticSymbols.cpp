#include "semantic/SemanticSymbols.hpp"

#include <iostream>

namespace DMZ {

std::ostream &operator<<(std::ostream &os, const ResolvedState &state) {
    switch (state) {
        case ResolvedState::Unresolved:
            return os << "Unresolved";
        case ResolvedState::InProgress:
            return os << "InProgress";
        case ResolvedState::DeclResolved:
            return os << "DeclResolved";
        case ResolvedState::FullyResolved:
            return os << "FullyResolved";
        case ResolvedState::Error:
            return os << "Error";
    }
}

template <>
void ConstantValueContainer<ComptimeValue>::dump_constant_value(size_t level) const {
    if (value.has_value()) {
        std::cerr << indent(level) << "| value: " << value.value() << '\n';
    }
}

void ResolvedDecl::dump_dependencies(size_t level, bool dot_format) const {
    if (!dot_format) {
        std::cerr << indent_line(level, 0, true) << name() << " [" << state << "]\n";
    } else {
        dmz_unreachable(SourceLocation::builtin(), "TODO");
    }
}

void ResolvedGenericTypeDecl::dump(size_t level, [[maybe_unused]] bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGenericTypeDecl " << identifier << '\n';
    // if (specializedType) {
    //     std::cerr << indent(level + 1) << "Specialized type ";
    //     (*specializedType).dump();
    //     std::cerr << '\n';
    // }
}

std::string ResolvedGenericTypeDecl::generic_types_to_str(
    const std::vector<ptr<ResolvedGenericTypeDecl>> &genericTypeDecls) {
    if (genericTypeDecls.size() == 0) return "";
    std::stringstream out;
    out << "(";
    for (size_t i = 0; i < genericTypeDecls.size(); i++) {
        out << genericTypeDecls[i]->identifier;
        if (i != genericTypeDecls.size() - 1) {
            out << ", ";
        }
    }
    out << ")";
    return out.str();
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

void ResolvedTypeExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTypeExpr:" << type->to_str() << "\n";
    if (onlySelf) return;
    dump_constant_value(level);
    resolvedType->dump(level + 1);
    if (typeExpr) typeExpr->dump(level + 1, onlySelf);
}

void ResolvedDeclRefExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeclRefExpr:" << type->to_str() << " " << decl.identifier << '\n';
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedCallExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCallExpr:" << type->to_str() << '\n';

    if (onlySelf) return;
    dump_constant_value(level);
    callee->dump(level + 1, onlySelf);

    for (auto &&arg : arguments) arg->dump(level + 1, onlySelf);
}

void ResolvedComptimeExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedComptimeExpr:" << type->to_str() << '\n';

    if (onlySelf) return;
    dump_constant_value(level);
    expr->dump(level + 1, onlySelf);
}

void ResolvedBlock::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedBlock\n";

    if (onlySelf) return;
    for (auto &&stmt : statements) stmt->dump(level + 1, onlySelf);
    for (auto &&d : defers) d->dump(level + 1, onlySelf);
}

void ResolvedParamDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedParamDecl:";
    if (isComptime) {
        std::cerr << "comptime ";
    }
    if (isVararg) {
        std::cerr << "vararg";
    } else {
        std::cerr << type->to_str();
    }
    std::cerr << " " << identifier << '\n';

    if (onlySelf) return;
}

void ResolvedTypeFunctionExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFunctionSignature:\n";
    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);
    if (resolvedReturnTypeExpr) resolvedReturnTypeExpr->dump(level + 1, onlySelf);
}

void ResolvedExternFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedExternFunctionDecl " << identifier << " " << type->to_str() << '\n';

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);
}

void ResolvedFunctionDecl::dump(size_t level, bool onlySelf) const {
    if (dynamic_cast<const ResolvedBuiltinFunctionDecl *>(this)) {
        std::cerr << indent(level) << "ResolvedBuiltinFunctionDecl ";
    } else if (parentDecl) {
        if (isStatic) {
            std::cerr << indent(level) << "ResolvedStaticMemberFunctionDecl ";
        } else {
            std::cerr << indent(level) << "ResolvedMemberFunctionDecl ";
        }
    } else if (dynamic_cast<const ResolvedTestDecl *>(this)) {
        std::cerr << indent(level) << "ResolvedTestDecl ";
    } else {
        std::cerr << indent(level) << "ResolvedFunctionDecl ";
    }
    std::cerr << identifier << " " << type->to_str() << '\n';
    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);

    if (body) body->dump(level + 1, onlySelf);
}

void ResolvedGenericFunctionDecl::dump(size_t level, bool onlySelf) const {
    if (parentDecl) {
        if (isStatic) {
            std::cerr << indent(level) << "ResolvedGenericStaticMemberFunctionDecl ";
        } else {
            std::cerr << indent(level) << "ResolvedGenericMemberFunctionDecl ";
        }
    } else {
        std::cerr << indent(level) << "ResolvedGenericFunctionDecl ";
    }
    std::cerr << identifier << " " << type->to_str() << '\n';

    if (onlySelf) return;

    for (auto &&genType : genericTypeDecls) genType->dump(level + 1, onlySelf);
    for (auto &&param : params) param->dump(level + 1, onlySelf);
    if (body) body->dump(level + 1, onlySelf);

    for (auto &&func : specializations) {
        func->dump(level + 1, onlySelf);
    }
}

void ResolvedGenericFunctionDecl::dump_dependencies(size_t level, bool dot_format) const {
    ResolvedDecl::dump_dependencies(level, dot_format);
    for (auto &&function : specializations) function->dump_dependencies(level + 1, dot_format);
}

std::string ResolvedGenericFunctionDecl::name() const {
    return ResolvedDecl::name() + ResolvedGenericTypeDecl::generic_types_to_str(genericTypeDecls);
}

void ResolvedSpecializedFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSpecializedFunctionDecl " << identifier << specializedTypes->to_str() << " "
              << type->to_str() << '\n';

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);

    body->dump(level + 1, onlySelf);
}

std::string ResolvedSpecializedFunctionDecl::name() const { return ResolvedDecl::name() + specializedTypes->to_str(); }

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
    std::cerr << indent(level) << (isInline ? "ResolvedInlineIfStmt\n" : "ResolvedIfStmt\n");

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

void ResolvedBreakStmt::dump(size_t level, [[maybe_unused]] bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedBreakStmt\n";
    if (expr) expr->dump(level + 1, onlySelf);
    for (auto &&d : defers) d->dump(level + 1, onlySelf);
}

void ResolvedContinueStmt::dump(size_t level, [[maybe_unused]] bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedContinueStmt\n";
}

void ResolvedCaptureDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCaptureDecl:" + type->to_str() + " " << identifier << "\n";
    if (onlySelf) return;
}

void ResolvedForStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << (isInline ? "ResolvedInlineForStmt\n" : "ResolvedForStmt\n");

    if (onlySelf) return;
    for (auto &&cond : conditions) {
        cond->dump(level + 1, onlySelf);
    }
    for (auto &&cap : captures) {
        cap->dump(level + 1, onlySelf);
    }
    body->dump(level + 1, onlySelf);
}

void ResolvedCaseStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCaseStmt\n";

    if (onlySelf) return;
    for (auto &&condition : conditions) {
        condition->dump(level + 1, onlySelf);
    }
    if (block) block->dump(level + 1, onlySelf);
}

void ResolvedSwitchStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << (isInline ? "ResolvedInlineSwitchStmt\n" : "ResolvedSwitchStmt\n");

    if (onlySelf) return;
    condition->dump(level + 1, onlySelf);

    for (auto &&c : cases) {
        c->dump(level + 1, onlySelf);
    }
    std::cerr << indent(level + 1) << "ElseBlock\n";
    elseBlock->dump(level + 2, onlySelf);
}

void ResolvedVarDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedVarDecl:" << (isMutable ? "" : "const ")
              << (type ? type->to_str() : "nullptr") << " " << identifier << '\n';
    if (onlySelf) return;
    dump_constant_value(level);
    // if (resolvedTypeExpr) resolvedTypeExpr->dump(level + 1, onlySelf);
    if (initializer) initializer->dump(level + 1, onlySelf);
    if (type) {
        if (auto strType = dynamic_cast<ResolvedTypeStructDecl *>(type.get())) {
            strType->dump(level + 1);
        }
    }
}

void ResolvedDeclStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeclStmt " << (type ? type->to_str() : "nullptr") << "\n";
    if (onlySelf) return;
    varDecl->dump(level + 1, onlySelf);
}

void ResolvedDeclStmt::dump_dependencies(size_t level, bool dot_format) const {
    ResolvedDecl::dump_dependencies(level, dot_format);
    if (varDecl->type && varDecl->type->kind == ResolvedTypeKind::StructDecl) {
        if (auto strDecl = dynamic_cast<ResolvedTypeStructDecl *>(varDecl->type.get())) {
            if (strDecl->ownedDecl) {
                strDecl->ownedDecl->dump_dependencies(level + 1, dot_format);
            }
        }
    }
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
    dump_constant_value(level);
    if (default_initializer) default_initializer->dump(level + 1, onlySelf);
}

void ResolvedStructDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStructDecl " << (isPacked ? "packed " : "") << type->to_str() << '\n';

    if (onlySelf) return;
    for (auto &&field : fields) field->dump(level + 1, onlySelf);
    for (auto &&function : functions) function->dump(level + 1, onlySelf);
    for (auto &&decl : otherDecls) decl->dump(level + 1, onlySelf);
}

void ResolvedStructDecl::dump_dependencies(size_t level, bool dot_format) const {
    ResolvedDecl::dump_dependencies(level, dot_format);
    for (auto &&function : functions) function->dump_dependencies(level + 1, dot_format);
}

void ResolvedUnionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedUnionDecl " << (isPacked ? "packed " : "") << type->to_str() << '\n';

    if (onlySelf) return;
    for (auto &&field : fields) field->dump(level + 1, onlySelf);
    for (auto &&function : functions) function->dump(level + 1, onlySelf);
    for (auto &&decl : otherDecls) decl->dump(level + 1, onlySelf);
}

void ResolvedUnionDecl::dump_dependencies(size_t level, bool dot_format) const {
    ResolvedDecl::dump_dependencies(level, dot_format);
    for (auto &&function : functions) function->dump_dependencies(level + 1, dot_format);
}

void ResolvedEnumDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedEnumDecl " << type->to_str() << '\n';

    if (onlySelf) return;
    for (auto &&field : fields) field->dump(level + 1, onlySelf);
    for (auto &&function : functions) function->dump(level + 1, onlySelf);
    for (auto &&decl : otherDecls) decl->dump(level + 1, onlySelf);
}

void ResolvedMemberExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedMemberExpr:" << type->to_str() << " " << member.identifier << '\n';
    if (onlySelf) return;
    dump_constant_value(level);

    base->dump(level + 1, onlySelf);
}

void ResolvedArrayAtExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedArrayAtExpr:" << type->to_str() << '\n';

    if (onlySelf) return;
    dump_constant_value(level);
    array->dump(level + 1, onlySelf);
    index->dump(level + 1, onlySelf);
}

void ResolvedFieldInitStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedFieldInitStmt:" << field.type->to_str() << " " << field.identifier << '\n';

    if (onlySelf) return;
    initializer->dump(level + 1, onlySelf);
}

void ResolvedStructInstantiationExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << (isTuple ? "ResolvedTupleInstantiationExpr:" : "ResolvedStructInstantiationExpr:")
              << type->to_str() << "\n";

    if (onlySelf) return;
    for (auto &&field : fieldInitializers) field->dump(level + 1, onlySelf);
}

void ResolvedUnionInstantiationExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedUnionInstantiationExpr:" << type->to_str() << "\n";

    if (onlySelf) return;
    fieldInitializer->dump(level + 1, onlySelf);
}

void ResolvedArrayInstantiationExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedArrayInstantiationExpr:" << type->to_str() << "\n";

    if (onlySelf) return;
    for (auto &&initializer : initializers) initializer->dump(level + 1, onlySelf);
}

void ResolvedRangeExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedRangeExpr" << "\n";

    if (onlySelf) return;
    if (startExpr) startExpr->dump(level + 1, onlySelf);
    if (endExpr) endExpr->dump(level + 1, onlySelf);
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

void ResolvedErrorInPlaceExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedErrorInPlaceExpr " << identifier << '\n';
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
    if (errorVar) errorVar->dump(level + 1, onlySelf);
    if (handler) handler->dump(level + 1, onlySelf);
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

void ResolvedModuleDecl::dump_dependencies(size_t level, bool dot_format) const {
    ResolvedDecl::dump_dependencies(level, dot_format);
    for (auto *user : isUsedBy) {
        std::cerr << indent_line(level + 1, 0, false) << "Is used by: " << user->identifier << '\n';
    }
    for (auto *used : dependsOn) {
        std::cerr << indent_line(level + 1, 0, false) << "Depends on: " << used->identifier << '\n';
    }
    for (auto &&decl : declarations) decl->dump_dependencies(level + 1, dot_format);
}

void ResolvedImportExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedImportExpr " << moduleDecl.name() << '\n';
    if (onlySelf) return;
}

void ResolvedTestDecl::dump(size_t level, bool onlySelf) const { ResolvedFunctionDecl::dump(level, onlySelf); }

void ResolvedGenericExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGenericExpr:" << type->to_str() << " " << decl.identifier << '\n';
    if (onlySelf) return;
    base->dump(level + 1, onlySelf);
    specializedTypes->dump(level + 1);
}

void ResolvedTypePointerExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTypePointerExpr:" << type->to_str() << '\n';
    if (onlySelf) return;
    pointerType->dump(level + 1, onlySelf);
}

void ResolvedTypeSliceExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTypeSliceExpr:" << type->to_str() << '\n';
    if (onlySelf) return;
    sliceType->dump(level + 1, onlySelf);
}

void ResolvedTypeOptionalExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTypeOptionalExpr:" << type->to_str() << '\n';
    if (onlySelf) return;
    optionalType->dump(level + 1, onlySelf);
}

void ResolvedTypeArrayExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTypeArrayExpr:" << type->to_str() << '\n';
    if (onlySelf) return;
    arrayType->dump(level + 1, onlySelf);
    sizeExpr->dump(level + 1, onlySelf);
}

void ResolvedAutoMemberExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedAutoMemberExpr ." << field << " " << type->to_str() << '\n';
    if (onlySelf) return;
    dump_constant_value(level);
}

}  // namespace DMZ