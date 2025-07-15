// #define DEBUG
#include "codegen/Codegen.hpp"

namespace DMZ {

std::string Codegen::generate_struct_name(const ResolvedStructDecl &structDecl) {
    std::string name = "struct." + structDecl.moduleID.to_string() + structDecl.type.to_str();
    debug_func(name);
    name = generate_symbol_name(name);
    return name;
}

std::string Codegen::generate_function_name(const ResolvedFuncDecl &functionDecl) {
    std::string name;
    debug_func(name);
    if (functionDecl.parent) {
        name += generate_function_name(*functionDecl.parent);
        if (dynamic_cast<const ResolvedFunctionDecl *>(&functionDecl)) return name;
    }

    if (auto memberFunction = dynamic_cast<const ResolvedMemberFunctionDecl *>(&functionDecl)) {
        name += generate_struct_name(memberFunction->structDecl);
        name += ".";
    }

    if (dynamic_cast<const ResolvedExternFunctionDecl *>(&functionDecl)) {
        name += std::string(functionDecl.identifier);
    } else if (dynamic_cast<const ResolvedFunctionDecl *>(&functionDecl) ||
               dynamic_cast<const ResolvedMemberFunctionDecl *>(&functionDecl)) {
        name += functionDecl.moduleID.to_string() + std::string(functionDecl.identifier);
    }

    if (auto specializedFunc = dynamic_cast<const ResolvedSpecializedFunctionDecl *>(&functionDecl)) {
        name += specializedFunc->genericTypes.to_str();
    }

    if (functionDecl.identifier == "main") {
        return "__builtin_main";
    }

    return generate_symbol_name(name);
}

void Codegen::generate_function_decl(const ResolvedFuncDecl &functionDecl) {
    debug_func("");
    if (auto resolvedFunctionDecl = dynamic_cast<const ResolvedFunctionDecl *>(&functionDecl)) {
        if (resolvedFunctionDecl->genericTypes) {
            for (auto &&func : resolvedFunctionDecl->specializations) {
                generate_function_decl(*func.get());
            }
            return;
        }
    }
    if (auto resolvedFunctionDecl = dynamic_cast<const ResolvedMemberFunctionDecl *>(&functionDecl)) {
        return generate_function_decl(*resolvedFunctionDecl->function.get());
    }

    llvm::Type *retType = generate_type(functionDecl.type);
    std::vector<llvm::Type *> paramTypes;

    if (functionDecl.type.kind == Type::Kind::Struct || functionDecl.type.isOptional) {
        paramTypes.emplace_back(llvm::PointerType::get(retType, 0));
        retType = m_builder.getVoidTy();
    }

    bool isVararg = false;
    for (auto &&param : functionDecl.params) {
        if (param->isVararg) {
            isVararg = true;
            continue;
        }
        llvm::Type *paramType = generate_type(param->type);
        if (param->type.kind == Type::Kind::Struct || param->type.isRef) {
            paramType = llvm::PointerType::get(paramType, 0);
        }
        paramTypes.emplace_back(paramType);
    }

    auto *type = llvm::FunctionType::get(retType, paramTypes, isVararg);
    std::string funcName = generate_function_name(functionDecl);
    auto *fn = llvm::Function::Create(type, llvm::Function::ExternalLinkage, funcName, *m_module);
    fn->setAttributes(construct_attr_list(functionDecl));
}

llvm::AttributeList Codegen::construct_attr_list(const ResolvedFuncDecl &funcDecl) {
    debug_func(funcDecl.moduleID << funcDecl.identifier);
    const ResolvedFuncDecl *fn;
    if (auto resFunctionDecl = dynamic_cast<const ResolvedMemberFunctionDecl *>(&funcDecl)) {
        fn = resFunctionDecl->function.get();
    } else {
        fn = &funcDecl;
    }
    bool isReturningStruct = fn->type.kind == Type::Kind::Struct || fn->type.isOptional;
    std::vector<llvm::AttributeSet> argsAttrSets;

    if (isReturningStruct) {
        llvm::AttrBuilder retAttrs(*m_context);
        retAttrs.addStructRetAttr(generate_type(fn->type));
        argsAttrSets.emplace_back(llvm::AttributeSet::get(*m_context, retAttrs));
    }

    for ([[maybe_unused]] auto &&param : fn->params) {
        debug_msg("Param: " << param->type);
        llvm::AttrBuilder paramAttrs(*m_context);
        if (param->type.kind == Type::Kind::Struct) {
            if (param->type.isRef) {
                paramAttrs.addByRefAttr(generate_type(param->type));
            } else {
                if (param->isMutable) {
                    paramAttrs.addByValAttr(generate_type(param->type));
                } else {
                    paramAttrs.addAttribute(llvm::Attribute::ReadOnly);
                }
            }
        } else {
            if (param->type.isRef) {
                paramAttrs.addByRefAttr(generate_type(param->type));
            }
        }
        argsAttrSets.emplace_back(llvm::AttributeSet::get(*m_context, paramAttrs));
    }

    return llvm::AttributeList::get(*m_context, llvm::AttributeSet{}, llvm::AttributeSet{}, argsAttrSets);
}

void Codegen::generate_function_body(const ResolvedFuncDecl &functionDecl) {
    debug_func("");
    if (auto resolvedFunctionDecl = dynamic_cast<const ResolvedFunctionDecl *>(&functionDecl)) {
        if (resolvedFunctionDecl->genericTypes) {
            for (auto &&func : resolvedFunctionDecl->specializations) {
                generate_function_body(*func.get());
            }
            return;
        }
    }
    if (auto resolvedFunctionDecl = dynamic_cast<const ResolvedMemberFunctionDecl *>(&functionDecl)) {
        return generate_function_body(*resolvedFunctionDecl->function.get());
    }

    m_currentFunction = &functionDecl;
    std::string funcName = generate_function_name(functionDecl);
    auto *function = m_module->getFunction(funcName);

    auto *entryBB = llvm::BasicBlock::Create(*m_context, "entry", function);
    m_builder.SetInsertPoint(entryBB);

    // Note: llvm:Instruction has a protected destructor.
    llvm::Value *undef = llvm::UndefValue::get(m_builder.getInt32Ty());
    m_allocaInsertPoint = new llvm::BitCastInst(undef, undef->getType(), "alloca.placeholder", entryBB);
    m_memsetInsertPoint = new llvm::BitCastInst(undef, undef->getType(), "memset.placeholder", entryBB);

    bool returnsVoid = functionDecl.type.kind == Type::Kind::Struct || functionDecl.type.kind == Type::Kind::Void ||
                       functionDecl.type.isOptional;
    if (!returnsVoid) {
        retVal = allocate_stack_variable("retval", functionDecl.type);
    }
    retBB = llvm::BasicBlock::Create(*m_context, "return");

    int idx = 0;
    for (auto &&arg : function->args()) {
        if (arg.hasStructRetAttr()) {
            arg.setName("ret");
            retVal = &arg;
            continue;
        }

        const auto *paramDecl = functionDecl.params[idx].get();
        arg.setName(paramDecl->identifier);

        llvm::Value *declVal = &arg;
        if (paramDecl->type.kind != Type::Kind::Struct && !paramDecl->type.isRef && paramDecl->isMutable) {
            declVal = allocate_stack_variable(paramDecl->identifier, paramDecl->type);
            store_value(&arg, declVal, paramDecl->type, paramDecl->type);
        }

        m_declarations[paramDecl] = declVal;
        ++idx;
    }

    // if (functionDecl.identifier == "println")
    // generate_builtin_println_body(functionDecl);
    // else
    ResolvedBlock *body;
    if (auto specFunc = dynamic_cast<const ResolvedSpecializedFunctionDecl *>(&functionDecl)) {
        body = specFunc->body.get();
    }
    if (auto function = dynamic_cast<const ResolvedFunctionDecl *>(&functionDecl)) {
        body = function->body.get();
    }
    if (!body) {
        dmz_unreachable("unexpected void body");
    }
    generate_block(*body);

    if (retBB->hasNPredecessorsOrMore(1)) {
        break_into_bb(retBB);
        retBB->insertInto(function);
        m_builder.SetInsertPoint(retBB);
    }

    m_allocaInsertPoint->eraseFromParent();
    m_allocaInsertPoint = nullptr;
    m_memsetInsertPoint->eraseFromParent();
    m_memsetInsertPoint = nullptr;

    if (returnsVoid) {
        m_builder.CreateRetVoid();
        return;
    }

    m_builder.CreateRet(load_value(retVal, functionDecl.type));
}

void Codegen::generate_struct_decl(const ResolvedStructDecl &structDecl) {
    debug_func("");
    if (structDecl.genericTypes) {
        for (auto &&espec : structDecl.specializations) {
            generate_struct_decl(*espec);
        }
        return;
    }

    llvm::StructType::create(*m_context, generate_struct_name(structDecl));
}

void Codegen::generate_struct_definition(const ResolvedStructDecl &structDecl) {
    debug_func("");
    if (structDecl.genericTypes) {
        for (auto &&espec : structDecl.specializations) {
            generate_struct_definition(*espec);
        }
        return;
    }
    auto *type = static_cast<llvm::StructType *>(generate_type(structDecl.type));

    std::vector<llvm::Type *> fieldTypes;
    for (auto &&field : structDecl.fields) {
        llvm::Type *t = generate_type(field->type);
        fieldTypes.emplace_back(t);
    }

    type->setBody(fieldTypes);
}

void Codegen::generate_err_no_err() {
    debug_func("");
    if (m_success) return;
    std::string str("SUCCESS");
    llvm::Constant *stringConst = llvm::ConstantDataArray::getString(*m_context, str, true);
    m_success =
        new llvm::GlobalVariable(*m_module, stringConst->getType(), true,
                                 llvm::GlobalVariable::LinkageTypes::PrivateLinkage, stringConst, "err.str." + str);
}

void Codegen::generate_err_group_decl(const ResolvedErrGroupDecl &errGroupDecl) {
    debug_func("");
    for (auto &err : errGroupDecl.errs) {
        auto name = err->moduleID.to_string() + std::string(err->identifier);
        auto symbol_name = generate_symbol_name(name);
        llvm::Constant *stringConst = llvm::ConstantDataArray::getString(*m_context, name, true);
        m_declarations[err.get()] = new llvm::GlobalVariable(*m_module, stringConst->getType(), true,
                                                             llvm::GlobalVariable::LinkageTypes::PrivateLinkage,
                                                             stringConst, "err.str." + symbol_name);
    }
}

void Codegen::generate_module_decl(const ResolvedModuleDecl &moduleDecl) {
    debug_func("");
    if (moduleDecl.nestedModule) {
        generate_module_decl(*moduleDecl.nestedModule);
    } else {
        generate_in_module_decl(moduleDecl.declarations);
    }
}

void Codegen::generate_in_module_decl(const std::vector<std::unique_ptr<ResolvedDecl>> &declarations, bool isGlobal) {
    debug_func("");
    for (auto &&decl : declarations) {
        if (const auto *fn = dynamic_cast<const ResolvedFuncDecl *>(decl.get())) {
            if (fn->identifier == "get_errno") {
                generate_builtin_get_errno();
            } else {
                generate_function_decl(*fn);
            }
        } else if (const auto *sd = dynamic_cast<const ResolvedStructDecl *>(decl.get())) {
            generate_struct_decl(*sd);
        } else if (dynamic_cast<const ResolvedErrGroupDecl *>(decl.get()) ||
                   dynamic_cast<const ResolvedModuleDecl *>(decl.get()) ||
                   dynamic_cast<const ResolvedImportDecl *>(decl.get())) {
            continue;
        } else {
            decl->dump();
            dmz_unreachable("unexpected top level in module declaration");
        }
    }

    if (isGlobal) {
        generate_main_wrapper();
    }

    generate_err_no_err();
    for (auto &&decl : declarations) {
        if (const auto *errGroup = dynamic_cast<const ResolvedErrGroupDecl *>(decl.get()))
            generate_err_group_decl(*errGroup);
    }

    for (auto &&decl : declarations) {
        if (dynamic_cast<const ResolvedExternFunctionDecl *>(decl.get()) ||
            dynamic_cast<const ResolvedErrGroupDecl *>(decl.get()) ||
            dynamic_cast<const ResolvedImportDecl *>(decl.get())) {
            continue;
        } else if (const auto *fn = dynamic_cast<const ResolvedFuncDecl *>(decl.get())) {
            generate_function_body(*fn);
        } else if (const auto *sd = dynamic_cast<const ResolvedStructDecl *>(decl.get())) {
            generate_struct_definition(*sd);
        } else if (const auto *modDecl = dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
            generate_module_decl(*modDecl);
        } else {
            decl->dump();
            dmz_unreachable("unexpected top level in module declaration");
        }
    }
}
}  // namespace DMZ
