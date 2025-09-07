// #define DEBUG
#include "semantic/Semantic.hpp"

#include "Stats.hpp"

// #define DEBUG_SCOPES
#ifdef DEBUG
#define DEBUG_SCOPES
#endif

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
    const auto foundDecl = lookup(decl.location, decl.identifier, ResolvedDeclType::ResolvedDecl, false);

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

std::vector<ResolvedDecl *> Sema::collect_scope() {
    std::vector<ResolvedDecl *> out;
    size_t needSize = 0;
    for (auto &&scope : m_scopes) {
        needSize += scope.size();
    }

    out.reserve(needSize);

    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
        for (auto &&[declID, decl] : *it) {
            out.emplace_back(decl);
        }
    }
    return out;
}

#define switch_resolved_decl_type(type, decl, bool, expresion)                    \
    switch (type) {                                                               \
        case ResolvedDeclType::Module:                                            \
            break;                                                                \
        case ResolvedDeclType::ResolvedDecl:                                      \
            if (bool dynamic_cast<ResolvedDecl *>(decl)) expresion;               \
            break;                                                                \
        case ResolvedDeclType::ResolvedErrorDecl:                                 \
            if (bool dynamic_cast<ResolvedErrorDecl *>(decl)) expresion;          \
            break;                                                                \
        case ResolvedDeclType::ResolvedImportExpr:                                \
            if (bool dynamic_cast<ResolvedImportExpr *>(decl)) expresion;         \
            break;                                                                \
        case ResolvedDeclType::ResolvedMemberFunctionDecl:                        \
            if (bool dynamic_cast<ResolvedMemberFunctionDecl *>(decl)) expresion; \
            break;                                                                \
        case ResolvedDeclType::ResolvedModuleDecl:                                \
            if (bool dynamic_cast<ResolvedModuleDecl *>(decl)) expresion;         \
            break;                                                                \
        case ResolvedDeclType::ResolvedStructDecl:                                \
            if (bool dynamic_cast<ResolvedStructDecl *>(decl)) expresion;         \
            break;                                                                \
        case ResolvedDeclType::ResolvedGenericTypeDecl:                           \
            if (bool dynamic_cast<ResolvedGenericTypeDecl *>(decl)) expresion;    \
            break;                                                                \
        case ResolvedDeclType::ResolvedFieldDecl:                                 \
            if (bool dynamic_cast<ResolvedFieldDecl *>(decl)) expresion;          \
            break;                                                                \
    }

ResolvedDecl *Sema::lookup(const SourceLocation &loc, const std::string_view id, ResolvedDeclType type,
                           bool needAddDeps) {
    debug_func(id << " " << type);
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

    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
        for (auto &&[declID, decl] : *it) {
            switch_resolved_decl_type(type, decl, !, continue);
            // debug_msg("check " << decl->identifier << " " << identifier);
            if (declID != id) continue;
            if (needAddDeps) add_dependency(decl);
            // debug_msg("Found");
            return decl;
        }
    }
    return nullptr;
}

ResolvedDecl *Sema::lookup_in_module(const ResolvedModuleDecl &moduleDecl, const std::string_view id,
                                     ResolvedDeclType type) {
    debug_func("Module: " << moduleDecl.identifier << " id: " << id << " type: " << type);
    add_dependency(const_cast<ResolvedModuleDecl *>(&moduleDecl));
    // println("m_currentModule '" << m_currentModule->identifier << "' Lookup in '" << moduleDecl.identifier
    //                            << "' look for '" << id << "' type: " << type);

    for (auto &&decl : moduleDecl.declarations) {
        debug_msg("Seach: " << decl->identifier);
        auto declPtr = decl.get();
        switch_resolved_decl_type(type, declPtr, !, continue);

        if (id != declPtr->identifier) continue;
        add_dependency(declPtr);
        return declPtr;
    }
    return nullptr;
}

