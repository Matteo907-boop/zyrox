#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instruction.h>
#include <utils/BasicBlockUtils.h>
#include <utils/Logger.h>

void BasicBlockUtils::FlattenSwitch(BasicBlock *bb)
{
    Instruction *terminator = bb->getTerminator();
    if (!isa<SwitchInst>(terminator))
    {
        Logger::Warn(
            "BasicBlockUtil::FlattenSwitch: received a Basic Block that"
            "doesn't have a switch");
        return;
    }

    SwitchInst *switch_inst = dyn_cast<SwitchInst>(terminator);

    std::vector<BasicBlock *> b_bs;
    Function *f = bb->getParent();
    LLVMContext &ctx = f->getContext();

    int i;
    for (i = 0; i < switch_inst->getNumCases(); i++)
    {
        b_bs.push_back(
            BasicBlock::Create(ctx, "switch.case." + std::to_string(i), f));
    }

    IRBuilder builder(&*bb->getParent()->getEntryBlock().getFirstInsertionPt());
    Value *switch_cond = switch_inst->getCondition();
    AllocaInst *switch_cond_val =
        builder.CreateAlloca(switch_cond->getType(), nullptr, "switch.cond");

    builder.SetInsertPoint(switch_inst);
    builder.CreateStore(switch_cond, switch_cond_val);
    terminator->replaceAllUsesWith(builder.CreateBr(b_bs.front()));

    i = 0;
    for (const SwitchInst::CaseHandle &case_handle : switch_inst->cases())
    {
        BasicBlock *case_cmp_block = b_bs[i++];
        builder.SetInsertPoint(case_cmp_block);
        Value *state_val = builder.CreateLoad(
            switch_cond->getType(), switch_cond_val, "switch.cond.val");
        Value *cmp =
            builder.CreateICmpEQ(state_val, case_handle.getCaseValue());
        if (i != b_bs.size())
        {
            // go to next case check
            builder.CreateCondBr(cmp, case_handle.getCaseSuccessor(), b_bs[i]);
        }
        else
        {
            builder.CreateCondBr(cmp, case_handle.getCaseSuccessor(),
                                 switch_inst->getDefaultDest());
        }
    }

    terminator->eraseFromParent();
}

std::unordered_map<BasicBlock *, std::unordered_map<std::string, std::any>>
    meta_map;

void BasicBlockUtils::AddMetaData(BasicBlock *bb, std::string key,
                                  std::any value)
{
    if (!meta_map.contains(bb))
    {
        meta_map[bb] = {};
    }
    meta_map[bb][key] = value;
}

std::optional<std::any> BasicBlockUtils::GetMetaData(BasicBlock *bb,
                                                     std::string key)
{
    if (meta_map.contains(bb))
        return meta_map[bb][key];
    return std::nullopt;
}

void BasicBlockUtils::RemoveMetaData(BasicBlock *bb) { meta_map.erase(bb); }
