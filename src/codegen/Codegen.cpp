#ifdef DEBUG_CODEGEN
#ifndef DEBUG
#define DEBUG
#endif
#endif

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassInstrumentation.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/StandardInstrumentations.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Scalar/Reassociate.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#pragma GCC diagnostic pop

#include "Debug.hpp"
#include "Stats.hpp"
#include "codegen/Codegen.hpp"
#include "semantic/SemanticSymbolsTypes.hpp"

namespace DMZ {
Codegen::Codegen(std::vector<ptr<ResolvedModuleDecl>> resolvedTree, std::string_view sourcePath, bool debugSymbols,
                 bool noRemoveUnused, bool isModule)
    : m_resolvedTree(move_vector_ptr<ResolvedModuleDecl, ResolvedDecl>(resolvedTree)),
      m_noRemoveUnused(noRemoveUnused),
      m_isModule(isModule),
      m_context(makePtr<llvm::LLVMContext>()),
      m_builder(*m_context),
      m_module(makePtr<llvm::Module>("<translation_unit>", *m_context)),
      m_debugBuilder(*m_module),
      m_debugSymbols(debugSymbols) {
    m_module->setSourceFileName(sourcePath);
    m_module->setTargetTriple(llvm::sys::getDefaultTargetTriple());
}

std::pair<ptr<llvm::LLVMContext>, ptr<llvm::Module>> Codegen::generate_ir(bool runTest,
                                                                          const std::string &optimizationLevel) {
    debug_func("");
    ScopedTimer(StatType::Codegen);

    if (m_debugSymbols) {
        m_debugBuilder.createCompileUnit(llvm::dwarf::DW_LANG_C, generate_debug_file(m_module->getSourceFileName()),
                                         "dmz Compiler", false, "", 0);

        m_module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
        m_module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", llvm::dwarf::DWARF_VERSION);
    }

    generate_error_no_err();

    std::string mainToCall = runTest ? "__builtin_main_test" : "main";

    std::function<void(const std::vector<ptr<ResolvedDecl>> &)> findAndGenMain =
        [&](const std::vector<ptr<ResolvedDecl>> &decls) {
            for (auto &&decl : decls) {
                if (auto fd = dynamic_cast<const ResolvedFuncDecl *>(decl.get())) {
                    if (fd->identifier == mainToCall) {
                        generate_decl(*fd);
                    }
                } else if (auto md = dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
                    findAndGenMain(md->declarations);
                }
            }
        };

    findAndGenMain(m_resolvedTree);

    if (m_isModule) {
        // Generate all declarations in the source file
        for (auto &&decl : m_resolvedTree) {
            if (auto md = dynamic_cast<const ResolvedModuleDecl *>(decl.get())) {
                if (md->module_path == m_module->getSourceFileName()) {
                    for (auto &&d : md->declarations) {
                        generate_decl(*d);
                    }
                }
            }
        }
    }

    generate_pending_decls();

    generate_main_wrapper(runTest);
    if (m_debugSymbols) {
        m_debugBuilder.finalize();
    }

    if (optimizationLevel != "-O0") {
        llvm::PassInstrumentationCallbacks PIC;
        llvm::StandardInstrumentations SI(*m_context, false);
        SI.registerCallbacks(PIC);

        llvm::LoopAnalysisManager LAM;
        llvm::FunctionAnalysisManager FAM;
        llvm::CGSCCAnalysisManager CGAM;
        llvm::ModuleAnalysisManager MAM;

        llvm::PassBuilder PB(nullptr, llvm::PipelineTuningOptions(), std::nullopt, &PIC);

        PB.registerModuleAnalyses(MAM);
        PB.registerCGSCCAnalyses(CGAM);
        PB.registerFunctionAnalyses(FAM);
        PB.registerLoopAnalyses(LAM);
        PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

        llvm::FunctionPassManager FPM;

        FPM.addPass(llvm::InstCombinePass());
        FPM.addPass(llvm::ReassociatePass());
        FPM.addPass(llvm::GVNPass());
        FPM.addPass(llvm::SimplifyCFGPass());

        for (auto &&function : m_module->functions()) {
            debug_msg("Running FunctionPassManager on " << function.getName().str() << " ("
                                                        << function.getInstructionCount() << " instructions)");
            if (function.getInstructionCount() == 0) continue;
            FPM.run(function, FAM);
        }
    }

    return {std::move(m_context), std::move(m_module)};
}

void Codegen::generate_decl(const ResolvedDecl &decl) {
    if (m_resolvedDecls.contains(&decl)) return;
    m_resolvedDecls.insert(&decl);

    if (auto sd = dynamic_cast<const ResolvedStructDecl *>(&decl)) {
        if (dynamic_cast<const ResolvedGenericStructDecl *>(&decl)) return;
        generate_struct_decl(*sd);
        m_pendingDecls.insert(&decl);
    } else if (auto ud = dynamic_cast<const ResolvedUnionDecl *>(&decl)) {
        generate_union_decl(*ud);
        m_pendingDecls.insert(&decl);
    } else if (auto fd = dynamic_cast<const ResolvedFuncDecl *>(&decl)) {
        if (dynamic_cast<const ResolvedGenericFunctionDecl *>(&decl)) return;
        if (dynamic_cast<const ResolvedBuiltinFunctionDecl *>(&decl)) return;
        generate_function_decl(*fd);
        if (!dynamic_cast<const ResolvedExternFunctionDecl *>(&decl)) {
            m_pendingDecls.insert(&decl);
        }
    } else if (auto ds = dynamic_cast<const ResolvedDeclStmt *>(&decl)) {
        generate_global_var_decl(*ds);
    }
}

void Codegen::generate_body(const ResolvedDecl &decl) {
    debug_func(decl.name());
    ptr<DebugScopeRAII> debugScope = nullptr;
    if (m_debugSymbols) {
        m_currentDebugFile = generate_debug_file(decl.location.file_name);
        debugScope = makePtr<DebugScopeRAII>(*this, m_currentDebugFile);
    }

    if (auto sd = dynamic_cast<const ResolvedStructDecl *>(&decl)) {
        generate_struct_fields(*sd);
    } else if (auto ud = dynamic_cast<const ResolvedUnionDecl *>(&decl)) {
        generate_union_fields(*ud);
    } else if (auto fd = dynamic_cast<const ResolvedFuncDecl *>(&decl)) {
        generate_function_body(*fd);
    }
}

llvm::Type *Codegen::generate_type(const ResolvedType &type, bool noOpaque) {
    llvm::Type *ret = nullptr;
    debug_func("In type: '" << type.to_str() << "' out type '" << Dumper([&ret]() {
                   if (ret)
                       ret->print(llvm::errs());
                   else
                       std::cerr << "null";
               }) << "'");
    if (type.kind == ResolvedTypeKind::Pointer || type.kind == ResolvedTypeKind::Error) {
        debug_msg("isPointer or error");
        ret = llvm::PointerType::get(*m_context, 0);
    } else if (type.kind == ResolvedTypeKind::Void) {
        debug_msg("kind Void");
        ret = m_builder.getVoidTy();
    } else if (auto typeNum = dynamic_cast<const ResolvedTypeNumber *>(&type)) {
        if (typeNum->numberKind == ResolvedNumberKind::Int || typeNum->numberKind == ResolvedNumberKind::UInt) {
            debug_msg("kind Int or UInt");
            ret = m_builder.getIntNTy(typeNum->bitSize);
        } else if (typeNum->numberKind == ResolvedNumberKind::Float) {
            debug_msg("kind Int or UInt");
            switch (typeNum->bitSize) {
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
                    dmz_unreachable(type.location, "float type have an incorrect size");
                    break;
            }
        } else {
            dmz_unreachable(type.location, "internal error");
        }
    } else if (type.kind == ResolvedTypeKind::Struct || type.kind == ResolvedTypeKind::StructDecl) {
        ResolvedStructDecl *decl = nullptr;
        if (auto typeStruct = dynamic_cast<const ResolvedTypeStructDecl *>(&type)) {
            decl = typeStruct->decl;
        } else if (auto typeStruct = dynamic_cast<const ResolvedTypeStruct *>(&type)) {
            decl = typeStruct->decl;
        }
        if (!decl) dmz_unreachable(type.location, "unexpected error");
        std::string name = generate_decl_name(*decl);
        debug_msg("struct '" << name << "'");
        auto structType = get_struct_decl(*decl);
        ret = structType;
        if (!ret) {
            dmz_unreachable(type.location, "cannot get type '" + name + "'");
        }
        if (noOpaque && structType->isOpaque()) {
            generate_struct_fields(*decl);
            ret = llvm::StructType::getTypeByName(*m_context, name);
            if (!ret) dmz_unreachable(type.location, "unexpected error generating struct decl");
        }
    } else if (type.kind == ResolvedTypeKind::Union || type.kind == ResolvedTypeKind::UnionDecl) {
        const ResolvedUnionDecl *decl = nullptr;
        if (auto typeUnion = dynamic_cast<const ResolvedTypeUnionDecl *>(&type)) {
            decl = typeUnion->unionDecl();
        } else if (auto typeUnion = dynamic_cast<const ResolvedTypeUnion *>(&type)) {
            decl = typeUnion->unionDecl();
        }
        if (!decl) dmz_unreachable(type.location, "unexpected error");
        std::string name = generate_decl_name(*decl);
        debug_msg("union '" << name << "'");
        auto unionType = get_union_decl(*decl);
        ret = unionType;
        if (!ret) {
            dmz_unreachable(type.location, "cannot get type '" + name + "'");
        }
        if (noOpaque && unionType->isOpaque()) {
            generate_union_fields(*decl);
            ret = llvm::StructType::getTypeByName(*m_context, name);
            if (!ret) dmz_unreachable(type.location, "unexpected error generating union decl");
        }
    } else if (type.kind == ResolvedTypeKind::Enum || type.kind == ResolvedTypeKind::EnumDecl) {
        ret = generate_type(ResolvedTypeNumber{SourceLocation::builtin(), ResolvedNumberKind::UInt, 32});
    } else if (auto typeArray = dynamic_cast<const ResolvedTypeArray *>(&type)) {
        ret = generate_type(*typeArray->arrayType, true);
        ret = llvm::ArrayType::get(ret, typeArray->arraySize);
    } else if (auto typeOptional = dynamic_cast<const ResolvedTypeOptional *>(&type)) {
        std::string structName("error.struct." + typeOptional->optionalType->to_str());
        ret = llvm::StructType::getTypeByName(*m_context, structName);
        if (!ret) {
            ret = llvm::StructType::create(*m_context, structName);

            std::vector<llvm::Type *> fieldTypes;
            // Type of value
            if (typeOptional->optionalType->kind == ResolvedTypeKind::Void) {
                fieldTypes.emplace_back(m_builder.getInt1Ty());
            } else {
                fieldTypes.emplace_back(generate_type(*typeOptional->optionalType));
            }
            // Type of error
            fieldTypes.emplace_back(llvm::PointerType::get(*m_context, 0));
            static_cast<llvm::StructType *>(ret)->setBody(fieldTypes);
        }
    } else if (auto typeVec = dynamic_cast<const ResolvedTypeSimd *>(&type)) {
        auto baseType = generate_type(*typeVec->simdType, true);
        ret = llvm::FixedVectorType::get(baseType, typeVec->simdSize);
    } else if (auto fnType = dynamic_cast<const ResolvedTypeFunction *>(&type)) {
        debug_msg(fnType->to_str());
        ret = generate_function_type(*fnType);
    } else if (dynamic_cast<const ResolvedTypeSlice *>(&type)) {
        std::string structName("slice.struct");
        ret = llvm::StructType::getTypeByName(*m_context, structName);
        if (!ret) {
            ret = llvm::StructType::create(*m_context, structName);
            std::vector<llvm::Type *> fieldTypes;
            fieldTypes.emplace_back(llvm::PointerType::get(*m_context, 0));
            fieldTypes.emplace_back(m_builder.getIntPtrTy(m_module->getDataLayout()));
            static_cast<llvm::StructType *>(ret)->setBody(fieldTypes);
        }
    }
    if (ret == nullptr) {
        type.dump();
        dmz_unreachable(type.location, "cannot generate type '" + type.to_str() + "'");
    }
    return ret;
}

llvm::DIType *Codegen::generate_debug_type(const ResolvedType &type) {
    debug_func(type.to_str());
    if (auto typeNum = dynamic_cast<const ResolvedTypeNumber *>(&type)) {
        unsigned int Encoding;
        switch (typeNum->numberKind) {
            case ResolvedNumberKind::Int:
                Encoding = llvm::dwarf::DW_ATE_signed;
                break;
            case ResolvedNumberKind::UInt:
                Encoding = llvm::dwarf::DW_ATE_unsigned;
                break;
            case ResolvedNumberKind::Float:
                Encoding = llvm::dwarf::DW_ATE_float;
                break;
        }
        return m_debugBuilder.createBasicType(typeNum->to_str(), typeNum->bitSize, Encoding);
    } else if (type.kind == ResolvedTypeKind::Void || type.kind == ResolvedTypeKind::VarArg) {
        return nullptr;
    } else if (auto typePtr = dynamic_cast<const ResolvedTypePointer *>(&type)) {
        return m_debugBuilder.createPointerType(generate_debug_type(*typePtr->pointerType),
                                                m_module->getDataLayout().getPointerSizeInBits());
    } else if (auto typeVec = dynamic_cast<const ResolvedTypeSimd *>(&type)) {
        std::vector<llvm::Metadata *> Subscripts;
        Subscripts.push_back(m_debugBuilder.getOrCreateSubrange(0, typeVec->simdSize));
        auto elementDebugType = generate_debug_type(*typeVec->simdType);
        auto elementType = generate_type(*typeVec->simdType);
        auto sizeInBits = m_module->getDataLayout().getTypeSizeInBits(elementType) * typeVec->simdSize;
        auto alignInBits = m_module->getDataLayout().getPrefTypeAlign(elementType).value() * 8;
        return m_debugBuilder.createVectorType(sizeInBits, alignInBits, elementDebugType,
                                               m_debugBuilder.getOrCreateArray(Subscripts));
    } else if (auto typeError = dynamic_cast<const ResolvedTypeError *>(&type)) {
        return generate_debug_type(*ResolvedTypePointer::opaquePtr(typeError->location));
    } else if (type.kind == ResolvedTypeKind::Struct || type.kind == ResolvedTypeKind::StructDecl) {
        ResolvedStructDecl *decl = nullptr;
        if (auto typeStruct = dynamic_cast<const ResolvedTypeStruct *>(&type)) {
            decl = typeStruct->decl;
        } else if (auto typeStruct = dynamic_cast<const ResolvedTypeStructDecl *>(&type)) {
            decl = typeStruct->decl;
        }
        if (!decl) dmz_unreachable(type.location, "unexpected error");

        if (auto it = m_debugTypes.find(decl->name()); it != m_debugTypes.end()) {
            return it->second;
        }

        auto structFile = generate_debug_file(decl->location.file_name);
        auto llvmStructType = generate_type(type, true);
        auto bitSize = m_module->getDataLayout().getTypeSizeInBits(llvmStructType);
        auto alingSize = m_module->getDataLayout().getPrefTypeAlign(llvmStructType).value() * 8;

        auto forwardDecl = m_debugBuilder.createReplaceableCompositeType(
            llvm::dwarf::DW_TAG_structure_type, decl->name(), structFile, structFile, decl->location.line, 0, bitSize,
            alingSize, llvm::DINode::DIFlags::FlagFwdDecl);

        m_debugTypes[decl->name()] = forwardDecl;

        std::vector<llvm::Metadata *> Elements;
        uint64_t offset = 0;
        for (auto &&field : decl->fields) {
            auto llvmMemberType = generate_type(*field->type, true);
            auto fieldBitSize = m_module->getDataLayout().getTypeSizeInBits(llvmMemberType);
            auto fieldAlingSize = m_module->getDataLayout().getPrefTypeAlign(llvmMemberType).value() * 8;
            auto memberFile = generate_debug_file(field->location.file_name);
            auto memberType = m_debugBuilder.createMemberType(
                memberFile, field->name(), memberFile, field->location.line, fieldBitSize, fieldAlingSize, offset,
                llvm::DINode::DIFlags::FlagPublic, generate_debug_type(*field->type));
            Elements.emplace_back(memberType);
            offset += fieldBitSize;
        }

        auto finalType = m_debugBuilder.createStructType(structFile, decl->name(), structFile, decl->location.line,
                                                         bitSize, alingSize, llvm::DINode::DIFlags::FlagPrototyped,
                                                         nullptr, m_debugBuilder.getOrCreateArray(Elements));

        m_debugBuilder.replaceTemporary(llvm::TempDICompositeType(forwardDecl), finalType);
        m_debugTypes[decl->name()] = finalType;
        return finalType;
    } else if (auto typeFn = dynamic_cast<const ResolvedTypeFunction *>(&type)) {
        std::vector<llvm::Metadata *> Elements;
        Elements.emplace_back(generate_debug_type(*typeFn->returnType));

        for (auto &&param : typeFn->paramsTypes) {
            Elements.emplace_back(generate_debug_type(*param));
        }

        return m_debugBuilder.createSubroutineType(m_debugBuilder.getOrCreateTypeArray(Elements));
    } else if (auto typeSlice = dynamic_cast<const ResolvedTypeSlice *>(&type)) {
        std::vector<llvm::Metadata *> Elements;
        uint64_t offset = 0;
        auto structFile = generate_debug_file(typeSlice->location.file_name);
        auto type_ptr = ResolvedTypePointer::opaquePtr(typeSlice->location);
        auto type_len = ResolvedTypeNumber::usize(typeSlice->location);
        // ptr
        auto llvmMemberType_ptr = generate_type(*type_ptr, true);
        auto bitSize_ptr = m_module->getDataLayout().getTypeSizeInBits(llvmMemberType_ptr);
        auto alingSize_ptr = m_module->getDataLayout().getPrefTypeAlign(llvmMemberType_ptr).value() * 8;
        auto memberType_ptr = m_debugBuilder.createMemberType(
            structFile, "ptr", structFile, typeSlice->location.line, bitSize_ptr, alingSize_ptr, offset,
            llvm::DINode::DIFlags::FlagPublic, generate_debug_type(*type_ptr));
        Elements.emplace_back(memberType_ptr);
        offset += bitSize_ptr;

        // len
        auto llvmMemberType_len = m_builder.getIntPtrTy(m_module->getDataLayout());
        auto bitSize_len = m_module->getDataLayout().getTypeSizeInBits(llvmMemberType_len);
        auto alingSize_len = m_module->getDataLayout().getPrefTypeAlign(llvmMemberType_len).value() * 8;
        auto memberType_len = m_debugBuilder.createMemberType(
            structFile, "len", structFile, typeSlice->location.line, bitSize_len, alingSize_len, offset,
            llvm::DINode::DIFlags::FlagPublic, generate_debug_type(*type_len));
        Elements.emplace_back(memberType_len);
        offset += bitSize_len;

        auto llvmStructType = generate_type(type, true);
        auto bitSize = m_module->getDataLayout().getTypeSizeInBits(llvmStructType);
        auto alingSize = m_module->getDataLayout().getPrefTypeAlign(llvmStructType).value() * 8;
        return m_debugBuilder.createStructType(structFile, "slice", structFile, type.location.line, bitSize, alingSize,
                                               llvm::DINode::DIFlags::FlagPrototyped, nullptr,
                                               m_debugBuilder.getOrCreateArray(Elements));
    } else if (auto typeOptional = dynamic_cast<const ResolvedTypeOptional *>(&type)) {
        std::vector<llvm::Metadata *> Elements;
        uint64_t offset = 0;
        std::string structName("error.struct." + typeOptional->optionalType->to_str());
        auto structFile = generate_debug_file(typeOptional->location.file_name);
        ptr<ResolvedType> type_value = nullptr;
        if (typeOptional->optionalType->kind == ResolvedTypeKind::Void) {
            type_value = makePtr<ResolvedTypeNumber>(typeOptional->location, ResolvedNumberKind::Int, 1);
        } else {
            type_value = typeOptional->optionalType->clone();
        }
        auto type_error = ResolvedTypePointer::opaquePtr(typeOptional->location);
        // ptr
        auto llvmMemberType_value = generate_type(*type_value, true);
        auto bitSize_value = m_module->getDataLayout().getTypeSizeInBits(llvmMemberType_value);
        auto alingSize_value = m_module->getDataLayout().getPrefTypeAlign(llvmMemberType_value).value() * 8;
        auto memberType_value = m_debugBuilder.createMemberType(
            structFile, "value", structFile, typeOptional->location.line, bitSize_value, alingSize_value, offset,
            llvm::DINode::DIFlags::FlagPublic, generate_debug_type(*type_value));
        Elements.emplace_back(memberType_value);
        offset += bitSize_value;

        // len
        auto llvmMemberType_error = m_builder.getIntPtrTy(m_module->getDataLayout());
        auto bitSize_error = m_module->getDataLayout().getTypeSizeInBits(llvmMemberType_error);
        auto alingSize_error = m_module->getDataLayout().getPrefTypeAlign(llvmMemberType_error).value() * 8;
        auto memberType_error = m_debugBuilder.createMemberType(
            structFile, "error", structFile, typeOptional->location.line, bitSize_error, alingSize_error, offset,
            llvm::DINode::DIFlags::FlagPublic, generate_debug_type(*type_error));
        Elements.emplace_back(memberType_error);
        offset += bitSize_error;

        auto llvmStructType = generate_type(type, true);
        auto bitSize = m_module->getDataLayout().getTypeSizeInBits(llvmStructType);
        auto alingSize = m_module->getDataLayout().getPrefTypeAlign(llvmStructType).value() * 8;
        return m_debugBuilder.createStructType(structFile, structName, structFile, type.location.line, bitSize,
                                               alingSize, llvm::DINode::DIFlags::FlagPrototyped, nullptr,
                                               m_debugBuilder.getOrCreateArray(Elements));
    } else if (auto typeArray = dynamic_cast<const ResolvedTypeArray *>(&type)) {
        auto llvmArrayType = generate_type(type, true);
        auto llvmElemType = generate_debug_type(*typeArray->arrayType);
        auto alingSize = m_module->getDataLayout().getPrefTypeAlign(llvmArrayType).value() * 8;
        return m_debugBuilder.createArrayType(typeArray->arraySize, alingSize, llvmElemType, nullptr);
    } else if (type.kind == ResolvedTypeKind::Union || type.kind == ResolvedTypeKind::UnionDecl) {
        const ResolvedUnionDecl *decl = nullptr;
        if (auto typeUnion = dynamic_cast<const ResolvedTypeUnion *>(&type)) {
            decl = typeUnion->unionDecl();
        } else if (auto typeUnion = dynamic_cast<const ResolvedTypeUnionDecl *>(&type)) {
            decl = typeUnion->unionDecl();
        }
        if (!decl) dmz_unreachable(type.location, "unexpected error");

        if (auto it = m_debugTypes.find(decl->name()); it != m_debugTypes.end()) {
            return it->second;
        }

        auto unionFile = generate_debug_file(decl->location.file_name);
        auto llvmUnionType = llvm::cast<llvm::StructType>(generate_type(type, true));
        const auto &dl = m_module->getDataLayout();
        const auto *structLayout = dl.getStructLayout(llvmUnionType);
        auto bitSize = dl.getTypeSizeInBits(llvmUnionType);
        auto alingSize = dl.getPrefTypeAlign(llvmUnionType).value() * 8;

        auto forwardDecl = m_debugBuilder.createReplaceableCompositeType(
            llvm::dwarf::DW_TAG_structure_type, decl->name(), unionFile, unionFile, decl->location.line, 0, bitSize,
            alingSize, llvm::DINode::DIFlags::FlagFwdDecl);

        m_debugTypes[decl->name()] = forwardDecl;

        std::vector<llvm::Metadata *> Elements;

        // Tag field
        auto tagDebugType = generate_debug_type(*decl->tag->type);
        auto tagType = generate_type(*decl->tag->type);
        auto tagBitSize = dl.getTypeSizeInBits(tagType);
        auto tagAlingSize = dl.getPrefTypeAlign(tagType).value() * 8;
        auto tagOffset = structLayout->getElementOffsetInBits(0);
        auto tagField =
            m_debugBuilder.createMemberType(unionFile, "tag", unionFile, decl->location.line, tagBitSize, tagAlingSize,
                                            tagOffset, llvm::DINode::DIFlags::FlagPublic, tagDebugType);
        Elements.emplace_back(tagField);

        // For the payload, we can create a union of all possible variants
        std::vector<llvm::Metadata *> VariantElements;
        for (auto &&field : decl->fields) {
            int variantBitSize = 0;
            int variantAlingSize = 0;
            if (field->type->kind != ResolvedTypeKind::Void) {
                auto llvmVariantType = generate_type(*field->type, true);
                variantBitSize = dl.getTypeSizeInBits(llvmVariantType);
                variantAlingSize = dl.getPrefTypeAlign(llvmVariantType).value() * 8;
            }
            auto variantMember = m_debugBuilder.createMemberType(
                unionFile, field->name(), unionFile, field->location.line, variantBitSize, variantAlingSize, 0,
                llvm::DINode::DIFlags::FlagPublic, generate_debug_type(*field->type));
            VariantElements.emplace_back(variantMember);
        }

        auto payloadOffset = structLayout->getElementOffsetInBits(1);
        auto payloadBitSize = bitSize - payloadOffset;
        auto payloadUnion = m_debugBuilder.createUnionType(
            unionFile, decl->name() + "_payload", unionFile, decl->location.line, payloadBitSize, alingSize,
            llvm::DINode::DIFlags::FlagPublic, m_debugBuilder.getOrCreateArray(VariantElements));

        auto payloadField =
            m_debugBuilder.createMemberType(unionFile, "payload", unionFile, decl->location.line, payloadBitSize,
                                            alingSize, payloadOffset, llvm::DINode::DIFlags::FlagPublic, payloadUnion);
        Elements.emplace_back(payloadField);

        auto finalType = m_debugBuilder.createStructType(unionFile, decl->name(), unionFile, decl->location.line,
                                                         bitSize, alingSize, llvm::DINode::DIFlags::FlagPrototyped,
                                                         nullptr, m_debugBuilder.getOrCreateArray(Elements));

        m_debugBuilder.replaceTemporary(llvm::TempDICompositeType(forwardDecl), finalType);
        m_debugTypes[decl->name()] = finalType;
        return finalType;
    }
    type.dump();
    dmz_unreachable(type.location, "TODO");
}

llvm::DIFile *Codegen::generate_debug_file(const std::string &file_name) {
    debug_func("");
    auto path = std::filesystem::path(file_name);
    if (path.empty()) dmz_unreachable(SourceLocation{.file_name = file_name}, "Path cannot be empty");
    if (std::filesystem::exists(path)) {
        path = std::filesystem::canonical(path);
    }
    return m_debugBuilder.createFile(path.filename().string(), path.parent_path().string());
}

void Codegen::generate_error_trace_push(const SourceLocation &location) {
    ResolvedTypeSlice sliceType(location, makePtr<ResolvedTypeNumber>(location, ResolvedNumberKind::UInt, 8));
    auto sliceLLVMType = generate_type(sliceType, true);
    if (!m_errorTraceGlobal) {
        m_errorTraceEntryType = llvm::StructType::create(*m_context, "ErrorTraceEntry");
        m_errorTraceEntryType->setBody({sliceLLVMType, m_builder.getInt32Ty(), m_builder.getInt32Ty(), sliceLLVMType});

        auto bufferType = llvm::ArrayType::get(m_errorTraceEntryType, 20);
        m_errorTraceType = llvm::StructType::create(*m_context, "ErrorTrace");
        m_errorTraceType->setBody({m_builder.getInt32Ty(), bufferType});

        m_errorTraceGlobal =
            new llvm::GlobalVariable(*m_module, m_errorTraceType, false, llvm::GlobalValue::InternalLinkage,
                                     llvm::ConstantAggregateZero::get(m_errorTraceType), "dmz_error_trace");
        static_cast<llvm::GlobalVariable *>(m_errorTraceGlobal)->setThreadLocal(true);
    }
    auto sizePtr = m_builder.CreateStructGEP(m_errorTraceType, m_errorTraceGlobal, 0);
    auto size = m_builder.CreateLoad(m_builder.getInt32Ty(), sizePtr);
    auto canPush = m_builder.CreateICmpSLT(size, m_builder.getInt32(20));

    auto function = get_current_function();
    auto pushBB = llvm::BasicBlock::Create(*m_context, "error_trace.push", function);
    auto mergeBB = llvm::BasicBlock::Create(*m_context, "error_trace.merge", function);

    m_builder.CreateCondBr(canPush, pushBB, mergeBB);
    m_builder.SetInsertPoint(pushBB);

    auto bufferPtr = m_builder.CreateStructGEP(m_errorTraceType, m_errorTraceGlobal, 1);
    auto entryPtr =
        m_builder.CreateGEP(llvm::ArrayType::get(m_errorTraceEntryType, 20), bufferPtr, {m_builder.getInt32(0), size});

    llvm::Value *entry_value = llvm::UndefValue::get(m_errorTraceEntryType);

    auto storeSlice = [&](const std::string name, const std::string &str, int idx) {
        auto strVal = create_global_string(str, name);

        llvm::Value *slice_value = llvm::UndefValue::get(sliceLLVMType);
        slice_value = m_builder.CreateInsertValue(slice_value, strVal, 0);
        slice_value = m_builder.CreateInsertValue(slice_value, m_builder.getInt64(str.length()), 1);

        entry_value = m_builder.CreateInsertValue(entry_value, slice_value, idx);
    };
    std::string file_name = location.file_name;
    if (std::filesystem::exists(file_name)) {
        file_name = std::filesystem::canonical(file_name);
    }
    storeSlice("global.str.trace.file", file_name, 0);
    entry_value = m_builder.CreateInsertValue(entry_value, m_builder.getInt32(location.line), 1);
    entry_value = m_builder.CreateInsertValue(entry_value, m_builder.getInt32(location.col + 1), 2);
    storeSlice("global.str.trace.function", m_currentFunction ? m_currentFunction->name() : "unknown", 3);

    m_builder.CreateStore(entry_value, entryPtr);

    auto newSize = m_builder.CreateAdd(size, m_builder.getInt32(1));
    m_builder.CreateStore(newSize, sizePtr);
    m_builder.CreateBr(mergeBB);

    m_builder.SetInsertPoint(mergeBB);
}

llvm::Value *Codegen::generate_error_trace_get_idx() {
    if (!m_errorTraceGlobal) return m_builder.getInt32(0);
    if (!m_builder.GetInsertBlock()) return m_builder.getInt32(0);
    auto sizePtr = m_builder.CreateStructGEP(m_errorTraceType, m_errorTraceGlobal, 0, "errorTraceGlobal.idx.ptr");
    auto size = m_builder.CreateLoad(m_builder.getInt32Ty(), sizePtr, "errorTraceGlobal.idx");
    return size;
}

void Codegen::generate_error_trace_clear(llvm::Value *idx) {
    if (!m_errorTraceGlobal) return;
    if (!m_builder.GetInsertBlock()) return;
    auto sizePtr = m_builder.CreateStructGEP(m_errorTraceType, m_errorTraceGlobal, 0, "errorTraceGlobal.idx.ptr");
    m_builder.CreateStore(idx ? idx : m_builder.getInt32(0), sizePtr);
}

llvm::Value *Codegen::generate_get_error_trace() {
    if (!m_errorTraceGlobal) {
        generate_error_trace_push({});
        generate_error_trace_clear();
    }
    return m_errorTraceGlobal;
}

void Codegen::set_debug_location(const SourceLocation &location) {
    if (m_debugSymbols) {
        debug_func(Dumper([this]() { m_currentDebugScope->print(llvm::errs()); }));
        m_DebugScopeStack.emplace(m_builder.getCurrentDebugLocation());
        m_builder.SetCurrentDebugLocation(
            llvm::DILocation::get(m_currentDebugScope->getContext(), location.line, location.col, m_currentDebugScope));
    }
}

void Codegen::unset_debug_location() {
    if (m_debugSymbols) {
        debug_func("");
        m_builder.SetCurrentDebugLocation(m_DebugScopeStack.top());
        m_DebugScopeStack.pop();
    }
}

llvm::AllocaInst *Codegen::allocate_stack_variable(const SourceLocation &location, const std::string_view identifier,
                                                   const ResolvedType &type) {
    debug_func("");
    assert(m_allocaInsertPoint != nullptr);
    assert(m_memsetInsertPoint != nullptr);
    llvm::IRBuilder<> tmpBuilder(*m_context);
    debug_msg("m_allocaInsertPoint " << (void *)m_allocaInsertPoint);
    tmpBuilder.SetInsertPoint(m_allocaInsertPoint);
    auto value = tmpBuilder.CreateAlloca(generate_type(type, true), nullptr, identifier);
    llvm::IRBuilder<> tmpBuilderMemset(*m_context);
    debug_msg("m_memsetInsertPoint " << (void *)m_memsetInsertPoint);
    tmpBuilderMemset.SetInsertPoint(m_memsetInsertPoint);
    const llvm::DataLayout &dl = m_module->getDataLayout();
    tmpBuilderMemset.CreateMemSetInline(value, dl.getPrefTypeAlign(value->getType()), tmpBuilderMemset.getInt8(0),
                                        tmpBuilderMemset.getInt64(*value->getAllocationSize(dl)));
    if (m_debugSymbols) {
        llvm::DILocalVariable *localVar = m_debugBuilder.createAutoVariable(
            m_currentDebugScope, identifier, m_currentDebugFile, location.line, generate_debug_type(type));
        m_debugBuilder.insertDeclare(
            value, localVar, m_debugBuilder.createExpression(),
            llvm::DILocation::get(*m_context, location.line, location.col, m_currentDebugScope),
            m_builder.GetInsertBlock());
    }
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

llvm::Value *Codegen::to_bool(llvm::Value *v, const ResolvedType &type) {
    debug_func("");
    // println("type: " << type.to_str());
    // v->dump();
    if (type.kind == ResolvedTypeKind::Pointer || type.kind == ResolvedTypeKind::Error) {
        v = m_builder.CreatePtrToInt(v, m_builder.getInt64Ty());
        return m_builder.CreateICmpNE(v, m_builder.getInt64(0), "ptr.to.bool");
    } else if (auto typeNum = dynamic_cast<const ResolvedTypeNumber *>(&type)) {
        if (typeNum->numberKind == ResolvedNumberKind::Int) {
            if (typeNum->bitSize == 1) return v;
            return m_builder.CreateICmpNE(
                v, llvm::ConstantInt::get(generate_type(type), 0, typeNum->numberKind == ResolvedNumberKind::Int),
                "int.to.bool");
        } else if (typeNum->numberKind == ResolvedNumberKind::UInt) {
            if (typeNum->bitSize == 1) return v;
            return m_builder.CreateICmpNE(
                v, llvm::ConstantInt::get(generate_type(type), 0, typeNum->numberKind == ResolvedNumberKind::Int),
                "uint.to.bool");
        } else if (typeNum->numberKind == ResolvedNumberKind::Float) {
            return m_builder.CreateFCmpONE(v, llvm::ConstantFP::get(generate_type(type), 0.0), "float.to.bool");
        }
    }
    type.dump();
    dmz_unreachable(type.location, "unsuported type in to_bool");
}

llvm::Value *Codegen::cast_to(llvm::Value *v, const ResolvedType &from, const ResolvedType &to) {
    debug_func("From: '" << from.to_str() << "' to: '" << to.to_str() << "' of: '" << Dumper([&]() {
                   if (v)
                       v->print(llvm::errs());
                   else
                       std::cerr << "nullptr";
               }) << "'");
    // debug_msg("From: '" << from.to_str() << "' to: '" << to.to_str());
    // m_module->dump();
    // v->dump();
    if (from.equal(to)) return v;

    const ResolvedType *fromPtr = &from;
    const ResolvedType *toPtr = &to;

    std::optional<ResolvedTypeNumber> fromTmp;
    std::optional<ResolvedTypeNumber> toTmp;
    if (fromPtr->kind == ResolvedTypeKind::Enum) {
        fromTmp.emplace(fromPtr->location, ResolvedNumberKind::UInt, 32);
        fromPtr = &fromTmp.value();
    }

    if (toPtr->kind == ResolvedTypeKind::Enum) {
        toTmp.emplace(toPtr->location, ResolvedNumberKind::UInt, 32);
        toPtr = &toTmp.value();
    }

    if (fromPtr->kind == ResolvedTypeKind::Pointer) {
        if (toPtr->kind == ResolvedTypeKind::Pointer) {
            return v;
        } else if (toPtr->kind == ResolvedTypeKind::Number) {
            return m_builder.CreatePtrToInt(v, generate_type(*toPtr), "ptr.to.int");
        } else {
            dmz_unreachable(fromPtr->location,
                            "From: " + fromPtr->to_str() + " to: " + toPtr->to_str() + " unsuported type from ptr");
        }
    } else if (auto fromNum = dynamic_cast<const ResolvedTypeNumber *>(fromPtr)) {
        if (auto toNum = dynamic_cast<const ResolvedTypeNumber *>(toPtr)) {
            if (fromNum->numberKind == ResolvedNumberKind::Int) {
                if (toNum->numberKind == ResolvedNumberKind::Int) {
                    if (fromNum->bitSize == 1)
                        return m_builder.CreateZExtOrTrunc(v, generate_type(*toPtr), "bool.to.int");
                    return m_builder.CreateSExtOrTrunc(v, generate_type(*toPtr), "int.to.int");
                } else if (toNum->numberKind == ResolvedNumberKind::UInt) {
                    return m_builder.CreateSExtOrTrunc(v, generate_type(*toPtr), "int.to.uint");
                } else if (toNum->numberKind == ResolvedNumberKind::Float) {
                    return m_builder.CreateSIToFP(v, generate_type(*toPtr), "int.to.float");
                } else {
                    dmz_unreachable(fromPtr->location, "From: " + fromPtr->to_str() + " to: " + toPtr->to_str() +
                                                           " unsuported type from Int");
                }
            } else if (fromNum->numberKind == ResolvedNumberKind::UInt) {
                if (toNum->numberKind == ResolvedNumberKind::Int) {
                    return m_builder.CreateZExtOrTrunc(v, generate_type(*toPtr), "uint.to.int");
                } else if (toNum->numberKind == ResolvedNumberKind::UInt) {
                    return m_builder.CreateZExtOrTrunc(v, generate_type(*toPtr), "uint.to.uint");
                } else if (toNum->numberKind == ResolvedNumberKind::Float) {
                    return m_builder.CreateUIToFP(v, generate_type(*toPtr));
                } else {
                    dmz_unreachable(fromPtr->location, "From: " + fromPtr->to_str() + " to: " + toPtr->to_str() +
                                                           " unsuported type from UInt");
                }
            } else if (fromNum->numberKind == ResolvedNumberKind::Float) {
                if (toNum->numberKind == ResolvedNumberKind::Int) {
                    return m_builder.CreateFPToSI(v, generate_type(*toPtr), "uint.to.int");
                } else if (toNum->numberKind == ResolvedNumberKind::UInt) {
                    return m_builder.CreateFPToUI(v, generate_type(*toPtr), "uint.to.uint");
                } else if (toNum->numberKind == ResolvedNumberKind::Float) {
                    if (fromNum->bitSize > toNum->bitSize) {
                        return m_builder.CreateFPTrunc(v, generate_type(*toPtr));
                    } else if (fromNum->bitSize < toNum->bitSize) {
                        return m_builder.CreateFPExt(v, generate_type(*toPtr));
                    } else {
                        return v;
                    }
                } else {
                    dmz_unreachable(fromPtr->location, "From: " + fromPtr->to_str() + " to: " + toPtr->to_str() +
                                                           " unsuported type from UInt");
                }
            }
        }
    } else if (fromPtr->kind == ResolvedTypeKind::Error) {
        if (toPtr->kind == ResolvedTypeKind::Error || toPtr->kind == ResolvedTypeKind::Pointer) {
            return v;
        } else {
            dmz_unreachable(fromPtr->location,
                            "From: " + fromPtr->to_str() + " to: " + toPtr->to_str() + " unsuported type from Err");
        }
    }

    dmz_unreachable(fromPtr->location,
                    "From: " + fromPtr->to_str() + " to: " + toPtr->to_str() + " unsuported type in cast_to");
}

llvm::Function *Codegen::get_current_function() { return m_builder.GetInsertBlock()->getParent(); };

void Codegen::break_into_bb(llvm::BasicBlock *targetBB) {
    debug_func("");
    llvm::BasicBlock *currentBB = m_builder.GetInsertBlock();

    if (currentBB && !currentBB->getTerminator()) m_builder.CreateBr(targetBB);

    m_builder.ClearInsertionPoint();
}

bool Codegen::store_value_generate_memcpy(const ResolvedType &from) {
    if (from.kind != ResolvedTypeKind::Pointer) {
        if (from.generate_struct()) {
            return true;
        }
        if (from.kind == ResolvedTypeKind::Array) {
            return true;
        }
    }
    return false;
}

llvm::Value *Codegen::store_value(llvm::Value *val, llvm::Value *ptr, const ResolvedType &from,
                                  const ResolvedType &to) {
    debug_func("From: '" << from.to_str() << "' to: '" << to.to_str() << "' " << Dumper([&]() {
                   std::cerr << "val: '";
                   if (val)
                       val->print(llvm::errs());
                   else
                       std::cerr << "nullptr";
                   std::cerr << "' ptr: '";
                   if (ptr)
                       ptr->print(llvm::errs());
                   else
                       std::cerr << "nullptr";
                   std::cerr << "'";
               }));
    if (from.kind != ResolvedTypeKind::Pointer) {
        if (from.generate_struct()) {
            const llvm::DataLayout &dl = m_module->getDataLayout();
            const llvm::StructLayout *sl = dl.getStructLayout(static_cast<llvm::StructType *>(generate_type(from)));

            return m_builder.CreateMemCpy(ptr, sl->getAlignment(), val, sl->getAlignment(), sl->getSizeInBytes());
        }
        if (from.kind == ResolvedTypeKind::Array) {
            const llvm::DataLayout &dl = m_module->getDataLayout();
            auto t = generate_type(from);

            return m_builder.CreateMemCpy(ptr, dl.getPrefTypeAlign(t), val, dl.getPrefTypeAlign(t),
                                          dl.getTypeAllocSize(t));
        }
    }

    return m_builder.CreateStore(cast_to(val, from, to), ptr);
}

llvm::Value *Codegen::load_value(llvm::Value *v, const ResolvedType &type) {
    debug_func("");
    if (type.kind == ResolvedTypeKind::Void) return nullptr;
    bool kp = false;
    kp |= type.generate_struct();
    kp |= type.kind == ResolvedTypeKind::Array;
    return kp ? v : m_builder.CreateLoad(generate_type(type), v);
}

llvm::GlobalVariable *Codegen::create_global_string(const std::string &str, const std::string &name) {
    debug_func(str);
    if (m_globalStrings.contains(str)) {
        return m_globalStrings[str];
    }

    size_t hashVal = std::hash<std::string>{}(str);

    std::stringstream ss;
    ss << name << "." << std::uppercase << std::hex << (hashVal & 0xFFFFFFFF);
    std::string uniqueName = ss.str();

    auto strVal = m_builder.CreateGlobalString(str, uniqueName, 0, m_module.get());
    m_globalStrings[str] = strVal;
    return strVal;
}
}  // namespace DMZ
