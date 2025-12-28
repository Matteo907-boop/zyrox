#ifndef STRING_ENCRYPTION_H
#define STRING_ENCRYPTION_H

#include <core/ZyroxPassOptions.h>
#include <llvm/IR/Function.h>

using namespace llvm;

class StringEncryption
{
  public:
    static void ObfuscateGlobalArrayStrings(Module &m);

    // private:
};

#endif // STRING_ENCRYPTION_H
