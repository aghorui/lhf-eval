#ifndef LFCPA_H
#define LFCPA_H

#include "Analysis.h"
#ifdef NAIVE_MODE
#include "CommonNaive.h"
#else
#include "Common.h"
#endif
#include "FISP_Def.h"
#include "FISP_Gen.h"
#include "FSSP_Kill.h"
#include "IR.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Argument.h"
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
// #include <Profiling.hpp>
using namespace llvm;

static llvm::LLVMContext context;

// using F = std::map<SLIMOperand *, std::set<SLIMOperand *>>;
using F = LFPointsToSet; // modAR::LHF
// using B = std::set<SLIMOperand *>;

// #ifdef LFCPA
// standard LFCPA
// using B = std::set<SLIMOperand *>;
using B = LFLivenessSet; // modAR::LHF

#ifdef PTA
using B = NoAnalysisType;
#endif

// just for testing
std::unique_ptr<llvm::Module> module;
BasicBlock *bBlk;

// global declaration
FISP_Def objDef(module, true, true, false, FISP,
                false); ////Create an object of Def class

class FISArray {
    SLIMOperand *opdArray;
    llvm::StringRef nmArray;
    std::set<BasicBlock *> ptUse;
    std::set<BasicBlock *> ptDef;
    LFLivenessSet arrPointees;
    bool flgChangeInPointees;
    bool flgChangeInUse;

public:
    FISArray(){};
    FISArray(SLIMOperand *);
    SLIMOperand *fetchSLIMArrayOperand();
    void setOnlyArrayName(llvm::StringRef);
    llvm::StringRef getOnlyArrayName();
    void setPtUse(BasicBlock *);
    std::set<BasicBlock *> getPtUse();
    void setPtDef(BasicBlock *);
    std::set<BasicBlock *> getPtDef();
    void setFlgChangeInPointees(bool);
    bool getFlgChangeInPointees();
    void setFlgChangeInUse(bool);
    bool getFlgChangeInUse();
    bool isPtUseNotEmpty();
    LFLivenessSet getArrayPointees();
    void setArrayPointees(LFLivenessSet);
};

class IPLFCPA : public Analysis<F, B> {
    std::set<Function *> npFunList;
    std::set<Function *> pointerFunList;
    std::list<FISArray *> listFISArrayObjects;
    vector<pair<Function *, Function *>> call_graph_edges;
    std::map<std::pair<Function *, std::pair<F, B>>, std::pair<std::pair<LFLivenessSet, int>, bool>> tabContextDefBlock;
    std::map<long long, std::list<std::pair<F, B>>> mapMergedCalleeDefBlock;
    /*std::map<std::tuple<Function *, long long, std::pair<F, B>>,
    std::pair<std::pair<LFLivenessSet, int>, bool>>
        tabContextDefBlock;*/ //instruction_id not required
    std::map<std::pair<llvm::Value *, std::vector<SLIMOperand *>>, SLIMOperand *> mapNewOperand;
    std::map<long long, SLIMOperand *> mapMallocInsOperand;
    std::map<long long, std::pair<LFLivenessSet, LFPointsToSet>> mapUsePointAndPtpairs;
    std::map<std::tuple<Function *, int, B>, bool>
        mapPropagatePointstoInfoToFunction;

public:
    IPLFCPA() : Analysis(){};
    IPLFCPA(std::unique_ptr<llvm::Module> &, bool, bool, bool, ExecMode, bool);
    IPLFCPA(std::unique_ptr<llvm::Module> &, const string &, bool, bool, bool, ExecMode, bool);
    // IPLFCPA(bool, bool);
    // IPLFCPA(bool, const string &,bool);
    // IPLFCPA(std::unique_ptr<llvm::Module>&, bool, bool, bool);
    // IPLFCPA(std::unique_ptr<llvm::Module>&, const string &, bool, bool, bool);

    // Backward Analysis
    B getBoundaryInformationBackward();
    B getInitialisationValueBackward();
    bool EqualDataFlowValuesBackward(const B &d1, const B &d2) const;
    bool EqualContextValuesBackward(const B &d1, const B &d2) const;
    B performMeetBackward(const B &, const B &) const;
    B getPurelyGlobalComponentBackward(const B &dfv) const;
    void printDataFlowValuesBackward(const B &dfv) const;
    // std::set<SLIMOperand *> backwardMerge(std::set<SLIMOperand *>, std::set<SLIMOperand *>);
    LFLivenessSet backwardMerge(LFLivenessSet, LFLivenessSet);
    //  B backwardMerge(B, B);
    B getFPandArgsBackward(long int, Instruction *);
    B computeInFromOut(BaseInstruction *I);
    B eraseFromLin(SLIMOperand *pointee, B INofInst);
    B insertRhsLin(
        B currentIN, std::vector<std::pair<SLIMOperand *, int>> rhslist, LFPointsToSet forwardIN, BaseInstruction *ins);
    // B getLocalComponentB(const B &); //not present in Analysis.h
    B getPurelyLocalComponentBackward(const B &dfv) const;
    B functionCallEffectBackward(Function *, const B &);

