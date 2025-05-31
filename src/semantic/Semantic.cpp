#include "semantic/Semantic.hpp"

#include <map>
#include <stack>
#include <unordered_set>

namespace DMZ {

std::mutex Sema::m_moduleScopesMutex = {};
std::unordered_multimap<std::string, ResolvedDecl *> Sema::m_moduleScopes = {};

void Sema::dump_module_scopes() {
    std::unique_lock lock(m_moduleScopesMutex);
    for (auto &&[key, value] : m_moduleScopes) {
        std::cerr << "Key: " << key << std::endl;
        std::cerr << "Value: ";
        value->dump(0, true);
    }
}

bool Sema::insert_decl_to_current_scope(ResolvedDecl &decl) {
    const auto &[foundDecl, scopeIdx] = lookup_decl<ResolvedDecl>(decl.modIdentifier);

    // println("insert_decl_to_current_scope " << std::quoted(decl.identifier) << " " << std::quoted(decl.modIdentifier)
    //                                         << " foundDecl " << foundDecl << " scopeIdx " << scopeIdx);

    if (!dynamic_cast<ResolvedImportDecl *>(&decl) && foundDecl && scopeIdx == 0) {
        report(decl.location, "redeclaration of '" + std::string(decl.identifier) + '\'');
        return false;
    }

    m_scopes.back().emplace_back(&decl);

    if (dynamic_cast<ResolvedFuncDecl *>(&decl) || dynamic_cast<ResolvedStructDecl *>(&decl) ||
        dynamic_cast<ResolvedErrDecl *>(&decl) || dynamic_cast<ResolvedModuleDecl *>(&decl)) {
        std::unique_lock lock(m_moduleScopesMutex);
        // println("identifier " << std::quoted(identifier));
        m_moduleScopes.emplace(std::piecewise_construct, std::forward_as_tuple(decl.modIdentifier),
                               std::forward_as_tuple(&decl));
    }

    return true;
}

template <typename T>
std::pair<T *, int> Sema::lookup_decl(const std::string_view id) {
    // println("lookup_decl " << std::quoted(std::string(id)));
    int scopeIdx = 0;
    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
        for (auto &&decl : *it) {
            auto *correctDecl = dynamic_cast<T *>(decl);

            if (!correctDecl) continue;

            if (decl->modIdentifier != id) continue;

            // Permit various module decl
            if (dynamic_cast<ResolvedModuleDecl *>(decl)) {
                return {correctDecl, -1};
            } else {
                return {correctDecl, scopeIdx};
            }
        }

        ++scopeIdx;
    }
    {
        std::unique_lock lock(m_moduleScopesMutex);
        auto it = m_moduleScopes.find(std::string(id));
        if (it != m_moduleScopes.end()) {
            auto *correctDecl = dynamic_cast<T *>((*it).second);

            if (correctDecl && correctDecl->modIdentifier == id) {
                // Permit various module decl
                if (dynamic_cast<ResolvedModuleDecl *>((*it).second)) {
                    return {correctDecl, -1};
                } else {
                    return {correctDecl, 0};
                }
            }
        }
    }

    return {nullptr, -1};
}
template std::pair<ResolvedStructDecl *, int> Sema::lookup_decl(std::string_view);
template std::pair<ResolvedDecl *, int> Sema::lookup_decl(std::string_view);
template std::pair<ResolvedErrDecl *, int> Sema::lookup_decl(std::string_view);
template std::pair<ResolvedImportDecl *, int> Sema::lookup_decl(std::string_view);
template std::pair<ResolvedModuleDecl *, int> Sema::lookup_decl(std::string_view);

std::optional<Type> Sema::resolve_type(Type parsedType) {
    if (parsedType.kind == Type::Kind::Custom) {
        if (lookup_decl<ResolvedStructDecl>(parsedType.name).first) {
            return Type::structType(parsedType);
        }

        return std::nullopt;
    }

    return parsedType;
}

std::vector<std::unique_ptr<ResolvedDecl>> Sema::resolve_ast_decl() {
    ScopedTimer st(Stats::type::semanticTime);

    std::vector<std::unique_ptr<ResolvedDecl>> resolvedTree;

    for (auto &&decl : m_ast) {
        if (!dynamic_cast<const ModuleDecl *>(decl.get())) {
            m_ast_withoutModules.emplace_back(std::move(decl));
        }
    }
    // Clear the withoutModules from m_ast
    std::erase_if(m_ast, [](const std::unique_ptr<Decl> &decl) { return decl.get() == nullptr; });

    auto resolvedDecls = resolve_in_module_decl(m_ast_withoutModules);

    for (auto &&decl : resolvedDecls) {
        resolvedTree.emplace_back(std::move(decl));
    }

    // Now resolve the modules
    bool error = false;
    for (auto &&decl : m_ast) {
        if (const auto *mod = dynamic_cast<const ModuleDecl *>(decl.get())) {
            auto resolvedModDecl = resolve_module_decl(*mod);
            resolvedTree.emplace_back(std::move(resolvedModDecl));
            continue;
        }
    }

    if (error) return {};

    // dump_module_scopes();
    return resolvedTree;
}

bool Sema::resolve_ast_body(std::vector<std::unique_ptr<ResolvedDecl>> &decls) {
    if (!resolve_in_module_body(decls)) {
        return false;
    }

    // Now resolve the modules
    bool error = false;
    for (auto &&decl : decls) {
        if (auto *mod = dynamic_cast<ResolvedModuleDecl *>(decl.get())) {
            if (!resolve_module_body(*mod)) {
                error = true;
                continue;
            }
            continue;
        }
    }

    if (error) {
        decls.clear();
        return false;
    }

    // dump_module_scopes();
    return true;
}

// std::vector<std::unique_ptr<ResolvedDecl>> Sema::resolve_ast() {
//     ScopeRAII globalScope(*this);
//     auto decls = resolve_ast_decl();

//     if (!resolve_ast_body(decls)) {
//         return {};
//     }
//     return decls;
// }

bool Sema::run_flow_sensitive_checks(const ResolvedFunctionDecl &fn) {
    CFG cfg = CFGBuilder().build(fn);

    bool error = false;
    error |= check_return_on_all_paths(fn, cfg);
    error |= check_variable_initialization(cfg);

    return error;
};

bool Sema::check_return_on_all_paths(const ResolvedFunctionDecl &fn, const CFG &cfg) {
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
    enum class State { Bottom, Unassigned, Assigned, Top };

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

                    if (!decl->isMutable && tmp[decl] != State::Unassigned) {
                        std::string msg = '\'' + std::string(decl->identifier) + "' cannot be mutated";
                        pendingErrors.emplace_back(assignment->location, std::move(msg));
                    }

                    tmp[decl] = State::Assigned;
                    continue;
                }

                if (const auto *dre = dynamic_cast<const ResolvedDeclRefExpr *>(stmt)) {
                    const auto *var = dynamic_cast<const ResolvedVarDecl *>(&dre->decl);

                    if (var && tmp[var] != State::Assigned) {
                        std::string msg = '\'' + std::string(var->identifier) + "' is not initialized";
                        pendingErrors.emplace_back(dre->location, std::move(msg));
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
}  // namespace DMZ