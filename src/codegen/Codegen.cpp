#include "codegen/Codegen.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/TargetParser/Host.h>
#pragma GCC diagnostic pop

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
    ScopedTimer st(Stats::type::codegenTime);
    // TODO: rethink lock to not lock all threads
    auto lock = get_shared_context().getLock();

    generate_in_module_decl(m_resolvedTree, true);

    return std::make_unique<llvm::orc::ThreadSafeModule>(std::move(m_module), get_shared_context());
}

llvm::Type *Codegen::generate_type(const Type &type) {
    llvm::Type *ret = nullptr;
    if (type.kind == Type::Kind::Err) {
        ret = llvm::PointerType::get(*m_context, 0);
        // ret = m_builder.getIntPtrTy(m_module->getDataLayout());
        return ret;
    }
    if (type.kind == Type::Kind::Int) {
        ret = m_builder.getInt32Ty();
    }
    if (type.kind == Type::Kind::Char) {
        ret = m_builder.getInt8Ty();
    }
    if (type.kind == Type::Kind::Bool) {
        ret = m_builder.getInt1Ty();
    }
    if (type.kind == Type::Kind::Struct) {
        ret = llvm::StructType::getTypeByName(*m_context, "struct." + std::string(type.name));
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
    llvm::IRBuilder<> tmpBuilder(*m_context);
    tmpBuilder.SetInsertPoint(m_allocaInsertPoint);
    auto value = tmpBuilder.CreateAlloca(generate_type(type), nullptr, identifier);
    if (type.isOptional) {
        llvm::IRBuilder<> tmpBuilderMemset(*m_context);
        tmpBuilderMemset.SetInsertPoint(m_memsetInsertPoint);
        const llvm::DataLayout &dl = m_module->getDataLayout();
        tmpBuilderMemset.CreateMemSetInline(value, dl.getPrefTypeAlign(value->getType()), tmpBuilderMemset.getInt8(0),
                                            tmpBuilderMemset.getInt64(*value->getAllocationSize(dl)));
    }
    return value;
}

void Codegen::generate_main_wrapper() {
    auto *builtinMain = m_module->getFunction("__builtin_main");

    auto *main = llvm::Function::Create(llvm::FunctionType::get(m_builder.getInt32Ty(), {}, false),
                                        llvm::Function::ExternalLinkage, "main", *m_module);

    auto *entry = llvm::BasicBlock::Create(*m_context, "entry", main);
    m_builder.SetInsertPoint(entry);

    if (builtinMain) m_builder.CreateCall(builtinMain);

    m_builder.CreateRet(llvm::ConstantInt::getSigned(m_builder.getInt32Ty(), 0));
}

llvm::Value *Codegen::int_to_bool(llvm::Value *v) {
    return m_builder.CreateICmpNE(v, m_builder.getInt32(0), "int.to.bool");
}

llvm::Value *Codegen::ptr_to_bool(llvm::Value *v) {
    v = m_builder.CreatePtrToInt(v, m_builder.getInt32Ty());
    return m_builder.CreateICmpNE(v, m_builder.getInt32(0), "ptr.to.bool");
}

llvm::Value *Codegen::bool_to_int(llvm::Value *v) {
    return m_builder.CreateIntCast(v, m_builder.getInt32Ty(), false, "bool.to.int");
}

llvm::Function *Codegen::get_current_function() { return m_builder.GetInsertBlock()->getParent(); };

void Codegen::break_into_bb(llvm::BasicBlock *targetBB) {
    llvm::BasicBlock *currentBB = m_builder.GetInsertBlock();

    if (currentBB && !currentBB->getTerminator()) m_builder.CreateBr(targetBB);

    m_builder.ClearInsertionPoint();
}

llvm::Value *Codegen::store_value(llvm::Value *val, llvm::Value *ptr, const Type &type) {
    if (type.kind == Type::Kind::Struct || type.isOptional) {
        const llvm::DataLayout &dl = m_module->getDataLayout();
        const llvm::StructLayout *sl = dl.getStructLayout(static_cast<llvm::StructType *>(generate_type(type)));

        return m_builder.CreateMemCpy(ptr, sl->getAlignment(), val, sl->getAlignment(), sl->getSizeInBytes());
    }
    if (type.isArray) {
        const llvm::DataLayout &dl = m_module->getDataLayout();
        auto t = generate_type(type);

        return m_builder.CreateMemCpy(ptr, dl.getPrefTypeAlign(t), val, dl.getPrefTypeAlign(t), dl.getTypeAllocSize(t));
    }

    return m_builder.CreateStore(val, ptr);
}

llvm::Value *Codegen::load_value(llvm::Value *v, Type type) { return m_builder.CreateLoad(generate_type(type), v); }

llvm::Type *Codegen::generate_optional_type(const Type &type, llvm::Type *llvmType) {
    std::string structName("err.struct." + type.withoutOptional().to_str());
    auto ret = llvm::StructType::getTypeByName(*m_context, structName);
    if (!ret) {
        ret = llvm::StructType::create(*m_context, structName);

        std::vector<llvm::Type *> fieldTypes;
        fieldTypes.emplace_back(llvmType);
        fieldTypes.emplace_back(generate_type(Type::builtinErr("err")));
        ret->setBody(fieldTypes);
    }
    return ret;
}

std::string Codegen::generate_symbol_name(std::string modIdentifier) {
    std::string_view to_find = "::";
    std::string_view to_replace = "__";

    size_t pos = modIdentifier.find(to_find);
    while (pos != std::string::npos) {
        modIdentifier.replace(pos, to_find.length(), to_replace);
        pos = modIdentifier.find(to_find, pos + to_replace.length());
    }
    return modIdentifier;
}
}  // namespace DMZ
