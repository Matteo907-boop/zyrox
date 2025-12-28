#ifndef BASIC_BLOCK_SPLITTER_H
#define BASIC_BLOCK_SPLITTER_H

#include <core/ZyroxMetaData.h>
#include <core/ZyroxPassOptions.h>

#include <llvm/IR/Function.h>

using namespace llvm;

class BasicBlockSplitter
{
  public:
    static void RunOnFunction(Function &f, ZyroxPassOptions *options);

    static void RegisterFromAnnotation(Function &f, ZyroxAnnotationArgs *args);

    inline static ZyroxFunctionPass pass_info = {
        .RunOnFunction = RunOnFunction,
        .RegisterFromAnnotation = RegisterFromAnnotation,
        .Name = "BasicBlockSplitter",
        .CodeName = "bbs",
    };

  private:
    static void ObfuscateFunction(Function &f, int min_block_size,
                                  int max_block_size, int block_split_chance);
};

#endif