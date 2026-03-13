// #define DEBUG
#include "lsp/semantic_tokens.hpp"

#include <algorithm>

#include "Debug.hpp"

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
    debug_msg(module.location);
    for (const auto& decl : module.declarations) {
        traverse_decl(*decl);
    }
}

void SemanticTokensCollector::traverse_decl(const ResolvedDecl& decl) {
    debug_msg(decl.location);
    if (decl.location.file_name == m_target_file) {
        if (auto* structDecl = dynamic_cast<const ResolvedStructDecl*>(&decl)) {
            if (structDecl->isTuple) return;
            add_token(structDecl->location, structDecl->identifier, SemanticTokenType::Type,
                      (uint32_t)SemanticTokenModifier::Declaration);
            if (auto* genStru = dynamic_cast<const ResolvedGenericStructDecl*>(structDecl)) {
                for (const auto& gt : genStru->genericTypeDecls) {
                    traverse_decl(*gt);
                }
            }
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
            if (auto* genFn = dynamic_cast<const ResolvedGenericFunctionDecl*>(&decl)) {
                for (const auto& gt : genFn->genericTypeDecls) {
                    traverse_decl(*gt);
                }
            }
        } else if (auto* paramDecl = dynamic_cast<const ResolvedParamDecl*>(&decl)) {
            add_token(paramDecl->location, paramDecl->identifier, SemanticTokenType::Parameter,
                      (uint32_t)SemanticTokenModifier::Declaration);
            if (paramDecl->resolvedTypeExpr) {
                traverse_expr(*paramDecl->resolvedTypeExpr);
            } else if (paramDecl->type)
                traverse_type(*paramDecl->type);
        } else if (auto* declStmt = dynamic_cast<const ResolvedDeclStmt*>(&decl)) {
            traverse_decl(*declStmt->varDecl);
        } else if (auto* varDecl = dynamic_cast<const ResolvedVarDecl*>(&decl)) {
            add_token(varDecl->location, varDecl->identifier, SemanticTokenType::Variable,
                      (uint32_t)SemanticTokenModifier::Declaration);
            if (varDecl->varDecl && varDecl->varDecl->type && varDecl->resolvedTypeExpr)
                traverse_expr(*varDecl->resolvedTypeExpr);
            else if (varDecl->varDecl && varDecl->varDecl->type)
                traverse_type(*varDecl->type);
            if (varDecl->initializer) {
                traverse_expr(*varDecl->initializer);
            }
        } else if (auto* fieldDecl = dynamic_cast<const ResolvedFieldDecl*>(&decl)) {
            add_token(fieldDecl->location, fieldDecl->identifier, SemanticTokenType::Property,
                      (uint32_t)SemanticTokenModifier::Declaration);
            if (fieldDecl->resolvedTypeExpr)
                traverse_expr(*fieldDecl->resolvedTypeExpr);
            else if (fieldDecl->type)
                traverse_type(*fieldDecl->type);
            if (fieldDecl->default_initializer) {
                traverse_expr(*fieldDecl->default_initializer);
            }
        } else if (auto* errorGroup = dynamic_cast<const ResolvedErrorGroupExprDecl*>(&decl)) {
            for (const auto& error : errorGroup->errors) {
                add_token(error->location, error->identifier, SemanticTokenType::Variable,
                          (uint32_t)SemanticTokenModifier::Declaration);
            }
        } else if (auto* genericTypeDecl = dynamic_cast<const ResolvedGenericTypeDecl*>(&decl)) {
            add_token(genericTypeDecl->location, genericTypeDecl->identifier, SemanticTokenType::Type,
                      (uint32_t)SemanticTokenModifier::Declaration);
        } else if (auto* capDecl = dynamic_cast<const ResolvedCaptureDecl*>(&decl)) {
            add_token(capDecl->location, capDecl->identifier, SemanticTokenType::Variable,
                      (uint32_t)SemanticTokenModifier::Declaration);
        }
    }
}

