#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include <vector>
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

using namespace llvm;

namespace {

    // New Pass Manager version
    struct ExitBlockPredecessorPass : public PassInfoMixin<ExitBlockPredecessorPass> {
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
            bool modified = runExitBlockPredecessorPass(F);
            
            if (!modified) {
                return PreservedAnalyses::all();
            }
            
            return PreservedAnalyses::none(); 
        }
    

    // Common implementation for both pass managers
    bool runExitBlockPredecessorPass(Function &F) {
            bool modified = false;
            
            // Find the last basic block in the function
            BasicBlock *lastBB = nullptr;
            for (auto &BB : F) {
             //   lastBB = &BB;
             Instruction *terminator = BB.getTerminator();
                
             // Check for return instructions
             if (isa<ReturnInst>(terminator)) {
                    lastBB = &BB;
                }
            }
            
            if (!lastBB) {
                return false; // Empty function
            }
            
            // Collect exit blocks first to avoid iterator invalidation
            std::vector<BasicBlock*> exitBlocks;
            for (auto &BB : F) {
                if (isExitBlock(&BB) && &BB != lastBB) {
                    exitBlocks.push_back(&BB);
                }
            }
            
            // Process each exit block to connect it as predecessor of the last block
            for (BasicBlock *exitBB : exitBlocks) {
                errs() << "Found exit block: " << exitBB->getName() << "\n";
                
                // Check if this block already has the last BB as successor
                bool hasLastBBAsSuccessor = false;
                for (auto *Succ : successors(exitBB)) {
                    if (Succ == lastBB) {
                        hasLastBBAsSuccessor = true;
                        break;
                    }
                }
                
                // If not already connected, add minimal CFG edge
                if (!hasLastBBAsSuccessor) {
                    if (addCFGEdgeToLast(exitBB, lastBB)) {
                        errs() << "Added CFG edge from exit block " << exitBB->getName() 
                               << " to last block " << lastBB->getName() << "\n";
                        modified = true;
                    }
                }
            }
            
            return modified;
        }
        
    private:
        // Check if a basic block is an exit block
        bool isExitBlock(BasicBlock *BB) {
            Instruction *terminator = BB->getTerminator();
            if (!terminator) return false;
            
            // Check for return instructions
            if (isa<ReturnInst>(terminator)) {
                return true;
            }
            
            // Check for unreachable instructions
            if (isa<UnreachableInst>(terminator)) {
                return true;
            }
            
            // Check for function calls that don't return (like exit(), abort())
            for (auto &I : *BB) {
                if (auto *CI = dyn_cast<CallInst>(&I)) {
                    if (Function *calledFunc = CI->getCalledFunction()) {
                        StringRef funcName = calledFunc->getName();
                        if (funcName == "exit" || funcName == "abort" || 
                            funcName == "_Exit" || funcName == "quick_exit") {
                            return true;
                        }
                        // Check for functions marked as noreturn
                        if (calledFunc->hasFnAttribute(Attribute::NoReturn)) {
                            return true;
                        }
                    }
                }
            }
            
            return false;
        }
        
        // Add minimal CFG edge from exit block to last block
        bool addCFGEdgeToLast(BasicBlock *exitBB, BasicBlock *lastBB) {
            Instruction *terminator = exitBB->getTerminator();
            if (!terminator) return false;
            
            // The key insight: we need to create a "phantom" or "auxiliary" branch
            // that doesn't affect the original control flow but creates the CFG edge
            
            if (/*isa<ReturnInst>(terminator) ||*/ isa<UnreachableInst>(terminator)) {
                // Create a new basic block that will serve as an intermediate
                Function *F = exitBB->getParent();
                BasicBlock *intermediateBB = BasicBlock::Create(
                    exitBB->getContext(), 
                    exitBB->getName() + ".to_last", 
                    F, 
                    lastBB // Insert before last block
                );
                
                // The intermediate block branches to the last block
                IRBuilder<> intermediateBuilder(intermediateBB);
                intermediateBuilder.CreateBr(lastBB);
                
                // Now we modify the exit block minimally:
                // Insert a conditional branch that never takes the path to intermediate
                // but creates the CFG edge we need
                
                IRBuilder<> exitBuilder(exitBB);
                exitBuilder.SetInsertPoint(terminator);
                
                // Create a condition that's always false
                Value *alwaysFalse = ConstantInt::getFalse(exitBB->getContext());
                
                // Create conditional branch: if (false) goto intermediate; else fallthrough
                // This creates the CFG edge but never actually branches to intermediate
                BranchInst *condBr = exitBuilder.CreateCondBr(
                    alwaysFalse, 
                    intermediateBB,  // Never taken
                    intermediateBB   // Create a temporary target, we'll fix this
                );
                
                // Now we need to handle the fallthrough case
                // Create another intermediate block for the original terminator
                BasicBlock *originalExitBB = BasicBlock::Create(
                    exitBB->getContext(),
                    exitBB->getName() + ".orig_exit",
                    F,
                    lastBB
                );
                
                // Move the original terminator to the new block
                terminator->removeFromParent();
                originalExitBB->getInstList().push_back(terminator);
              //  terminator->insertBefore(originalExitBB->end());
                
                // Fix the conditional branch to go to original exit for false case
                condBr->setSuccessor(1, originalExitBB);
                
                return true;
            }
            else if (auto *brInst = dyn_cast<BranchInst>(terminator)) {
                if (brInst->isUnconditional()) {
                    // For unconditional branches, we can create a switch instruction
                    // that has the same behavior but includes last block as a case
                    
                    BasicBlock *originalTarget = brInst->getSuccessor(0);
                    IRBuilder<> builder(exitBB);
                    builder.SetInsertPoint(brInst);
                    
                    // Create a switch that always goes to original target
                    // but has last block as an unreachable case (creating CFG edge)
                    Value *alwaysZero = ConstantInt::get(
                        IntegerType::get(exitBB->getContext(), 32), 0
                    );
                    
                    SwitchInst *switchInst = builder.CreateSwitch(
                        alwaysZero, 
                        originalTarget, // default case
                        1 // reserve space for one case
                    );
                    
                    // Add case that's never hit but creates CFG edge to last block
                    switchInst->addCase(
                        ConstantInt::get(IntegerType::get(exitBB->getContext(), 32), 1),
                        lastBB
                    );
                    
                    // Remove original branch
                    brInst->eraseFromParent();
                    
                    return true;
                }
            }
            
            return false;
        }
    };
}


// For new pass manager (LLVM 14+)
// extern "C" llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
//     return {
//         LLVM_PLUGIN_API_VERSION, "ExitBlockPredecessorPass", "v0.1",
//         [](PassBuilder &PB) {
//             // Register the pass for function pass manager
//             PB.registerPipelineParsingCallback(
//                 [](StringRef Name, FunctionPassManager &FPM,
//                    ArrayRef<PassBuilder::PipelineElement>) {
//                     if (Name == "exit-block-predecessor") {
//                         FPM.addPass(ExitBlockPredecessorPass());
//                         return true;
//                     }
//                     return false;
//                 });
                
//             // Register the pass for optimization pipeline insertion
//             PB.registerOptimizerLastEPCallback(
//                 [](ModulePassManager &MPM, OptimizationLevel Level) {
//                     FunctionPassManager FPM;
//                     FPM.addPass(ExitBlockPredecessorPass());
//                     MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
//                 });
//         }};
// }

