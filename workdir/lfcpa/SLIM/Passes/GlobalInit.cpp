#include "llvm/IR/Constant.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#define DEBUG_TYPE "global-init"

using namespace llvm;

namespace {
class GlobalInit : public PassInfoMixin<GlobalInit> {
  private:
    bool isGlobalArrayOfFunctionPointers(GlobalVariable &globalVariable) {
        auto *globalType = globalVariable.getType()->getContainedType(0);
        return globalType->isArrayTy() &&
               globalType->getContainedType(0)->isPointerTy() &&
               globalType->getContainedType(0)
                   ->getContainedType(0)
                   ->isFunctionTy();
    }

  public:
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
        // get the main function
        Function *mainFunction = M.getFunction("main");
        if (!mainFunction) {
            return PreservedAnalyses::all();
        }
        // get the first basic block
        BasicBlock &entryBlock = mainFunction->getEntryBlock();
        // get the first instruction
        Instruction &firstInstruction = *entryBlock.getFirstNonPHI();
        // create a new IRBuilder
        IRBuilder<> builder(&firstInstruction);
        // iterate through all global variables
        for (GlobalVariable &globalVariable : M.globals()) {
            if (isGlobalArrayOfFunctionPointers(globalVariable) &&
                globalVariable.hasInitializer()) {
                llvm::errs() << "Global variable: " << globalVariable.getName()
                             << " is an array\n";
                llvm::Constant *initializer = globalVariable.getInitializer();
                // convert the initializer to store instructions in the main
                // function
                for (unsigned i = 0; i < initializer->getNumOperands(); i++) {
                    llvm::Constant *operand =
                        initializer->getAggregateElement(i);
                    if (!operand) {
                        continue;
                    }
                    llvm::Function *function = dyn_cast<Function>(operand);
                    if (function) {
                        llvm::errs()
                            << "Function: " << function->getName() << "\n";
                        // create a new store instruction
                        // auto gep = builder.CreateGEP(
                        //     globalVariable.getType()->getContainedType(0),
                        //     &globalVariable, {0, i + 1});
                        // create a store instruction that stores it through a
                        // GEP operator
                        // create instruction like this:
                        // store  i32 (i32, i32)* @ErrorIt, i32 (i32, i32)**
                        // getelementptr inbounds ([7 x i32 (i32, i32)*], [7 x
                        // i32 (i32, i32)*]* @evalRoutines, i64 0, i64 0)
                        llvm::Constant *constant0 = ConstantInt::get(
                            Type::getInt64Ty(M.getContext()), 0);
                        llvm::Constant *constanti = ConstantInt::get(
                            Type::getInt64Ty(M.getContext()), i);
                        auto gepExpr = ConstantExpr::getInBoundsGetElementPtr(
                            globalVariable.getType()->getContainedType(0),
                            &globalVariable,
                            ArrayRef<Constant *>{constant0, constanti});
                        // gepExpr->dump();
                        builder.CreateStore(function, gepExpr);
                        if (llvm::verifyModule(M, &llvm::errs())) {
                            llvm::errs() << "Module verification failed\n";
                            // print the module
                            M.print(llvm::errs(), nullptr);
                            return PreservedAnalyses::all();
                        }
                    }
                }
            }
        }
        return PreservedAnalyses::none();
    }
};
} // namespace

extern "C" PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
    return {LLVM_PLUGIN_API_VERSION, "GlobalInit", "v0.1", [](PassBuilder &PB) {
                PB.registerPipelineParsingCallback(
                    [](StringRef Name, ModulePassManager &MPM,
                       ArrayRef<PassBuilder::PipelineElement>) {
                        if (Name == "global-init") {
                            MPM.addPass(GlobalInit());
                            return true;
                        }
                        return false;
                    });
            }};
}
