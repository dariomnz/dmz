// #define DEBUG
#include "semantic/Semantic.hpp"

#include "Debug.hpp"
#include "Stats.hpp"
#include "Utils.hpp"
#include "parser/ParserSymbols.hpp"
#include "semantic/SemanticSymbols.hpp"

// #define DEBUG_SCOPES
// #ifdef DEBUG
// #define DEBUG_SCOPES
// #endif

namespace DMZ {

void Sema::dump_scopes() const {
    debug_msg("m_scopes.size " << m_scopes.size());
    size_t level = 0;
    for (auto &&scope : m_scopes) {
        debug_msg("m_scopes[" << level << "].size " << scope.size());
        for (auto &&[id, decl] : scope) {
            println(indent(level) << "Identifier: " << id);
            decl->dump(level, true);
        }
        level++;
    }
}
void Sema::dump_modules_for_import() const {
    println("Modules for import:");
    for (auto &&[k, v] : m_modules_for_import) {
        println(indent(2) << "Module: " << k);
    }
}

bool Sema::insert_decl_to_current_scope(ResolvedDecl &decl) {
    debug_func(decl.identifier << " " << decl.location);
#ifdef DEBUG_SCOPES
    println("======================>>insert_decl_to_current_scope " << decl.identifier << " ======================");
#endif
    const auto foundDecl = lookup(decl.location, decl.identifier, false);

    if (foundDecl) {
#ifdef DEBUG_SCOPES
        dump_scopes();
#endif
        report(decl.location, "redeclaration of '" + decl.identifier + '\'');
        return false;
    }

    m_scopes.back().emplace(decl.identifier, &decl);
#ifdef DEBUG_SCOPES
    dump_scopes();
    println("======================<<insert_decl_to_current_scope " << decl.identifier << " ======================");
#endif
    return true;
}

bool Sema::insert_decl_to_module(ResolvedModuleDecl &moduleDecl, ptr<ResolvedDecl> decl) {
    [[maybe_unused]] auto declPtr = decl.get();
    debug_func(declPtr->identifier << " " << declPtr->location);
    auto it = std::find_if(moduleDecl.declarations.begin(), moduleDecl.declarations.end(),
                           [&](const ptr<ResolvedDecl> &d) { return decl->identifier == d->identifier; });
    if (it != moduleDecl.declarations.end()) {
        report(decl->location, "redeclaration of '" + decl->identifier + '\'');
        return false;
    }
    moduleDecl.declarations.emplace_back(std::move(decl));
    return true;
}

std::vector<ResolvedDecl *> Sema::collect_scope() {
    debug_func("");
    std::vector<ResolvedDecl *> out;
    size_t needSize = 0;
    for (auto &&scope : m_scopes) {
        needSize += scope.size();
    }

    out.reserve(needSize);

    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
        for (auto &&[declID, decl] : *it) {
            debug_msg("Collect scope " << declID);
            out.emplace_back(decl);
        }
    }
    return out;
}

ResolvedDecl *Sema::lookup(const SourceLocation &loc, const std::string_view id, bool needAddDeps) {
    debug_func(loc << " " << id);
#ifdef DEBUG_SCOPES
    println("---------------------->>lookup " << std::quoted(std::string(id)) << " ----------------------");
    dump_scopes();
    println("----------------------<<lookup " << std::quoted(std::string(id)) << " ----------------------");
#endif

    if (id == "@This") {
        if (m_currentStruct) {
            return m_currentStruct;
        } else {
            return report(loc, "unexpected use of @This outside a struct");
        }
    }

    std::string identifier(id);
    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
        auto it_decl = (*it).find(identifier);
        if (it_decl != (*it).end()) {
            // Delayed initialization if it was not initialized
            if (auto declStmt = dynamic_cast<ResolvedDeclStmt *>(it_decl->second)) {
                if (!declStmt->type) {
                    if (!resolve_decl_stmt_initialize(*declStmt)) return nullptr;
                }
            }
            if (needAddDeps) add_dependency(it_decl->second);
            return it_decl->second;
        }
    }

    if (m_currentModule) {
        return lookup_in_module(loc, *m_currentModule, id);
    }

    return nullptr;
}

