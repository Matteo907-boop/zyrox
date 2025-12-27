#include <fstream>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <ostream>
#include <utils/CryptoUtils.h>
#include <utils/Logger.h>

std::map<int, CryptoUtils::ZyroxTable> CryptoUtils::zyrox_tables;

int CryptoUtils::zyrox_table_counter = 1;

void CryptoUtils::AddZyroxTable(ZyroxTable &zyrox_table)
{
    zyrox_tables[zyrox_table.table_id] = zyrox_table;
}

int CryptoUtils::GetUniqueZyroxTableId() { return zyrox_table_counter++; }

void CryptoUtils::FinalizeZyroxTables()
{
    std::ofstream outfile("zyrox_tables.txt");
    if (!outfile.is_open())
    {
        Logger::Error("Error opening output file zyrox_tables.txt");
    }

    for (const auto &pair : zyrox_tables)
    {
        const ZyroxTable &table = pair.second;

        outfile << "@table " << table.table_id << "\n";

        for (const ZyroxTableEntryInfo &entry : table.entries)
        {
            outfile << entry.xtea_key[0] << " " << entry.xtea_key[1] << " "
                    << entry.xtea_key[2] << " " << entry.xtea_key[3] << " "
                    << entry.delta << " " << entry.nb_rounds << "\n";
        }
    }

    outfile.close();
}

