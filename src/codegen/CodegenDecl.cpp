#ifdef DEBUG_CODEGEN
#ifndef DEBUG
#define DEBUG
#endif
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/IR/Module.h>
#pragma GCC diagnostic pop

#include "Debug.hpp"
#include "codegen/Codegen.hpp"

namespace DMZ {

std::string Codegen::generate_decl_name(const ResolvedDecl &decl) {
    std::string name;
    debug_func(Dumper([&name]() { std::cerr << name; }));
    if (dynamic_cast<const ResolvedFuncDecl *>(&decl)) {
        debug_msg("Generating decl name for: " << decl.name());
        if (auto functionDecl = dynamic_cast<const ResolvedFunctionDecl *>(&decl)) {
            if (functionDecl->isExport) {
                name = functionDecl->identifier;
                return name;
            }
        }
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
    name = decl.name();
    return name;
}

llvm::FunctionType *Codegen::generate_function_type(const ResolvedTypeFunction &fnType) {
    llvm::Type *retType = generate_type(*fnType.returnType);
    std::vector<llvm::Type *> paramTypes;

    if (fnType.returnType->generate_struct()) {
        paramTypes.emplace_back(llvm::PointerType::get(retType, 0));
        retType = m_builder.getVoidTy();
    }

    bool isVararg = false;
    const ResolvedFuncDecl *fd = fnType.fnDecl;
    for (size_t i = 0; i < fnType.paramsTypes.size(); i++) {
        if (fd && i < fd->params.size() && fd->params[i]->isComptime) {
            continue;
        }
        auto &&param = fnType.paramsTypes[i];
        if (param->kind == ResolvedTypeKind::VarArg) {
            isVararg = true;
            continue;
        }
        llvm::Type *paramType = generate_type(*param);
        if (param->generate_struct()) {
            paramType = llvm::PointerType::get(paramType, 0);
        }
        paramTypes.emplace_back(paramType);
    }

    return llvm::FunctionType::get(retType, paramTypes, isVararg);
}

llvm::Function *Codegen::generate_function_decl(const ResolvedFuncDecl &functionDecl) {
    debug_func(functionDecl.name());
    if (dynamic_cast<const ResolvedBuiltinFunctionDecl *>(&functionDecl)) {
        return nullptr;
    }
    if (dynamic_cast<const ResolvedGenericFunctionDecl *>(&functionDecl)) {
        return nullptr;
    }

    auto llvmFnDecl = m_module->getFunction(generate_decl_name(functionDecl));
    if (llvmFnDecl) return llvmFnDecl;

    auto fnType = functionDecl.getFnType();

    auto *type = generate_function_type(*fnType);
    std::string funcName = generate_decl_name(functionDecl);
    auto *fn = llvm::Function::Create(type, llvm::Function::ExternalLinkage, funcName, *m_module);
    fn->setAttributes(construct_attr_list(*fnType));

    generate_decl(functionDecl);

    return fn;
}

llvm::AttributeList Codegen::construct_attr_list(const ResolvedTypeFunction &fnType) {
    debug_func(fnType.to_str());

    bool isReturningStruct = fnType.returnType->generate_struct();
    std::vector<llvm::AttributeSet> argsAttrSets;

    if (isReturningStruct) {
        llvm::AttrBuilder retAttrs(*m_context);
        retAttrs.addStructRetAttr(generate_type(*fnType.returnType));
        argsAttrSets.emplace_back(llvm::AttributeSet::get(*m_context, retAttrs));
    }

    for (size_t i = 0; i < fnType.paramsTypes.size(); i++) {
        if (fnType.fnDecl && i < fnType.fnDecl->params.size() && fnType.fnDecl->params[i]->isComptime) {
            continue;
        }
        auto &&param = fnType.paramsTypes[i];
        debug_msg("Param: " << param->to_str());
        llvm::AttrBuilder paramAttrs(*m_context);
        if (auto typePrt = dynamic_cast<ResolvedTypePointer *>(param.get())) {
            if (typePrt->pointerType->kind != ResolvedTypeKind::Void &&
                typePrt->pointerType->kind != ResolvedTypeKind::Function) {
                paramAttrs.addByRefAttr(generate_type(*typePrt->pointerType));
            }
        } else if (param->kind == ResolvedTypeKind::Struct) {
            paramAttrs.addAttribute(llvm::Attribute::ReadOnly);
        } else if (param->generate_struct()) {
            paramAttrs.addByValAttr(generate_type(*param));
        }
        argsAttrSets.emplace_back(llvm::AttributeSet::get(*m_context, paramAttrs));
    }

    return llvm::AttributeList::get(*m_context, llvm::AttributeSet{}, llvm::AttributeSet{}, argsAttrSets);
}

void Codegen::generate_function_body(const ResolvedFuncDecl &functionDecl) {
    debug_func(functionDecl.name() << " " << functionDecl.type->to_str());
    if (dynamic_cast<const ResolvedBuiltinFunctionDecl *>(&functionDecl)) {
        return;
    }
    if (auto resolvedFunctionDecl = dynamic_cast<const ResolvedGenericFunctionDecl *>(&functionDecl)) {
        for (auto &&func : resolvedFunctionDecl->specializations) {
            auto cast_func = dynamic_cast<ResolvedFuncDecl *>(func.get());
            if (!cast_func) {
                func->dump();
                dmz_unreachable(functionDecl.location, "internal error: unexpected declaration in specializations");
            }

            if (func->state != ResolvedState::FullyResolved) {
                debug_msg(func->symbolName << " is not fully resolved skiping");
                continue;
            }
            if (func->specializedTypes->is_generic()) continue;
            generate_function_body(*cast_func);
        }
        return;
    }

    auto fnType = functionDecl.getFnType();

    m_currentFunction = &functionDecl;
    std::string funcName = generate_decl_name(functionDecl);
    auto *function = m_module->getFunction(funcName);
    if (!function) dmz_unreachable(functionDecl.location, "internal error no function '" + funcName + "'");

    if (!function->empty()) return;
    if (dynamic_cast<const ResolvedExternFunctionDecl *>(&functionDecl)) return;

    ptr<DebugScopeRAII> debugScope = nullptr;
    if (m_debugSymbols) {
        llvm::DISubprogram *subProgram = m_debugBuilder.createFunction(
            m_currentDebugScope, functionDecl.name(), llvm::StringRef(), m_currentDebugFile, functionDecl.location.line,
            static_cast<llvm::DISubroutineType *>(generate_debug_type(*fnType)), functionDecl.location.line,
            llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
        function->setSubprogram(subProgram);
        debugScope = makePtr<DebugScopeRAII>(*this, subProgram);
    }
    set_debug_location(functionDecl.location);
    defer([&]() { unset_debug_location(); });

    auto *entryBB = llvm::BasicBlock::Create(*m_context, "entry", function);
    m_builder.SetInsertPoint(entryBB);

    // Note: llvm:Instruction has a protected destructor.
    llvm::Value *undef = llvm::UndefValue::get(m_builder.getInt32Ty());
    m_allocaInsertPoint = new llvm::BitCastInst(undef, undef->getType(), "alloca.placeholder", entryBB);
    m_memsetInsertPoint = new llvm::BitCastInst(undef, undef->getType(), "memset.placeholder", entryBB);

    bool returnsVoid = fnType->returnType->generate_struct() || fnType->returnType->kind == ResolvedTypeKind::Void;

    if (!returnsVoid) {
        debug_msg("retVal is not null");
        retVal = allocate_stack_variable(functionDecl.location, "retval", *fnType->returnType);
    }
    retBB = llvm::BasicBlock::Create(*m_context, "return");
    int idx = 0;
    for (auto &&arg : function->args()) {
        if (arg.hasStructRetAttr()) {
            arg.setName("ret");
            // Prevent return optional with previous values
            const llvm::DataLayout &dl = m_module->getDataLayout();
            auto retType = generate_type(*fnType->returnType, true);
            m_builder.CreateMemSetInline(
                &arg, dl.getPrefTypeAlign(retType), m_builder.getInt8(0),
                llvm::ConstantInt::get(m_builder.getIntPtrTy(dl), dl.getTypeAllocSize(retType)));
            debug_msg("retVal is in a arg");
            retVal = &arg;
            continue;
        }

        const auto *paramDecl = functionDecl.params[idx].get();
        if (paramDecl->isComptime) {
            ++idx;
            continue;
        }
        // arg.setName(paramDecl->identifier);

        llvm::Value *declVal = &arg;
        if (!paramDecl->type->generate_struct()) {
            declVal = allocate_stack_variable(paramDecl->location, paramDecl->identifier, *paramDecl->type);
            store_value(&arg, declVal, *paramDecl->type, *paramDecl->type);
        }
        if (m_debugSymbols) {
            llvm::DILocalVariable *D = m_debugBuilder.createParameterVariable(
                m_currentDebugScope, paramDecl->name(), idx + 1, m_currentDebugFile, paramDecl->location.line,
                generate_debug_type(*paramDecl->type), true);

            m_debugBuilder.insertDeclare(declVal, D, m_debugBuilder.createExpression(),
                                         llvm::DILocation::get(*m_context, paramDecl->location.line,
                                                               paramDecl->location.col, m_currentDebugScope),
                                         m_builder.GetInsertBlock());
        }

        m_declarations[paramDecl] = declVal;
        ++idx;
    }

    // if (functionDecl.identifier == "println")
    // generate_builtin_println_body(functionDecl);
    // else
    ResolvedBlock *body = nullptr;
    if (auto function = dynamic_cast<const ResolvedFunctionDecl *>(&functionDecl)) {
        body = function->body.get();
    }
    if (!body) {
        functionDecl.dump();
        dmz_unreachable(functionDecl.location, "unexpected void body");
    }
    generate_block(*body);

    if (retBB->hasNPredecessorsOrMore(1)) {
        break_into_bb(retBB);
        retBB->insertInto(function);
        m_builder.SetInsertPoint(retBB);
    } else {
        delete retBB;
        retBB = nullptr;
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

    generate_decl(structDecl);
    return llvm::StructType::getTypeByName(*m_context, name);
}

llvm::StructType *Codegen::generate_struct_decl(const ResolvedStructDecl &structDecl) {
    debug_func(structDecl.name());
    if (dynamic_cast<const ResolvedGenericStructDecl *>(&structDecl)) {
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
    debug_func(structDecl.name());
    if (dynamic_cast<const ResolvedGenericStructDecl *>(&structDecl)) {
        return;
    }
    auto *type = static_cast<llvm::StructType *>(generate_type(*structDecl.type));

    if (!type->isOpaque()) {
        debug_msg("already generated " << structDecl.name());
        return;
    }

    std::vector<llvm::Type *> fieldTypes;
    for (auto &&field : structDecl.fields) {
        llvm::Type *t = generate_type(*field->type, true);
        fieldTypes.emplace_back(t);
    }

    type->setBody(fieldTypes, structDecl.isPacked);
}

void Codegen::generate_struct_functions(const ResolvedStructDecl &structDecl) {
    debug_func(structDecl.name());
    if (dynamic_cast<const ResolvedGenericStructDecl *>(&structDecl)) {
        return;
    }
    for (auto &&func : structDecl.functions) {
        generate_function_body(*func);
    }
}

llvm::StructType *Codegen::get_union_decl(const ResolvedUnionDecl &unionDecl) {
    auto name = generate_decl_name(unionDecl);
    auto unionType = llvm::StructType::getTypeByName(*m_context, name);
    if (unionType) return unionType;

    generate_decl(unionDecl);
    return llvm::StructType::getTypeByName(*m_context, name);
}

llvm::StructType *Codegen::generate_union_decl(const ResolvedUnionDecl &unionDecl) {
    debug_func(unionDecl.name());
    auto unionType = llvm::StructType::create(*m_context, generate_decl_name(unionDecl));
    debug_msg(Dumper([&]() { unionType->print(llvm::errs()); }));

    for (auto &&func : unionDecl.functions) {
        generate_function_decl(*func);
    }

    return unionType;
}

void Codegen::generate_union_fields(const ResolvedUnionDecl &unionDecl) {
    debug_func(unionDecl.name());
    auto *type = static_cast<llvm::StructType *>(generate_type(*unionDecl.type));

    if (!type->isOpaque()) {
        debug_msg("already generated " << unionDecl.name());
        return;
    }

    uint64_t maxSize = 0;
    const llvm::DataLayout &dl = m_module->getDataLayout();
    for (auto &&field : unionDecl.fields) {
        if (field->type->kind == ResolvedTypeKind::Void) continue;
        llvm::Type *t = generate_type(*field->type, true);
        auto size = dl.getTypeAllocSize(t).getFixedValue();
        debug_msg(field->type->to_str() << " size: " << size);
        maxSize = std::max(maxSize, size);
    }

    std::vector<llvm::Type *> fieldTypes;
    fieldTypes.emplace_back(generate_type(*unionDecl.tag->type, true));             // Tag
    fieldTypes.emplace_back(llvm::ArrayType::get(m_builder.getInt8Ty(), maxSize));  // Payload

    type->setBody(fieldTypes, false);
}

void Codegen::generate_union_functions(const ResolvedUnionDecl &unionDecl) {
    debug_func(unionDecl.name());
    for (auto &&func : unionDecl.functions) {
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
        generate_error_decl(*error);
    }
}

llvm::Value *Codegen::generate_error_decl(const ResolvedErrorDecl &errorDecl) {
    debug_func(errorDecl.identifier);
    std::string errName = "error.str." + errorDecl.identifier;
    auto global = m_module->getNamedGlobal(errName);
    if (!global) {
        llvm::Constant *stringConst = llvm::ConstantDataArray::getString(*m_context, errorDecl.identifier, true);
        global = new llvm::GlobalVariable(*m_module, stringConst->getType(), true,
                                          llvm::GlobalVariable::LinkageTypes::PrivateLinkage, stringConst, errName);
    }
    m_declarations[&errorDecl] = global;
    return global;
}

void Codegen::generate_module_decl(const ResolvedModuleDecl &moduleDecl) {
    debug_func("");
    auto prevModule = m_currentModule;
    defer([&]() mutable { m_currentModule = prevModule; });
    m_currentModule = &moduleDecl;
    ptr<DebugScopeRAII> debugScope = nullptr;
    if (m_debugSymbols) {
        m_currentDebugFile = m_debugBuilder.createFile(moduleDecl.module_path.filename().string(),
                                                       moduleDecl.module_path.parent_path().string());
        debugScope = makePtr<DebugScopeRAII>(*this, m_currentDebugFile);
    }
    generate_in_module_decl(moduleDecl.declarations);
}

void Codegen::generate_module_body(const ResolvedModuleDecl &moduleDecl) {
    debug_func("");
    auto prevModule = m_currentModule;
    defer([&]() mutable { m_currentModule = prevModule; });
    m_currentModule = &moduleDecl;
    ptr<DebugScopeRAII> debugScope = nullptr;
    if (m_debugSymbols) {
        m_currentDebugFile = m_debugBuilder.createFile(moduleDecl.module_path.filename().string(),
                                                       moduleDecl.module_path.parent_path().string());
        debugScope = makePtr<DebugScopeRAII>(*this, m_currentDebugFile);
    }
    generate_in_module_body(moduleDecl.declarations);
}

void Codegen::generate_in_module_decl(const std::vector<ptr<ResolvedDecl>> &declarations) {
    debug_func("");
    generate_error_no_err();
    debug_msg("Declarations: " << declarations.size());
    for (auto &&decl : declarations) {
        debug_msg(decl->name() << " " << decl->state);
        if (decl->state != ResolvedState::FullyResolved) {
            debug_msg(decl->symbolName << " is not fully resolved skiping");
            continue;
        }
        if (const auto *sd = dynamic_cast<const ResolvedStructDecl *>(decl.get())) {
            generate_struct_decl(*sd);
        } else if (const auto *ud = dynamic_cast<const ResolvedUnionDecl *>(decl.get())) {
            generate_union_decl(*ud);
        } else if (const auto *ds = dynamic_cast<const ResolvedDeclStmt *>(decl.get())) {
            generate_global_var_decl(*ds);
        } else if (dynamic_cast<const ResolvedFuncDecl *>(decl.get()) ||
                   dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
            continue;
        } else {
            decl->dump();
            dmz_unreachable(decl->location, "unexpected top level in module declaration");
        }
    }

    for (auto &&decl : declarations) {
        if (const auto *modDecl = dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
            generate_module_decl(*modDecl);
        }
    }
    for (auto &&decl : declarations) {
        if (decl->state != ResolvedState::FullyResolved) {
            debug_msg(decl->symbolName << " is not fully resolved skiping");
            continue;
        }
        if (const auto *fn = dynamic_cast<const ResolvedFuncDecl *>(decl.get())) {
            generate_function_decl(*fn);
        } else if (dynamic_cast<const ResolvedModuleDecl *>(decl.get()) ||
                   dynamic_cast<const ResolvedStructDecl *>(decl.get()) ||
                   dynamic_cast<const ResolvedUnionDecl *>(decl.get()) ||
                   dynamic_cast<const ResolvedDeclStmt *>(decl.get())) {
            continue;
        } else {
            decl->dump();
            dmz_unreachable(decl->location, "unexpected top level in module declaration");
        }
    }
}

void Codegen::generate_in_module_body(const std::vector<ptr<ResolvedDecl>> &declarations) {
    debug_func("");
    for (auto &&decl : declarations) {
        if (decl->state != ResolvedState::FullyResolved) {
            debug_msg(decl->symbolName << " is not fully resolved skiping");
            continue;
        }
        if (dynamic_cast<const ResolvedDeclStmt *>(decl.get()) || dynamic_cast<const ResolvedFuncDecl *>(decl.get()) ||
            dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
            continue;
        } else if (const auto *sd = dynamic_cast<const ResolvedStructDecl *>(decl.get())) {
            generate_struct_fields(*sd);
        } else if (const auto *ud = dynamic_cast<const ResolvedUnionDecl *>(decl.get())) {
            generate_union_fields(*ud);
        } else {
            decl->dump();
            dmz_unreachable(decl->location, "unexpected top level in module declaration");
        }
    }
    for (auto &&decl : declarations) {
        if (const auto *modDecl = dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
            generate_module_body(*modDecl);
        }
    }
    debug_msg("Finish structs bodys");
    for (auto &&decl : declarations) {
        if (decl->state != ResolvedState::FullyResolved) {
            debug_msg(decl->symbolName << " is not fully resolved skiping");
            continue;
        }
        if (dynamic_cast<const ResolvedExternFunctionDecl *>(decl.get()) ||
            dynamic_cast<const ResolvedDeclStmt *>(decl.get()) ||
            dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
            continue;
        } else if (const auto *sd = dynamic_cast<const ResolvedStructDecl *>(decl.get())) {
            generate_struct_functions(*sd);
        } else if (const auto *ud = dynamic_cast<const ResolvedUnionDecl *>(decl.get())) {
            generate_union_functions(*ud);
        } else if (const auto *fn = dynamic_cast<const ResolvedFuncDecl *>(decl.get())) {
            generate_function_body(*fn);
        } else {
            decl->dump();
            dmz_unreachable(decl->location, "unexpected top level in module declaration");
        }
    }
}

void Codegen::generate_global_var_decl(const ResolvedDeclStmt &stmt) {
    debug_func("");
    if (stmt.type->kind == ResolvedTypeKind::Module || stmt.type->kind == ResolvedTypeKind::Function ||
        stmt.type->kind == ResolvedTypeKind::StructDecl || stmt.type->kind == ResolvedTypeKind::UnionDecl ||
        stmt.type->kind == ResolvedTypeKind::Type)
        return;

    if (stmt.type->kind == ResolvedTypeKind::ErrorGroup) {
        if (auto errorGroup = dynamic_cast<ResolvedErrorGroupExprDecl *>(stmt.varDecl->initializer.get())) {
            generate_error_group_expr_decl(*errorGroup);
        } else {
            stmt.varDecl->initializer->dump();
            dmz_unreachable(stmt.varDecl->location, "unexpected declaration instead of error group");
        }
        return;
    }

    llvm::GlobalVariable *globalVar = nullptr;
    if (auto strLit = dynamic_cast<ResolvedStringLiteral *>(stmt.varDecl->initializer.get())) {
        llvm::GlobalVariable *ptr = create_global_string(strLit->value, "string.literal." + stmt.name());
        if (strLit->type->kind == ResolvedTypeKind::Slice) {
            auto sliceType = cast<llvm::StructType>(generate_type(*strLit->type));
            std::vector<llvm::Constant *> elements;
            elements.push_back(ptr);

            auto lengthTy = m_builder.getIntPtrTy(m_module->getDataLayout());
            elements.push_back(llvm::ConstantInt::get(lengthTy, strLit->value.size()));
            llvm::Constant *initializer = llvm::ConstantStruct::get(sliceType, elements);
            globalVar = new llvm::GlobalVariable(*m_module, sliceType, false, llvm::GlobalValue::ExternalLinkage,
                                                 initializer, stmt.name());

            m_declarations[&stmt] = globalVar;
            m_declarations[stmt.varDecl.get()] = globalVar;
        } else {
            globalVar = ptr;
            m_declarations[&stmt] = globalVar;
            m_declarations[stmt.varDecl.get()] = globalVar;
        }
    } else {
        llvm::Constant *initializer = nullptr;
        if (auto constVal = stmt.varDecl->initializer->get_constant_value()) {
            if (constVal->isInt()) {
                initializer = llvm::ConstantInt::get(generate_type(*stmt.type), constVal->getInt());
            } else if (constVal->isFloat()) {
                initializer = llvm::ConstantFP::get(generate_type(*stmt.type), constVal->getFloat());
            }
        }
        globalVar =
            new llvm::GlobalVariable(generate_type(*stmt.type), !stmt.isMutable,
                                     llvm::GlobalValue::LinkageTypes::InternalLinkage, initializer, stmt.name());
        m_module->insertGlobalVariable(globalVar);
        m_declarations[&stmt] = globalVar;
        m_declarations[stmt.varDecl.get()] = globalVar;
    }

    if (m_debugSymbols && globalVar) {
        bool isLocal =
            globalVar->hasLocalLinkage() || globalVar->hasInternalLinkage() || globalVar->hasPrivateLinkage();
        auto *gvDebug = m_debugBuilder.createGlobalVariableExpression(m_currentDebugScope, stmt.identifier, stmt.name(),
                                                                      m_currentDebugFile, stmt.location.line,
                                                                      generate_debug_type(*stmt.type), isLocal);
        globalVar->addDebugInfo(gvDebug);
    }
}

void Codegen::generate_pending_decls() {
    debug_func("");
    while (!m_pendingDecls.empty()) {
        auto pending = std::move(m_pendingDecls);
        for (auto *decl : pending) {
            generate_body(*decl);
        }
    }
}
}  // namespace DMZ