ResolvedDecl *Sema::lookup_in_module(const SourceLocation &loc, const ResolvedModuleDecl &moduleDecl,
                                     const std::string_view id) {
    debug_func("Module: " << moduleDecl.identifier << " id: " << id);
    add_dependency(const_cast<ResolvedModuleDecl *>(&moduleDecl));
    for (auto &&decl : moduleDecl.declarations) {
        auto declPtr = decl.get();
        if (id != declPtr->identifier) continue;
        if (&moduleDecl != m_currentModule && !decl->isPublic) {
            report(loc, "cannot access private member '" + std::string(id) + "'");
            return report(decl->location, "'" + std::string(id) + "' must be marked as pub");
        }
        // Delayed initialization if it was not initialized
        if (auto declStmt = dynamic_cast<ResolvedDeclStmt *>(declPtr)) {
            if (!declStmt->type) {
                if (!resolve_decl_stmt_initialize(*declStmt)) return nullptr;
            }
        }
        add_dependency(declPtr);
        return declPtr;
    }
    return nullptr;
}

ResolvedDecl *Sema::lookup_in_struct(const SourceLocation &loc, const ResolvedStructDecl &structDecl,
                                     const std::string_view id) {
    debug_func("Struct " << structDecl.identifier << " " << id);
    add_dependency(const_cast<ResolvedStructDecl *>(&structDecl));
    for (auto &&decl : structDecl.functions) {
        if (id != decl->identifier) continue;
        if (&structDecl != m_currentStruct && !decl->isPublic) {
            report(loc, "cannot access private member '" + std::string(id) + "'");
            return report(decl->location, "'" + std::string(id) + "' must be marked as pub");
        }
        add_dependency(decl.get());
        return decl.get();
    }
    for (auto &&decl : structDecl.fields) {
        if (id != decl->identifier) continue;
        return decl.get();
    }
    return nullptr;
}

