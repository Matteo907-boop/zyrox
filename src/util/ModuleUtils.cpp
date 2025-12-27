#include <core/ZyroxPassOptions.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Linker/Linker.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <random>
#include <sstream>
#include <utils/FunctionUtils.h>
#include <utils/Logger.h>
#include <utils/ModuleUtils.h>

std::vector<std::string> meta_datas;

void ModuleUtils::ShuffleGlobals(Module &m)
{
    std::vector<GlobalVariable *> globals;
    for (auto &gv : m.globals())
    {
        globals.push_back(&gv);
    }

    if (globals.size() <= 1)
        return;

    for (auto *gv : globals)
    {
        gv->removeFromParent();
    }

    std::random_device rd;
    std::mt19937 rng(rd());
    std::ranges::shuffle(globals, rng);

    for (auto *gv : globals)
    {
        m.insertGlobalVariable(gv);
    }
}

void ModuleUtils::ShuffleFunctions(Module &m)
{
    std::vector<Function *> funcs;

    for (Function &f : m)
    {
        if (!f.isDeclaration())
        {
            funcs.push_back(&f);
        }
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::ranges::shuffle(funcs, g);

    for (Function *f : funcs)
    {
        m.getFunctionList().remove(f);
    }

    for (Function *f : funcs)
    {
        m.getFunctionList().push_back(f);
    }
}

std::vector<std::string> Split(const std::string &s, char delim);

void AddPotentialPass(Function &f, StringRef &annotation);

void ModuleUtils::ExpandCustomAnnotations(Module &m)
{
    if (GlobalVariable *global_annotations =
            m.getNamedGlobal("llvm.global.annotations"))
    {
        ConstantArray *annotations_array =
            cast<ConstantArray>(global_annotations->getInitializer());
        for (int i = 0; i < annotations_array->getNumOperands(); i++)
        {
            ConstantStruct *annotation_struct =
                cast<ConstantStruct>(annotations_array->getOperand(i));

            if (Function *f =
                    dyn_cast<Function>(annotation_struct->getOperand(0)))
            {
                GlobalVariable *annotation_str =
                    cast<GlobalVariable>(annotation_struct->getOperand(1));
                if (isa<ConstantDataArray>(annotation_str->getInitializer()))
                {
                    StringRef annotation = cast<ConstantDataArray>(
                                               annotation_str->getInitializer())
                                               ->getAsCString();
                    AddPotentialPass(*f, annotation);
                }
            }
        }
    }
}

std::unique_ptr<Module> ModuleUtils::LoadFromIR(LLVMContext &ctx,
                                                const char *ir)
{
    SMDiagnostic err;

    ctx.setDiscardValueNames(false);

    std::unique_ptr<Module> src_module = parseAssemblyString(ir, err, ctx);

    ctx.setDiscardValueNames(true);

    if (!src_module)
    {
        err.print("IR Parsing", errs());
        Logger::Error("failed to load a module from IR");
    }

    return src_module;
}

void ModuleUtils::LinkModules(Module &dst, std::unique_ptr<Module> src)
{
    src->setDataLayout(dst.getDataLayout());
    src->setTargetTriple(dst.getTargetTriple());
    if (Linker::linkModules(dst, std::move(src)))
    {
        Logger::Error("failed to link modules");
    }
}

void ModuleUtils::AddMetaData(const char *meta_data)
{
    meta_datas.push_back(meta_data);
}

void AddMetaDatas(Module &m);

void ModuleUtils::Finalize(Module &m)
{
    AddMetaDatas(m);
    ShuffleFunctions(m);
    ShuffleGlobals(m);
}

void AddPotentialPass(Function &f, StringRef &annotation)
{
    for (std::string &pass : Split(annotation.data(), ' '))
    {
        std::vector<std::string> parts = Split(pass, ':');
        std::string pass_code_name = parts[0];
        // first we make sure pass exist before we proceed.
        ZyroxFunctionPass *target_pass = nullptr;
        for (ZyroxFunctionPass &function_pass : zyrox_passes)
        {
            if (function_pass.CodeName == pass_code_name)
            {
                target_pass = &function_pass;
                break;
            }
        }
        if (target_pass == nullptr)
            break;

        std::vector<int> pass_args;
        if (parts.size() > 1) // Has Args
        {
            parts = Split(parts[1], ',');
            pass_args.resize(parts.size());
            std::ranges::transform(parts, pass_args.begin(),
                                   [](const std::string &s)
                                   { return std::stoi(s); });
        }
        ZyroxAnnotationArgs annotation_args(pass_args);
        target_pass->RegisterFromAnnotation(f, &annotation_args);
    }
}

std::vector<std::string> Split(const std::string &s, char delim)
{
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim))
        out.push_back(item);
    return out;
}

void AddMetaDatas(Module &m)
{
    LLVMContext &ctx = m.getContext();

    for (std::string meta_data : meta_datas)
    {
        Constant *str_val =
            ConstantDataArray::getString(ctx, meta_data.c_str(), true);
        GlobalVariable *gv = new GlobalVariable(m, str_val->getType(), true,
                                                GlobalValue::PrivateLinkage,
                                                str_val, "obf.meta");

        gv->setSection(".rodata.meta");

        appendToUsed(m, {gv});
    }
}
