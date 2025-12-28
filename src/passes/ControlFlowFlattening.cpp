#include <passes/ControlFlowFlattening.h>
#include <core/ZyroxMetaData.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Verifier.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/Local.h>
#include <quickjs/QuickConfig.h>
#include <set>
#include <utils/BasicBlockUtils.h>
#include <utils/FunctionUtils.h>
#include <utils/HashUtils.h>
#include <utils/Logger.h>
#include <utils/OpaqueTransformer.h>
#include <utils/Random.h>

Function *sip_hash_fn = nullptr;

Function *CreateFunctionForStateResolverCheck(
    Module *m, uint64_t target_state,
    ControlFlowFlattening::TransformationOptions *options,
    std::set<uint64_t> &states, bool is_arm32);

Value *GetTargetState(Module *m, IRBuilderBase &builder, uint64_t target_state,
                      ControlFlowFlattening::TransformationOptions *options,
                      bool is_arm32);

void MaybeTransformDispatcherState(
    Module *m, IRBuilderBase &builder, Value *&dispatcher_state,
    uint64_t &target_state,
    ControlFlowFlattening::TransformationOptions *options,
    std::set<uint64_t> &states, bool is_arm32);

void ControlFlowFlattening::RunOnFunction(Function &f,
                                          ZyroxPassOptions *options)
{
    if (FunctionUtils::HasCXXExceptions(f))
    {
        Logger::Info("ControlFlowFlattening: Skipping {}, function have "
                     "c++ exceptions which are "
                     "unsupported yet.",
                     demangle(f.getName()));
        return;
    }

    TransformationOptions t_options = {
        .UseFunctionResolverChance =
            options->Get("ControlFlowFlattening.UseFunctionResolverChance"),
        .UseGlobalStateVariablesChance =
            options->Get("ControlFlowFlattening.UseGlobalStateVariablesChance"),
        .UseOpaqueTransformationChance =
            options->Get("ControlFlowFlattening.UseOpaqueTransformationChance"),
        .UseGlobalVariableOpaquesChance = options->Get(
            "ControlFlowFlattening.UseGlobalVariableOpaquesChance"),
        .UseSipHashedStateChance =
            options->Get("ControlFlowFlattening.UseSipHashedStateChance"),
        .CloneSipHashChance =
            options->Get("ControlFlowFlattening.CloneSipHashChance"),
    };

    if (sip_hash_fn == nullptr)
    {
        sip_hash_fn = f.getParent()->getFunction("___siphash");
        if (sip_hash_fn == nullptr)
        {
            Logger::Error(
                "siphash function is not found, was HashUtil IR linked?");
        }
        FunctionUtils::DemoteRegToStack(*sip_hash_fn);
        FunctionUtils::FlattenSwitches(*sip_hash_fn);
        FunctionUtils::DemotePHIToStack(*sip_hash_fn);
        sip_hash_fn->setLinkage(GlobalValue::InternalLinkage);
    }

    int iterations_count = options->Get("PassIterations");

    for (int i = 0; i < iterations_count; i++)
    {
        ObfuscateFunction(f, &t_options);
    }

    FunctionUtils::ShuffleBlocks(f);
    FunctionUtils::EnsureAllocasInEntryBlocks(f);
    FunctionUtils::DemoteRegToStack(f);
    FunctionUtils::DemotePHIToStack(f);
}

void ControlFlowFlattening::RegisterFromAnnotation(Function &f,
                                                   ZyroxAnnotationArgs *args)
{
    ZyroxMetaDataKV kv = {
        {"PassIterations", args->NextOrDefault(1)},
        {"ControlFlowFlattening.UseFunctionResolverChance", args->Next()},
        {"ControlFlowFlattening.UseGlobalStateVariablesChance", args->Next()},
        {"ControlFlowFlattening.UseOpaqueTransformationChance", args->Next()},
        {"ControlFlowFlattening.UseGlobalVariableOpaquesChance", args->Next()},
        {"ControlFlowFlattening.UseSipHashedStateChance", args->Next()},
        {"ControlFlowFlattening.CloneSipHashChance", args->Next()}};
    ZyroxPassesMetadata::AddPass(f, pass_info.CodeName, kv);
}