ptr<ResolvedType> Sema::resolve_type(const Expr &type) {
    ptr<ResolvedType> ret = nullptr;
    ResolvedType *retPtr = nullptr;
    debug_func("'" << type.to_str() << "' -> '" << (retPtr ? retPtr->to_str() : "nullptr") << "'");

    if (dynamic_cast<const TypeVoid *>(&type)) {
        ret = makePtr<ResolvedTypeVoid>(type.location);
        retPtr = ret.get();
        return ret;
    }
    if (dynamic_cast<const TypeBool *>(&type)) {
        ret = makePtr<ResolvedTypeBool>(type.location);
        retPtr = ret.get();
        return ret;
    }
    if (auto numType = dynamic_cast<const TypeNumber *>(&type)) {
        auto num = numType->name.substr(1);
        int bitSize = 0;
        auto res = std::from_chars(num.data(), num.data() + num.size(), bitSize);
        if (bitSize == 0 || res.ec != std::errc()) {
            return report(type.location, "unexpected size of 0 in i type");
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
        ret = makePtr<ResolvedTypeNumber>(type.location, kind, bitSize);
        retPtr = ret.get();
        return ret;
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
        return ret;
    }
    if (auto ptrType = dynamic_cast<const DerefPtrExpr *>(&type)) {
        varOrReturn(pointerType, resolve_type(*ptrType->expr));

        ret = makePtr<ResolvedTypePointer>(type.location, std::move(pointerType));
        retPtr = ret.get();
        return ret;
    }
    if (auto optType = dynamic_cast<const UnaryOperator *>(&type)) {
        if (optType->op != TokenType::op_excla_mark)
            return report(type.location, "unexpected op in unary operator of a type");
        varOrReturn(optionalType, resolve_type(*optType->operand));

        ret = makePtr<ResolvedTypeOptional>(type.location, std::move(optionalType));
        retPtr = ret.get();
        return ret;
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
            return report(arraySizeExpr->location, "cannot deduce array size");
        }

        ret = makePtr<ResolvedTypeArray>(type.location, std::move(arrayType), arraySize);
        retPtr = ret.get();
        return ret;
    }
    if (auto declRefType = dynamic_cast<const DeclRefExpr *>(&type)) {
        auto decl = lookup(type.location, declRefType->identifier);
        if (!decl) {
            dump_scopes();
            return report(declRefType->location, "symbol '" + declRefType->identifier + "' not found");
        }
        if (auto struDecl = dynamic_cast<ResolvedStructDecl *>(decl)) {
            ret = makePtr<ResolvedTypeStruct>(type.location, struDecl);
            retPtr = ret.get();
            return ret;
        }
        if (auto genDecl = dynamic_cast<ResolvedGenericTypeDecl *>(decl)) {
            if (genDecl->specializedType) {
                ret = genDecl->specializedType->clone();
                retPtr = ret.get();
                return ret;
            } else {
                ret = makePtr<ResolvedTypeGeneric>(type.location, genDecl);
                retPtr = ret.get();
                return ret;
            }
        }
        decl->dump();
        dmz_unreachable("TODO");
    }
    if (auto genType = dynamic_cast<const GenericExpr *>(&type)) {
        varOrReturn(specExpr, resolve_generic_expr(*genType));
        if (auto struDecl = dynamic_cast<ResolvedTypeStructDecl *>(specExpr->type.get())) {
            ret = makePtr<ResolvedTypeStruct>(type.location, struDecl->decl);
        } else {
            ret = specExpr->type->clone();
        }
        retPtr = ret.get();
        return ret;
    }
    if (auto memType = dynamic_cast<const MemberExpr *>(&type)) {
        varOrReturn(resolvedMem, resolve_member_expr(*memType));
        if (auto struDecl = dynamic_cast<ResolvedTypeStructDecl *>(resolvedMem->type.get())) {
            ret = makePtr<ResolvedTypeStruct>(type.location, struDecl->decl);
        } else {
            ret = resolvedMem->type->clone();
        }
        retPtr = ret.get();
        return ret;
    }

    type.dump();
    dmz_unreachable("TODO");
    (void)retPtr;
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
    debug_func(Dumper([&]() { type.dump(); }));
    if (auto genType = dynamic_cast<const ResolvedTypeGeneric *>(&type)) {
        if (genType->decl->specializedType) {
            return re_resolve_type(*genType->decl->specializedType);
        } else {
            return genType->clone();
        }
    }
    if (auto arrType = dynamic_cast<const ResolvedTypeArray *>(&type)) {
        return makePtr<ResolvedTypeArray>(arrType->location, re_resolve_type(*arrType->arrayType), arrType->arraySize);
    }
    if (auto optType = dynamic_cast<const ResolvedTypeOptional *>(&type)) {
        return makePtr<ResolvedTypeOptional>(optType->location, re_resolve_type(*optType->optionalType));
    }
    if (auto ptrType = dynamic_cast<const ResolvedTypePointer *>(&type)) {
        return makePtr<ResolvedTypePointer>(ptrType->location, re_resolve_type(*ptrType->pointerType));
    }
    if (type.kind == ResolvedTypeKind::Void || type.kind == ResolvedTypeKind::Number ||
        type.kind == ResolvedTypeKind::StructDecl || type.kind == ResolvedTypeKind::Struct ||
        type.kind == ResolvedTypeKind::ErrorGroup || type.kind == ResolvedTypeKind::Error ||
        type.kind == ResolvedTypeKind::Function) {
        return type.clone();
    }
    type.dump();
    dmz_unreachable("TODO");
}

bool Sema::resolve_import_modules(std::vector<ptr<ResolvedModuleDecl>> &out_resolvedModules) {
    debug_func("");
    auto &imported_modules = Driver::instance().imported_modules;
    std::vector<ptr<ModuleDecl>> moduleDecls;
    std::vector<std::filesystem::path> moduleDeclsPaths;
    for (auto &&[k, v] : imported_modules) {
        moduleDeclsPaths.emplace_back(std::move(k));
        moduleDecls.emplace_back(std::move(v));
    }
    imported_modules.clear();

    auto resolvedModuleDecl = resolve_modules_decls(moduleDecls);
    for (size_t i = 0; i < moduleDecls.size(); i++) {
        imported_modules.emplace(std::move(moduleDeclsPaths[i]), std::move(moduleDecls[i]));
    }

    if (resolvedModuleDecl.size() != moduleDecls.size()) return false;
    for (size_t i = 0; i < resolvedModuleDecl.size(); i++) {
        debug_msg("register module " << moduleDeclsPaths[i] << " " << resolvedModuleDecl[i]->name());
        m_modules_for_import.emplace(moduleDeclsPaths[i], resolvedModuleDecl[i].get());
    }
    out_resolvedModules = std::move(resolvedModuleDecl);
    return true;
}

