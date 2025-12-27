#ifndef ZYROX_PLUGIN_H
#define ZYROX_PLUGIN_H

#include <llvm/IR/PassManager.h>

using namespace llvm;

class ZyroxPlugin : public PassInfoMixin<ZyroxPlugin>
{
  public:
    PreservedAnalyses run(Module &m, ModuleAnalysisManager &mam);

    static bool isRequired() { return true; }
};

#endif