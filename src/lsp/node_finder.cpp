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
    return m_col <= loc.col + length;
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
        if (std->decl &&
            is_at_location(std->location, (std->is_this ? std::string("@This") : std->decl->identifier).length())) {
            found_decl = std->decl;
            return;
        }
        if (auto* specStru = dynamic_cast<const ResolvedSpecializedStructDecl*>(std->decl)) {
            if (specStru->specializedTypes) find_in_type(*specStru->specializedTypes);
        }
    } else if (const auto* st = dynamic_cast<const ResolvedTypeStruct*>(&type)) {
        if (st->decl &&
            is_at_location(st->location, (st->is_this ? std::string("@This") : st->decl->identifier).length())) {
            found_decl = st->decl;
            return;
        }
        if (auto* specStru = dynamic_cast<const ResolvedSpecializedStructDecl*>(st->decl)) {
            if (specStru->specializedTypes) find_in_type(*specStru->specializedTypes);
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
    } else if (const auto* errg = dynamic_cast<const ResolvedTypeErrorGroup*>(&type)) {
        if (errg->decl && is_at_location(errg->location, errg->decl->identifier.length())) {
            found_decl = errg->decl;
            return;
        }
    } else if (const auto* spect = dynamic_cast<const ResolvedTypeSpecialized*>(&type)) {
        for (const auto& ty : spect->specializedTypes) {
            find_in_type(*ty);
            if (found_decl) return;
        }
    }
}

void NodeFinder::find_in_decl(const ResolvedDecl& decl) {
    if (found_decl) return;

    if (is_at_location(decl.location, decl.identifier.length())) {
        found_decl = &decl;
        return;
    }

    if (decl.type) {
        if (const auto* var = dynamic_cast<const ResolvedVarDecl*>(&decl)) {
            if (var->resolvedTypeExpr) find_in_expr(*var->resolvedTypeExpr);
        } else if (const auto* param = dynamic_cast<const ResolvedParamDecl*>(&decl)) {
            if (param->resolvedTypeExpr) find_in_expr(*param->resolvedTypeExpr);
        } else if (const auto* field = dynamic_cast<const ResolvedFieldDecl*>(&decl)) {
            if (field->resolvedTypeExpr) find_in_expr(*field->resolvedTypeExpr);
        }

        if (found_decl) return;

        find_in_type(*decl.type);
        if (found_decl) return;
    }

    if (const auto* fd = dynamic_cast<const ResolvedFunctionDecl*>(&decl)) {
        if (const auto* genFn = dynamic_cast<const ResolvedGenericFunctionDecl*>(fd)) {
            for (const auto& gt : genFn->genericTypeDecls) {
                find_in_decl(*gt);
                if (found_decl) return;
            }
        }
        for (const auto& param : fd->params) {
            find_in_decl(*param);
            if (found_decl) return;
        }
        if (fd->body) find_in_stmt(*fd->body);
    } else if (const auto* sd = dynamic_cast<const ResolvedStructDecl*>(&decl)) {
        if (sd->isTuple) return;
        if (const auto* genStru = dynamic_cast<const ResolvedGenericStructDecl*>(sd)) {
            for (const auto& gt : genStru->genericTypeDecls) {
                find_in_decl(*gt);
                if (found_decl) return;
            }
        }
        for (const auto& field : sd->fields) {
            find_in_decl(*field);
            if (found_decl) return;
        }
        for (const auto& method : sd->functions) {
            find_in_decl(*method);
            if (found_decl) return;
        }
    } else if (const auto* ds = dynamic_cast<const ResolvedDeclStmt*>(&decl)) {
        if (ds->varDecl) find_in_decl(*ds->varDecl);
    } else if (const auto* var = dynamic_cast<const ResolvedVarDecl*>(&decl)) {
        if (var->initializer) find_in_expr(*var->initializer);
        if (var->type) find_in_type(*var->type);
    } else if (const auto* param = dynamic_cast<const ResolvedParamDecl*>(&decl)) {
        if (param->type) find_in_type(*param->type);
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
    } else if (const auto* as = dynamic_cast<const ResolvedAssignment*>(&stmt)) {
        find_in_expr(*as->assignee);
        if (found_decl) return;
        find_in_expr(*as->expr);
    } else if (const auto* expr = dynamic_cast<const ResolvedExpr*>(&stmt)) {
        find_in_expr(*expr);
    } else if (const auto* def = dynamic_cast<const ResolvedDeferStmt*>(&stmt)) {
        find_in_stmt(*def->block);
    } else if (const auto* switchStmt = dynamic_cast<const ResolvedSwitchStmt*>(&stmt)) {
        find_in_expr(*switchStmt->condition);
        for (const auto& caseStmt : switchStmt->cases) {
            find_in_stmt(*caseStmt);
        }
        if (switchStmt->elseBlock) find_in_stmt(*switchStmt->elseBlock);
    } else if (const auto* caseStmt = dynamic_cast<const ResolvedCaseStmt*>(&stmt)) {
        find_in_expr(*caseStmt->condition);
        find_in_stmt(*caseStmt->block);
    }
}