std::vector<ptr<ResolvedModuleDecl>> Sema::resolve_ast_decl() {
    debug_func("");
    ScopedTimer(StatType::Semantic_Declarations);
    std::vector<ptr<ResolvedModuleDecl>> imported_mods;
    bool error = false;
    error = !resolve_import_modules(imported_mods);

    auto declarations = resolve_modules_decls(m_ast);

    if (error) return {};

    std::vector<ptr<ResolvedModuleDecl>> resolvedDecls;
    resolvedDecls.reserve(imported_mods.size() + declarations.size());

    for (auto &i : imported_mods) {
        resolvedDecls.emplace_back(std::move(i));
    }
    for (auto &i : declarations) {
        resolvedDecls.emplace_back(std::move(i));
    }

    return resolvedDecls;
}

bool Sema::resolve_ast_body(std::vector<ptr<ResolvedModuleDecl>> &moduleDecls) {
    debug_func("");
    ScopedTimer(StatType::Semantic_Body);
    bool error = false;
    for (auto &&module : moduleDecls) {
        auto ret = resolve_module_body(*module);
        if (!ret) {
            error = true;
        }
    }
    if (!error) {
        resolve_symbol_names(moduleDecls);
        fill_depends(moduleDecls);
    }
    return !error;
}

void Sema::fill_depends(std::vector<ptr<ResolvedModuleDecl>> &decls) {
    debug_func("");
    for (auto &&decl : decls) {
        debug_msg("ResolvedModuleDecl " << decl->name());
        fill_depends(decl.get(), decl->declarations);
    }
}

void Sema::fill_depends(ResolvedDependencies *parent, std::vector<ptr<ResolvedDecl>> &decls) {
    debug_func("parent " << (parent ? parent->name() : "nullptr"));
    for (auto &&decl : decls) {
        if (!dynamic_cast<ResolvedDependencies *>(decl.get())) continue;
        debug_msg(decl->name());

        if (auto md = dynamic_cast<ResolvedModuleDecl *>(decl.get())) {
            debug_msg("ResolvedModuleDecl " << md->name());
            fill_depends(md, md->declarations);
        }
        if (auto sd = dynamic_cast<ResolvedStructDecl *>(decl.get())) {
            debug_msg("ResolvedStructDecl " << sd->name());
            if (auto gen = dynamic_cast<ResolvedGenericStructDecl *>(decl.get())) {
                debug_msg("ResolvedGenericStructDecl " << gen->name());

                auto aux_decls = move_vector_ptr<ResolvedSpecializedStructDecl, ResolvedDecl>(gen->specializations);
                fill_depends(gen, aux_decls);
                gen->specializations = move_vector_ptr<ResolvedDecl, ResolvedSpecializedStructDecl>(aux_decls);
            } else {
                auto aux_decls = move_vector_ptr<ResolvedMemberFunctionDecl, ResolvedDecl>(sd->functions);
                fill_depends(sd, aux_decls);
                sd->functions = move_vector_ptr<ResolvedDecl, ResolvedMemberFunctionDecl>(aux_decls);
            }
        }
        if (auto gen = dynamic_cast<ResolvedGenericFunctionDecl *>(decl.get())) {
            debug_msg("ResolvedGenericFunctionDecl " << gen->name());

            auto aux_decls = move_vector_ptr<ResolvedSpecializedFunctionDecl, ResolvedDecl>(gen->specializations);
            fill_depends(gen, aux_decls);
            gen->specializations = move_vector_ptr<ResolvedDecl, ResolvedSpecializedFunctionDecl>(aux_decls);
        }
        if (dynamic_cast<ResolvedModuleDecl *>(parent)) {
            if (auto declStmt = dynamic_cast<ResolvedDeclStmt *>(decl.get())) {
                if (!declStmt->isMutable && (declStmt->type->kind == ResolvedTypeKind::StructDecl ||
                                             declStmt->type->kind == ResolvedTypeKind::Function ||
                                             declStmt->type->kind == ResolvedTypeKind::Module)) {
                    continue;
                }
            }
        }
        if (auto declStmt = dynamic_cast<ResolvedDeclStmt *>(decl.get())) {
            debug_msg("ResolvedDeclStmt " << declStmt->name());
            std::vector<ptr<ResolvedDecl>> varDecl;
            varDecl.reserve(1);
            varDecl.emplace_back(std::move(declStmt->varDecl));
            fill_depends(declStmt, varDecl);
            declStmt->varDecl = ptr<ResolvedVarDecl>(static_cast<ResolvedVarDecl *>(varDecl[0].release()));
        }
        if (auto deps = dynamic_cast<ResolvedDependencies *>(decl.get())) {
            debug_msg("ResolvedDependencies " << deps->name());
            if (parent) {
                parent->isUsedBy.emplace(deps);
                deps->dependsOn.emplace(parent);
            }
            continue;
        }
    }
}

