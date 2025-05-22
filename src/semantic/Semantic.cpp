#include "semantic/Semantic.hpp"

#include <map>
#include <stack>
#include <unordered_set>

namespace DMZ {

bool Sema::insert_decl_to_current_scope(ResolvedDecl &decl) {
    const auto &[foundDecl, scopeIdx] = lookup_decl<ResolvedDecl>(decl.identifier);

    if (foundDecl && scopeIdx == 0) {
        report(decl.location, "redeclaration of '" + std::string(decl.identifier) + '\'');
        return false;
    }

    m_scopes.back().emplace_back(&decl);
    return true;
}

template <typename T>
std::pair<T *, int> Sema::lookup_decl(const std::string_view id) {
    int scopeIdx = 0;
    for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); ++it) {
        for (auto &&decl : *it) {
            auto *correctDecl = dynamic_cast<T *>(decl);

            if (!correctDecl) continue;

            if (decl->identifier != id) continue;

            return {correctDecl, scopeIdx};
        }

        ++scopeIdx;
    }

    return {nullptr, -1};
}
template std::pair<ResolvedStructDecl *, int> Sema::lookup_decl(std::string_view);
template std::pair<ResolvedDecl *, int> Sema::lookup_decl(std::string_view);
template std::pair<ResolvedErrDecl *, int> Sema::lookup_decl(std::string_view);

std::optional<Type> Sema::resolve_type(Type parsedType) {
    if (parsedType.kind == Type::Kind::Custom) {
        if (lookup_decl<ResolvedStructDecl>(parsedType.name).first) {
            return Type::structType(parsedType);
        }

        return std::nullopt;
    }

    return parsedType;
}

std::vector<std::unique_ptr<ResolvedDecl>> Sema::resolve_ast() {
    ScopedTimer st(Stats::type::semanticTime);

    ScopeRAII globalScope(*this);
    std::vector<std::unique_ptr<ResolvedDecl>> resolvedTree;

    bool error = false;
    std::vector<const FuncDecl *> functionsToResolve;

    // Resolve every struct first so that functions have access to them in their
    // signature.
    {
        ScopedTimer st(Stats::type::semanticResolveStructsTime);
        for (auto &&decl : m_ast) {
            if (const auto *st = dynamic_cast<const StructDecl *>(decl.get())) {
                std::unique_ptr<ResolvedDecl> resolvedDecl = resolve_struct_decl(*st);

                if (!resolvedDecl || !insert_decl_to_current_scope(*resolvedDecl)) {
                    error = true;
                    continue;
                }

                resolvedTree.emplace_back(std::move(resolvedDecl));
                continue;
            }

            if (const auto *fn = dynamic_cast<const FuncDecl *>(decl.get())) {
                functionsToResolve.emplace_back(fn);
                continue;
            }
            if (const auto *err = dynamic_cast<const ErrGroupDecl *>(decl.get())) {
                std::unique_ptr<ResolvedDecl> resolvedDecl = resolve_err_group_decl(*err);
                if (!resolvedDecl) {
                    error = true;
                    continue;
                }
                resolvedTree.emplace_back(std::move(resolvedDecl));
                continue;
            }

            dmz_unreachable("unexpected declaration");
        }
    }

    if (error) return {};

    // Insert println first to be able to detect a possible redeclaration.
    auto *printlnDecl = resolvedTree.emplace_back(create_builtin_println()).get();
    insert_decl_to_current_scope(*printlnDecl);
    {
        ScopedTimer st(Stats::type::semanticResolveFunctionsTime);
        for (auto &&fn : functionsToResolve) {
            if (auto resolvedDecl = resolve_function_decl(*fn);
                resolvedDecl && insert_decl_to_current_scope(*resolvedDecl)) {
                resolvedTree.emplace_back(std::move(resolvedDecl));
                continue;
            }

            error = true;
        }
    }
    {
        decltype(functionsToResolve) aux_vec;
        aux_vec.reserve(functionsToResolve.size());

        for (auto &func : functionsToResolve) {
            if (!dynamic_cast<const ExternFunctionDecl *>(func)) {
                aux_vec.emplace_back(func);
            }
        }

        functionsToResolve.swap(aux_vec);
    }
    // Clear the extern functions
    // std::erase_if(functionsToResolve,
    //               [](const FuncDecl *func) { return dynamic_cast<const ExternFunctionDecl *>(func); });

    if (error) return {};
    {
        ScopedTimer st(Stats::type::semanticResolveBodysTime);
        auto nextFunctionDecl = functionsToResolve.begin();
        for (auto &&currentDecl : resolvedTree) {
            if (auto *st = dynamic_cast<ResolvedStructDecl *>(currentDecl.get())) {
                if (!resolve_struct_fields(*st)) error = true;

                continue;
            }

            if (auto *fn = dynamic_cast<ResolvedFunctionDecl *>(currentDecl.get())) {
                if (fn == printlnDecl) continue;

                ScopeRAII paramScope(*this);
                for (auto &&param : fn->params) insert_decl_to_current_scope(*param);

                currentFunction = fn;
                if (auto nextFunc = dynamic_cast<const FunctionDecl *>(*nextFunctionDecl++)) {
                    if (auto resolvedBody = resolve_block(*nextFunc->body)) {
                        fn->body = std::move(resolvedBody);
                        error |= run_flow_sensitive_checks(*fn);
                        continue;
                    }
                } else {
                    continue;
                }

                error = true;
            }
        }
    }
    if (error) return {};

    return resolvedTree;
}

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