// #define DEBUG
#include "Debug.hpp"
#include "codegen/Codegen.hpp"

namespace DMZ {

std::string Codegen::generate_decl_name(const ResolvedDecl &decl) {
    std::string name;
    debug_func(Dumper([&name]() { std::cerr << name; }));
    if (dynamic_cast<const ResolvedFuncDecl *>(&decl)) {
        if (decl.identifier == "main") {
            name = "__builtin_main";
            return name;
        }
        if (decl.identifier == "__builtin_main_test") {
            name = decl.identifier;
            return name;
        }
        if (dynamic_cast<const ResolvedExternFunctionDecl *>(&decl)) {
            name = decl.identifier;
            return name;
        }
    }
    if (!decl.symbolName.empty()) {
        name = decl.symbolName;
    } else {
        name = decl.identifier;
    }

    return name;
}

llvm::Function *Codegen::generate_function_decl(const ResolvedFuncDecl &functionDecl) {
    debug_func(functionDecl.symbolName);
    if (auto resolvedFunctionDecl = dynamic_cast<const ResolvedGenericFunctionDecl *>(&functionDecl)) {
        for (auto &&func : resolvedFunctionDecl->specializations) {
            auto cast_func = dynamic_cast<ResolvedFuncDecl *>(func.get());
            if (!cast_func) {
                func->dump();
                dmz_unreachable("internal error: unexpected declaration in specializations");
            }
            generate_function_decl(*cast_func);
        }
        return nullptr;
    }

    auto llvmFnDecl = m_module->getFunction(generate_decl_name(functionDecl));
    if (llvmFnDecl) return llvmFnDecl;

    auto fnType = functionDecl.getFnType();

    llvm::Type *retType = generate_type(*fnType->returnType);
    std::vector<llvm::Type *> paramTypes;

    if (fnType->returnType->kind == ResolvedTypeKind::Struct ||
        fnType->returnType->kind == ResolvedTypeKind::Optional) {
        paramTypes.emplace_back(llvm::PointerType::get(retType, 0));
        retType = m_builder.getVoidTy();
    }

    bool isVararg = false;
    for (auto &&param : functionDecl.params) {
        if (param->isVararg) {
            isVararg = true;
            continue;
        }
        llvm::Type *paramType = generate_type(*param->type);
        if (dynamic_cast<ResolvedTypeStruct *>(param->type.get())) {
            paramType = llvm::PointerType::get(paramType, 0);
        }
        paramTypes.emplace_back(paramType);
    }

    auto *type = llvm::FunctionType::get(retType, paramTypes, isVararg);
    std::string funcName = generate_decl_name(functionDecl);
    auto *fn = llvm::Function::Create(type, llvm::Function::ExternalLinkage, funcName, *m_module);
    fn->setAttributes(construct_attr_list(*fnType));
    return fn;
}

llvm::AttributeList Codegen::construct_attr_list(const ResolvedTypeFunction &fnType) {
    debug_func(fnType.to_str());

    bool isReturningStruct =
        fnType.returnType->kind == ResolvedTypeKind::Struct || fnType.returnType->kind == ResolvedTypeKind::Optional;
    std::vector<llvm::AttributeSet> argsAttrSets;

    if (isReturningStruct) {
        llvm::AttrBuilder retAttrs(*m_context);
        retAttrs.addStructRetAttr(generate_type(*fnType.returnType));
        argsAttrSets.emplace_back(llvm::AttributeSet::get(*m_context, retAttrs));
    }

    for (auto &&param : fnType.paramsTypes) {
        debug_msg("Param: " << param->to_str());
        llvm::AttrBuilder paramAttrs(*m_context);
        if (auto typePrt = dynamic_cast<ResolvedTypePointer *>(param.get())) {
            if (!dynamic_cast<ResolvedTypeVoid *>(typePrt->pointerType.get())) {
                paramAttrs.addByRefAttr(generate_type(*typePrt->pointerType));
            }
        } else if (param->kind == ResolvedTypeKind::Struct) {
            paramAttrs.addAttribute(llvm::Attribute::ReadOnly);
        }
        argsAttrSets.emplace_back(llvm::AttributeSet::get(*m_context, paramAttrs));
    }

    return llvm::AttributeList::get(*m_context, llvm::AttributeSet{}, llvm::AttributeSet{}, argsAttrSets);
}

void Codegen::generate_function_body(const ResolvedFuncDecl &functionDecl) {
    debug_func(functionDecl.symbolName << " " << functionDecl.type->to_str());
    if (auto resolvedFunctionDecl = dynamic_cast<const ResolvedGenericFunctionDecl *>(&functionDecl)) {
        for (auto &&func : resolvedFunctionDecl->specializations) {
            auto cast_func = dynamic_cast<ResolvedFuncDecl *>(func.get());
            if (!cast_func) {
                func->dump();
                dmz_unreachable("internal error: unexpected declaration in specializations");
            }
            generate_function_body(*cast_func);
        }
        return;
    }

    auto fnType = functionDecl.getFnType();

    m_currentFunction = &functionDecl;
    std::string funcName = generate_decl_name(functionDecl);
    auto *function = m_module->getFunction(funcName);
    if (!function) dmz_unreachable("internal error no function '" + funcName + "'");

    auto *entryBB = llvm::BasicBlock::Create(*m_context, "entry", function);
    m_builder.SetInsertPoint(entryBB);

    // Note: llvm:Instruction has a protected destructor.
    llvm::Value *undef = llvm::UndefValue::get(m_builder.getInt32Ty());
    m_allocaInsertPoint = new llvm::BitCastInst(undef, undef->getType(), "alloca.placeholder", entryBB);
    m_memsetInsertPoint = new llvm::BitCastInst(undef, undef->getType(), "memset.placeholder", entryBB);

    bool returnsVoid = fnType->returnType->kind == ResolvedTypeKind::Struct ||
                       fnType->returnType->kind == ResolvedTypeKind::Void ||
                       fnType->returnType->kind == ResolvedTypeKind::Optional;

    if (!returnsVoid) {
        debug_msg("retVal is not null");
        retVal = allocate_stack_variable("retval", *fnType->returnType);
    }
    retBB = llvm::BasicBlock::Create(*m_context, "return");

    int idx = 0;
    for (auto &&arg : function->args()) {
        if (arg.hasStructRetAttr()) {
            arg.setName("ret");
            debug_msg("retVal is in a arg");
            retVal = &arg;
            continue;
        }

        const auto *paramDecl = functionDecl.params[idx].get();
        arg.setName(paramDecl->identifier);

        llvm::Value *declVal = &arg;
        if (paramDecl->type->kind != ResolvedTypeKind::Struct && paramDecl->isMutable) {
            declVal = allocate_stack_variable(paramDecl->identifier, *paramDecl->type);
            store_value(&arg, declVal, *paramDecl->type, *paramDecl->type);
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

    m_builder.CreateRet(load_value(retVal, *fnType->returnType));

    m_currentFunction = nullptr;
}

llvm::StructType *Codegen::get_struct_decl(const ResolvedStructDecl &structDecl) {
    auto name = generate_decl_name(structDecl);
    auto structType = llvm::StructType::getTypeByName(*m_context, name);
    if (structType) return structType;

    auto ret = generate_struct_decl(structDecl);
    generate_struct_fields(structDecl);
    return ret;
}

llvm::StructType *Codegen::generate_struct_decl(const ResolvedStructDecl &structDecl) {
    debug_func(structDecl.symbolName);
    if (auto genStruct = dynamic_cast<const ResolvedGenericStructDecl *>(&structDecl)) {
        for (auto &&espec : genStruct->specializations) {
            generate_struct_decl(*espec);
        }
        return nullptr;
    }
    auto structType = llvm::StructType::create(*m_context, generate_decl_name(structDecl));
    debug_msg(Dumper([&]() { structType->print(llvm::errs()); }));

    for (auto &&func : structDecl.functions) {
        generate_function_decl(*func);
    }

    return structType;
}

void Codegen::generate_struct_fields(const ResolvedStructDecl &structDecl) {
    debug_func(structDecl.symbolName);
    if (auto genStruct = dynamic_cast<const ResolvedGenericStructDecl *>(&structDecl)) {
        for (auto &&espec : genStruct->specializations) {
            generate_struct_fields(*espec);
        }
        return;
    }
    auto *type = static_cast<llvm::StructType *>(generate_type(*structDecl.type));

    if (!type->isOpaque()) {
        debug_msg("already generated " << structDecl.symbolName);
        return;
    }

    std::vector<llvm::Type *> fieldTypes;
    for (auto &&field : structDecl.fields) {
        llvm::Type *t = generate_type(*field->type);
        fieldTypes.emplace_back(t);
    }

    type->setBody(fieldTypes, structDecl.isPacked);
}

void Codegen::generate_struct_functions(const ResolvedStructDecl &structDecl) {
    debug_func(structDecl.symbolName);
    if (auto genStruct = dynamic_cast<const ResolvedGenericStructDecl *>(&structDecl)) {
        for (auto &&espec : genStruct->specializations) {
            generate_struct_functions(*espec);
        }
        return;
    }
    for (auto &&func : structDecl.functions) {
        generate_function_body(*func);
    }
}

void Codegen::generate_error_no_err() {
    debug_func("");
    if (m_success) return;
    std::string str("SUCCESS");
    llvm::Constant *stringConst = llvm::ConstantDataArray::getString(*m_context, str, true);
    m_success =
        new llvm::GlobalVariable(*m_module, stringConst->getType(), true,
                                 llvm::GlobalVariable::LinkageTypes::PrivateLinkage, stringConst, "error.str." + str);
}

void Codegen::generate_error_group_expr_decl(const ResolvedErrorGroupExprDecl &ErrorGroupExprDecl) {
    debug_func("");
    for (auto &error : ErrorGroupExprDecl.errors) {
        std::string errName = "error.str." + error->identifier;
        auto global = m_module->getGlobalVariable(errName);
        if (!global) {
            llvm::Constant *stringConst = llvm::ConstantDataArray::getString(*m_context, error->identifier, true);
            m_declarations[error.get()] =
                new llvm::GlobalVariable(*m_module, stringConst->getType(), true,
                                         llvm::GlobalVariable::LinkageTypes::PrivateLinkage, stringConst, errName);
        }
    }
}

void Codegen::generate_module_decl(const ResolvedModuleDecl &moduleDecl) {
    debug_func("");
    generate_in_module_decl(moduleDecl.declarations);
}

void Codegen::generate_module_body(const ResolvedModuleDecl &moduleDecl) {
    debug_func("");
    generate_in_module_body(moduleDecl.declarations);
}

void Codegen::generate_in_module_decl(const std::vector<ptr<ResolvedDecl>> &declarations) {
    debug_func("");
    generate_error_no_err();
    for (auto &&decl : declarations) {
        if (const auto *sd = dynamic_cast<const ResolvedStructDecl *>(decl.get())) {
            generate_struct_decl(*sd);
        } else if (const auto *ds = dynamic_cast<const ResolvedDeclStmt *>(decl.get())) {
            generate_global_var_decl(*ds);
        } else if (dynamic_cast<const ResolvedFuncDecl *>(decl.get()) ||
                   dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
            continue;
        } else {
            decl->dump();
            dmz_unreachable("unexpected top level in module declaration");
        }
    }

    for (auto &&decl : declarations) {
        if (const auto *modDecl = dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
            generate_module_decl(*modDecl);
        }
    }
    for (auto &&decl : declarations) {
        if (const auto *fn = dynamic_cast<const ResolvedFuncDecl *>(decl.get())) {
            generate_function_decl(*fn);
        } else if (dynamic_cast<const ResolvedModuleDecl *>(decl.get()) ||
                   dynamic_cast<const ResolvedStructDecl *>(decl.get()) ||
                   dynamic_cast<const ResolvedDeclStmt *>(decl.get())) {
            continue;
        } else {
            decl->dump();
            dmz_unreachable("unexpected top level in module declaration");
        }
    }
}

void Codegen::generate_in_module_body(const std::vector<ptr<ResolvedDecl>> &declarations) {
    debug_func("");
    for (auto &&decl : declarations) {
        if (dynamic_cast<const ResolvedDeclStmt *>(decl.get()) || dynamic_cast<const ResolvedFuncDecl *>(decl.get()) ||
            dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
            continue;
        } else if (const auto *sd = dynamic_cast<const ResolvedStructDecl *>(decl.get())) {
            generate_struct_fields(*sd);
        } else {
            decl->dump();
            dmz_unreachable("unexpected top level in module declaration");
        }
    }
    for (auto &&decl : declarations) {
        if (const auto *modDecl = dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
            generate_module_body(*modDecl);
        }
    }
    debug_msg("Finish structs bodys");
    for (auto &&decl : declarations) {
        if (dynamic_cast<const ResolvedExternFunctionDecl *>(decl.get()) ||
            dynamic_cast<const ResolvedDeclStmt *>(decl.get()) ||
            dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
            continue;
        } else if (const auto *sd = dynamic_cast<const ResolvedStructDecl *>(decl.get())) {
            generate_struct_functions(*sd);
        } else if (const auto *fn = dynamic_cast<const ResolvedFuncDecl *>(decl.get())) {
            generate_function_body(*fn);
        } else {
            decl->dump();
            dmz_unreachable("unexpected top level in module declaration");
        }
    }
}

void Codegen::generate_global_var_decl(const ResolvedDeclStmt &stmt) {
    debug_func("");
    if (stmt.type->kind == ResolvedTypeKind::Module || stmt.type->kind == ResolvedTypeKind::Function ||
        stmt.type->kind == ResolvedTypeKind::StructDecl)
        return;

    if (stmt.type->kind == ResolvedTypeKind::ErrorGroup) {
        if (auto errorGroup = dynamic_cast<ResolvedErrorGroupExprDecl *>(stmt.varDecl->initializer.get())) {
            generate_error_group_expr_decl(*errorGroup);
        } else {
            stmt.varDecl->initializer->dump();
            dmz_unreachable("unexpected declaration instead of error group");
        }
        return;
    }

    llvm::Constant *initializer = nullptr;
    if (auto constVal = stmt.varDecl->initializer->get_constant_value()) {
        initializer = m_builder.getInt32(*constVal);
    }
    auto globalVar =
        new llvm::GlobalVariable(generate_type(*stmt.type), !stmt.isMutable,
                                 llvm::GlobalValue::LinkageTypes::InternalLinkage, initializer, stmt.identifier);
    m_module->insertGlobalVariable(globalVar);
    m_declarations[&stmt] = globalVar;
}
}  // namespace DMZ
