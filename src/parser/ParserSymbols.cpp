#include "parser/ParserSymbols.hpp"

namespace DMZ {

// bool Type::can_convert(const Type &to, const Type &from) {
//     bool canConvert = false;
//     canConvert |= from.kind == Type::Kind::Int && to.kind == Type::Kind::Int;
//     canConvert |= from.kind == Type::Kind::Int && to.kind == Type::Kind::UInt;
//     canConvert |= from.kind == Type::Kind::Int && to.kind == Type::Kind::Float;
//     canConvert |= from.kind == Type::Kind::UInt && to.kind == Type::Kind::UInt;
//     canConvert |= from.kind == Type::Kind::UInt && to.kind == Type::Kind::Int;
//     canConvert |= from.kind == Type::Kind::UInt && to.kind == Type::Kind::Float;
//     canConvert |= from.kind == Type::Kind::Float && to.kind == Type::Kind::Float;
//     canConvert |= from.kind == Type::Kind::Float && to.kind == Type::Kind::Int;
//     canConvert |= from.kind == Type::Kind::Float && to.kind == Type::Kind::UInt;
//     return canConvert;
// }

// bool Type::compare(const Type &lhs, const Type &rhs) {
//     bool equal = false;

// #ifdef DEBUG
//     debug_msg("Types: '" << lhs << "' '" << rhs << "'");
//     defer([&equal]() { debug_msg_func("compare", (equal ? "true" : "false")); });
// #endif
//     bool equalArray = false;
//     bool equalOptional = false;
//     bool equalPointer = false;
//     if (lhs == rhs) {
//         equal = true;
//         return true;
//     }

//     if (lhs.isOptional && rhs.kind == Kind::Error) {
//         equal = true;
//         return equal;
//     }
//     if (rhs.isOptional && lhs.kind == Kind::Error) {
//         equal = true;
//         return equal;
//     }

//     if ((lhs.isPointer && rhs == Type::builtinVoid().pointer()) ||
//         (rhs.isPointer && lhs == Type::builtinVoid().pointer())) {
//         equal = true;
//         return equal;
//     }

//     equalArray |= (lhs.isArray && *lhs.isArray == 0);
//     equalArray |= (rhs.isArray && *rhs.isArray == 0);
//     equalArray |= (lhs.isArray == rhs.isArray);

//     equalOptional |= lhs.isOptional == rhs.isOptional;
//     equalOptional |= lhs.isOptional == true && rhs.isOptional == false;

//     equalPointer |= lhs.isPointer == rhs.isPointer;

//     equal = equalArray && equalOptional && equalPointer;
//     if (equal) {
//         if (can_convert(lhs, rhs) || lhs == rhs) {
//             equal = true;
//             return equal;
//         } else {
//             equal = false;
//         }
//     }
//     equal &= lhs.kind == rhs.kind;
//     if ((lhs.kind == Type::Kind::Struct || lhs.kind == Type::Kind::Custom) &&
//         (rhs.kind == Type::Kind::Struct || rhs.kind == Type::Kind::Custom)) {
//         equal &= lhs.name == rhs.name;
//     }

//     return equal;
// }

// void Type::dump() const { std::cerr << *this; }

// std::string Type::to_str(bool removeKind) const {
//     std::stringstream out;
//     out << *this;
//     auto str = out.str();
//     if (removeKind) {
//         const auto to_rem = Type::KindString(kind).size() + 1;
//         return str.substr(to_rem, str.size() - to_rem);
//     }
//     return str;
// }

// std::ostream &operator<<(std::ostream &os, const Type &t) {
//     os << Type::KindString(t.kind) << " ";
//     if (t.isPointer)
//         for (int i = 0; i < *t.isPointer; i++) os << "*";
//     os << t.name;
//     if (t.genericTypes) {
//         os << (*t.genericTypes);
//     }
//     if (t.isArray) {
//         os << "[";
//         if (*t.isArray != 0) {
//             os << *t.isArray;
//         }
//         os << "]";
//     }
//     if (t.isOptional) os << "!";
//     return os;
// }

// GenericTypes::GenericTypes(std::vector<ptr<Expr>> types) noexcept : types(std::move(types)) {}

// GenericTypes::GenericTypes(const GenericTypes &other) {
//     dmz_unreachable("TODO");
    // types.reserve(other.types.size());
    // for (const auto &ptr : other.types) {
    //     if (ptr) {
    //         types.emplace_back(makePtr<Expr>(*ptr));
    //     } else {
    //         types.emplace_back(nullptr);
    //     }
    // }
// }

// GenericTypes &GenericTypes::operator=(const GenericTypes &other) {
//     dmz_unreachable("TODO");
    // if (this != &other) {
    //     types.clear();
    //     types.reserve(other.types.size());
    //     for (const auto &ptr : other.types) {
    //         if (ptr) {
    //             types.emplace_back(makePtr<Type>(*ptr));
    //         } else {
    //             types.emplace_back(nullptr);
    //         }
    //     }
    // }
    // return *this;
// }

// GenericTypes::GenericTypes(GenericTypes &&other) noexcept : types(std::move(other.types)) {}

// void GenericTypes::dump() const { std::cerr << to_str(); }

// std::string GenericTypes::to_str() const {
//     std::stringstream out;
//     if (types.size() == 0) return "";
//     out << "<";
//     for (size_t i = 0; i < types.size(); i++) {
//         out << types[i]->to_str();
//         if (i != types.size() - 1) out << ", ";
//     }
//     out << ">";
//     return out.str();
// }

void GenericTypeDecl::dump(size_t level) const {
    std::cerr << indent(level) << "GenericTypeDecl " << identifier << '\n';
}

std::string GenericTypeDecl::to_str() const { dmz_unreachable("TODO"); }

void TypeVoid::dump(size_t level) const { std::cerr << indent(level) << "TypeVoid " << to_str() << '\n'; }

std::string TypeVoid::to_str() const { return "void"; }

void TypeNumber::dump(size_t level) const { std::cerr << indent(level) << "TypeNumber " << to_str() << '\n'; }

std::string TypeNumber::to_str() const { return name; }

void TypeBool::dump(size_t level) const { std::cerr << indent(level) << "TypeBool " << to_str() << '\n'; }

std::string TypeBool::to_str() const { return "bool"; }

void FunctionDecl::dump(size_t level) const {
    if (auto member = dynamic_cast<const MemberFunctionDecl *>(this)) {
        if (member->isStatic) {
            std::cerr << indent(level) << "StaticMemberFunctionDecl ";
        } else {
            std::cerr << indent(level) << "MemberFunctionDecl ";
        }
    } else if (dynamic_cast<const TestDecl *>(this)) {
        std::cerr << indent(level) << "TestDecl ";
    } else {
        std::cerr << indent(level) << "FunctionDecl ";
    }
    std::cerr << identifier << " -> " << type->to_str() << "\n";

    for (auto &&param : params) param->dump(level + 1);

    body->dump(level + 1);
}

std::string FunctionDecl::to_str() const { dmz_unreachable("TODO"); }

void GenericFunctionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "GenericFunctionDecl " << identifier << " -> " << type->to_str() << "\n";
    for (auto &&genType : genericTypes) genType->dump(level + 1);

