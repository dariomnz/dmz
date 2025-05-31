#include "codegen/Codegen.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/TargetParser/Host.h>
#pragma GCC diagnostic pop

namespace DMZ {

void Codegen::generate_function_decl(const ResolvedFuncDecl &functionDecl) {
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
    std::string modIdentifier = dynamic_cast<const ResolvedExternFunctionDecl *>(&functionDecl)
                                    ? std::string(functionDecl.identifier)
                                    : functionDecl.modIdentifier;
    std::string funcName = functionDecl.identifier == "main" ? "__builtin_main" : generate_symbol_name(modIdentifier);
    auto *fn = llvm::Function::Create(type, llvm::Function::ExternalLinkage, funcName, *m_module);
    fn->setAttributes(construct_attr_list(functionDecl));
}

llvm::AttributeList Codegen::construct_attr_list(const ResolvedFuncDecl &fn) {
    bool isReturningStruct = fn.type.kind == Type::Kind::Struct || fn.type.isOptional;
    std::vector<llvm::AttributeSet> argsAttrSets;

    if (isReturningStruct) {
        llvm::AttrBuilder retAttrs(*m_context);
        retAttrs.addStructRetAttr(generate_type(fn.type));
        argsAttrSets.emplace_back(llvm::AttributeSet::get(*m_context, retAttrs));
    }

    for ([[maybe_unused]] auto &&param : fn.params) {
        llvm::AttrBuilder paramAttrs(*m_context);
        if (param->type.kind == Type::Kind::Struct) {
            if (param->isMutable) {
                if (param->type.isRef) {
                    paramAttrs.addByRefAttr(generate_type(param->type));
                } else {
                    paramAttrs.addByValAttr(generate_type(param->type));
                }
            } else {
                paramAttrs.addAttribute(llvm::Attribute::ReadOnly);
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

void Codegen::generate_function_body(const ResolvedFunctionDecl &functionDecl) {
    m_currentFunction = &functionDecl;
    std::string funcName =
        functionDecl.identifier == "main" ? "__builtin_main" : generate_symbol_name(functionDecl.modIdentifier);
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
            store_value(&arg, declVal, paramDecl->type);
        }

        m_declarations[paramDecl] = declVal;
        ++idx;
    }

    // if (functionDecl.identifier == "println")
    // generate_builtin_println_body(functionDecl);
    // else
    generate_block(*functionDecl.body);

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

// void Codegen::generate_builtin_println_body(const ResolvedFunctionDecl &println) {
//     auto *type = llvm::FunctionType::get(m_builder.getInt32Ty(), {m_builder.getInt8Ty()->getPointerTo()}, true);
//     auto *printf = llvm::Function::Create(type, llvm::Function::ExternalLinkage, "printf", m_module);
//     auto *format = m_builder.CreateGlobalStringPtr("%d\n");

//     llvm::Value *param = m_declarations[println.params[0].get()];

//     m_builder.CreateCall(printf, {format, param});
// }

void Codegen::generate_struct_decl(const ResolvedStructDecl &structDecl) {
    std::string structName("struct." + generate_symbol_name(structDecl.modIdentifier));
    llvm::StructType::create(*m_context, structName);
}

void Codegen::generate_struct_definition(const ResolvedStructDecl &structDecl) {
    auto *type = static_cast<llvm::StructType *>(generate_type(structDecl.type));

    std::vector<llvm::Type *> fieldTypes;
    for (auto &&field : structDecl.fields) {
        llvm::Type *t = generate_type(field->type);
        fieldTypes.emplace_back(t);
    }

    type->setBody(fieldTypes);
}

void Codegen::generate_err_no_err() {
    if (m_success) return;
    std::string str("SUCCESS");
    llvm::Constant *stringConst = llvm::ConstantDataArray::getString(*m_context, str, true);
    m_success =
        new llvm::GlobalVariable(*m_module, stringConst->getType(), true,
                                 llvm::GlobalVariable::LinkageTypes::PrivateLinkage, stringConst, "err.str." + str);
}

void Codegen::generate_err_group_decl(const ResolvedErrGroupDecl &errGroupDecl) {
    for (auto &err : errGroupDecl.errs) {
        auto symbol_name = generate_symbol_name(err->modIdentifier);
        llvm::Constant *stringConst = llvm::ConstantDataArray::getString(*m_context, symbol_name, true);
        m_declarations[err.get()] = new llvm::GlobalVariable(*m_module, stringConst->getType(), true,
                                                             llvm::GlobalVariable::LinkageTypes::PrivateLinkage,
                                                             stringConst, "err.str." + symbol_name);
    }
}

void Codegen::generate_module_decl(const ResolvedModuleDecl &moduleDecl) {
    if (moduleDecl.nestedModule) {
        generate_module_decl(*moduleDecl.nestedModule);
    } else {
        generate_in_module_decl(moduleDecl.declarations);
    }
}

void Codegen::generate_in_module_decl(const std::vector<std::unique_ptr<ResolvedDecl>> &declarations, bool isGlobal) {
    for (auto &&decl : declarations) {
        if (const auto *fn = dynamic_cast<const ResolvedFuncDecl *>(decl.get())) {
            if (fn->identifier == "get_errno") {
                generate_builtin_get_errno();
            } else {
                generate_function_decl(*fn);
            }
        } else if (const auto *sd = dynamic_cast<const ResolvedStructDecl *>(decl.get()))
            generate_struct_decl(*sd);
        else if (dynamic_cast<const ResolvedErrGroupDecl *>(decl.get()) ||
                 dynamic_cast<const ResolvedModuleDecl *>(decl.get()) ||
                 dynamic_cast<const ResolvedImportDecl *>(decl.get()))
            continue;
        else {
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
            dynamic_cast<const ResolvedImportDecl *>(decl.get()))
            continue;
        else if (const auto *fn = dynamic_cast<const ResolvedFunctionDecl *>(decl.get()))
            generate_function_body(*fn);
        else if (const auto *sd = dynamic_cast<const ResolvedStructDecl *>(decl.get()))
            generate_struct_definition(*sd);
        else if (const auto *modDecl = dynamic_cast<const ResolvedModuleDecl *>(decl.get()))
            generate_module_decl(*modDecl);
        else {
            decl->dump();
            dmz_unreachable("unexpected top level in module declaration");
        }
    }
}
}  // namespace DMZ
