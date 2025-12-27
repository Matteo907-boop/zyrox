#include <Passes/IndirectBranch.h>
#include <Passes/MBASub.hpp>
#include <core/ZyroxMetaData.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <map>
#include <numeric>
#include <utils/BasicBlockUtils.h>
#include <utils/CryptoUtils.h>
#include <utils/FunctionUtils.h>
#include <utils/Logger.h>
#include <utils/Random.h>

#define DEBUG_IBR 0
#define DEBUG_ANDROID 0

#if DEBUG_IBR
#if DEBUG_ANDROID
#define DLOG(fmt, ...)                                                         \
    builder.CreateCall(log_func,                                               \
                       {builder.getInt32(7), log_tag,                          \
                        builder.CreateGlobalStringPtr(fmt), ##__VA_ARGS__});
#else
#define DLOG(fmt, ...)                                                         \
    builder.CreateCall(printf_func,                                            \
                       {builder.CreateGlobalStringPtr(fmt), ##__VA_ARGS__});
#endif
#else
#define DLOG(fmt, ...)                                                         \
    do                                                                         \
    {                                                                          \
    } while (0)
#endif

template <typename K, typename V>
int IndexOf(const std::map<K, V> &m, const K &key)
{
    int index = 0;
    for (auto it = m.begin(); it != m.end(); ++it, ++index)
    {
        if (it->first == key)
            return index;
    }
    return -1;
}

void IndirectBranch::RunOnFunction(Function &f, ZyroxPassOptions *options)
{
    int iterations_count = options->Get("PassIterations");
    int replace_br_chance = options->Get("IndirectBranch.Chance");
    if (replace_br_chance == 0)
        replace_br_chance = 50;

    Logger::Info("indirect branch chance: {}", replace_br_chance);

    for (int i = 0; i < iterations_count; i++)
    {
        ObfuscateFunction(f, replace_br_chance);
    }
}

void IndirectBranch::RegisterFromAnnotation(Function &f,
                                            ZyroxAnnotationArgs *args)
{
    ZyroxMetaDataKV kv = {
        {"PassIterations", args->NextOrDefault(1)},
        {"IndirectBranch.Chance", args->NextOrDefault(40)},
    };
    ZyroxPassesMetadata::AddPass(f, pass_info.CodeName, kv);
}

int CountBasicBlockUses(BasicBlock *bb)
{
    int count = 0;
    for (User *user : bb->users())
    {
        if (isa<Instruction>(user))
        {
            ++count;
        }
    }
    return count;
}

void IndirectBranch::ObfuscateFunction(Function &f, int replace_br_chance)
{
    if (f.size() < 2)
        return;

    FunctionUtils::FlattenSwitches(f);

    int count = std::accumulate(
        f.begin(), f.end(), 0u,
        [](unsigned acc, const BasicBlock &bb)
        {
            return acc +
                   (bb.getTerminator()->getOpcode() == Instruction::Br ? 1 : 0);
        });

    if (count == 0)
    {
        Logger::Warn("ignoring IndirectBranch pass on {}: did not find any "
                     "Branch instruction",
                     demangle(f.getName()).data());
        return;
    }

    BasicBlock *entry = &f.getEntryBlock();
    IRBuilder builder(entry->getTerminator());

    LLVMContext &ctx = f.getContext();
    Module *module = f.getParent();

    Type *i8_ty = Type::getInt8Ty(ctx);

    const DataLayout &dl = module->getDataLayout();

    unsigned ptr_size = dl.getPointerSize(0);

    Type *pint_ty = builder.getIntPtrTy(dl);

#define IS_ARM32() (ptr_size == 4)

    PointerType *block_address_ty = PointerType::getUnqual(i8_ty);

#if DEBUG_IBR
#if DEBUG_ANDROID
    FunctionType *log_type = FunctionType::get(
        builder.getInt32Ty(),
        {builder.getInt32Ty(), PointerType::getUnqual(builder.getInt8Ty()),
         PointerType::getUnqual(builder.getInt8Ty())},
        true);
    Function *log_func = module->getFunction("__android_log_print");
    if (!log_func)
    {
        log_func = Function::Create(log_type, Function::ExternalLinkage,
                                    "__android_log_print", module);
    }
    Value *log_tag = builder.CreateGlobalStringPtr("Zyrox");
#else
    FunctionType *printf_type =
        FunctionType::get(builder.getInt32Ty(),
                          {PointerType::getUnqual(builder.getInt8Ty())}, true);
    Function *printf_func = module->getFunction("printf");
    if (!printf_func)
    {
        printf_func = Function::Create(printf_type, Function::ExternalLinkage,
                                       "printf", module);
    }
#endif
#endif

    std::vector<BranchInst *> branches;
    std::map<BasicBlock *, uint32_t> target_bbs;

    for (BasicBlock &bb : f)
    {
        if (BranchInst *branch = llvm::dyn_cast<BranchInst>(bb.getTerminator()))
        {
            if (Random::Chance(replace_br_chance))
            {
                branches.push_back(branch);
                for (BasicBlock *target_bb : branch->successors())
                {
                    if (!target_bbs.contains(target_bb))
                    {
                        target_bbs[target_bb] = 0;
                    }
                }
            }
        }
    }

    constexpr int array_marker = 3 + 1; // 3 magic integers, 1 for array id

    std::vector<Constant *> elems;
    // for arm32 we will pad extra 4 bytes for xtea encryption.
    elems.reserve(target_bbs.size() * (IS_ARM32() ? 2 : 1) + array_marker);
    for (int i = 0; i < array_marker - 1; i++)
    {
        elems.push_back(ConstantExpr::getIntToPtr(
            ConstantInt::get(pint_ty, 0xDEADBEEF), block_address_ty));
    }

    uint32_t seed = Random::UInt32();

    CryptoUtils::ZyroxTable zyrox_table = {};
    zyrox_table.table_id = CryptoUtils::GetUniqueZyroxTableId();

    elems.push_back(ConstantExpr::getIntToPtr(
        ConstantInt::get(pint_ty, zyrox_table.table_id), block_address_ty));

    bool is_thumb_mode = f.hasFnAttribute("target-features") &&
                         f.getFnAttribute("target-features")
                             .getValueAsString()
                             .contains("+thumb-mode");

    Random::SimpleRNG rng(seed);

    std::unordered_map<BasicBlock *, int> bb_index_map;

    for (auto &[bb, seed] : target_bbs)
    {
        elems.push_back(BlockAddress::get(bb));
        seed = rng.Next();

        Random::SimpleRNG bb_rng(seed);

        uint32_t bb_key[4] = {
            bb_rng.Next(),
            bb_rng.Next(),
            bb_rng.Next(),
            bb_rng.Next(),
        };

        uint32_t bb_delta = bb_rng.Next();

        uint32_t bb_rounds = (bb_rng.Next() % 3) + 1;

        if (auto block_state =
                BasicBlockUtils::GetMetaData(bb, "cff.block_state");
            block_state.has_value())
        {
            if (CountBasicBlockUses(bb) == 1)
            {
                Logger::Info("using indirect dispatcher state");
                bb_delta =
                    std::any_cast<uint64_t>(block_state.value()) & 0xFFFFFFFF;
            }
        }

        bb_index_map[bb] = zyrox_table.entries.size();
        zyrox_table.entries.push_back({
            .xtea_key = {bb_key[0], bb_key[1], bb_key[2], bb_key[3]},
            .delta = bb_delta,
            .nb_rounds = static_cast<int>(bb_rounds),
        });

        if (IS_ARM32())
        {
            elems.push_back(builder.getInt32(0)); // pad 4 bytes for xtea
        }
    }

    CryptoUtils::AddZyroxTable(zyrox_table);

    assert(elems.size() ==
           target_bbs.size() * (IS_ARM32() ? 2 : 1) + array_marker);

    ArrayType *bb_array_ty = ArrayType::get(block_address_ty, elems.size());
    Constant *initializer = ConstantArray::get(bb_array_ty, elems);
    GlobalVariable *bb_array = new GlobalVariable(
        *module, bb_array_ty, false, GlobalValue::InternalLinkage, initializer);
    bb_array->setSection(".data");

    Type *u64_ty = builder.getInt64Ty();

#define BLOCK_XTEA_INFO(bb) zyrox_table.entries[bb_index_map[bb]]

    Type *u32_ty = builder.getInt32Ty();

    builder.SetInsertPoint(f.getEntryBlock().getFirstInsertionPt());

    AllocaInst *var_v0 = builder.CreateAlloca(u32_ty, nullptr, "v0");
    AllocaInst *var_v1 = builder.CreateAlloca(u32_ty, nullptr, "v1");
    AllocaInst *var_sum = builder.CreateAlloca(u32_ty, nullptr, "sum");
    AllocaInst *var_i = builder.CreateAlloca(u32_ty, nullptr, "i");

    AllocaInst *temp_storage =
        builder.CreateAlloca(u64_ty, nullptr, "xtea_temp");

    for (BranchInst *branch : branches)
    {
        builder.SetInsertPoint(branch);

        Value *indices[2];
        Value *xtea_key, *xtea_rounds, *xtea_delta;

        Type *i32_ty = Type::getInt32Ty(ctx);
        ArrayType *array_ty = ArrayType::get(i32_ty, 4);

        if (branch->isConditional())
        {
            indices[0] = ConstantInt::get(pint_ty, 0);

            BasicBlock *bb_true = branch->getSuccessor(0);
            BasicBlock *bb_false = branch->getSuccessor(1);

            CryptoUtils::ZyroxTableEntryInfo bb_true_info =
                BLOCK_XTEA_INFO(bb_true);
            CryptoUtils::ZyroxTableEntryInfo bb_false_info =
                BLOCK_XTEA_INFO(bb_false);

            std::vector<Constant *> values_key_true(4);
            for (int i = 0; i < 4; i++)
                values_key_true[i] =
                    ConstantInt::get(i32_ty, bb_true_info.xtea_key[i]);
            GlobalVariable *g_key_true = new GlobalVariable(
                *module, array_ty, false, GlobalValue::PrivateLinkage,
                ConstantArray::get(array_ty, values_key_true),
                "key_array_true");

            std::vector<Constant *> values_key_false(4);
            for (int i = 0; i < 4; i++)
                values_key_false[i] =
                    ConstantInt::get(i32_ty, bb_false_info.xtea_key[i]);
            GlobalVariable *g_key_false = new GlobalVariable(
                *module, array_ty, false, GlobalValue::PrivateLinkage,
                ConstantArray::get(array_ty, values_key_false),
                "key_array_false");

            Value *cond = branch->getCondition();

            xtea_key = builder.CreateSelect(
                cond,
                builder.CreateInBoundsGEP(
                    array_ty, g_key_true,
                    {builder.getInt32(0), builder.getInt32(0)}, "key_true_ptr"),
                builder.CreateInBoundsGEP(
                    array_ty, g_key_false,
                    {builder.getInt32(0), builder.getInt32(0)},
                    "key_false_ptr"));

            Value *bb_true_delta, *bb_false_delta;

            if (auto cff_state = BasicBlockUtils::GetMetaData(
                    bb_true, "cff.dispatcher_state");
                cff_state.has_value() && CountBasicBlockUses(bb_true) == 1)
            {
                Value *load = builder.CreateLoad(
                    IS_ARM32() ? builder.getInt32Ty() : builder.getInt64Ty(),
                    std::any_cast<AllocaInst *>(cff_state.value()), true);
                bb_true_delta =
                    IS_ARM32()
                        ? load
                        : builder.CreateTrunc(load, builder.getInt32Ty());
            }
            else
            {
                bb_true_delta = builder.getInt32(bb_true_info.delta);
            }

            if (auto cff_state = BasicBlockUtils::GetMetaData(
                    bb_false, "cff.dispatcher_state");
                cff_state.has_value() && CountBasicBlockUses(bb_false) == 1)
            {
                Value *load = builder.CreateLoad(
                    IS_ARM32() ? builder.getInt32Ty() : builder.getInt64Ty(),
                    std::any_cast<AllocaInst *>(cff_state.value()), true);
                bb_false_delta =
                    IS_ARM32()
                        ? load
                        : builder.CreateTrunc(load, builder.getInt32Ty());
            }
            else
            {
                bb_false_delta = builder.getInt32(bb_false_info.delta);
            }

            xtea_delta =
                builder.CreateSelect(cond, bb_true_delta, bb_false_delta);
            xtea_rounds = builder.CreateSelect(
                cond, builder.getInt32(bb_true_info.nb_rounds),
                builder.getInt32(bb_false_info.nb_rounds));

            indices[1] = builder.CreateSelect(
                cond,
                ConstantInt::get(pint_ty,
                                 array_marker + IndexOf(target_bbs, bb_true) *
                                                    (IS_ARM32() ? 2 : 1)),
                ConstantInt::get(pint_ty,
                                 array_marker + IndexOf(target_bbs, bb_false) *
                                                    (IS_ARM32() ? 2 : 1)));
        }
        else
        {
            BasicBlock *bb = branch->getSuccessor(0);

            CryptoUtils::ZyroxTableEntryInfo bb_info = BLOCK_XTEA_INFO(bb);

            std::vector<Constant *> values(4);
            for (int i = 0; i < 4; i++)
                values[i] = ConstantInt::get(i32_ty, bb_info.xtea_key[i]);
            GlobalVariable *g_key = new GlobalVariable(
                *module, array_ty, false, GlobalValue::PrivateLinkage,
                ConstantArray::get(array_ty, values), "bb.xtea_key");

            if (auto cff_state =
                    BasicBlockUtils::GetMetaData(bb, "cff.dispatcher_state");
                cff_state.has_value() && CountBasicBlockUses(bb) == 1)
            {
                Value *load = builder.CreateLoad(
                    IS_ARM32() ? builder.getInt32Ty() : builder.getInt64Ty(),
                    std::any_cast<AllocaInst *>(cff_state.value()), true);
                xtea_delta =
                    IS_ARM32()
                        ? load
                        : builder.CreateTrunc(load, builder.getInt32Ty());
            }
            else
            {
                xtea_delta = builder.getInt32(bb_info.delta);
            }

            xtea_rounds = builder.getInt32(bb_info.nb_rounds);

            xtea_key = builder.CreateInBoundsGEP(
                array_ty, g_key, {builder.getInt32(0), builder.getInt32(0)},
                "key_ptr");

            indices[0] = ConstantInt::get(pint_ty, 0);
            indices[1] = ConstantInt::get(
                pint_ty,
                array_marker + IndexOf(target_bbs, branch->getSuccessor(0)) *
                                   (IS_ARM32() ? 2 : 1));
        }

        Value *seed_ptr = builder.CreateInBoundsGEP(
            bb_array->getValueType(), bb_array,
            {builder.getInt32(0), builder.getInt32(array_marker - 1)},
            "seed_ptr"); // seed will host base after our python plugin run

        Value *base_ptr = builder.CreateLoad(block_address_ty, seed_ptr, true,
                                             "seed_ptr_val");

        Value *base_int = builder.CreatePtrToInt(base_ptr, pint_ty, "seed_val");

        DLOG("[DEBUG] Accessing array[%ld] (base=%p)\n", indices[1], bb_array);

        Value *encrypted_value;

        if (IS_ARM32())
        {
            Value *gep =
                builder.CreateInBoundsGEP(bb_array_ty, bb_array, indices);

            // Cast the pointer to a 32-bit integer pointer
            Value *i32_ptr = builder.CreateBitCast(
                gep, builder.getInt32Ty()->getPointerTo());

            // Load the lower 32 bits
            Value *low32 = builder.CreateLoad(builder.getInt32Ty(), i32_ptr,
                                              true, "low_part");

            // Get a pointer to the upper 32 bits and load them
            Value *high_ptr =
                builder.CreateGEP(builder.getInt32Ty(), i32_ptr,
                                  builder.getInt32(1), "high_part_ptr");
            Value *high32 = builder.CreateLoad(builder.getInt32Ty(), high_ptr,
                                               true, "high_part");

            // Combine into a 64-bit value
            Value *high64 = builder.CreateZExt(high32, builder.getInt64Ty());
            Value *low64 = builder.CreateZExt(low32, builder.getInt64Ty());
            Value *shifted_high = builder.CreateShl(high64, 32);
            encrypted_value =
                builder.CreateOr(shifted_high, low64, "encrypted_offset_arm32");
        }
        else
        {
            Value *gep = builder.CreateGEP(bb_array_ty, bb_array, indices);

            Value *casted_gep =
                builder.CreateBitCast(gep, PointerType::getUnqual(pint_ty));

            encrypted_value = builder.CreateLoad(pint_ty, casted_gep, true,
                                                 "encrypted_offset");
        }

#if DEBUG_IBR
        Value *gep = builder.CreateGEP(bb_array_ty, bb_array, indices);

        Value *casted_gep =
            builder.CreateBitCast(gep, PointerType::getUnqual(pint_ty));

        Value *casted_gep_int = builder.CreatePtrToInt(casted_gep, pint_ty);
        Value *base_ptr_int = builder.CreatePtrToInt(base_ptr, pint_ty);
        Value *offset_from_base =
            builder.CreateSub(casted_gep_int, base_ptr_int);

        DLOG("encrypted value: 0x%llx at 0x%lx\n", encrypted_value,
             offset_from_base);

#endif

        builder.CreateStore(encrypted_value, temp_storage, true);

        Value *casted = builder.CreateBitCast(
            temp_storage, PointerType::getUnqual(builder.getInt32Ty()));

        CryptoUtils::XteaInfo xtea_info = {
            .xtea_key = xtea_key,
            .num_rounds = xtea_rounds,
            .delta = xtea_delta,
        };
        CryptoUtils::XteaOptions xtea_options = {};

#if DEBUG_IBR
        for (int ki = 0; ki < 4; ++ki)
        {
            Value *idx = builder.getInt32(ki);
            Value *key_ptr_i = builder.CreateInBoundsGEP(
                array_ty, xtea_key, {builder.getInt32(0), idx}, "key_ptr_i");
            Value *key_val_i = builder.CreateLoad(builder.getInt32Ty(),
                                                  key_ptr_i, true, "key_val_i");

            Value *ki_val = builder.getInt32(ki);
            DLOG("[DEBUG] XTEA key[%d] = 0x%08x\n", ki_val, key_val_i);
        }

        DLOG("[DEBUG] XTEA delta = 0x%08x\n", xtea_delta);

        DLOG("[DEBUG] XTEA rounds = %d\n", xtea_rounds);
#endif

        CryptoUtils::WriteXTEADecipher(builder, xtea_info, xtea_options, casted,
                                       var_v0, var_v1, var_sum, var_i);

        Value *decrypted_offset =
            builder.CreateLoad(u64_ty, temp_storage, true, "decrypted_offset");

        if (IS_ARM32())
        {
            Value *low32 = builder.CreateTrunc(decrypted_offset,
                                               builder.getInt32Ty(), "low32");
            decrypted_offset = builder.CreateZExt(low32, pint_ty);
        }

        DLOG("[DEBUG] Decrypted offset: 0x%lx\n", decrypted_offset);

        DLOG("[DEBUG] Runtime base: %p\n", base_ptr);

        Value *final_addr_int =
            builder.CreateAdd(base_int, decrypted_offset, "final_addr_int");

        if (is_thumb_mode)
        {
            final_addr_int =
                builder.CreateOr(final_addr_int, ConstantInt::get(pint_ty, 1),
                                 "thumb_adjusted_addr");
        }

        Value *final_ptr = builder.CreateIntToPtr(
            final_addr_int, PointerType::getUnqual(builder.getInt8Ty()),
            "final_ptr");

        DLOG("[DEBUG] Final target: %p\n", final_ptr);

        IndirectBrInst *indir_branch =
            builder.CreateIndirectBr(final_ptr, branch->getNumSuccessors());

        for (BasicBlock *bb : branch->successors())
        {
            indir_branch->addDestination(bb);
        }

        branch->replaceAllUsesWith(indir_branch);
        branch->eraseFromParent();
    }
}