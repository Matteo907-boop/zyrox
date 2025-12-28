#ifndef SIMPLE_INDIRECT_BRANCH_H
#define SIMPLE_INDIRECT_BRANCH_H

#include <core/ZyroxPassOptions.h>
#include <llvm/IR/Function.h>

using namespace llvm;

class SimpleIndirectBranch
{
  public:
    static void RunOnFunction(Function &f, ZyroxPassOptions *options);

    static void RegisterFromAnnotation(Function &f, ZyroxAnnotationArgs *args);

    inline static ZyroxFunctionPass pass_info = {
        .RunOnFunction = RunOnFunction,
        .RegisterFromAnnotation = RegisterFromAnnotation,
        .Name = "SimpleIndirectBranch",
        .CodeName = "sibr",
    };

  private:
    static void ObfuscateFunction(Function &f, int replace_br_chance);
};

#endif // SIMPLE_INDIRECT_BRANCH_H
