#ifndef BASIC_BLOCK_UTIL_H
#define BASIC_BLOCK_UTIL_H

#include <any>
#include <llvm/IR/Value.h>

using namespace llvm;

class BasicBlockUtils
{
  public:
    static void FlattenSwitch(BasicBlock *bb);

    static void AddMetaData(BasicBlock *bb, std::string key, std::any value);

    static std::optional<std::any> GetMetaData(BasicBlock *bb, std::string key);

    static void RemoveMetaData(BasicBlock *bb);
};

#endif // BASIC_BLOCK_UTIL_H
