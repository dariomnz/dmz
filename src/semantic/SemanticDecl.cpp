#include <map>
#include <stack>
#include <unordered_set>

#include "semantic/Semantic.hpp"

namespace DMZ {

std::unique_ptr<ResolvedFunctionDecl> Sema::create_builtin_println() {
    SourceLocation loc{"<builtin>", 0, 0};

    auto param = std::make_unique<ResolvedParamDecl>(loc, "n", Type::builtinInt(), false);

    std::vector<std::unique_ptr<ResolvedParamDecl>> params;
    params.emplace_back(std::move(param));

    auto block = std::make_unique<ResolvedBlock>(loc, std::vector<std::unique_ptr<ResolvedStmt>>());

    return std::make_unique<ResolvedFunctionDecl>(loc, "println", Type::builtinVoid(), std::move(params),
                                                  std::move(block));
};

std::unique_ptr<ResolvedParamDecl> Sema::resolve_param_decl(const ParamDecl &param) {
    std::optional<Type> type = resolve_type(param.type);

    if (!param.isVararg)
        if (!type || type->kind == Type::Kind::Void)
            return report(param.location, "parameter '" + std::string(param.identifier) + "' has invalid '" +
                                              std::string(param.type.name) + "' type");

    return std::make_unique<ResolvedParamDecl>(param.location, param.identifier, *type, param.isMutable,
                                               param.isVararg);
}

std::unique_ptr<ResolvedFuncDecl> Sema::resolve_function_decl(const FuncDecl &function) {
    std::optional<Type> type = resolve_type(function.type);

    if (!type)
        return report(function.location, "function '" + std::string(function.identifier) + "' has invalid '" +
                                             std::string(function.type.name) + "' type");

    if (function.identifier == "main") {
        if (type->kind != Type::Kind::Void)
            return report(function.location, "'main' function is expected to have 'void' type");

        if (!function.params.empty())
            return report(function.location, "'main' function is expected to take no arguments");
    } else if (function.identifier == "printf") {
        return report(function.location,
                      "'printf' is a reserved function name and cannot be used for "
                      "user-defined functions");
    }

    std::vector<std::unique_ptr<ResolvedParamDecl>> resolvedParams;

    ScopeRAII paramScope(*this);
    bool haveVararg = false;
    for (auto &&param : function.params) {
        auto resolvedParam = resolve_param_decl(*param);
        if (haveVararg) {
            report(resolvedParam->location, "vararg '...' can only be in the last argument");
            return nullptr;
        }

        if (!resolvedParam || !insert_decl_to_current_scope(*resolvedParam)) return nullptr;

        if (resolvedParam->isVararg) {
            haveVararg = true;
        }

        resolvedParams.emplace_back(std::move(resolvedParam));
    }

    if (dynamic_cast<const ExternFunctionDecl *>(&function)) {
        return std::make_unique<ResolvedExternFunctionDecl>(function.location, function.identifier, *type,
                                                            std::move(resolvedParams));
    }
    if (dynamic_cast<const FunctionDecl *>(&function)) {
        return std::make_unique<ResolvedFunctionDecl>(function.location, function.identifier, *type,
                                                      std::move(resolvedParams), nullptr);
    }

    dmz_unreachable("unexpected function");
}

std::unique_ptr<ResolvedVarDecl> Sema::resolve_var_decl(const VarDecl &varDecl) {
    if (!varDecl.type && !varDecl.initializer)
        return report(varDecl.location, "an uninitialized variable is expected to have a type specifier");

    std::unique_ptr<ResolvedExpr> resolvedInitializer = nullptr;
    if (varDecl.initializer) {
        resolvedInitializer = resolve_expr(*varDecl.initializer);
        if (!resolvedInitializer) return nullptr;
    }

    Type resolvableType = varDecl.type.value_or(resolvedInitializer->type);
    std::optional<Type> type = resolve_type(resolvableType);

    if (!type || type->kind == Type::Kind::Void)
        return report(varDecl.location, "variable '" + std::string(varDecl.identifier) + "' has invalid '" +
                                            std::string(resolvableType.name) + "' type");

    if (resolvedInitializer) {
        if (!Type::compare(*type, resolvedInitializer->type))
            return report(resolvedInitializer->location, "initializer type mismatch");

        resolvedInitializer->set_constant_value(cee.evaluate(*resolvedInitializer, false));
    }
    return std::make_unique<ResolvedVarDecl>(varDecl.location, varDecl.identifier, *type, varDecl.isMutable,
                                             std::move(resolvedInitializer));
}

std::unique_ptr<ResolvedStructDecl> Sema::resolve_struct_decl(const StructDecl &structDecl) {
    std::set<std::string_view> identifiers;
    std::vector<std::unique_ptr<ResolvedFieldDecl>> resolvedFields;

    unsigned idx = 0;
    for (auto &&field : structDecl.fields) {
        if (!identifiers.emplace(field->identifier).second)
            return report(field->location, "field '" + std::string(field->identifier) + "' is already declared");

        resolvedFields.emplace_back(
            std::make_unique<ResolvedFieldDecl>(field->location, field->identifier, field->type, idx++));
    }

    return std::make_unique<ResolvedStructDecl>(structDecl.location, structDecl.identifier,
                                                Type::structType(structDecl.identifier), std::move(resolvedFields));
}

bool Sema::resolve_struct_fields(ResolvedStructDecl &resolvedStructDecl) {
    std::stack<std::pair<ResolvedStructDecl *, std::set<const ResolvedStructDecl *>>> worklist;
    worklist.push({&resolvedStructDecl, {}});

    while (!worklist.empty()) {
        auto [currentDecl, visited] = worklist.top();
        worklist.pop();

        if (!visited.emplace(currentDecl).second) {
            report(currentDecl->location, "struct '" + std::string(currentDecl->identifier) + "' contains itself");
            return false;
        }

        for (auto &&field : currentDecl->fields) {
            auto type = resolve_type(field->type);
            if (!type) {
                report(field->location,
                       "unable to resolve '" + std::string(field->type.name) + "' type of struct field");
                return false;
            }

            if (type->kind == Type::Kind::Void) {
                report(field->location, "struct field cannot be void");
                return false;
            }

            if (type->kind == Type::Kind::Struct) {
                auto *nestedStruct = lookup_decl<ResolvedStructDecl>(type->name).first;
                if (!nestedStruct) {
                    report(field->location, "unexpected type");
                    return false;
                }

                worklist.push({nestedStruct, visited});
            }

            field->type = *type;
        }
    }

    return true;
}

std::unique_ptr<ResolvedErrGroupDecl> Sema::resolve_err_group_decl(const ErrGroupDecl &errGroupDecl) {
    std::vector<std::unique_ptr<ResolvedErrDecl>> resolvedErrors;

    for (auto &&err : errGroupDecl.errs) {
        auto &errDecl = resolvedErrors.emplace_back(std::make_unique<ResolvedErrDecl>(err->location, err->identifier));
        if (!insert_decl_to_current_scope(*errDecl)) return nullptr;
    }

    return std::make_unique<ResolvedErrGroupDecl>(errGroupDecl.location, errGroupDecl.identifier,
                                                  std::move(resolvedErrors));
}
}  // namespace DMZ