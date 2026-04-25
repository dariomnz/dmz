#ifdef DEBUG_SEMANTIC
#ifndef DEBUG
#define DEBUG
#endif
#endif
#include "semantic/Semantic.hpp"

#include "Debug.hpp"
#include "Stats.hpp"
#include "Utils.hpp"
#include "codegen/CodegenUtils.hpp"
#include "driver/Driver.hpp"
#include "parser/ParserSymbols.hpp"
#include "semantic/SemanticSymbols.hpp"

// #define DEBUG_SCOPES
// #ifdef DEBUG
// #define DEBUG_SCOPES
// #endif

namespace DMZ {

void Sema::dump_scopes() const {
    size_t level = 0;
    for (auto scope = m_currentScope; scope; scope = scope->parent) {
        debug_msg("Scope level " << level << " size " << scope->table.size());
        for (auto &&[id, decl] : scope->table) {
            println(indent(level) << "Identifier: " << id << " type " << decl->className());
            decl->dump(level, true);
        }
        level++;
    }
}

bool Sema::insert_decl_to_current_scope(ResolvedDecl &decl, bool ignoreIfFound) {
    debug_func(decl.identifier << " " << decl.location);
#ifdef DEBUG_SCOPES
    println("======================>>insert_decl_to_current_scope " << decl.identifier << " ======================");
#endif
    const auto foundDecl = lookup(decl.location, decl.identifier);

    if (foundDecl) {
        if (ignoreIfFound) return true;
#ifdef DEBUG_SCOPES
        dump_scopes();
#endif
        report(decl.location, "redeclaration of '" + decl.identifier + '\'');
        return false;
    }

    m_currentScope->table.emplace(decl.identifier, &decl);
#ifdef DEBUG_SCOPES
    dump_scopes();
    println("======================<<insert_decl_to_current_scope " << decl.identifier << " ======================");
#endif
    return true;
}

void Sema::remove_decl_to_current_scope(ResolvedDecl &decl) {
    for (auto scope = m_currentScope; scope; scope = scope->parent) {
        std::erase_if(scope->table, [&decl](auto &pair) { return &decl == pair.second; });
    }
}

bool Sema::insert_decl_to_module(ResolvedModuleDecl &moduleDecl, ptr<ResolvedDecl> decl) {
    [[maybe_unused]] auto declPtr = decl.get();
    debug_func("module: '" << moduleDecl.name() << "' decl '" << declPtr->identifier << "' " << declPtr->location);
    if (!declPtr->identifier.empty()) {
        auto it = std::find_if(moduleDecl.declarations.begin(), moduleDecl.declarations.end(),
                               [&](const ptr<ResolvedDecl> &d) { return declPtr->identifier == d->identifier; });
        if (it != moduleDecl.declarations.end()) {
            report(declPtr->location, "redeclaration in module of '" + declPtr->identifier + '\'');
            return false;
        }
    }
    moduleDecl.declarations.emplace_back(std::move(decl));
    return true;
}

std::string Sema::resolve_decl_name(std::string_view identifier) {
    std::string ret(identifier);
    if (m_currentStruct) {
        ret = m_currentStruct->name();
        ret += ".";
        ret += identifier;
        debug_msg("With struct " << ret);
    } else if (m_currentModule) {
        ret = m_currentModule->name();
        ret += ".";
        ret += identifier;
        debug_msg("With module " << ret);
    } else {
        debug_msg("Without all " << ret);
    }
    return ret;
}

ResolvedDecl *Sema::lookup(const SourceLocation &loc, const std::string_view id, ResolvedScope *scopeToUse) {
    debug_func(loc << " " << id);
#ifdef DEBUG_SCOPES
    println("---------------------->>lookup " << std::quoted(std::string(id)) << " ----------------------");
    dump_scopes();
    println("----------------------<<lookup " << std::quoted(std::string(id)) << " ----------------------");
#endif
    if (!scopeToUse) scopeToUse = m_currentScope;
    if (id == "@This") {
        if (m_currentStruct) {
            return m_currentStruct;
        } else {
            return report(loc, "unexpected use of @This outside a struct");
        }
    }

    std::string identifier(id);
    for (auto scope = scopeToUse; scope; scope = scope->parent) {
        auto it_decl = scope->table.find(identifier);
        if (it_decl != scope->table.end()) {
            auto declPtr = it_decl->second;
            // Delayed initialization if it was not initialized
            if (auto declStmt = dynamic_cast<ResolvedDeclStmt *>(declPtr)) {
                if (!declStmt->type) {
                    if (!resolve_decl_stmt_initialize(*declStmt)) return debug_ret(nullptr);
                }
            }
            return debug_ret(it_decl->second);
        }
    }

    return debug_ret(nullptr);
}

ResolvedDecl *Sema::lookup_in_module(const SourceLocation &loc, const ResolvedModuleDecl &moduleDecl,
                                     const std::string_view id) {
    debug_func("Module: " << moduleDecl.identifier << " id: " << id);
    if (!ensure_module_discovered(const_cast<ResolvedModuleDecl &>(moduleDecl))) return nullptr;
    for (auto &&decl : moduleDecl.declarations) {
        auto declPtr = decl.get();
        if (auto ds = dynamic_cast<ResolvedDeclStmt *>(declPtr)) {
            if (id == ds->varDecl->identifier) declPtr = ds->varDecl.get();
        }
        if (id != declPtr->identifier) continue;
        debug_msg("Search: " << declPtr->identifier << " in " << moduleDecl.identifier << " to find " << id);
        if (&moduleDecl != m_currentModule && !declPtr->isPublic) {
            report(loc, "cannot access private member '" + std::string(id) + "'");
            return report(decl->location, "'" + std::string(id) + "' must be marked as pub");
        }
        ScopeRAII moduleScope(*this, moduleDecl.scope.get());
        if (!ensure_fully_resolved(*declPtr)) return nullptr;
        return declPtr;
    }
    return nullptr;
}

ResolvedDecl *Sema::lookup_in_struct(const SourceLocation &loc, const ResolvedStructDecl &structDecl,
                                     const std::string_view id) {
    debug_func("Struct " << structDecl.identifier << " " << id);
    // Lazy: ensure members and functions are resolved before searching
    if (!ensure_struct_funcs_resolved(const_cast<ResolvedStructDecl &>(structDecl))) return debug_ret(nullptr);
    for (auto &&decl : structDecl.functions) {
        debug_msg("Search: " << decl->identifier << " in " << structDecl.identifier << " to find " << id);
        if (id != decl->identifier) continue;
        if (&structDecl != m_currentStruct && !decl->isPublic) {
            report(loc, "cannot access private member '" + std::string(id) + "'");
            return report(decl->location, "'" + std::string(id) + "' must be marked as pub");
        }

        debug_msg("Adding struct func " << decl->name() << " to pending decls");
        m_pending_decls.emplace(decl.get());
        return decl.get();
    }
    if (!ensure_struct_members_resolved(const_cast<ResolvedStructDecl &>(structDecl))) return debug_ret(nullptr);
    for (auto &&decl : structDecl.fields) {
        debug_msg("Search: " << decl->identifier << " in " << structDecl.identifier << " to find " << id);
        if (id != decl->identifier) continue;
        return decl.get();
    }
    if (auto unionDecl = dynamic_cast<const ResolvedUnionDecl *>(&structDecl)) {
        if (id == "tag") {
            return unionDecl->tag.get();
        }
    }
    return debug_ret(nullptr);
}

ptr<ResolvedType> Sema::resolve_type(const Expr &type) {
    ptr<ResolvedType> ret = nullptr;
    ResolvedType *retPtr = nullptr;
    debug_func("'" << type.to_str() << "' -> '" << (retPtr ? retPtr->to_str() : "nullptr") << "'");

    if (dynamic_cast<const TypeVoid *>(&type)) {
        ret = makePtr<ResolvedTypeVoid>(type.location);
        retPtr = ret.get();
        return debug_ret(ret);
    }
    if (dynamic_cast<const TypeError *>(&type)) {
        ret = makePtr<ResolvedTypeError>(type.location);
        retPtr = ret.get();
        return debug_ret(ret);
    }
    if (dynamic_cast<const TypeBool *>(&type)) {
        ret = makePtr<ResolvedTypeBool>(type.location);
        retPtr = ret.get();
        return debug_ret(ret);
    }
    if (auto numType = dynamic_cast<const TypeNumber *>(&type)) {
        auto num = numType->name.substr(1);
        int bitSize = 0;
        bool isPlatformSize = num == "size";
        if (isPlatformSize) {
            bitSize = CodegenUtils::ptrBitSize();
        } else {
            auto res = std::from_chars(num.data(), num.data() + num.size(), bitSize);
            if (bitSize == 0 || res.ec != std::errc()) {
                return report(type.location, "unexpected size of 0 in i type");
            }
        }
        ResolvedNumberKind kind;
        switch (numType->name[0]) {
            case 'i':
                kind = ResolvedNumberKind::Int;
                break;
            case 'u':
                kind = ResolvedNumberKind::UInt;
                break;
            case 'f':
                kind = ResolvedNumberKind::Float;
                break;
            default:
                return report(type.location, "unexpected kind '" + numType->name + "' in number type");
        }
        ret = makePtr<ResolvedTypeNumber>(type.location, kind, bitSize, isPlatformSize);
        retPtr = ret.get();
        return debug_ret(ret);
    }
    if (auto slcType = dynamic_cast<const TypeSlice *>(&type)) {
        varOrReturn(sliceType, resolve_type(*slcType->sliceType));
        ret = makePtr<ResolvedTypeSlice>(type.location, std::move(sliceType));
        retPtr = ret.get();
        return debug_ret(ret);
    }
    if (auto ptrType = dynamic_cast<const TypePointer *>(&type)) {
        varOrReturn(pointerType, resolve_type(*ptrType->pointerType));
        ret = makePtr<ResolvedTypePointer>(type.location, std::move(pointerType));
        retPtr = ret.get();
        return debug_ret(ret);
    }
    if (auto fnType = dynamic_cast<const TypeFunction *>(&type)) {
        std::vector<ptr<ResolvedType>> paramsTypes;
        for (auto &&param : fnType->paramsTypes) {
            varOrReturn(paramType, resolve_type(*param));
            paramsTypes.emplace_back(std::move(paramType));
        }
        varOrReturn(returnType, resolve_type(*fnType->returnType));

        ret = makePtr<ResolvedTypeFunction>(type.location, nullptr, std::move(paramsTypes), std::move(returnType));
        retPtr = ret.get();
        return debug_ret(ret);
    }
    if (auto ptrType = dynamic_cast<const DerefPtrExpr *>(&type)) {
        varOrReturn(pointerType, resolve_type(*ptrType->expr));

        ret = makePtr<ResolvedTypePointer>(type.location, std::move(pointerType));
        retPtr = ret.get();
        return debug_ret(ret);
    }
    if (auto optType = dynamic_cast<const UnaryOperator *>(&type)) {
        if (optType->op != TokenType::op_excla_mark)
            return report(type.location, "unexpected op in unary operator of a type");
        varOrReturn(optionalType, resolve_type(*optType->operand));

        ret = makePtr<ResolvedTypeOptional>(type.location, std::move(optionalType));
        retPtr = ret.get();
        return debug_ret(ret);
    }
    if (auto arrType = dynamic_cast<const ArrayAtExpr *>(&type)) {
        varOrReturn(arrayType, resolve_type(*arrType->array));

        varOrReturn(arraySizeExpr, resolve_expr(*arrType->index));
        int arraySize = 0;
        if (auto as = arraySizeExpr->get_constant_value()) {
            arraySize = as.value();
        } else if (auto intLit = dynamic_cast<const ResolvedIntLiteral *>(arraySizeExpr.get())) {
            arraySize = intLit->value;
        } else {
            debug_msg("arraySizeExpr: " << arraySizeExpr->location);
            return report(arraySizeExpr->location, "cannot deduce array size");
        }

        ret = makePtr<ResolvedTypeArray>(type.location, std::move(arrayType), arraySize);
        retPtr = ret.get();
        return debug_ret(ret);
    }
    if (auto declRefType = dynamic_cast<const DeclRefExpr *>(&type)) {
        auto decl = lookup(type.location, declRefType->identifier);
        if (!decl) {
#ifdef DEBUG_SCOPES
            dump_scopes();
#endif
            return report(declRefType->location, "type symbol '" + declRefType->identifier + "' not found");
        }
        if (dynamic_cast<ResolvedDeclStmt *>(decl) || dynamic_cast<ResolvedParamDecl *>(decl) ||
            dynamic_cast<ResolvedStructDecl *>(decl) || dynamic_cast<ResolvedUnionDecl *>(decl) ||
            dynamic_cast<ResolvedCaptureDecl *>(decl) || dynamic_cast<ResolvedVarDecl *>(decl)) {
            // Lazy: trigger full resolution if type is missing (e.g., from uninitialized DeclStmt)
            if (!decl->type) {
                if (!ensure_fully_resolved(*decl)) return nullptr;
                if (!decl->type) {
                    return report(declRefType->location,
                                  "could not resolve type for '" + declRefType->identifier + "'");
                }
            }
            if (auto unionType = dynamic_cast<ResolvedTypeUnionDecl *>(decl->type.get())) {
                ret = makePtr<ResolvedTypeUnion>(type.location, unionType->unionDecl());
            } else if (auto unionType = dynamic_cast<ResolvedTypeUnion *>(decl->type.get())) {
                ret = makePtr<ResolvedTypeUnion>(type.location, unionType->unionDecl());
            } else if (auto enumType = dynamic_cast<ResolvedTypeEnumDecl *>(decl->type.get())) {
                ret = makePtr<ResolvedTypeEnum>(type.location, enumType->enumDecl());
            } else if (auto enumType = dynamic_cast<ResolvedTypeEnum *>(decl->type.get())) {
                ret = makePtr<ResolvedTypeEnum>(type.location, enumType->enumDecl());
            } else if (auto struType = dynamic_cast<ResolvedTypeStructDecl *>(decl->type.get())) {
                ret = makePtr<ResolvedTypeStruct>(type.location, struType->decl);
            } else if (auto struType = dynamic_cast<ResolvedTypeStruct *>(decl->type.get())) {
                ret = makePtr<ResolvedTypeStruct>(type.location, struType->decl);
            } else {
                ret = decl->type->clone();
            }
            retPtr = ret.get();
            return debug_ret(ret);
        }
        if (auto genDecl = dynamic_cast<ResolvedGenericTypeDecl *>(decl)) {
            if (genDecl->specializedType) {
                ret = genDecl->specializedType->clone();
                retPtr = ret.get();
                return debug_ret(ret);
            } else {
                ret = makePtr<ResolvedTypeGeneric>(type.location, genDecl);
                retPtr = ret.get();
                return debug_ret(ret);
            }
        }
        decl->dump();
        dmz_unreachable(decl->location, "TODO");
    }
    if (auto vecType = dynamic_cast<const TypeSimd *>(&type)) {
        return resolve_simd_type(*vecType);
    }
    if (auto genType = dynamic_cast<const GenericExpr *>(&type)) {
        varOrReturn(specExpr, resolve_generic_expr(*genType));
        if (auto struDecl = dynamic_cast<ResolvedTypeStructDecl *>(specExpr->type.get())) {
            ret = makePtr<ResolvedTypeStruct>(type.location, struDecl->decl);
        } else {
            ret = specExpr->type->clone();
        }
        retPtr = ret.get();
        return debug_ret(ret);
    }
    if (auto memType = dynamic_cast<const MemberExpr *>(&type)) {
        varOrReturn(resolvedMem, resolve_member_expr(*memType));
        if (auto struDecl = dynamic_cast<ResolvedTypeStructDecl *>(resolvedMem->type.get())) {
            ret = makePtr<ResolvedTypeStruct>(type.location, struDecl->decl);
        } else {
            ret = resolvedMem->type->clone();
        }
        retPtr = ret.get();
        return debug_ret(ret);
    }
    if (const auto *stru = dynamic_cast<const StructDecl *>(&type)) {
        ptr<ResolvedStructDecl> ownedStructDecl = nullptr;
        ResolvedStructDecl *res = nullptr;
        auto it = m_resolvedStructs.find(stru);
        if (it != m_resolvedStructs.end()) {
            res = it->second;
        } else {
            varOrReturn(structDecl, resolve_struct_decl(*stru));
            res = structDecl.get();
            ownedStructDecl = std::move(structDecl);
        }
        ptr<ResolvedTypeStructDecl> retTypeStruct;
        if (auto unionDecl = dynamic_cast<ResolvedUnionDecl *>(res)) {
            retTypeStruct = makePtr<ResolvedTypeUnionDecl>(type.location, unionDecl);
        } else if (auto enumDecl = dynamic_cast<ResolvedEnumDecl *>(res)) {
            retTypeStruct = makePtr<ResolvedTypeEnumDecl>(type.location, enumDecl);
        } else {
            retTypeStruct = makePtr<ResolvedTypeStructDecl>(type.location, res);
        }
        retTypeStruct->ownedDecl = std::move(ownedStructDecl);
        if (retTypeStruct->ownedDecl) {
            if (m_currentModule && m_currentModule->module_path == m_driver.m_options.source) {
                debug_msg("Adding struct " << retTypeStruct->ownedDecl->name() << " to pending decls");
                m_pending_decls.emplace(retTypeStruct->ownedDecl.get());
            }
        }
        ret = std::move(retTypeStruct);
        retPtr = ret.get();
        return debug_ret(ret);
    }

    type.dump();
    dmz_unreachable(type.location, "TODO");
    (void)retPtr;
}

ptr<ResolvedType> Sema::resolve_simd_type(const TypeSimd &simdType) {
    debug_func(simdType.location);
    varOrReturn(resType, resolve_type(*simdType.simdType));

    varOrReturn(sizeExpr, resolve_expr(*simdType.simdSize));
    int vectorSize = 0;
    if (auto as = sizeExpr->get_constant_value()) {
        vectorSize = as.value();
    } else if (auto intLit = dynamic_cast<const ResolvedIntLiteral *>(sizeExpr.get())) {
        vectorSize = intLit->value;
    } else {
        return report(sizeExpr->location, "cannot deduce vector size, expected constant integer");
    }

    if (vectorSize <= 0) {
        return report(simdType.location, "vector size must be greater than 0");
    }

    return makePtr<ResolvedTypeSimd>(simdType.location, std::move(resType), std::move(sizeExpr), vectorSize);
}

ptr<ResolvedTypeSpecialized> Sema::resolve_specialized_type(const GenericExpr &genericExpr) {
    debug_func(genericExpr.location << " " << genericExpr.to_str());
    std::vector<ptr<ResolvedType>> specializedTypes;

    for (auto &&t : genericExpr.types) {
        varOrReturn(resType, resolve_type(*t));
        specializedTypes.emplace_back(std::move(resType));
    }

    return makePtr<ResolvedTypeSpecialized>(genericExpr.location, std::move(specializedTypes));
}

ptr<ResolvedType> Sema::re_resolve_type(const ResolvedType &type) {
    ptr<ResolvedType> ret = nullptr;
    ResolvedType *retPtr = nullptr;
    debug_func(type.className() << " '" << type.to_str() << "' -> " << (retPtr ? retPtr->className() : "nullptr")
                                << " '" << (retPtr ? retPtr->to_str() : "nullptr") << "'");
    if (auto genType = dynamic_cast<const ResolvedTypeGeneric *>(&type)) {
        if (genType->decl && genType->decl->specializedType) {
            ret = re_resolve_type(*genType->decl->specializedType);
            retPtr = ret.get();
            return ret;
        } else {
            ret = genType->clone();
            retPtr = ret.get();
            return ret;
        }
    }
    if (auto arrType = dynamic_cast<const ResolvedTypeArray *>(&type)) {
        ret = makePtr<ResolvedTypeArray>(arrType->location, re_resolve_type(*arrType->arrayType), arrType->arraySize);
        retPtr = ret.get();
        return ret;
    }
    if (auto optType = dynamic_cast<const ResolvedTypeOptional *>(&type)) {
        ret = makePtr<ResolvedTypeOptional>(optType->location, re_resolve_type(*optType->optionalType));
        retPtr = ret.get();
        return ret;
    }
    if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(&type)) {
        ret = makePtr<ResolvedTypePointer>(ptrType->location, re_resolve_type(*ptrType->pointerType));
        retPtr = ret.get();
        return ret;
    }
    if (auto sliceType = dynamic_cast<const ResolvedTypeSlice *>(&type)) {
        ret = makePtr<ResolvedTypeSlice>(sliceType->location, re_resolve_type(*sliceType->sliceType));
        retPtr = ret.get();
        return ret;
    }
    if (auto vectorType = dynamic_cast<const ResolvedTypeSimd *>(&type)) {
        auto cloned = castPtr<ResolvedTypeSimd>(vectorType->clone());
        cloned->simdType = re_resolve_type(*vectorType->simdType);
        ret = std::move(cloned);
        retPtr = ret.get();
        return ret;
    }
    if (type.kind == ResolvedTypeKind::Void || type.kind == ResolvedTypeKind::Number ||
        type.kind == ResolvedTypeKind::Bool || type.kind == ResolvedTypeKind::StructDecl ||
        type.kind == ResolvedTypeKind::Struct || type.kind == ResolvedTypeKind::UnionDecl ||
        type.kind == ResolvedTypeKind::Union || type.kind == ResolvedTypeKind::EnumDecl ||
        type.kind == ResolvedTypeKind::Enum || type.kind == ResolvedTypeKind::ErrorGroup ||
        type.kind == ResolvedTypeKind::Error || type.kind == ResolvedTypeKind::Function ||
        type.kind == ResolvedTypeKind::Module) {
        ret = type.clone();
        retPtr = ret.get();
        return ret;
    }
    (void)retPtr;
    type.dump();
    dmz_unreachable(type.location, "TODO");
}