ResolvedDecl *Sema::lookup_in_struct(const ResolvedStructDecl &structDecl, const std::string_view id,
                                     ResolvedDeclType type) {
    debug_func("Struct " << structDecl.identifier << " " << id << " " << type);
    add_dependency(const_cast<ResolvedStructDecl *>(&structDecl));
    if (type == ResolvedDeclType::ResolvedDecl || type == ResolvedDeclType::ResolvedMemberFunctionDecl) {
        for (auto &&decl : structDecl.functions) {
            if (id != decl->identifier) continue;
            add_dependency(decl.get());
            return decl.get();
        }
    }
    if (type == ResolvedDeclType::ResolvedDecl || type == ResolvedDeclType::ResolvedFieldDecl) {
        for (auto &&decl : structDecl.fields) {
            if (id != decl->identifier) continue;
            return decl.get();
        }
    }
    if (type != ResolvedDeclType::ResolvedDecl && type != ResolvedDeclType::ResolvedMemberFunctionDecl &&
        type != ResolvedDeclType::ResolvedFieldDecl) {
        std::stringstream msg;
        msg << "Unexpected type " << type << "in lookup_in_struct";
        dmz_unreachable(msg.str().c_str());
    }
    return nullptr;
}

ptr<ResolvedType> Sema::resolve_type(const Type &parsedType) {
    ptr<ResolvedType> ret = nullptr;
    debug_func("'" << parsedType << "' -> '" << (ret.has_value() ? retCopy.to_str() : "nullopt") << "'");
    switch (parsedType.kind) {
        case Type::Kind::Custom: {
            if (parsedType.name == "@This") {
                if (m_currentStruct == nullptr) {
                    report(parsedType.location, "unexpected use of @This outside a struct");
                    ret = nullptr;
                    return ret;
                }
                ret = makePtr<ResolvedTypeStruct>(parsedType.location, m_currentStruct);
                return ret;
            }

            if (auto structDecl = cast_lookup(parsedType.location, parsedType.name, ResolvedStructDecl)) {
                if (auto genStructDecl = dynamic_cast<ResolvedGenericStructDecl *>(structDecl)) {
                    auto specializedTypes =
                        resolve_specialized_type(parsedType.location, *structDecl, *parsedType.genericTypes);
                    if (!specializedTypes) return report(parsedType.location, "cannot specialize generic types");
                    auto auxstructDecl =
                        specialize_generic_struct(parsedType.location, *genStructDecl, *specializedTypes);
                    if (auxstructDecl) structDecl = auxstructDecl;
                }
                ret = makePtr<ResolvedTypeStruct>(parsedType.location, structDecl);
                return ret;
            }
            if (auto decl = cast_lookup(parsedType.location, parsedType.name, ResolvedGenericTypeDecl)) {
                if (decl->specializedType) {
                    ret = re_resolve_type(*decl->specializedType);
                    return ret;
                } else {
                    ret = makePtr<ResolvedTypeGeneric>(parsedType.location, decl);
                    return ret;
                }
            }
#ifdef DEBUG
            dump_scopes();
#endif
            ret = report(parsedType.location, "cannot resolve type '" + parsedType.to_str() + "'");
            return ret;
        } break;

        case Type::Kind::Generic: {
            if (auto decl = cast_lookup(parsedType.location, parsedType.name, ResolvedGenericTypeDecl)) {
                if (decl->specializedType) {
                    ret = re_resolve_type(*decl->specializedType);
                }
            }
        } break;
        case Type::Kind::Int: {
            ret = makePtr<ResolvedTypeNumber>(parsedType.location, ResolvedNumberKind::Int, parsedType.size);
        } break;
        case Type::Kind::UInt: {
            ret = makePtr<ResolvedTypeNumber>(parsedType.location, ResolvedNumberKind::UInt, parsedType.size);
        } break;
        case Type::Kind::Bool: {
            ret = makePtr<ResolvedTypeBool>(parsedType.location);
        } break;
        case Type::Kind::Float: {
            ret = makePtr<ResolvedTypeNumber>(parsedType.location, ResolvedNumberKind::Float, parsedType.size);
        } break;
        case Type::Kind::Void: {
            ret = makePtr<ResolvedTypeVoid>(parsedType.location);
        } break;
        case Type::Kind::Error: {
            ret = makePtr<ResolvedTypeError>(parsedType.location);
        } break;
        case Type::Kind::Struct:
        case Type::Kind::ErrorGroup:
        case Type::Kind::Module:
            parsedType.dump();
            dmz_unreachable("Unsuported type");
            break;
    }
    if (parsedType.isPointer) {
        ret = makePtr<ResolvedTypePointer>(ret->location, std::move(ret));
    }
    if (parsedType.isArray) {
        ret = makePtr<ResolvedTypeArray>(ret->location, std::move(ret), *parsedType.isArray);
    }
    if (parsedType.isOptional) {
        ret = makePtr<ResolvedTypeOptional>(ret->location, std::move(ret));
    }
    return ret;
}