    // Forward Analysis
    F getBoundaryInformationForward();
    F getInitialisationValueForward();
    bool EqualDataFlowValuesForward(const F &d1, const F &d2) const;
    bool EqualContextValuesForward(const F &d1, const F &d2) const;
    F restrictByLivness(F, B);
    F performMeetForward(const F &d1, const F &d2, const string pos);
    F performMeetForward(const F &d1, const F &d2);
    F performMeetForwardWithParameterMapping(const F &d1, const F &d2, const string pos);
    F performMeetForwardTemp(F d1, F d2);
    F getPurelyLocalComponentForward(const F &dfv) const;
    F getPurelyGlobalComponentForward(const F &dfv) const;
    void printDataFlowValuesForward(const F &dfv) const;
    F forwardMerge(F, F);
    F computeOutFromIn(BaseInstruction *I);
    F functionCallEffectForward(Function *, const F &);

    pair<F, B> CallInflowFunction(
        int, Function *, BasicBlock *, const F &, const B &, pair<SLIMOperand *, SLIMOperand *> return_operand_map);
    pair<F, B> CallOutflowFunction(
        int, Function *, BasicBlock *, const F &, const B &, const F &, const B &,
        pair<SLIMOperand *, SLIMOperand *> return_operand_map);
    pair<F, B> CallOutflowFunction(
        int, BaseInstruction *, Function *, BasicBlock *, const F &, const B &, const F &, const B &,
        pair<SLIMOperand *, SLIMOperand *> return_operand_map);

    void printIndices(SLIMOperand *) const;
    SLIMOperand *createDummyOperand(int);

    /* Added on 3.2.23*/
    pair<pair<F, B>, pair<F, B>> getInitialisationValueGenerativeKill();
    pair<F, B> computeGenerative(int context_label, Function *Func);
    pair<F, B> performMeetGenerative(const pair<F, B> &d1, const pair<F, B> &d2);
    pair<F, B> computeKill(int context_label, Function *Func);
    pair<F, B> CallInflowFunction(
        int context_label, Function *Func, BasicBlock *bb, const F &a1, const B &d1, pair<F, B> generativeAtCall,
        pair<SLIMOperand *, SLIMOperand *> return_operand_map);
    pair<F, B> CallOnflowFunction(
        int context_label, BaseInstruction *I, pair<F, B> killAtCall,
        pair<SLIMOperand *, SLIMOperand *> return_operand_map);
    pair<F, B> computeSetDifference(F, B, LFLivenessSet, pair<SLIMOperand *, SLIMOperand *>);
    B performMergeAtCallBackward(const B &d1, const B &d2) const;
    F performMergeAtCallForward(const F &d1, const F &d2);

    B getMainBoundaryInformationBackward(BaseInstruction *I);
    F getMainBoundaryInformationForward(BaseInstruction *I);
    bool compareIndices(std::vector<SLIMOperand *>, std::vector<SLIMOperand *>) const;
    bool compareOperands(SLIMOperand *, SLIMOperand *) const;

    F performCallReturnArgEffectForward(
        const F &dfv, const pair<SLIMOperand *, SLIMOperand *> return_operand_map) const;
    B performCallReturnArgEffectBackward(
        const B &dfv, const pair<SLIMOperand *, SLIMOperand *> return_operand_map) const;

    F getGlobalandFunctionArgComponentForward(const F &dfv) const;
    B getGlobalandFunctionArgComponentBackward(const B &dfv) const;

    //-------------
    void simplifyIR(slim::IR *optIR);
    void identifyNoPtrFun(slim::IR *optIR);
    bool isFunctionIgnored(llvm::Function *Fun);
    bool isPointerSource(SLIMOperand *op);
    void addAllPredecessorFunctions(Function *fun);
    void identifyPtrFun(slim::IR *optIR);
    void populateCallGraphEdges(std::unique_ptr<llvm::Module> &mod);
    void findNode(CallGraphNode *CN);

    set<BasicBlock *> getMixedFISBasicBlocksBackward();
    set<BasicBlock *> getMixedFISBasicBlocksForward();

    bool checkChangeinPointees(LFLivenessSet, LFLivenessSet);

