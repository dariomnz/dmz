#include "lsp/node_finder.hpp"

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
inline size_t identifier_len(std::string_view name) {
    size_t ret = name.length();
    if (name.find("structL") == 0)
        ret = 6;
    else if (name.find("unionL") == 0)
        ret = 5;
    else if (name.find("enumL") == 0)
        ret = 4;
    return ret;
};

void NodeFinder::find_in_decl(const ResolvedDecl& decl) {
    if (found_decl) return;

    size_t len = identifier_len(decl.identifier);

    if (is_at_location(decl.location, len)) {
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
    }

    if (const auto* fd = dynamic_cast<const ResolvedFuncDecl*>(&decl)) {
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
        if (fd->resolvedReturnTypeExpr) find_in_expr(*fd->resolvedReturnTypeExpr);
        if (found_decl) return;

        if (const auto* functionDecl = dynamic_cast<const ResolvedFunctionDecl*>(fd)) {
            if (functionDecl->body) find_in_stmt(*functionDecl->body);
        }
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
    } else if (const auto* ud = dynamic_cast<const ResolvedUnionDecl*>(&decl)) {
        for (const auto& field : ud->fields) {
            find_in_decl(*field);
            if (found_decl) return;
        }
        for (const auto& method : ud->functions) {
            find_in_decl(*method);
            if (found_decl) return;
        }
    } else if (const auto* ds = dynamic_cast<const ResolvedDeclStmt*>(&decl)) {
        if (ds->varDecl) find_in_decl(*ds->varDecl);
    } else if (const auto* varDecl = dynamic_cast<const ResolvedVarDecl*>(&decl)) {
        if (varDecl->initializer) find_in_expr(*varDecl->initializer);
        if (varDecl->resolvedTypeExpr) find_in_expr(*varDecl->resolvedTypeExpr);
        if (varDecl->type) {
            if (auto* type = dynamic_cast<const ResolvedTypeStructDecl*>(varDecl->type.get())) {
                if (type->ownedDecl) {
                    find_in_decl(*type->ownedDecl);
                }
            }
        }
    } else if (const auto* field = dynamic_cast<const ResolvedFieldDecl*>(&decl)) {
        if (field->default_initializer) find_in_expr(*field->default_initializer);
        if (field->resolvedTypeExpr) find_in_expr(*field->resolvedTypeExpr);
    } else if (const auto* param = dynamic_cast<const ResolvedParamDecl*>(&decl)) {
        if (param->resolvedTypeExpr) find_in_expr(*param->resolvedTypeExpr);
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
        for (const auto& cond : caseStmt->conditions) {
            find_in_expr(*cond);
        }
        find_in_stmt(*caseStmt->block);
    }
}

void NodeFinder::find_in_expr(const ResolvedExpr& expr) {
    if (found_decl) return;

    if (const auto* dr = dynamic_cast<const ResolvedDeclRefExpr*>(&expr)) {
        if (is_at_location(dr->location, dr->identifier.length())) {
            found_decl = &dr->decl;
            return;
        }
    } else if (const auto* ge = dynamic_cast<const ResolvedGenericExpr*>(&expr)) {
        find_in_expr(*ge->base);
        if (found_decl) return;
        for (const auto& tyExpr : ge->specializedTypesExpr) {
            find_in_expr(*tyExpr);
            if (found_decl) return;
        }
    } else if (const auto* me = dynamic_cast<const ResolvedMemberExpr*>(&expr)) {
        // me->location is the dot. Its length is 1 + identifier length.
        if (is_at_location(me->location, 1 + me->member.identifier.length())) {
            found_decl = &me->member;
            return;
        }
        find_in_expr(*me->base);
    } else if (const auto* me = dynamic_cast<const ResolvedAutoMemberExpr*>(&expr)) {
        if (is_at_location(me->location, me->field.length())) {
            found_decl = me->fieldDecl;
            return;
        }
    } else if (const auto* sie = dynamic_cast<const ResolvedStructInstantiationExpr*>(&expr)) {
        if (!sie->isTuple) {
            find_in_expr(*sie->base);
            if (found_decl) return;
        }
        for (const auto& init : sie->fieldInitializers) {
            if (!sie->isTuple && is_at_location(init->location, init->field.identifier.length())) {
                found_decl = &init->field;
                return;
            }
            find_in_expr(*init->initializer);
            if (found_decl) return;
        }
    } else if (const auto* uie = dynamic_cast<const ResolvedUnionInstantiationExpr*>(&expr)) {
        if (is_at_location(uie->location, uie->unionDecl.identifier.length())) {
            found_decl = &uie->unionDecl;
            return;
        }
        if (is_at_location(uie->fieldInitializer->location, uie->fieldInitializer->field.identifier.length())) {
            found_decl = &uie->fieldInitializer->field;
            return;
        }
        find_in_expr(*uie->fieldInitializer->initializer);
    } else if (const auto* re = dynamic_cast<const ResolvedArrayInstantiationExpr*>(&expr)) {
        for (const auto& init : re->initializers) {
            find_in_expr(*init);
            if (found_decl) return;
        }
    } else if (const auto* te = dynamic_cast<const ResolvedTypeExpr*>(&expr)) {
        if (te->typeExpr) find_in_expr(*te->typeExpr);
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
    } else if (const auto* se = dynamic_cast<const ResolvedTypeSimdExpr*>(&expr)) {
        find_in_expr(*se->simdType);
        if (found_decl) return;
        find_in_expr(*se->sizeExpr);
    } else if (auto* fnSig = dynamic_cast<const ResolvedTypeFunctionExpr*>(&expr)) {
        find_in_expr(*fnSig->resolvedReturnTypeExpr);
        if (found_decl) return;
        for (auto&& param : fnSig->params) {
            find_in_expr(*param);
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
    } else if (auto* ptrExpr = dynamic_cast<const ResolvedRefPtrExpr*>(&expr)) {
        find_in_expr(*ptrExpr->expr);
    } else if (auto* ptrExpr = dynamic_cast<const ResolvedDerefPtrExpr*>(&expr)) {
        find_in_expr(*ptrExpr->expr);
    } else if (dynamic_cast<const ResolvedErrorInPlaceExpr*>(&expr)) {
        return;
    } else if (auto* catchErr = dynamic_cast<const ResolvedCatchErrorExpr*>(&expr)) {
        find_in_expr(*catchErr->errorToCatch);
        if (found_decl) return;
        if (catchErr->errorVar) {
            find_in_decl(*catchErr->errorVar);
            if (found_decl) return;
        }
        find_in_stmt(*catchErr->handler);
    } else if (auto* tryErr = dynamic_cast<const ResolvedTryErrorExpr*>(&expr)) {
        find_in_expr(*tryErr->errorToTry);
    } else if (auto* orelseErr = dynamic_cast<const ResolvedOrElseErrorExpr*>(&expr)) {
        find_in_expr(*orelseErr->errorToOrElse);
        if (found_decl) return;
        find_in_expr(*orelseErr->orElseExpr);
    } else if (auto* rangeExpr = dynamic_cast<const ResolvedRangeExpr*>(&expr)) {
        find_in_expr(*rangeExpr->startExpr);
        find_in_expr(*rangeExpr->endExpr);
    } else if (auto* importExpr = dynamic_cast<const ResolvedImportExpr*>(&expr)) {
        // 'import("' is 8 characters. We estimate the length to cover the string.
        if (is_at_location(importExpr->location, 10 + importExpr->moduleDecl.identifier.length())) {
            found_decl = &importExpr->moduleDecl;
        }
    }
}
}  // namespace DMZ::lsp
