#ifdef DEBUG_PARSER
#ifndef DEBUG
#define DEBUG
#endif
#endif

#include "parser/ParserSymbols.hpp"

#include <iostream>

namespace DMZ {

void Decoration::dump([[maybe_unused]] size_t level) const {}

std::string Decoration::to_str() const { dmz_unreachable(location, "TODO"); }

void TypeVoid::dump(size_t level) const { std::cerr << indent(level) << "TypeVoid " << to_str() << '\n'; }

std::string TypeVoid::to_str() const { return "void"; }

void TypeType::dump(size_t level) const { std::cerr << indent(level) << "TypeType " << to_str() << '\n'; }

std::string TypeType::to_str() const { return "type"; }

void TypeNumber::dump(size_t level) const { std::cerr << indent(level) << "TypeNumber " << to_str() << '\n'; }

std::string TypeNumber::to_str() const { return name; }

void TypeBool::dump(size_t level) const { std::cerr << indent(level) << "TypeBool " << to_str() << '\n'; }

std::string TypeBool::to_str() const { return "bool"; }

void TypeError::dump(size_t level) const { std::cerr << indent(level) << "TypeError\n"; }

std::string TypeError::to_str() const { return "err"; }

void TypeAnyType::dump(size_t level) const { std::cerr << indent(level) << "TypeAnyType\n"; }

std::string TypeAnyType::to_str() const { return "anytype"; }

void TypeOptional::dump(size_t level) const { std::cerr << indent(level) << "TypeOptional " << to_str() << '\n'; }

std::string TypeOptional::to_str() const { return "!" + optionalType->to_str(); }

void TypeSlice::dump(size_t level) const { std::cerr << indent(level) << "TypeSlice " << to_str() << '\n'; }

std::string TypeSlice::to_str() const { return "[]" + sliceType->to_str(); }

void TypeArray::dump(size_t level) const { std::cerr << indent(level) << "TypeArray " << to_str() << '\n'; }

std::string TypeArray::to_str() const { return "[" + arraySize->to_str() + "]" + arrayType->to_str(); }

void TypeFunction::dump(size_t level) const { std::cerr << indent(level) << "TypeFunction " << to_str() << '\n'; }

std::string TypeFunction::to_str() const {
    std::stringstream out;
    out << "fn(";
    for (size_t i = 0; i < paramsTypes.size(); i++) {
        out << paramsTypes[i]->to_str();
        if (i != paramsTypes.size() - 1) {
            out << ", ";
        }
    }
    out << ")->";
    out << returnType->to_str();
    return out.str();
}

void TypePointer::dump(size_t level) const { std::cerr << indent(level) << "TypePointer " << to_str() << '\n'; }

std::string TypePointer::to_str() const { return "*" + pointerType->to_str(); }

void FunctionDecl::dump(size_t level) const {
    if (parentDecl) {
        std::cerr << indent(level) << "MemberFunctionDecl ";
    } else if (dynamic_cast<const TestDecl *>(this)) {
        std::cerr << indent(level) << "TestDecl ";
    } else if (isExport) {
        std::cerr << indent(level) << "ExportFunctionDecl ";
    } else {
        std::cerr << indent(level) << "FunctionDecl ";
    }
    std::cerr << identifier << " -> " << type->to_str() << "\n";

    for (auto &&param : params) param->dump(level + 1);

    body->dump(level + 1);
}

std::string FunctionDecl::to_str() const { dmz_unreachable(location, "TODO"); }

void ExternFunctionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ExternFunctionDecl " << identifier << " -> " << type->to_str() << "\n";

    for (auto &&param : params) param->dump(level + 1);
}

std::string ExternFunctionDecl::to_str() const { dmz_unreachable(location, "TODO"); }

void Block::dump(size_t level) const {
    std::cerr << indent(level) << "Block\n";
    for (auto &&stmt : statements) stmt->dump(level + 1);
}

std::string Block::to_str() const { dmz_unreachable(location, "TODO"); }

void ReturnStmt::dump(size_t level) const {
    std::cerr << indent(level) << "ReturnStmt\n";

    if (expr) expr->dump(level + 1);
}

std::string ReturnStmt::to_str() const { dmz_unreachable(location, "TODO"); }

void BreakStmt::dump(size_t level) const { std::cerr << indent(level) << "BreakStmt\n"; }

std::string BreakStmt::to_str() const { return "break"; }

void ContinueStmt::dump(size_t level) const { std::cerr << indent(level) << "ContinueStmt\n"; }