    //---------Functions to incorporate the improved approach
    void preAnalysis(std::unique_ptr<llvm::Module> &mod, slim::IR *optIR);
    void setTabContextDefBlock(std::pair<Function *, std::pair<F, B>>, std::pair<std::pair<LFLivenessSet, int>, bool>);
    std::pair<std::pair<LFLivenessSet, int>, bool> getTabContextDefBlock(pair<Function *, pair<F, B>>);
    /*  void setTabContextDefBlock(
          std::tuple<Function *, long long, std::pair<F, B>>,
          std::pair<std::pair<LFLivenessSet, int>, bool>);
      std::pair<std::pair<LFLivenessSet, int>, bool>
         getTabContextDefBlock(std::tuple<Function *, long long, pair<F, B>>);
    */
    pair<F, B> CallInflowFunction(
        int context_label, Function *Func, BasicBlock *bb, const F &a1, const B &d1, int source_context_label,
        pair<SLIMOperand *, SLIMOperand *> return_operand_map);
    pair<F, B> CallOnflowFunction(
        int context_label, BaseInstruction *I, int source_context_label,
        pair<SLIMOperand *, SLIMOperand *> return_operand_map);
    //    std::pair<std::set<SLIMOperand*>, int>
    //    performMergeDefBlock(std::pair<std::set<SLIMOperand*>, int> prev,
    //    std::pair<std::set<SLIMOperand*>, int> curr);

    std::pair<std::pair<LFLivenessSet, int>, bool> performMergeDefBlock(
        std::pair<std::pair<LFLivenessSet, int>, bool> prev, std::pair<std::pair<LFLivenessSet, int>, bool> curr);

    /* pair<B, bool> analyseSourcePointeesBackward(SLIMOperand *source,
                                                 SLIMOperand *LHS_orig, B INofInst,
                                                 BaseInstruction *I);

      std::pair<F, std::pair<LFLivenessSet, int>>
     analyseSourcePointeesForward(
         SLIMOperand *source, SLIMOperand *LHS_orig,
         std::set<SLIMOperand *> rhsSet, F OUTofInst, BaseInstruction *I,
         std::pair<std::set<SLIMOperand *>, int> valDefBlock);*/

    void handleArrayElementBackward(SLIMOperand *, BaseInstruction *I);
    bool checkIsSource(SLIMOperand *);
    SLIMOperand *fetchNewOperand(llvm::Value *value, std::vector<SLIMOperand *> vecIndex);
    // void insertGEPOperand(llvm::Value* value, std::vector<SLIMOperand *>
    // vecIndex, SLIMOperand*); SLIMOperand* checkGEPOperand(llvm::Value* value,
    // std::vector<SLIMOperand *> vecIndex);

    set<Function *> handleFunctionPointers(F Pin, SLIMOperand *function_operand, int context_label, BaseInstruction *I);
    pair<F, B> performParameterMapping(BaseInstruction *base_instruction, Function *callee, F PIN, B LIN_StartQ);
    B computeInFromOutForIndirectCalls(BaseInstruction *I);
    // B handleMallocInsBackward(BaseInstruction *I); //#MALLOC
    // F handleMallocInsForward(BaseInstruction *I);  //#MALLOC
    SLIMOperand *getMallocInsOperand(long long);
    void setMallocInsOperand(long long, SLIMOperand *);
    SLIMOperand *fetchMallocInsOperand(long long, BasicBlock *);
    pair<F, B> CallOnflowFunctionForIndirect(
        int context_label, BaseInstruction *I, int source_context_label,
        pair<SLIMOperand *, SLIMOperand *> return_operand_map, Function *);
    bool isBlockChangedForForward(int context_label, BaseInstruction *I);
    void printFileDataFlowValuesForward(const F &dfv, std::ofstream &out) const;
    void printFileDataFlowValuesBackward(const B &dfv, std::ofstream &out) const;
    bool checkOperandInDEF(LFLivenessSet, SLIMOperand *);

    // std::list<std::pair<F, B>> getCalleeListForInsID(long long);
    // void insertInflowToCalleeListForInsID(long long, std::pair<F, B>);
    bool compareOperands_1(SLIMOperand *, SLIMOperand *) const;
    //bool checkInBackwardsVector(SLIMOperand *, long long, LFInstLivenessSet);
    //LFInstLivenessSet addInLIN(SLIMOperand *, long long, LFInstLivenessSet);
    long long getLastInsIDForFunction(Function *);
    void setForwardINForNonEntryBasicBlock(pair<int, BasicBlock *>);
    //std::set<BasicBlock *> manageWorklistForward(LFInstLivenessSet, BasicBlock *, Function *);
    bool isFirstInstruction(BaseInstruction *);
    void propagatePINFromStart(BaseInstruction *);
    void setUsePoint(long long, SLIMOperand *);
    void setPointsToPairForUsePoint(long long, SLIMOperand *, LFPointsToSet);
    void printUsePointAndPtpairs();
    void setPropagatePinFlagForFunContextPair(Function *, int, B);
    bool getPropagatePinFlagForFunContextPair(Function *, int, B);
    B generateLivenessOfSource(
        B, F, std::set<std::pair<SLIMOperand *, int> *>, BaseInstruction *); // idea of (source+dereflevel) in Slim
    std::pair<SLIMOperand *, int> getReferenceWithIndir(SLIMOperand *, int);
    LFLivenessSet generatePointeeSetOfSource(SLIMOperand *, int, F, BaseInstruction *);
};

#endif