std::vector<ptr<ResolvedModuleDecl>> Sema::resolve_ast_decl(std::filesystem::path sourcePath, bool needMain) {
    debug_func("");
    ScopedTimer(StatType::Semantic_Declarations);

    std::filesystem::path sourceAbsPath = std::filesystem::canonical(sourcePath);
    m_ast->module_path = sourceAbsPath;

    std::vector<ptr<ModuleDecl>> modules;

    // Add main module
    modules.emplace_back(std::move(m_ast));

    auto resolvedModules = resolve_modules_decls(modules);
    for (auto &mod : resolvedModules) {
        m_lazy_modules.push_back(std::move(mod));
    }

    // Put main module AST back
    if (!modules.empty()) {
        m_ast = std::move(modules[0]);
    }

    if (m_driver.m_options.test) {
        ImportExpr stdImportExpr(SourceLocation::builtin(), "std");
        auto std_import_expr = resolve_import_expr(stdImportExpr);
        if (!std_import_expr) return {};
        ImportExpr builtinImportExpr(SourceLocation::builtin(), "builtin");
        auto builtin_import_expr = resolve_import_expr(builtinImportExpr);
        if (!builtin_import_expr) return {};

        for (auto &decl : builtin_import_expr->moduleDecl.declarations) {
            if (decl->identifier == "__builtin_main_test") {
                if (!ensure_fully_resolved(*decl)) return {};
                break;
            }
        }
    }

    for (size_t i = 0; i < m_lazy_modules.size(); ++i) {
        auto &mod = m_lazy_modules[i];
        if (!ensure_module_discovered(*mod)) {
            report(mod->location, "Failed to resolve lazy module " + mod->identifier);
            return {};
        }
    }

    if (needMain) {
        bool haveMain = false;
        ResolvedModuleDecl *mainMod = nullptr;
        for (auto &&mod : m_lazy_modules) {
            if (mod->module_path == sourceAbsPath) {
                mainMod = mod.get();
                for (auto &&decl : mod->declarations) {
                    if (decl->identifier == "main") {
                        haveMain = true;
                        break;
                    }
                }
                break;
            }
        }
        if (!haveMain) {
            report(mainMod ? mainMod->location : SourceLocation{}, "main function not found in module");
            return {};
        }
    }

    resolvedModules.swap(m_lazy_modules);
    return resolvedModules;
}