ptr<ResolvedTypeSpecialized> Sema::resolve_specialized_type(SourceLocation location, const ResolvedDecl &genericDecl,
                                                            const GenericTypes &parsedType) {
    std::vector<ptr<ResolvedType>> specializedTypes;

    for (auto &&t : parsedType.types) {
        varOrReturn(resType, resolve_type(*t));
        specializedTypes.emplace_back(std::move(resType));
    }

    return makePtr<ResolvedTypeSpecialized>(location, genericDecl.type->clone(), std::move(specializedTypes));
}

ptr<ResolvedType> Sema::re_resolve_type(const ResolvedType &type) {
    if (dynamic_cast<const ResolvedTypeVoid *>(&type) || dynamic_cast<const ResolvedTypeNumber *>(&type) ||
        dynamic_cast<const ResolvedTypeStruct *>(&type) || dynamic_cast<const ResolvedTypeArray *>(&type) ||
        dynamic_cast<const ResolvedTypeErrorGroup *>(&type) || dynamic_cast<const ResolvedTypeError *>(&type) ||
        dynamic_cast<const ResolvedTypeOptional *>(&type) || dynamic_cast<const ResolvedTypePointer *>(&type)) {
        return type.clone();
    }
    type.dump();
    dmz_unreachable("TODO");
}

std::vector<ptr<ResolvedDecl>> Sema::resolve_ast_decl() {
    debug_func("");
    ScopedTimer(StatType::Semantic_Declarations);
    std::vector<ptr<Decl>> decls;
    decls.reserve(m_ast.size());

    for (auto &moduleDeclPtr : m_ast) {
        decls.emplace_back(std::move(moduleDeclPtr));
    }
    m_ast.clear();

    auto declarations = resolve_in_module_decl(decls);

    for (auto &moduleDeclPtr : decls) {
        m_ast.emplace_back(static_cast<ModuleDecl *>(moduleDeclPtr.release()));
    }
    decls.clear();
    return declarations;
}

bool Sema::resolve_ast_body(std::vector<ptr<ResolvedDecl>> &decls) {
    debug_func("");
    ScopedTimer(StatType::Semantic_Body);
    auto ret = resolve_in_module_body(decls);

    resolve_symbol_names(decls);
    fill_depends(nullptr, decls);
    return ret;
}

