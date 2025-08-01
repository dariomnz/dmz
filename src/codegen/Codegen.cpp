// #define DEBUG
#include "codegen/Codegen.hpp"

namespace DMZ {
Codegen::Codegen(const std::vector<std::unique_ptr<ResolvedDecl>> &resolvedTree, std::string_view sourcePath)
    : m_resolvedTree(resolvedTree),
      m_context(get_shared_context().getContext()),
      m_builder(*m_context),
      m_module(std::make_unique<llvm::Module>("<translation_unit>", *m_context)) {
    m_module->setSourceFileName(sourcePath);
    m_module->setTargetTriple(llvm::sys::getDefaultTargetTriple());
}

std::unique_ptr<llvm::orc::ThreadSafeModule> Codegen::generate_ir() {
    debug_func("");
    ScopedTimer st(Stats::type::codegenTime);
    // TODO: rethink lock to not lock all threads
    auto lock = get_shared_context().getLock();

    generate_in_module_decl(m_resolvedTree, true);

    generate_main_wrapper();

    return std::make_unique<llvm::orc::ThreadSafeModule>(std::move(m_module), get_shared_context());
}

llvm::Type *Codegen::generate_type(const Type &type) {
    debug_func(type);
    llvm::Type *ret = nullptr;
    if (type.kind == Type::Kind::Err || type.isPointer) {
        ret = llvm::PointerType::get(*m_context, 0);
        return ret;
    }
    if (type.kind == Type::Kind::Void) {
        ret = m_builder.getVoidTy();
    }
    if (type.kind == Type::Kind::Int || type.kind == Type::Kind::UInt) {
        ret = m_builder.getIntNTy(type.size);
    }
    if (type.kind == Type::Kind::Float) {
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
        debug_msg(name);
        ret = llvm::StructType::getTypeByName(*m_context, name);
        // ret->dump();
        // ret = llvm::StructType::getTypeByName(*m_context, "struct." + std::string(type.name));
    }
    if (ret == nullptr) return m_builder.getVoidTy();

    if (ret != nullptr && type.isArray) {
        if (*type.isArray != 0) {
            ret = llvm::ArrayType::get(ret, *type.isArray);
        } else {
            ret = llvm::PointerType::get(ret, 0);
        }
    }

    if (type.isOptional) {
        ret = generate_optional_type(type, ret);
    }
    return ret;
}

llvm::AllocaInst *Codegen::allocate_stack_variable(const std::string_view identifier, const Type &type) {
    debug_func("");
    llvm::IRBuilder<> tmpBuilder(*m_context);
    tmpBuilder.SetInsertPoint(m_allocaInsertPoint);
    auto value = tmpBuilder.CreateAlloca(generate_type(type), nullptr, identifier);
    // if (type.isOptional) {
    llvm::IRBuilder<> tmpBuilderMemset(*m_context);
    tmpBuilderMemset.SetInsertPoint(m_memsetInsertPoint);
    const llvm::DataLayout &dl = m_module->getDataLayout();
    tmpBuilderMemset.CreateMemSetInline(value, dl.getPrefTypeAlign(value->getType()), tmpBuilderMemset.getInt8(0),
                                        tmpBuilderMemset.getInt64(*value->getAllocationSize(dl)));
    // }
    return value;
}

void Codegen::generate_main_wrapper() {
    debug_func("");
    auto *builtinMain = m_module->getFunction("__builtin_main");
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
    if (type.isPointer || type.kind == Type::Kind::Err) {
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
    debug_func("");
    // println("From: " << from.to_str() << " to: " << to.to_str());
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
    } else if (from.kind == Type::Kind::Err) {
        if (to.kind == Type::Kind::Err) {
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
    debug_func("");
    if (from.kind == Type::Kind::Struct || from.isOptional) {
        const llvm::DataLayout &dl = m_module->getDataLayout();
        const llvm::StructLayout *sl = dl.getStructLayout(static_cast<llvm::StructType *>(generate_type(from)));

        return m_builder.CreateMemCpy(ptr, sl->getAlignment(), val, sl->getAlignment(), sl->getSizeInBytes());
    }
    if (from.isArray) {
        const llvm::DataLayout &dl = m_module->getDataLayout();
        auto t = generate_type(from);

        return m_builder.CreateMemCpy(ptr, dl.getPrefTypeAlign(t), val, dl.getPrefTypeAlign(t), dl.getTypeAllocSize(t));
    }

    return m_builder.CreateStore(cast_to(val, from, to), ptr);
}

llvm::Value *Codegen::load_value(llvm::Value *v, Type type) {
    debug_func("");
    return m_builder.CreateLoad(generate_type(type), v);
}

llvm::Type *Codegen::generate_optional_type(const Type &type, llvm::Type *llvmType) {
    debug_func("");
    std::string structName("err.struct." + type.withoutOptional().to_str());
    auto ret = llvm::StructType::getTypeByName(*m_context, structName);
    if (!ret) {
        ret = llvm::StructType::create(*m_context, structName);

        std::vector<llvm::Type *> fieldTypes;
        if (type.kind == Type::Kind::Void) {
            fieldTypes.emplace_back(m_builder.getInt1Ty());
        } else {
            fieldTypes.emplace_back(llvmType);
        }
        fieldTypes.emplace_back(generate_type(Type::builtinErr("err")));
        ret->setBody(fieldTypes);
    }
    return ret;
}

// std::string Codegen::generate_symbol_name(std::string modIdentifier) {
//     debug_func(modIdentifier);
//     std::string_view to_find = "::";
//     std::string_view to_replace = "__";

//     size_t pos = modIdentifier.find(to_find);
//     while (pos != std::string::npos) {
//         modIdentifier.replace(pos, to_find.length(), to_replace);
//         pos = modIdentifier.find(to_find, pos + to_replace.length());
//     }
//     return modIdentifier;
// }

void Codegen::generate_builtin_get_errno() {
    debug_func("");
    llvm::Type *i32PtrTy = llvm::PointerType::get(m_builder.getInt32Ty(), 0);

    llvm::FunctionType *errnoLocationFTy = llvm::FunctionType::get(i32PtrTy, false);
    llvm::Function *errnoLocationFunc =
        llvm::Function::Create(errnoLocationFTy, llvm::Function::ExternalLinkage, "__errno_location", *m_module);

    llvm::FunctionType *getErrnoValueFTy = llvm::FunctionType::get(m_builder.getInt32Ty(), false);
    llvm::Function *getErrnoFunc =
        llvm::Function::Create(getErrnoValueFTy, llvm::Function::ExternalLinkage, "get_errno", *m_module);

    llvm::BasicBlock *entryBB = llvm::BasicBlock::Create(*m_context, "entry", getErrnoFunc);
    m_builder.SetInsertPoint(entryBB);

    llvm::Value *errnoPtr = m_builder.CreateCall(errnoLocationFunc);

    llvm::Value *errnoVal = m_builder.CreateLoad(m_builder.getInt32Ty(), errnoPtr, "errno_val");

    m_builder.CreateRet(errnoVal);
}
}  // namespace DMZ
