// #define DEBUG
#include "codegen/Codegen.hpp"

#include "Stats.hpp"

namespace DMZ {
Codegen::Codegen(const std::vector<std::unique_ptr<ResolvedDecl>> &resolvedTree, std::string_view sourcePath)
    : m_resolvedTree(resolvedTree),
      m_context(get_shared_context().getContext()),
      m_builder(*m_context),
      m_module(std::make_unique<llvm::Module>("<translation_unit>", *m_context)) {
    m_module->setSourceFileName(sourcePath);
    m_module->setTargetTriple(llvm::sys::getDefaultTargetTriple());
}

std::unique_ptr<llvm::orc::ThreadSafeModule> Codegen::generate_ir(bool runTest) {
    debug_func("");
    ScopedTimer(StatType::Codegen);
    // TODO: rethink lock to not lock all threads
    auto lock = get_shared_context().getLock();

    generate_in_module_decl(m_resolvedTree);
    generate_in_module_body(m_resolvedTree);

    generate_main_wrapper(runTest);

    return std::make_unique<llvm::orc::ThreadSafeModule>(std::move(m_module), get_shared_context());
}

llvm::Type *Codegen::generate_type(const Type &type, bool noOpaque) {
    llvm::Type *ret = nullptr;
    debug_func("In type: '" << type << "' out type '" << Dumper([&ret]() {
                   if (ret)
                       ret->print(llvm::errs());
                   else
                       std::cerr << "null";
               }) << "'");
    if (type.kind == Type::Kind::Error || type.isPointer) {
        debug_msg("isPointer or error");
        ret = llvm::PointerType::get(*m_context, 0);
        return ret;
    }
    if (type.kind == Type::Kind::Void) {
        debug_msg("kind Void");
        ret = m_builder.getVoidTy();
    }
    if (type.kind == Type::Kind::Int || type.kind == Type::Kind::UInt) {
        debug_msg("kind Int or UInt");
        ret = m_builder.getIntNTy(type.size);
    }
    if (type.kind == Type::Kind::Float) {
        debug_msg("kind Int or UInt");
        switch (type.size) {
            case 16:
                ret = m_builder.getHalfTy();
                break;
            case 32:
                ret = m_builder.getFloatTy();
                break;
            case 64:
                ret = m_builder.getDoubleTy();
                break;
            default:
                dmz_unreachable("float type have an incorrect size");
                break;
        }
    }
    if (type.kind == Type::Kind::Struct) {
        std::string name = generate_decl_name(*type.decl);
        debug_msg("struct '" << name << "'");
        auto structType = llvm::StructType::getTypeByName(*m_context, name);
        ret = structType;
        if (!ret) {
            dmz_unreachable("cannot get type '" + name + "'");
        }
        if (noOpaque && structType->isOpaque()) {
            if (auto structDecl = dynamic_cast<ResolvedStructDecl *>(type.decl)) {
                generate_struct_fields(*structDecl);
                ret = llvm::StructType::getTypeByName(*m_context, name);
                if (!ret) dmz_unreachable("unexpected error generating struct decl");
            } else {
                dmz_unreachable("expected resolved struct decl");
            }
        }
    }
    if (ret == nullptr) {
        debug_msg("null in ret VOID");
        return m_builder.getVoidTy();
    }

    if (ret != nullptr && type.isArray) {
        debug_msg("is array");
        if (*type.isArray != 0) {
            ret = llvm::ArrayType::get(ret, *type.isArray);
        } else {
            ret = llvm::PointerType::get(ret, 0);
        }
    }

    if (type.isOptional) {
        debug_msg("is optional");
        ret = generate_optional_type(type, ret);
    }
    return ret;
}

llvm::AllocaInst *Codegen::allocate_stack_variable(const std::string_view identifier, const Type &type) {
    debug_func("");
    debug_msg("m_allocaInsertPoint " << (void *)m_allocaInsertPoint);
    debug_msg("m_memsetInsertPoint " << (void *)m_memsetInsertPoint);
    assert(m_allocaInsertPoint != nullptr);
    assert(m_memsetInsertPoint != nullptr);
    llvm::IRBuilder<> tmpBuilder(*m_context);
    tmpBuilder.SetInsertPoint(m_allocaInsertPoint);
    auto value = tmpBuilder.CreateAlloca(generate_type(type, true), nullptr, identifier);
    // if (type.isOptional) {
    llvm::IRBuilder<> tmpBuilderMemset(*m_context);
    tmpBuilderMemset.SetInsertPoint(m_memsetInsertPoint);
    const llvm::DataLayout &dl = m_module->getDataLayout();
    tmpBuilderMemset.CreateMemSetInline(value, dl.getPrefTypeAlign(value->getType()), tmpBuilderMemset.getInt8(0),
                                        tmpBuilderMemset.getInt64(*value->getAllocationSize(dl)));
    // }
    return value;
}

void Codegen::generate_main_wrapper(bool runTest) {
    debug_func("");
    std::string mainToCall = "__builtin_main";
    if (runTest) {
        mainToCall = "__builtin_main_test";
    }
    auto *builtinMain = m_module->getFunction(mainToCall);
    if (!builtinMain) return;

    auto *main = llvm::Function::Create(llvm::FunctionType::get(m_builder.getInt32Ty(), {}, false),
                                        llvm::Function::ExternalLinkage, "main", *m_module);

    auto *entry = llvm::BasicBlock::Create(*m_context, "entry", main);
    m_builder.SetInsertPoint(entry);

    if (builtinMain) m_builder.CreateCall(builtinMain);

    m_builder.CreateRet(llvm::ConstantInt::getSigned(m_builder.getInt32Ty(), 0));
}

llvm::Value *Codegen::to_bool(llvm::Value *v, const Type &type) {
    debug_func("");
    // println("type: " << type.to_str());
    // v->dump();
    if (type.isPointer || type.kind == Type::Kind::Error) {
        v = m_builder.CreatePtrToInt(v, m_builder.getInt64Ty());
        return m_builder.CreateICmpNE(v, m_builder.getInt64(0), "ptr.to.bool");
    } else if (type.kind == Type::Kind::Int) {
        if (type.size == 1) return v;
        return m_builder.CreateICmpNE(v, llvm::ConstantInt::get(generate_type(type), 0, type.kind == Type::Kind::Int),
                                      "int.to.bool");
    } else if (type.kind == Type::Kind::UInt) {
        if (type.size == 1) return v;
        return m_builder.CreateICmpNE(v, llvm::ConstantInt::get(generate_type(type), 0, type.kind == Type::Kind::Int),
                                      "uint.to.bool");
    } else if (type.kind == Type::Kind::Float) {
        return m_builder.CreateFCmpONE(v, llvm::ConstantFP::get(generate_type(type), 0.0), "float.to.bool");
    } else {
        type.dump();
        dmz_unreachable("unsuported type in to_bool");
    }
}

llvm::Value *Codegen::cast_to(llvm::Value *v, const Type &from, const Type &to) {
    debug_func("From: '" << from.to_str() << "' to: '" << to.to_str() << "' of: '" << Dumper([&]() {
                   if (v)
                       v->print(llvm::errs());
                   else
                       std::cerr << "null";
               }) << "'");
    // m_module->dump();
    // v->dump();
    if (from.isPointer) {
        if (to.isPointer) {
            return v;
        } else {
            dmz_unreachable("unsuported type from ptr");
        }
    } else if (from.kind == Type::Kind::Int) {
        if (to.kind == Type::Kind::Int) {
            if (from.size == 1) return m_builder.CreateZExtOrTrunc(v, generate_type(to), "bool.to.int");
            return m_builder.CreateSExtOrTrunc(v, generate_type(to), "int.to.int");
        } else if (to.kind == Type::Kind::UInt) {
            return m_builder.CreateSExtOrTrunc(v, generate_type(to), "int.to.uint");
        } else if (to.kind == Type::Kind::Float) {
            return m_builder.CreateSIToFP(v, generate_type(to), "int.to.float");
        } else {
            dmz_unreachable("unsuported type from Int");
        }
    } else if (from.kind == Type::Kind::UInt) {
        if (to.kind == Type::Kind::Int) {
            return m_builder.CreateZExtOrTrunc(v, generate_type(to), "uint.to.int");
        } else if (to.kind == Type::Kind::UInt) {
            return m_builder.CreateZExtOrTrunc(v, generate_type(to), "uint.to.uint");
        } else if (to.kind == Type::Kind::Float) {
            return m_builder.CreateUIToFP(v, generate_type(to));
        } else {
            dmz_unreachable("unsuported type from UInt");
        }
    } else if (from.kind == Type::Kind::Float) {
        if (to.kind == Type::Kind::Int) {
            return m_builder.CreateFPToSI(v, generate_type(to), "uint.to.int");
        } else if (to.kind == Type::Kind::UInt) {
            return m_builder.CreateFPToUI(v, generate_type(to), "uint.to.uint");
        } else if (to.kind == Type::Kind::Float) {
            if (from.size > to.size) {
                return m_builder.CreateFPTrunc(v, generate_type(to));
            } else if (from.size < to.size) {
                return m_builder.CreateFPExt(v, generate_type(to));
            } else {
                return v;
            }
        } else {
            dmz_unreachable("unsuported type from UInt");
        }
    } else if (from.kind == Type::Kind::Error) {
        if (to.kind == Type::Kind::Error) {
            return v;
        } else {
            dmz_unreachable("unsuported type from Err");
        }
    } else {
        println("From: " << from.to_str() << " to: " << to.to_str());
        dmz_unreachable("unsuported type in cast_to");
    }
}

llvm::Function *Codegen::get_current_function() { return m_builder.GetInsertBlock()->getParent(); };

void Codegen::break_into_bb(llvm::BasicBlock *targetBB) {
    debug_func("");
    llvm::BasicBlock *currentBB = m_builder.GetInsertBlock();

    if (currentBB && !currentBB->getTerminator()) m_builder.CreateBr(targetBB);

    m_builder.ClearInsertionPoint();
}

llvm::Value *Codegen::store_value(llvm::Value *val, llvm::Value *ptr, const Type &from, const Type &to) {
    debug_func("from " << from << " to " << to);
    if (!from.isPointer) {
        if (from.kind == Type::Kind::Struct || from.isOptional) {
            const llvm::DataLayout &dl = m_module->getDataLayout();
            const llvm::StructLayout *sl = dl.getStructLayout(static_cast<llvm::StructType *>(generate_type(from)));

            return m_builder.CreateMemCpy(ptr, sl->getAlignment(), val, sl->getAlignment(), sl->getSizeInBytes());
        }
        if (from.isArray) {
            const llvm::DataLayout &dl = m_module->getDataLayout();
            auto t = generate_type(from);

            return m_builder.CreateMemCpy(ptr, dl.getPrefTypeAlign(t), val, dl.getPrefTypeAlign(t),
                                          dl.getTypeAllocSize(t));
        }
    }

    return m_builder.CreateStore(cast_to(val, from, to), ptr);
}

llvm::Value *Codegen::load_value(llvm::Value *v, Type type) {
    debug_func("");
    return m_builder.CreateLoad(generate_type(type), v);
}

llvm::Type *Codegen::generate_optional_type(const Type &type, llvm::Type *llvmType) {
    debug_func("");
    std::string structName("error.struct." + type.withoutOptional().to_str());
    auto ret = llvm::StructType::getTypeByName(*m_context, structName);
    if (!ret) {
        ret = llvm::StructType::create(*m_context, structName);

        std::vector<llvm::Type *> fieldTypes;
        if (type.kind == Type::Kind::Void) {
            fieldTypes.emplace_back(m_builder.getInt1Ty());
        } else {
            fieldTypes.emplace_back(llvmType);
        }
        fieldTypes.emplace_back(generate_type(Type::builtinError("error")));
        ret->setBody(fieldTypes);
    }
    return ret;
}

}  // namespace DMZ
