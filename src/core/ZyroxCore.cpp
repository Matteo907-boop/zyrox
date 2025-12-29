#include <core/ZyroxCore.h>
#include <core/ZyroxMetaData.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>
#include <utils/FunctionUtils.h>
#include <utils/Logger.h>

void DebugRun(std::string &function_name, ZyroxPassOptions *pass_options);

void Zyrox::RunOnFunction(Function &f)
{
    if (f.isDeclaration() || !f.hasMetadata("zyrox"))
        return;

    if (ZyroxPassesMetadata::IsObfuscated(f))
        return;

    ZyroxPassesMetadata::MarkObfuscated(f);

    if (FunctionUtils::HasCXXExceptions(f))
    {
        Logger::Warn("skipping {} because it have cxx exceptions which "
                     "are not supported entirely yet.",
                     f.getName().data());
        return;
    }
    Logger::Info("Running passes on {}", demangle(f.getName()));
    std::string function_name = demangle(f.getName());

    FunctionUtils::FlattenSwitches(f);
    FunctionUtils::DemotePHIToStack(f);

    for (ZyroxPassOptions pass_options : ZyroxPassesMetadata::PassesOf(f))
    {
        DebugRun(function_name, &pass_options);
        pass_options.RunPass(f);
        if (verifyFunction(f, &errs()))
        {
            f.print(errs());
            Logger::Error("Function verification failed after running {} on {}",
                          pass_options.GetPass().Name, function_name);
        }
    }
}

void DebugRun(std::string &function_name, ZyroxPassOptions *pass_options)
{
    int iterations_count = pass_options->Get("PassIterations");
    Logger::Info("Running {} on {} {} {}", pass_options->GetPass().Name,
                 function_name, iterations_count,
                 iterations_count > 1 ? "times" : "time");
}