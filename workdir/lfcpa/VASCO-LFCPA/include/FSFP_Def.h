#ifndef DEFS_H
#define DEFS_H

#include "Analysis.h"
#include "AnalysisDef.h"
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

using F3 = std::pair<std::set<SLIMOperand *>, std::set<SLIMOperand *>>;
using B3 = NoAnalysisType;

class FSFP_Def : public AnalysisDef<F3, B3> {

public:
    FSFP_Def() : AnalysisDef(){};
    FSFP_Def(std::unique_ptr<llvm::Module> &, bool, bool, bool, ExecMode, bool);

    // Forward Analysis
    F3 getBoundaryInformationForward();
    F3 getInitialisationValueForward();

    pair<F3, B3> CallInflowFunction(int, Function *, BasicBlock *, const F3 &, const B3 &);
    pair<F3, B3> CallOutflowFunction(int, Function *, BasicBlock *, const F3 &, const B3 &, const F3 &, const B3 &);
    F3 computeOutFromIn(llvm::Instruction &I, int depContext);
    F3 computeOutFromIn(BaseInstruction *I, int depContext);
    F3 performMeetForward(const F3 &d1, const F3 &d2, const string pos);

    bool EqualDataFlowValuesForward(const F3 &d1, const F3 &d2) const; //{}
    bool EqualContextValuesForward(const F3 &d1, const F3 &d2) const;  //{}
    F3 getPurelyLocalComponentForward(const F3 &dfv) const;
    F3 getPurelyGlobalComponentForward(const F3 &dfv) const;
    F3 getMixedComponentForward(const F3 &dfv) const;
    F3 getCombinedValuesAtCallForward(const F3 &dfv1, const F3 &dfv2) const;
    void printDataFlowValuesForwardFSFP(const F3 &dfv) const;
    F3 performMergeAtCallForward(const F3 &d1, const F3 &d2); //{}
    //  F3 getPinStartCallee(long int, Instruction *, F3 &, Function *);
    //  unsigned int getSize(F3 &);
    //  void printFileDataFlowValuesForward(const F3 &dfv, std::ofstream &out) const {}
    //  F3 functionCallEffectForward(Function *, const F3 &);
};
#endif