bool Sema::resolve_ast_body(std::vector<ptr<ResolvedModuleDecl>> &moduleDecls) {
    bool error = false;
    debug_func((error ? "error" : "no error"));
    {
        ScopedTimer(StatType::Semantic_Body);
        m_lazy_modules.swap(moduleDecls);
        for (size_t i = 0; i < m_lazy_modules.size(); i++) {
            auto &&module = m_lazy_modules[i];
            if (module->module_path != m_driver.m_options.source) continue;
            ScopedTimerName(StatType::Semantic_Body, module->name());
            if (!resolve_module_body(*module)) {
                error = true;
            }
        }

        if (!resolve_pending_body()) {
            error = true;
        }
    }

    moduleDecls.swap(m_lazy_modules);
    return !error;
}

bool Sema::run_flow_sensitive_checks(const ResolvedFuncDecl &fn) {
    debug_func(fn.location);
    const ResolvedBlock *block;
    if (auto resfn = dynamic_cast<const ResolvedFunctionDecl *>(&fn)) {
        block = resfn->body.get();
    } else if (auto resfn = dynamic_cast<const ResolvedSpecializedFunctionDecl *>(&fn)) {
        block = resfn->body.get();
    } else {
        dmz_unreachable(fn.location, "unexpected function");
    }
    CFG cfg = CFGBuilder().build(*block);

    bool error = false;
    error |= check_return_on_all_paths(fn, cfg);
    error |= check_variable_initialization(cfg);
    debug_msg("error " << error);
    return error;
};