void SemanticTokensCollector::traverse_stmt(const ResolvedStmt& stmt) {
    debug_msg(stmt.location);
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
    debug_msg(expr.location);
    if (expr.location.file_name == m_target_file) {
        if (auto* declRef = dynamic_cast<const ResolvedDeclRefExpr*>(&expr)) {
            debug_msg("ResolvedDeclRefExpr");

            SemanticTokenType type = SemanticTokenType::Variable;
            bool is_this = false;
            if (dynamic_cast<const ResolvedFuncDecl*>(&declRef->decl))
                type = SemanticTokenType::Function;
            else if (dynamic_cast<const ResolvedStructDecl*>(&declRef->decl)) {
                type = SemanticTokenType::Type;
                if (auto* structDeclType = dynamic_cast<const ResolvedTypeStructDecl*>(declRef->type.get())) {
                    is_this = structDeclType->is_this;
                }
                if (auto* structType = dynamic_cast<const ResolvedTypeStruct*>(declRef->type.get())) {
                    is_this = structType->is_this;
                }
            } else if (dynamic_cast<const ResolvedParamDecl*>(&declRef->decl))
                type = SemanticTokenType::Parameter;
            else if (dynamic_cast<const ResolvedModuleDecl*>(&declRef->decl))
                type = SemanticTokenType::Namespace;
            else if (dynamic_cast<const ResolvedGenericTypeDecl*>(&declRef->decl))
                type = SemanticTokenType::Type;
            else if (declRef->type->kind == ResolvedTypeKind::Generic)
                type = SemanticTokenType::Type;

            if (declRef->type->kind == ResolvedTypeKind::Function) type = SemanticTokenType::Function;
            if (declRef->type->kind == ResolvedTypeKind::Module) type = SemanticTokenType::Namespace;

            add_token(declRef->location, is_this ? "@This" : declRef->decl.identifier, type);
        } else if (auto* ge = dynamic_cast<const ResolvedGenericExpr*>(&expr)) {
            debug_msg("ResolvedGenericExpr");
            traverse_expr(*ge->base);
            if (ge->specializedTypes) {
                for (const auto& ty : ge->specializedTypes->specializedTypes) {
                    traverse_type(*ty);
                }
            }
        } else if (auto* memberExpr = dynamic_cast<const ResolvedMemberExpr*>(&expr)) {
            debug_msg("ResolvedMemberExpr");
            if (memberExpr->member.identifier.empty()) return;
            // .member -> starts at location + 1
            SourceLocation memberLoc = memberExpr->location;
            memberLoc.col += 1;
            SemanticTokenType type = SemanticTokenType::Property;
            if (memberExpr->member.type->kind == ResolvedTypeKind::Function) type = SemanticTokenType::Function;
            if (memberExpr->member.type->kind == ResolvedTypeKind::Module) type = SemanticTokenType::Namespace;
            add_token(memberLoc, memberExpr->member.identifier, type);
            traverse_expr(*memberExpr->base);
        } else if (auto* instantiation = dynamic_cast<const ResolvedStructInstantiationExpr*>(&expr)) {
            debug_msg("ResolvedStructInstantiationExpr");
            std::cerr << "ResolvedStructInstantiationExpr: " << instantiation->structDecl.identifier
                      << (instantiation->isTuple ? " tuple" : " struct") << std::endl;
            if (!instantiation->isTuple) {
                bool is_this = false;
                if (auto* structDeclType = dynamic_cast<const ResolvedTypeStructDecl*>(instantiation->type.get())) {
                    is_this = structDeclType->is_this;
                }
                if (auto* structType = dynamic_cast<const ResolvedTypeStruct*>(instantiation->type.get())) {
                    is_this = structType->is_this;
                }

                std::cerr << "color: " << (is_this ? "@This" : instantiation->structDecl.identifier) << std::endl;
                add_token(instantiation->location, is_this ? "@This" : instantiation->structDecl.identifier,
                          SemanticTokenType::Type);
            }
            for (const auto& init : instantiation->fieldInitializers) {
                if (!instantiation->isTuple) {
                    add_token(init->location, init->field.identifier, SemanticTokenType::Property);
                }
                traverse_expr(*init->initializer);
            }
        } else if (auto* arrInstantiation = dynamic_cast<const ResolvedArrayInstantiationExpr*>(&expr)) {
            debug_msg("ResolvedArrayInstantiationExpr");
            for (const auto& init : arrInstantiation->initializers) {
                traverse_expr(*init);
            }
        } else if (auto* call = dynamic_cast<const ResolvedCallExpr*>(&expr)) {
            debug_msg("ResolvedCallExpr");
            traverse_expr(*call->callee);
            for (const auto& arg : call->arguments) {
                traverse_expr(*arg);
            }
        } else if (auto* binary = dynamic_cast<const ResolvedBinaryOperator*>(&expr)) {
            debug_msg("ResolvedBinaryOperator");
            traverse_expr(*binary->lhs);
            traverse_expr(*binary->rhs);
        } else if (auto* unary = dynamic_cast<const ResolvedUnaryOperator*>(&expr)) {
            debug_msg("ResolvedUnaryOperator");
            traverse_expr(*unary->operand);
        } else if (auto* group = dynamic_cast<const ResolvedGroupingExpr*>(&expr)) {
            debug_msg("ResolvedGroupingExpr");
            traverse_expr(*group->expr);
        } else if (auto* arrayAt = dynamic_cast<const ResolvedArrayAtExpr*>(&expr)) {
            debug_msg("ResolvedArrayAtExpr");
            traverse_expr(*arrayAt->array);
            traverse_expr(*arrayAt->index);
        } else if (auto* typeExpr = dynamic_cast<const ResolvedTypeExpr*>(&expr)) {
            debug_msg("ResolvedTypeExpr");
            traverse_type(*typeExpr->resolvedType);
        } else if (auto* pe = dynamic_cast<const ResolvedTypePointerExpr*>(&expr)) {
            debug_msg("ResolvedTypePointerExpr");
            traverse_expr(*pe->pointerType);
        } else if (auto* se = dynamic_cast<const ResolvedTypeSliceExpr*>(&expr)) {
            debug_msg("ResolvedTypeSliceExpr");
            traverse_expr(*se->sliceType);
        } else if (auto* oe = dynamic_cast<const ResolvedTypeOptionalExpr*>(&expr)) {
            debug_msg("ResolvedTypeOptionalExpr");
            traverse_expr(*oe->optionalType);
        } else if (auto* ae = dynamic_cast<const ResolvedTypeArrayExpr*>(&expr)) {
            debug_msg("ResolvedTypeArrayExpr");
            traverse_expr(*ae->arrayType);
            traverse_expr(*ae->sizeExpr);
        } else if (auto* ptrExpr = dynamic_cast<const ResolvedRefPtrExpr*>(&expr)) {
            debug_msg("ResolvedRefPtrExpr");
            traverse_expr(*ptrExpr->expr);
        } else if (auto* ptrExpr = dynamic_cast<const ResolvedDerefPtrExpr*>(&expr)) {
            debug_msg("ResolvedDerefPtrExpr");
            traverse_expr(*ptrExpr->expr);
        } else if (auto* catchErr = dynamic_cast<const ResolvedCatchErrorExpr*>(&expr)) {
            debug_msg("ResolvedCatchErrorExpr");
            traverse_expr(*catchErr->errorToCatch);
        } else if (auto* tryErr = dynamic_cast<const ResolvedTryErrorExpr*>(&expr)) {
            debug_msg("ResolvedTryErrorExpr");
            traverse_expr(*tryErr->errorToTry);
        } else if (auto* orelseErr = dynamic_cast<const ResolvedOrElseErrorExpr*>(&expr)) {
            debug_msg("ResolvedOrElseErrorExpr");
            traverse_expr(*orelseErr->errorToOrElse);
            traverse_expr(*orelseErr->orElseExpr);
        } else if (auto* sizeofExpr = dynamic_cast<const ResolvedSizeofExpr*>(&expr)) {
            debug_msg("ResolvedSizeofExpr");
            traverse_type(*sizeofExpr->sizeofType);
        } else if (auto* typeidExpr = dynamic_cast<const ResolvedTypeidExpr*>(&expr)) {
            debug_msg("ResolvedTypeidExpr");
            traverse_expr(*typeidExpr->typeidExpr);
        } else if (auto* rangeExpr = dynamic_cast<const ResolvedRangeExpr*>(&expr)) {
            debug_msg("ResolvedRangeExpr");
            traverse_expr(*rangeExpr->startExpr);
            traverse_expr(*rangeExpr->endExpr);
        } else if (auto* errorGroupExprDecl = dynamic_cast<const ResolvedErrorGroupExprDecl*>(&expr)) {
            debug_msg("ResolvedErrorGroupExprDecl");
            traverse_decl(*errorGroupExprDecl);
        } else if (dynamic_cast<const ResolvedImportExpr*>(&expr)) {
            debug_msg("ResolvedImportExpr");
        }
    }
}