    for (auto &&param : params) param->dump(level + 1);

    body->dump(level + 1);
}

std::string GenericFunctionDecl::to_str() const { dmz_unreachable("TODO"); }

void MemberFunctionDecl::dump(size_t level) const { FunctionDecl::dump(level); }

std::string MemberFunctionDecl::to_str() const { dmz_unreachable("TODO"); }

void MemberGenericFunctionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "MemberGenericFunctionDecl:" << structBase->identifier << "\n";
    GenericFunctionDecl::dump(level + 1);
}

std::string MemberGenericFunctionDecl::to_str() const { dmz_unreachable("TODO"); }

void ExternFunctionDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ExternFunctionDecl " << identifier << " -> " << type->to_str() << "\n";

    for (auto &&param : params) param->dump(level + 1);
}

std::string ExternFunctionDecl::to_str() const { dmz_unreachable("TODO"); }

void Block::dump(size_t level) const {
    std::cerr << indent(level) << "Block\n";
    for (auto &&stmt : statements) stmt->dump(level + 1);
}

std::string Block::to_str() const { dmz_unreachable("TODO"); }

void ReturnStmt::dump(size_t level) const {
    std::cerr << indent(level) << "ReturnStmt\n";

    if (expr) expr->dump(level + 1);
}

std::string ReturnStmt::to_str() const { dmz_unreachable("TODO"); }

void IntLiteral::dump(size_t level) const { std::cerr << indent(level) << "IntLiteral '" << value << "'\n"; }

std::string IntLiteral::to_str() const { return value; }

void FloatLiteral::dump(size_t level) const { std::cerr << indent(level) << "FloatLiteral '" << value << "'\n"; }

std::string FloatLiteral::to_str() const { dmz_unreachable("TODO"); }

void CharLiteral::dump(size_t level) const { std::cerr << indent(level) << "CharLiteral '" << value << "'\n"; }

std::string CharLiteral::to_str() const { dmz_unreachable("TODO"); }

void BoolLiteral::dump(size_t level) const { std::cerr << indent(level) << "BoolLiteral '" << value << "'\n"; }

std::string BoolLiteral::to_str() const { dmz_unreachable("TODO"); }

void StringLiteral::dump(size_t level) const { std::cerr << indent(level) << "StringLiteral '" << value << "'\n"; }

std::string StringLiteral::to_str() const { dmz_unreachable("TODO"); }

