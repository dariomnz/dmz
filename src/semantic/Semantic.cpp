// #define DEBUG
#include "semantic/Semantic.hpp"

#include "Stats.hpp"

// #define DEBUG_SCOPES

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
    const auto &[foundDecl, scopeIdx] = lookup(decl.identifier, ResolvedDeclType::ResolvedDecl, false);

    if (foundDecl) {
        dump_scopes();
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

std::pair<ResolvedDecl *, int> Sema::lookup(const std::string_view id, ResolvedDeclType type, bool needAddDeps) {
    debug_func(id << " " << type);
#ifdef DEBUG_SCOPES
    println("---------------------->>lookup " << std::quoted(std::string(id)) << " ----------------------");
    dump_scopes();
    println("----------------------<<lookup " << std::quoted(std::string(id)) << " ----------------------");
#endif
    int scopeIdx = 0;
    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
        for (auto &&[declID, decl] : *it) {
            switch_resolved_decl_type(type, decl, !, continue);
            // debug_msg("check " << decl->identifier << " " << identifier);
            if (declID != id) continue;
            if (needAddDeps) add_dependency(decl);
            // debug_msg("Found");
            return {decl, scopeIdx};
        }

        ++scopeIdx;
    }
    return {nullptr, -1};
}

ResolvedDecl *Sema::lookup_in_module(const ResolvedModuleDecl &moduleDecl, const std::string_view id,
                                     ResolvedDeclType type) {
    debug_func("Module: " << moduleDecl.identifier << " id: " << id << " type: " << type);
    add_dependency(const_cast<ResolvedModuleDecl *>(&moduleDecl));
    // println("m_actualModule '" << m_actualModule->identifier << "' Lookup in '" << moduleDecl.identifier
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

std::optional<Type> Sema::resolve_type(Type parsedType) {
    debug_func(parsedType);
    if (parsedType.kind == Type::Kind::Custom) {
        if (auto structDecl = cast_lookup(parsedType.name, ResolvedStructDecl)) {
            return Type::structType(parsedType, structDecl);
        }
        if (auto decl = lookup((parsedType.name), ResolvedDeclType::ResolvedGenericTypeDecl).first) {
            auto resolvedGenericTypeDecl = dynamic_cast<ResolvedGenericTypeDecl *>(decl);
            if (resolvedGenericTypeDecl->specializedType) {
                return resolvedGenericTypeDecl->specializedType;
            } else {
                return Type::genericType(parsedType);
            }
        }

        return std::nullopt;
    }

    return parsedType;
}

std::vector<std::unique_ptr<ResolvedDecl>> Sema::resolve_ast_decl() {
    debug_func("");
    ScopedTimer(StatType::Semantic_Declarations);
    std::vector<std::unique_ptr<Decl>> decls;
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
    resolve_symbol_names(declarations);
    return declarations;
}

bool Sema::resolve_ast_body(std::vector<std::unique_ptr<ResolvedDecl>> &decls) {
    debug_func("");
    ScopedTimer(StatType::Semantic_Body);
    auto ret = resolve_in_module_body(decls);

    fill_depends(nullptr, decls);
    return ret;
}

void Sema::fill_depends(ResolvedDependencies *parent, std::vector<std::unique_ptr<ResolvedDecl>> &decls) {
    debug_func("");
    // auto add_deps = [parent](std::unique_ptr<DMZ::ResolvedDecl> &d) {
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
            std::vector<std::unique_ptr<ResolvedDecl>> aux_decls;
            decls.reserve(sd->functions.size());

            for (auto &fnDecl : sd->functions) {
                aux_decls.emplace_back(std::move(fnDecl));
            }
            sd->functions.clear();
            fill_depends(sd, aux_decls);

            for (auto &fnDecl : aux_decls) {
                sd->functions.emplace_back(static_cast<ResolvedMemberFunctionDecl *>(fnDecl.release()));
            }
            aux_decls.clear();

            // add_deps(decl);
            continue;
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

bool Sema::recurse_needed(ResolvedDependencies &resolvedDeps, bool buildTest) {
    debug_func(resolvedDeps.identifier);
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
        if (recurse_needed(*decl, buildTest)) {
            debug_msg(decl.identifier << " is needed");
            return true;
        }
    }

    debug_msg(decl.identifier << " is not needed");
    return false;
}

void Sema::remove_unused(std::vector<std::unique_ptr<ResolvedDecl>> &decls, bool buildTest) {
    debug_func("");

    // std::unordered_set<ResolvedDecl *> to_remove;
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
    auto add_to_remove_smart = [&](std::unique_ptr<DMZ::ResolvedDecl> &d) {
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
            if (!recurse_needed(*md, buildTest)) {
                add_to_remove_smart(decl);
            }
            continue;
        }
        if (auto sd = dynamic_cast<ResolvedStructDecl *>(decl.get())) {
            debug_msg("StructDecl " << sd->identifier);
            std::vector<std::unique_ptr<ResolvedDecl>> aux_decls;
            decls.reserve(sd->functions.size());

            for (auto &fnDecl : sd->functions) {
                aux_decls.emplace_back(std::move(fnDecl));
            }
            sd->functions.clear();
            remove_unused(aux_decls, buildTest);

            for (auto &fnDecl : aux_decls) {
                sd->functions.emplace_back(static_cast<ResolvedMemberFunctionDecl *>(fnDecl.release()));
            }
            aux_decls.clear();

            if (!recurse_needed(*sd, buildTest)) {
                add_to_remove_smart(decl);
            }
            continue;
        }
        if (auto fd = dynamic_cast<ResolvedFuncDecl *>(decl.get())) {
            debug_msg("FuncDecl " << fd->identifier);
            if (!recurse_needed(*fd, buildTest)) {
                add_to_remove_smart(decl);
                continue;
            }
        }
        if (auto td = dynamic_cast<ResolvedTestDecl *>(decl.get())) {
            debug_msg("TestDecl " << td->identifier);
            if (!recurse_needed(*td->testFunction, buildTest)) {
                add_to_remove(td->testFunction.get());
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
    //   [&](std::unique_ptr<ResolvedDecl> &d) -> bool { return to_remove.find(d.get()) != to_remove.end(); });
    std::erase_if(decls, [&](std::unique_ptr<ResolvedDecl> &d) -> bool { return d ? false : true; });
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
    if (fn.type.kind == Type::Kind::Void) return false;

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

                    if (!decl->isMutable && !decl->type.isRef && !decl->type.isPointer &&
                        tmp[decl] != State::Unassigned) {
                        std::string msg = '\'' + std::string(decl->identifier) + "' cannot be mutated";
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
                            std::string msg = '\'' + std::string(var->identifier) + "' is not initialized";
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

void Sema::resolve_symbol_names(const std::vector<std::unique_ptr<ResolvedDecl>> &declarations) {
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

        e.decl->symbolName = e.symbol + std::string(e.decl->identifier);
        // if (const auto *func = dynamic_cast<const ResolvedMemberFunctionDecl *>(e.decl)) {
        //     func->function->symbolName = e.decl->symbolName;
        // }
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
        } else if (const auto *ErrorGroupExprDecl = dynamic_cast<const ResolvedErrorGroupExprDecl *>(e.decl)) {
            for (auto &&decl : ErrorGroupExprDecl->errors) {
                stack.push(elem{decl.get(), e.level + 1, new_symbol_name});
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
    auto dep = dynamic_cast<ResolvedDependencies *>(decl);
    if (!dep) return;

    if (m_currentFunction) {
        m_currentFunction->dependsOn.emplace(dep);
        dep->isUsedBy.emplace(m_currentFunction);
    } else if (m_actualModule) {
        // auto it_depends = m_actualModule->dependsOn.find(dep);
        // if (it_depends != m_actualModule->dependsOn.end()) {
        //     (*it_depends).second++;
        // } else {
        // }
        m_actualModule->dependsOn.emplace(dep);
        dep->isUsedBy.emplace(m_actualModule);

        // auto it_dependencies = dep->isUsedBy.find(m_actualModule);
        // if (it_dependencies != dep->isUsedBy.end()) {
        //     (*it_dependencies).second++;
        // } else {
        // }
    } else {
        dmz_unreachable("internal error: unexpected add depencendy inside no module");
    }
}
}  // namespace DMZ