bool Sema::recurse_needed(ResolvedDependencies &resolvedDeps, bool buildTest,
                          std::unordered_set<ResolvedDependencies *> &recurse_check) {
    bool ret = false;
    bool isReasonRecurse = false;
    debug_func(resolvedDeps.name() << " " << (ret ? "true" : "false"));
    defer([&]() {
        recurse_check.erase(&resolvedDeps);
        if (ret == false && isReasonRecurse == false) {
            debug_msg(resolvedDeps.name() << " cached not needed");
            resolvedDeps.cachedIsNotNeeded = true;
        }
    });

    if (m_removed_decls.find(&resolvedDeps) != m_removed_decls.end()) {
        debug_msg("ResolvedDecl is already removed");
        ret = false;
        return ret;
    }

    if (resolvedDeps.cachedIsNotNeeded) {
        debug_msg("ResolvedDecl cached is not needed");
        ret = false;
        return ret;
    }

    if (!buildTest && dynamic_cast<ResolvedTestDecl *>(&resolvedDeps)) {
        debug_msg("ResolvedDecl is a test and not necesary");
        ret = false;
        return ret;
    }

    if (auto modDecl = dynamic_cast<ResolvedModuleDecl *>(&resolvedDeps)) {
        if (modDecl->declarations.size() == 0) {
            debug_msg("ResolvedModuleDecl is empty");
            ret = false;
            return ret;
        }
    }

    if (resolvedDeps.identifier == "main") {
        debug_msg(resolvedDeps.name() << " is needed main");
        ret = true;
        return ret;
    }

    if (buildTest && resolvedDeps.identifier == "__builtin_main_test") {
        debug_msg(resolvedDeps.name() << " is needed buildTest or __builtin_main_test");
        ret = true;
        return ret;
    }

    size_t reasonRecurse = 0;
    for (auto &&decl : resolvedDeps.isUsedBy) {
        debug_msg(decl->name() << " in isUsedBy " << resolvedDeps.name());
        if (!recurse_check.emplace(decl).second) {
            debug_msg(resolvedDeps.name() << " is not needed recurse check");
            reasonRecurse += 1;
            continue;
        }
        if (recurse_needed(*decl, buildTest, recurse_check)) {
            debug_msg(decl->name() << " is needed recurse");
            ret = true;
            return ret;
        }
    }
    if (reasonRecurse > 0) {
        isReasonRecurse = true;
    }

    debug_msg(resolvedDeps.name() << " is not needed");
    ret = false;
    return ret;
}

void Sema::remove_unused(std::vector<ptr<ResolvedModuleDecl>> &moduleDecls, bool buildTest) {
    for (auto &&module : moduleDecls) {
        remove_unused(module->declarations, buildTest);
    }
}

