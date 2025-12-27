#ifndef FUNCTION_UTIL_H
#define FUNCTION_UTIL_H
#include <llvm/IR/Function.h>

using namespace llvm;

class FunctionUtils
{

  public:
    static bool HasCXXExceptions(Function &f);

    static void ShuffleBlocks(Function &f);

    static void DemotePHIToStack(Function &f);

    static void DemoteRegToStack(Function &f);

    static void FlattenSwitches(Function &f);

    static void EnsureAllocasInEntryBlocks(Function &f);
};

#endif // FUNCTION_UTIL_H