bool Sema::check_return_on_all_paths(const ResolvedFuncDecl &fn, const CFG &cfg) {
    debug_func(fn.location);
    auto fnType = fn.getFnType();
    auto optType = dynamic_cast<const ResolvedTypeOptional *>(fnType->returnType.get());
    if (fnType->returnType->kind == ResolvedTypeKind::Void ||
        (optType && optType->optionalType->kind == ResolvedTypeKind::Void))
        return false;

    int returnCount = 0;
    bool exitReached = false;

    std::unordered_set<int> visited;
    std::vector<int> worklist;
    worklist.emplace_back(cfg.entry);

    while (!worklist.empty()) {
        int bb = worklist.back();
        worklist.pop_back();

        if (!visited.emplace(bb).second) continue;

        exitReached |= bb == cfg.exit;

        const auto &[preds, succs, stmts] = cfg.m_basicBlocks[bb];

        if (!stmts.empty() && dynamic_cast<const ResolvedReturnStmt *>(stmts[0])) {
            ++returnCount;
            continue;
        }

        for (auto &&[succ, reachable] : succs)
            if (reachable) worklist.emplace_back(succ);
    }

    if (exitReached || returnCount == 0) {
        report(fn.location, returnCount > 0 ? "non-void function doesn't return a value on every path"
                                            : "non-void function doesn't return a value");
    }

    return exitReached || returnCount == 0;
}