void Sema::remove_unused(std::vector<ptr<ResolvedDecl>> &decls, bool buildTest) {
    debug_func("");

    auto add_to_remove = [&](ptr<DMZ::ResolvedDecl> &d) {
        if (auto deps = dynamic_cast<ResolvedDependencies *>(d.get())) {
            debug_msg_func("add_to_remove", deps->identifier);
            debug_msg_func("add_to_remove",
                           "Removing all " << deps->dependsOn.size() << " dependsOn of " << deps->identifier);
            for (auto &&decl : deps->dependsOn) {
                debug_msg_func("add_to_remove", "Removing " << deps->identifier << " from " << decl->identifier);
                if (!decl->isUsedBy.erase(deps)) {
                    debug_msg_func("add_to_remove", "Error erasing");
                }
            }
            deps->dependsOn.clear();

            debug_msg_func("add_to_remove",
                           "Removing all " << deps->isUsedBy.size() << " isUsedBy of " << deps->identifier);
            for (auto &&decl : deps->isUsedBy) {
                debug_msg_func("add_to_remove", "Removing " << deps->identifier << " from " << decl->identifier);
                if (!decl->dependsOn.erase(deps)) {
                    debug_msg_func("add_to_remove", "Error erasing");
                }
            }
            deps->isUsedBy.clear();
            m_removed_decls.emplace(deps);
            d.reset();
        } else {
            d->dump();
            dmz_unreachable("unexpected declaration");
        }
    };
    std::unordered_set<ResolvedDependencies *> recurse_check;
    for (auto &&decl : decls) {
        recurse_check.clear();
        if (!decl || m_removed_decls.find(decl.get()) != m_removed_decls.end()) {
            debug_msg("Continue in to_remove");
            continue;
        }
        if (auto md = dynamic_cast<ResolvedModuleDecl *>(decl.get())) {
            debug_msg("ModuleDecl " << md->identifier);
            remove_unused(md->declarations, buildTest);
            if (!recurse_needed(*md, buildTest, recurse_check)) {
                add_to_remove(decl);
            }
            continue;
        }
        if (auto sd = dynamic_cast<ResolvedStructDecl *>(decl.get())) {
            if (auto gen = dynamic_cast<ResolvedGenericStructDecl *>(decl.get())) {
                debug_msg("ResolvedGenericStructDecl " << gen->identifier);

                auto aux_decls = move_vector_ptr<ResolvedSpecializedStructDecl, ResolvedDecl>(gen->specializations);
                remove_unused(aux_decls, buildTest);
                gen->specializations = move_vector_ptr<ResolvedDecl, ResolvedSpecializedStructDecl>(aux_decls);

                if (!recurse_needed(*gen, buildTest, recurse_check)) {
                    add_to_remove(decl);
                }
                continue;
            }
            debug_msg("StructDecl " << sd->identifier);

            auto aux_decls = move_vector_ptr<ResolvedMemberFunctionDecl, ResolvedDecl>(sd->functions);
            remove_unused(aux_decls, buildTest);
            sd->functions = move_vector_ptr<ResolvedDecl, ResolvedMemberFunctionDecl>(aux_decls);

            if (!recurse_needed(*sd, buildTest, recurse_check)) {
                add_to_remove(decl);
            }
            continue;
        }
        if (auto fd = dynamic_cast<ResolvedFuncDecl *>(decl.get())) {
            if (auto gen = dynamic_cast<ResolvedGenericFunctionDecl *>(decl.get())) {
                debug_msg("ResolvedGenericFunctionDecl " << gen->identifier);

                auto aux_decls = move_vector_ptr<ResolvedSpecializedFunctionDecl, ResolvedDecl>(gen->specializations);
                remove_unused(aux_decls, buildTest);
                gen->specializations = move_vector_ptr<ResolvedDecl, ResolvedSpecializedFunctionDecl>(aux_decls);

                if (!recurse_needed(*gen, buildTest, recurse_check)) {
                    add_to_remove(decl);
                }
                continue;
            }

            debug_msg("FuncDecl " << fd->identifier);
            if (!recurse_needed(*fd, buildTest, recurse_check)) {
                add_to_remove(decl);
            }
            continue;
        }
        if (auto deps = dynamic_cast<ResolvedDependencies *>(decl.get())) {
            debug_msg("ResolvedDependencies " << deps->identifier);
            if (!recurse_needed(*deps, buildTest, recurse_check)) {
                add_to_remove(decl);
            }
            continue;
        }
    }

    std::erase_if(decls, [&](ptr<ResolvedDecl> &d) -> bool { return d ? false : true; });
}

