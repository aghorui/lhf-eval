#include "llvm/IR/Function.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/ReplaceConstant.h"
#include "llvm/IR/Instructions.h"

#include <stack>
#include <unordered_map>

using namespace llvm;

namespace {

bool flattenExpressionList(llvm::Instruction *inst) {
	using namespace llvm;

	bool changed = false;
	std::stack<llvm::Instruction *> stack;

	stack.push(inst);

	while (stack.size() > 0) {
		Instruction *curr = stack.top();
		stack.pop();

		if (isa<PHINode>(curr)) {
			std::unordered_map<llvm::BasicBlock *, llvm::Value *> dedup;
			PHINode *phi = dyn_cast<PHINode>(curr);

			for (unsigned int i = 0; i < phi->getNumIncomingValues(); i++) {
				Value *oper = phi->getIncomingValue(i);
				BasicBlock *pred = phi->getIncomingBlock(i);

				if (!isa<ConstantExpr>(oper)) {
					continue;
				}

				changed = true;
				ConstantExpr *expr = dyn_cast<ConstantExpr>(oper);

				Value *finalIncoming = nullptr;
				auto cursor = dedup.find(pred);

				if (cursor != dedup.end()) {
					finalIncoming = cursor->second;
				} else {
					Instruction *newInst = expr->getAsInstruction();
					newInst->insertBefore(pred->getTerminator());
					finalIncoming = newInst;
					dedup[pred] = finalIncoming;
					stack.push(newInst);
				}

				phi->setIncomingValue(i, finalIncoming);
			}
		} else {
			for (unsigned int i = 0; i < curr->getNumOperands(); i++) {
				Value *oper = curr->getOperand(i);

				if (!isa<ConstantExpr>(oper)) {
					continue;
				}

				changed = true;
				ConstantExpr *expr = dyn_cast<ConstantExpr>(oper);
				Instruction *newInst = expr->getAsInstruction();
				newInst->insertBefore(curr);

				curr->setOperand(i, newInst);
				stack.push(newInst);
			}
		}

	}

	return changed;
}
struct FlattenConstExprPass : public PassInfoMixin<FlattenConstExprPass> {
	PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM) {
		// simple example action
		for (auto &BB : F) {
			for (auto &inst : BB) {
				flattenExpressionList(&inst);
			}
		}
		return PreservedAnalyses::none();
	}
};

}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
	return {
		LLVM_PLUGIN_API_VERSION,
		"FlattenConstExprPass",
		"v0.1",
		[](PassBuilder &PB) {
			PB.registerPipelineParsingCallback(
				[](StringRef Name, FunctionPassManager &FPM,
				   ArrayRef<PassBuilder::PipelineElement>) {
					if (Name == "flatten-constexpr") {
						FPM.addPass(FlattenConstExprPass());
						return true;
					}
					return false;
				});
		}
	};
}