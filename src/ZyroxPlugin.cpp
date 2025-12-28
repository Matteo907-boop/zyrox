#include <ZyroxPlugin.h>
#include <atomic>
#include <core/ZyroxCore.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <passes/StringEncryption.h>
#include <quickjs/QuickConfig.h>
#include <quickjs/QuickRt.h>
#include <utils/CryptoUtils.h>
#include <utils/HashUtils.h>
#include <utils/Logger.h>
#include <utils/ModuleUtils.h>

using namespace llvm;

std::atomic state{false};

PreservedAnalyses ZyroxPlugin::run(Module &m, ModuleAnalysisManager &)
{
    QuickRt::InitZyroxRuntime();

    m.getContext().setDiscardValueNames(false);
    ModuleUtils::LinkModules(
        m, ModuleUtils::LoadFromIR(m.getContext(), HashUtils::SipHashLlvmIR()));
    m.getContext().setDiscardValueNames(true);

    StripDebugInfo(m);

    StringEncryption::ObfuscateGlobalArrayStrings(m);

    ModuleUtils::ExpandCustomAnnotations(m);
    QuickConfig::RegisterPasses(m);

    auto &func_list = m.getFunctionList();
    auto it = func_list.begin();
    while (it != func_list.end())
    {
        Zyrox::RunOnFunction(*it);
        ++it;
    }

    ModuleUtils::Finalize(m);

    QuickRt::DestroyInstance();
    CryptoUtils::FinalizeZyroxTables();

    Logger::Info("Zyrox: finish.");

    return PreservedAnalyses::none();
}

PassPluginLibraryInfo GetZyroxPluginPluginInfo()
{
    return {LLVM_PLUGIN_API_VERSION, "ZyroxPlugin", LLVM_VERSION_STRING,
            [](PassBuilder &pb)
            {
                // clang comptime pass
                pb.registerPipelineEarlySimplificationEPCallback(
                    [&](ModulePassManager &mpm, OptimizationLevel)
                    {
                        if (state.load())
                            return true;

                        state.store(true);

                        mpm.addPass(ZyroxPlugin());

                        return true;
                    });
                // lto pass
                pb.registerFullLinkTimeOptimizationEarlyEPCallback(
                    [&](ModulePassManager &mpm, OptimizationLevel)
                    {
                        if (state.load())
                            return true;

                        state.store(true);

                        mpm.addPass(ZyroxPlugin());

                        return true;
                    });
            }};
}

extern "C" LLVM_ATTRIBUTE_WEAK PassPluginLibraryInfo llvmGetPassPluginInfo()
{
    return GetZyroxPluginPluginInfo();
}