// call me a mad-ass but this is the only way to safely inline it ;)
void CryptoUtils::WriteXTEADecipher(IRBuilderBase &builder, XteaInfo &xtea_info,
                                    XteaOptions &, Value *value,
                                    AllocaInst *var_v0, AllocaInst *var_v1,
                                    AllocaInst *var_sum, AllocaInst *var_i)
{
    Type *u32_ty = builder.getInt32Ty();

    Function *fn = builder.GetInsertBlock()->getParent();
    LLVMContext &ctx = fn->getContext();

    BasicBlock *current_bb = builder.GetInsertBlock();
    BasicBlock *split_bb =
        current_bb->splitBasicBlock(builder.GetInsertPoint(), "xtea.split_bb");

    Value *arg_v = value;
    arg_v->setName("v");

    builder.SetInsertPoint(current_bb->getTerminator());

    Value *xtea_key = xtea_info.xtea_key;

    // Load v[0] and v[1] into v0 and v1
    Value *v0_ptr =
        builder.CreateInBoundsGEP(u32_ty, arg_v, builder.getInt32(0), "v0_ptr");
    Value *v1_ptr =
        builder.CreateInBoundsGEP(u32_ty, arg_v, builder.getInt32(1), "v1_ptr");

    Value *v0_val = builder.CreateLoad(u32_ty, v0_ptr, "v0_val");
    Value *v1_val = builder.CreateLoad(u32_ty, v1_ptr, "v1_val");

    builder.CreateStore(v0_val, var_v0);
    builder.CreateStore(v1_val, var_v1);

    // sum = delta * num_rounds
    Value *sum_init = builder.CreateMul(xtea_info.delta, xtea_info.num_rounds);
    builder.CreateStore(sum_init, var_sum);

    // i = 0
    builder.CreateStore(builder.getInt32(0), var_i);

    // Create loop basic blocks
    BasicBlock *loop_cond = BasicBlock::Create(ctx, "loop_cond", fn);
    BasicBlock *loop_body = BasicBlock::Create(ctx, "loop_body", fn);
    BasicBlock *loop_end = BasicBlock::Create(ctx, "loop_end", fn);

    // Jump from entry to loop_cond
    builder.CreateBr(loop_cond);
    current_bb->getTerminator()->eraseFromParent(); // remove a br to split_bb

    // Loop condition block
    builder.SetInsertPoint(loop_cond);
    Value *i_val = builder.CreateLoad(u32_ty, var_i, "i_val");
    Value *cond =
        builder.CreateICmpULT(i_val, xtea_info.num_rounds, "loop_cond");
    builder.CreateCondBr(cond, loop_body, loop_end);

    // Loop body block
    builder.SetInsertPoint(loop_body);

    // Load v0, v1, sum
    v0_val = builder.CreateLoad(u32_ty, var_v0, "v0");
    v1_val = builder.CreateLoad(u32_ty, var_v1, "v1");
    Value *sum_val = builder.CreateLoad(u32_ty, var_sum, "sum");

    // Compute ((v0 << 4) ^ (v0 >> 5)) + v0
    Value *v0_shl4 = builder.CreateShl(v0_val, builder.getInt32(4));
    Value *v0_lshr5 = builder.CreateLShr(v0_val, builder.getInt32(5));
    Value *v0_xor = builder.CreateXor(v0_shl4, v0_lshr5);
    Value *tmp1 = builder.CreateAdd(v0_xor, v0_val);

    // key index: (sum >> 11) & 3
    Value *sum_shr11 = builder.CreateLShr(sum_val, builder.getInt32(11));
    Value *key_index = builder.CreateAnd(sum_shr11, builder.getInt32(3));

    // Load key[key_index]
    Value *key_ptr =
        builder.CreateInBoundsGEP(u32_ty, xtea_key, key_index, "key_ptr");
    Value *key_val = builder.CreateLoad(u32_ty, key_ptr, "key_val");

    // Calculate v1 -= (tmp1 ^ (sum + key[key_index]))
    Value *sum_plus_key = builder.CreateAdd(sum_val, key_val);
    Value *xor_val = builder.CreateXor(tmp1, sum_plus_key);
    Value *v1_sub = builder.CreateSub(v1_val, xor_val);
    builder.CreateStore(v1_sub, var_v1);

    // sum -= delta
    Value *sum_sub = builder.CreateSub(sum_val, xtea_info.delta);
    builder.CreateStore(sum_sub, var_sum);

    // Now for v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3])
    v1_val = builder.CreateLoad(u32_ty, var_v1, "v1_updated");
    Value *v1_shl4 = builder.CreateShl(v1_val, builder.getInt32(4));
    Value *v1_lshr5 = builder.CreateLShr(v1_val, builder.getInt32(5));
    Value *v1_xor = builder.CreateXor(v1_shl4, v1_lshr5);
    Value *tmp2 = builder.CreateAdd(v1_xor, v1_val);

    Value *sum_val2 = builder.CreateLoad(u32_ty, var_sum, "sum2");
    Value *key_index2 = builder.CreateAnd(sum_val2, builder.getInt32(3));

    Value *key_ptr2 =
        builder.CreateInBoundsGEP(u32_ty, xtea_key, key_index2, "key_ptr2");
    Value *key_val2 = builder.CreateLoad(u32_ty, key_ptr2, "key_val2");

    Value *sum_plus_key2 = builder.CreateAdd(sum_val2, key_val2);
    Value *xor_val2 = builder.CreateXor(tmp2, sum_plus_key2);

    v0_val = builder.CreateLoad(u32_ty, var_v0, "v0_updated");
    Value *v0_sub = builder.CreateSub(v0_val, xor_val2);
    builder.CreateStore(v0_sub, var_v0);

    // i++
    Value *i_inc = builder.CreateAdd(i_val, builder.getInt32(1));
    builder.CreateStore(i_inc, var_i);

    // Jump back to loop_cond
    builder.CreateBr(loop_cond);

    // Loop end block
    builder.SetInsertPoint(loop_end);

    // Store v0 and v1 back to v[0] and v[1]
    v0_val = builder.CreateLoad(u32_ty, var_v0);
    v1_val = builder.CreateLoad(u32_ty, var_v1);
    builder.CreateStore(v0_val, v0_ptr);
    builder.CreateStore(v1_val, v1_ptr);

    builder.CreateBr(split_bb);
    builder.SetInsertPoint(&split_bb->front());
}