bool Sema::check_variable_initialization(const CFG &cfg) {
    debug_func("");
    enum class State { Bottom, Unassigned, Assigned, Top };
    static std::unordered_map<State, std::string> state_to_string = {
        {State::Bottom, "Bottom"},
        {State::Unassigned, "Unassigned"},
        {State::Assigned, "Assigned"},
        {State::Top, "Top"},
    };

    using Lattice = std::unordered_map<const ResolvedDecl *, State>;

    auto joinStates = [](State s1, State s2) {
        if (s1 == s2) return s1;

        if (s1 == State::Bottom) return s2;

        if (s2 == State::Bottom) return s1;

        return State::Top;
    };

    std::vector<Lattice> curLattices(cfg.m_basicBlocks.size());
    std::vector<std::pair<SourceLocation, std::string>> pendingErrors;

    bool changed = true;
    while (changed) {
        changed = false;
        pendingErrors.clear();

        for (int bb = cfg.entry; bb != cfg.exit; --bb) {
            const auto &[preds, succs, stmts] = cfg.m_basicBlocks[bb];

            Lattice tmp;
            for (auto &&pred : preds) {
                for (auto &&[decl, state] : curLattices[pred.first]) {
                    tmp[decl] = joinStates(tmp[decl], state);
                }
            }

            for (auto it = stmts.rbegin(); it != stmts.rend(); ++it) {
                const ResolvedStmt *stmt = *it;

                if (auto *decl = dynamic_cast<const ResolvedDeclStmt *>(stmt)) {
                    tmp[decl->varDecl.get()] = (decl->varDecl->initializer) ? State::Assigned : State::Unassigned;
                    continue;
                }
                if (auto *decl = dynamic_cast<const ResolvedParamDecl *>(stmt)) {
                    tmp[decl] = State::Assigned;
                    continue;
                }
                if (auto *decl = dynamic_cast<const ResolvedCaptureDecl *>(stmt)) {
                    tmp[decl] = State::Assigned;
                    continue;
                }

                if (const auto *assignment = dynamic_cast<const ResolvedAssignment *>(stmt)) {
                    const ResolvedExpr *base = assignment->assignee.get();
                    while (const auto *member = dynamic_cast<const ResolvedMemberExpr *>(base))
                        base = member->base.get();

                    if (const auto *dre = dynamic_cast<const ResolvedDeclRefExpr *>(base)) {
                        const ResolvedDecl *decl = &dre->decl;
                        if (auto resDecl = dynamic_cast<const ResolvedDeclStmt *>(decl)) {
                            decl = resDecl->varDecl.get();
                        }

                        if (decl) {
                            if (!decl->isMutable && decl->type->kind != ResolvedTypeKind::Pointer &&
                                tmp[decl] != State::Unassigned) {
                                std::string msg = '\'' + decl->identifier + "' cannot be mutated";
                                pendingErrors.emplace_back(assignment->location, std::move(msg));
                            }
                            tmp[decl] = State::Assigned;
                        }
                    }
                    continue;
                }

                if (const auto *dre = dynamic_cast<const ResolvedDeclRefExpr *>(stmt)) {
                    const ResolvedVarDecl *var = dynamic_cast<const ResolvedVarDecl *>(&dre->decl);
                    if (!var) {
                        if (auto resDecl = dynamic_cast<const ResolvedDeclStmt *>(&dre->decl)) {
                            var = resDecl->varDecl.get();
                        }
                    }

                    if (var) {
                        if (var->type->kind != ResolvedTypeKind::StructDecl &&
                            var->type->kind != ResolvedTypeKind::UnionDecl &&
                            var->type->kind != ResolvedTypeKind::EnumDecl) {
                            if (var->initializer) {
                                tmp[var] = State::Assigned;
                            }

                            if (tmp[var] != State::Assigned) {
                                std::string msg = '\'' + var->identifier + "' is not initialized";
                                pendingErrors.emplace_back(dre->location, std::move(msg));
                            }
                        }
                    }
                }
            }

            if (curLattices[bb] != tmp) {
                curLattices[bb] = tmp;
                changed = true;
            }
        }
    }

    for (auto &&[loc, msg] : pendingErrors) {
        report(loc, msg);
    }

    return !pendingErrors.empty();
}

