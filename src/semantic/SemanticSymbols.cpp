#include "semantic/SemanticSymbols.hpp"

#include "Debug.hpp"

namespace DMZ {

void ResolvedExpr::dump_constant_value(size_t level) const {
    if (value) {
        std::cerr << indent(level) << "| value: " << *value << '\n';
    }
}

bool ResolvedDecl::is_needed() {
    if (auto *deps = dynamic_cast<const ResolvedDependencies *>(this)) {
        return deps->isNeeded;
    }
    return true;
}

ResolvedDependencies::~ResolvedDependencies() { clean_dependencies(); }

void ResolvedDependencies::clean_dependencies() {
    debug_msg("Removing all " << dependsOn.size() << " dependsOn of " << name());
    for (auto &&decl : dependsOn) {
        if (!decl->isUsedBy.contains(this)) continue;
        debug_msg("Removing " << name() << " from " << decl->name());
        if (!decl->isUsedBy.erase(this)) {
            debug_msg("Error erasing");
        }
    }
    dependsOn.clear();

    debug_msg("Removing all " << isUsedBy.size() << " isUsedBy of " << name());
    for (auto &&decl : isUsedBy) {
        if (!decl->dependsOn.contains(this)) continue;
        debug_msg("Removing " << name() << " from " << decl->name());
        if (!decl->dependsOn.erase(this)) {
            debug_msg("Error erasing");
        }
    }
    isUsedBy.clear();
}

void ResolvedDependencies::dump_dependencies(size_t level, bool dot_format) const {
    // dump(level, true);
    if (!dot_format) {
        std::cerr << indent_line(level, 0, true) << name() << (isNeeded ? "" : " (not needed)") << '\n';
        if (!isNeeded) return;
        std::cerr << indent_line(level + 1, 1, false) << "Depends on " << dependsOn.size() << ": [ ";
        for (auto &&dep : dependsOn) {
            if (dep->symbolName.empty()) {
                std::cerr << "id'" << dep->identifier << "'" << typeid(*dep).name() << " ";
            } else {
                std::cerr << "'" << dep->name() << "' ";
            }
        }
        std::cerr << "]\n";
        std::cerr << indent_line(level + 1, 1, false) << "Is used by " << isUsedBy.size() << ": [ ";
        for (auto &&dep : isUsedBy) {
            if (dep->symbolName.empty()) {
                std::cerr << "id'" << dep->identifier << "' ";
            } else {
                std::cerr << "'" << dep->name() << "' ";
            }
        }
        std::cerr << "]\n";
    } else {
        if (!isNeeded) return;
        for (auto &&dep : isUsedBy) {
            // std::cerr << '"' << name() << "\" -> \"";
            // if (dep->symbolName.empty()) {
            //     std::cerr << "id'" << dep->identifier << "'";
            // } else {
            //     std::cerr << dep->name();
            // }
            // std::cerr << "\"\n";
            std::cerr << '"';
            if (dep->symbolName.empty()) {
                std::cerr << "id'" << dep->identifier << "'";
            } else {
                std::cerr << dep->name();
            }
            std::cerr << "\" -> \"" << name() << "\"\n";
        }
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
    out << "<";
    for (size_t i = 0; i < genericTypeDecls.size(); i++) {
        out << genericTypeDecls[i]->identifier;
        if (i != genericTypeDecls.size() - 1) {
            out << ", ";
        }
    }
    out << ">";
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

void ResolvedSizeofExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSizeofExpr:" << type->to_str() << "\n";
    std::cerr << indent(level + 1) << sizeofType->to_str() << "\n";
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedTypeidExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTypeidExpr:" << type->to_str() << "\n";
    if (onlySelf) return;
    dump_constant_value(level);
    typeidExpr->dump(level + 1, onlySelf);
}

void ResolvedTypeinfoExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTypeinfoExpr:" << type->to_str() << "\n";
    if (onlySelf) return;
    dump_constant_value(level);
    typeinfoExpr->dump(level + 1, onlySelf);
}

void ResolvedHasMethodExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedHasMethodExpr:" << type->to_str() << " " << methodName << "\n";
    if (onlySelf) return;
    dump_constant_value(level);
    structTypeExpr->dump(level + 1, onlySelf);
}

void ResolvedSimdSizeExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSimdSizeExpr:" << type->to_str() << "\n";
    if (onlySelf) return;
    dump_constant_value(level);
    typeExpr->dump(level + 1, onlySelf);
}

void ResolvedTypeExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTypeExpr:" << type->to_str() << "\n";
    if (onlySelf) return;
    dump_constant_value(level);
    resolvedType->dump(level + 1);
}

void ResolvedDeclRefExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeclRefExpr:" << type->to_str() << " " << decl.identifier << '\n';
    if (onlySelf) return;
    dump_constant_value(level);
}

void ResolvedCallExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedCallExpr:" << type->to_str() << '\n';

    if (onlySelf) return;
    callee->dump(level + 1, onlySelf);
    dump_constant_value(level);

    for (auto &&arg : arguments) arg->dump(level + 1, onlySelf);
}

void ResolvedLambdaExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedLambdaExpr:" << type->to_str() << '\n';
    if (onlySelf) return;
    dump_constant_value(level);
    for (auto &&init : captureInitializers) init->dump(level + 1, onlySelf);
    lambdaFunc->dump(level + 1, onlySelf);
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
        std::cerr << type->to_str();
    }
    std::cerr << " " << identifier << '\n';

    if (onlySelf) return;
}

void ResolvedExternFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedExternFunctionDecl " << identifier << " " << type->to_str() << '\n';

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);
}

void ResolvedFunctionDecl::dump(size_t level, bool onlySelf) const {
    if (auto member = dynamic_cast<const ResolvedMemberFunctionDecl *>(this)) {
        if (member->isStatic) {
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

void ResolvedLambdaFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedLambdaFunctionDecl " << identifier << " " << type->to_str() << '\n';
    if (onlySelf) return;
    std::cerr << indent(level + 1) << "Lambda captures\n";
    for (auto &&cap : captures) cap->dump(level + 2, onlySelf);
    std::cerr << indent(level + 1) << "Lambda params\n";
    for (auto &&param : params) param->dump(level + 2, onlySelf);
    std::cerr << indent(level + 1) << "Lambda body\n";
    if (body) body->dump(level + 2, onlySelf);
}

void ResolvedGenericFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGenericFunctionDecl " << identifier << " " << type->to_str() << '\n';
    for (auto &&genType : genericTypeDecls) genType->dump(level + 1, onlySelf);

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);

    if (body) body->dump(level + 1, onlySelf);

    for (auto &&func : specializations) {
        func->dump(level + 1, onlySelf);
    }
}

void ResolvedGenericFunctionDecl::dump_dependencies(size_t level, bool dot_format) const {
    ResolvedDependencies::dump_dependencies(level, dot_format);
    for (auto &&function : specializations) function->dump_dependencies(level + 1, dot_format);
}

void ResolvedMemberFunctionDecl::dump(size_t level, bool onlySelf) const {
    ResolvedFunctionDecl::dump(level, onlySelf);
    if (onlySelf) return;
}

void ResolvedSpecializedFunctionDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSpecializedFunctionDecl " << identifier << specializedTypes->to_str() << " "
              << type->to_str() << '\n';

    if (onlySelf) return;
    for (auto &&param : params) param->dump(level + 1, onlySelf);

    body->dump(level + 1, onlySelf);
}

std::string ResolvedSpecializedFunctionDecl::name() const {
    std::string base;
    if (symbolName.empty()) {
        base = identifier;
    } else {
        base = symbolName;
    }
    return base + specializedTypes->to_str();
}

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
    if (initializer) initializer->dump(level + 1, onlySelf);
}

void ResolvedDeclStmt::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedDeclStmt " << (type ? type->to_str() : "nullptr") << "\n";
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
    std::cerr << indent(level) << "ResolvedFieldDecl:" << type->to_str() << " " << identifier << '\n';
    if (onlySelf) return;
    if (default_initializer) default_initializer->dump(level + 1, onlySelf);
}

void ResolvedStructDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedStructDecl " << (isPacked ? "packed " : "") << type->to_str() << '\n';

    if (onlySelf) return;
    for (auto &&field : fields) field->dump(level + 1, onlySelf);
    for (auto &&function : functions) function->dump(level + 1, onlySelf);
}

void ResolvedStructDecl::dump_dependencies(size_t level, bool dot_format) const {
    ResolvedDependencies::dump_dependencies(level, dot_format);
    for (auto &&function : functions) function->dump_dependencies(level + 1, dot_format);
}

void ResolvedGenericStructDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedGenericStructDecl " << (isPacked ? "packed " : "") << type->to_str() << '\n';
    for (auto &&genType : genericTypeDecls) genType->dump(level + 1, onlySelf);

    if (onlySelf) return;
    for (auto &&field : fields) field->dump(level + 1, onlySelf);
    for (auto &&function : functions) function->dump(level + 1, onlySelf);
    for (auto &&spec : specializations) spec->dump(level + 1, onlySelf);
}

void ResolvedGenericStructDecl::dump_dependencies(size_t level, bool dot_format) const {
    ResolvedDependencies::dump_dependencies(level, dot_format);
    for (auto &&function : functions) function->dump_dependencies(level + 1, dot_format);
    for (auto &&spec : specializations) spec->dump_dependencies(level + 1, dot_format);
}

void ResolvedSpecializedStructDecl::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedSpecializedStructDecl " << (isPacked ? "packed " : "") << type->to_str()
              << '\n';

    if (onlySelf) return;
    for (auto &&field : fields) field->dump(level + 1, onlySelf);
    for (auto &&function : functions) function->dump(level + 1, onlySelf);
}

std::string ResolvedSpecializedStructDecl::name() const {
    std::string base;
    if (symbolName.empty()) {
        base = identifier;
    } else {
        base = symbolName;
    }
    return base + specializedTypes->to_str();
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
    ResolvedDependencies::dump_dependencies(level, dot_format);
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

void ResolvedTypeSimdExpr::dump(size_t level, bool onlySelf) const {
    std::cerr << indent(level) << "ResolvedTypeSimdExpr:" << type->to_str() << '\n';
    if (onlySelf) return;
    simdType->dump(level + 1, onlySelf);
    sizeExpr->dump(level + 1, onlySelf);
}

}  // namespace DMZ