void NullLiteral::dump(size_t level) const { std::cerr << indent(level) << "NullLiteral\n"; }

std::string NullLiteral::to_str() const { dmz_unreachable("TODO"); }

void SizeofExpr::dump(size_t level) const { std::cerr << indent(level) << "Sizeof " << sizeofType->to_str() << "\n"; }

std::string SizeofExpr::to_str() const { dmz_unreachable("TODO"); }

void DeclRefExpr::dump(size_t level) const {
    std::cerr << indent(level) << "DeclRefExpr ";
    std::cerr << identifier << '\n';
}

std::string DeclRefExpr::to_str() const { return identifier; }

void CallExpr::dump(size_t level) const {
    std::cerr << indent(level) << "CallExpr" << "\n";

    callee->dump(level + 1);

    for (auto &&arg : arguments) arg->dump(level + 1);
}

std::string CallExpr::to_str() const { dmz_unreachable("TODO"); }

void ParamDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ParamDecl:";
    if (isVararg) {
        std::cerr << "vararg";
    } else {
        std::cerr << type->to_str();
    }
    std::cerr << " " << identifier << '\n';
}

std::string ParamDecl::to_str() const { dmz_unreachable("TODO"); }

void BinaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "BinaryOperator '" << get_op_str(op) << '\'' << '\n';

    lhs->dump(level + 1);
    rhs->dump(level + 1);
}

std::string BinaryOperator::to_str() const { dmz_unreachable("TODO"); }

void UnaryOperator::dump(size_t level) const {
    std::cerr << indent(level) << "UnaryOperator '" << get_op_str(op) << '\'' << '\n';

    operand->dump(level + 1);
}

std::string UnaryOperator::to_str() const { return operand->to_str() + get_op_str(op); }

void RefPtrExpr::dump(size_t level) const {
    std::cerr << indent(level) << "RefPtrExpr" << '\n';

    expr->dump(level + 1);
}

std::string RefPtrExpr::to_str() const { dmz_unreachable("TODO"); }

void DerefPtrExpr::dump(size_t level) const {
    std::cerr << indent(level) << "DerefPtrExpr" << '\n';

    expr->dump(level + 1);
}

std::string DerefPtrExpr::to_str() const { return "*" + expr->to_str(); }

void GroupingExpr::dump(size_t level) const {
    std::cerr << indent(level) << "GroupingExpr\n";

    expr->dump(level + 1);
}

std::string GroupingExpr::to_str() const { dmz_unreachable("TODO"); }

void IfStmt::dump(size_t level) const {
    std::cerr << indent(level) << "IfStmt\n";

    condition->dump(level + 1);
    trueBlock->dump(level + 1);
    if (falseBlock) falseBlock->dump(level + 1);
}

std::string IfStmt::to_str() const { dmz_unreachable("TODO"); }

void WhileStmt::dump(size_t level) const {
    std::cerr << indent(level) << "WhileStmt\n";

    condition->dump(level + 1);
    body->dump(level + 1);
}

std::string WhileStmt::to_str() const { dmz_unreachable("TODO"); }

void CaseStmt::dump(size_t level) const {
    std::cerr << indent(level) << "CaseStmt\n";

    condition->dump(level + 1);
    block->dump(level + 1);
}

std::string CaseStmt::to_str() const { dmz_unreachable("TODO"); }

void SwitchStmt::dump(size_t level) const {
    std::cerr << indent(level) << "SwitchStmt\n";

    condition->dump(level + 1);

    for (auto &&c : cases) {
        c->dump(level + 1);
    }
    std::cerr << indent(level + 1) << "ElseBlock\n";
    elseBlock->dump(level + 1);
}

std::string SwitchStmt::to_str() const { dmz_unreachable("TODO"); }

void VarDecl::dump(size_t level) const {
    std::cerr << indent(level) << "VarDecl:" << (isMutable ? "" : "const ");
    if (type) {
        std::cerr << type->to_str();
    }
    std::cerr << " " << identifier << '\n';

    if (initializer) initializer->dump(level + 1);
}

std::string VarDecl::to_str() const { dmz_unreachable("TODO"); }

void DeclStmt::dump(size_t level) const {
    std::cerr << indent(level) << "DeclStmt\n";
    varDecl->dump(level + 1);
}

std::string DeclStmt::to_str() const { dmz_unreachable("TODO"); }

void Assignment::dump(size_t level) const {
    std::cerr << indent(level) << "Assignment\n";
    assignee->dump(level + 1);
    expr->dump(level + 1);
}

std::string Assignment::to_str() const { dmz_unreachable("TODO"); }

void FieldDecl::dump(size_t level) const {
    std::cerr << indent(level) << "FieldDecl:" << type->to_str() << " " << identifier << '\n';
}

std::string FieldDecl::to_str() const { dmz_unreachable("TODO"); }

