#ifndef OPAQUE_TRANSFORMER_H
#define OPAQUE_TRANSFORMER_H

#include <cstdint>
#include <llvm/IR/IRBuilder.h>
#include <vector>

using namespace llvm;

enum class OpType
{
    XOR,
    ADD,
    SUB,
    ROL,
    ROR
};

class OpaqueTransformer
{
    std::vector<OpType> m_Ops;
    std::vector<uint64_t> m_Constants;
    bool m_isArm32;

    uint64_t RandomInt();

    Value *ROTL(IRBuilderBase &builder, Value *val, unsigned shift);

    Value *ROTR(IRBuilderBase &builder, Value *val, unsigned shift);

  public:
    explicit OpaqueTransformer(bool is_arm32);

    uint64_t TransformConstant(uint64_t input);

    Value *Transform(Module &m, IRBuilderBase &builder, Value *input,
                     int use_global_variables_chance);
};

#endif // OPAQUE_TRANSFORMER_H
