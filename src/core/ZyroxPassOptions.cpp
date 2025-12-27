#include <Passes/BasicBlockSplitter.h>
#include <Passes/ControlFlowFlattening.h>
#include <Passes/IndirectBranch.h>
#include <Passes/MBASub.hpp>
#include <Passes/SimpleIndirectBranch.h>
#include <core/ZyroxPassOptions.h>

std::vector<ZyroxFunctionPass> zyrox_passes = {
    ControlFlowFlattening::pass_info, BasicBlockSplitter::pass_info,
    IndirectBranch::pass_info,        MBASub::pass_info,
    SimpleIndirectBranch::pass_info,
};

ZyroxAnnotationArgs::ZyroxAnnotationArgs(std::vector<int> args)
    : m_Args(args), m_Index(0)
{
}

int ZyroxAnnotationArgs::Next()
{
    if (m_Index < m_Args.size())
        return m_Args[m_Index++];
    return 0;
}

int ZyroxAnnotationArgs::NextOrDefault(int default_value)
{
    if (m_Index < m_Args.size())
        return m_Args[m_Index++];
    return default_value;
}