std::string ContinueStmt::to_str() const { return "continue"; }

void IntLiteral::dump(size_t level) const { std::cerr << indent(level) << "IntLiteral '" << value << "'\n"; }

std::string IntLiteral::to_str() const { return value; }

void FloatLiteral::dump(size_t level) const { std::cerr << indent(level) << "FloatLiteral '" << value << "'\n"; }

std::string FloatLiteral::to_str() const { dmz_unreachable(location, "TODO"); }

void CharLiteral::dump(size_t level) const { std::cerr << indent(level) << "CharLiteral '" << value << "'\n"; }

std::string CharLiteral::to_str() const { dmz_unreachable(location, "TODO"); }

void BoolLiteral::dump(size_t level) const { std::cerr << indent(level) << "BoolLiteral '" << value << "'\n"; }

std::string BoolLiteral::to_str() const { dmz_unreachable(location, "TODO"); }

void StringLiteral::dump(size_t level) const { std::cerr << indent(level) << "StringLiteral '" << value << "'\n"; }

std::string StringLiteral::to_str() const { return "\"" + value + "\""; }

void NullLiteral::dump(size_t level) const { std::cerr << indent(level) << "NullLiteral\n"; }

std::string NullLiteral::to_str() const { dmz_unreachable(location, "TODO"); }

void RangeExpr::dump(size_t level) const {
    std::cerr << indent(level) << "RangeExpr\n";
    if (startExpr) startExpr->dump(level + 1);
    if (endExpr) endExpr->dump(level + 1);
}

std::string RangeExpr::to_str() const { dmz_unreachable(location, "TODO"); }

void DeclRefExpr::dump(size_t level) const { std::cerr << indent(level) << "DeclRefExpr " << identifier << '\n'; }

std::string DeclRefExpr::to_str() const { return identifier; }

void CallExpr::dump(size_t level) const {
    std::cerr << indent(level) << "CallExpr" << "\n";

    callee->dump(level + 1);

    for (auto &&arg : arguments) arg->dump(level + 1);
}

std::string CallExpr::to_str() const {
    std::stringstream out;
    out << callee->to_str() << "(";
    for (size_t i = 0; i < arguments.size(); i++) {
        out << arguments[i]->to_str();
        if (i != arguments.size() - 1) {
            out << ", ";
        }
    }
    out << ")";
    return out.str();
}

void ParamDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ParamDecl:";
    if (isComptime) {
        std::cerr << "comptime ";
    }
    if (isVararg) {
        std::cerr << "vararg";
    } else {
        std::cerr << type->to_str();
    }
    std::cerr << " " << identifier << '\n';
}

std::string ParamDecl::to_str() const { dmz_unreachable(location, "TODO"); }

void BinaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "BinaryOperator '" << get_op_str(op) << '\'' << '\n';

    lhs->dump(level + 1);
    rhs->dump(level + 1);
}

std::string BinaryOperator::to_str() const { dmz_unreachable(location, "TODO"); }

void UnaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "UnaryOperator '" << get_op_str(op) << '\'' << '\n';

    operand->dump(level + 1);
}

std::string UnaryOperator::to_str() const { return get_op_str(op) + operand->to_str(); }

void RefPtrExpr::dump(size_t level) const {
    std::cerr << indent(level) << "RefPtrExpr" << '\n';

    expr->dump(level + 1);
}

std::string RefPtrExpr::to_str() const { dmz_unreachable(location, "TODO"); }

void DerefPtrExpr::dump(size_t level) const {
    std::cerr << indent(level) << "DerefPtrExpr" << '\n';

    expr->dump(level + 1);
}

std::string DerefPtrExpr::to_str() const { return "*" + expr->to_str(); }

void GroupingExpr::dump(size_t level) const {
    std::cerr << indent(level) << "GroupingExpr\n";

    expr->dump(level + 1);
}

std::string GroupingExpr::to_str() const { dmz_unreachable(location, "TODO"); }

void IfStmt::dump(size_t level) const {
    std::cerr << indent(level) << (isInline ? "InlineIfStmt\n" : "IfStmt\n");

    condition->dump(level + 1);
    trueBlock->dump(level + 1);
    if (falseBlock) falseBlock->dump(level + 1);
}

std::string IfStmt::to_str() const { dmz_unreachable(location, "TODO"); }

void WhileStmt::dump(size_t level) const {
    std::cerr << indent(level) << "WhileStmt\n";

    condition->dump(level + 1);
    body->dump(level + 1);
}

