#include <Passes/BasicBlockSplitter.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <utils/FunctionUtils.h>
#include <utils/Logger.h>
#include <utils/Random.h>

void SplitBlock(BasicBlock *bb, int block_max_size);

void BasicBlockSplitter::RunOnFunction(Function &f, ZyroxPassOptions *options)
{
    if (FunctionUtils::HasCXXExceptions(f))
    {
        Logger::Info("BasicBlockSplitter: Skipping {}, function have "
                     "c++ exceptions which are "
                     "unsupported yet.",
                     demangle(f.getName()));
        return;
    }

    int iterations_count = options->Get("PassIterations");

    int min_block_size = options->Get("BasicBlockSplitter.SplitBlockMinSize");
    int max_block_size = options->Get("BasicBlockSplitter.SplitBlockMaxSize");
    int block_split_chance =
        options->Get("BasicBlockSplitter.SplitBlockChance");

    if (min_block_size == 0)
        min_block_size = 10;

    if (max_block_size == 0)
        max_block_size = 20;

    if (block_split_chance == 0)
        block_split_chance = 40;

    FunctionUtils::FlattenSwitches(f);
    FunctionUtils::DemotePHIToStack(f);
    FunctionUtils::FlattenSwitches(f);

    for (int i = 0; i < iterations_count; i++)
    {
        ObfuscateFunction(f, min_block_size, max_block_size,
                          block_split_chance);
    }

    FunctionUtils::ShuffleBlocks(f);
    FunctionUtils::DemoteRegToStack(f);
}

void BasicBlockSplitter::RegisterFromAnnotation(Function &f,
                                                ZyroxAnnotationArgs *args)
{
    ZyroxMetaDataKV kv = {
        {"PassIterations", args->NextOrDefault(1)},
        {"BasicBlockSplitter.SplitBlockMinSize", args->NextOrDefault(10)},
        {"BasicBlockSplitter.SplitBlockMaxSize", args->NextOrDefault(20)},
        {"BasicBlockSplitter.SplitBlockChance", args->NextOrDefault(40)},
    };
    ZyroxPassesMetadata::AddPass(f, pass_info.CodeName, kv);
}

void BasicBlockSplitter::ObfuscateFunction(Function &f, int min_block_size,
                                           int max_block_size,
                                           int block_split_chance)
{
    std::vector<BasicBlock *> blocks;
    BasicBlock *largest_block = nullptr;

    for (BasicBlock &bb : f)
    {
        if (size_t block_size = bb.size(); block_size >= min_block_size)
        {
            largest_block = &bb;
            if (Random::Chance(block_split_chance))
            {
                blocks.push_back(&bb);
            }
        }
    }

    // Use the largest block if no candidates found
    if (blocks.empty() && largest_block)
    {
        blocks.push_back(largest_block);
    }

    // Split all candidate blocks to enforce MaxBlockSize
    for (BasicBlock *bb : blocks)
    {
        SplitBlock(bb, max_block_size);
    }
}

void SplitBlock(BasicBlock *bb, int block_max_size)
{
    if (!bb || bb->size() <= block_max_size)
        return;

    // Use worklist to handle nested splits
    std::vector<BasicBlock *> work_list;
    work_list.push_back(bb);

    while (!work_list.empty())
    {
        BasicBlock *current = work_list.back();
        work_list.pop_back();

        // Skip blocks that are already small enough
        if (current->size() <= block_max_size)
            continue;

        // Get iterators for non-PHI instructions
        Instruction *first_non_phi = current->getFirstNonPHI();
        BasicBlock::iterator start_it(first_non_phi);
        BasicBlock::iterator end_it = current->end();
        size_t ins_count = std::distance(start_it, end_it);

        // Cannot split if no valid split points
        if (ins_count < 2)
            continue;

        // Calculate target split size
        size_t target_size =
            std::min(static_cast<size_t>(block_max_size) - 1, ins_count - 1);
        if (target_size == 0)
            continue;

        // Find valid split point
        BasicBlock::iterator split_it = start_it;
        std::advance(split_it, target_size);

        // Adjust backward if at terminator
        if (split_it->isTerminator())
        {
            while (split_it != start_it && split_it->isTerminator())
            {
                --split_it;
            }
            // Skip if no valid split found
            if (split_it->isTerminator())
                continue;
        }

        // Perform the split
        BasicBlock *new_block = current->splitBasicBlock(&*split_it);

        // Add both blocks back to worklist for further splitting
        work_list.push_back(current);
        work_list.push_back(new_block);
    }
}