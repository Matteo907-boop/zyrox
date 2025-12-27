#ifndef ZYROX_PASS_OPTIONS_H
#define ZYROX_PASS_OPTIONS_H

#include <llvm/IR/Function.h>
#include <vector>

using namespace llvm;

class ZyroxPassOptions;

class ZyroxAnnotationArgs
{
    std::vector<int> m_Args;
    int m_Index;

  public:
    explicit ZyroxAnnotationArgs(std::vector<int> args);
    int Next();
    int NextOrDefault(int default_value);
};

typedef struct
{
    std::function<void(Function &f, ZyroxPassOptions *options)> RunOnFunction;
    std::function<void(Function &f, ZyroxAnnotationArgs *args)>
        RegisterFromAnnotation;
    const char *Name;
    const char *CodeName;
} ZyroxFunctionPass;

extern std::vector<ZyroxFunctionPass> zyrox_passes;

#endif // ZYROX_PASS_OPTIONS_H