void NodeFinder::find_in_expr(const ResolvedExpr& expr) {
    if (found_decl) return;

    if (const auto* dr = dynamic_cast<const ResolvedDeclRefExpr*>(&expr)) {
        if (is_at_location(dr->location, dr->decl.identifier.length())) {
            found_decl = &dr->decl;
            return;
        }
    } else if (const auto* ge = dynamic_cast<const ResolvedGenericExpr*>(&expr)) {
        if (ge->specializedTypes) {
            for (const auto& ty : ge->specializedTypes->specializedTypes) {
                find_in_type(*ty);
                if (found_decl) return;
            }
        }
        find_in_expr(*ge->base);
    } else if (const auto* me = dynamic_cast<const ResolvedMemberExpr*>(&expr)) {
        // me->location is the dot. Its length is 1 + identifier length.
        if (is_at_location(me->location, 1 + me->member.identifier.length())) {
            found_decl = &me->member;
            return;
        }
        find_in_expr(*me->base);
    } else if (const auto* sie = dynamic_cast<const ResolvedStructInstantiationExpr*>(&expr)) {
        if (!sie->isTuple && is_at_location(sie->location, sie->structDecl.identifier.length())) {
            found_decl = &sie->structDecl;
            return;
        }
        for (const auto& init : sie->fieldInitializers) {
            if (!sie->isTuple && is_at_location(init->location, init->field.identifier.length())) {
                found_decl = &init->field;
                return;
            }
            find_in_expr(*init->initializer);
            if (found_decl) return;
        }
    } else if (const auto* re = dynamic_cast<const ResolvedArrayInstantiationExpr*>(&expr)) {
        for (const auto& init : re->initializers) {
            find_in_expr(*init);
            if (found_decl) return;
        }
    } else if (const auto* te = dynamic_cast<const ResolvedTypeExpr*>(&expr)) {
        find_in_type(*te->resolvedType);
    } else if (const auto* pe = dynamic_cast<const ResolvedTypePointerExpr*>(&expr)) {
        find_in_expr(*pe->pointerType);
    } else if (const auto* se = dynamic_cast<const ResolvedTypeSliceExpr*>(&expr)) {
        find_in_expr(*se->sliceType);
    } else if (const auto* oe = dynamic_cast<const ResolvedTypeOptionalExpr*>(&expr)) {
        find_in_expr(*oe->optionalType);
    } else if (const auto* ae = dynamic_cast<const ResolvedTypeArrayExpr*>(&expr)) {
        find_in_expr(*ae->arrayType);
        if (found_decl) return;
        find_in_expr(*ae->sizeExpr);
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
    } else if (auto* ptrExpr = dynamic_cast<const ResolvedRefPtrExpr*>(&expr)) {
        find_in_expr(*ptrExpr->expr);
    } else if (auto* ptrExpr = dynamic_cast<const ResolvedDerefPtrExpr*>(&expr)) {
        find_in_expr(*ptrExpr->expr);
    } else if (dynamic_cast<const ResolvedErrorInPlaceExpr*>(&expr)) {
        return;
    } else if (auto* catchErr = dynamic_cast<const ResolvedCatchErrorExpr*>(&expr)) {
        find_in_expr(*catchErr->errorToCatch);
    } else if (auto* tryErr = dynamic_cast<const ResolvedTryErrorExpr*>(&expr)) {
        find_in_expr(*tryErr->errorToTry);
    } else if (auto* orelseErr = dynamic_cast<const ResolvedOrElseErrorExpr*>(&expr)) {
        find_in_expr(*orelseErr->errorToOrElse);
        if (found_decl) return;
        find_in_expr(*orelseErr->orElseExpr);
    } else if (auto* sizeofExpr = dynamic_cast<const ResolvedSizeofExpr*>(&expr)) {
        find_in_type(*sizeofExpr->type);
    } else if (auto* typeofExpr = dynamic_cast<const ResolvedTypeofExpr*>(&expr)) {
        find_in_expr(*typeofExpr->typeofExpr);
    } else if (auto* rangeExpr = dynamic_cast<const ResolvedRangeExpr*>(&expr)) {
        find_in_expr(*rangeExpr->startExpr);
        find_in_expr(*rangeExpr->endExpr);
    } else if (auto* importExpr = dynamic_cast<const ResolvedImportExpr*>(&expr)) {
        // 'import("' is 8 characters. We estimate the length to cover the string.
        if (is_at_location(importExpr->location, 10 + importExpr->moduleDecl.moduleDecl.identifier.length())) {
            found_decl = &importExpr->moduleDecl;
        }
    }
}

}  // namespace DMZ::lsp
