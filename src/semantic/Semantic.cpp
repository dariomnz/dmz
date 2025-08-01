// #define DEBUG
#include "semantic/Semantic.hpp"

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
    std::string identifier(decl.identifier);
    // if (dynamic_cast<ResolvedModuleDecl *>(&decl)) {
    //     identifier = "module " + identifier;
    // }
#ifdef DEBUG_SCOPES
    println("======================>>insert_decl_to_current_scope " << identifier << " ======================");
#endif
    const auto &[foundDecl, scopeIdx] = lookup(identifier, ResolvedDeclType::ResolvedDecl);

    if (foundDecl) {
        dump_scopes();
        report(decl.location, "redeclaration of '" + identifier + '\'');
        return false;
    }

    m_scopes.back().emplace(identifier, &decl);
#ifdef DEBUG_SCOPES
    dump_scopes();
    println("======================<<insert_decl_to_current_scope " << identifier << " ======================");
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
        case ResolvedDeclType::ResolvedErrDecl:                                   \
            if (bool dynamic_cast<ResolvedErrDecl *>(decl)) expresion;            \
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

std::pair<ResolvedDecl *, int> Sema::lookup(const std::string_view id, ResolvedDeclType type) {
    std::string identifier(id);
    // if (type == ResolvedDeclType::ResolvedModuleDecl) {
    //     identifier = "module " + identifier;
    // }
    debug_func(identifier << " " << type);
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
            if (declID != identifier) continue;
            // debug_msg("Found");
            return {decl, scopeIdx};
        }

        ++scopeIdx;
    }
    return {nullptr, -1};
}

ResolvedDecl *Sema::lookup_in_module(const ResolvedModuleDecl &moduleDecl, const std::string_view id,
                                     ResolvedDeclType type) {
    std::string identifier(id);
    // if (type == ResolvedDeclType::ResolvedModuleDecl) {
    //     identifier = "module " + identifier;
    // }
    debug_func("Module: " << moduleDecl.identifier << " id: " << identifier << " type: " << type);

    for (auto &&decl : moduleDecl.declarations) {
        debug_msg("Seach: " << decl->identifier);
        auto declPtr = decl.get();
        switch_resolved_decl_type(type, declPtr, !, continue);

        if (identifier != declPtr->identifier) continue;
        return declPtr;
    }
    return nullptr;
}

ResolvedDecl *Sema::lookup_in_struct(const ResolvedStructDecl &structDecl, const std::string_view id,
                                     ResolvedDeclType type) {
    debug_func("Struct " << structDecl.identifier << " " << id << " " << type);
    if (type == ResolvedDeclType::ResolvedDecl || type == ResolvedDeclType::ResolvedMemberFunctionDecl) {
        for (auto &&decl : structDecl.functions) {
            if (id != decl->identifier) continue;
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
    ScopedTimer st(Stats::type::semanticTime);
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
    return resolve_in_module_body(decls);
}

bool Sema::run_flow_sensitive_checks(const ResolvedFuncDecl &fn) {
    debug_func(fn.location);
    const ResolvedBlock *block;
    if (auto resfn = dynamic_cast<const ResolvedFunctionDecl *>(&fn)) {
        block = resfn->body.get();
    } else if (auto resfn = dynamic_cast<const ResolvedMemberFunctionDecl *>(&fn)) {
        block = resfn->function->body.get();
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
        //            dynamic_cast<const ResolvedErrGroupDecl *>(decl.get())) {
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
        if (const auto *func = dynamic_cast<const ResolvedMemberFunctionDecl *>(e.decl)) {
            func->function->symbolName = e.decl->symbolName;
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
        } else if (const auto *errGroupDecl = dynamic_cast<const ResolvedErrGroupDecl *>(e.decl)) {
            for (auto &&decl : errGroupDecl->errs) {
                stack.push(elem{decl.get(), e.level + 1, new_symbol_name});
            }
        } else if (dynamic_cast<const ResolvedDeclStmt *>(e.decl) || dynamic_cast<const ResolvedFuncDecl *>(e.decl) ||
                   dynamic_cast<const ResolvedErrDecl *>(e.decl)) {
        } else {
            e.decl->dump(0, true);
            dmz_unreachable("unexpected declaration");
        }
    }
}
}  // namespace DMZ