void StructDecl::dump(size_t level) const {
    std::cerr << indent(level) << "StructDecl " << (isPacked ? "packed " : "") << identifier << '\n';

    for (auto &&field : fields) field->dump(level + 1);
    for (auto &&function : functions) function->dump(level + 1);
}

std::string StructDecl::to_str() const { dmz_unreachable("TODO"); }

void GenericStructDecl::dump(size_t level) const {
    std::cerr << indent(level) << "GenericStructDecl " << (isPacked ? "packed " : "") << identifier << '\n';
    for (auto &&genType : genericTypes) genType->dump(level + 1);

    for (auto &&field : fields) field->dump(level + 1);
    for (auto &&function : functions) function->dump(level + 1);
}

std::string GenericStructDecl::to_str() const { dmz_unreachable("TODO"); }

void MemberExpr::dump(size_t level) const {
    std::cerr << indent(level) << "MemberExpr ." << field << '\n';

    base->dump(level + 1);
}

std::string MemberExpr::to_str() const { dmz_unreachable("TODO"); }

void GenericExpr::dump(size_t level) const {
    std::cerr << indent(level) << "GenericExpr " << to_str() << '\n';

    base->dump(level + 1);
}

std::string GenericExpr::to_str() const {
    std::stringstream out;
    out << base->to_str();
    out << "<";
    for (size_t i = 0; i < types.size(); i++) {
        out << types[i]->to_str();
        if (i != types.size() - 1) {
            out << ", ";
        }
    }
    out << ">";
    return out.str();
}

void SelfMemberExpr::dump(size_t level) const { std::cerr << indent(level) << "SelfMemberExpr ." << field << '\n'; }

std::string SelfMemberExpr::to_str() const { dmz_unreachable("TODO"); }

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

std::string StructInstantiationExpr::to_str() const { dmz_unreachable("TODO"); }

void ArrayInstantiationExpr::dump(size_t level) const {
    std::cerr << indent(level) << "ArrayInstantiationExpr " << '\n';

    for (auto &&initializer : initializers) initializer->dump(level + 1);
}

std::string ArrayInstantiationExpr::to_str() const { dmz_unreachable("TODO"); }

void FieldInitStmt::dump(size_t level) const {
    std::cerr << indent(level) << "FieldInitStmt " << identifier << '\n';
    initializer->dump(level + 1);
}

std::string FieldInitStmt::to_str() const { dmz_unreachable("TODO"); }

void DeferStmt::dump(size_t level) const {
    std::cerr << indent(level);
    if (isErrDefer) {
        std::cerr << "ErrDeferStmt\n";
    } else {
        std::cerr << "DeferStmt\n";
    }
    block->dump(level + 1);
}

std::string DeferStmt::to_str() const { dmz_unreachable("TODO"); }

void ErrorDecl::dump(size_t level) const { std::cerr << indent(level) << "ErrorDecl " << identifier << '\n'; }

std::string ErrorDecl::to_str() const { dmz_unreachable("TODO"); }

void ErrorGroupExprDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ErrorGroupExprDecl " << '\n';

    for (auto &&err : errs) err->dump(level + 1);
}

std::string ErrorGroupExprDecl::to_str() const { dmz_unreachable("TODO"); }

void CatchErrorExpr::dump(size_t level) const {
    std::cerr << indent(level) << "CatchErrorExpr " << '\n';

    if (errorToCatch) errorToCatch->dump(level + 1);
}

std::string CatchErrorExpr::to_str() const { dmz_unreachable("TODO"); }

void TryErrorExpr::dump(size_t level) const {
    std::cerr << indent(level) << "TryErrorExpr " << '\n';

    if (errorToTry) errorToTry->dump(level + 1);
}

std::string TryErrorExpr::to_str() const { dmz_unreachable("TODO"); }

void OrElseErrorExpr::dump(size_t level) const {
    std::cerr << indent(level) << "OrElseErrorExpr " << '\n';

    if (errorToOrElse) errorToOrElse->dump(level + 1);
    if (orElseExpr) orElseExpr->dump(level + 1);
}

std::string OrElseErrorExpr::to_str() const { dmz_unreachable("TODO"); }

void ModuleDecl::dump(size_t level) const {
    std::cerr << indent(level) << "ModuleDecl " << identifier << '\n';

    for (auto &&decl : declarations) decl->dump(level + 1);
}

std::string ModuleDecl::to_str() const { dmz_unreachable("TODO"); }

void ImportExpr::dump(size_t level) const { std::cerr << indent(level) << "ImportExpr " << identifier << '\n'; }

std::string ImportExpr::to_str() const { dmz_unreachable("TODO"); }

void TestDecl::dump(size_t level) const { FunctionDecl::dump(level); }

std::string TestDecl::to_str() const { dmz_unreachable("TODO"); }
}  // namespace DMZ