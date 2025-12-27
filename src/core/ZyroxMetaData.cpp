#include <core/ZyroxMetaData.h>
#include <core/ZyroxPassOptions.h>
#include <llvm/IR/Constants.h>

ZyroxFunctionPass ZyroxPassOptions::GetPass()
{
    if (auto *s = dyn_cast<MDString>(m_Node->getOperand(0)))
    {
        StringRef pass_code_name = s->getString();
        for (ZyroxFunctionPass function_pass : zyrox_passes)
        {
            if (function_pass.CodeName == pass_code_name)
            {
                return function_pass;
            }
        }
    }
    llvm_unreachable("");
}

int ZyroxPassOptions::Get(StringRef key)
{
    if (m_Node->getNumOperands() < 2)
        return 0;

    auto *params = dyn_cast<MDNode>(m_Node->getOperand(1));
    if (!params)
        return 0;

    for (unsigned i = 0; i + 1 < params->getNumOperands(); i += 2)
    {
        if (auto *k = dyn_cast<MDString>(params->getOperand(i)))
        {
            if (k->getString() == key)
            {
                if (auto *v =
                        dyn_cast<ConstantAsMetadata>(params->getOperand(i + 1)))
                {
                    if (auto *ci = dyn_cast<ConstantInt>(v->getValue()))
                    {
                        return ci->getZExtValue();
                    }
                }
            }
        }
    }

    return 0;
}

void ZyroxPassOptions::RunPass(Function &f)
{
    this->GetPass().RunOnFunction(f, this);
}

void ZyroxPassesMetadata::AddPass(Function &f, StringRef kind,
                                  ZyroxMetaDataKV &key_vals)
{
    LLVMContext &ctx = f.getContext();

    SmallVector<Metadata *, 8> param_nodes;
    for (const auto &[K, V] : key_vals)
    {
        param_nodes.push_back(MDString::get(ctx, K));
        param_nodes.push_back(ConstantAsMetadata::get(
            ConstantInt::get(Type::getInt64Ty(ctx), V)));
    }

    MDTuple *param_tuple = MDTuple::get(ctx, param_nodes);
    MDTuple *kind_tuple =
        MDTuple::get(ctx, {MDString::get(ctx, kind), param_tuple});

    MDNode *root = f.getMetadata("zyrox");
    SmallVector<Metadata *, 8> root_nodes;

    if (root)
    {
        for (const auto &op : root->operands())
            root_nodes.push_back(op.get());
    }

    root_nodes.push_back(kind_tuple);
    f.setMetadata("zyrox", MDTuple::get(ctx, root_nodes));
}

void ZyroxPassesMetadata::RemovePass(Function &f, const std::string &pass_name)
{
    MDNode *root = f.getMetadata("zyrox");
    if (!root)
        return;

    LLVMContext &ctx = f.getContext();
    std::vector<Metadata *> new_ops;

    for (const auto &op : root->operands())
    {
        MDNode *node = dyn_cast<MDNode>(op.get());
        if (!node)
            continue;

        if (MDString *s = dyn_cast<MDString>(node->getOperand(0)))
        {
            if (s->getString() == pass_name)
                continue;
        }

        new_ops.push_back(node);
    }

    if (new_ops.empty())
    {
        f.setMetadata("zyrox", nullptr);
    }
    else
    {
        MDNode *new_md = MDNode::get(ctx, new_ops);
        f.setMetadata("zyrox", new_md);
    }
}

std::vector<ZyroxPassOptions> ZyroxPassesMetadata::PassesOf(Function &f)
{
    std::vector<ZyroxPassOptions> result;
    MDNode *root = f.getMetadata("zyrox");

    if (!root)
        return result;

    for (const auto &op : root->operands())
    {
        if (auto *n = dyn_cast<MDNode>(op.get()))
            result.emplace_back(n);
    }

    return result;
}

StringRef ZyroxPassesMetadata::Get(Instruction *i, StringRef kind_name)
{
    unsigned kind_id = i->getContext().getMDKindID(kind_name);

    if (MDNode *n = i->getMetadata(kind_id))
    {
        if (auto *md_str = dyn_cast<MDString>(n->getOperand(0)))
        {
            return md_str->getString();
        }
    }
    return StringRef();
}

void ZyroxPassesMetadata::Put(Instruction *i, StringRef kind_name,
                              StringRef value)
{
    LLVMContext &ctx = i->getContext();
    unsigned kind_id = ctx.getMDKindID(kind_name);

    MDNode *n = MDNode::get(ctx, MDString::get(ctx, value));
    i->setMetadata(kind_id, n);
}

void ZyroxPassesMetadata::MarkObfuscated(Function &f)
{
    LLVMContext &ctx = f.getContext();

    MDNode *md = MDNode::get(ctx, MDString::get(ctx, "obfuscated"));

    f.setMetadata("obfuscated", md);
}

bool ZyroxPassesMetadata::IsObfuscated(Function &f)
{
    MDNode *md = f.getMetadata("obfuscated");
    if (!md)
        return false;

    if (MDString *s = dyn_cast<MDString>(md->getOperand(0)))
        return s->getString() == "obfuscated";

    return false;
}