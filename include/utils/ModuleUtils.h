#ifndef MODULE_UTIL_H
#define MODULE_UTIL_H

#include <llvm/IR/Module.h>

using namespace llvm;

class ModuleUtils
{

  public:
    static void ShuffleGlobals(Module &m);

    static void ShuffleFunctions(Module &m);

    static void ExpandCustomAnnotations(Module &m);

    static std::unique_ptr<Module> LoadFromIR(LLVMContext &ctx, const char *ir);

    static void LinkModules(Module &dst, std::unique_ptr<Module> src);

    static void AddMetaData(const char *meta_data);

    static void Finalize(Module &m);
};

#endif // MODULE_UTIL_H
