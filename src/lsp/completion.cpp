#include "lsp/completion.hpp"

#include "lsp/protocol.hpp"
#include "lsp/server.hpp"

namespace DMZ::lsp {

void LSPServer::collect_member_completions(const ResolvedStructDecl* decl, std::stringstream& items, bool& has_items) {
    if (!decl) return;
    for (const auto& field : decl->fields) {
        if (has_items) items << ",";
        items << "{\"label\":\"" << escape_json(field->identifier) << "\",\"kind\":5,\"detail\":\""
              << escape_json(field->type->to_str()) << "\"}";
        has_items = true;
    }
    for (const auto& method : decl->functions) {
        if (has_items) items << ",";
        items << "{\"label\":\"" << escape_json(method->identifier) << "\",\"kind\":2,\"detail\":\""
              << escape_json(method->type->to_str()) << "\"}";
        has_items = true;
    }
}

void LSPServer::collect_module_completions(const ResolvedModuleDecl* decl, std::stringstream& items, bool& has_items) {
    if (!decl) return;
    for (const auto& d : decl->declarations) {
        if (!d->isPublic) continue;
        if (dynamic_cast<const ResolvedTestDecl*>(d.get())) continue;
        if (has_items) items << ",";
        CompletionItemKind kind = CompletionItemKind::Text;
        if (dynamic_cast<const ResolvedFunctionDecl*>(d.get()))
            kind = CompletionItemKind::Function;
        else if (dynamic_cast<const ResolvedStructDecl*>(d.get()) ||
                 dynamic_cast<const ResolvedTypeStructDecl*>(d->type.get()))
            kind = CompletionItemKind::Struct;
        else if (dynamic_cast<const ResolvedModuleDecl*>(d.get()) ||
                 dynamic_cast<const ResolvedTypeModule*>(d->type.get()))
            kind = CompletionItemKind::Module;
        else if (dynamic_cast<const ResolvedDeclStmt*>(d.get()) || dynamic_cast<const ResolvedVarDecl*>(d.get()))
            kind = CompletionItemKind::Variable;
        else if (dynamic_cast<const ResolvedGenericTypeDecl*>(d.get()))
            kind = CompletionItemKind::TypeParameter;

        items << "{\"label\":\"" << escape_json(d->identifier) << "\",\"kind\":" << static_cast<int>(kind)
              << ",\"detail\":\"" << escape_json(d->type != nullptr ? d->type->to_str() : "") << "\"}";
        has_items = true;
    }
}

void LSPServer::collect_completions_from_type(const ResolvedType* type, std::stringstream& items, bool& has_items) {
    if (!type) return;
    while (auto pt = dynamic_cast<const ResolvedTypePointer*>(type)) {
        type = pt->pointerType.get();
    }
    if (auto st = dynamic_cast<const ResolvedTypeStruct*>(type)) {
        collect_member_completions(st->decl, items, has_items);
    } else if (auto st_decl = dynamic_cast<const ResolvedTypeStructDecl*>(type)) {
        collect_member_completions(st_decl->decl, items, has_items);
    } else if (auto mt = dynamic_cast<const ResolvedTypeModule*>(type)) {
        collect_module_completions(mt->moduleDecl, items, has_items);
    }
}

// Walk the resolved AST to find ResolvedMemberExpr with empty member identifier on the target line.
// Returns the resolved type of the base expression, or nullptr if not found.
const ResolvedType* LSPServer::find_incomplete_member_base_type(const ResolvedModuleDecl* mainModule,
                                                                const std::string& file, size_t line) {
    // Walk all expressions recursively looking for an incomplete MemberExpr on the target line
    struct IncompleteMemberFinder {
        const std::string& target_file;
        size_t target_line;
        const ResolvedType* result = nullptr;

        void visit_module(const ResolvedModuleDecl& mod) {
            for (const auto& decl : mod.declarations) visit_decl(*decl);
        }

        void visit_decl(const ResolvedDecl& decl) {
            if (result) return;
            if (const auto* fd = dynamic_cast<const ResolvedFunctionDecl*>(&decl)) {
                for (const auto& param : fd->params) visit_decl(*param);
                if (fd->body) visit_stmt(*fd->body);
            } else if (const auto* sd = dynamic_cast<const ResolvedStructDecl*>(&decl)) {
                for (const auto& method : sd->functions) visit_decl(*method);
            } else if (const auto* vd = dynamic_cast<const ResolvedVarDecl*>(&decl)) {
                if (vd->initializer) visit_expr(*vd->initializer);
            }
        }

        void visit_stmt(const ResolvedStmt& stmt) {
            if (result) return;
            if (const auto* block = dynamic_cast<const ResolvedBlock*>(&stmt)) {
                for (const auto& s : block->statements) visit_stmt(*s);
            } else if (const auto* ds = dynamic_cast<const ResolvedDeclStmt*>(&stmt)) {
                if (ds->varDecl) visit_decl(*ds->varDecl);
            } else if (const auto* rs = dynamic_cast<const ResolvedReturnStmt*>(&stmt)) {
                if (rs->expr) visit_expr(*rs->expr);
            } else if (const auto* is = dynamic_cast<const ResolvedIfStmt*>(&stmt)) {
                visit_expr(*is->condition);
                visit_stmt(*is->trueBlock);
                if (is->falseBlock) visit_stmt(*is->falseBlock);
            } else if (const auto* ws = dynamic_cast<const ResolvedWhileStmt*>(&stmt)) {
                visit_expr(*ws->condition);
                visit_stmt(*ws->body);
            } else if (const auto* fs = dynamic_cast<const ResolvedForStmt*>(&stmt)) {
                for (const auto& cond : fs->conditions) visit_expr(*cond);
                visit_stmt(*fs->body);
            } else if (const auto* as = dynamic_cast<const ResolvedAssignment*>(&stmt)) {
                visit_expr(*as->assignee);
                visit_expr(*as->expr);
            } else if (const auto* def = dynamic_cast<const ResolvedDeferStmt*>(&stmt)) {
                visit_stmt(*def->block);
            } else if (const auto* switchStmt = dynamic_cast<const ResolvedSwitchStmt*>(&stmt)) {
                visit_expr(*switchStmt->condition);
                for (const auto& caseStmt : switchStmt->cases) {
                    visit_stmt(*caseStmt);
                }
                if (switchStmt->elseBlock) visit_stmt(*switchStmt->elseBlock);
            } else if (const auto* caseStmt = dynamic_cast<const ResolvedCaseStmt*>(&stmt)) {
                for (const auto& cond : caseStmt->conditions) {
                    visit_expr(*cond);
                    if (result) return;
                }
                visit_stmt(*caseStmt->block);
            } else if (const auto* expr = dynamic_cast<const ResolvedExpr*>(&stmt)) {
                visit_expr(*expr);
            }
        }

        void visit_expr(const ResolvedExpr& expr) {
            if (result) return;
            if (const auto* me = dynamic_cast<const ResolvedMemberExpr*>(&expr)) {
                if (me->member.identifier.empty() && me->location.file_name == target_file &&
                    me->location.line == target_line) {
                    result = me->base->type.get();
                    return;
                }
                visit_expr(*me->base);
            } else if (const auto* call = dynamic_cast<const ResolvedCallExpr*>(&expr)) {
                visit_expr(*call->callee);
                for (const auto& arg : call->arguments) visit_expr(*arg);
            } else if (const auto* bin = dynamic_cast<const ResolvedBinaryOperator*>(&expr)) {
                visit_expr(*bin->lhs);
                visit_expr(*bin->rhs);
            } else if (const auto* un = dynamic_cast<const ResolvedUnaryOperator*>(&expr)) {
                visit_expr(*un->operand);
            } else if (const auto* grp = dynamic_cast<const ResolvedGroupingExpr*>(&expr)) {
                visit_expr(*grp->expr);
            } else if (const auto* at = dynamic_cast<const ResolvedArrayAtExpr*>(&expr)) {
                visit_expr(*at->array);
                visit_expr(*at->index);
            }
        }
    };

    IncompleteMemberFinder finder{file, line};
    if (mainModule) {
        finder.visit_module(*mainModule);
    }
    return finder.result;
}

// Walk the resolved AST to find the innermost scope that contains the cursor position.
const ResolvedScope* LSPServer::find_scope_at_position(const ResolvedModuleDecl* mainModule, const std::string& file,
                                                       size_t line, size_t col) {
    if (!mainModule) return nullptr;

    struct ScopeFinder {
        const std::string& target_file;
        size_t target_line;
        size_t target_col;
        const ResolvedScope* best = nullptr;
        size_t best_start_line = 0;

        // Try to adopt scope if it's at a more specific (deeper) position
        void consider(const ResolvedScope* scope, size_t start_line) {
            if (!scope) return;
            if (start_line <= target_line && start_line >= best_start_line) {
                best = scope;
                best_start_line = start_line;
            }
        }

        void visit_module(const ResolvedModuleDecl& mod) {
            consider(mod.scope.get(), mod.location.line);
            for (const auto& decl : mod.declarations) visit_decl(*decl);
        }

        void visit_decl(const ResolvedDecl& decl) {
            if (const auto* fd = dynamic_cast<const ResolvedFuncDecl*>(&decl)) {
                if (fd->location.file_name != target_file) return;
                consider(fd->scope.get(), fd->location.line);
                if (const auto* fn = dynamic_cast<const ResolvedFunctionDecl*>(fd)) {
                    if (fn->body) visit_block(*fn->body);
                }
            } else if (const auto* sd = dynamic_cast<const ResolvedStructDecl*>(&decl)) {
                if (sd->location.file_name != target_file) return;
                consider(sd->scope.get(), sd->location.line);
                for (const auto& method : sd->functions) visit_decl(*method);
            } else if (const auto* vd = dynamic_cast<const ResolvedVarDecl*>(&decl)) {
                if (vd->location.file_name != target_file) return;
                consider(vd->scope.get(), vd->location.line);
            }
        }

        void visit_block(const ResolvedBlock& block) {
            if (block.location.file_name != target_file) return;
            consider(block.scope.get(), block.location.line);
            for (const auto& stmt : block.statements) visit_stmt(*stmt);
        }

        void visit_stmt(const ResolvedStmt& stmt) {
            if (const auto* block = dynamic_cast<const ResolvedBlock*>(&stmt)) {
                visit_block(*block);
            } else if (const auto* ifStmt = dynamic_cast<const ResolvedIfStmt*>(&stmt)) {
                if (ifStmt->trueBlock) visit_block(*ifStmt->trueBlock);
                if (ifStmt->falseBlock) visit_block(*ifStmt->falseBlock);
            } else if (const auto* ws = dynamic_cast<const ResolvedWhileStmt*>(&stmt)) {
                if (ws->body) visit_block(*ws->body);
            } else if (const auto* fs = dynamic_cast<const ResolvedForStmt*>(&stmt)) {
                consider(fs->scope.get(), fs->location.file_name == target_file ? fs->location.line : 0);
                if (fs->body) visit_block(*fs->body);
            } else if (const auto* ds = dynamic_cast<const ResolvedDeclStmt*>(&stmt)) {
                if (ds->varDecl) visit_decl(*ds->varDecl);
            } else if (const auto* sw = dynamic_cast<const ResolvedSwitchStmt*>(&stmt)) {
                for (const auto& c : sw->cases)
                    if (c->block) visit_block(*c->block);
                if (sw->elseBlock) visit_block(*sw->elseBlock);
            } else if (const auto* ce = dynamic_cast<const ResolvedCatchErrorExpr*>(&stmt)) {
                consider(ce->scope.get(), ce->location.file_name == target_file ? ce->location.line : 0);
                if (ce->handler) visit_stmt(*ce->handler);
            }
        }
    };

    ScopeFinder finder{file, line, col};
    finder.visit_module(*mainModule);
    return finder.best;
}

// Collect all visible symbols from a scope chain into completion items.
void LSPServer::collect_scope_completions(const ResolvedScope* scope, size_t cursor_line, std::stringstream& items,
                                          bool& has_items) {
    std::unordered_set<std::string> seen;
    const ResolvedScope* s = scope;
    while (s) {
        for (const auto& [name, decl] : s->table) {
            if (!seen.insert(name).second) continue;  // Already emitted from inner scope
            if (!decl) continue;
            // Skip internal generated names (starting with $)
            if (!name.empty() && name[0] == '$') continue;

            if (decl->location.line > cursor_line) continue;

            CompletionItemKind kind = CompletionItemKind::Variable;  // Variable default
            if (dynamic_cast<const ResolvedFunctionDecl*>(decl) ||
                dynamic_cast<const ResolvedExternFunctionDecl*>(decl))
                kind = CompletionItemKind::Function;
            else if (dynamic_cast<const ResolvedStructDecl*>(decl))
                kind = CompletionItemKind::Struct;
            else if (dynamic_cast<const ResolvedModuleDecl*>(decl))
                kind = CompletionItemKind::Module;
            else if (dynamic_cast<const ResolvedParamDecl*>(decl))
                kind = CompletionItemKind::Variable;
            else if (dynamic_cast<const ResolvedGenericTypeDecl*>(decl))
                kind = CompletionItemKind::TypeParameter;

            if (has_items) items << ",";
            items << "{\"label\":\"" << escape_json(name) << "\",\"kind\":" << static_cast<int>(kind);
            if (decl->type) {
                items << ",\"detail\":\"" << escape_json(decl->type->to_str()) << "\"";
            }
            items << "}";
            has_items = true;
        }
        s = s->parent;
    }
}

}  // namespace DMZ::lsp