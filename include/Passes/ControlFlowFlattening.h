#ifndef CONTROL_FLOW_FLATTENING_H
#define CONTROL_FLOW_FLATTENING_H

#include <core/ZyroxPassOptions.h>
#include <llvm/IR/Function.h>

using namespace llvm;

class ControlFlowFlattening
{
  public:
    struct TransformationOptions
    {
        int UseFunctionResolverChance;
        int UseGlobalStateVariablesChance;
        int UseOpaqueTransformationChance;
        int UseGlobalVariableOpaquesChance;
        int UseSipHashedStateChance;
        int CloneSipHashChance;
    };

    static void RunOnFunction(Function &f, ZyroxPassOptions *options);

    static void RegisterFromAnnotation(Function &f, ZyroxAnnotationArgs *args);

    inline static ZyroxFunctionPass pass_info = {
        .RunOnFunction = RunOnFunction,
        .RegisterFromAnnotation = RegisterFromAnnotation,
        .Name = "ControlFlowFlattening",
        .CodeName = "cff",
    };

  private:
    static void ObfuscateFunction(Function &f, TransformationOptions *options);
};

#endif // CONTROL_FLOW_FLATTENING_H