void Sema::fill_depends(ResolvedDependencies *parent, std::vector<ptr<ResolvedDecl>> &decls) {
    debug_func("");
    // auto add_deps = [parent](ref<DMZ::ResolvedDecl> &d) {
    //     if (parent == nullptr) return;
    //     debug_msg_func("add_deps", d->identifier);

    //     debug_msg_func("add_deps", "Add all " << d->dependsOn.size() << " dependsOn of " << d->identifier);
    //     for (auto &&decl : d->dependsOn) {
    //         debug_msg_func("add_to_remove", "Adding " << parent->identifier << " to " << decl->identifier);
    //         parent->dependsOn.emplace(decl);
    //         decl->isUsedBy.emplace(parent);
    //     }
    // };
    for (auto &&decl : decls) {
        auto deps = dynamic_cast<ResolvedDependencies *>(decl.get());
        if (!deps) continue;
        if (auto md = dynamic_cast<ResolvedModuleDecl *>(decl.get())) {
            debug_msg("ModuleDecl " << md->identifier);
            fill_depends(md, md->declarations);
            // add_deps(decl);
            continue;
        }
        if (auto sd = dynamic_cast<ResolvedStructDecl *>(decl.get())) {
            debug_msg("StructDecl " << sd->identifier);
            std::vector<ptr<ResolvedDecl>> aux_decls;
            decls.reserve(sd->functions.size());

            for (auto &fnDecl : sd->functions) {
                aux_decls.emplace_back(std::move(fnDecl));
            }
            sd->functions.clear();
            fill_depends(sd, aux_decls);

            for (auto &fnDecl : aux_decls) {
                sd->functions.emplace_back(dynamic_cast<ResolvedMemberFunctionDecl *>(fnDecl.release()));
            }
            aux_decls.clear();

            // add_deps(decl);
            continue;
        }
        if (auto gen = dynamic_cast<ResolvedGenericFunctionDecl *>(decl.get())) {
            debug_msg("ResolvedFunctionDecl " << gen->identifier);
            std::vector<ptr<ResolvedDecl>> aux_decls;
            decls.reserve(gen->specializations.size());

            for (auto &fnDecl : gen->specializations) {
                aux_decls.emplace_back(std::move(fnDecl));
            }
            gen->specializations.clear();
            fill_depends(gen, aux_decls);

            for (auto &fnDecl : aux_decls) {
                gen->specializations.emplace_back(dynamic_cast<ResolvedSpecializedFunctionDecl *>(fnDecl.release()));
            }
            aux_decls.clear();
        }
        if (auto deps = dynamic_cast<ResolvedDependencies *>(decl.get())) {
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
    debug_func(resolvedDeps.identifier);
    if (!recurse_check.emplace(&resolvedDeps).second) {
        debug_msg(resolvedDeps.identifier << " is not needed recurse check");
        return false;
    }
    defer([&]() { recurse_check.erase(&resolvedDeps); });

    if (m_removed_decls.find(&resolvedDeps) != m_removed_decls.end()) {
        debug_msg("ResolvedDecl is already removed");
        return false;
    }

    if (!buildTest && dynamic_cast<ResolvedTestDecl *>(&resolvedDeps)) {
        debug_msg("ResolvedDecl is a test and not necesary");
        return false;
    }

    if (resolvedDeps.identifier.find(".dmz") != std::string::npos || resolvedDeps.identifier == "main") {
        debug_msg(resolvedDeps.identifier << " is needed");
        return true;
    }

    if (buildTest && resolvedDeps.identifier == "__builtin_main_test") {
        debug_msg(resolvedDeps.identifier << " is needed");
        return true;
    }

    for (auto &&decl : resolvedDeps.isUsedBy) {
        if (recurse_needed(*decl, buildTest, recurse_check)) {
            debug_msg(decl->identifier << " is needed");
            return true;
        }
    }

    debug_msg(resolvedDeps.identifier << " is not needed");
    return false;
}

void Sema::remove_unused(std::vector<ptr<ResolvedDecl>> &decls, bool buildTest) {
    debug_func("");

    std::unordered_set<ResolvedDependencies *> recurse_check;
    auto add_to_remove = [this](ResolvedDependencies *d) {
        debug_msg_func("add_to_remove", d->identifier);
        // to_remove.emplace(d);
        debug_msg_func("add_to_remove", "Removing all " << d->dependsOn.size() << " dependsOn of " << d->identifier);
        for (auto &&decl : d->dependsOn) {
            debug_msg_func("add_to_remove", "Removing " << d->identifier << " from " << decl->identifier);
            if (!decl->isUsedBy.erase(d)) {
                debug_msg_func("add_to_remove", "Error erasing");
            }
        }
        d->dependsOn.clear();

        debug_msg_func("add_to_remove", "Removing all " << d->isUsedBy.size() << " isUsedBy of " << d->identifier);
        for (auto &&decl : d->isUsedBy) {
            debug_msg_func("add_to_remove", "Removing " << d->identifier << " from " << decl->identifier);
            if (!decl->dependsOn.erase(d)) {
                debug_msg_func("add_to_remove", "Error erasing");
            }
        }
        d->isUsedBy.clear();
        m_removed_decls.emplace(d);
    };
    auto add_to_remove_smart = [&](ptr<DMZ::ResolvedDecl> &d) {
        if (auto deps = dynamic_cast<ResolvedDependencies *>(d.get())) {
            add_to_remove(deps);
            d.reset();
        } else {
            d->dump();
            dmz_unreachable("unexpected declaration");
        }
    };
    for (auto &&decl : decls) {
        if (!decl || m_removed_decls.find(decl.get()) != m_removed_decls.end()) {
            // if (to_remove.find(decl.get()) != to_remove.end()) {
            debug_msg("Continue in to_remove");
            continue;
        }
        if (auto md = dynamic_cast<ResolvedModuleDecl *>(decl.get())) {
            debug_msg("ModuleDecl " << md->identifier);
            remove_unused(md->declarations, buildTest);
            if (!recurse_needed(*md, buildTest, recurse_check)) {
                add_to_remove_smart(decl);
            }
            continue;
        }
        if (auto sd = dynamic_cast<ResolvedStructDecl *>(decl.get())) {
            debug_msg("StructDecl " << sd->identifier);
            std::vector<ptr<ResolvedDecl>> aux_decls;
            decls.reserve(sd->functions.size());

            for (auto &fnDecl : sd->functions) {
                aux_decls.emplace_back(std::move(fnDecl));
            }
            sd->functions.clear();
            remove_unused(aux_decls, buildTest);

            for (auto &fnDecl : aux_decls) {
                sd->functions.emplace_back(dynamic_cast<ResolvedMemberFunctionDecl *>(fnDecl.release()));
            }
            aux_decls.clear();

            if (!recurse_needed(*sd, buildTest, recurse_check)) {
                add_to_remove_smart(decl);
            }
            continue;
        }
        if (auto fd = dynamic_cast<ResolvedFuncDecl *>(decl.get())) {
            debug_msg("FuncDecl " << fd->identifier);
            if (!recurse_needed(*fd, buildTest, recurse_check)) {
                add_to_remove_smart(decl);
                continue;
            }
        }
    }

    // for (auto &&d : to_remove) {
    //     debug_msg("Cleaning " << d->identifier);
    //     for (auto &&decl : d->dependsOn) {
    //         decl->isUsedBy.erase(d);
    //     }
    //     d->dependsOn.clear();
    // }

    // std::erase_if(decls,
    //   [&](ref<ResolvedDecl> &d) -> bool { return to_remove.find(d.get()) != to_remove.end(); });
    std::erase_if(decls, [&](ptr<ResolvedDecl> &d) -> bool { return d ? false : true; });
}

bool Sema::run_flow_sensitive_checks(const ResolvedFuncDecl &fn) {
    debug_func(fn.location);
    const ResolvedBlock *block;
    if (auto resfn = dynamic_cast<const ResolvedFunctionDecl *>(&fn)) {
        block = resfn->body.get();
        // } else if (auto resfn = dynamic_cast<const ResolvedMemberFunctionDecl *>(&fn)) {
        //     block = resfn->function->body.get();
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
    auto optType = dynamic_cast<const ResolvedTypeOptional *>(fn.type.get());
    if (dynamic_cast<const ResolvedTypeVoid *>(fn.type.get()) ||
        (optType && dynamic_cast<const ResolvedTypeVoid *>(optType->optionalType.get())))
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
    std::unordered_map<State, std::string_view> state_to_string = {
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

                    if (!decl->isMutable && !dynamic_cast<const ResolvedTypePointer *>(decl->type.get()) &&
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

void Sema::resolve_symbol_names(const std::vector<ptr<ResolvedDecl>> &declarations) {
    debug_func("");
    struct elem {
        ResolvedDecl *decl;
        int level;
        std::string symbol;
    };
    std::stack<elem> stack;
    for (auto &&decl : declarations) {
        decl->symbolName = decl->identifier;
        stack.push(elem{decl.get(), 0, ""});
        // println("Symbol name: " << e.decl->symbolName);
        // if (dynamic_cast<const ResolvedModuleDecl *>(decl.get()) ||
        //     dynamic_cast<const ResolvedStructDecl *>(decl.get())) {
        //     stack.push(elem{decl.get(), 0, ""});
        // } else if (dynamic_cast<const ResolvedDeclStmt *>(decl.get()) ||
        //            dynamic_cast<const ResolvedFuncDecl *>(decl.get()) ||
        //            dynamic_cast<const ResolvedErrorGroupExprDecl *>(decl.get())) {
        // } else {
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
        // println(indent(e.level) << "Symbol identifier: " << e.decl->identifier);
        // println(indent(e.level) << "e Symbol: " << e.symbol);
        // println(indent(e.level) << "Symbol name: " << e.decl->symbolName);
        // e.decl->dump();
        // std::cout << indent(e.level) << e.decl->symbolName << std::endl;

        std::string new_symbol_name = "";
        if (e.decl->symbolName.find(".dmz") == std::string::npos) {
            new_symbol_name = e.decl->symbolName + ".";
        }

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
    debug_func("Adding " << decl->identifier);
    auto dep = dynamic_cast<ResolvedDependencies *>(decl);
    if (!dep) return;

    if (m_currentFunction) {
        debug_msg("Adding " << decl->identifier << " to function " << m_currentFunction->identifier);
        m_currentFunction->dependsOn.emplace(dep);
        dep->isUsedBy.emplace(m_currentFunction);
    }
    if (m_currentModule) {
        debug_msg("Adding " << decl->identifier << " to module " << m_currentModule->identifier);
        m_currentModule->dependsOn.emplace(dep);
        dep->isUsedBy.emplace(m_currentModule);
    }
    if (m_currentStruct) {
        debug_msg("Adding " << decl->identifier << " to struct " << m_currentModule->identifier);
        m_currentStruct->dependsOn.emplace(dep);
        dep->isUsedBy.emplace(m_currentStruct);
    }
}
}  // namespace DMZ