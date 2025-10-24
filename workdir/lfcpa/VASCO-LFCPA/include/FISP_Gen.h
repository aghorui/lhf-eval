#ifndef FISP_H
#define FISP_H

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
static llvm::LLVMContext context2;

// using F = std::map<SLIMOperand*, std::set<SLIMOperand*>>;
using F2 = std::pair<std::set<SLIMOperand *>, std::set<SLIMOperand *>>;
using B2 = NoAnalysisType;

class FISP_Gen : public AnalysisGenKill<F2, B2> {
    std::map<std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, std::set<SLIMOperand *>> *, int>, F2>
        mapDepObjGen;
    std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, std::set<SLIMOperand *>> *, int>
        varDependentObj;

public:
    FISP_Gen() : AnalysisGenKill(){};
    FISP_Gen(std::unique_ptr<llvm::Module> &, bool, bool, bool, ExecMode, bool);

    // Forward Analysis
    F2 getBoundaryInformationFIS();
    F2 getInitialisationValueFIS();
    bool EqualDataFlowValuesFIS(const F2 &d1, const F2 &d2) const;
    void printDataFlowValuesFIS(const F2 &dfv) const;
    F2 computeDFValueFIS(BaseInstruction *I, int depAnalysisContext);
    F2 performMeetFIS(const F2 &d1, const F2 &d2);
    std::set<SLIMOperand *> insertRhsintoRef(BaseInstruction *, int);

    F2 getGenValforDepObjContext(
        std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, std::set<SLIMOperand *>> *, int>);
    void setDependentObj(
        std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, std::set<SLIMOperand *>> *, int>);
    std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, std::set<SLIMOperand *>> *, int>
    getDependentObj();

    F2 getPurelyLocalComponentFIS(const F2 &dfv) const;
    F2 getPurelyGlobalComponentFIS(const F2 &dfv) const;
    F2 CallInflowFunctionFIS(int, Function *, BasicBlock *, const F2 &);
    F2 CallOutflowFunctionFIS(int, Function *, BasicBlock *, const F2 &, const F2 &);
    F2 performMergeAtCallFIS(const F2 &d1, const F2 &d2);
    //    int getDepAnalysisCalleeContext(int, BaseInstruction*);

    //      F2 getPurelyGlobalComponentBackward(const F2& dfv) const;
    //    F2 getPurelyLocalComponentBackward(const F2 &dfv) const;

    /*bool EqualContextValuesBackward(const B1& d1, const B1& d2) const;
          B1 performMeetBackward(const B1&, const B1&) const;
          B1 getPurelyGlobalComponentBackward(const B1& dfv) const;
          void printDataFlowValuesBackward(const B1& dfv) const;
          B1 computeInFromOut(BaseInstruction* I);
          B1 getPurelyLocalComponentBackward(const B1 &dfv) const;*/
    // B backwardMerge(B, B);
    //   B eraseFromLin(SLIMOperand* pointee, B INofInst);
    //  B insertRhsLin(B currentIN, std::vector<std::pair<SLIMOperand *, int>> rhslist, std::map<SLIMOperand*,
    //  std::set<SLIMOperand*>>  forwardIN, BaseInstruction* ins);
    // B getLocalComponentB(const B &); //not present in Analysis.h
};
#endif
