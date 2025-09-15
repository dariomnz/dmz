// #define DEBUG
#include "semantic/Semantic.hpp"

#include "Stats.hpp"

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

#define switch_resolved_decl_type(type, decl, bool, expresion)                    \
    switch (type) {                                                               \
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
        case ResolvedDeclType::ResolvedGenericTypeDecl:                           \
            if (bool dynamic_cast<ResolvedGenericTypeDecl *>(decl)) expresion;    \
            break;                                                                \
        case ResolvedDeclType::ResolvedFieldDecl:                                 \
            if (bool dynamic_cast<ResolvedFieldDecl *>(decl)) expresion;          \
            break;                                                                \
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
            if (needAddDeps) add_dependency(it_decl->second);
            return it_decl->second;
        }
    }
    return nullptr;
}

ResolvedDecl *Sema::lookup_in_module(const ResolvedModuleDecl &moduleDecl, const std::string_view id) {
    debug_func("Module: " << moduleDecl.identifier << " id: " << id);
    add_dependency(const_cast<ResolvedModuleDecl *>(&moduleDecl));
    for (auto &&decl : moduleDecl.declarations) {
        auto declPtr = decl.get();
        if (id != declPtr->identifier) continue;
        add_dependency(declPtr);
        return declPtr;
    }
    return nullptr;
}

ResolvedDecl *Sema::lookup_in_struct(const ResolvedStructDecl &structDecl, const std::string_view id) {
    debug_func("Struct " << structDecl.identifier << " " << id);
    add_dependency(const_cast<ResolvedStructDecl *>(&structDecl));
    for (auto &&decl : structDecl.functions) {
        if (id != decl->identifier) continue;
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
        if (!decl) return report(declRefType->location, "symbol '" + declRefType->identifier + "' not found");
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
        type.kind == ResolvedTypeKind::ErrorGroup || type.kind == ResolvedTypeKind::Error) {
        return type.clone();
    }
    type.dump();
    dmz_unreachable("TODO");
}

std::vector<ptr<ResolvedModuleDecl>> Sema::resolve_import_modules() {
    debug_func("");
    auto &imported_modules = Driver::instance().imported_modules;
    std::vector<ptr<ResolvedModuleDecl>> resolvedDecl;
    resolvedDecl.reserve(imported_modules.size());
    bool error = false;
    for (auto &&im : imported_modules) {
        auto d = resolve_module(*im.decl);
        if (!d || !resolve_module_decl(*im.decl, *d)) {
            error = true;
            continue;
        }
        auto &resDecl = resolvedDecl.emplace_back(std::move(d));
        m_modules_for_import.emplace(im.identifier, resDecl.get());
    }
    if (error) return {};
    return resolvedDecl;
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

    auto imported_mods = resolve_import_modules();

    auto declarations = resolve_in_module_decl(decls);

    for (auto &moduleDeclPtr : decls) {
        m_ast.emplace_back(static_cast<ModuleDecl *>(moduleDeclPtr.release()));
    }
    decls.clear();

    std::vector<ptr<ResolvedDecl>> resolvedDecls;
    resolvedDecls.reserve(imported_mods.size() + declarations.size());

    for (auto &i : imported_mods) {
        resolvedDecls.emplace_back(std::move(i));
    }
    for (auto &i : declarations) {
        resolvedDecls.emplace_back(std::move(i));
    }

    return resolvedDecls;
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
    auto optType = dynamic_cast<const ResolvedTypeOptional *>(fn.type.get());
    if (fn.type->kind == ResolvedTypeKind::Void || (optType && optType->optionalType->kind == ResolvedTypeKind::Void))
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