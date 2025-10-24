#ifndef FSSP_H
#define FSSP_H

#include "Analysis.h"
#include "AnalysisGenKill.h"
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
// #include "IR.h"

using namespace llvm;
static llvm::LLVMContext context1;

// using F = std::map<SLIMOperand*, std::set<SLIMOperand*>>;
using F1 = NoAnalysisType;
using B1 = std::set<SLIMOperand *>;

class FSSP_Kill : public AnalysisGenKill<F1, B1> {
    std::map<std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, B1> *, int>, B1> mapDepObjKill;
    std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, B1> *, int> varDependentObj;

public:
    FSSP_Kill() : AnalysisGenKill(){};
    FSSP_Kill(std::unique_ptr<llvm::Module> &, bool, bool, bool, ExecMode, bool);

    // Backward Analysis
    B1 getBoundaryInformationBackward();
    B1 getInitialisationValueBackward();
    bool EqualDataFlowValuesBackward(const B1 &d1, const B1 &d2) const;
    bool EqualContextValuesBackward(const B1 &d1, const B1 &d2) const;
    B1 performMeetBackward(const B1 &, const B1 &) const;
    B1 getPurelyGlobalComponentBackward(const B1 &dfv) const;
    void printDataFlowValuesBackward(const B1 &dfv) const;
    B1 computeInFromOut(BaseInstruction *I, int depAnalysisContext);
    B1 getPurelyLocalComponentBackward(const B1 &dfv) const;
    B1 getKillValforDepObjContext(std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, B1> *, int>);
    void setDependentObj(std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, B1> *, int>);
    std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, B1> *, int> getDependentObj();
    B1 backwardMerge(B1, B1) const;
    pair<F1, B1> CallInflowFunction(int, Function *, BasicBlock *, const F1 &, const B1 &);
    pair<F1, B1> CallOutflowFunction(int, Function *, BasicBlock *, const F1 &, const B1 &, const F1 &, const B1 &);
    B1 performMergeAtCallBackward(const B1 &d1, const B1 &d2) const;
    //  int getDepAnalysisCalleeContext(int, BaseInstruction*);
    // B backwardMerge(B, B);
    //   B eraseFromLin(SLIMOperand* pointee, B INofInst);
    //  B insertRhsLin(B currentIN, std::vector<std::pair<SLIMOperand *, int>> rhslist, std::map<SLIMOperand*,
    //  std::set<SLIMOperand*>>  forwardIN, BaseInstruction* ins);
    // B getLocalComponentB(const B &); //not present in Analysis.h
};

#endif
