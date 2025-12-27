#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <llvm/IR/IRBuilder.h>
#include <map>

using namespace llvm;

class CryptoUtils
{

  public:
    struct XteaInfo
    {
        Value *xtea_key;
        Value *num_rounds;
        Value *delta;
    };

    struct XteaOptions
    {
        int use_global_variable_for_key_chance;
        int use_global_variable_for_delta_chance;
    };

    struct ZyroxTableEntryInfo
    {
        uint32_t xtea_key[4];
        uint32_t delta;
        int nb_rounds;
    };

    struct ZyroxTable
    {
        int table_id;

        std::vector<ZyroxTableEntryInfo> entries;
    };

    static void AddZyroxTable(ZyroxTable &zyrox_table);

    static int GetUniqueZyroxTableId();

    static void FinalizeZyroxTables();

    static void WriteXTEADecipher(IRBuilderBase &builder, XteaInfo &xtea_info,
                                  XteaOptions &, Value *value,
                                  AllocaInst *var_v0, AllocaInst *var_v1,
                                  AllocaInst *var_sum, AllocaInst *var_i);

  private:
    static std::map<int, ZyroxTable> zyrox_tables;

    static int zyrox_table_counter;
};

#endif // CRYPTO_UTILS_H
