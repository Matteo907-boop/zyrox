#include <Passes/MBASub.hpp>
#include <Passes/SimpleIndirectBranch.h>
#include <core/ZyroxMetaData.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <numeric>
#include <utils/Random.h>

void SimpleIndirectBranch::RunOnFunction(Function &f, ZyroxPassOptions *options)
{
    int iterations_count = options->Get("PassIterations");

    int replace_br_chance = options->Get("SimpleIndirectBranch.Chance");
    if (replace_br_chance == 0)
        replace_br_chance = 50;

    for (int i = 0; i < iterations_count; i++)
    {
        ObfuscateFunction(f, replace_br_chance);
    }
}

void SimpleIndirectBranch::RegisterFromAnnotation(Function &f,
                                                  ZyroxAnnotationArgs *args)
{
    ZyroxMetaDataKV kv = {
        {"PassIterations", args->NextOrDefault(1)},
        {"SimpleIndirectBranch.Chance", args->NextOrDefault(40)},
    };
    ZyroxPassesMetadata::AddPass(f, pass_info.CodeName, kv);
}

Value *ComputeFakeIndex(IRBuilder<> &builder, Value *index);

void SimpleIndirectBranch::ObfuscateFunction(Function &f, int replace_br_chance)
{
    if (f.size() < 2)
        return;

    int count = std::accumulate(
        f.begin(), f.end(), 0u,
        [](unsigned acc, const BasicBlock &bb)
        {
            return acc +
                   (bb.getTerminator()->getOpcode() == Instruction::Br ? 1 : 0);
        });

    if (count == 0)
        return;

    BasicBlock *entry = &f.getEntryBlock();
    IRBuilder builder(&*entry->getFirstInsertionPt());

    Type *pint_ty = PointerType::getUnqual(builder.getInt8Ty());

    ArrayType *blocks_addresses_array_t = ArrayType::get(pint_ty, 2);

    Value *blocks_addresses_array = builder.CreateAlloca(
        blocks_addresses_array_t, nullptr, "ibr.blocksAddressesArray");

    bool is_thumb_mode = f.hasFnAttribute("target-features") &&
                         f.getFnAttribute("target-features")
                             .getValueAsString()
                             .contains("+thumb-mode");

    for (BasicBlock &bb : f)
    {
        Instruction *term = bb.getTerminator();
        if (auto *branch = dyn_cast<BranchInst>(term))
        {
            if (!Random::Chance(replace_br_chance))
                continue;

            builder.SetInsertPoint(branch);

            for (unsigned i = 0; i < branch->getNumSuccessors(); ++i)
            {
                Value *indices[] = {builder.getInt32(0), builder.getInt32(i)};
                Value *block_index = builder.CreateGEP(
                    blocks_addresses_array_t, blocks_addresses_array, indices);
                builder.CreateStore(BlockAddress::get(branch->getFunction(),
                                                      branch->getSuccessor(i)),
                                    block_index, true);
            }

            Value *index;

            if (branch->isConditional())
            {
                Value *inverted_cond =
                    builder.CreateNot(branch->getCondition());
                index = builder.CreateZExt(inverted_cond, builder.getInt32Ty());
            }
            else
            {
                index = builder.getInt32(0);
            }

            index = ComputeFakeIndex(builder, index);

            Value *indices[] = {builder.getInt32(0), index};
            Value *gep = builder.CreateGEP(blocks_addresses_array_t,
                                           blocks_addresses_array, indices);
            Value *load_indirect_addr = builder.CreateLoad(pint_ty, gep, true);

            if (is_thumb_mode)
            {
                Value *addr_int = builder.CreatePtrToInt(
                    load_indirect_addr, builder.getInt32Ty(), "ptr_to_int");
                Value *thumb_addr_int = builder.CreateOr(
                    addr_int, ConstantInt::get(builder.getInt32Ty(), 1),
                    "thumb_adjusted_addr");
                load_indirect_addr = builder.CreateIntToPtr(
                    thumb_addr_int, pint_ty, "adjusted_ptr");
            }

            IndirectBrInst *indir_branch = builder.CreateIndirectBr(
                load_indirect_addr, branch->getNumSuccessors());

            for (BasicBlock *successor : branch->successors())
            {
                indir_branch->addDestination(successor);
            }

            branch->replaceAllUsesWith(indir_branch);
            branch->eraseFromParent();
        }
    }
}

Value *ComputeFakeIndex(IRBuilder<> &builder, Value *index)
{
    Type *int_ty = index->getType();
    unsigned bit_width = int_ty->getIntegerBitWidth();

    uint64_t rand_val = bit_width <= 32 ? Random::UInt32() : Random::UInt64();
    Constant *rand = ConstantInt::get(int_ty, rand_val);

    BinaryOperator *xor1 = BinaryOperator::CreateXor(index, rand);
    builder.Insert(xor1);
    Value *obf1 = MBASub::ObfuscateXor(builder, xor1);

    BinaryOperator *xor2 = BinaryOperator::CreateXor(obf1, rand);
    builder.Insert(xor2);
    return MBASub::ObfuscateXor(builder, xor2);
}