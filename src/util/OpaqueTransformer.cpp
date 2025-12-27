#include <random>
#include <utils/OpaqueTransformer.h>
#include <utils/Random.h>

uint64_t OpaqueTransformer::RandomInt()
{
    static std::mt19937 gen(std::random_device{}());
    return gen();
}

Value *OpaqueTransformer::ROTL(IRBuilderBase &builder, Value *val,
                               unsigned shift)
{
    shift %= 32;
    Value *lsh = builder.CreateShl(val, shift);
    Value *rsh = builder.CreateLShr(val, 32 - shift);
    return builder.CreateOr(lsh, rsh);
}

Value *OpaqueTransformer::ROTR(IRBuilderBase &builder, Value *val,
                               unsigned shift)
{
    shift %= 32;
    Value *rsh = builder.CreateLShr(val, shift);
    Value *lsh = builder.CreateShl(val, 32 - shift);
    return builder.CreateOr(rsh, lsh);
}

OpaqueTransformer::OpaqueTransformer(bool is_arm32) : m_isArm32(is_arm32)
{
    int num_steps = Random::IntRanged(2, 6);
    for (int i = 0; i < num_steps; i++)
    {
        OpType t = static_cast<OpType>(Random::IntRanged(0, 4));
        uint64_t c = Random::IntRanged<uint64_t>(
            0x000F0000, m_isArm32 ? UINT32_MAX : UINT64_MAX);
        if (t == OpType::ROL || t == OpType::ROR)
        {
            c %= 31;
            c += 1;
        }
        m_Ops.push_back(t);
        m_Constants.push_back(c);
    }
}

uint64_t OpaqueTransformer::TransformConstant(uint64_t input)
{
    uint64_t current = input;
    for (size_t i = 0; i < m_Ops.size(); i++)
    {
        uint64_t c = m_Constants[i];
        switch (m_Ops[i])
        {
        case OpType::XOR:
            current ^= c;
            break;
        case OpType::ADD:
            current += c;
            break;
        case OpType::SUB:
            current -= c;
            break;
        case OpType::ROL:
            if (m_isArm32)
            {
                current &= 0xFFFFFFFF;
                c %= 32;
                current = (current << c) | (current >> (32 - c));
                current &= 0xFFFFFFFF;
            }
            else
            {
                c %= 64;
                current = (current << c) | (current >> (64 - c));
            }
            break;
        case OpType::ROR:
            if (m_isArm32)
            {
                current &= 0xFFFFFFFF;
                c %= 32;
                current = (current >> c) | (current << (32 - c));
                current &= 0xFFFFFFFF;
            }
            else
            {
                c %= 64;
                current = (current >> c) | (current << (64 - c));
            }
            break;
        }
        if (m_isArm32)
        {
            current &= 0xFFFFFFFF;
        }
    }
    return current;
}

Value *GetConstantValue(Module &m, IRBuilderBase &builder, uint64_t c,
                        int use_global_variables_chance, bool is_arm32)
{
    if (Random::Chance(use_global_variables_chance))
    {
        GlobalVariable *gv = new GlobalVariable(
            m, is_arm32 ? builder.getInt32Ty() : builder.getInt64Ty(), false,
            GlobalValue::PrivateLinkage,
            is_arm32 ? builder.getInt32(c) : builder.getInt64(c),
            "__state_var_" + std::to_string(c));

        return builder.CreateLoad(
            is_arm32 ? builder.getInt32Ty() : builder.getInt64Ty(), gv);
    }
    return is_arm32 ? builder.getInt32(c) : builder.getInt64(c);
}

Value *OpaqueTransformer::Transform(Module &m, IRBuilderBase &builder,
                                    Value *input,
                                    int use_global_variables_chance)
{
    Value *current = input;
    std::string base_name = "obf";

    IntegerType *target_type =
        m_isArm32 ? builder.getInt32Ty() : builder.getInt64Ty();

    // Convert input to target type if needed
    if (current->getType() != target_type)
    {
        if (m_isArm32)
        {
            current = builder.CreateTrunc(current, target_type, "trunc_input");
        }
        else
        {
            current =
                builder.CreateZExtOrTrunc(current, target_type, "ext_input");
        }
    }

    for (size_t i = 0; i < m_Ops.size(); i++)
    {
        OpType op = m_Ops[i];
        uint64_t c = m_Constants[i];

#define GET_CONSTANT(c)                                                        \
    GetConstantValue(m, builder, c, use_global_variables_chance, m_isArm32)

        switch (op)
        {
        case OpType::XOR:
            current = builder.CreateXor(current, c);
            break;
        case OpType::ADD:
            current = builder.CreateAdd(current, GET_CONSTANT(c));
            break;
        case OpType::SUB:
            current = builder.CreateSub(current, GET_CONSTANT(c));
            break;
        case OpType::ROL:
        {
            unsigned bit_width = m_isArm32 ? 32 : 64;
            c %= bit_width;
            Value *shift_const = GET_CONSTANT(c);
            Value *shifted_left = builder.CreateShl(current, shift_const);

            // For 32-bit: use 32-c, for 64-bit: use 64-c
            Value *inverse_shift = GET_CONSTANT(bit_width - c);
            Value *shifted_right = builder.CreateLShr(current, inverse_shift);

            current = builder.CreateOr(shifted_left, shifted_right);
            break;
        }
        case OpType::ROR:
        {
            unsigned bit_width = m_isArm32 ? 32 : 64;
            c %= bit_width;
            Value *shift_const = GET_CONSTANT(c);
            Value *shifted_right = builder.CreateLShr(current, shift_const);

            // For 32-bit: use 32-c, for 64-bit: use 64-c
            Value *inverse_shift = GET_CONSTANT(bit_width - c);
            Value *shifted_left = builder.CreateShl(current, inverse_shift);

            current = builder.CreateOr(shifted_right, shifted_left);
            break;
        }
        }

#undef GET_CONSTANT
    }
    if (current->getType() != target_type)
    {
        if (m_isArm32)
        {
            current = builder.CreateTrunc(current, target_type, "trunc_result");
        }
        else
        {
            current = builder.CreateZExt(current, target_type, "ext_result");
        }
    }

    return current;
}