std::string WhileStmt::to_str() const { dmz_unreachable(location, "TODO"); }

void CaptureDecl::dump(size_t level) const { std::cerr << indent(level) << "CaptureDecl " << identifier << "\n"; }

std::string CaptureDecl::to_str() const { dmz_unreachable(location, "TODO"); }

void ForStmt::dump(size_t level) const {
    std::cerr << indent(level) << (isInline ? "InlineForStmt\n" : "ForStmt\n");

    for (auto &&cond : conditions) {
        cond->dump(level + 1);
    }
    for (auto &&cap : captures) {
        cap->dump(level + 1);
    }
    body->dump(level + 1);
}

std::string ForStmt::to_str() const { dmz_unreachable(location, "TODO"); }

void CaseStmt::dump(size_t level) const {
    std::cerr << indent(level) << "CaseStmt\n";

    for (auto &&cond : conditions) {
        cond->dump(level + 1);
    }
    block->dump(level + 1);
}

std::string CaseStmt::to_str() const { dmz_unreachable(location, "TODO"); }

void SwitchStmt::dump(size_t level) const {
    std::cerr << indent(level) << (isInline ? "InlineSwitchStmt\n" : "SwitchStmt\n");

    condition->dump(level + 1);

    for (auto &&c : cases) {
        c->dump(level + 1);
    }
    std::cerr << indent(level + 1) << "ElseBlock\n";
    elseBlock->dump(level + 1);
}

std::string SwitchStmt::to_str() const { dmz_unreachable(location, "TODO"); }

void VarDecl::dump(size_t level) const {
    std::cerr << indent(level) << "VarDecl:" << (isMutable ? "" : "const ");
    if (type) {
        std::cerr << type->to_str();
    }
    std::cerr << " " << identifier << (isGlobal ? " global" : "") << '\n';

    if (initializer) initializer->dump(level + 1);
}

std::string VarDecl::to_str() const { dmz_unreachable(location, "TODO"); }

void DeclStmt::dump(size_t level) const {
    std::cerr << indent(level) << "DeclStmt\n";
    varDecl->dump(level + 1);
}

std::string DeclStmt::to_str() const { dmz_unreachable(location, "TODO"); }

void Assignment::dump(size_t level) const {
    std::cerr << indent(level) << "Assignment\n";
    assignee->dump(level + 1);
    expr->dump(level + 1);
}

std::string Assignment::to_str() const { dmz_unreachable(location, "TODO"); }

void AssignmentOperator::dump(size_t level) const {
    std::cerr << indent(level) << "AssignmentOperator '" << get_op_str(op) << '\'' << '\n';
    assignee->dump(level + 1);
    expr->dump(level + 1);
}

std::string AssignmentOperator::to_str() const { dmz_unreachable(location, "TODO"); }

void FieldDecl::dump(size_t level) const {
    std::cerr << indent(level) << "FieldDecl:" << (type ? type->to_str() : "") << " " << identifier << '\n';
    // if (type) type->dump(level + 1);
    if (default_initializer) default_initializer->dump(level + 1);
}

std::string FieldDecl::to_str() const { dmz_unreachable(location, "TODO"); }

void StructDecl::dump(size_t level) const {
    std::cerr << indent(level) << "StructDecl " << (isPacked ? "packed " : "") << identifier << '\n';

    for (auto &&decl : decls) decl->dump(level + 1);
}

std::string StructDecl::to_str() const { return (isPacked ? "packed " : "") + identifier; }

void UnionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "UnionDecl " << (isPacked ? "packed " : "") << identifier << '\n';

    for (auto &&decl : decls) decl->dump(level + 1);
}

std::string UnionDecl::to_str() const { return (isPacked ? "packed " : "") + identifier; }

void EnumDecl::dump(size_t level) const {
    std::cerr << indent(level) << "EnumDecl " << identifier << '\n';

    for (auto &&decl : decls) decl->dump(level + 1);
}

std::string EnumDecl::to_str() const { return identifier; }

void MemberExpr::dump(size_t level) const {
    std::cerr << indent(level) << "MemberExpr ." << field << '\n';

    base->dump(level + 1);
}

std::string MemberExpr::to_str() const { return base->to_str() + "." + field; }

void AutoMemberExpr::dump(size_t level) const { std::cerr << indent(level) << "AutoMemberExpr ." << field << '\n'; }

std::string AutoMemberExpr::to_str() const { return "." + field; }

void ArrayAtExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ArrayAtExpr" << '\n';

    array->dump(level + 1);
    index->dump(level + 1);
}

std::string ArrayAtExpr::to_str() const { return array->to_str() + "[" + index->to_str() + "]"; }

void StructInstantiationExpr::dump(size_t level) const {
    std::cerr << indent(level) << "StructInstantiationExpr " << '\n';

    if (base) base->dump(level + 1);
    for (auto &&field : fieldInitializers) field->dump(level + 1);
}

std::string StructInstantiationExpr::to_str() const { dmz_unreachable(location, "TODO"); }

void TupleInstantiationExpr::dump(size_t level) const {
    std::cerr << indent(level) << "TupleInstantiationExpr " << '\n';

    for (auto &&element : elements) element->dump(level + 1);
}

std::string TupleInstantiationExpr::to_str() const { dmz_unreachable(location, "TODO"); }

void ArrayInstantiationExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ArrayInstantiationExpr " << '\n';

    for (auto &&initializer : initializers) initializer->dump(level + 1);
}

std::string ArrayInstantiationExpr::to_str() const { dmz_unreachable(location, "TODO"); }

void ComptimeExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ComptimeExpr\n";
    expr->dump(level + 1);
}

std::string ComptimeExpr::to_str() const { return "comptime " + expr->to_str(); }

void FieldInitStmt::dump(size_t level) const {
    std::cerr << indent(level) << "FieldInitStmt " << identifier << '\n';
    initializer->dump(level + 1);
}

std::string FieldInitStmt::to_str() const { dmz_unreachable(location, "TODO"); }

void DeferStmt::dump(size_t level) const {
    std::cerr << indent(level);
    if (isErrDefer) {
        std::cerr << "ErrDeferStmt\n";
    } else {
        std::cerr << "DeferStmt\n";
    }
    block->dump(level + 1);
}

std::string DeferStmt::to_str() const { dmz_unreachable(location, "TODO"); }

void ErrorDecl::dump(size_t level) const { std::cerr << indent(level) << "ErrorDecl " << identifier << '\n'; }

std::string ErrorDecl::to_str() const { dmz_unreachable(location, "TODO"); }

void ErrorInPlaceExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ErrorInPlaceExpr " << identifier << '\n';
}

std::string ErrorInPlaceExpr::to_str() const { dmz_unreachable(location, "TODO"); }

void ErrorGroupExprDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ErrorGroupExprDecl " << '\n';

    for (auto &&err : errs) err->dump(level + 1);
}

std::string ErrorGroupExprDecl::to_str() const { dmz_unreachable(location, "TODO"); }

void CatchErrorExpr::dump(size_t level) const {
    std::cerr << indent(level) << "CatchErrorExpr " << (captureIdentifier.empty() ? "" : "|") << captureIdentifier
              << (captureIdentifier.empty() ? "" : "|") << '\n';

    if (errorToCatch) errorToCatch->dump(level + 1);
    if (handler) handler->dump(level + 1);
}

std::string CatchErrorExpr::to_str() const {
    return errorToCatch->to_str() + " catch " + (captureIdentifier.empty() ? "" : "|" + captureIdentifier + "| ") +
           handler->to_str();
}

void TryErrorExpr::dump(size_t level) const {
    std::cerr << indent(level) << "TryErrorExpr " << '\n';

    if (errorToTry) errorToTry->dump(level + 1);
}

std::string TryErrorExpr::to_str() const { dmz_unreachable(location, "TODO"); }

void OrElseErrorExpr::dump(size_t level) const {
    std::cerr << indent(level) << "OrElseErrorExpr " << '\n';

    if (errorToOrElse) errorToOrElse->dump(level + 1);
    if (orElseExpr) orElseExpr->dump(level + 1);
}

std::string OrElseErrorExpr::to_str() const { dmz_unreachable(location, "TODO"); }

void ModuleDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ModuleDecl " << identifier << '\n';

    for (auto &&decl : declarations) decl->dump(level + 1);
}

std::string ModuleDecl::to_str() const { dmz_unreachable(location, "TODO"); }

void ImportExpr::dump(size_t level) const { std::cerr << indent(level) << "ImportExpr " << identifier << '\n'; }

std::string ImportExpr::to_str() const { dmz_unreachable(location, "TODO"); }

void TestDecl::dump(size_t level) const { FunctionDecl::dump(level); }

std::string TestDecl::to_str() const { dmz_unreachable(location, "TODO"); }

}  // namespace DMZ
