#ifndef INDIRECT_BRANCH_H
#define INDIRECT_BRANCH_H

#include <core/ZyroxPassOptions.h>
#include <llvm/IR/Function.h>

using namespace llvm;

class IndirectBranch
{
  public:
    static void RunOnFunction(Function &f, ZyroxPassOptions *options);

    static void RegisterFromAnnotation(Function &f, ZyroxAnnotationArgs *args);

    inline static ZyroxFunctionPass pass_info = {
        .RunOnFunction = RunOnFunction,
        .RegisterFromAnnotation = RegisterFromAnnotation,
        .Name = "IndirectBranch",
        .CodeName = "ibr",
    };

  private:
    static void ObfuscateFunction(Function &f, int replace_br_chance);
};

#endif // INDIRECT_BRANCH_H