bool Sema::run_flow_sensitive_checks(const ResolvedFuncDecl &fn) {
    debug_func(fn.location);
    const ResolvedBlock *block;
    if (auto resfn = dynamic_cast<const ResolvedFunctionDecl *>(&fn)) {
        block = resfn->body.get();
    } else if (auto resfn = dynamic_cast<const ResolvedSpecializedFunctionDecl *>(&fn)) {
        block = resfn->body.get();
    } else {
        dmz_unreachable("unexpected function");
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

    std::set<int> visited;
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

    using Lattice = std::map<const ResolvedDecl *, State>;

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
                    tmp[decl->varDecl.get()] = decl->varDecl->initializer ? State::Assigned : State::Unassigned;
                    continue;
                }

                if (auto *assignment = dynamic_cast<const ResolvedAssignment *>(stmt)) {
                    const ResolvedExpr *base = assignment->assignee.get();
                    while (const auto *member = dynamic_cast<const ResolvedMemberExpr *>(base))
                        base = member->base.get();

                    const auto *dre = dynamic_cast<const ResolvedDeclRefExpr *>(base);

                    // The base of the expression is not a variable, but a temporary,
                    // which can be mutated.
                    if (!dre) continue;

                    const auto *decl = dynamic_cast<const ResolvedDecl *>(&dre->decl);

                    if (!decl->isMutable && decl->type->kind != ResolvedTypeKind::Pointer &&
                        tmp[decl] != State::Unassigned) {
                        std::string msg = '\'' + decl->identifier + "' cannot be mutated";
                        pendingErrors.emplace_back(assignment->location, std::move(msg));
                    }

                    tmp[decl] = State::Assigned;
                    continue;
                }

                if (const auto *dre = dynamic_cast<const ResolvedDeclRefExpr *>(stmt)) {
                    if (const auto *var = dynamic_cast<const ResolvedVarDecl *>(&dre->decl)) {
                        if (var->initializer) {
                            tmp[var] = State::Assigned;
                        }

                        if (tmp[var] != State::Assigned) {
                            std::string msg = '\'' + var->identifier + "' is not initialized";
                            pendingErrors.emplace_back(dre->location, std::move(msg));
                        }
                    }

                    continue;
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

void Sema::resolve_symbol_names(const std::vector<ptr<ResolvedModuleDecl>> &declarations) {
    debug_func("");
    struct elem {
        ResolvedDecl *decl;
        int level;
        std::string symbol;
    };
    std::stack<elem> stack;
    for (auto &&decl : declarations) {
        debug_msg(indent(0) << "Symbol name: " << decl->name());

        if (auto *modDecl = dynamic_cast<ResolvedModuleDecl *>(decl.get())) {
            debug_msg(indent(0) << "module path: " << modDecl->module_path);
            modDecl->symbolName = modDecl->name();
            for (auto &&decl : modDecl->declarations) {
                stack.push(elem{decl.get(), 1, modDecl->symbolName + "."});
            }
        }
        if (!dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
            decl->dump();
            dmz_unreachable("unexpected declaration");
        }
    }

    while (!stack.empty()) {
        elem e = stack.top();
        stack.pop();

        e.decl->symbolName = e.symbol + e.decl->identifier;

        if (const auto *func = dynamic_cast<const ResolvedSpecializedFunctionDecl *>(e.decl)) {
            e.decl->symbolName += func->specializedTypes->to_str();
        }
        if (const auto *struc = dynamic_cast<const ResolvedSpecializedStructDecl *>(e.decl)) {
            e.decl->symbolName += struc->specializedTypes->to_str();
        }
        debug_msg(indent(e.level) << "Symbol name: " << e.decl->symbolName);
        // println(indent(e.level) << "Symbol identifier: " << e.decl->identifier);
        // println(indent(e.level) << "e Symbol: " << e.symbol);
        // println(indent(e.level) << "Symbol name: " << e.decl->symbolName);
        // e.decl->dump();
        // std::cout << indent(e.level) << e.decl->symbolName << std::endl;

        std::string new_symbol_name = e.decl->symbolName + ".";
        // if (e.decl->symbolName.find(".dmz") == std::string::npos) {
        // new_symbol_name = e.decl->symbolName + ".";
        // }

        if (const auto *modDecl = dynamic_cast<const ResolvedModuleDecl *>(e.decl)) {
            for (auto &&decl : modDecl->declarations) {
                stack.push(elem{decl.get(), e.level + 1, new_symbol_name});
            }
        } else if (const auto *strDecl = dynamic_cast<const ResolvedStructDecl *>(e.decl)) {
            for (auto &&decl : strDecl->functions) {
                stack.push(elem{decl.get(), e.level + 1, new_symbol_name});
            }
            if (const auto *genStrDecl = dynamic_cast<const ResolvedGenericStructDecl *>(e.decl)) {
                for (auto &&decl : genStrDecl->specializations) {
                    stack.push(elem{decl.get(), e.level + 1, e.symbol});
                }
            }
        } else if (const auto *ErrorGroupExprDecl = dynamic_cast<const ResolvedErrorGroupExprDecl *>(e.decl)) {
            for (auto &&decl : ErrorGroupExprDecl->errors) {
                stack.push(elem{decl.get(), e.level + 1, new_symbol_name});
            }
        } else if (const auto *genDecl = dynamic_cast<const ResolvedGenericFunctionDecl *>(e.decl)) {
            for (auto &&decl : genDecl->specializations) {
                stack.push(elem{decl.get(), e.level + 1, e.symbol});
            }
        } else if (dynamic_cast<const ResolvedDeclStmt *>(e.decl) || dynamic_cast<const ResolvedFuncDecl *>(e.decl) ||
                   dynamic_cast<const ResolvedErrorDecl *>(e.decl) || dynamic_cast<const ResolvedTestDecl *>(e.decl)) {
        } else {
            e.decl->dump(0, true);
            dmz_unreachable("TODO: unexpected declaration");
        }
    }
}

void Sema::add_dependency(ResolvedDecl *decl) {
    debug_func("Adding " << decl->identifier << " " << decl);
    ResolvedDependencies *declDep = nullptr;
    if (!decl->isDependency) return;
    auto dep = static_cast<ResolvedDependencies *>(decl);

    if (!decl->isMutable &&
        (decl->type->kind == ResolvedTypeKind::Module || decl->type->kind == ResolvedTypeKind::StructDecl ||
         decl->type->kind == ResolvedTypeKind::Function || decl->type->kind == ResolvedTypeKind::ErrorGroup)) {
        if (auto modType = dynamic_cast<ResolvedTypeModule *>(decl->type.get())) {
            declDep = dynamic_cast<ResolvedDependencies *>(modType->moduleDecl);
        } else if (auto strType = dynamic_cast<ResolvedTypeStructDecl *>(decl->type.get())) {
            declDep = dynamic_cast<ResolvedDependencies *>(strType->decl);
        } else if (auto fnType = dynamic_cast<ResolvedTypeFunction *>(decl->type.get())) {
            declDep = dynamic_cast<ResolvedDependencies *>(fnType->fnDecl);
        } else if (auto egType = dynamic_cast<ResolvedTypeErrorGroup *>(decl->type.get())) {
            declDep = dynamic_cast<ResolvedDependencies *>(egType->decl);
        }
    }

    debug_msg("m_currentFunction " << m_currentFunction << " m_currentModule " << m_currentModule << " m_currentStruct "
                                   << m_currentStruct);
    if (m_currentFunction) {
        debug_msg("Adding " << dep->name() << " to function " << m_currentFunction->name());
        m_currentFunction->dependsOn.emplace(dep);
        dep->isUsedBy.emplace(m_currentFunction);

        if (declDep) {
            debug_msg("Adding " << declDep->name() << " to function " << m_currentFunction->name());
            m_currentFunction->dependsOn.emplace(declDep);
            declDep->isUsedBy.emplace(m_currentFunction);
        }
    }
    if (m_currentModule) {
        debug_msg("Adding " << dep->name() << " to module " << m_currentModule->name());
        m_currentModule->dependsOn.emplace(dep);
        dep->isUsedBy.emplace(m_currentModule);

        if (declDep) {
            debug_msg("Adding " << declDep->name() << " to module " << m_currentModule->name());
            m_currentModule->dependsOn.emplace(declDep);
            declDep->isUsedBy.emplace(m_currentModule);
        }
    }
    if (m_currentStruct) {
        debug_msg("Adding " << decl->name() << " to struct " << m_currentModule->name());
        m_currentStruct->dependsOn.emplace(dep);
        dep->isUsedBy.emplace(m_currentStruct);

        if (declDep) {
            debug_msg("Adding " << declDep->name() << " to struct " << m_currentModule->name());
            m_currentStruct->dependsOn.emplace(declDep);
            declDep->isUsedBy.emplace(m_currentStruct);
        }
    }
}
}  // namespace DMZ