bool Sema::perform_implicit_cast(ptr<ResolvedExpr> &expr, const ResolvedType &expectedType) {
    debug_func(expr->location << " type " << expr->type->className() << " " << expr->type->to_str() << " expectedType "
                              << expectedType.className() << " " << expectedType.to_str());
    if (auto strLit = dynamic_cast<ResolvedStringLiteral *>(expr.get())) {
        if (auto sliceType = dynamic_cast<const ResolvedTypeSlice *>(&expectedType)) {
            if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(sliceType->sliceType.get())) {
                if (numType->numberKind == ResolvedNumberKind::UInt && numType->bitSize == 8) {
                    strLit->type = sliceType->clone();
                }
            }
        }
    }

    if (auto intLit = dynamic_cast<ResolvedIntLiteral *>(expr.get())) {
        if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(&expectedType)) {
            if (numType->numberKind == ResolvedNumberKind::Int || numType->numberKind == ResolvedNumberKind::UInt) {
                intLit->type = numType->clone();
            }
        }
    }

    if (auto floatLit = dynamic_cast<ResolvedFloatLiteral *>(expr.get())) {
        if (auto numType = dynamic_cast<const ResolvedTypeNumber *>(&expectedType)) {
            if (numType->numberKind == ResolvedNumberKind::Float) {
                floatLit->type = numType->clone();
            }
        }
    }

    if (auto splatExpr = dynamic_cast<ResolvedSimdSplatExpr *>(expr.get())) {
        if (auto simdType = dynamic_cast<const ResolvedTypeSimd *>(&expectedType)) {
            if (!perform_implicit_cast(splatExpr->value, *simdType->simdType)) return false;
            if (simdType->simdType->compare(*splatExpr->value->type)) {
                splatExpr->type = simdType->clone();
            } else {
                report(splatExpr->location,
                       "cannot splat '" + splatExpr->value->type->to_str() + "' into '" + expectedType.to_str() + "'");
                return false;
            }
        }
    }

    if (auto iotaExpr = dynamic_cast<ResolvedSimdIotaExpr *>(expr.get())) {
        if (auto simdType = dynamic_cast<const ResolvedTypeSimd *>(&expectedType)) {
            iotaExpr->type = simdType->clone();
        }
    }

    if (auto binOp = dynamic_cast<ResolvedBinaryOperator *>(expr.get())) {
        if (auto simdType = dynamic_cast<const ResolvedTypeSimd *>(&expectedType)) {
            if (binOp->type->kind == ResolvedTypeKind::Simd &&
                dynamic_cast<const ResolvedTypeSimd *>(binOp->type.get())->simdSize == 0) {
                if (!perform_implicit_cast(binOp->lhs, *simdType)) return false;
                if (!perform_implicit_cast(binOp->rhs, *simdType)) return false;
                if (binOp->lhs->type->compare(*simdType) && binOp->rhs->type->compare(*simdType)) {
                    binOp->type = simdType->clone();
                }
            }
        }
    }

    if (auto grouping = dynamic_cast<ResolvedGroupingExpr *>(expr.get())) {
        if (auto simdType = dynamic_cast<const ResolvedTypeSimd *>(&expectedType)) {
            if (grouping->type->kind == ResolvedTypeKind::Simd &&
                dynamic_cast<const ResolvedTypeSimd *>(grouping->type.get())->simdSize == 0) {
                if (!perform_implicit_cast(grouping->expr, *simdType)) return false;
                if (grouping->expr->type->compare(*simdType)) {
                    grouping->type = simdType->clone();
                }
            }
        }
    }

    if (auto arrayInstantiation = dynamic_cast<ResolvedArrayInstantiationExpr *>(expr.get())) {
        if (auto arrayType = dynamic_cast<const ResolvedTypeArray *>(&expectedType)) {
            for (auto &elem : arrayInstantiation->initializers) {
                if (!perform_implicit_cast(elem, *arrayType->arrayType)) return false;
            }
            if (!arrayInstantiation->initializers.empty() &&
                arrayInstantiation->initializers[0]->type->compare(*arrayType->arrayType)) {
                arrayInstantiation->type = arrayType->clone();
            }
        }
    }

    if (auto autoMember = dynamic_cast<ResolvedAutoMemberExpr *>(expr.get())) {
        if (auto enumType = dynamic_cast<const ResolvedTypeEnum *>(&expectedType)) {
            if (!ensure_struct_members_resolved(*enumType->decl)) return false;
            auto member = lookup_in_struct(autoMember->location, *enumType->decl, autoMember->field);
            if (!member) {
                report(autoMember->location,
                       "enum '" + enumType->decl->identifier + "' has no member '" + autoMember->field + "'");
                return false;
            }
            expr->type = enumType->clone();
            if (auto field = dynamic_cast<ResolvedFieldDecl *>(member)) {
                autoMember->fieldDecl = field;
            } else {
                dmz_unreachable(member->location, "Expected a field decl get " + std::string(member->className()));
            }
            expr->set_constant_value(member->get_constant_value());
        }
    }
    return true;
}