void ControlFlowFlattening::ObfuscateFunction(Function &f,
                                              TransformationOptions *options)
{
    LLVMContext &ctx = f.getContext();

    if (f.size() < 2)
        return;

    IRBuilder builder(&*f.getEntryBlock().getFirstInsertionPt());
    const DataLayout &dl = f.getParent()->getDataLayout();

    unsigned ptr_size = dl.getPointerSize(0);

#define IS_ARM32() (ptr_size == 4)
    IntegerType *int_ty =
        IS_ARM32() ? builder.getInt32Ty() : builder.getInt64Ty();

    AllocaInst *dispatcher_state =
        builder.CreateAlloca(int_ty, nullptr, "state");
    builder.CreateStore(ConstantInt::get(int_ty, 0), dispatcher_state, true);

    std::vector<BasicBlock *> original_blocks;
    for (auto &bb : f)
    {
        if (&bb != &f.getEntryBlock())
        {
            original_blocks.push_back(&bb);
        }
    }

    std::map<BasicBlock *, uint64_t> block_state_map;
    std::set<uint64_t> states;
    for (auto *bb : original_blocks)
    {
        uint64_t state;
        do
        {
            if (IS_ARM32())
            {
                state = Random::IntRanged<uint64_t>(0x000F0000, UINT32_MAX);
            }
            else
            {
                state = Random::IntRanged<uint64_t>(0x000F0000, UINT64_MAX);
            }
        } while (states.contains(state));
        states.insert(state);
        block_state_map[bb] = state;
        BasicBlockUtils::AddMetaData(bb, "cff.dispatcher_state",
                                     dispatcher_state);
        BasicBlockUtils::AddMetaData(bb, "cff.block_state",
                                     block_state_map[bb]);
    }

    std::vector<BasicBlock *> condition_blocks;
    for (int i = 0; i < original_blocks.size(); ++i)
    {
        BasicBlock *bb =
            BasicBlock::Create(ctx, "cond_check." + std::to_string(i), &f);
        condition_blocks.push_back(bb);
    }

    // branch to first condition block
    // we create the jump from entry to dispatcher at the end.
    BasicBlock *dispatch_bb = BasicBlock::Create(ctx, "dispatch", &f);
    builder.SetInsertPoint(dispatch_bb);
    builder.CreateBr(condition_blocks.front());

    Module *m = f.getParent();

    for (int i = 0; i < condition_blocks.size(); ++i)
    {
        builder.SetInsertPoint(condition_blocks[i]);

        uint64_t target_state = block_state_map[original_blocks[i]];

        Value *state_val =
            builder.CreateLoad(int_ty, dispatcher_state, true, "state_val");

        Value *cmp;
        if (Random::Chance(options->UseFunctionResolverChance))
        {
            Function *state_resolver = CreateFunctionForStateResolverCheck(
                m, target_state, options, states, IS_ARM32());
            cmp = builder.CreateCall(state_resolver, {state_val});
        }
        else
        {
            MaybeTransformDispatcherState(m, builder, state_val, target_state,
                                          options, states, IS_ARM32());
            cmp = builder.CreateICmpEQ(
                state_val,
                GetTargetState(m, builder, target_state, options, IS_ARM32()));
        }

        if (i < condition_blocks.size() - 1)
        {
            // not last block: branch to target or next condition
            builder.CreateCondBr(cmp, original_blocks[i],
                                 condition_blocks[i + 1]);
        }
        else
        {
            // last block: branch to target or default (first block)
            BasicBlock *default_bb = BasicBlock::Create(ctx, "default", &f);
            IRBuilder<> default_bb_ir(default_bb);
            default_bb_ir.CreateBr(dispatch_bb);

            builder.CreateCondBr(cmp, original_blocks[i], default_bb);
        }
    }

    // modify state and return to dispatcher
    // entry block could have a condition so we cannot just put a Br and call it
    // a day.
    // so we just handle it as any other block.
    original_blocks.push_back(&f.getEntryBlock());
    for (auto *bb : original_blocks)
    {
        Instruction *terminator = bb->getTerminator();
        builder.SetInsertPoint(terminator);

        if (auto *br = dyn_cast<BranchInst>(terminator))
        {
            if (br->isUnconditional())
            {
                BasicBlock *target = br->getSuccessor(0);
                builder.CreateStore(
                    ConstantInt::get(int_ty, block_state_map[target]),
                    dispatcher_state, true);
                terminator->replaceAllUsesWith(builder.CreateBr(dispatch_bb));
                terminator->eraseFromParent();
            }
            else
            {
                BasicBlock *true_bb = br->getSuccessor(0);
                BasicBlock *false_bb = br->getSuccessor(1);

                BasicBlock *true_state =
                    BasicBlock::Create(ctx, "cff.block.true_state", &f);
                BasicBlock *false_state =
                    BasicBlock::Create(ctx, "cff.block.false_state", &f);

                builder.SetInsertPoint(true_state);
                builder.CreateStore(
                    ConstantInt::get(int_ty, block_state_map[true_bb]),
                    dispatcher_state, true);
                builder.CreateBr(dispatch_bb);

                builder.SetInsertPoint(false_state);
                builder.CreateStore(
                    ConstantInt::get(int_ty, block_state_map[false_bb]),
                    dispatcher_state, true);
                builder.CreateBr(dispatch_bb);

                builder.SetInsertPoint(terminator);

                terminator->replaceAllUsesWith(builder.CreateCondBr(
                    br->getCondition(), true_state, false_state));
                terminator->eraseFromParent();
            }
        }
    }

    FunctionUtils::DemoteRegToStack(f);
}

