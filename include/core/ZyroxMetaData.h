#ifndef ZYROX_METADATA_H
#define ZYROX_METADATA_H

#include <core/ZyroxPassOptions.h>
#include <llvm/IR/Function.h>
#include <string>

using namespace llvm;

typedef std::vector<std::pair<std::string, uint64_t>> ZyroxMetaDataKV;

class ZyroxPassOptions
{
    MDNode *m_Node;

  public:
    explicit ZyroxPassOptions(MDNode *n) : m_Node(n) {}

    ZyroxFunctionPass GetPass();

    int Get(StringRef key);

    void RunPass(Function &f);
};

class ZyroxPassesMetadata
{
  public:
    static void AddPass(Function &f, StringRef kind, ZyroxMetaDataKV &key_vals);

    static void RemovePass(Function &f, const std::string &pass_name);

    static std::vector<ZyroxPassOptions> PassesOf(Function &f);

    static void Put(Instruction *i, StringRef kind_name, StringRef value);

    static StringRef Get(Instruction *i, StringRef kind_name);

    static void MarkObfuscated(Function &f);

    static bool IsObfuscated(Function &f);
};

#endif // ZYROX_METADATA_H
