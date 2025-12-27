/**
 * @implements {ZyroxPlugin}
 */
class ZyroxPluginImpl {
    RunOnFunction(Name) {
        z.RegisterPass(ObfuscationType.BasicBlockSplitter, {
            PassIterations: 1,
            "BasicBlockSplitter.SplitBlockChance": 100,
            "BasicBlockSplitter.SplitBlockMinSize": 2,
            "BasicBlockSplitter.SplitBlockMaxSize": 5,
        });
        if (Name.indexOf("JNI_OnLoad") === -1 && Name.indexOf("__test_impl") === -1) {
            z.RegisterPass(ObfuscationType.SimpleIndirectBranch, {
                PassIterations: 1,
                "SimpleIndirectBranch.Chance": 100,
            });
            return;
        }

        z.RegisterPass(ObfuscationType.MixedBooleanArithmetic, {
            PassIterations: 1,
        });

        z.RegisterPass(ObfuscationType.BasicBlockSplitter, {
            PassIterations: 1,
            "BasicBlockSplitter.SplitBlockChance": 60,
            "BasicBlockSplitter.SplitBlockMinSize": 5,
            // after using MBA we have enough amount of instructions to generate more blocks for CFF, hehehe
            "BasicBlockSplitter.SplitBlockMaxSize": 10,
        });
        z.RegisterPass(ObfuscationType.ControlFlowFlattening, {
            PassIterations: 2,
            "ControlFlowFlattening.UseFunctionResolverChance": 60,
            "ControlFlowFlattening.UseGlobalStateVariablesChance": 60,
            "ControlFlowFlattening.UseOpaqueTransformationChance": 40,
            "ControlFlowFlattening.UseGlobalVariableOpaquesChance": 80,
            "ControlFlowFlattening.UseSipHashedStateChance": 40,
            "ControlFlowFlattening.CloneSipHashChance": 80,
        });

        z.RegisterPass(ObfuscationType.IndirectBranch, {
            PassIterations: 1,
            "IndirectBranch.Chance": 100,
        });

        z.RegisterPass(ObfuscationType.SimpleIndirectBranch, {
            PassIterations: 1,
            "SimpleIndirectBranch.Chance": 100,
        });
    }

    OnString(Str) {
        z.log(Str);
        return z.None;
    }

    Init() {
        z.AddMetaData("@shield::<meow>{Vortexus=0.2, Zyrox=0.4}");
    }
}

z.RegisterClass(new ZyroxPluginImpl());