Function *CreateFunctionForStateResolverCheck(
    Module *m, uint64_t target_state,
    ControlFlowFlattening::TransformationOptions *options,
    std::set<uint64_t> &states, bool is_arm32)
{
    LLVMContext &ctx = m->getContext();
    FunctionType *function_ty = FunctionType::get(
        Type::getInt1Ty(ctx),
        {is_arm32 ? Type::getInt32Ty(ctx) : Type::getInt64Ty(ctx)}, false);
    Function *f = Function::Create(function_ty, GlobalValue::InternalLinkage,
                                   "cff_resolve_state_check", m);

    BasicBlock *state_resolver_block =
        BasicBlock::Create(ctx, "resolver.entry", f);

    IRBuilder builder(state_resolver_block);

    Value *state_arg = f->getArg(0);
    MaybeTransformDispatcherState(m, builder, state_arg, target_state, options,
                                  states, is_arm32);

    Value *cmp = builder.CreateICmpEQ(
        state_arg, GetTargetState(m, builder, target_state, options, is_arm32));

    builder.CreateRet(cmp);

    return f;
}

Value *GetTargetState(Module *m, IRBuilderBase &builder, uint64_t target_state,
                      ControlFlowFlattening::TransformationOptions *options,
                      bool is_arm32)
{
    if (Random::Chance(options->UseGlobalStateVariablesChance))
    {
        GlobalVariable *gv = new GlobalVariable(
            *m, is_arm32 ? builder.getInt32Ty() : builder.getInt64Ty(), false,
            GlobalValue::PrivateLinkage,
            is_arm32 ? builder.getInt32(target_state)
                     : builder.getInt64(target_state),
            "__state_" + std::to_string(target_state));

        return builder.CreateLoad(
            is_arm32 ? builder.getInt32Ty() : builder.getInt64Ty(), gv, true);
    }
    return is_arm32 ? builder.getInt32(target_state)
                    : builder.getInt64(target_state);
}

void MaybeTransformDispatcherState(
    Module *m, IRBuilderBase &builder, Value *&dispatcher_state,
    uint64_t &target_state,
    ControlFlowFlattening::TransformationOptions *options,
    std::set<uint64_t> &states, bool is_arm32)
{
    if (Random::Chance(options->UseSipHashedStateChance))
    {
        uint64_t hashed_state = 0;
        while (true)
        {
            uint64_t SipHashStateOptions[] = {
                Random::IntRanged<uint64_t>(0x000F0000, UINT64_MAX), // k0
                Random::IntRanged<uint64_t>(0x000F0000, UINT64_MAX), // k1
                Random::IntRanged<uint64_t>(0x000F0000, UINT64_MAX), // v0
                Random::IntRanged<uint64_t>(0x000F0000, UINT64_MAX), // v1
                Random::IntRanged<uint64_t>(0x000F0000, UINT64_MAX), // v2
                Random::IntRanged<uint64_t>(0x000F0000, UINT64_MAX), // v3
            };
#define ARG(n) SipHashStateOptions[n]
#define SIPHASH(a1, a2, a3, a4, a5, a6, a7)                                    \
    (HashUtils::SipHash(a1, a2, a3, a4, a5, a6, a7) &                          \
     (is_arm32 ? UINT32_MAX : UINT64_MAX))

            hashed_state = SIPHASH(target_state, ARG(0), ARG(1), ARG(2), ARG(3),
                                   ARG(4), ARG(5));
            int collisions_count = 0;
            for (uint64_t state : states)
            {
                if (SIPHASH(state, ARG(0), ARG(1), ARG(2), ARG(3), ARG(4),
                            ARG(5)) == hashed_state)
                {
                    collisions_count++;
                }
            }
#undef ARG
            if (collisions_count == 1 && !states.contains(hashed_state))
            {
                // only TargetState matches this hashed output
                target_state = hashed_state;
#define ARG(n) builder.getInt64(SipHashStateOptions[n])
                Function *fn = sip_hash_fn;

                if (Random::Chance(options->CloneSipHashChance))
                {
                    ValueToValueMapTy vmap;
                    fn = CloneFunction(sip_hash_fn, vmap);
                    fn->setLinkage(GlobalValue::InternalLinkage);
                    // cross fingers later passes will apply this, lol. llvm
                    // will use a threshold so it won't be THAT bad
                    fn->addFnAttr(Attribute::AlwaysInline);
                    fn->removeFnAttr(Attribute::NoInline);

                    assert(!verifyFunction(*fn, &errs()) &&
                           "Cloned function is broken!");
                }

                Value *value = builder.CreateCall(
                    fn,
                    {is_arm32 ? builder.CreateZExt(dispatcher_state,
                                                   builder.getInt64Ty(), "zext")
                              : dispatcher_state,
                     ARG(0), ARG(1), ARG(2), ARG(3), ARG(4), ARG(5)});

                dispatcher_state =
                    is_arm32 ? builder.CreateTrunc(value, builder.getInt32Ty(),
                                                   "trunc")
                             : value;
#undef ARG
                break;
            }
        }
    }
    if (Random::Chance(options->UseOpaqueTransformationChance))
    {
        OpaqueTransformer transformer(is_arm32);
        dispatcher_state =
            transformer.Transform(*m, builder, dispatcher_state,
                                  options->UseGlobalVariableOpaquesChance);
        target_state = transformer.TransformConstant(target_state);
    }
}