#include "lsp/node_finder.hpp"

#include <iostream>

#include "semantic/SemanticSymbols.hpp"
#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ::lsp {

NodeFinder::NodeFinder(const std::string& file, size_t line, size_t col)
    : found_decl(nullptr), m_target_file(file), m_line(line), m_col(col) {}

bool NodeFinder::is_at_location(const SourceLocation& loc, size_t length) const {
    if (loc.file_name != m_target_file) return false;
    if (loc.line != m_line) return false;
    if (m_col < loc.col) return false;
    if (length == 0) return m_col == loc.col;
    return m_col < loc.col + length;
}

void NodeFinder::find_in_module(const ResolvedModuleDecl& mod) {
    for (const auto& decl : mod.declarations) {
        find_in_decl(*decl);
        if (found_decl) return;
    }
}

void NodeFinder::find_in_type(const ResolvedType& type) {
    if (found_decl) return;

    if (const auto* std = dynamic_cast<const ResolvedTypeStructDecl*>(&type)) {
        if (std->decl && is_at_location(std->location, std->decl->identifier.length())) {
            found_decl = std->decl;
            return;
        }
    } else if (const auto* st = dynamic_cast<const ResolvedTypeStruct*>(&type)) {
        if (st->decl && is_at_location(st->location, st->decl->identifier.length())) {
            found_decl = st->decl;
            return;
        }
    } else if (const auto* mdt = dynamic_cast<const ResolvedTypeModule*>(&type)) {
        if (mdt->moduleDecl && is_at_location(mdt->location, mdt->moduleDecl->identifier.length())) {
            found_decl = mdt->moduleDecl;
            return;
        }
    } else if (const auto* ft = dynamic_cast<const ResolvedTypeFunction*>(&type)) {
        for (const auto& pt : ft->paramsTypes) find_in_type(*pt);
        if (ft->returnType) find_in_type(*ft->returnType);
    } else if (const auto* pt = dynamic_cast<const ResolvedTypePointer*>(&type)) {
        find_in_type(*pt->pointerType);
    } else if (const auto* slt = dynamic_cast<const ResolvedTypeSlice*>(&type)) {
        find_in_type(*slt->sliceType);
    } else if (const auto* art = dynamic_cast<const ResolvedTypeArray*>(&type)) {
        find_in_type(*art->arrayType);
    } else if (const auto* opt = dynamic_cast<const ResolvedTypeOptional*>(&type)) {
        find_in_type(*opt->optionalType);
    }
}

void NodeFinder::find_in_decl(const ResolvedDecl& decl) {
    if (found_decl) return;

    if (is_at_location(decl.location, decl.identifier.length())) {
        found_decl = &decl;
        return;
    }

    if (decl.type) {
        find_in_type(*decl.type);
        if (found_decl) return;
    }

    if (const auto* fd = dynamic_cast<const ResolvedFunctionDecl*>(&decl)) {
        for (const auto& param : fd->params) {
            find_in_decl(*param);
            if (found_decl) return;
        }
        if (fd->body) find_in_stmt(*fd->body);
    } else if (const auto* sd = dynamic_cast<const ResolvedStructDecl*>(&decl)) {
        for (const auto& field : sd->fields) {
            find_in_decl(*field);
            if (found_decl) return;
        }
        for (const auto& method : sd->functions) {
            find_in_decl(*method);
            if (found_decl) return;
        }
    } else if (const auto* vd = dynamic_cast<const ResolvedVarDecl*>(&decl)) {
        if (vd->initializer) find_in_expr(*vd->initializer);
    }
}

void NodeFinder::find_in_stmt(const ResolvedStmt& stmt) {
    if (found_decl) return;

    if (const auto* block = dynamic_cast<const ResolvedBlock*>(&stmt)) {
        for (const auto& s : block->statements) {
            find_in_stmt(*s);
            if (found_decl) return;
        }
    } else if (const auto* ds = dynamic_cast<const ResolvedDeclStmt*>(&stmt)) {
        if (ds->varDecl) find_in_decl(*ds->varDecl);
    } else if (const auto* rs = dynamic_cast<const ResolvedReturnStmt*>(&stmt)) {
        if (rs->expr) find_in_expr(*rs->expr);
    } else if (const auto* is = dynamic_cast<const ResolvedIfStmt*>(&stmt)) {
        find_in_expr(*is->condition);
        find_in_stmt(*is->trueBlock);
        if (is->falseBlock) find_in_stmt(*is->falseBlock);
    } else if (const auto* ws = dynamic_cast<const ResolvedWhileStmt*>(&stmt)) {
        find_in_expr(*ws->condition);
        find_in_stmt(*ws->body);
    } else if (const auto* fs = dynamic_cast<const ResolvedForStmt*>(&stmt)) {
        for (const auto& cond : fs->conditions) find_in_expr(*cond);
        for (const auto& capt : fs->captures) find_in_decl(*capt);
        find_in_stmt(*fs->body);
    } else if (const auto* expr = dynamic_cast<const ResolvedExpr*>(&stmt)) {
        find_in_expr(*expr);
    }
}

void NodeFinder::find_in_expr(const ResolvedExpr& expr) {
    if (found_decl) return;

    if (const auto* dr = dynamic_cast<const ResolvedDeclRefExpr*>(&expr)) {
        if (is_at_location(dr->location, dr->decl.identifier.length())) {
            found_decl = &dr->decl;
            return;
        }
    } else if (const auto* me = dynamic_cast<const ResolvedMemberExpr*>(&expr)) {
        if (is_at_location({me->location.file_name, me->location.line, me->location.col},
                           1 + me->member.identifier.length())) {
            if (m_col > me->location.col) {
                found_decl = &me->member;
                return;
            }
        }
        find_in_expr(*me->base);
    } else if (const auto* sie = dynamic_cast<const ResolvedStructInstantiationExpr*>(&expr)) {
        if (is_at_location(sie->location, sie->structDecl.identifier.length())) {
            found_decl = &sie->structDecl;
            return;
        }
        for (const auto& init : sie->fieldInitializers) {
            if (is_at_location(init->location, init->field.identifier.length())) {
                found_decl = &init->field;
                return;
            }
            find_in_expr(*init->initializer);
            if (found_decl) return;
        }
    } else if (const auto* call = dynamic_cast<const ResolvedCallExpr*>(&expr)) {
        find_in_expr(*call->callee);
        if (found_decl) return;
        for (const auto& arg : call->arguments) {
            find_in_expr(*arg);
            if (found_decl) return;
        }
    } else if (const auto* bin = dynamic_cast<const ResolvedBinaryOperator*>(&expr)) {
        find_in_expr(*bin->lhs);
        if (found_decl) return;
        find_in_expr(*bin->rhs);
    } else if (const auto* un = dynamic_cast<const ResolvedUnaryOperator*>(&expr)) {
        find_in_expr(*un->operand);
    } else if (const auto* cast = dynamic_cast<const ResolvedGroupingExpr*>(&expr)) {
        find_in_expr(*cast->expr);
    } else if (const auto* at = dynamic_cast<const ResolvedArrayAtExpr*>(&expr)) {
        find_in_expr(*at->array);
        if (found_decl) return;
        find_in_expr(*at->index);
    }
}

}  // namespace DMZ::lsp
