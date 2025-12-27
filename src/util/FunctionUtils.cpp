#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Utils/Local.h>
#include <random>
#include <utils/BasicBlockUtils.h>
#include <utils/FunctionUtils.h>

bool FunctionUtils::HasCXXExceptions(Function &f)
{
    for (BasicBlock &bb : f)
    {
        if (Instruction *terminator = bb.getTerminator();
            bb.isEHPad() || bb.isLandingPad() || isa<ResumeInst>(terminator) ||
            isa<InvokeInst>(terminator))
        {
            return true;
        }
    }
    return false;
}

void FunctionUtils::ShuffleBlocks(Function &f)
{
    if (f.empty())
        return;

    std::vector<BasicBlock *> b_bs;
    BasicBlock &entry = f.getEntryBlock();

    for (BasicBlock &bb : f)
    {
        if (&bb != &entry)
        {
            b_bs.push_back(&bb);
        }
    }

    if (b_bs.empty())
        return;

    // Shuffle the collected blocks
    std::random_device rd;
    std::mt19937 g(rd());
    std::ranges::shuffle(b_bs, g);

    // Reattach blocks in shuffled order after entry
    BasicBlock *insert_point = &entry;
    for (BasicBlock *bb : b_bs)
    {
        bb->moveAfter(insert_point);
        insert_point = bb;
    }
}

void FunctionUtils::DemotePHIToStack(Function &f)
{
    std::vector<PHINode *> phi_nodes;

    for (BasicBlock &bb : f)
    {
        for (Instruction &i : bb)
        {
            if (PHINode *phi = dyn_cast<PHINode>(&i))
            {
                phi_nodes.push_back(phi);
            }
        }
    }

    for (PHINode *phi : phi_nodes)
    {
        llvm::DemotePHIToStack(phi, nullptr);
    }
}

void FunctionUtils::DemoteRegToStack(Function &f)
{
    DominatorTree dt;
    dt.recalculate(f);
    std::vector<Instruction *> to_demote;
    for (BasicBlock &bb : f)
    {
        for (Instruction &i : bb)
        {
            if (i.getType()->isVoidTy() || isa<AllocaInst>(i) ||
                i.isTerminator())
                continue;
            for (User *u : i.users())
            {
                if (Instruction *ui = dyn_cast<Instruction>(u))
                {
                    if (!dt.dominates(&i, ui))
                    {
                        to_demote.push_back(&i);
                        break;
                    }
                }
            }
        }
    }
    for (Instruction *i : to_demote)
    {
        llvm::DemoteRegToStack(*i, false);
    }
}

void FunctionUtils::FlattenSwitches(Function &f)
{
    for (BasicBlock &bb : f)
    {
        if (isa<SwitchInst>(bb.getTerminator()))
        {
            BasicBlockUtils::FlattenSwitch(&bb);
        }
    }
}

void FunctionUtils::EnsureAllocasInEntryBlocks(Function &f)
{
    Instruction *first_instruction = &*f.getEntryBlock().begin();

    std::vector<AllocaInst *> allocas_to_move;
    for (BasicBlock &bb : f)
    {
        if (bb.isEntryBlock())
            continue;

        for (Instruction &i : bb)
        {
            if (AllocaInst *ai = dyn_cast<AllocaInst>(&i))
            {
                allocas_to_move.push_back(ai);
            }
        }
    }

    for (AllocaInst *ai : allocas_to_move)
    {
        ai->moveBefore(first_instruction);
    }
}