void SemanticTokensCollector::traverse_type(const ResolvedType& type) {
    debug_msg(type.location << " " << type.to_str());
    if (auto* structTy = dynamic_cast<const ResolvedTypeStruct*>(&type)) {
        if (structTy->location.file_name == m_target_file) {
            add_token(structTy->location, structTy->is_this ? "@This" : structTy->decl->identifier,
                      SemanticTokenType::Type);
        }
        if (auto* specStru = dynamic_cast<const ResolvedSpecializedStructDecl*>(structTy->decl)) {
            if (specStru->specializedTypes) traverse_type(*specStru->specializedTypes);
        }
    } else if (auto* structDecl = dynamic_cast<const ResolvedTypeStructDecl*>(&type)) {
        if (structDecl->location.file_name == m_target_file) {
            add_token(structDecl->location, structDecl->is_this ? "@This" : structDecl->decl->identifier,
                      SemanticTokenType::Type);
        }
        if (auto* specStru = dynamic_cast<const ResolvedSpecializedStructDecl*>(structDecl->decl)) {
            if (specStru->specializedTypes) traverse_type(*specStru->specializedTypes);
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
    debug_msg("add_token: " << type << " " << identifier << " " << loc.line << ":" << col);
    m_tokens.push_back({loc.line - 1, col, identifier.length(), type, modifiers});
}

}  // namespace DMZ::lsp
