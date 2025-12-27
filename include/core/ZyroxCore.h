#ifndef ZYROX_CORE_H
#define ZYROX_CORE_H

#include <llvm/IR/GlobalVariable.h>

using namespace llvm;

class Zyrox
{
  public:
    static void RunOnFunction(Function &f);

}; // namespace Zyrox

#endif // ZYROX_CORE_H
