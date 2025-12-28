#ifndef MBASUB_H
#define MBASUB_H

#include <core/ZyroxPassOptions.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstrTypes.h>

using namespace llvm;

#define DEFINE_FN(Op)                                                          \
    static void runOn##Op(BasicBlock &BB);                                     \
    static Value *Obfuscate##Op(IRBuilder<> &Builder, BinaryOperator *BinOp);

class MBASub
{
  public:
    DEFINE_FN(Sub)
    DEFINE_FN(Add)
    DEFINE_FN(Xor)
    DEFINE_FN(Mul)
    DEFINE_FN(Or)

    static void RunOnBasicBlock(BasicBlock &);

    static void RunOnFunction(Function &, ZyroxPassOptions *options);

    static void RegisterFromAnnotation(Function &f, ZyroxAnnotationArgs *args);

    inline static ZyroxFunctionPass pass_info = {
        .RunOnFunction = RunOnFunction,
        .RegisterFromAnnotation = RegisterFromAnnotation,
        .Name = "MixedBooleanArithmetic",
        .CodeName = "mba",
    };

  private:
    static void ObfuscateFunction(Function &);
};

#undef DEFINE_FN

#endif // MBASUB_H