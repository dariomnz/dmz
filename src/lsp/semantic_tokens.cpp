#include "lsp/semantic_tokens.hpp"

#include <algorithm>

namespace DMZ::lsp {

SemanticTokensCollector::SemanticTokensCollector(const std::string& target_file, const std::string& source)
    : m_target_file(target_file), m_source(source) {}

std::vector<SemanticToken> SemanticTokensCollector::collect(const std::vector<ptr<ResolvedModuleDecl>>& resolvedAST) {
    m_tokens.clear();
    for (const auto& mod : resolvedAST) {
        traverse_module(*mod);
    }

    // Sort tokens by line then by column
    std::sort(m_tokens.begin(), m_tokens.end(), [](const SemanticToken& a, const SemanticToken& b) {
        if (a.line != b.line) return a.line < b.line;
        return a.col < b.col;
    });

    return m_tokens;
}

void SemanticTokensCollector::traverse_module(const ResolvedModuleDecl& module) {
    // if (module.location.file_name == m_target_file) {
    //     add_token(module.location, module.identifier, SemanticTokenType::Namespace,
    //               (uint32_t)SemanticTokenModifier::Declaration);
    // }

    for (const auto& decl : module.declarations) {
        traverse_decl(*decl);
    }
}

void SemanticTokensCollector::traverse_decl(const ResolvedDecl& decl) {
    if (decl.location.file_name == m_target_file) {
        if (auto* structDecl = dynamic_cast<const ResolvedStructDecl*>(&decl)) {
            add_token(structDecl->location, structDecl->identifier, SemanticTokenType::Type,
                      (uint32_t)SemanticTokenModifier::Declaration);
            for (const auto& field : structDecl->fields) {
                traverse_decl(*field);
            }
            for (const auto& func : structDecl->functions) {
                traverse_decl(*func);
            }
        } else if (auto* funcDecl = dynamic_cast<const ResolvedFuncDecl*>(&decl)) {
            if (!dynamic_cast<const ResolvedTestDecl*>(funcDecl)) {
                add_token(funcDecl->location, funcDecl->identifier, SemanticTokenType::Function,
                          (uint32_t)SemanticTokenModifier::Declaration);
                if (auto fnType = funcDecl->getFnType()) {
                    if (fnType->returnType) traverse_type(*fnType->returnType);
                }
                for (const auto& param : funcDecl->params) {
                    traverse_decl(*param);
                }
            }
            if (auto* functionDecl = dynamic_cast<const ResolvedFunctionDecl*>(&decl)) {
                if (functionDecl->body) {
                    traverse_stmt(*functionDecl->body);
                }
            }
        } else if (auto* paramDecl = dynamic_cast<const ResolvedParamDecl*>(&decl)) {
            add_token(paramDecl->location, paramDecl->identifier, SemanticTokenType::Parameter,
                      (uint32_t)SemanticTokenModifier::Declaration);
            if (paramDecl->type) traverse_type(*paramDecl->type);
        } else if (auto* declStmt = dynamic_cast<const ResolvedDeclStmt*>(&decl)) {
            traverse_decl(*declStmt->varDecl);
        } else if (auto* varDecl = dynamic_cast<const ResolvedVarDecl*>(&decl)) {
            add_token(varDecl->location, varDecl->identifier, SemanticTokenType::Variable,
                      (uint32_t)SemanticTokenModifier::Declaration);
            if (varDecl->varDecl->type) traverse_type(*varDecl->type);
            if (varDecl->initializer) {
                traverse_expr(*varDecl->initializer);
            }
        } else if (auto* fieldDecl = dynamic_cast<const ResolvedFieldDecl*>(&decl)) {
            add_token(fieldDecl->location, fieldDecl->identifier, SemanticTokenType::Property,
                      (uint32_t)SemanticTokenModifier::Declaration);
            if (fieldDecl->type) traverse_type(*fieldDecl->type);
            if (fieldDecl->default_initializer) {
                traverse_expr(*fieldDecl->default_initializer);
            }
        } else if (auto* errorGroup = dynamic_cast<const ResolvedErrorGroupExprDecl*>(&decl)) {
            for (const auto& error : errorGroup->errors) {
                add_token(error->location, error->identifier, SemanticTokenType::Variable,
                          (uint32_t)SemanticTokenModifier::Declaration);
            }
        }
    } else {
        // Even if the declaration is in another file, its body/initializer might have references in OUR file?
        // No, declarations are entire units. But wait, what if it's a partially analyzed file?
        // Let's just follow the bodies if they exist.
        if (auto* funcDecl = dynamic_cast<const ResolvedFunctionDecl*>(&decl)) {
            if (funcDecl->body) traverse_stmt(*funcDecl->body);
        }
    }
}

void SemanticTokensCollector::traverse_stmt(const ResolvedStmt& stmt) {
    if (auto* block = dynamic_cast<const ResolvedBlock*>(&stmt)) {
        for (const auto& s : block->statements) {
            if (s) traverse_stmt(*s);
        }
    } else if (auto* ifStmt = dynamic_cast<const ResolvedIfStmt*>(&stmt)) {
        traverse_expr(*ifStmt->condition);
        traverse_stmt(*ifStmt->trueBlock);
        if (ifStmt->falseBlock) traverse_stmt(*ifStmt->falseBlock);
    } else if (auto* whileStmt = dynamic_cast<const ResolvedWhileStmt*>(&stmt)) {
        traverse_expr(*whileStmt->condition);
        traverse_stmt(*whileStmt->body);
    } else if (auto* forStmt = dynamic_cast<const ResolvedForStmt*>(&stmt)) {
        for (const auto& cond : forStmt->conditions) traverse_expr(*cond);
        for (const auto& cap : forStmt->captures) traverse_decl(*cap);
        traverse_stmt(*forStmt->body);
    } else if (auto* returnStmt = dynamic_cast<const ResolvedReturnStmt*>(&stmt)) {
        if (returnStmt->expr) traverse_expr(*returnStmt->expr);
    } else if (auto* switchStmt = dynamic_cast<const ResolvedSwitchStmt*>(&stmt)) {
        traverse_expr(*switchStmt->condition);
        for (const auto& cas : switchStmt->cases) {
            traverse_expr(*cas->condition);
            traverse_stmt(*cas->block);
        }
        if (switchStmt->elseBlock) traverse_stmt(*switchStmt->elseBlock);
    } else if (auto* declStmt = dynamic_cast<const ResolvedDeclStmt*>(&stmt)) {
        traverse_decl(*declStmt->varDecl);
    } else if (auto* assignment = dynamic_cast<const ResolvedAssignment*>(&stmt)) {
        traverse_expr(*assignment->assignee);
        traverse_expr(*assignment->expr);
    } else if (auto* expr = dynamic_cast<const ResolvedExpr*>(&stmt)) {
        traverse_expr(*expr);
    } else if (auto* deferStmt = dynamic_cast<const ResolvedDeferStmt*>(&stmt)) {
        traverse_stmt(*deferStmt->block);
    } else if (auto* deferStmt = dynamic_cast<const ResolvedFieldInitStmt*>(&stmt)) {
        traverse_expr(*deferStmt->initializer);
    }
}

void SemanticTokensCollector::traverse_expr(const ResolvedExpr& expr) {
    if (expr.location.file_name == m_target_file) {
        if (auto* declRef = dynamic_cast<const ResolvedDeclRefExpr*>(&expr)) {
            SemanticTokenType type = SemanticTokenType::Variable;
            if (dynamic_cast<const ResolvedFuncDecl*>(&declRef->decl))
                type = SemanticTokenType::Function;
            else if (dynamic_cast<const ResolvedStructDecl*>(&declRef->decl))
                type = SemanticTokenType::Type;
            else if (dynamic_cast<const ResolvedParamDecl*>(&declRef->decl))
                type = SemanticTokenType::Parameter;
            else if (dynamic_cast<const ResolvedModuleDecl*>(&declRef->decl))
                type = SemanticTokenType::Namespace;

            if (declRef->type->kind == ResolvedTypeKind::Module) type = SemanticTokenType::Namespace;

            add_token(declRef->location, declRef->decl.identifier, type);
        } else if (auto* member = dynamic_cast<const ResolvedMemberExpr*>(&expr)) {
            traverse_expr(*member->base);
            // member is a property/function
            SemanticTokenType type = SemanticTokenType::Property;
            if (member->member.type->kind == ResolvedTypeKind::Function) type = SemanticTokenType::Function;
            if (member->member.type->kind == ResolvedTypeKind::Module) type = SemanticTokenType::Namespace;
            add_token(member->location, member->member.identifier, type);
        } else if (auto* structInit = dynamic_cast<const ResolvedStructInstantiationExpr*>(&expr)) {
            add_token(structInit->location, structInit->structDecl.identifier, SemanticTokenType::Type);
            for (const auto& fieldInit : structInit->fieldInitializers) {
                add_token(fieldInit->location, fieldInit->field.identifier, SemanticTokenType::Property);
                traverse_expr(*fieldInit->initializer);
            }
        }
    }

    // Continue traversal for nested expressions
    if (auto* binary = dynamic_cast<const ResolvedBinaryOperator*>(&expr)) {
        traverse_expr(*binary->lhs);
        traverse_expr(*binary->rhs);
    } else if (auto* unary = dynamic_cast<const ResolvedUnaryOperator*>(&expr)) {
        traverse_expr(*unary->operand);
    } else if (auto* call = dynamic_cast<const ResolvedCallExpr*>(&expr)) {
        traverse_expr(*call->callee);
        for (const auto& arg : call->arguments) traverse_expr(*arg);
    } else if (auto* arrayAt = dynamic_cast<const ResolvedArrayAtExpr*>(&expr)) {
        traverse_expr(*arrayAt->array);
        traverse_expr(*arrayAt->index);
    } else if (auto* grouping = dynamic_cast<const ResolvedGroupingExpr*>(&expr)) {
        traverse_expr(*grouping->expr);
    } else if (dynamic_cast<const ResolvedImportExpr*>(&expr)) {
        return;
    } else if (auto* catchExpr = dynamic_cast<const ResolvedCatchErrorExpr*>(&expr)) {
        traverse_expr(*catchExpr->errorToCatch);
    } else if (auto* errorInPlace = dynamic_cast<const ResolvedErrorInPlaceExpr*>(&expr)) {
        add_token(errorInPlace->location, errorInPlace->identifier, SemanticTokenType::Variable);
    } else if (auto* tryExpr = dynamic_cast<const ResolvedTryErrorExpr*>(&expr)) {
        traverse_expr(*tryExpr->errorToTry);
    } else if (auto* orElseExpr = dynamic_cast<const ResolvedOrElseErrorExpr*>(&expr)) {
        traverse_expr(*orElseExpr->errorToOrElse);
        traverse_expr(*orElseExpr->orElseExpr);
    } else if (auto* refPtr = dynamic_cast<const ResolvedRefPtrExpr*>(&expr)) {
        traverse_expr(*refPtr->expr);
    } else if (auto* derefPtr = dynamic_cast<const ResolvedDerefPtrExpr*>(&expr)) {
        traverse_expr(*derefPtr->expr);
    } else if (auto* errorGroup = dynamic_cast<const ResolvedErrorGroupExprDecl*>(&expr)) {
        traverse_decl(*errorGroup);
    } else if (auto* array = dynamic_cast<const ResolvedArrayInstantiationExpr*>(&expr)) {
        for (auto&& init : array->initializers) {
            traverse_expr(*init);
        }
    }
}

void SemanticTokensCollector::traverse_type(const ResolvedType& type) {
    if (auto* structTy = dynamic_cast<const ResolvedTypeStruct*>(&type)) {
        if (structTy->location.file_name == m_target_file) {
            add_token(structTy->location, structTy->decl->identifier, SemanticTokenType::Type);
        }
    } else if (auto* structDecl = dynamic_cast<const ResolvedTypeStructDecl*>(&type)) {
        if (structDecl->location.file_name == m_target_file) {
            add_token(structDecl->location, structDecl->decl->identifier, SemanticTokenType::Type);
        }
    } else if (auto* genericTy = dynamic_cast<const ResolvedTypeGeneric*>(&type)) {
        if (genericTy->location.file_name == m_target_file) {
            add_token(genericTy->location, genericTy->decl->identifier, SemanticTokenType::Type);
        }
    } else if (dynamic_cast<const ResolvedTypeNumber*>(&type) || dynamic_cast<const ResolvedTypeVoid*>(&type) ||
               dynamic_cast<const ResolvedTypeGeneric*>(&type) || dynamic_cast<const ResolvedTypeBool*>(&type)) {
        if (type.location.file_name == m_target_file) {
            add_token(type.location, type.to_str(), SemanticTokenType::Type);
        }
    } else if (auto* ptrTy = dynamic_cast<const ResolvedTypePointer*>(&type)) {
        traverse_type(*ptrTy->pointerType);
    } else if (auto* sliceTy = dynamic_cast<const ResolvedTypeSlice*>(&type)) {
        traverse_type(*sliceTy->sliceType);
    } else if (auto* arrayTy = dynamic_cast<const ResolvedTypeArray*>(&type)) {
        traverse_type(*arrayTy->arrayType);
    } else if (auto* optTy = dynamic_cast<const ResolvedTypeOptional*>(&type)) {
        traverse_type(*optTy->optionalType);
    } else if (auto* specTy = dynamic_cast<const ResolvedTypeSpecialized*>(&type)) {
        for (const auto& spec : specTy->specializedTypes) traverse_type(*spec);
    } else if (auto* funcTy = dynamic_cast<const ResolvedTypeFunction*>(&type)) {
        for (const auto& param : funcTy->paramsTypes) traverse_type(*param);
        traverse_type(*funcTy->returnType);
    }
}

void SemanticTokensCollector::add_token(const SourceLocation& loc, std::string_view identifier, SemanticTokenType type,
                                        uint32_t modifiers) {
    if (loc.file_name != m_target_file) return;
    if (loc.line == 0 || identifier.empty()) return;

    size_t col = loc.col;
    if (!m_source.empty()) {
        size_t line_start = 0;
        for (size_t i = 1; i < loc.line; ++i) {
            line_start = m_source.find('\n', line_start);
            if (line_start == std::string::npos) break;
            line_start++;
        }
        if (line_start != std::string::npos) {
            size_t end_of_line = m_source.find('\n', line_start);
            if (end_of_line == std::string::npos) end_of_line = m_source.length();

            std::string_view line_str(m_source.data() + line_start, end_of_line - line_start);
            size_t search_start = std::min(loc.col, line_str.length());
            size_t pos = line_str.find(identifier, search_start);
            if (pos == std::string_view::npos && search_start > 0) {
                pos = line_str.find(identifier, 0);
            }

            if (pos != std::string_view::npos) {
                col = pos;
            }
        }
    }

    m_tokens.push_back({loc.line - 1, col, identifier.length(), type, modifiers});
}

}  // namespace DMZ::lsp
