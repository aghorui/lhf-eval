#ifndef DEFS_H
#define DEFS_H

#include "Analysis.h"
#include "AnalysisDef.h"
#ifdef NAIVE_MODE
#include "CommonNaive.h"
#else
#include "Common.h"
#endif
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/UnifyFunctionExitNodes.h"
#include <bits/stdc++.h>
#include <cxxabi.h>
#include <memory>
#include <string>

using namespace llvm;
static llvm::LLVMContext context3;

using F3 = std::pair<LFLivenessSet, long int>; // F3 = pair<Def, Block> Eg: <{x,y,z}, 20>
using B3 = NoAnalysisType;

class FISP_Def : public AnalysisDef<F3, B3> {
    std::map<Function *, std::pair<LFLivenessSet, int>> mapFunDefBlock;

public:
    FISP_Def() : AnalysisDef(){};
    FISP_Def(std::unique_ptr<llvm::Module> &, bool, bool, bool, ExecMode, bool);

    void setMapFunDefBlock(Function *, std::pair<LFLivenessSet, int>);
    std::pair<LFLivenessSet, int> getMapFunDefBlock(Function *);
    void printMapDefBlock();

    // Forward Analysis
    F3 getBoundaryInformationFIS();
    F3 getInitialisationValueFIS();
    bool EqualDataFlowValuesFIS(const F3 &d1, const F3 &d2) const;
    void printDataFlowValuesFIS(const F3 &dfv) const;
    F3 computeDFValueFIS(BaseInstruction *I, int, F3 prevdfv);
    F3 performMeetFIS(const F3 &d1, const F3 &d2);
    F3 getPurelyLocalComponentFIS(const F3 &dfv) const;
    F3 getPurelyGlobalComponentFIS(const F3 &dfv) const;
    F3 CallInflowFunctionFIS(int, Function *, BasicBlock *, const F3 &);
    F3 CallOutflowFunctionFIS(int, Function *, BasicBlock *, const F3 &, const F3 &);
    F3 performMergeAtCallFIS(const F3 &d1, const F3 &d2);
    int getDepAnalysisCalleeContext(int, BaseInstruction *);
    bool checkInDef(SLIMOperand *, F3);
    bool compareDefValues(SLIMOperand *, SLIMOperand *);
    bool compareIndices_Def(std::vector<SLIMOperand *> ipVec1, std::vector<SLIMOperand *> ipVec2);
};
#endif
