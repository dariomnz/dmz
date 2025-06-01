#include "semantic/Semantic.hpp"

#include <cassert>
#include <map>
#include <stack>
#include <unordered_set>

// #define DEBUG

namespace DMZ {

std::mutex Sema::m_moduleScopesMutex = {};
Sema::ModuleScope Sema::m_globalmodule = {};

ResolvedDecl *Sema::ModuleScope::lookup_decl(const std::string_view id) {
    auto it = m_decls.find(std::string(id));
    return it == m_decls.end() ? nullptr : it->second;
}

Sema::ModuleScope *Sema::ModuleScope::lookup_module(const std::string_view id) {
    auto it = m_modules.find(std::string(id));
    return it == m_modules.end() ? nullptr : &it->second;
}

bool Sema::ModuleScope::insert(ResolvedDecl &decl) {
    return m_decls
        .emplace(std::piecewise_construct, std::forward_as_tuple(decl.identifier), std::forward_as_tuple(&decl))
        .second;
}

bool Sema::ModuleScope::insert(std::string_view identifier) {
    return m_modules.emplace(std::piecewise_construct, std::forward_as_tuple(identifier), std::forward_as_tuple())
        .second;
}

void Sema::ModuleScope::dump(size_t level) const {
    for (auto &[key, value] : m_modules) {
        std::cerr << indent(level) << "Module: " << std::quoted(key) << std::endl;
        value.dump(level + 1);
    }
    for (auto &[key, value] : m_decls) {
        std::cerr << indent(level) << "Decl: " << std::quoted(key) << std::endl;
    }
}

void Sema::dump_module_scopes() const {
    std::unique_lock lock(m_moduleScopesMutex);
    m_globalmodule.dump();
}

void Sema::dump_scopes() const {
    size_t level = 0;
    for (auto &&scope : m_scopes) {
        for (auto &&decl : scope) {
            decl->dump(level, true);
        }
        level++;
    }
}

bool Sema::insert_decl_to_current_scope(ResolvedDecl &decl) {
    std::string identifier(decl.identifier);
    if (auto importDecl = dynamic_cast<ResolvedImportDecl *>(&decl)) {
        if (importDecl->alias.empty()) {
            identifier = importDecl->moduleID.to_string() + identifier;
        } else {
            identifier = importDecl->alias;
        }
    }
#ifdef DEBUG
    println("==================================");
    println("insert_decl_to_current_scope " << identifier);
    println("==================================");
#endif
    const auto &[foundDecl, scopeIdx] = lookup_decl<ResolvedDecl>(identifier);

    if (!dynamic_cast<ResolvedImportDecl *>(&decl) && foundDecl && scopeIdx == 0) {
        report(decl.location, "redeclaration of '" + identifier + '\'');
        return false;
    }

    m_scopes.back().emplace_back(&decl);
    return true;
}

bool Sema::insert_decl_to_modules(ResolvedDecl &decl) {
#ifdef DEBUG
    println("==================================");
    println("insert_decl_to_modules " << decl.moduleID << " " << decl.identifier);
    println("==================================");
#endif
    std::unique_lock lock(m_moduleScopesMutex);
    auto *currentModule = &m_globalmodule;
    for (auto &&modName : decl.moduleID.modules) {
        currentModule->insert(modName);
        currentModule = currentModule->lookup_module(modName);
        assert(currentModule);
    }

    if (dynamic_cast<ResolvedModuleDecl *>(&decl)) return true;
    return currentModule->insert(decl);
}

template <typename T>
std::pair<T *, int> Sema::lookup_decl(const std::string_view id) {
#ifdef DEBUG
    println("-----------------------------------------");
    println("lookup_decl " << std::quoted(std::string(id)));
    dump_scopes();
    println("-----------------------------------------");
#endif
    int scopeIdx = 0;
    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
        for (auto &&decl : *it) {
            auto *correctDecl = dynamic_cast<T *>(decl);
            if (!correctDecl) continue;
            if (auto importDecl = dynamic_cast<ResolvedImportDecl *>(decl)) {
                std::string identifier(decl->identifier);
                if (importDecl->alias.empty()) {
                    identifier = importDecl->moduleID.to_string() + identifier;
                } else {
                    identifier = importDecl->alias;
                }
                if (identifier != id) continue;
            } else {
                if (decl->identifier != id) continue;
            }

            return {correctDecl, scopeIdx};
        }

        ++scopeIdx;
    }
    return {nullptr, -1};
}
template std::pair<ResolvedStructDecl *, int> Sema::lookup_decl(std::string_view);
template std::pair<ResolvedDecl *, int> Sema::lookup_decl(std::string_view);
template std::pair<ResolvedErrDecl *, int> Sema::lookup_decl(std::string_view);
template std::pair<ResolvedImportDecl *, int> Sema::lookup_decl(std::string_view);
template std::pair<ResolvedModuleDecl *, int> Sema::lookup_decl(std::string_view);

template <typename T>
T *Sema::lookup_decl_in_modules(const ModuleID &moduleID, const std::string_view id) {
#ifdef DEBUG
    println("-----------------------------------------");
    println("lookup_decl_in_modules " << moduleID << " " << id);
    dump_module_scopes();
    println("-----------------------------------------");
#endif
    std::unique_lock lock(m_moduleScopesMutex);
    auto *currentModule = &m_globalmodule;
    for (auto &&modName : moduleID.modules) {
        currentModule = currentModule->lookup_module(modName);
        if (!currentModule) return nullptr;
    }

    auto decl = currentModule->lookup_decl(id);
    if (!decl) return nullptr;
    if (auto Tdecl = dynamic_cast<T *>(decl)) return Tdecl;
    return nullptr;
}
template ResolvedDecl *Sema::lookup_decl_in_modules(const ModuleID &, std::string_view);
template ResolvedImportDecl *Sema::lookup_decl_in_modules(const ModuleID &, std::string_view);
template ResolvedModuleDecl *Sema::lookup_decl_in_modules(const ModuleID &, std::string_view);
template ResolvedErrDecl *Sema::lookup_decl_in_modules(const ModuleID &, std::string_view);

bool Sema::lookup_in_modules(const ModuleID &moduleID, const std::string_view id) {
#ifdef DEBUG
    println("-----------------------------------------");
    println("lookup_in_modules " << moduleID << " " << id);
    dump_module_scopes();
    println("-----------------------------------------");
#endif
    std::unique_lock lock(m_moduleScopesMutex);
    auto *currentModule = &m_globalmodule;
    for (auto &&modName : moduleID.modules) {
        currentModule = currentModule->lookup_module(modName);
        if (!currentModule) return false;
    }
    auto decl = currentModule->lookup_module(id);
    if (decl) return true;
    return false;
}

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
            auto resolvedModDecl = resolve_module_decl(*mod, ModuleID{});
            resolvedTree.emplace_back(std::move(resolvedModDecl));
            continue;
        }
    }

    if (error) return {};

    // dump_scopes();
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

    // dump_scopes();
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