ResolvedBuiltinFunctionDecl *Sema::resolve_builtin_function_symbol(const std::string &fnName) {
    debug_func(fnName);
    auto it = m_funcBuiltins.find(fnName);
    if (it != m_funcBuiltins.end() && it->second != nullptr) {
        debug_msg("found");
        return it->second;
    }

    SourceLocation loc = SourceLocation::builtin();
    if (fnName == "@call") {
        auto genericType = makePtr<ResolvedTypeGeneric>(loc, nullptr);
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "fn", genericType->clone(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "args", genericType->clone(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), genericType->clone());
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        debug_msg("created @call");
        return &funcDecl;
    } else if (fnName == "@atomicLoad") {
        auto genericType = makePtr<ResolvedTypeGeneric>(loc, nullptr);
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "atomicPtr", genericType->clone(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), genericType->clone());
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        debug_msg("created @atomicLoad");
        return &funcDecl;
    } else if (fnName == "@atomicStore") {
        auto genericType = makePtr<ResolvedTypeGeneric>(loc, nullptr);
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "atomicPtr", genericType->clone(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "value", genericType->clone(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), genericType->clone());
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        debug_msg("created @atomicStore");
        return &funcDecl;
    } else if (fnName == "@atomicCmpExS" || fnName == "@atomicCmpExW") {
        auto genericType = makePtr<ResolvedTypeGeneric>(loc, nullptr);
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "atomicPtr", genericType->clone(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "expected", genericType->clone(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "desired", genericType->clone(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        paramsTypes.emplace_back(params[2]->type->clone());
        auto fnType =
            makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), makePtr<ResolvedTypeBool>(loc));
        if (fnName == "@atomicCmpExS") {
            static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
            it->second = &funcDecl;
        } else if (fnName == "@atomicCmpExW") {
            static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
            it->second = &funcDecl;
        }
        debug_msg("created " + fnName);
        return it->second;
    } else if (fnName == "@atomicRmw") {
        auto genericType = makePtr<ResolvedTypeGeneric>(loc, nullptr);
        std::vector<ptr<ResolvedParamDecl>> params;
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "atom", genericType->clone(), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "op", makePtr<ResolvedTypeEnum>(loc, nullptr), false));
        params.emplace_back(makePtr<ResolvedParamDecl>(loc, "value", genericType->clone(), false));
        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(params[0]->type->clone());
        paramsTypes.emplace_back(params[1]->type->clone());
        paramsTypes.emplace_back(params[2]->type->clone());
        auto fnType = makePtr<ResolvedTypeFunction>(loc, nullptr, std::move(paramsTypes), genericType->clone());
        static auto funcDecl = ResolvedBuiltinFunctionDecl(loc, fnName, std::move(fnType), std::move(params), true);
        it->second = &funcDecl;
        debug_msg("created @atomicRmw");
        return &funcDecl;
    }
    debug_msg("return null");
    return nullptr;
}

ptr<ResolvedTypeFunction> Sema::resolve_builtin_function_expr(ResolvedExpr &call,
                                                              ResolvedBuiltinFunctionDecl &resolvedCallee,
                                                              std::vector<ptr<ResolvedExpr>> &resolvedArguments) {
    // Return real builtin function type to check params afterwards
    if (resolvedCallee.identifier == "@call") {
        auto &fnParam = resolvedArguments[0];
        auto &argsParam = resolvedArguments[1];

        // fn is a pointer to a function
        ptr<ResolvedTypeFunction> fnType = nullptr;
        if (auto *fnPtrType = dynamic_cast<const ResolvedTypePointer *>(fnParam->type.get())) {
            if (auto *paramFnType = dynamic_cast<const ResolvedTypeFunction *>(fnPtrType->pointerType.get())) {
                fnType = castPtr<ResolvedTypeFunction>(paramFnType->clone());
            } else {
                return report(fnParam->location, "@call: fn is not a pointer to a function");
            }
        } else if (fnParam->type->kind != ResolvedTypeKind::Generic) {
            return report(fnParam->location, "@call: fn is not a pointer to a function");
        }
        // args is a touple with exactly the same number of params than the function
        ptr<ResolvedTypeStruct> argsTupleType = nullptr;
        if (auto *tupleType = dynamic_cast<const ResolvedTypeStruct *>(argsParam->type.get())) {
            if (tupleType->decl && tupleType->decl->isTuple) {
                if (fnType) {
                    if (tupleType->decl->fields.size() == fnType->paramsTypes.size()) {
                        if (auto *struInit = dynamic_cast<ResolvedStructInstantiationExpr *>(argsParam.get())) {
                            for (size_t i = 0; i < struInit->fieldInitializers.size(); i++) {
                                if (!perform_implicit_cast(struInit->fieldInitializers[i]->initializer,
                                                           *fnType->paramsTypes[i]))
                                    return nullptr;
                            }
                        }
                        argsTupleType = castPtr<ResolvedTypeStruct>(tupleType->clone());
                    } else {
                        return report(argsParam->location, "@call: tuple size does not match function params size");
                    }
                } else {
                    argsTupleType = castPtr<ResolvedTypeStruct>(tupleType->clone());
                }
            } else {
                return report(argsParam->location, "@call: args is not a tuple");
            }
        } else if (argsParam->type->kind != ResolvedTypeKind::Generic) {
            return report(argsParam->location, "@call: args is not a tuple");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(fnParam->type->clone());
        paramsTypes.emplace_back(argsParam->type->clone());
        auto retReturnType =
            fnType ? fnType->returnType->clone() : makePtr<ResolvedTypeGeneric>(call.location, nullptr);
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                                 std::move(retReturnType));
        call.type = ret->clone();
        return ret;
    } else if (resolvedCallee.identifier == "@atomicLoad") {
        auto &atomicPtrParam = resolvedArguments[0];
        auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(atomicPtrParam->type.get());
        if (!ptrType) {
            return report(atomicPtrParam->location,
                          "expected pointer type for @atomicLoad, actual '" + atomicPtrParam->type->to_str() + "'");
        }
        if (ptrType->pointerType->kind != ResolvedTypeKind::Number &&
            ptrType->pointerType->kind != ResolvedTypeKind::Pointer) {
            return report(ptrType->location,
                          "atomic load operation only supported for numeric or pointer types, actual '" +
                              ptrType->pointerType->to_str() + "'");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(atomicPtrParam->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                                 ptrType->pointerType->clone());
        call.type = ret->returnType->clone();
        return ret;
    } else if (resolvedCallee.identifier == "@atomicStore") {
        auto &atomicPtrParam = resolvedArguments[0];
        auto &valueParam = resolvedArguments[1];
        auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(atomicPtrParam->type.get());
        if (!ptrType) {
            return report(atomicPtrParam->location,
                          "expected pointer type for @atomicStore, actual '" + atomicPtrParam->type->to_str() + "'");
        }
        if (ptrType->pointerType->kind != ResolvedTypeKind::Number &&
            ptrType->pointerType->kind != ResolvedTypeKind::Pointer) {
            return report(ptrType->location,
                          "atomic store operation only supported for numeric or pointer types, actual '" +
                              ptrType->pointerType->to_str() + "'");
        }
        if (!perform_implicit_cast(valueParam, *ptrType->pointerType)) return nullptr;
        if (!valueParam->type->compare(*ptrType->pointerType)) {
            return report(valueParam->location, "cannot store '" + valueParam->type->to_str() + "' into '" +
                                                    ptrType->pointerType->to_str() + "'");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(atomicPtrParam->type->clone());
        paramsTypes.emplace_back(valueParam->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                                 makePtr<ResolvedTypeVoid>(call.location));
        call.type = ret->returnType->clone();
        return ret;
    } else if (resolvedCallee.identifier == "@atomicCmpExS" || resolvedCallee.identifier == "@atomicCmpExW") {
        auto &atomicPtrParam = resolvedArguments[0];
        auto &expectedParam = resolvedArguments[1];
        auto &desiredParam = resolvedArguments[2];
        auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(atomicPtrParam->type.get());
        if (!ptrType) {
            return report(atomicPtrParam->location, "expected pointer type for " + resolvedCallee.identifier +
                                                        ", actual '" + atomicPtrParam->type->to_str() + "'");
        }
        if (ptrType->pointerType->kind != ResolvedTypeKind::Number &&
            ptrType->pointerType->kind != ResolvedTypeKind::Pointer) {
            return report(ptrType->location,
                          "atomic compare-exchange operation only supported for numeric or pointer types, actual '" +
                              ptrType->pointerType->to_str() + "'");
        }
        if (!perform_implicit_cast(expectedParam, *ptrType->pointerType)) return nullptr;
        if (!perform_implicit_cast(desiredParam, *ptrType->pointerType)) return nullptr;

        if (!expectedParam->type->compare(*ptrType->pointerType)) {
            return report(expectedParam->location, "expected '" + ptrType->pointerType->to_str() +
                                                       "' for second parameter of " + resolvedCallee.identifier +
                                                       ", actual '" + expectedParam->type->to_str() + "'");
        }
        if (!desiredParam->type->compare(*ptrType->pointerType)) {
            return report(desiredParam->location, "expected '" + ptrType->pointerType->to_str() +
                                                      "' for third parameter of " + resolvedCallee.identifier +
                                                      ", actual '" + desiredParam->type->to_str() + "'");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(atomicPtrParam->type->clone());
        paramsTypes.emplace_back(expectedParam->type->clone());
        paramsTypes.emplace_back(desiredParam->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                                 makePtr<ResolvedTypeBool>(call.location));
        call.type = ret->returnType->clone();
        return ret;
    } else if (resolvedCallee.identifier == "@atomicRmw") {
        auto &atomicPtrParam = resolvedArguments[0];
        auto &opParam = resolvedArguments[1];
        auto &valueParam = resolvedArguments[2];

        auto *ptrType = dynamic_cast<const ResolvedTypePointer *>(atomicPtrParam->type.get());
        if (!ptrType) {
            return report(atomicPtrParam->location,
                          "expected pointer type for @atomicRmw, actual '" + atomicPtrParam->type->to_str() + "'");
        }

        ImportExpr atomicImportExpr(SourceLocation::builtin(), "atomic");
        auto atomic_import_expr = resolve_import_expr(atomicImportExpr);
        if (!atomic_import_expr) return {};

        auto atomicOpDecl = lookup_in_module(call.location, atomic_import_expr->moduleDecl, "AtomicOp");
        if (atomicOpDecl) {
            println("AtomicOp found " << atomicOpDecl->type->className());
            if (auto enumDecl = dynamic_cast<ResolvedTypeEnumDecl *>(atomicOpDecl->type.get())) {
                if (!perform_implicit_cast(opParam, *makePtr<ResolvedTypeEnum>(call.location, enumDecl->enumDecl()))) {
                    return nullptr;
                }
            }
        } else {
            return report(opParam->location, "AtomicOp not found in std/atomic.dmz");
        }

        if (ptrType->pointerType->kind != ResolvedTypeKind::Number &&
            ptrType->pointerType->kind != ResolvedTypeKind::Pointer) {
            std::string opStr = "unknown";
            if (auto autoMember = dynamic_cast<ResolvedAutoMemberExpr *>(opParam.get())) {
                opStr = autoMember->field;
            }
            return report(call.location, "atomic RMW operation '" + opStr +
                                             "' only supported for numeric or pointer types, actual '" +
                                             ptrType->pointerType->to_str() + "'");
        }

        if (!perform_implicit_cast(valueParam, *ptrType->pointerType)) return nullptr;
        if (!valueParam->type->compare(*ptrType->pointerType)) {
            return report(valueParam->location, "cannot RMW '" + valueParam->type->to_str() + "' into '" +
                                                    ptrType->pointerType->to_str() + "'");
        }

        std::vector<ptr<ResolvedType>> paramsTypes;
        paramsTypes.emplace_back(atomicPtrParam->type->clone());
        paramsTypes.emplace_back(opParam->type->clone());
        paramsTypes.emplace_back(valueParam->type->clone());
        auto ret = makePtr<ResolvedTypeFunction>(call.location, &resolvedCallee, std::move(paramsTypes),
                                                 ptrType->pointerType->clone());
        call.type = ret->returnType->clone();
        return ret;
    }
    return report(call.location, "unknown builtin function");
}
}  // namespace DMZ