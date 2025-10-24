#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "chrono"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include <cassert>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <map>
#include <ostream>
#include <set>
#include <stack>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

#include "Context.h"
#include "Worklist.h"
// #include "TransformIR.h"
#include "TransitionGraph.h"

#include "IR.h"
#include "sample.hpp"
#include "llvm/Support/raw_ostream.h"

#ifdef NAIVE_MODE
#include "CommonNaive.h"
#else
#include "Common.h"
#endif

// #include "main_lbm.hpp"
// #include "main_mcf.hpp"
// #include "main_libquantum.hpp"
// #include "main_bzip2.hpp"
// #include "main_sjeng.hpp"
// #include "main_hmmer.hpp"
// #include "main_h264ref.hpp"
#include "Profiling.hpp"

using namespace llvm;
using namespace std;
using namespace std::chrono;
enum NoAnalysisType { NoAnalyisInThisDirection };

#ifdef ENABLE_PROCESS_MEM_USAGE

inline void process_mem_usage(float &vm_usage, float &resident_set) {
    using std::ifstream;
    using std::ios_base;
    using std::string;

    vm_usage = 0.0;
    resident_set = 0.0;

    ifstream stat_stream("/proc/self/stat", ios_base::in);

    string pid, comm, state, ppid, pgrp, session, tty_nr;
    string tpgid, flags, minflt, cminflt, majflt, cmajflt;
    string utime, stime, cutime, cstime, priority, nice;
    string O, itrealvalue, starttime;

    // the two fields we want
    //
    unsigned long vsize;
    long rss;

    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags >> minflt >> cminflt >>
        majflt >> cmajflt >> utime >> stime >> cutime >> cstime >> priority >> nice >> O >> itrealvalue >> starttime >>
        vsize >> rss;

    stat_stream.close();
    long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024;
    vm_usage = static_cast<float>(vsize) / 1024.0f;
    resident_set = static_cast<float>(rss * page_size_kb);
}

#else

inline void process_mem_usage(float &vm_usage, float &resident_set) {}

#endif

inline void printMemory(float memory, std::ofstream &out) {
    out << fixed;
    out << setprecision(6);
    out << memory / 1024.0;
    out << " MB\n";
}

/*class HashFunction {
public:
    auto operator()(const pair<int, BasicBlock *> &P) const {
        // return hash<int>()(P.first) ^ std::hash<BasicBlock *>()(P.second);
        // Same as above line, but viewed the actual source code of C++ and then
wrote it
        // Referred: functional_hash.h (line 114)
        // Referred: functional_hash.h (line 110)
        return static_cast<size_t>(P.first) ^
reinterpret_cast<size_t>(P.second);
    }

    auto operator()(const pair<int, Instruction *> &P) const {
        // return hash<int>()(P.first) ^ std::hash<Instruction *>()(P.second);
        // Same as above line, but viewed the actual source code of C++ and then
wrote it
        // Referred: functional_hash.h (line 114)
        // Referred: functional_hash.h (line 110)
        return static_cast<size_t>(P.first) ^
reinterpret_cast<size_t>(P.second);
    }

    auto operator()(const pair<int, BaseInstruction *> &P) const {
        // return hash<int>()(P.first) ^ std::hash<Instruction *>()(P.second);
        // Same as above line, but viewed the actual source code of C++ and then
wrote it
        // Referred: functional_hash.h (line 114)
        // Referred: functional_hash.h (line 110)
        return static_cast<size_t>(P.first) ^
reinterpret_cast<size_t>(P.second);
    }

    auto operator()(const pair<pair<int, BasicBlock *>,int> &P) const {
        // return hash<int>()(P.first) ^ std::hash<Instruction *>()(P.second);
        // Same as above line, but viewed the actual source code of C++ and then
wrote it
        // Referred: functional_hash.h (line 114)
        // Referred: functional_hash.h (line 110)
        return static_cast<size_t>(P.first.first) ^
reinterpret_cast<size_t>(P.first.second);
    }

    auto operator()(const pair<pair<int, BasicBlock *>,bool> &P) const {
        // return hash<int>()(P.first) ^ std::hash<Instruction *>()(P.second);
        // Same as above line, but viewed the actual source code of C++ and then
wrote it
        // Referred: functional_hash.h (line 114)
        // Referred: functional_hash.h (line 110)
        return static_cast<size_t>(P.first.first) ^
reinterpret_cast<size_t>(P.first.second);
    }
};
*/

typedef enum { FSFP, FSSP, FIFP, FISP } ExecMode;

template <class F, class B>
class Analysis {
private:
    Module *current_module;
    int context_label_counter;
    int current_analysis_direction{}; // 0:initial pass, 1:forward, 2:backward
    int processing_context_label{};
    std::unordered_map<int, unordered_map<llvm::Instruction *, pair<F, B>>> IN, OUT;
    std::unordered_map<int, unordered_map<BaseInstruction *, pair<F, B>>> SLIM_IN, SLIM_OUT;
    // mod:AR
    std::unordered_map<Function *, unordered_map<BaseInstruction *, pair<F, B>>> SLIM_IN_ALL, SLIM_OUT_ALL;

    std::list<BasicBlock *> backwardsBBList, forwardsBBList;
    std::unordered_map<int, unordered_map<Function *, F>> DFVALUE;

    std::string direction;
    unordered_map<int, Context<F, B> *> context_label_to_context_object_map;

    // mapping from context object to context label
    // mapping from function to  pair<inflow,outflow>
    // inflow and outflow are themselves pairs of forward and backward component
    // values. The forward and backward components are themselves pairs of G,L
    // dataflow values.
    bool debug{};
    bool SLIM{};
    float total_memory{}, vm{}, rss{};

    void printLine(int, int);

    BaseInstruction *current_instruction;

    // intraprocFlag : "true" : intraprocedural analysis; "false" :
    // interprocedural analysis;
    bool intraprocFlag = false;
    ExecMode modeOfExec = FSFP;
    bool ___e01 = false;
    std::unique_ptr<llvm::Module> moduleUniquePtr;
    std::chrono::milliseconds AnalysisTime, backward_time, forward_time;

#ifdef PRINTSTATS

    // std::chrono::milliseconds SLIMTime; // SplittingBBTime;
    //  std::chrono::milliseconds SplittingBBTime;
    //++++++++++++++++++++++++++++++++++++++++++++++++++++
    long F_BBanalysedCount = 0, B_BBanalysedCount = 0;
    long contextReuseCount = 0;
    long forwardsRoundCount = 0, backwardsRoundCount = 0;
    unordered_map<pair<int, BaseInstruction *>, pair<F, B>, HashFunction> all_onflow_information;
    // #BBCOUNTPERROUND
    std::map<long, long> mapforwardsRoundBB, mapbackwardsRoundBB;
#endif

protected:
    // List of contexts
    unordered_set<int> ProcedureContext;
    Worklist<pair<pair<int, BasicBlock *>, bool>, HashFunction> backward_worklist, forward_worklist;

    // mapping from (context label,call site) to target context label
    unordered_map<pair<int, llvm::Instruction *>, int, HashFunction> context_transition_graph;
    // unordered_map<pair<int, BaseInstruction *>, int, HashFunction>
    // SLIM_context_transition_graph;
    TransitionGraph SLIM_transition_graph;
    // std::unordered_map<int, pair<int, fetchLR *>>
    // Reverse_SLIM_context_transition_graph;

    // std::map<std::pair<llvm::Function *, llvm::BasicBlock *>, std::list<long
    // long>> funcBBInsMap; std::map<long long, BaseInstruction *>
    // globalInstrIndexList;

    unordered_map<pair<int, BaseInstruction *>, pair<pair<F, B>, pair<F, B>>, HashFunction> GenerativeKill;

    DumpPointsToInfo indirect_functions_obj;

public:
    Analysis(){};

    Analysis(std::unique_ptr<llvm::Module> &, bool, bool, bool, ExecMode, bool);

    Analysis(std::unique_ptr<llvm::Module> &, const string &, bool, bool, bool, ExecMode, bool);

    // Analysis(bool,bool);

    // Analysis(bool, const string &,bool);

    // Analysis(std::unique_ptr<llvm::Module>&, bool, bool, bool);

    // Analysis(std::unique_ptr<llvm::Module>&, const string &, bool, bool,
    // bool);

    ~Analysis();

    slim::IR *optIR;

    // void doAnalysis(Module &M);s
    void doAnalysis(std::unique_ptr<llvm::Module> &M, slim::IR *optIR);

    int getContextLabelCounter();

    void setContextLabelCounter(int);

    int getCurrentAnalysisDirection();

    void setCurrentAnalysisDirection(int);

    int getProcessingContextLabel() const;

    void setProcessingContextLabel(int);

    unsigned int getNumberOfContexts();

    bool isAnIgnorableDebugInstruction(llvm::Instruction *);
    bool isAnIgnorableDebugInstruction(BaseInstruction *);

    bool isIgnorableInstruction(BaseInstruction *);

    void INIT_CONTEXT(llvm::Function *, const std::pair<F, B> &, const std::pair<F, B> &);

    int check_if_context_already_exists(llvm::Function *, const pair<F, B> &, const pair<F, B> &);

    void doAnalysisForward();

    void doAnalysisBackward();

    F NormalFlowFunctionForward(pair<int, BasicBlock *>);

    B NormalFlowFunctionBackward(pair<int, BasicBlock *>);

    void startSplitting(std::unique_ptr<llvm::Module> &);

    void performSplittingBB(Function &f);

    void setCurrentModule(Module *);

    Module *getCurrentModule();

    void setCurrentInstruction(BaseInstruction *);

    BaseInstruction *getCurrentInstruction() const;

    F getForwardComponentAtInOfThisInstruction(llvm::Instruction &I);
    F getForwardComponentAtInOfThisInstruction(BaseInstruction *I);

    F getForwardComponentAtInOfThisInstruction(BaseInstruction *I,
                                               int label); // #########

    F getForwardComponentAtOutOfThisInstruction(llvm::Instruction &I);
    F getForwardComponentAtOutOfThisInstruction(BaseInstruction *I);

    F getForwardComponentAtOutOfThisInstruction(BaseInstruction *I,
                                                int label); // #########

    B getBackwardComponentAtInOfThisInstruction(llvm::Instruction &I);
    B getBackwardComponentAtInOfThisInstruction(BaseInstruction *I);

    B getBackwardComponentAtInOfThisInstruction(BaseInstruction *I,
                                                int label); // #########

    B getBackwardComponentAtOutOfThisInstruction(llvm::Instruction &I);
    B getBackwardComponentAtOutOfThisInstruction(BaseInstruction *I);

    B getBackwardComponentAtOutOfThisInstruction(BaseInstruction *I,
                                                 int label); // #########

    void setForwardComponentAtInOfThisInstruction(llvm::Instruction *I, const F &in_value);
    void setForwardComponentAtInOfThisInstruction(BaseInstruction *I, const F &in_value);

    void setBackwardComponentAtInOfThisInstruction(llvm::Instruction *I, const B &in_value);
    void setBackwardComponentAtInOfThisInstruction(BaseInstruction *I, const B &in_value);

    void setForwardComponentAtOutOfThisInstruction(llvm::Instruction *I, const F &out_value);
    void setForwardComponentAtOutOfThisInstruction(BaseInstruction *I, const F &out_value);

    void setBackwardComponentAtOutOfThisInstruction(llvm::Instruction *I, const B &out_value);
    void setBackwardComponentAtOutOfThisInstruction(BaseInstruction *I, const B &out_value);

    pair<F, B> getIn(int, llvm::BasicBlock *);

    pair<F, B> getOut(int, llvm::BasicBlock *);

    void setForwardIn(int, llvm::BasicBlock *, const F &);

    void setForwardOut(int, llvm::BasicBlock *, const F &);

    void setBackwardIn(int, llvm::BasicBlock *, const B &);

    void setBackwardOut(int, llvm::BasicBlock *, const B &);

    F getForwardInflowForThisContext(int);

    B getBackwardInflowForThisContext(int);

    F getForwardOutflowForThisContext(int);

    B getBackwardOutflowForThisContext(int);

    void setForwardInflowForThisContext(int, const F &);

    void setBackwardInflowForThisContext(int, const B &);

    void setForwardOutflowForThisContext(int, const F &);

    void setBackwardOutflowForThisContext(int, const B &);

    Function *getFunctionAssociatedWithThisContext(int);

    void printModule(Module &M);
    void printContext();
    void printInOutMaps();

    float getTotalMemory();
    void printStats();
    void printStatistics();
    void validation(bool);
    float MedianOfVector(vector<int> vec);
    int MaximumInVector(vector<int> vec);

    virtual pair<F, B>
    CallInflowFunction(int, Function *, BasicBlock *, const F &, const B &, pair<SLIMOperand *, SLIMOperand *>);

    /*  virtual pair<F, B> CallOutflowFunction(int, Function *, BasicBlock *,
                                             const F &, const B &, const F &,
                                             const B &,
                                             pair<SLIMOperand *, SLIMOperand
       *>);*/
    virtual pair<F, B> CallOutflowFunction(
        int, BaseInstruction *, Function *, BasicBlock *, const F &, const B &, const F &, const B &,
        pair<SLIMOperand *, SLIMOperand *>);

    virtual F computeOutFromIn(llvm::Instruction &I);
    virtual F computeOutFromIn(BaseInstruction *I);

    virtual F getBoundaryInformationForward(); //{}
    virtual F getInitialisationValueForward(); //{}
    // virtual F performMeetForward(const F& d1, const F& d2);//{}
    virtual F performMeetForward(const F &d1, const F &d2, const string pos);
    virtual F performMeetForward(const F &d1,
                                 const F &d2); // mod:AR display all values across all contexts
    virtual F performMeetForwardWithParameterMapping(const F &d1, const F &d2, const string pos);

    virtual bool EqualDataFlowValuesForward(const F &d1, const F &d2) const; //{}
    virtual bool EqualContextValuesForward(const F &d1, const F &d2) const;  //{}
    virtual F getPurelyLocalComponentForward(const F &dfv) const;
    virtual F getPurelyGlobalComponentForward(const F &dfv) const;
    virtual F getMixedComponentForward(const F &dfv) const;
    virtual F getCombinedValuesAtCallForward(const F &dfv1, const F &dfv2) const;
    virtual void printDataFlowValuesForward(const F &dfv) const {}

    virtual F performMergeAtCallForward(const F &d1, const F &d2); //{}
    virtual F getMainBoundaryInformationForward(BaseInstruction *I);
    virtual F getMainBoundaryInformationForward(BaseInstruction *I, slim::IR *slimIRptr);

    virtual B computeInFromOut(llvm::Instruction &I);
    virtual B computeInFromOut(BaseInstruction *I);

    virtual B getBoundaryInformationBackward();                               //{}
    virtual B getInitialisationValueBackward();                               //{}
    virtual B performMeetBackward(const B &d1, const B &d2) const;            //{}
    virtual bool EqualDataFlowValuesBackward(const B &d1, const B &d2) const; //{}
    virtual bool EqualContextValuesBackward(const B &d1, const B &d2) const;  //{}
    virtual B getPurelyLocalComponentBackward(const B &dfv) const;
    virtual B getPurelyGlobalComponentBackward(const B &dfv) const;
    virtual B getMixedComponentBackward(const B &dfv) const;
    virtual B getCombinedValuesAtCallBackward(const B &dfv1, const B &dfv2) const;
    virtual void printDataFlowValuesBackward(const B &dfv) const {}

    virtual B performMergeAtCallBackward(const B &d1, const B &d2) const;
    virtual B getMainBoundaryInformationBackward(BaseInstruction *I);

    virtual std::vector<Function *> getIndirectCalleeFromIN(long int, F &);
    virtual B getFPandArgsBackward(long int, Instruction *);
    virtual F getPinStartCallee(long int, Instruction *, F &, Function *);

    virtual unsigned int getSize(F &);
    virtual unsigned int getSize(B &);

    virtual void printFileDataFlowValuesForward(const F &dfv, std::ofstream &out) const;
    virtual void printFileDataFlowValuesBackward(const B &dfv, std::ofstream &out) const;

    virtual B functionCallEffectBackward(Function *, const B &);
    virtual F functionCallEffectForward(Function *, const F &);

    // #########
    //  int getCalleeContext(int callerContext, BaseInstruction *I);
    int getCalleeContext(int callerContext, BaseInstruction *I, Function *callee_fun);
    bool isGenerativeKillInitialised(int context_label, BaseInstruction *I);
    pair<pair<F, B>, pair<F, B>> getGenerativeKillAtCall(int context_label, BaseInstruction *I);
    void setGenerativeAtCall(int context_label, BaseInstruction *I, pair<F, B> generativeAtCall);
    void setKillAtCall(int context_label, BaseInstruction *I, pair<F, B> killAtCall);
    void setInitialisationValueGenerativeKill(int context_label, BaseInstruction *I);

    virtual pair<pair<F, B>, pair<F, B>> getInitialisationValueGenerativeKill();
    virtual pair<F, B> computeGenerative(int context_label, Function *Func);
    virtual pair<F, B> performMeetGenerative(const pair<F, B> &g1, const pair<F, B> &g2);
    virtual pair<F, B> computeKill(int context_label, Function *Func);
    virtual pair<F, B> CallInflowFunction(
        int context_label, Function *Func, BasicBlock *bb, const F &a1, const B &d1, pair<F, B> generativeAtCall,
        pair<SLIMOperand *, SLIMOperand *> return_operand_map);
    virtual pair<F, B> CallOnflowFunction(
        int context_label, BaseInstruction *I, pair<F, B> killAtCall,
        pair<SLIMOperand *, SLIMOperand *> return_operand_map);
    virtual pair<F, B> CallInflowFunction(
        int context_label, Function *Func, BasicBlock *bb, const F &a1, const B &d1, int source_context_label,
        pair<SLIMOperand *, SLIMOperand *> return_operand_map);
    virtual pair<F, B> CallOnflowFunction(
        int context_label, BaseInstruction *I, int source_context_label,
        pair<SLIMOperand *, SLIMOperand *> return_operand_map);
    virtual pair<F, B> CallOnflowFunctionForIndirect(
        int context_label, BaseInstruction *I, int source_context_label,
        pair<SLIMOperand *, SLIMOperand *> return_operand_map, Function *target_function);

    // For Flow-insensitive analysis
    void INIT_CONTEXT_FIS(llvm::Function *, const F &, const F &);
    void doAnalysisFIS(int context_label, Function *func);
    F NormalFlowFunctionFIS(pair<int, BasicBlock *>);

    void setDFValueFIS(int, Function *, const F &);
    F getDFValueFIS(int, Function *);

    virtual F getBoundaryInformationFIS();
    virtual F getInitialisationValueFIS();
    virtual void printDataFlowValuesFIS(const F &dfv) const {}
    virtual bool EqualDataFlowValuesFIS(const F &d1, const F &d2) const;
    virtual F computeDFValueFIS(llvm::Instruction &I);
    virtual F computeDFValueFIS(BaseInstruction *I);
    virtual F performMeetFIS(const F &d1, const F &d2);
    virtual F getPurelyLocalComponentFIS(const F &dfv) const;
    virtual F getPurelyGlobalComponentFIS(const F &dfv) const;
    virtual F CallInflowFunctionFIS(int, Function *, BasicBlock *, const F &, pair<SLIMOperand *, SLIMOperand *>);
    virtual F
    CallOutflowFunctionFIS(int, Function *, BasicBlock *, const F &, const F &, pair<SLIMOperand *, SLIMOperand *>);
    virtual F performMergeAtCallFIS(const F &d1, const F &d2);

    void doAnalysis(std::unique_ptr<llvm::Module> &M, slim::IR *optIR, Function *Func);

    virtual F performCallReturnArgEffectForward(
        const F &dfv, const std::pair<SLIMOperand *, SLIMOperand *> return_operand_map) const;
    virtual B performCallReturnArgEffectBackward(
        const B &dfv, const std::pair<SLIMOperand *, SLIMOperand *> return_operand_map) const;
    virtual F
    performCallReturnArgEffectFIS(const F &dfv, const std::pair<SLIMOperand *, SLIMOperand *> return_operand_map) const;

    virtual F getGlobalandFunctionArgComponentForward(const F &dfv) const;
    virtual B getGlobalandFunctionArgComponentBackward(const B &dfv) const;
    virtual F getGlobalandFunctionArgComponentFIS(const F &dfv) const;
    virtual bool isFunctionIgnored(llvm::Function *Func);

    virtual set<BasicBlock *> getMixedFISBasicBlocksBackward();
    virtual set<BasicBlock *> getMixedFISBasicBlocksForward();
    vector<int> getAllContextsofFunction(Function *func);
    // #########

    // Temp added for testing
    // BaseInstruction * getBaseInstruction(long long id);
    // BaseInstruction * getBaseIns(long long id, SLIM);
    void setSLIMIRPointer(slim::IR *slimIRptr);

    // ----------------------------------------------
    // ----------------------------------------------
    pair<F, B> getInflowforContext(int);
    B getBackwardIn(int, llvm::BasicBlock *);
    B getBackwardOut(int, llvm::BasicBlock *);
    F getForwardIn(int, llvm::BasicBlock *);
    F getForwardOut(int, llvm::BasicBlock *);
    B getBackwardComponentAtInOfThisInstruction(llvm::Instruction *I, int label);
    B getBackwardComponentAtOutOfThisInstruction(llvm::Instruction *I, int label);
    F getForwardComponentAtInOfThisInstruction(llvm::Instruction *I, int label);
    F getForwardComponentAtOutOfThisInstruction(llvm::Instruction *I, int label);
    // mod:AR
    F getAllForwardComponentAtInOfThisInstruction(Function *, BaseInstruction *);
    F getAllForwardComponentAtOutOfThisInstruction(Function *, BaseInstruction *);
    B getAllBackwardComponentAtInOfThisInstruction(Function *, BaseInstruction *);
    B getAllBackwardComponentAtOutOfThisInstruction(Function *, BaseInstruction *);
    void setAllForwardComponentAtInOfThisInstruction(Function *, BaseInstruction *I, const F &in_value);
    void setAllForwardComponentAtOutOfThisInstruction(Function *, BaseInstruction *I, const F &out_value);
    void setAllBackwardComponentAtInOfThisInstruction(Function *, BaseInstruction *I, const B &in_value);
    void setAllBackwardComponentAtOutOfThisInstruction(Function *, BaseInstruction *I, const B &out_value);
    void insertIntoBackwardsBBList(BasicBlock *);
    void insertIntoForwardsBBList(BasicBlock *);
    void printBasicBlockLists();
    //---
    void INIT_CONTEXT(llvm::Function *, const std::pair<F, B> &, const std::pair<F, B> &, int source_context_label);
    int getCalleeContext(int callerContext, llvm::Instruction *I);

    virtual set<Function *>
    handleFunctionPointers(F Pin, SLIMOperand *function_operand, int current_context, BaseInstruction *I);
    virtual pair<F, B>
    performParameterMapping(BaseInstruction *I, Function *target_function, F a1, B LinStart_at_targetfunction);
    virtual B computeInFromOutForIndirectCalls(BaseInstruction *I);
    virtual bool isBlockChangedForForward(int context_label, BaseInstruction *I);

    string getFileName(llvm::Instruction *I);
    string getFileName(BaseInstruction *I);
    unsigned getSourceLineNumberForInstruction(llvm::Instruction *I);
    unsigned getSourceLineNumberForInstruction(BaseInstruction *I);
    set<Function *> getIndirectFunctionsAtCallSite(BaseInstruction *I);
    Function *getFunctionWithName(string fun_name);
    bool isUseInstruction(BaseInstruction *I);
    int getSourceContextAtCallSiteForFunction(int current_context_label, BaseInstruction *I, Function *target_function);
    std::vector<SLIMOperand *> getGlobalListFromSLIMIR();
    // mod:AR
    virtual void setForwardINForNonEntryBasicBlock(pair<int, BasicBlock *> current_pair);
    virtual std::set<BasicBlock *> manageWorklistForward(B, BasicBlock *, Function *);
    BasicBlock *fetchExitBlockofFunction(Function *);
};

template <class F, class B>
BasicBlock *Analysis<F, B>::fetchExitBlockofFunction(Function *func) {
    Function &f = *func;
    for (auto &BB : f) {
        // BasicBlock &b = BB;
        Instruction *terminator = BB.getTerminator();
        // Check for return instructions
        if (isa<ReturnInst>(terminator)) {
            /* llvm::outs() << "\n TERMINATOR INSTRUCTION: ";
             llvm::outs() << terminator << "\n";*/
            return &BB;
        }
    }
}

template <class F, class B>
void Analysis<F, B>::setForwardINForNonEntryBasicBlock(pair<int, BasicBlock *> current_pair) {
    llvm::outs() << "\n This function setForwardINForNonEntryBasicBlock() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
std::set<BasicBlock *> Analysis<F, B>::manageWorklistForward(B LOUT_BB, BasicBlock *currBB, Function *function) {
    llvm::outs() << "\n This function manageWorklistForward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
void Analysis<F, B>::setSLIMIRPointer(slim::IR *slimIRptr) {
    this->optIR = slimIRptr;
}

/*
template<class F, class B>
BaseInstruction * Analysis<F,B>::getBaseInstruction(long long id){
    BaseInstruction * ins = optIR->getInstrFromIndex(id);
    return ins;
}*/

// #########
template <class F, class B>
F Analysis<F, B>::getForwardComponentAtInOfThisInstruction(BaseInstruction *I, int label) {
    // int label = getProcessingContextLabel();
    return SLIM_IN[label][I].first;
}

// mod:AR
template <class F, class B>
void Analysis<F, B>::insertIntoBackwardsBBList(BasicBlock *bb) {
    backwardsBBList.push_back(bb);
}

template <class F, class B>
void Analysis<F, B>::insertIntoForwardsBBList(BasicBlock *bb) {
    forwardsBBList.push_back(bb);
}

template <class F, class B>
void Analysis<F, B>::printBasicBlockLists() {
    llvm::outs() << "\n Displaying basic blocks in Backwards Worklist:\n";
    for (auto basic_block : backwardsBBList) {
        llvm::outs() << basic_block->getName() << ", ";
    }
    llvm::outs() << "\n\n Displaying basic blocks in Forwards Worklist:\n";
    for (auto basic_block : forwardsBBList) {
        llvm::outs() << basic_block->getName() << ", ";
    }
}

template <class F, class B>
F Analysis<F, B>::getAllForwardComponentAtInOfThisInstruction(Function *function, BaseInstruction *I) {
    return SLIM_IN_ALL[function][I].first;
}

template <class F, class B>
F Analysis<F, B>::getAllForwardComponentAtOutOfThisInstruction(Function *function, BaseInstruction *I) {
    return SLIM_OUT_ALL[function][I].first;
}

template <class F, class B>
B Analysis<F, B>::getAllBackwardComponentAtInOfThisInstruction(Function *function, BaseInstruction *I) {
    return SLIM_IN_ALL[function][I].second;
}

template <class F, class B>
B Analysis<F, B>::getAllBackwardComponentAtOutOfThisInstruction(Function *function, BaseInstruction *I) {
    return SLIM_OUT_ALL[function][I].second;
}

template <class F, class B>
void Analysis<F, B>::setAllForwardComponentAtInOfThisInstruction(
    Function *function, BaseInstruction *I, const F &in_value) {
    SLIM_IN_ALL[function][I].first = in_value;
}

template <class F, class B>
void Analysis<F, B>::setAllForwardComponentAtOutOfThisInstruction(
    Function *function, BaseInstruction *I, const F &out_value) {
    SLIM_OUT_ALL[function][I].first = out_value;
}

template <class F, class B>
void Analysis<F, B>::setAllBackwardComponentAtInOfThisInstruction(
    Function *function, BaseInstruction *I, const B &in_value) {
    SLIM_IN_ALL[function][I].second = in_value;
}

template <class F, class B>
void Analysis<F, B>::setAllBackwardComponentAtOutOfThisInstruction(
    Function *function, BaseInstruction *I, const B &out_value) {
    SLIM_OUT_ALL[function][I].second = out_value;
}

//-----------
template <class F, class B>
F Analysis<F, B>::getForwardComponentAtOutOfThisInstruction(BaseInstruction *I, int label) {
    // int label = getProcessingContextLabel();
    return SLIM_OUT[label][I].first;
}

template <class F, class B>
B Analysis<F, B>::getBackwardComponentAtInOfThisInstruction(BaseInstruction *I, int label) {
    // int label = getProcessingContextLabel();
    return SLIM_IN[label][I].second;
}

template <class F, class B>
B Analysis<F, B>::getBackwardComponentAtOutOfThisInstruction(BaseInstruction *I, int label) {
    // int label = getProcessingContextLabel();
    return SLIM_OUT[label][I].second;
}

template <class F, class B>
int Analysis<F, B>::getCalleeContext(int callerContext, BaseInstruction *I, Function *callee_fun) {
    return SLIM_transition_graph.getCalleeContext(callerContext, I, callee_fun);
}

template <class F, class B>
bool Analysis<F, B>::isGenerativeKillInitialised(int context_label, BaseInstruction *I) {
    pair<int, BaseInstruction *> keyGenerativeKill = make_pair(context_label, I);
    if (GenerativeKill.find(keyGenerativeKill) != GenerativeKill.end()) {
        // key is present, thus initialised, so return true
        return true;
    }
    return false;
}

template <class F, class B>
pair<pair<F, B>, pair<F, B>> Analysis<F, B>::getGenerativeKillAtCall(int context_label, BaseInstruction *I) {
    pair<int, BaseInstruction *> keyGenerativeKill = make_pair(context_label, I);
    // TODO what to do if not present?
    return GenerativeKill[keyGenerativeKill];
}

template <class F, class B>
void Analysis<F, B>::setGenerativeAtCall(int context_label, BaseInstruction *I, pair<F, B> generativeAtCall) {
    pair<int, BaseInstruction *> keyGenerativeKill = make_pair(context_label, I);
    // setting existing value of Kill and new value of Generative
    pair<pair<F, B>, pair<F, B>> valueGenerativeKill =
        make_pair(generativeAtCall, getGenerativeKillAtCall(context_label, I).second);
    GenerativeKill[keyGenerativeKill] = valueGenerativeKill;
}

template <class F, class B>
void Analysis<F, B>::setKillAtCall(int context_label, BaseInstruction *I, pair<F, B> killAtCall) {
    pair<int, BaseInstruction *> keyGenerativeKill = make_pair(context_label, I);
    // setting existing value of Generative and new value of Kill
    pair<pair<F, B>, pair<F, B>> valueGenerativeKill =
        make_pair(getGenerativeKillAtCall(context_label, I).first, killAtCall);
    GenerativeKill[keyGenerativeKill] = valueGenerativeKill;
}

template <class F, class B>
void Analysis<F, B>::setInitialisationValueGenerativeKill(int context_label, BaseInstruction *I) {
    pair<pair<F, B>, pair<F, B>> initGenerativeKill = getInitialisationValueGenerativeKill();
    pair<int, BaseInstruction *> keyGenerativeKill = make_pair(context_label, I);
    GenerativeKill[keyGenerativeKill] = initGenerativeKill;
}

template <class F, class B>
vector<int> Analysis<F, B>::getAllContextsofFunction(Function *func) {
    vector<int> labels;
    for (auto itr : context_label_to_context_object_map) {
        if (itr.second->getFunction() == func)
            labels.push_back(itr.first);
    }
    return labels;
}

//--virtual functions--
template <class F, class B>
pair<pair<F, B>, pair<F, B>> Analysis<F, B>::getInitialisationValueGenerativeKill() {
    llvm::outs() << "\nThis function getInitialisationValueGenerativeKill() has "
                    "not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> Analysis<F, B>::computeGenerative(int context_label, Function *Func) {
    llvm::outs() << "\nThis function computeGenerative() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> Analysis<F, B>::performMeetGenerative(const pair<F, B> &g1, const pair<F, B> &g2) {
    llvm::outs() << "\nThis function performMeetGenerative() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> Analysis<F, B>::computeKill(int context_label, Function *Func) {
    llvm::outs() << "\nThis function computeKill() has not been implemented. "
                    "EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> Analysis<F, B>::CallInflowFunction(
    int context_label, Function *Func, BasicBlock *bb, const F &a1, const B &d1, pair<F, B> generativeAtCall,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    llvm::outs() << "\nThis function CallInflowFunction() with Generative has "
                    "not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> Analysis<F, B>::CallOnflowFunction(
    int context_label, BaseInstruction *I, pair<F, B> killAtCall,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    llvm::outs() << "\nThis function CallOnflowFunction() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getBoundaryInformationFIS() {
    llvm::outs() << "\nThis function getBoundaryInformationFIS() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getInitialisationValueFIS() {
    llvm::outs() << "\nThis function getInitialisationValueFIS() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
bool Analysis<F, B>::EqualDataFlowValuesFIS(const F &d1, const F &d2) const {
    llvm::outs() << "\nThis function EqualDataFlowValuesFIS() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::computeDFValueFIS(llvm::Instruction &I) {
    llvm::outs() << "\nThis function computeDFValueFIS() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::computeDFValueFIS(BaseInstruction *I) {
    llvm::outs() << "\nThis function computeDFValueFIS() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::performMeetFIS(const F &d1, const F &d2) {
    llvm::outs() << "\nThis function performMeetFIS() has not been implemented. "
                    "EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getPurelyGlobalComponentFIS(const F &dfv) const {
    llvm::outs() << "\nThis function getPurelyGlobalComponentFIS() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getPurelyLocalComponentFIS(const F &dfv) const {
    llvm::outs() << "\nThis function getPurelyLocalComponentFIS() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::CallInflowFunctionFIS(
    int context_label, Function *target_function, BasicBlock *bb, const F &a1,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    llvm::outs() << "\nThis function CallInflowFunctionFIS() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::CallOutflowFunctionFIS(
    int context_label, Function *target_function, BasicBlock *bb, const F &a3, const F &a1,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    llvm::outs() << "\nThis function CallOutflowFunctionFIS() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::performMergeAtCallFIS(const F &d1, const F &d2) {
    llvm::outs() << "\nThis function performMergeAtCallFIS() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::performCallReturnArgEffectForward(
    const F &dfv, std::pair<SLIMOperand *, SLIMOperand *> return_operand_map) const {
    llvm::outs() << "\nThis function performCallReturnArgEffectForward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::performCallReturnArgEffectBackward(
    const B &dfv, std::pair<SLIMOperand *, SLIMOperand *> return_operand_map) const {
    llvm::outs() << "\nThis function performCallReturnArgEffectBackward() has "
                    "not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::performCallReturnArgEffectFIS(
    const F &dfv, std::pair<SLIMOperand *, SLIMOperand *> return_operand_map) const {
    llvm::outs() << "\nThis function performCallReturnArgEffectFIS() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getGlobalandFunctionArgComponentForward(const F &dfv) const {
    llvm::outs() << "\nThis function getGlobalandFunctionArgComponentForward() "
                    "has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::getGlobalandFunctionArgComponentBackward(const B &dfv) const {
    llvm::outs() << "\nThis function getGlobalandFunctionArgComponentBackward() "
                    "has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getGlobalandFunctionArgComponentFIS(const F &dfv) const {
    llvm::outs() << "\nThis function getGlobalandFunctionArgComponentFIS() has "
                    "not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
bool Analysis<F, B>::isFunctionIgnored(llvm::Function *Func) {
    llvm::outs() << "\nThis function isFunctionIgnored() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
set<BasicBlock *> Analysis<F, B>::getMixedFISBasicBlocksBackward() {
    llvm::outs() << "\nThis function getMixedFISBasicBlocksBackward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
set<BasicBlock *> Analysis<F, B>::getMixedFISBasicBlocksForward() {
    llvm::outs() << "\nThis function getMixedFISBasicBlocksForward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
set<Function *>
Analysis<F, B>::handleFunctionPointers(F Pin, SLIMOperand *function_operand, int current_context, BaseInstruction *I) {
    llvm::outs() << "\nThis function handleFunctionPointers() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> Analysis<F, B>::performParameterMapping(
    BaseInstruction *I, Function *target_function, F a1, B LinStart_at_targetfunction) {
    llvm::outs() << "\nThis function performParameterMapping() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::computeInFromOutForIndirectCalls(BaseInstruction *I) {
    llvm::outs() << "\nThis function computeInFromOutForIndirectCalls() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
bool Analysis<F, B>::isBlockChangedForForward(int context_label, BaseInstruction *I) {
    llvm::outs() << "\nThis function isBlockChangedForForward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
void Analysis<F, B>::doAnalysis(std::unique_ptr<llvm::Module> &Mod, slim::IR *slimIRObj, Function *Func) {
    llvm::outs() << "\n In doAnalysis 1....";
    llvm::Module &M = *Mod;
    setCurrentModule(&M);

    /*
    //====================================SPLITTING========================================
    auto start = high_resolution_clock::now();
    startSplitting();
    auto stop = high_resolution_clock::now();
    this->SplittingBBTime = duration_cast<milliseconds>(stop - start);

    //Generate SLIM IR
    slim::IR *transformIR = new slim::IR(Mod);
    //generateIR(Mod);
    optIR = transformIR->optimizeIR();
    */

    // SLIM IR object passed by client is set in the data member optIR
    if (SLIM) {
        setSLIMIRPointer(slimIRObj);
    }

    // llvm::outs() << "Inside doAnalysis with SLIM parameter as " << SLIM <<
    // "\n";

    auto start = high_resolution_clock::now();

    int i = 0;

    if (modeOfExec == FSFP || modeOfExec == FSSP) {

        F forward_inflow_bi;
        B backward_inflow_bi;
        F forward_outflow_bi;
        B backward_outflow_bi;
        // Function *fptr = &function;
        if (std::is_same<F, NoAnalysisType>::value) {
            backward_inflow_bi = getBoundaryInformationBackward();
        } else if (std::is_same<B, NoAnalysisType>::value) {
            forward_inflow_bi = getBoundaryInformationForward();
        } else {
            backward_inflow_bi = getBoundaryInformationBackward();
            forward_inflow_bi = getBoundaryInformationForward();
        }
        setCurrentAnalysisDirection(0);
        INIT_CONTEXT(Func, {forward_inflow_bi, backward_inflow_bi}, {forward_outflow_bi, backward_outflow_bi});

        if (std::is_same<F, NoAnalysisType>::value) {
            // backward analysis
            direction = "backward";
            setCurrentAnalysisDirection(2);
            while (not backward_worklist.empty()) {
                doAnalysisBackward();
            }
        } else if (std::is_same<B, NoAnalysisType>::value) {
            // forward analysis
            direction = "forward";
            setCurrentAnalysisDirection(1);
            while (not forward_worklist.empty()) {
                doAnalysisForward();
            }
        } else {
            direction = "bidirectional";
            int fi = 1, bi = 1;
            int iteration = 1;
            while (not forward_worklist.empty() || not backward_worklist.empty()) {
                // current_analysis_direction=2;
                setCurrentAnalysisDirection(2);
                while (not backward_worklist.empty()) {
                    doAnalysisBackward();
                }
                // current_analysis_direction=1;
                setCurrentAnalysisDirection(1);
                while (not forward_worklist.empty()) {
                    doAnalysisForward();
                }
                iteration++;
            }

// After the dataflow values reach fixed point for flow-sensitive components,
// check if any blocks need to be analysed for flow-insensitive components
#ifdef COMPUTE_CIS_FIS
            // do{   TODO  TODO  TODO
            llvm::outs() << "\n Inside COMPUTE_CIS_FIS_BLOCKS\n";
//}while(not forward_worklist.empty() || not backward_worklist.empty());
#endif

#ifdef COMPUTE_CS_FIS
            llvm::outs() << "\n Inside COMPUTE_CS_FIS_BLOCKS\n";
#endif
        }
    } else {
        // FISP or FIFP
        F flow_insensitive_inflow;
        F flow_insensitive_outflow;
        if (std::is_same<B, NoAnalysisType>::value) {
            flow_insensitive_inflow = getBoundaryInformationFIS();
        }
        setCurrentAnalysisDirection(0);
        INIT_CONTEXT_FIS(Func, flow_insensitive_inflow, flow_insensitive_outflow);
        if (std::is_same<B, NoAnalysisType>::value) {
            // forward analysis
            direction = "forward";
            setCurrentAnalysisDirection(1);
            // while (not forward_worklist.empty()) {
            doAnalysisFIS(getProcessingContextLabel(), Func);
            //}
        }
    }

    auto stop = high_resolution_clock::now();
    this->AnalysisTime = duration_cast<milliseconds>(stop - start);
}

template <class F, class B>
void Analysis<F, B>::INIT_CONTEXT_FIS(llvm::Function *function, const F &InflowFIS, const F &OutflowFIS) {
    // llvm::outs() << "\n Inside INIT_CONTEXT_FIS..............";
    B tempB;
    std::pair<F, B> Inflow = make_pair(InflowFIS, tempB);
    std::pair<F, B> Outflow = make_pair(OutflowFIS, tempB);
    context_label_counter++;
    Context<F, B> *context_object = new Context<F, B>(context_label_counter, function, Inflow, Outflow);
    int current_context_label = context_object->getLabel();
    setProcessingContextLabel(current_context_label);
    if (std::is_same<B, NoAnalysisType>::value) {
        if (debug) {
            llvm::outs() << "INITIALIZING CONTEXT 1:-" << "\n";
            llvm::outs() << "LABEL: " << context_object->getLabel() << "\n";
            llvm::outs() << "FUNCTION: " << function->getName() << "\n";
            llvm::outs() << "Inflow Value: ";
            printDataFlowValuesFIS(Inflow.first);
            llvm::outs() << "Outflow value: ";
            printDataFlowValuesFIS(getInitialisationValueFIS());
        }
        // forward analysis
        context_label_to_context_object_map[current_context_label] = context_object;

        setForwardOutflowForThisContext(current_context_label,
                                        getInitialisationValueFIS()); // setting outflow forward
        ProcedureContext.insert(current_context_label);

        setDFValueFIS(current_context_label, function, getInitialisationValueFIS());
        for (BasicBlock *BB : post_order(&context_object->getFunction()->getEntryBlock())) {
            BasicBlock &b = *BB;
            forward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));

            // initialise IN-OUT maps for every instruction
            // Since flow insensitive, this step is not required
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
        if (current_context_label == 0) // main function with first invocation
        {
            setForwardInflowForThisContext(
                current_context_label,
                getBoundaryInformationFIS()); // setting inflow forward
        } else {
            setForwardInflowForThisContext(current_context_label, context_object->getInflowValue().first);
        }
        setDFValueFIS(current_context_label, function, getForwardInflowForThisContext(current_context_label));
    }
}

template <class F, class B>
void Analysis<F, B>::doAnalysisFIS(int context_label, Function *func) {
    llvm::outs() << "\n In doAnalysis 2....";
    bool changed = true;
    while (changed) {
        F previous_dfvalue_of_this_function = getDFValueFIS(context_label, func);
        while (not forward_worklist.empty()) // step 2
        {
            // step 3 and 4
            pair<pair<int, BasicBlock *>, bool> worklistElement = forward_worklist.workDelete();
            pair<int, BasicBlock *> current_pair = worklistElement.first;
            int current_context_label = current_pair.first;
            BasicBlock *bb = current_pair.second;
            bool calleeAnalysed = worklistElement.second;
            setProcessingContextLabel(current_context_label);

#ifdef PRVASCO
            llvm::outs() << "\n\n";
            llvm::outs() << "Popping from Flow Insensitive Worklist (Context:" << current_context_label
                         << " calleeAnalysed: " << calleeAnalysed << ")\n";
            for (Instruction &I : *bb)
                llvm::outs() << I << "\n";
#endif
            BasicBlock &b = *bb;
            Function *f = context_label_to_context_object_map[current_context_label]->getFunction();
            Function &function = *f;

            // step 5
            /*if (bb != (&function.getEntryBlock())) {
                //step 6
                setForwardIn(current_pair.first, current_pair.second,
            getInitialisationValueForward());

                //step 7 and 8
                for (auto pred_bb:predecessors(bb)) {
                    llvm::outs()<<"\n";
                    //get first instruction of bb and
            setCurrentInstruction(inst); BaseInstruction *firstInst =
            optIR->inst_id_to_object[optIR->getFirstIns(bb->getParent(),bb)];
                    setCurrentInstruction(firstInst);

                    printDataFlowValuesForward(getIn(current_pair.first,
            current_pair.second).first);
                    //llvm::outs() << "ISues in perform meet\n";
                    setForwardIn(current_pair.first, current_pair.second,
                                performMeetForward(getIn(current_pair.first,
            current_pair.second).first, getOut(current_pair.first,
            pred_bb).first, "IN")); //
            CS_BB_OUT[make_pair(current_pair.first,pred_bb)].first
                }
            } else {
                //In value of this node is same as INFLOW value
                setForwardIn(current_pair.first, current_pair.second,
                            getForwardInflowForThisContext(current_context_label));
            }*/

            // step 9 //TODO check its use
            // F a1 = getIn(current_pair.first, current_pair.second).first;
            // B d1 = getOut(current_pair.first, current_pair.second).second;

#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Previous Dataflow Value is ";
            printDataFlowValuesFIS(previous_dfvalue_of_this_function);
            llvm::outs() << "\n";
#endif

            bool contains_a_method_call = false;
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{f, bb}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    if (inst->getCall()) { // getCall() present in new SLIM
                        Instruction *Inst = inst->getLLVMInstruction();
                        /*CallInst *ci = dyn_cast<CallInst>(Inst);
                        Function *target_function = ci->getCalledFunction();*/
                        // Changed to fetch target (Callee) function from SLIM
                        // IR instead of LLVM IR
                        CallInstruction *cTempInst = new CallInstruction(Inst);
                        // if(cTempInst->isIndirectCall())
                        //     continue;
                        Function *target_function = cTempInst->getCalleeFunction();
                        if (target_function &&
                            (target_function->isDeclaration() || isAnIgnorableDebugInstruction(Inst))) {
                            continue; // this is an inbuilt function so doesn't
                                      // need to be processed.
                        }
                        contains_a_method_call = true;
                    }
                }
            } else {
                // step 10
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    Instruction &I = *inst;
                    if (CallInst *ci = dyn_cast<CallInst>(&I)) {
                        Function *target_function = ci->getCalledFunction();
                        if (not target_function || target_function->isDeclaration() ||
                            isAnIgnorableDebugInstruction(&I)) {
                            continue; // this is an inbuilt function so doesn't
                                      // need to be processed.
                        }
                        contains_a_method_call = true;
                        break;
                    }
                }
            }
            if (contains_a_method_call) {
                if (SLIM) {
                    F prevdfval = getDFValueFIS(context_label, func);
                    for (auto &index : optIR->func_bb_to_inst_id[{f, bb}]) {
                        auto &inst = optIR->inst_id_to_object[index];
                        setCurrentInstruction(inst);
                        if (inst->getCall()) {
                            Instruction *Inst = inst->getLLVMInstruction();
                            /*CallInst *ci = dyn_cast<CallInst>(Inst);
                            Function *target_function = ci->getCalledFunction();
                          */
                            // Changed to fetch target (Callee) function from
                            // SLIM IR instead of LLVM IR
                            CallInstruction *cTempInst = new CallInstruction(Inst);

                            if (cTempInst->isIndirectCall()) {
                                if (debug)
                                    llvm::outs() << "\nINDIRET CALL DETECTED within "
                                                    "doAnalysiFIS:contains "
                                                    "a method call\nSRI!! Everything "
                                                    "needed for indirect "
                                                    "call needs to be implemented.";
                            }

                            Function *target_function = cTempInst->getCalleeFunction();
                            if (not target_function || target_function->isDeclaration() ||
                                isAnIgnorableDebugInstruction(Inst)) {
                                continue; // this is an inbuilt function so
                                          // doesn't need to be processed.
                            }

                            // get the return variable in callee to map it with
                            // the variable in caller Example: z = call Q() and
                            // defintion of Q(){ ... return x;} , pass pair <z,
                            // x> to compute Inflow/Outflow
                            SLIMOperand *returnInCaller = nullptr;
                            SLIMOperand *returnInCallee = nullptr;
                            std::pair<SLIMOperand *, int> callerRetOperand = inst->getLHS();
                            if (callerRetOperand.second >= 0) {
                                SLIMOperand *calleeRet = callerRetOperand.first->getCalleeReturnOperand();
                                if (!(calleeRet == nullptr || calleeRet->getOperandType() == NULL_OPERAND)) {
                                    returnInCaller = callerRetOperand.first;
                                    returnInCallee = calleeRet;
                                }
                                /*else
                                    llvm::outs() << "Return VOID callee\n";*/
                            } else {
                                // llvm::outs() << "Return VOID\n";
                            }
                            std::pair<SLIMOperand *, SLIMOperand *> return_operand_map =
                                make_pair(returnInCaller, returnInCallee);
                            /*
                            if(return_operand_map.first!=nullptr)
                                llvm::outs() << "return_operand_map.first IS NOT
                            NULL\n"; else llvm::outs() <<
                            "return_operand_map.first IS NULL\n";
                            if(return_operand_map.second!=nullptr)
                                llvm::outs() << "return_operand_map.second IS
                            NOT NULL\n"; else llvm::outs() <<
                            "return_operand_map.second IS NULL\n";
                            */
                            // if(intraprocFlag ||
                            // isFunctionIgnored(target_function)){
                            if (intraprocFlag) {
                                // TODO
                            } else {
                                F inflow;
                                if (___e01) {
                                    // TODO
                                } else {
                                    inflow = CallInflowFunctionFIS(
                                        current_context_label, target_function, bb, prevdfval, return_operand_map);
                                }

                                // step 12
                                F a2 = inflow;

                                F new_outflow_fis;
                                B dummyB;

                                int matching_context_label = 0;
                                matching_context_label = check_if_context_already_exists(
                                    target_function, {inflow, dummyB}, make_pair(new_outflow_fis, dummyB));

                                if (matching_context_label > 0) {
                                    if (debug) {
                                        printLine(current_context_label, 0);
                                        llvm::outs() << "\nProcessing instruction at "
                                                        "INDEX = "
                                                     << index;
                                        inst->printInstruction();
                                        llvm::outs() << "\nDataflow value: ";
                                    }

                                    // step 14
                                    pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                    // SRI-- Commented
                                    // SLIM_context_transition_graph[mypair] =
                                    // matching_context_label;
                                    SLIM_transition_graph.addEdge(mypair, matching_context_label, target_function);
#ifdef PRVASCO
                                    llvm::outs() << "\nUpdating context transition "
                                                    "graph (c"
                                                 << current_context_label << ", i" << index << ") -->"
                                                 << matching_context_label << "\n";
#endif

                                    // step 16 and 17
                                    F a3 = getForwardOutflowForThisContext(matching_context_label);
                                    B d3 = getBackwardOutflowForThisContext(matching_context_label);

                                    F outflow = CallOutflowFunctionFIS(
                                        current_context_label, target_function, bb, a3, prevdfval, return_operand_map);

                                    F outflowVal = outflow;

                                    if (debug) {
                                        llvm::outs() << "\nThe outflow value obtained "
                                                        "for function "
                                                     << target_function->getName() << " for context "
                                                     << matching_context_label << " is ";
                                        printDataFlowValuesFIS(a3);
                                        llvm::outs() << "\nOutflow after "
                                                        "CallOutflowFunctionFIS ";
                                        printDataFlowValuesFIS(outflowVal);
                                        llvm::outs() << "\n";
                                    }
                                    // B d4 = outflow_pair.second;

                                    // step 18 and 19

                                    // At the call instruction, the value at IN
                                    // should be splitted into two components.
                                    // The purely global component is given to
                                    // the callee while the mixed component is
                                    // propagated to OUT of this instruction
                                    // after executing computeOUTfromIN() on it.

                                    // #########
                                    F a5;
                                    if (___e01) {
                                        // TODO
                                    } else {
                                        a5 = getPurelyLocalComponentFIS(prevdfval);
                                        if (returnInCallee != nullptr)
                                            a5 = performCallReturnArgEffectFIS(a5, return_operand_map);
                                    }

                                    F dfval = performMeetFIS(prevdfval, performMergeAtCallFIS(outflowVal, a5));
                                    setDFValueFIS(current_pair.first, f, dfval);
                                    prevdfval = dfval;
                                    if (debug) {
                                        printDataFlowValuesFIS(prevdfval);
                                        printLine(current_context_label, 1);
                                    }
                                } else {
                                    // create new context
                                    INIT_CONTEXT_FIS(target_function, a2, new_outflow_fis);
                                    // step 14
                                    // This step must be done after above step
                                    // because context label counter gets
                                    // updated after INIT-Context is invoked.
                                    pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                    // SRI-- Commented
                                    // SLIM_context_transition_graph[mypair] =
                                    // getContextLabelCounter();
                                    SLIM_transition_graph.addEdge(mypair, getContextLabelCounter(), target_function);
#ifdef PRVASCO
                                    llvm::outs() << "\nUpdating context transition "
                                                    "graph (c"
                                                 << current_context_label << ", i" << index << ") -->"
                                                 << getContextLabelCounter() << "\n";
#endif
                                }
                            }
                        } else {
                            // not call instruction
                            if (debug) {
                                printLine(current_context_label, 0);
                                llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                                inst->printInstruction();
                                llvm::outs() << "\nDataflow value: ";
                            }

                            F newdfval = computeDFValueFIS(inst);
                            F dfval = performMeetFIS(prevdfval, newdfval);
                            setDFValueFIS(current_pair.first, f, dfval);
                            prevdfval = dfval;
                            if (debug) {
                                printDataFlowValuesFIS(prevdfval);
                                printLine(current_context_label, 1);
                            }

                        } // end else (not call instr)

                    } // end for
                } else {
                    // NOT SLIM
                    F prevdfval = getDFValueFIS(context_label, func);
                    for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                        Instruction &I = *inst;
                        if (CallInst *ci = dyn_cast<CallInst>(&I)) {
                            Function *target_function = ci->getCalledFunction();
                            if (not target_function || target_function->isDeclaration() ||
                                isAnIgnorableDebugInstruction(&I)) {
                                continue; // this is an inbuilt function so
                                          // doesn't need to be processed.
                            }

                            // TODO find caller - callee return arg mapping for
                            // NOT SLIM case
                            SLIMOperand *returnInCaller = nullptr;
                            SLIMOperand *returnInCallee = nullptr;
                            std::pair<SLIMOperand *, SLIMOperand *> return_operand_map =
                                make_pair(returnInCaller, returnInCallee);

                            // At the call instruction, the value at IN should
                            // be splitted into two components: 1) Purely Global
                            // and 2) Mixed. The purely global component is
                            // given to the start of callee.

                            // step 12
                            F inflow = CallInflowFunctionFIS(
                                current_context_label, target_function, bb, prevdfval, return_operand_map);

                            // step 12
                            F a2 = inflow;

                            F new_outflow_fis;
                            B dummyB;

                            int matching_context_label = 0;
                            matching_context_label = check_if_context_already_exists(
                                target_function, {inflow, dummyB}, make_pair(new_outflow_fis, dummyB));

                            if (matching_context_label > 0) {
                                if (debug) {
                                    printLine(current_context_label, 0);
                                    llvm::outs() << "\nProcessing instruction " << *inst << "\n";
                                    llvm::outs() << "\nDataflow value: ";
                                }

                                // step 14
                                pair<int, Instruction *> mypair = make_pair(current_context_label, &(*inst));
                                context_transition_graph[mypair] = matching_context_label;

                                // step 16 and 17
                                F a3 = getForwardOutflowForThisContext(matching_context_label);
                                B d3 = getBackwardOutflowForThisContext(matching_context_label);

                                F outflow = CallOutflowFunctionFIS(
                                    current_context_label, target_function, bb, a3, prevdfval, return_operand_map);

                                F outflowVal = outflow;

                                F a5 = getPurelyLocalComponentFIS(prevdfval);

                                F dfval = performMeetFIS(prevdfval, performMergeAtCallFIS(outflowVal, a5));
                                setDFValueFIS(current_pair.first, f, dfval);
                                prevdfval = dfval;
                                if (debug) {
                                    printDataFlowValuesFIS(prevdfval);
                                    printLine(current_context_label, 1);
                                }

                            } else {
                                INIT_CONTEXT_FIS(target_function, a2,
                                                 new_outflow_fis); // step 21

                                // step 14
                                // This step must be done after above step
                                // because context label counter gets updated
                                // after INIT-Context is invoked.
                                pair<int, Instruction *> mypair = make_pair(current_context_label, &(*inst));
                                context_transition_graph[mypair] = getContextLabelCounter();
                            }

                        } else {
                            // not call instruction
                            if (debug) {
                                printLine(current_context_label, 0);
                                llvm::outs() << "\nProcessing instruction " << *inst << "\n";
                                llvm::outs() << "\nDataflow value: ";
                            }

                            F newdfval = computeDFValueFIS(*inst);
                            F dfval = performMeetFIS(prevdfval, newdfval);
                            setDFValueFIS(current_pair.first, f, dfval);
                            prevdfval = dfval;
                            if (debug) {
                                printDataFlowValuesFIS(prevdfval);
                                printLine(current_context_label, 1);
                            }
                        } // end else
                    } // end for
                } // end not slim
            } else { // Step 22
                // meet with existing value
                F prevdfval = getDFValueFIS(context_label, func);
                F dfval = NormalFlowFunctionFIS(current_pair);
                setDFValueFIS(current_pair.first, f, performMeetFIS(prevdfval, dfval));
            }

            // we need the context of the processed node, so adding inside
            // while(worklist not empty)
            // Update outflow
            F outflowVal = getPurelyGlobalComponentFIS(getDFValueFIS(context_label, func));
            if (debug) {
                llvm::outs() << "\nUpdating outflow for function " << func->getName() << " for context "
                             << current_context_label << " as ";
                printDataFlowValuesFIS(outflowVal);
                llvm::outs() << "\n";
            }
            setForwardOutflowForThisContext(current_context_label,
                                            outflowVal); // setting forward outflow
            // Add caller to worklist
            if (bb == &function.back()) // step 27
            {
                if (debug)
                    llvm::outs() << "\nChecking CTG to insert callers into worklist\n";
                if (SLIM) {
                    /*
                    SRI-- Commented
                    for(auto context_inst_pairs : SLIM_context_transition_graph)
                    { if (context_inst_pairs.second == current_context_label) {
                    //Matching called function
                        //step 30
                        BasicBlock *bb =
                    context_inst_pairs.first.second->getBasicBlock(); pair<int,
                    BasicBlock *> context_bb_pair =
                    make_pair(context_inst_pairs.first.first, bb);

                            #ifdef PRVASCO
                                llvm::outs() << "\nInserting caller node into
                    Worklist at end of callee in FIS Analysis (Context:" <<
                    current_context_label << " to " <<
                    context_inst_pairs.first.first << ")\n"; for (Instruction &I
                    : *bb) llvm::outs() << I << "\n"; #endif
                            //Checking if callee has change and
                    computeGenerative is to be called again bool changeInFunc =
                    context_label_to_context_object_map[current_context_label]->isDFValueChanged();
                            bool isCalleeAnalysed = true;
                            if(changeInFunc){
                                isCalleeAnalysed = false;
                            }
                            forward_worklist.workInsert(make_pair(context_bb_pair,
                    isCalleeAnalysed)); break;
                        }
                    }*/
                    vector<pair<int, BasicBlock *>> caller_contexts =
                        SLIM_transition_graph.getCallers(current_context_label);
                    for (auto context_bb_pair : caller_contexts) {
                        BasicBlock *bb = context_bb_pair.second;

#ifdef PRVASCO
                        llvm::outs() << "\nInserting caller node into Worklist "
                                        "at end of "
                                        "callee in FIS Analysis (Context:"
                                     << current_context_label << " to " << context_bb_pair.first << ")\n";
                        for (Instruction &I : *bb)
                            llvm::outs() << I << "\n";
#endif
                        // Checking if callee has change and computeGenerative
                        // is to be called again
                        bool changeInFunc =
                            context_label_to_context_object_map[current_context_label]->isDFValueChanged();
                        bool isCalleeAnalysed = true;
                        if (changeInFunc) {
                            isCalleeAnalysed = false;
                        }
                        forward_worklist.workInsert(make_pair(context_bb_pair, isCalleeAnalysed));
                    }
                } else {
                    for (auto context_inst_pairs : context_transition_graph) {
                        if (context_inst_pairs.second == current_context_label) {
                            // step 30
                            BasicBlock *bb = context_inst_pairs.first.second->getParent();
                            pair<int, BasicBlock *> context_bb_pair = make_pair(context_inst_pairs.first.first, bb);
                            // Checking if callee has change and
                            // computeGenerative is to be called again
                            bool changeInFunc =
                                context_label_to_context_object_map[current_context_label]->isDFValueChanged();
                            bool isCalleeAnalysed = true;
                            if (changeInFunc) {
                                isCalleeAnalysed = false;
                            }
                            forward_worklist.workInsert(make_pair(context_bb_pair, isCalleeAnalysed));
                            // break;
                        }
                    }
                }
            }

            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }

        if (not EqualDataFlowValuesFIS(getDFValueFIS(context_label, func), previous_dfvalue_of_this_function)) {
            changed = true;
        } else {
            changed = false;
        }

#ifdef PRVASCO
        llvm::outs() << "\n";
        string chg = (changed == true) ? "TRUE" : "FALSE";
        llvm::outs() << "Is Dataflow value of the function changed in Analysis: " << chg;
        llvm::outs() << "\n(Previous Dataflow value: ";
        printDataFlowValuesFIS(previous_dfvalue_of_this_function);
        llvm::outs() << " , Current Dataflow value: ";
        printDataFlowValuesFIS(getDFValueFIS(context_label, func));
        llvm::outs() << ")\n";
#endif

        // Set flag in context object
        if (changed) {
            context_label_to_context_object_map[context_label]->setDFValueChange(true);
        }

        if (changed && modeOfExec == FIFP) // step 24
        {
            // not yet converged
            // Add all basic blocks to worklist again

            for (BasicBlock *BB : post_order(&func->getEntryBlock())) {
                BasicBlock &b = *BB;
                forward_worklist.workInsert(make_pair(make_pair(context_label, &b), false));

                process_mem_usage(this->vm, this->rss);
                this->total_memory = max(this->total_memory, this->vm);
            }

            /*for (auto succ_bb:successors(bb))//step 25
            {
                #ifdef PRVASCO
                    llvm::outs() << "\n";
                    llvm::outs() << "Inserting successors into Forward Worklist
            on change in OUT value in Forward Analysis (Context:" <<
            current_context_label << ")\n"; for (Instruction &I : *succ_bb)
                        llvm::outs() << I << "\n";
                #endif
                //step 26
                forward_worklist.workInsert({current_context_label,succ_bb});
            }*/
        }
    }
    if (debug) {
        llvm::outs() << "Outflow value final at function " << func->getName() << " is ";
        printDataFlowValuesFIS(getForwardOutflowForThisContext(context_label));
    }
}

template <class F, class B>
F Analysis<F, B>::NormalFlowFunctionFIS(pair<int, BasicBlock *> current_pair_of_context_label_and_bb) {
    BasicBlock &b = *(current_pair_of_context_label_and_bb.second);
    Context<F, B> *context_object = context_label_to_context_object_map[current_pair_of_context_label_and_bb.first];
    F dfvalue = getDFValueFIS(current_pair_of_context_label_and_bb.first, context_object->getFunction());
    // traverse a basic block in forward direction
    if (SLIM) {
        for (auto &index : optIR->func_bb_to_inst_id[{context_object->getFunction(), &b}]) {
            auto &inst = optIR->inst_id_to_object[index];

            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 0);
                llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                inst->printInstruction();
            }

            F newdfvalue = computeDFValueFIS(inst);
            dfvalue = performMeetFIS(
                getDFValueFIS(current_pair_of_context_label_and_bb.first, context_object->getFunction()), newdfvalue);
            setDFValueFIS(current_pair_of_context_label_and_bb.first, context_object->getFunction(), dfvalue);

            if (debug) {
                llvm::outs() << "\nDataflow value: ";
                printDataFlowValuesFIS(dfvalue);
            }
            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 1);
            }
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
    } else {
        for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 0);
                llvm::outs() << *inst << "\n";
            }

            // TODO perform meet with existing value
            dfvalue = computeDFValueFIS(*inst);

            if (debug) {
                llvm::outs() << "Dataflow value: ";
                printDataFlowValuesFIS(dfvalue);
            }
            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 1);
            }
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
    }
    return dfvalue;
}

template <class F, class B>
void Analysis<F, B>::setDFValueFIS(int label, Function *func, const F &dataflowvalue) {
    DFVALUE[label][func] = dataflowvalue;
    return;
}

template <class F, class B>
F Analysis<F, B>::getDFValueFIS(int label, Function *func) {
    return DFVALUE[label][func];
}

// #########
template <class F, class B>
Analysis<F, B>::Analysis(
    std::unique_ptr<llvm::Module> &module, bool debug, bool SLIM, bool intraProc, ExecMode modeOfExec, bool ___e01) {
    // bool intraProc :
    //   set true for intraprocedural analysis
    //   set false for interprocedural analysis
    llvm::outs() << "\nInside CONSTRUCTOR with slim as: " << SLIM << "\n";
    current_module = nullptr;
    context_label_counter = -1;
    this->debug = debug;
    this->direction = "";
    this->SLIM = SLIM;
    this->current_instruction = nullptr;
    this->intraprocFlag = intraProc;
    this->modeOfExec = modeOfExec;
    this->___e01 = ___e01;
}

template <class F, class B>
Analysis<F, B>::Analysis(
    std::unique_ptr<llvm::Module> &module, const string &fileName, bool debug, bool SLIM, bool intraProc,
    ExecMode modeOfExec, bool ___e01) {
    // bool intraProc :
    //   set true for intraprocedural analysis
    //   set false for interprocedural analysis
    llvm::outs() << "\nInside CONSTRUCTOR with file output and slim as: " << SLIM << "\n";
    current_module = nullptr;
    context_label_counter = -1;
    this->debug = debug;
    // Uncomment if needed.
    // freopen(fileName.c_str(), "w", stdout);
    this->direction = "";
    this->SLIM = SLIM;
    this->current_instruction = nullptr;
    this->intraprocFlag = intraProc;
    this->modeOfExec = modeOfExec;
    this->___e01 = ___e01;
}

// #########

/*template<class F, class B>
Analysis<F,B>::Analysis(bool debug,bool SLIM) {
    current_module = nullptr;
    context_label_counter = -1;
    this->debug = debug;
    this->direction = "";
    this->SLIM = SLIM;
    this->current_instruction = nullptr;
}

template<class F, class B>
Analysis<F,B>::Analysis(bool debug, const string &fileName, bool SLIM) {
    current_module = nullptr;
    context_label_counter = -1;
    this->debug = debug;
    freopen(fileName.c_str(), "w", stdout);
    this->direction = "";
    this->SLIM = SLIM;
    this->current_instruction = nullptr;
}

template<class F, class B>
Analysis<F,B>::Analysis(std::unique_ptr<llvm::Module> &module, bool debug, bool
SLIM, bool intraProc) {
    //bool intraProc :
    //  set true for intraprocedural analysis
    //  set false for interprocedural analysis
    llvm::outs() << "\n CONSTRUCTOR slim: "<<SLIM;
    current_module = nullptr;
    context_label_counter = -1;
    this->debug = debug;
    this->direction = "";
    this->SLIM = SLIM;
    this->current_instruction = nullptr;
    this->intraprocFlag = intraProc;
}

template<class F, class B>
Analysis<F,B>::Analysis(std::unique_ptr<llvm::Module> &module, const string
&fileName, bool debug, bool SLIM, bool intraProc) {
    //bool intraProc :
    //  set true for intraprocedural analysis
    //  set false for interprocedural analysis
    llvm::outs() << "\n CONSTRUCTOR slim with file output: "<<SLIM;
    current_module = nullptr;
    context_label_counter = -1;
    this->debug = debug;
    freopen(fileName.c_str(), "w", stdout);
    this->direction = "";
    this->SLIM = SLIM;
    this->current_instruction = nullptr;
    this->intraprocFlag = intraProc;
}
*/

template <class F, class B>
Analysis<F, B>::~Analysis() {}

//=========================================================================================

template <class F, class B>
int Analysis<F, B>::getContextLabelCounter() {
    return context_label_counter;
}

template <class F, class B>
void Analysis<F, B>::setContextLabelCounter(int new_context_label_counter) {
    context_label_counter = new_context_label_counter;
}

template <class F, class B>
int Analysis<F, B>::getCurrentAnalysisDirection() {
    return current_analysis_direction;
}

template <class F, class B>
void Analysis<F, B>::setCurrentAnalysisDirection(int direction) {
    current_analysis_direction = direction;
}

template <class F, class B>
int Analysis<F, B>::getProcessingContextLabel() const {
    return processing_context_label;
}

template <class F, class B>
void Analysis<F, B>::setProcessingContextLabel(int label) {
    processing_context_label = label;
}

template <class F, class B>
unsigned int Analysis<F, B>::getNumberOfContexts() {
    return ProcedureContext.size();
}

template <class F, class B>
bool Analysis<F, B>::isAnIgnorableDebugInstruction(llvm::Instruction *inst) {
    if (isa<DbgDeclareInst>(inst) || isa<DbgValueInst>(inst)) {
        return true;
    }
    return false;
}

template <class F, class B>
bool Analysis<F, B>::isIgnorableInstruction(BaseInstruction *inst) {
    /*if (isa<DbgDeclareInst>(inst) || isa<DbgValueInst>(inst)) {
        return true;
    }*/
    Instruction *Inst = inst->getLLVMInstruction();
    if (inst->getInstructionType() == UNREACHABLE)
        return true;

    if (inst->getInstructionType() == SWITCH)
        return true;

    if (inst->getIsDynamicAllocation()) {
// llvm::outs()<<"Inside isIgnorableInstruction function";
#ifdef IGNORE
        return true;
#endif
#ifndef IGNORE
        return false;
#endif
    }

    if (inst->getCall()) { //(llvm::CallInst *ci =
                           // dyn_cast<llvm::CallInst>(Inst)){
        /*CallInst *ci = dyn_cast<CallInst>(inst);
        Function *target_function = ci->getCalledFunction();*/
        // Changed to fetch target (Callee) function from SLIM IR instead of
        // LLVM IR
        CallInstruction *cTempInst = new CallInstruction(inst->getLLVMInstruction());
        if (cTempInst->isIndirectCall())
        return false;
        //   llvm::outs() << "\nInside isIgnorableInstruction!!! Indirect calls
        //   not " "ignorable anymore :)";
        Function *target_function = cTempInst->getCalleeFunction();
        if (not target_function || target_function->isDeclaration() || isAnIgnorableDebugInstruction(Inst)) {
            return true; // this is an inbuilt function so doesn't need to be
                         // processed.
        }
    }

    return false;
}

template <class F, class B>
bool Analysis<F, B>::isAnIgnorableDebugInstruction(BaseInstruction *inst) {
    return false;
}

template <class F, class B>
void Analysis<F, B>::startSplitting(std::unique_ptr<llvm::Module> &mod) {
    // for (Function &function : *(this->current_module)) {
    for (Function &function : *mod) {
        if (function.size() > 0) {
            performSplittingBB(function);
        }
    }
}

template <class F, class B>
void Analysis<F, B>::performSplittingBB(Function &function) {
    int flag = 0;
    Instruction *I = nullptr;
    bool previousInstructionWasSplitted = false;
    bool previousInstructionWasCallInstruction = false;
    map<Instruction *, bool> isSplittedOnThisInstruction;
    for (BasicBlock *BB : inverse_post_order(&function.back())) {
        // for (BasicBlock *BB :
        // inverse_post_order(optIR->getLastBasicBlock(&function))) {
        BasicBlock &b = *BB;
        previousInstructionWasSplitted = true;
        previousInstructionWasCallInstruction = false;
        for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
            I = &(*inst);
            if (inst == &*(b.begin())) {
                if (isa<CallInst>(*I)) {
                    CallInst *ci = dyn_cast<CallInst>(I);

                    Function *target_function = ci->getCalledFunction();
                    CallInstruction *cTempInst = new CallInstruction(I);
                    if (!cTempInst->isIndirectCall() &&
                        (not target_function || target_function->isDeclaration() || isAnIgnorableDebugInstruction(I))) {
                        continue; // this is an inbuilt function so doesn't need
                                  // to be processed.
                    }
                    isSplittedOnThisInstruction[I] = false;
                    previousInstructionWasCallInstruction = true;
                    previousInstructionWasSplitted = true; // creating a false positive
                }
                continue;
            }

            if (isa<BranchInst>(*I)) {

            } else if (isa<CallInst>(*I)) {
                CallInst *ci = dyn_cast<CallInst>(I);

                Function *target_function = ci->getCalledFunction();
                CallInstruction *cTempInst = new CallInstruction(I);
                if (!cTempInst->isIndirectCall() &&
                    (not target_function || target_function->isDeclaration() || isAnIgnorableDebugInstruction(I))) {

                    // llvm::outs() << "\n Not splitting on "<<*I<<" ";
                    // llvm::outs() <<not(target_function)<<" "<<
                    // target_function->isDeclaration() <<"
                    // "<<isAnIgnorableDebugInstruction(I);

                    continue;
                }

                isSplittedOnThisInstruction[I] = true;
                previousInstructionWasCallInstruction = true;
                previousInstructionWasSplitted = true;
            } else {
                if (previousInstructionWasSplitted) {
                    if (previousInstructionWasCallInstruction) {
                        isSplittedOnThisInstruction[I] = true;
                        previousInstructionWasSplitted = true;
                    } else {
                        previousInstructionWasSplitted = false;
                    }
                } else {
                    // do nothing
                }
                previousInstructionWasCallInstruction = false;
            }
        }
    }
    BasicBlock *containingBB;

    for (auto split_here : isSplittedOnThisInstruction) {
        if (split_here.second == false) // no splitting to be done
            continue;
        containingBB = split_here.first->getParent();
        containingBB->splitBasicBlock(split_here.first);
    }
}

template <class F, class B>
Module *Analysis<F, B>::getCurrentModule() {
    return current_module;
}

template <class F, class B>
void Analysis<F, B>::setCurrentModule(Module *m) {
    current_module = m;
}

template <class F, class B>
BaseInstruction *Analysis<F, B>::getCurrentInstruction() const {
    return this->current_instruction;
}

template <class F, class B>
void Analysis<F, B>::setCurrentInstruction(BaseInstruction *current_instruction) {
    this->current_instruction = current_instruction;
}

//=========================================================================================

template <class F, class B>
void Analysis<F, B>::printLine(int label, int startend) {
    string Name = "";
    if (current_analysis_direction == 1) {
        Name = "FORWARD";
    } else {
        Name = "BACKWARD";
    }

    // startend => 0 : start printline , 1 : end printline
    Context<F, B> *current_obj = context_label_to_context_object_map[label];
    std::string Str = "\n===================================[" + Name + "-" + to_string(label) +
                      " ]===========================================\n";
    llvm::outs() << Str;
    if (startend == 0) {
        llvm::outs() << "For Value Context : Forward Inflow - ";
        printDataFlowValuesForward(current_obj->getInflowValue().first);
        llvm::outs() << " Backward Inflow - ";
        printDataFlowValuesBackward(current_obj->getInflowValue().second);
        llvm::outs() << "\n";
    }
}

template <class F, class B>
void Analysis<F, B>::printModule(Module &M) {
    llvm::outs() << "--------------------------------------------" << "\n";
    for (Function &Func : M) {
        llvm::outs() << "FUNCTION NAME: ";
        llvm::outs() << Func.getName() << "\n";
        llvm::outs() << "----------------------------" << "\n";
        for (BasicBlock &BB : Func) {
            llvm::outs() << "----------------------" << "\n";
            for (Instruction &inst : BB) {
                llvm::outs() << inst << "\n";
            }
            llvm::outs() << "----------------------" << "\n";
        }
        llvm::outs() << "-----------------------------" << "\n";
    }
    llvm::outs() << "---------------------------------------------" << "\n";
}

template <class F, class B>
void Analysis<F, B>::printContext() {
    llvm::outs() << "\n";
    for (auto label : ProcedureContext) {
        llvm::outs() << "=========================================================="
                        "========================================"
                     << "\n";
        auto context = context_label_to_context_object_map[label];
        llvm::outs() << "LABEL: " << label << "\n";
        llvm::outs() << "FUNCTION NAME: " << context->getFunction()->getName() << "\n";
        llvm::outs() << "INFLOW VALUE: ";
        llvm::outs() << "Forward:-";
        printDataFlowValuesForward(context->getInflowValue().first);
        llvm::outs() << "Backward:-";
        printDataFlowValuesBackward(context->getInflowValue().second);
        llvm::outs() << "OUTFLOW VALUE: ";
        llvm::outs() << "Forward:-";
        printDataFlowValuesForward(context->getOutflowValue().first);
        llvm::outs() << "Backward:-";
        printDataFlowValuesBackward(context->getOutflowValue().second);
        llvm::outs() << "\n";
        llvm::outs() << "=========================================================="
                        "========================================"
                     << "\n";
    }
}

template <class F, class B>
void Analysis<F, B>::printInOutMaps() {
    llvm::outs() << "\n";
}

template <class F, class B>
float Analysis<F, B>::getTotalMemory() {
    return this->total_memory;
}

template <class F, class B>
void Analysis<F, B>::printStats() {
    llvm::Module *M = this->getCurrentModule();
    string fileName = M->getName().str();
    fileName += "_stats.txt";
    std::ofstream fout(fileName);
    std::unordered_map<llvm::Function *, int> CountContext;
    fout << "\n=================-------------------Statistics of "
            "Analysis-------------------=================";
    fout << "\n Total number of Contexts: " << this->getNumberOfContexts();
    fout << "\n Total time taken in Splitting Basic Blocks: " << this->SplittingBBTime.count() << " milliseconds";
    // fout << "\n Total time taken in SLIM Modelling: " <<
    // this->SLIMTime.count()
    // << " milliseconds"; fout << "\n Total time taken in Analysis: " <<
    // this->AnalysisTime.count() << " milliseconds";
    fout << "\n Total memory taken by Analysis: ";
    printMemory(this->total_memory, fout);
    fout << "\n------------------------List of all "
            "Contexts------------------------------------------";
    double forwardSum = 0.0;
    double backwardSum = 0.0;
    for (auto &label : ProcedureContext) {
        fout << "\n---------------------------------------";
        Context<F, B> *context = this->context_label_to_context_object_map[label];
        F temp1 = context->getInflowValue().first;
        B temp2 = context->getInflowValue().second;
        fout << "\n Context Label: " << label;
        fout << "\n Function Name: " << context->getFunction()->getName().str();
        fout << "\n Forward Inflow: ";
        printFileDataFlowValuesForward(temp1, fout);
        fout << "\n Backward Inflow: ";
        printFileDataFlowValuesBackward(temp2, fout);
        fout << "\n";
        fout << "\n Forward size: " << this->getSize(temp1);
        fout << "\n Backward size: " << this->getSize(temp2);
        fout << "\n---------------------------------------";
        forwardSum += this->getSize(temp1);
        backwardSum += this->getSize(temp2);
        CountContext[context->getFunction()]++;
    }

    fout << "\n\n--------------------------Average size of Data Flow Values "
            "over "
            "contexts----------------------------------------";
    double forwardAvg = forwardSum / this->getNumberOfContexts();
    double backwardAvg = backwardSum / this->getNumberOfContexts();
    fout << fixed;
    fout << setprecision(6);
    fout << "\n Average Forward Data Flow Value Size : ";
    fout << forwardAvg;
    fout << "\n Average Backward Data Flow Value size : ";
    fout << backwardAvg;
    fout << "\n";

    fout << "\n--------------------------Number of context generated for each "
            "function----------------------------------------";
    for (auto &p : CountContext) {
        fout << "\n---------------------------------------";
        fout << "\n Function Name: " << p.first->getName().str();
        fout << "\n Number of contexts: " << p.second;
        fout << "\n---------------------------------------";
    }
}

template <class F, class B>
void Analysis<F, B>::printStatistics() {
#ifdef PRINTSTATS
    std::map<Function *, std::set<long long>> func_ins_map;
    int totalNoOfContexts = 0;
    int totalpointsToSize = 0;
    int totalliveInfoSize = 0;
    int total_num_functions = 0;
    F cumulativeForwardsInfo{};
    long cumulativeForwardSize = 0, cumulativeBackwardSize = 0;
    long cumulativeVecWithZeroForwardsInfo = 0, totalForwardVectors = 0;
    ;
    // B cumulativeBackwardsInfo{};
    std::set<SLIMOperand *> cumulativeBackwardsInfo{};
    vector<int> pointsTo;
    vector<int> liveness;
    for (auto &entry : optIR->func_bb_to_inst_id) {
        Function *func = entry.first.first;
        for (long long instruction_id : entry.second) {
            func_ins_map[func].insert(instruction_id);
        }
    }
    ////  llvm::outs() << "\nFunction | Number of Contexts | Avg, Medain  and
    /// Max " /                  "Points to size | Avg, Medain and Max Liveness
    /// Size";
    for (auto &entry : func_ins_map) {
        Function *func = entry.first;
        vector<int> contexts = getAllContextsofFunction(func);
        int NoOfcontext = contexts.size();
        totalNoOfContexts += NoOfcontext;
        int pointsToSize = 0;
        int liveInfoSize = 0;
        vector<int> pointsToPerFun;
        vector<int> livenessPerFun;
        for (int con : contexts) {
            Context<F, B> *contextObj = context_label_to_context_object_map[con];
            std::pair<F, B> inflow = contextObj->getInflowValue();
            pointsToSize += inflow.first.size();
            liveInfoSize += inflow.second.size();
            pointsToPerFun.push_back(inflow.first.size());
            livenessPerFun.push_back(inflow.second.size());
            // Checking withing functions, i.e. at IN and OUT of each
            // instructions
            for (long long instruction_id : entry.second) {
                BaseInstruction *I = optIR->inst_id_to_object[instruction_id];
                /*if(debugFlag){
                    llvm::outs() << "\n Processing instruction within the
                unnamed function"; I->printInstruction();
                }*/
                F forwardIn = getForwardComponentAtInOfThisInstruction(I, con);
                B backwardIn = getBackwardComponentAtInOfThisInstruction(I, con);
                F forwardOut = getForwardComponentAtOutOfThisInstruction(I, con);
                B backwardOut = getBackwardComponentAtOutOfThisInstruction(I, con);
                // I->printInstruction();
                // llvm::outs() << "\n" << forwardIn.size()<<" "<<"
                // "<<forwardOut.size()<<" "<<backwardIn.size()<<"
                // "<<backwardOut.size();
                cumulativeForwardsInfo = cumulativeForwardsInfo.set_union(forwardIn);
                cumulativeForwardSize += forwardIn.size();
                if (forwardIn.size() == 0) {
                    cumulativeVecWithZeroForwardsInfo += 1;
                }
                cumulativeForwardSize += forwardOut.size();
                if (forwardOut.size() == 0) {
                    cumulativeVecWithZeroForwardsInfo += 1;
                }
                totalForwardVectors += 2;
                cumulativeForwardsInfo = cumulativeForwardsInfo.set_union(forwardOut);
                for (auto a : backwardIn)
                    cumulativeBackwardsInfo.insert(GET_KEY(a));
                for (auto b : backwardOut)
                    cumulativeBackwardsInfo.insert(GET_KEY(b));
            }
        }
        totalpointsToSize += pointsToSize;
        totalliveInfoSize += liveInfoSize;
        pointsTo.insert(pointsTo.end(), pointsToPerFun.begin(), pointsToPerFun.end());

        liveness.insert(liveness.end(), livenessPerFun.begin(), livenessPerFun.end());
        /* if (NoOfcontext > 0)
           llvm::outs() << "\n"
                        << func->getName() << "\t\t " << NoOfcontext << "\t "
                        << (float)pointsToSize / NoOfcontext << "\t  "
                        << MedianOfVector(pointsToPerFun) << "\t  "
                        << MaximumInVector(pointsToPerFun) << "\t "
                        << (float)liveInfoSize / NoOfcontext << "\t "
                        << MedianOfVector(livenessPerFun) << "\t "
                        << MaximumInVector(livenessPerFun);
         else
           llvm::outs() << "\n" << func->getName() << "\t\t " << NoOfcontext;*/

        total_num_functions++;
    }
    /* llvm::outs() << "\nAverage number of contexts per function = "
                  << (float)totalNoOfContexts / func_ins_map.size();
     llvm::outs() << "\nAverage Points to info size per context= "
                  << (float)totalpointsToSize / totalNoOfContexts;
     llvm::outs() << "\nAverage liveness info size per context= "
                  << (float)totalliveInfoSize / totalNoOfContexts;
     llvm::outs() << "\nMeadian Points to info size per context= "
                  << MedianOfVector(pointsTo);
     llvm::outs() << "\nMeadian liveness info size per context= "
                  << MedianOfVector(liveness);
     llvm::outs() << "\nMax Points to info size per context= "
                  << MaximumInVector(pointsTo);
     llvm::outs() << "\nMax liveness info size per context= "
                  << MaximumInVector(liveness);
     llvm::outs()
         << "\n\n=========================================================="
            "===========================";*/
    llvm::outs() << "\nTotal number of functions = " << total_num_functions;
    llvm::outs() << "\nTotal number of contexts = " << totalNoOfContexts;

    llvm::outs() << " \nTotal number of BB analysed during Forward Analysis = " << F_BBanalysedCount;
    llvm::outs() << " \nTotal number of BB analysed during Backward Analysis = " << B_BBanalysedCount;
    llvm::outs() << " \nTotal number of times Context is Reused = " << contextReuseCount;

    llvm::outs() << " \n Total forward rounds: " << forwardsRoundCount;
    llvm::outs() << " \n Total backwards rounds " << backwardsRoundCount;

    llvm::outs() << " \n Cumulative forwards size = " << cumulativeForwardsInfo.size();

    llvm::outs() << " \n Cumulative backwards size = " << cumulativeBackwardsInfo.size();

    llvm::outs() << "\n Cumulative Forwards Information size passed to the callee: " << totalpointsToSize;
    llvm::outs() << "\n Cumulative Backwards Information size passed to the callee: " << totalliveInfoSize;

    llvm::outs() << "\n\n=========================================================="
                    "===========================";
    llvm::outs() << "\nTotal number of contexts = " << totalNoOfContexts;
    llvm::outs() << " \nTotal number of BB(F+B) = " << F_BBanalysedCount + B_BBanalysedCount;
    llvm::outs() << " \n Cumulative forwards size = " << cumulativeForwardSize;
    llvm::outs() << "\n Cumulative Forwards Information size passed to the callee: " << totalpointsToSize;
    llvm::outs() << "\n Cumulative Backwards Information size passed to the callee: " << totalliveInfoSize;

    llvm::outs() << " \n Total number of rounds(F+B): " << forwardsRoundCount + backwardsRoundCount;
    llvm::outs() << " \n Number of vectors with no forward info: " << cumulativeVecWithZeroForwardsInfo;

    llvm::outs() << " \n Total number of PIN/POUT vectors = " << totalForwardVectors;
    // #BBCOUNTPERROUND
    llvm::outs() << "\n Count of basic blocks per round: ";
    llvm::outs() << "\n FORWARD(Round, Count): \n";
    for (auto fb : mapforwardsRoundBB) {
        llvm::outs() << "\t (" << fb.first << ", " << fb.second << ")";
    }
    llvm::outs() << "\n BACKWARD(Round, Count): \n";
    for (auto bb : mapbackwardsRoundBB) {
        llvm::outs() << "\t (" << bb.first << ", " << bb.second << ")";
    }
#endif

#ifdef PRINTSTATS
    llvm::outs() << "\n\n ==========================Displaying Data flow value "
                    "across for all contexts==========================\n";

    for (auto &entry : SLIM_IN_ALL) {
        llvm::Function *func_name = entry.first;
        llvm::outs() << "\n Displaying IN value for function: " << func_name->getName();
        unordered_map<BaseInstruction *, pair<F, B>> mapInsInValue = entry.second;

        for (auto it : mapInsInValue) {
            llvm::outs() << "\n\n Instruction : ";
            it.first->printInstruction();
            llvm::outs() << " PIN : ";
            printDataFlowValuesForward(getAllForwardComponentAtInOfThisInstruction(func_name, it.first));
            llvm::outs() << "\n POUT : ";
            printDataFlowValuesForward(getAllForwardComponentAtOutOfThisInstruction(func_name, it.first));

            llvm::outs() << "\n LIN : ";
            printDataFlowValuesBackward(getAllBackwardComponentAtInOfThisInstruction(func_name, it.first));

            llvm::outs() << "\n LOUT : ";
            printDataFlowValuesBackward(getAllBackwardComponentAtOutOfThisInstruction(func_name, it.first));
        }
        llvm::outs() << "\n\n";
    }

#endif

    llvm::outs() << "\n\nTotal time taken in Analysis: " << this->AnalysisTime.count() << " milliseconds";
    llvm::outs() << "\n\nTotal time taken for Backwards Analysis: " << this->backward_time.count() << " milliseconds";
    llvm::outs() << "\n\nTotal time taken for Forwards Analysis: " << this->forward_time.count() << " milliseconds";
}

template <class F, class B>
void Analysis<F, B>::validation(bool ___e01) {
#ifdef PRINTSTATS
    llvm::Module *M = this->getCurrentModule();
    string fileName; // = M->getName().str();
    if (___e01) {
        // TODO
    } else {
        fileName = "lfcpa_validation.txt";
    }
    std::ofstream fout(fileName);
    fout << "==========================================================";
    fout << "VALIDATION PHASE";
    fout << "==========================================================" << "\n";
    std::map<Function *, std::set<long long>> func_ins_map;
    for (auto &entry : optIR->func_bb_to_inst_id) {
        Function *func = entry.first.first;
        for (long long instruction_id : entry.second) {
            func_ins_map[func].insert(instruction_id);
        }
    }
    unordered_map<Function *, pair<F, B>> onflow_info;
    if (___e01) {
        // TODO
    }
    for (auto &entry : func_ins_map) {
        Function *func = entry.first;
        pair<F, B> onflow_val = onflow_info[func];
        // llvm::outs() <<"\n"<< func->getName();
        printDataFlowValuesForward(onflow_val.first);
        printDataFlowValuesBackward(onflow_val.second);
        unordered_map<BaseInstruction *, pair<F, B>> Forward_Backward_info_at_IN, Forward_Backward_info_at_OUT;
        vector<int> contexts = getAllContextsofFunction(func);
        for (int con : contexts) {
            // llvm::outs() <<" "<< con;
            for (long long instruction_id : entry.second) {
                BaseInstruction *I = optIR->inst_id_to_object[instruction_id];
                F forwardIn = getForwardComponentAtInOfThisInstruction(I, con);
                B backwardIn = getBackwardComponentAtInOfThisInstruction(I, con);
                F forwardOut = getForwardComponentAtOutOfThisInstruction(I, con);
                B backwardOut = getBackwardComponentAtOutOfThisInstruction(I, con);

                Forward_Backward_info_at_IN[I].first.insert(forwardIn.begin(), forwardIn.end());
                Forward_Backward_info_at_IN[I].first.insert(onflow_val.first.begin(), onflow_val.first.end());
                Forward_Backward_info_at_IN[I].second.insert(backwardIn.begin(), backwardIn.end());
                Forward_Backward_info_at_IN[I].second.insert(onflow_val.second.begin(), onflow_val.second.end());

                Forward_Backward_info_at_OUT[I].first.insert(forwardOut.begin(), forwardOut.end());
                Forward_Backward_info_at_OUT[I].first.insert(onflow_val.first.begin(), onflow_val.first.end());
                Forward_Backward_info_at_OUT[I].second.insert(backwardOut.begin(), backwardOut.end());
                Forward_Backward_info_at_OUT[I].second.insert(onflow_val.second.begin(), onflow_val.second.end());
            }
        }
        for (long long instruction_id : entry.second) {
            BaseInstruction *I = optIR->inst_id_to_object[instruction_id];
            llvm::outs() << "\n\n";
            I->printInstruction();
            if (isUseInstruction(I)) {
                // llvm::outs()<<" Yes";
                fout << "\n\n";

                long long instid = I->getInstructionId();
                std::string funcName = func->getName().str();
                fout << "Function : " << funcName << ", Instruction : ";
                Instruction *Inst = I->getLLVMInstruction();
                std::string str;
                llvm::raw_string_ostream output(str);
                Inst->print(output);
                // fout << I->getValue().str();
                fout << str;
                // I->printInstruction();
                fout << "\nForward : \n";
                fout << "IN value : ";
                printFileDataFlowValuesForward(Forward_Backward_info_at_IN[I].first, fout);
                // llvm::outs() << "\n";
                // for(int con:contexts){
                //     llvm::outs() << con <<" ";
                //     printDataFlowValuesForward(getForwardComponentAtInOfThisInstruction(I,
                //     con));

                // }

                fout << "\nOUT value : ";
                printFileDataFlowValuesForward(Forward_Backward_info_at_OUT[I].first, fout);
                // llvm::outs() << "\n";
                // for(int con:contexts)
                //     printDataFlowValuesForward(getForwardComponentAtOutOfThisInstruction(I,
                //     con));

                fout << "\nBackward : \n";
                fout << "OUT value : ";
                printFileDataFlowValuesBackward(Forward_Backward_info_at_OUT[I].second, fout);
                // llvm::outs() << "\n";
                // for(int con:contexts)
                //     printDataFlowValuesBackward(getBackwardComponentAtInOfThisInstruction(I,
                //     con));
                fout << "\nIN value : ";
                printFileDataFlowValuesBackward(Forward_Backward_info_at_IN[I].second, fout);
                fout << "\n";
                // for(int con:contexts)
                //     printDataFlowValuesBackward(getBackwardComponentAtOutOfThisInstruction(I,
                //     con));
                // llvm::outs() << "\n\n";
            }
        }
    }
#endif
}

template <class F, class B>
float Analysis<F, B>::MedianOfVector(vector<int> vec) {
    std::sort(vec.begin(), vec.end());
    int n = vec.size();
    // llvm::outs() <<"\nVector=\n";

    if (n % 2 == 0) {
        return (vec[(n - 1) / 2] + vec[n / 2]) / 2;
    } else {
        return vec[n / 2];
    }
}

template <class F, class B>
int Analysis<F, B>::MaximumInVector(vector<int> vec) {
    return *max_element(vec.begin(), vec.end());
}

//===============================Virtual
// functions=========================================

template <class F, class B>
B Analysis<F, B>::getFPandArgsBackward(long int Index, Instruction *I) {
    llvm::outs() << "\nThis function getFPandArgs() has not been implemented. "
                    "EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getPinStartCallee(long int index, Instruction *I, F &dfv, Function *Func) {
    llvm::outs() << "\nThis function getPinStartCallee() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
std::vector<Function *> Analysis<F, B>::getIndirectCalleeFromIN(long int Index, F &dfv) {
    llvm::outs() << "\nThis function getIndirectCalleeFromIN() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::computeInFromOut(llvm::Instruction &I) {
    llvm::outs() << "\nThis function computeInFromOut() has not been "
                    "implemented. EXITING 1 !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::computeInFromOut(BaseInstruction *I) {
    llvm::outs() << "\nThis function computeInFromOut() has not been "
                    "implemented. EXITING 2 !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::computeOutFromIn(llvm::Instruction &I) {
    llvm::outs() << "\nThis function computeOutFromIn() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::computeOutFromIn(BaseInstruction *I) {
    llvm::outs() << "\nThis function computeOutFromIn() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getBoundaryInformationForward() {
    llvm::outs() << "\nThis function getBoundaryInformationForward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getMainBoundaryInformationForward(BaseInstruction *I) {
    llvm::outs() << "\nThis function getMainBoundaryInformationForward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getMainBoundaryInformationForward(BaseInstruction *I, slim::IR *slimIRptr) {
    llvm::outs() << "\nThis function getMainBoundaryInformationForward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::getBoundaryInformationBackward() {
    llvm::outs() << "\nThis function getBoundaryInformationBackward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::getMainBoundaryInformationBackward(BaseInstruction *I) {
    llvm::outs() << "\nThis function getMainBoundaryInformationBackward() has "
                    "not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getInitialisationValueForward() {
    llvm::outs() << "\nThis function getInitialisationValueForward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::getInitialisationValueBackward() {
    llvm::outs() << "\nThis function getInitialisationValueBackward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::performMeetForward(const F &d1, const F &d2, const string pos) {
    llvm::outs() << "\nThis function performMeetForward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::performMeetForward(const F &d1, const F &d2) {
    llvm::outs() << "\nThis function performMeetForward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::performMeetForwardWithParameterMapping(const F &d1, const F &d2, const string pos) {
    llvm::outs() << "\nThis function performMeetForward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::performMergeAtCallForward(const F &d1, const F &d2) {
    llvm::outs() << "\nThis function performMergeAtCallForward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::performMeetBackward(const B &d1, const B &d2) const {
    llvm::outs() << "\nThis function performMeetBackward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::performMergeAtCallBackward(const B &d1, const B &d2) const {
    llvm::outs() << "\nThis function performMergeAtCallBackward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
bool Analysis<F, B>::EqualDataFlowValuesForward(const F &d1, const F &d2) const {
    llvm::outs() << "\nThis function EqualDataFlowValuesForward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
bool Analysis<F, B>::EqualContextValuesForward(const F &d1, const F &d2) const {
    llvm::outs() << "\nThis function EqualContextValuesForward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
bool Analysis<F, B>::EqualDataFlowValuesBackward(const B &d1, const B &d2) const {
    llvm::outs() << "\nThis function EqualDataFlowValuesBackward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
bool Analysis<F, B>::EqualContextValuesBackward(const B &d1, const B &d2) const {
    llvm::outs() << "\nThis function EqualContextValuesBackward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::getPurelyGlobalComponentBackward(const B &dfv) const {
    llvm::outs() << "\nThis function getPurelyGlobalComponentBackward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getPurelyGlobalComponentForward(const F &dfv) const {
    llvm::outs() << "\nThis function getPurelyGlobalComponentForward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getPurelyLocalComponentForward(const F &dfv) const {
    llvm::outs() << "\nThis function getPurelyLocalComponentForward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}
template <class F, class B>
B Analysis<F, B>::getPurelyLocalComponentBackward(const B &dfv) const {
    llvm::outs() << "\nThis function getPurelyLocalComponentBackward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::getMixedComponentBackward(const B &dfv) const {
    llvm::outs() << "\nThis function getMixedComponentBackward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getMixedComponentForward(const F &dfv) const {
    llvm::outs() << "\nThis function getMixedComponentForward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::getCombinedValuesAtCallBackward(const B &dfv1, const B &dfv2) const {
    llvm::outs() << "\nThis function getCombinedValuesAtCallBackward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::getCombinedValuesAtCallForward(const F &dfv1, const F &dfv2) const {
    llvm::outs() << "\nThis function getCombinedValuesAtCallForward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
unsigned int Analysis<F, B>::getSize(F &dfv) {
    llvm::outs() << "\nThis function getSize() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
unsigned int Analysis<F, B>::getSize(B &dfv) {
    llvm::outs() << "\nThis function getSize() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::functionCallEffectBackward(Function *target_function, const B &dfv) {
    llvm::outs() << "\nThis function functionCallEffectBackward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F Analysis<F, B>::functionCallEffectForward(Function *target_function, const F &dfv) {
    llvm::outs() << "\nThis function functionCallEffectForward() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> Analysis<F, B>::CallInflowFunction(
    int context_label, Function *target_function, BasicBlock *bb, const F &a1, const B &d1,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    llvm::outs() << "\nThis function CallInflowFunction() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

/*template <class F, class B>
pair<F, B> Analysis<F, B>::CallOutflowFunction(
    int context_label, Function *target_function, BasicBlock *bb, const F &a3,
    const B &d3, const F &a1, const B &d1,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
  llvm::outs() << "\nThis function CallOutflowFunction() has not been "
                  "implemented. EXITING !!\n";
  exit(-1);
}*/

template <class F, class B>
pair<F, B> Analysis<F, B>::CallOutflowFunction(
    int context_label, BaseInstruction *I, Function *target_function, BasicBlock *bb, const F &a3, const B &d3,
    const F &a1, const B &d1, pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    llvm::outs() << "\nThis function CallOutflowFunction() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
void Analysis<F, B>::printFileDataFlowValuesForward(const F &dfv, std::ofstream &out) const {
    llvm::outs() << "\nThis function printFileDataFlowValuesForward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
void Analysis<F, B>::printFileDataFlowValuesBackward(const B &dfv, std::ofstream &out) const {
    llvm::outs() << "\nThis function printFileDataFlowValuesBackward() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

//=====================setter and getters for IN-OUT
// Maps==================================

template <class F, class B>
F Analysis<F, B>::getForwardComponentAtInOfThisInstruction(llvm::Instruction &I) {
    int label = getProcessingContextLabel();
    return IN[label][&I].first;
}

template <class F, class B>
F Analysis<F, B>::getForwardComponentAtInOfThisInstruction(BaseInstruction *I) {
    int label = getProcessingContextLabel();
    return SLIM_IN[label][I].first;
}

template <class F, class B>
F Analysis<F, B>::getForwardComponentAtOutOfThisInstruction(llvm::Instruction &I) {
    int label = getProcessingContextLabel();
    return OUT[label][&I].first;
}

template <class F, class B>
F Analysis<F, B>::getForwardComponentAtOutOfThisInstruction(BaseInstruction *I) {
    int label = getProcessingContextLabel();
    return SLIM_OUT[label][I].first;
}

template <class F, class B>
B Analysis<F, B>::getBackwardComponentAtInOfThisInstruction(llvm::Instruction &I) {
    int label = getProcessingContextLabel();
    return IN[label][&I].second;
}

template <class F, class B>
B Analysis<F, B>::getBackwardComponentAtInOfThisInstruction(BaseInstruction *I) {
    int label = getProcessingContextLabel();
    return SLIM_IN[label][I].second;
}

template <class F, class B>
B Analysis<F, B>::getBackwardComponentAtOutOfThisInstruction(llvm::Instruction &I) {
    int label = getProcessingContextLabel();
    llvm::outs() << "label" << label;
    return OUT[label][&I].second;
}

template <class F, class B>
B Analysis<F, B>::getBackwardComponentAtOutOfThisInstruction(BaseInstruction *I) {
    int label = getProcessingContextLabel();
    return SLIM_OUT[label][I].second;
}

template <class F, class B>
void Analysis<F, B>::setForwardComponentAtInOfThisInstruction(llvm::Instruction *I, const F &in_value) {
    int label = getProcessingContextLabel();
    IN[label][I].first = in_value;
}

template <class F, class B>
void Analysis<F, B>::setForwardComponentAtInOfThisInstruction(BaseInstruction *I, const F &in_value) {
    int label = getProcessingContextLabel();
    SLIM_IN[label][I].first = in_value;
}

template <class F, class B>
void Analysis<F, B>::setForwardComponentAtOutOfThisInstruction(llvm::Instruction *I, const F &out_value) {
    int label = getProcessingContextLabel();
    OUT[label][I].first = out_value;
}

template <class F, class B>
void Analysis<F, B>::setForwardComponentAtOutOfThisInstruction(BaseInstruction *I, const F &out_value) {
    int label = getProcessingContextLabel();
    SLIM_OUT[label][I].first = out_value;
}

template <class F, class B>
void Analysis<F, B>::setBackwardComponentAtInOfThisInstruction(llvm::Instruction *I, const B &in_value) {
    int label = getProcessingContextLabel();
    IN[label][I].second = in_value;
}

template <class F, class B>
void Analysis<F, B>::setBackwardComponentAtInOfThisInstruction(BaseInstruction *I, const B &in_value) {
    int label = getProcessingContextLabel();
    SLIM_IN[label][I].second = in_value;
}

template <class F, class B>
void Analysis<F, B>::setBackwardComponentAtOutOfThisInstruction(llvm::Instruction *I, const B &out_value) {
    int label = getProcessingContextLabel();
    OUT[label][I].second = out_value;
}

template <class F, class B>
void Analysis<F, B>::setBackwardComponentAtOutOfThisInstruction(BaseInstruction *I, const B &out_value) {
    int label = getProcessingContextLabel();
    SLIM_OUT[label][I].second = out_value;
}

//=====================setter and getters
// CS_BB============================================

template <class F, class B>
pair<F, B> Analysis<F, B>::getIn(int label, llvm::BasicBlock *BB) {
    //    return IN[{label,&(*BB->begin())}];
    if (SLIM) {
        return SLIM_IN[label][optIR->inst_id_to_object[optIR->getFirstIns(BB->getParent(), BB)]];
    }
    return IN[label][&(*(BB->begin()))];
}

template <class F, class B>
pair<F, B> Analysis<F, B>::getOut(int label, llvm::BasicBlock *BB) {
    if (SLIM) {
        return SLIM_OUT[label][optIR->inst_id_to_object[optIR->getLastIns(BB->getParent(), BB)]];
    }
    return OUT[label][&(BB->back())];
}

template <class F, class B>
void Analysis<F, B>::setForwardIn(int label, llvm::BasicBlock *BB, const F &dataflowvalue) {
    if (SLIM) {
        SLIM_IN[label][optIR->inst_id_to_object[optIR->getFirstIns(BB->getParent(), BB)]].first = dataflowvalue;
        return;
    }
    IN[label][&(*(BB->begin()))].first = dataflowvalue;
}

template <class F, class B>
void Analysis<F, B>::setForwardOut(int label, llvm::BasicBlock *BB, const F &dataflowvalue) {
    if (SLIM) {
        SLIM_OUT[label][optIR->inst_id_to_object[optIR->getLastIns(BB->getParent(), BB)]].first = dataflowvalue;
        return;
    }
    OUT[label][&(BB->back())].first = dataflowvalue;
}

template <class F, class B>
void Analysis<F, B>::setBackwardIn(int label, llvm::BasicBlock *BB, const B &dataflowvalue) {
    if (SLIM) {
        SLIM_IN[label][optIR->inst_id_to_object[optIR->getFirstIns(BB->getParent(), BB)]].second = dataflowvalue;
        return;
    }
    IN[label][&(*(BB->begin()))].second = dataflowvalue;
}

template <class F, class B>
void Analysis<F, B>::setBackwardOut(int label, llvm::BasicBlock *BB, const B &dataflowvalue) {
    if (SLIM) {
        SLIM_OUT[label][optIR->inst_id_to_object[optIR->getLastIns(BB->getParent(), BB)]].second = dataflowvalue;
        return;
    }
    OUT[label][&(BB->back())].second = dataflowvalue;
}

//=========================================================================================

//=====================setter and getters for context
// objects==============================

template <class F, class B>
F Analysis<F, B>::getForwardInflowForThisContext(int context_label) {
    //    return
    //    context_label_to_context_object_map[context_label].second.first.first;
    return context_label_to_context_object_map[context_label]->getInflowValue().first;
}

template <class F, class B>
B Analysis<F, B>::getBackwardInflowForThisContext(int context_label) {
    //    return
    //    context_label_to_context_object_map[context_label].second.first.second;
    return context_label_to_context_object_map[context_label]->getInflowValue().second;
}

template <class F, class B>
F Analysis<F, B>::getForwardOutflowForThisContext(int context_label) {
    //    return
    //    context_label_to_context_object_map[context_label].second.second.first;
    return context_label_to_context_object_map[context_label]->getOutflowValue().first;
}

template <class F, class B>
B Analysis<F, B>::getBackwardOutflowForThisContext(int context_label) {
    //    return
    //    context_label_to_context_object_map[context_label].second.second.second;
    return context_label_to_context_object_map[context_label]->getOutflowValue().second;
}

template <class F, class B>
void Analysis<F, B>::setForwardInflowForThisContext(int context_label, const F &forward_inflow) {
    //    context_label_to_context_object_map[context_label].second.first.first=forward_inflow;
    context_label_to_context_object_map[context_label]->setForwardInflow(forward_inflow);
}

template <class F, class B>
void Analysis<F, B>::setBackwardInflowForThisContext(int context_label, const B &backward_inflow) {
    //    context_label_to_context_object_map[context_label].second.first.second=backward_inflow;
    context_label_to_context_object_map[context_label]->setBackwardInflow(backward_inflow);
}

template <class F, class B>
void Analysis<F, B>::setForwardOutflowForThisContext(int context_label, const F &forward_outflow) {
    //    context_label_to_context_object_map[context_label].second.second.first=forward_outflow;
    context_label_to_context_object_map[context_label]->setForwardOutflow(forward_outflow);
}

template <class F, class B>
void Analysis<F, B>::setBackwardOutflowForThisContext(int context_label, const B &backward_outflow) {
    //    context_label_to_context_object_map[context_label].second.second.second=backward_outflow;
    context_label_to_context_object_map[context_label]->setBackwardOutflow(backward_outflow);
}

template <class F, class B>
Function *Analysis<F, B>::getFunctionAssociatedWithThisContext(int context_label) {
    return context_label_to_context_object_map[context_label]->getFunction();
}

//=========================================================================================

template <class F, class B>
void Analysis<F, B>::doAnalysis(std::unique_ptr<llvm::Module> &Mod, slim::IR *slimIRObj) {
    llvm::outs() << "\n In doAnalysis 3....";
    llvm::Module &M = *Mod;
    setCurrentModule(&M);

    /*
    //====================================SPLITTING========================================
    auto start = high_resolution_clock::now();
    startSplitting();
    auto stop = high_resolution_clock::now();
    this->SplittingBBTime = duration_cast<milliseconds>(stop - start);

    //Generate SLIM IR
    slim::IR *transformIR = new slim::IR(Mod);
    //generateIR(Mod);
    optIR = transformIR->optimizeIR();
    */
    // SLIM IR object passed by client is set in the data member optIR
    if (SLIM) {
        setSLIMIRPointer(slimIRObj);
    }

    // llvm::outs() << "Inside doAnalysis with SLIM parameter as " << SLIM <<
    // "\n";

    auto start = high_resolution_clock::now();

    int i = 0;

    if (modeOfExec == FSFP || modeOfExec == FSSP) {
        for (Function &function : M) {
            if (function.getName() == "main") {
                // if (function.getName() == "matchpat_goal_anchor") {
                F forward_inflow_bi;
                B backward_inflow_bi;
                F forward_outflow_bi;
                B backward_outflow_bi;
                Function *fptr = &function;
                if (std::is_same<F, NoAnalysisType>::value) {
                    backward_inflow_bi = getBoundaryInformationBackward();
                } else if (std::is_same<B, NoAnalysisType>::value) {
                    forward_inflow_bi = getBoundaryInformationForward();
                } else {
                    backward_inflow_bi = getBoundaryInformationBackward();
                    forward_inflow_bi = getBoundaryInformationForward();
                }
                setCurrentAnalysisDirection(0);
                INIT_CONTEXT(fptr, {forward_inflow_bi, backward_inflow_bi}, {forward_outflow_bi, backward_outflow_bi});
            }
        }
        if (std::is_same<F, NoAnalysisType>::value) {
            // backward analysis
            direction = "backward";
            setCurrentAnalysisDirection(2);
            while (not backward_worklist.empty()) {
                doAnalysisBackward();
            }
        } else if (std::is_same<B, NoAnalysisType>::value) {
            // forward analysis
            direction = "forward";
            setCurrentAnalysisDirection(1);
            while (not forward_worklist.empty()) {
                doAnalysisForward();
            }
        } else {
            direction = "bidirectional";
            int fi = 1, bi = 1;
            int iteration = 1;
            while (not forward_worklist.empty() || not backward_worklist.empty()) {
                // current_analysis_direction=2;
                setCurrentAnalysisDirection(2);
                auto start_back = high_resolution_clock::now();
                while (not backward_worklist.empty()) {
                    doAnalysisBackward();
                }
                auto stop_back = high_resolution_clock::now();
                this->backward_time += duration_cast<milliseconds>(stop_back - start_back);
                // current_analysis_direction=1;
                setCurrentAnalysisDirection(1);
                auto start_forward = high_resolution_clock::now();
                while (not forward_worklist.empty()) {
                    doAnalysisForward();
                }
                auto stop_forward = high_resolution_clock::now();
                this->forward_time += duration_cast<milliseconds>(stop_forward - start_forward);
                iteration++;
            }

#ifdef PRVASCO
            llvm::outs() << "\nCOMPLETED Flow-sensitive LFCPA part\n STARTING with "
                            "FIS components\n";
#endif
            // After the dataflow values reach fixed point for flow-sensitive
            // components, check if any blocks need to be analysed for
            // flow-insensitive components

#ifdef COMPUTE_CIS_FIS
            // Fetch the basic blocks to be added to backward worklist from
            // client analysis
            std::set<BasicBlock *> fis_basic_block_bw = getMixedFISBasicBlocksBackward();
            // Fetch the basic blocks to be added to Forward worklist from
            // client analysis
            std::set<BasicBlock *> fis_basic_block_fw = getMixedFISBasicBlocksForward();
            while (fis_basic_block_bw.size() != 0 || fis_basic_block_fw.size() != 0) {
                do {
#ifdef PRVASCO
                    llvm::outs() << "\n Inside COMPUTE_CIS_FIS_BLOCKS\n";
#endif

                    // Since context-insensitive, process this basic block in
                    // all the contexts of the function to which it belongs
                    for (BasicBlock *bb : fis_basic_block_bw) {
                        Function *parentFunc = bb->getParent();
                        vector<int> contextLabels = getAllContextsofFunction(parentFunc);
                        for (int c = 0; c < contextLabels.size(); c++) {
#ifdef PRVASCO
                            BasicBlock &bObj = *bb;
                            llvm::outs() << "\n";
                            llvm::outs() << "Inserting into Backward Worklist for FIS "
                                            "components computation:"
                                         << contextLabels[c] << "\n";
                            for (Instruction &I : bObj)
                                llvm::outs() << I << "\n";
#endif
                            backward_worklist.workInsert(make_pair(make_pair(contextLabels[c], bb), false));
#ifdef PRVASCO
                            llvm::outs() << "\n";
                            llvm::outs() << "Inserting usePoints into Forward "
                                            "Worklist for "
                                            "FIS components computation when "
                                            "pointees change:"
                                         << contextLabels[c] << "\n";
                            for (Instruction &I : bObj)
                                llvm::outs() << I << "\n";
#endif
                            forward_worklist.workInsert(make_pair(make_pair(contextLabels[c], bb), false));
                        }
                    }
                    fis_basic_block_bw.clear();
                    // end of inserting into worklist
                    //  current_analysis_direction=2;
                    setCurrentAnalysisDirection(2);
                    while (not backward_worklist.empty()) {
                        doAnalysisBackward();
                    }

                    // Since context-insensitive, process this basic block in
                    // all the contexts of the function to which it belongs
                    for (BasicBlock *bb : fis_basic_block_fw) {
                        Function *parentFunc = bb->getParent();
                        vector<int> contextLabels = getAllContextsofFunction(parentFunc);
                        for (int c = 0; c < contextLabels.size(); c++) {
#ifdef PRVASCO
                            BasicBlock &bObj = *bb;
                            llvm::outs() << "\n";
                            llvm::outs() << "Inserting into Forward Worklist for FIS "
                                            "components computation:"
                                         << contextLabels[c] << "\n";
                            for (Instruction &I : bObj)
                                llvm::outs() << I << "\n";
#endif
                            forward_worklist.workInsert(make_pair(make_pair(contextLabels[c], bb), false));
                        }
                    }
                    fis_basic_block_fw.clear();
                    // end of inserting into backward worklist
                    //  current_analysis_direction=1;
                    setCurrentAnalysisDirection(1);
                    while (not forward_worklist.empty()) {
                        doAnalysisForward();
                    }
                    iteration++;
                } while (not forward_worklist.empty() || not backward_worklist.empty());

                fis_basic_block_bw = getMixedFISBasicBlocksBackward();
                fis_basic_block_fw = getMixedFISBasicBlocksForward();
                // llvm::outs() << "\nfis_basic_block_bw size: "
                //              << fis_basic_block_bw.size() << "
                //              fis_basic_block_fw size "
                //              << fis_basic_block_fw.size() << "\n";
            }
#endif

#ifdef PRVASCO
            llvm::outs() << "\nCOMPLETED FIS components !! END\n";
#endif

#ifdef COMPUTE_CS_FIS
            llvm::outs() << "\n Inside COMPUTE_CS_FIS_BLOCKS\n";
#endif
        }
    } else {
        for (Function &function : M) {
            if (function.getName() == "main") {
                F flow_insensitive_inflow;
                F flow_insensitive_outflow;
                if (std::is_same<B, NoAnalysisType>::value) {
                    flow_insensitive_inflow = getBoundaryInformationFIS();
                }
                setCurrentAnalysisDirection(0);
                INIT_CONTEXT_FIS(&function, flow_insensitive_inflow, flow_insensitive_outflow);

                if (std::is_same<B, NoAnalysisType>::value) {
                    // forward analysis
                    direction = "forward";
                    setCurrentAnalysisDirection(1);
                    doAnalysisFIS(getProcessingContextLabel(), &function);
                }
            }
        }
    }

    auto stop = high_resolution_clock::now();
    this->AnalysisTime = duration_cast<milliseconds>(stop - start);

    // Displaying Data flow value obtained for all contexts
    if (SLIM) {

#ifdef SHOW_OUTPUT
            llvm::outs() << "\n==========================Displaying Data flow value "
                            "obtained for all contexts==========================\n";
            if (modeOfExec == FSFP || modeOfExec == FSSP) {
                for (int icontext = 0; icontext <= context_label_counter; icontext++) {

                    Context<F, B> *context_object = context_label_to_context_object_map[icontext];
                    Function *currFun = context_object->getFunction();

                    for (auto &entry : optIR->func_bb_to_inst_id) {
                        llvm::Function *func = entry.first.first;
                        llvm::BasicBlock *basic_block = entry.first.second;
                        if (func->getName() == currFun->getName()) {
                            llvm::outs() << "\n Function : " << currFun->getName() << "\n";
                            for (long long instruction_id : entry.second) {

                                llvm::outs() << "\n\n";
                                BaseInstruction *instruction = optIR->inst_id_to_object[instruction_id];
                                long long instid = instruction->getInstructionId();

                                llvm::outs() << "[Context : " << icontext << "]  Instruction : [["
                                             << instruction->getInstructionId() << "]] ";
                                instruction->printInstruction();
                                // llvm::outs() << "Forward : \n";
                                llvm::outs() << "PIN value : ";
                                printDataFlowValuesForward(
                                    getForwardComponentAtInOfThisInstruction(instruction, icontext));
                                llvm::outs() << "\nPOUT value : ";
                                printDataFlowValuesForward(
                                    getForwardComponentAtOutOfThisInstruction(instruction, icontext));

                                //---
                                /*                //mod:AR
                                                setAllForwardComponentAtInOfThisInstruction(//ForwardIN
                                                    func,instruction,
                                                    performMeetForward(
                                                         getAllForwardComponentAtInOfThisInstruction(func,instruction),
                                                         getForwardComponentAtInOfThisInstruction(instruction,icontext)));
                                                 setAllForwardComponentAtOutOfThisInstruction(//ForwardOUT
                                                    func,instruction,
                                                    performMeetForward(
                                                         getAllForwardComponentAtOutOfThisInstruction(func,instruction),
                                                         getForwardComponentAtOutOfThisInstruction(instruction,icontext)));
                                //---
                                */
                                // llvm::outs() << "\nBackward : \n";
                                llvm::outs() << "\nLOUT value : ";
                                printDataFlowValuesBackward(
                                    getBackwardComponentAtOutOfThisInstruction(instruction, icontext));
                                llvm::outs() << "\nLIN value : ";
                                printDataFlowValuesBackward(
                                    getBackwardComponentAtInOfThisInstruction(instruction, icontext));
                                /*             //mod:AR

                                              setAllBackwardComponentAtInOfThisInstruction(//BackwardIN
                                                  func,instruction,
                                                  performMeetBackward(
                                                       getAllBackwardComponentAtInOfThisInstruction(func,instruction),
                                                       getBackwardComponentAtInOfThisInstruction(instruction,icontext)));
                                              setAllBackwardComponentAtOutOfThisInstruction(//BackwardOUT
                                                  func,instruction,
                                                  performMeetBackward(
                                                       getAllBackwardComponentAtOutOfThisInstruction(func,instruction),
                                                       getBackwardComponentAtOutOfThisInstruction(instruction,icontext)));
                              //---
                              */
                                llvm::outs() << "\n\n";
                            }
                        }
                    }
                }
            } else {
                // FISP or FIFP
                // TODO
            }
#endif
    }
}

template <class F, class B>
void Analysis<F, B>::INIT_CONTEXT(
    llvm::Function *function, const std::pair<F, B> &Inflow, const std::pair<F, B> &Outflow) {
    /// llvm::outs() << "\n Inside INIT_CONTEXT 1..............";

    context_label_counter++;
    Context<F, B> *context_object = new Context<F, B>(context_label_counter, function, Inflow, Outflow);
    int current_context_label = context_object->getLabel();
    setProcessingContextLabel(current_context_label);
    if (std::is_same<B, NoAnalysisType>::value) {
        if (debug) {
            llvm::outs() << "INITIALIZING CONTEXT 2:-" << "\n";
            llvm::outs() << "LABEL: " << context_object->getLabel() << "\n";
            llvm::outs() << "FUNCTION: " << function->getName() << "\n";
            llvm::outs() << "Inflow Value: ";
            printDataFlowValuesForward(Inflow.first);
            llvm::outs() << "Outflow value: ";
            printDataFlowValuesForward(getInitialisationValueForward());
        }
        // forward analysis
        context_label_to_context_object_map[current_context_label] = context_object;

        setForwardOutflowForThisContext(
            current_context_label,
            getInitialisationValueForward()); // setting outflow forward
        ProcedureContext.insert(current_context_label);

        for (BasicBlock *BB : post_order(&context_object->getFunction()->getEntryBlock())) {
            BasicBlock &b = *BB;
            forward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            if (direction == "bidirectional") {
                backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            }
            setForwardIn(current_context_label, &b, getInitialisationValueForward());
            setForwardOut(current_context_label, &b, getInitialisationValueForward());

            // initialise IN-OUT maps for every instruction
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{function, BB}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setForwardComponentAtInOfThisInstruction(inst, getInitialisationValueForward());
                    setForwardComponentAtOutOfThisInstruction(inst, getInitialisationValueForward());
                }
            } else {
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    setForwardComponentAtInOfThisInstruction(&(*inst), getInitialisationValueForward());
                    setForwardComponentAtOutOfThisInstruction(&(*inst), getInitialisationValueForward());
                }
            }
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
        if (current_context_label == 0) // main function with first invocation
        {
            setForwardInflowForThisContext(
                current_context_label,
                getBoundaryInformationForward()); // setting inflow forward
        } else {
            setForwardInflowForThisContext(current_context_label, context_object->getInflowValue().first);
        }
        setForwardIn(
            current_context_label, &context_object->getFunction()->getEntryBlock(),
            getForwardInflowForThisContext(current_context_label));
    } else if (std::is_same<F, NoAnalysisType>::value) {
        if (debug) {
            llvm::outs() << "INITIALIZING CONTEXT 3:-" << "\n";
            llvm::outs() << "LABEL: " << context_object->getLabel() << "\n";
            llvm::outs() << "FUNCTION: " << function->getName() << "\n";
            llvm::outs() << "Inflow Value: ";
            printDataFlowValuesBackward(Inflow.second);
        }
        // backward analysis
        context_label_to_context_object_map[current_context_label] = context_object;
        setBackwardOutflowForThisContext(
            current_context_label,
            getInitialisationValueBackward()); // setting outflow backward
        ProcedureContext.insert(current_context_label);

        for (BasicBlock *BB : inverse_post_order(&context_object->getFunction()->back())) {
            //  for (BasicBlock *BB :
            //       inverse_post_order(optIR->getLastBasicBlock(context_object->getFunction())))
            //       {

            BasicBlock &b = *BB;
            backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            if (direction == "bidirectional") {
                backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            }
            setBackwardIn(current_context_label, &b, getInitialisationValueBackward());
            setBackwardOut(current_context_label, &b, getInitialisationValueBackward());

            // initialise IN-OUT maps for every instruction
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{function, BB}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setBackwardComponentAtInOfThisInstruction(inst, getInitialisationValueBackward());
                    setBackwardComponentAtOutOfThisInstruction(inst, getInitialisationValueBackward());
                }
            } else {
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    setBackwardComponentAtInOfThisInstruction(&(*inst), getInitialisationValueBackward());
                    setBackwardComponentAtOutOfThisInstruction(&(*inst), getInitialisationValueBackward());
                }
            }
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
        //@Traversing all basic blocks with no successors
        std::set<llvm::BasicBlock *> *setTerminateBB = optIR->getTerminatingBBList(context_object->getFunction());
        for (BasicBlock *BB : *setTerminateBB) {
            BasicBlock &b = *BB;
            backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            if (direction == "bidirectional") {
                backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            }
            setBackwardIn(current_context_label, &b, getInitialisationValueBackward());
            setBackwardOut(current_context_label, &b, getInitialisationValueBackward());

            // initialise IN-OUT maps for every instruction
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{function, BB}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setBackwardComponentAtInOfThisInstruction(inst, getInitialisationValueBackward());
                    setBackwardComponentAtOutOfThisInstruction(inst, getInitialisationValueBackward());
                }
            } else {
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    setBackwardComponentAtInOfThisInstruction(&(*inst), getInitialisationValueBackward());
                    setBackwardComponentAtOutOfThisInstruction(&(*inst), getInitialisationValueBackward());
                }
            }
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }

        if (current_context_label == 0) // main function with first invocation
        {
            setBackwardInflowForThisContext(
                current_context_label,
                getBoundaryInformationBackward()); // setting inflow backward
        } else {
            setBackwardInflowForThisContext(
                current_context_label,
                context_object->getInflowValue().second); // setting inflow backward
        }
        setBackwardOut(
            current_context_label, &context_object->getFunction()->back(),
            getBackwardInflowForThisContext(current_context_label));

    } else {
        context_label_to_context_object_map[current_context_label] = context_object;

        if (debug) {
            llvm::outs() << "\nINITIALIZING CONTEXT 4:-" << "\n";
            llvm::outs() << "LABEL: " << context_object->getLabel() << "\n";
            llvm::outs() << "FUNCTION: " << function->getName() << "\n";
            llvm::outs() << "Inflow Value: ";
            llvm::outs() << "Forward:- ";
            printDataFlowValuesForward(Inflow.first);
            llvm::outs() << "Backward:- ";
            printDataFlowValuesBackward(Inflow.second);
#ifdef PRINTSTATS
            llvm::outs() << " \nTotal number of BB analysed during Forward Analysis = " << F_BBanalysedCount;
            llvm::outs() << " \nTotal number of BB analysed during Backward Analysis = " << B_BBanalysedCount;
            llvm::outs() << " \n Total forward rounds: " << forwardsRoundCount;
            llvm::outs() << " \n Total backwards rounds " << backwardsRoundCount;
            // #BBCOUNTPERROUND
            llvm::outs() << "\n Count of basic blocks per round: ";
            llvm::outs() << "\n FORWARD(Round, Count): \n";
            for (auto fb : mapforwardsRoundBB) {
                llvm::outs() << "\t (" << fb.first << ", " << fb.second << ")";
            }
            llvm::outs() << "\n BACKWARD(Round, Count): \n";
            for (auto bb : mapbackwardsRoundBB) {
                llvm::outs() << "\t (" << bb.first << ", " << bb.second << ")";
            }
            printStatistics();
            stat.dump();
#endif
        } //

        setForwardOutflowForThisContext(
            current_context_label,
            getInitialisationValueForward()); // setting outflow forward
        setBackwardOutflowForThisContext(
            current_context_label,
            getInitialisationValueBackward()); // setting outflow backward
        ProcedureContext.insert(current_context_label);

        // for (BasicBlock *BB :
        // inverse_post_order(optIR->getLastBasicBlock(function))) {
        assert(context_object->getFunction());
        assert(!context_object->getFunction()->empty());
        for (BasicBlock *BB : inverse_post_order(&context_object->getFunction()->back())) {
            // populate backward worklist
            BasicBlock &b = *BB;
#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Inserting into Backward Worklist in INIT_CONTEXT:" << current_context_label << "\n";
            for (Instruction &I : b)
                llvm::outs() << I << "\n";
#endif

            backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            setBackwardIn(current_context_label, &b, getInitialisationValueBackward());
            setBackwardOut(current_context_label, &b, getInitialisationValueBackward());

            // initialise IN-OUT maps for every instruction
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{function, BB}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setBackwardComponentAtInOfThisInstruction(inst, getInitialisationValueBackward());
                    setBackwardComponentAtOutOfThisInstruction(inst, getInitialisationValueBackward());
                }
            } else {
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    setBackwardComponentAtInOfThisInstruction(&(*inst), getInitialisationValueBackward());
                    setBackwardComponentAtOutOfThisInstruction(&(*inst), getInitialisationValueBackward());
                }
            }

            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }

        //@Traversing all basic blocks with no successors
        std::set<llvm::BasicBlock *> *setTerminateBB = optIR->getTerminatingBBList(context_object->getFunction());
        for (BasicBlock *BB : *setTerminateBB) {
            /// llvm::outs() << "\n WITHIN THIS LOOPPPPP";
            // populate backward worklist
            BasicBlock &b = *BB;
#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Inserting into Backward Worklist in INIT_CONTEXT:" << current_context_label << "\n";
            for (Instruction &I : b)
                llvm::outs() << I << "\n";
#endif

            backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            setBackwardIn(current_context_label, &b, getInitialisationValueBackward());
            setBackwardOut(current_context_label, &b, getInitialisationValueBackward());

            // initialise IN-OUT maps for every instruction
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{function, BB}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setBackwardComponentAtInOfThisInstruction(inst, getInitialisationValueBackward());
                    setBackwardComponentAtOutOfThisInstruction(inst, getInitialisationValueBackward());
                }
            } else {
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    setBackwardComponentAtInOfThisInstruction(&(*inst), getInitialisationValueBackward());
                    setBackwardComponentAtOutOfThisInstruction(&(*inst), getInitialisationValueBackward());
                }
            }

            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
        //---
        for (BasicBlock *BB : post_order(&context_object->getFunction()->getEntryBlock())) {
            // populate forward worklist
            BasicBlock &b = *BB;

#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Inserting into Forward Worklist in INIT_CONTEXT:" << current_context_label << "\n";
            for (Instruction &I : b)
                llvm::outs() << I << "\n";
#endif

            forward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            setForwardIn(current_context_label, &b, getInitialisationValueForward());
            setForwardOut(current_context_label, &b, getInitialisationValueForward());

            // initialise IN-OUT maps for every instruction
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{function, BB}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setForwardComponentAtInOfThisInstruction(inst, getInitialisationValueForward());
                    setForwardComponentAtOutOfThisInstruction(inst, getInitialisationValueForward());
                }
            } else {
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    setForwardComponentAtInOfThisInstruction(&(*inst), getInitialisationValueForward());
                    setForwardComponentAtOutOfThisInstruction(&(*inst), getInitialisationValueForward());
                }
            }
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
        if (current_context_label == 0) { // main function with first invocation
            setForwardInflowForThisContext(
                current_context_label,
                getBoundaryInformationForward()); // setting inflow forward
            setBackwardInflowForThisContext(
                current_context_label,
                getBoundaryInformationBackward()); // setting inflow backward
        } else {
            setForwardInflowForThisContext(
                current_context_label,
                context_object->getInflowValue().first); // setting inflow forward
            setBackwardInflowForThisContext(
                current_context_label,
                context_object->getInflowValue().second); // setting inflow backward
        }
        setForwardIn(
            current_context_label, &context_object->getFunction()->getEntryBlock(),
            getForwardInflowForThisContext(current_context_label));
        setBackwardOut(
            current_context_label, &context_object->getFunction()->back(),
            getBackwardInflowForThisContext(current_context_label));
    }
    // exit(0);
}

template <class F, class B>
int Analysis<F, B>::check_if_context_already_exists(
    llvm::Function *function, const pair<F, B> &Inflow, const pair<F, B> &Outflow) {
    if (std::is_same<B, NoAnalysisType>::value) {
        // forward only
        if (modeOfExec == FSFP || modeOfExec == FSSP) {
            for (auto set_itr : ProcedureContext) {
                Context<F, B> *current_object = context_label_to_context_object_map[set_itr];
                F new_context_values = Inflow.first;
                F current_context_values = current_object->getInflowValue().first;
                if (function->getName() == current_object->getFunction()->getName() &&
                    EqualDataFlowValuesForward(new_context_values, current_context_values)) {
                    if (debug) {
                        llvm::outs() << "======================================"
                                        "============"
                                        "===================================="
                                     << "\n";
                        llvm::outs() << "Context found!!!!!" << "\n";
                        llvm::outs() << "LABEL: " << set_itr << "\n";
                        llvm::outs() << "======================================"
                                        "============"
                                        "===================================="
                                     << "\n";
                    }
                    return set_itr;
                    process_mem_usage(this->vm, this->rss);
                    this->total_memory = max(this->total_memory, this->vm);
                }
            }
        } else {
            /*llvm::outs() << "Printing all Context for function " <<
            function->getName(); for (auto set_itr:ProcedureContext) {
            Context<F, B> *current_object =
            context_label_to_context_object_map[set_itr]; F new_context_values =
            Inflow.first; F current_context_values =
            current_object->getInflowValue().first;

                if (function->getName() ==
            current_object->getFunction()->getName()){ llvm::outs() <<
            "\ncurrent_context_values \n label " << set_itr << " Inflow - ";
            printDataFlowValuesFIS(current_context_values); llvm::outs()
            << "\n";
                }

            }*/

            for (auto set_itr : ProcedureContext) {
                Context<F, B> *current_object = context_label_to_context_object_map[set_itr];
                F new_context_values = Inflow.first;
                F current_context_values = current_object->getInflowValue().first;

                if (function->getName() == current_object->getFunction()->getName() &&
                    EqualDataFlowValuesFIS(new_context_values, current_context_values)) {
                    if (debug) {
                        llvm::outs() << "======================================"
                                        "============"
                                        "===================================="
                                     << "\n";
                        llvm::outs() << "Context found!!!!!" << "\n";
                        llvm::outs() << "LABEL: " << set_itr << "\n";
                        llvm::outs() << "======================================"
                                        "============"
                                        "===================================="
                                     << "\n";
                    }
                    return set_itr;
                    process_mem_usage(this->vm, this->rss);
                    this->total_memory = max(this->total_memory, this->vm);
                }
            }
        }
    } else if (std::is_same<F, NoAnalysisType>::value) {
        // Backward Analysis
        for (auto set_itr : ProcedureContext) {
            Context<F, B> *current_object = context_label_to_context_object_map[set_itr];
            B new_context_values = Inflow.second;
            B current_context_values = current_object->getInflowValue().second;
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
            if (function->getName() == current_object->getFunction()->getName() &&
                EqualDataFlowValuesBackward(new_context_values, current_context_values)) {
                if (debug) {
                    llvm::outs() << "=========================================="
                                    "=========="
                                    "=================================="
                                 << "\n";
                    llvm::outs() << "Context found!!!!!" << "\n";
                    llvm::outs() << "LABEL: " << set_itr << "\n";
                    llvm::outs() << "=========================================="
                                    "=========="
                                    "=================================="
                                 << "\n";
                }
                process_mem_usage(this->vm, this->rss);
                this->total_memory = max(this->total_memory, this->vm);
                return set_itr;
            }
        }
    } else {
        for (auto set_itr : ProcedureContext) {
            Context<F, B> *current_object = context_label_to_context_object_map[set_itr];
            F new_context_values_forward = Inflow.first;
            B new_context_values_backward = Inflow.second;
            F current_context_values_forward = current_object->getInflowValue().first;
            B current_context_values_backward = current_object->getInflowValue().second;
            //           if(function->getName() ==
            //           current_object->getFunction()->getName() &&
            //         EqualDataFlowValuesForward(new_context_values_forward,
            //         current_context_values_forward) &&
            //       EqualDataFlowValuesBackward(new_context_values_backward,
            //       current_context_values_backward)) {
            // Changes on 16.8.22

            if (function->getName() == current_object->getFunction()->getName() &&
                EqualContextValuesForward(new_context_values_forward, current_context_values_forward) &&
                EqualContextValuesBackward(new_context_values_backward, current_context_values_backward)) {
                if (debug) {
                    llvm::outs() << "\n========================================"
                                    "=========="
                                    "===================================="
                                 << "\n";
                    llvm::outs() << "Context found!!!!!" << "\n";
                    llvm::outs() << "LABEL: " << set_itr << "\n";
                    llvm::outs() << "\nForward Inflow Value:- ";
                    printDataFlowValuesForward(current_object->getInflowValue().first);
                    llvm::outs() << "\nBackward Inflow Value:- ";
                    printDataFlowValuesBackward(current_object->getInflowValue().second);
                    llvm::outs() << "\nForward Outflow value:- ";
                    printDataFlowValuesForward(current_object->getOutflowValue().first);
                    llvm::outs() << "\nBackward Outflow value:- ";
                    printDataFlowValuesBackward(current_object->getOutflowValue().second);
                    llvm::outs() << "\n========================================"
                                    "=========="
                                    "===================================="
                                 << "\n";
                } //
                process_mem_usage(this->vm, this->rss);
                this->total_memory = max(this->total_memory, this->vm);
                return set_itr;
            }
        }
    }
    if (debug) {
        llvm::outs() << "\n========================================================"
                        "=============================="
                     << "\n";
        llvm::outs() << "Context NOT found!!!!!" << "\n";
        llvm::outs() << "Forward Inflow Value:- ";
        printDataFlowValuesForward(Inflow.first);
        llvm::outs() << "Backward Inflow Value:- ";
        printDataFlowValuesBackward(Inflow.second);
        llvm::outs() << "\n========================================================"
                        "=============================="
                     << "\n";
    }
    return 0;
}

template <class F, class B>
void Analysis<F, B>::doAnalysisForward() {

    long forwardsBBPerRound = 0; // #BBCOUNTPERROUND
#ifdef PRINTSTATS
    forwardsRoundCount += 1;
    // llvm::outs() << "\n FORWARD ANALYSIS ROUND: "<<forwardsRoundCount;
#endif

    while (not forward_worklist.empty()) // step 2
    {
#ifdef PRINTSTATS
        F_BBanalysedCount++;
        forwardsBBPerRound++; // #BBCOUNTPERROUND
#endif
        // step 3 and 4
        pair<pair<int, BasicBlock *>, bool> worklistElement = forward_worklist.workDelete();
        pair<int, BasicBlock *> current_pair = worklistElement.first;
        int current_context_label = current_pair.first;
        BasicBlock *bb = current_pair.second;
        bool calleeAnalysed = worklistElement.second;
        setProcessingContextLabel(current_context_label);
        // mod:AR
        /////insertIntoForwardsBBList(bb); ????

#ifdef PRVASCO
        llvm::outs() << "\n\n";
        llvm::outs() << "Popping from Forward Worklist (Context:" << current_context_label
                     << " calleeAnalysed: " << calleeAnalysed << ")\n";
        for (Instruction &I : *bb)
            llvm::outs() << I << "\n";
#endif
        BasicBlock &b = *bb;
        Function *f = context_label_to_context_object_map[current_context_label]->getFunction();
        Function &function = *f;

        // MSCHANGE 1
        F previous_value_at_out_of_this_node = getOut(current_pair.first, current_pair.second).first;
        F previous_value_at_in_of_this_node = getIn(current_pair.first, current_pair.second).first;

        // step 5
        auto pos = optIR->getExitBBList(f)->find(bb);
        if (pos == optIR->getExitBBList(f)->end()) {
            if (bb != (&function.getEntryBlock())) {
                setForwardINForNonEntryBasicBlock(current_pair); // mod:AR
            } else {
                // In value of this node is same as INFLOW value
                setForwardIn(
                    current_pair.first, current_pair.second, getForwardInflowForThisContext(current_context_label));

                // Boundary information recomputation, if function first basic
                // block of 'main'
                if (current_pair.second->getParent()->getName() == "main") {
                    if (debug)
                        llvm::outs() << "Calling getMainBoundaryInformationForward for "
                                        "first "
                                        "instruction of 'main'\n";
                    BaseInstruction *firstIns = optIR->getInstrFromIndex(
                        optIR->getFirstIns(current_pair.second->getParent(), current_pair.second));

                    setForwardIn(current_pair.first, current_pair.second, getMainBoundaryInformationForward(firstIns));
                }
            }
        } // fi outer

        // step 9
        F a1 = getIn(current_pair.first, current_pair.second).first;
        B d1 = getOut(current_pair.first, current_pair.second).second;

        // Moved up above step5
        /*F previous_value_at_out_of_this_node = getOut(current_pair.first,
                                                      current_pair.second).first;
        F previous_value_at_in_of_this_node = getIn(current_pair.first,
                                                      current_pair.second).first;
        */

#ifdef PRVASCO
        llvm::outs() << "\n";
        llvm::outs() << "Previous IN value at the node in Forward Analysis";
        printDataFlowValuesForward(previous_value_at_in_of_this_node);
        llvm::outs() << "\n";
        llvm::outs() << "Previous OUT value at the node in Forward Analysis";
        printDataFlowValuesForward(previous_value_at_out_of_this_node);
        llvm::outs() << "\n";
#endif
        bool contains_dynamic_allocation = false;
        bool contains_a_method_call = false;
        if (SLIM) {
            for (auto &index : optIR->func_bb_to_inst_id[{f, bb}]) {
                auto &inst = optIR->inst_id_to_object[index];
                setCurrentInstruction(inst);

                if (inst->getCall()) { // getCall() present in new SLIM
                    Instruction *Inst = inst->getLLVMInstruction();
                    /*CallInst *ci = dyn_cast<CallInst>(Inst);
                    Function *target_function = ci->getCalledFunction();*/
                    // Changed to fetch target (Callee) function from SLIM IR
                    // instead of LLVM IR
                    if (inst->getIsDynamicAllocation()) {
#ifndef IGNORE
                        contains_dynamic_allocation = true;
#endif
                        continue;
                    }
                    CallInstruction *cTempInst = new CallInstruction(Inst);


                    Function *target_function = cTempInst->getCalleeFunction();
                    if (target_function && (target_function->isDeclaration() || isAnIgnorableDebugInstruction(Inst))) {
                        if (debug) {
                            llvm::outs() << " Ignoring instruction: ";
                            inst->printInstruction();
                        }
                        continue; // this is an inbuilt function so doesn't need
                                  // to be processed.
                    }
                    contains_a_method_call = true;
                }
            }
        } else {
            // step 10
            for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                Instruction &I = *inst;
                if (CallInst *ci = dyn_cast<CallInst>(&I)) {
                    Function *target_function = ci->getCalledFunction();
                    if (not target_function || target_function->isDeclaration() || isAnIgnorableDebugInstruction(&I)) {
                        continue; // this is an inbuilt function so doesn't need
                                  // to be processed.
                    }
                    contains_a_method_call = true;
                    break;
                }
            }
        }

        if (contains_a_method_call) {
            // step 11
            if (SLIM) {
                F prevIn = getForwardComponentAtInOfThisInstruction(
                    optIR->inst_id_to_object[optIR->func_bb_to_inst_id[{f, bb}].front()]);
                F prev = getForwardComponentAtInOfThisInstruction(
                    optIR->inst_id_to_object[optIR->func_bb_to_inst_id[{f, bb}].front()]);
                d1 = getBackwardComponentAtOutOfThisInstruction(
                    optIR->inst_id_to_object[optIR->func_bb_to_inst_id[{f, bb}].back()]);
                for (auto &index : optIR->func_bb_to_inst_id[{f, bb}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setCurrentInstruction(inst);
                    d1 = getBackwardComponentAtOutOfThisInstruction(inst);
                    if (inst->getCall()) {
                        bool contains_indirect_call = false;
                        Instruction *Inst = inst->getLLVMInstruction();
                        /*CallInst *ci = dyn_cast<CallInst>(Inst);
                        Function *target_function = ci->getCalledFunction(); */
                        // Changed to fetch target (Callee) function from SLIM
                        // IR instead of LLVM IR
                        CallInstruction *cTempInst = new CallInstruction(Inst);
                        set<Function *> indirect_functions;
                        if (cTempInst->isIndirectCall()) {
                            if (debug)
                                llvm::outs() << "\nINDIRET CALL DETECTED within "
                                                "doAnalysisForward:contains a method "
                                                "call.\n";
                            contains_indirect_call = true;
                            SLIMOperand *function_operand = cTempInst->getIndirectCallOperand();
                            // llvm::outs() << "\n FUNCTION OPERAND NAME:
                            // "<<function_operand->getOnlyName();

                            /*   if (debug) {
                                 llvm::outs() << "Before calling
                               handleFunctionPointers()\n";
                                 printDataFlowValuesForward(a1);
                                 llvm::outs() << "-------\n";
                                 printDataFlowValuesForward(
                                     getForwardComponentAtInOfThisInstruction(inst));
                                 llvm::outs() << "DF Backward OUT\n";
                                 printDataFlowValuesBackward(d1);
                                 llvm::outs() << "DF Backward IN\n";
                                 printDataFlowValuesBackward(
                                     getBackwardComponentAtInOfThisInstruction(inst));
                               }*/

                            indirect_functions =
                                handleFunctionPointers(a1, function_operand, current_context_label, inst);
                            // indirect_functions =
                            // getIndirectFunctionsAtCallSite(inst);
                            // llvm::outs() << indirect_functions.size();
                            // llvm::outs() << "\nPrinting all indirect
                            // functions fetched from
                            // "
                            //                 "handleFunctionPointers:\n";
                            // for (auto fun : indirect_functions) {
                            //   llvm::outs() << fun->getName() << ", ";
                            // }
                            if (indirect_functions.size() == 0) {
                                continue;
                            }
                        }

                        if (contains_indirect_call) {

// #ifndef LFCPA
                            F forwardIN_at_callnode = a1;
                            for (Function *target_function : indirect_functions) {
                                // get the return variable in callee to map it
                                // with the variable in caller Example: z = call
                                // Q() and defintion of Q(){ ... return x;} ,
                                // pass pair <z, x> to compute Inflow/Outflow
                                SLIMOperand *returnInCaller = nullptr;
                                SLIMOperand *returnInCallee = nullptr;
                                std::pair<SLIMOperand *, int> callerRetOperand = inst->getLHS();
                                if (callerRetOperand.second >= 0) {
                                    SLIMOperand *calleeRet =
                                        callerRetOperand.first->getCalleeReturnOperand(target_function);
                                    if (!(calleeRet == nullptr ||
                                          calleeRet->getOperandType() == NULL_OPERAND)) { //(calleeRet!=nullptr){
                                        returnInCaller = callerRetOperand.first;
                                        returnInCallee = calleeRet;
                                    }
                                }
                                std::pair<SLIMOperand *, SLIMOperand *> return_operand_map =
                                    make_pair(returnInCaller, returnInCallee);

                                // Parameter Mapping
                                B LinStart_at_targetfunction;
                                if (SLIM_transition_graph.getCalleeContext(
                                        current_context_label, inst, target_function) != -1) {
                                    LinStart_at_targetfunction =
                                        getBackwardOutflowForThisContext(SLIM_transition_graph.getCalleeContext(
                                            current_context_label, inst, target_function));
                                }
                                if (debug) {
                                    llvm::outs() << "Forward value at IN "
                                                    "before param mapping\n";
                                    printDataFlowValuesForward(a1);
                                }

                                pair<F, B> mapped_inflow =
                                    performParameterMapping(inst, target_function, a1, LinStart_at_targetfunction);
                                // setForwardComponentAtInOfThisInstruction(inst,
                                // performMeetForward(
                                //			getForwardComponentAtInOfThisInstruction(inst),
                                //			mapped_inflow.first));

                                /* if (debug) {
                                   llvm::outs() << "\nbefore merge:";
                                   printDataFlowValuesForward(mapped_inflow.first);
                                 }*/
                                // prevIn =
                                // performMeetForwardWithParameterMapping(getForwardComponentAtInOfThisInstruction(inst),
                                // mapped_inflow.first, "IN");
                                prevIn = mapped_inflow.first;
                                ///  llvm::outs() << "\n SET PIN 1";
                                /// inst->printInstruction();
                                setForwardComponentAtInOfThisInstruction(inst, prevIn);

                                /*  if (debug) {
                                    llvm::outs() << "\n Set forward :: After
                                  merge:"; printDataFlowValuesForward(prevIn);
                                  }*/

                                if (intraprocFlag) {
                                    /*  If intraprocFlag is true, then target
                                       function is not analysed, instead the
                                       data flow value is taken from user based
                                       on how the effect is defined
                                    */

                                    setForwardComponentAtInOfThisInstruction(inst, prevIn); // compute IN from
                                                                                            // previous OUT-value

                                    F outOfFunction = functionCallEffectForward(target_function, prevIn);

                                    if (debug) {
                                        printLine(current_context_label, 0);
                                        llvm::outs() << "\nProcessing instruction at "
                                                        "INDEX = "
                                                     << index;
                                        inst->printInstruction();
                                        llvm::outs() << "\nIN: ";
                                        printDataFlowValuesForward(prevIn);
                                    }

                                    F a5 = getPurelyLocalComponentForward(prevIn);

                                    // setForwardComponentAtOutOfThisInstruction(inst,
                                    // performMeetForward(
                                    //         performMeetForward(outOfFunction,
                                    //         getForwardComponentAtOutOfThisInstruction(inst),
                                    //         "OUT"),
                                    //                             a5, "OUT"));
                                    // Merging value from END of callee with
                                    // value propagated from IN isn't
                                    // necessarily meet Thus, adding new
                                    // function performMergeAtCallForward()
                                    setForwardComponentAtOutOfThisInstruction(
                                        inst, performMeetForward(
                                                  getForwardComponentAtOutOfThisInstruction(inst),
                                                  performMergeAtCallForward(outOfFunction, a5), "OUT"));

                                    prev = getForwardComponentAtOutOfThisInstruction((inst));
                                    if (debug) {
                                        llvm::outs() << "\nOUT: ";
                                        printDataFlowValuesForward(prev);
                                        printLine(current_context_label, 1);
                                    }
                                } else {
                                    int matching_context_label = 0;
                                    F a2;
                                    B d2;

                                    F new_outflow_forward;
                                    B new_outflow_backward;
                                    int prev_callee_context_label = getSourceContextAtCallSiteForFunction(
                                        current_context_label, inst, target_function);

                                    if (!calleeAnalysed) {
                                        pair<F, B> inflow_pair;

                                        if (___e01) {
                                            // TODO
                                        } else {
                                            if (debug) {
                                                llvm::outs() << "\n PIN before calling "
                                                                "CALLINFLOW-2";
                                                printDataFlowValuesForward(a1);
                                            }
                                            inflow_pair = CallInflowFunction(
                                                current_context_label, target_function, bb, prevIn, d1,
                                                prev_callee_context_label /*add1*/
                                                ,
                                                return_operand_map);
                                        }

                                        // step 12
                                        // pair<F, B> inflow_pair =
                                        // CallInflowFunction(current_context_label,
                                        // target_function, bb, prev, d1);
                                        // pair<F, B> inflow_pair =
                                        // CallInflowFunction(current_context_label,
                                        // target_function, bb, prev, d1,
                                        // generativeKill.first);
                                        a2 = inflow_pair.first;
                                        d2 = inflow_pair.second;

// mod: AR
// #ifndef LFCPA
                                        //------

                                        // step 13
                                        if (___e01) {
                                            // TODO
                                        } else {
                                            if (debug) {
                                                llvm::outs() << "\n Set forward---1::";
                                                printDataFlowValuesForward(prevIn);
                                            }
                                            setForwardComponentAtInOfThisInstruction(inst, prevIn);
                                        }

                                        matching_context_label = check_if_context_already_exists(
                                            target_function, inflow_pair,
                                            make_pair(new_outflow_forward, new_outflow_backward));

                                    } else {
                                        // Callee is analysed, thus fetch the
                                        // callee context from Context
                                        // transition graph
                                        matching_context_label = SLIM_transition_graph.getCalleeContext(
                                            current_context_label, inst, target_function);
                                    }

                                    if (matching_context_label > 0) {
#ifdef PRINTSTATS
                                        //+++++++++++++++++++++++++++++++++++++++++++
                                        contextReuseCount++;
#endif
                                        if (debug) {
                                            printLine(current_context_label, 0);
                                            llvm::outs() << "\nProcessing instruction "
                                                            "at INDEX = "
                                                         << index;
                                            inst->printInstruction();
                                            llvm::outs() << "\nIN: ";
                                        }
                                        // step 14
                                        pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                        // SRI-- Commented
                                        // SLIM_context_transition_graph[mypair]
                                        // = matching_context_label;
                                        Function *targ_func =
                                            context_label_to_context_object_map[matching_context_label]->getFunction();
                                        if (debug)
                                            llvm::outs() << "\nAdding edge for matching "
                                                            "context in forwards  "
                                                         << matching_context_label;
                                        SLIM_transition_graph.addEdge(mypair, matching_context_label, targ_func);
#ifdef PRVASCO
                                        llvm::outs() << "\nUpdating context transition "
                                                        "graph (c"
                                                     << current_context_label << ", i" << index << ") -->"
                                                     << matching_context_label << "\n";
#endif
                                        if (debug) {
                                            printDataFlowValuesForward(prevIn);
                                        }

                                        // step 16 and 17
                                        F a3 = getForwardOutflowForThisContext(matching_context_label);
                                        B d3 = getBackwardOutflowForThisContext(matching_context_label);
                                        /*  if (debug) {
                                            llvm::outs() << "Before
                                          CallOutflowFunction() in forward "
                                                            "analysis\n"
                                                         <<
                                          matching_context_label << "\n";
                                            printDataFlowValuesForward(a3);
                                            printDataFlowValuesBackward(d3);
                                          }*/

                                        pair<F, B> outflow_pair;
                                        outflow_pair = CallOutflowFunction(
                                            current_context_label, inst, target_function, bb, a3, d3, prevIn, d1,
                                            return_operand_map);

                                        F value_to_be_meet_with_prev_out = outflow_pair.first;
                                        B d4 = outflow_pair.second;

                                        // step 18 and 19

                                        /*
                                        At the call instruction, the value at IN
                                        should be splitted into two components.
                                        The purely global component is given to
                                        the callee while the mixed component is
                                        propagated to OUT of this instruction
                                        after executing computeOUTfromIN() on
                                        it.
                                        */

                                        // #########
                                        F a5;
                                        if (___e01) {
                                            // TODO
                                        } else {
                                            a5 = getPurelyLocalComponentForward(prevIn);
                                            if (returnInCallee != nullptr)
                                                a5 = performCallReturnArgEffectForward(a5, return_operand_map);
                                        }

                                        /*
                                        At the OUT of this instruction, the
                                        value from END of callee procedure is to
                                        be merged with the local(mixed) value
                                        propagated from IN. Note that this
                                        merging "isn't" exactly (necessarily)
                                        the meet between these two values.
                                        */

                                        /*
                                        As explained in ip-vasco,pdf, we need to
                                        perform meet with the original value of
                                        OUT of this instruction to avoid the
                                        oscillation problem.
                                        */
                                        // setForwardComponentAtOutOfThisInstruction(&inst,
                                        // performMeetForward(value_to_be_meet_with_prev_out,
                                        //       getForwardComponentAtOutOfThisInstruction(inst)));

                                        // Merging value from END of callee with
                                        // value propagated from IN isn't
                                        // necessarily meet Thus, adding new
                                        // function performMergeAtCallForward()
                                        if (debug) {
                                            llvm::outs() << "Before merging in "
                                                            "doAnalysisForward, :";
                                            printDataFlowValuesForward(getForwardComponentAtOutOfThisInstruction(inst));
                                            llvm::outs() << "\n";
                                            printDataFlowValuesForward(value_to_be_meet_with_prev_out);
                                            llvm::outs() << "\n";
                                            printDataFlowValuesForward(a5);
                                        }

                                        setForwardComponentAtOutOfThisInstruction(
                                            inst,
                                            performMeetForward(
                                                getForwardComponentAtOutOfThisInstruction(inst),
                                                performMergeAtCallForward(value_to_be_meet_with_prev_out, a5), "OUT"));

                                        // setForwardComponentAtOutOfThisInstruction(inst,
                                        // performMeetForward(
                                        //     performMeetForward(value_to_be_meet_with_prev_out,
                                        //                         getForwardComponentAtOutOfThisInstruction(inst),
                                        //                         "OUT"), a5,
                                        //                         "OUT"));
                                        prev = getForwardComponentAtOutOfThisInstruction((inst));
                                        if (debug) {
                                            llvm::outs() << "\nOUT: ";
                                            printDataFlowValuesForward(prev);
                                            printLine(current_context_label, 1);
                                        }

                                    } else {
                                        /*
                                        If the call instruction is encountered
                                        before callee is analysed (i.e with
                                        calleeAnalysed as false) and there is no
                                        matching context existing maintain the
                                        caller basic block with calleeAnalysed
                                        as true and create new context
                                        */
                                        forward_worklist.workInsert(make_pair(current_pair, true));
#ifdef PRVASCO
                                        llvm::outs() << "\nInserting caller node into "
                                                        "Forward Worklist with "
                                                        "calleeAnalysed as TRUE "
                                                        "(Context:"
                                                     << current_context_label << ")\n";
                                        for (Instruction &I : *bb)
                                            llvm::outs() << I << "\n";
#endif

                                        // creating a new context
                                        //  int prev_callee_context_label =
                                        //  SLIM_transition_graph.getCalleeContext(current_context_label,
                                        //  inst, target_function);
                                        if (prev_callee_context_label == -1)
                                            INIT_CONTEXT(
                                                target_function, {a2, d2},
                                                {new_outflow_forward, new_outflow_backward}); // step
                                                                                              // 21
                                        else
                                            INIT_CONTEXT(
                                                target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward},
                                                prev_callee_context_label);

                                        // step 14
                                        // This step must be done after above
                                        // step because context label counter
                                        // gets updated after INIT-Context is
                                        // invoked.
                                        pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                        // SRI-- Commented
                                        // SLIM_context_transition_graph[mypair]
                                        // = getContextLabelCounter();
                                        if (debug)
                                            llvm::outs() << "\nAdding edge for new "
                                                            "context in forwards  "
                                                         << getContextLabelCounter();
                                        SLIM_transition_graph.addEdge(
                                            mypair, getContextLabelCounter(), target_function);
#ifdef PRVASCO
                                        llvm::outs() << "\nUpdating context transition "
                                                        "graph (c"
                                                     << current_context_label << ", i" << index << ") -->"
                                                     << getContextLabelCounter() << "\n";
#endif

#ifdef PRVASCO
                                        llvm::outs() << "\n";
                                        llvm::outs() << "After New Context Creation, IN "
                                                        "value at "
                                                        "the node in Forward Analysis "
                                                        "(Context:"
                                                     << current_pair.first << ") ";
                                        printDataFlowValuesForward(
                                            getIn(current_pair.first, current_pair.second).first);
                                        llvm::outs() << "\nAfter New Context Creation, "
                                                        "OUT value "
                                                        "at the node in Forward "
                                                        "Analysis (Context: "
                                                     << current_pair.first << ") ";
                                        printDataFlowValuesForward(
                                            getOut(current_pair.first, current_pair.second).first);
                                        llvm::outs() << "\n";
#endif
                                        // context_label_to_context_object_map[getContextLabelCounter()]->setParent(mypair);
                                        // llvm::errs() << "\n" <<
                                        // SLIM_context_transition_graph.size();
                                        // MSCHANGE 3 --commented return
                                        // return;
                                        // Reverse_SLIM_context_transition_graph[getContextLabelCounter()]
                                        // = mypair;
                                    }
                                }
                                prevIn = forwardIN_at_callnode;
                                if (debug) {
                                    llvm::outs() << "\n SET FORWARD ---2:: ";
                                    printDataFlowValuesForward(prevIn);
                                }
// mod: AR
// #ifndef LFCPA
                                //------
                                /// llvm::outs() << "\n SET PIN 3";
                                /// inst->printInstruction();
                                setForwardComponentAtInOfThisInstruction(inst, prevIn);
                            }
                        }

                        else {

                            Function *target_function = cTempInst->getCalleeFunction();
                            /*    if (not target_function ||
                                    (target_function->isDeclaration() ||
                                     isAnIgnorableDebugInstruction(Inst))) {*/
                            if (target_function &&
                                (target_function->isDeclaration() || isAnIgnorableDebugInstruction(Inst))) {
                                if (debug) {
                                    llvm::outs() << " Ignoring instruction: ";
                                    inst->printInstruction();
                                }
                                continue; // this is an inbuilt function so
                                          // doesn't need to be processed.
                            }
                            if (debug) {
                                llvm::outs() << "Printing PIN values for direct call\n";
                                printDataFlowValuesForward(getForwardComponentAtInOfThisInstruction(inst));
                                llvm::outs() << "DF value in a1=\n";
                                printDataFlowValuesForward(a1);
                            }
                            // get the return variable in callee to map it with
                            // the variable in caller Example: z = call Q() and
                            // defintion of Q(){ ... return x;} , pass pair <z,
                            // x> to compute Inflow/Outflow
                            SLIMOperand *returnInCaller = nullptr;
                            SLIMOperand *returnInCallee = nullptr;
                            std::pair<SLIMOperand *, int> callerRetOperand = inst->getLHS();
                            if (callerRetOperand.second >= 0) {
                                SLIMOperand *calleeRet = callerRetOperand.first->getCalleeReturnOperand();
                                if (!(calleeRet == nullptr ||
                                      calleeRet->getOperandType() == NULL_OPERAND)) { //(calleeRet!=nullptr){
                                    returnInCaller = callerRetOperand.first;
                                    returnInCallee = calleeRet;
                                } /*else
                                  llvm::outs() << "Return VOID callee\n";*/
                            } /*else {
                              llvm::outs() << "Return VOID\n";
                            }*/
                            std::pair<SLIMOperand *, SLIMOperand *> return_operand_map =
                                make_pair(returnInCaller, returnInCallee);

                            // if(intraprocFlag ||
                            // isFunctionIgnored(target_function)){
                            if (intraprocFlag) {
                                /*  If intraprocFlag is true, then target
                                   function is not analysed, instead the data
                                   flow value is taken from user based on how
                                   the effect is defined
                                */

                                setForwardComponentAtInOfThisInstruction(
                                    inst,
                                    prev); // compute IN from previous OUT-value

                                F outOfFunction = functionCallEffectForward(target_function, prev);

                                if (debug) {
                                    printLine(current_context_label, 0);
                                    llvm::outs() << "\nProcessing instruction "
                                                    "at INDEX = "
                                                 << index;
                                    inst->printInstruction();
                                    llvm::outs() << "\nIN: ";
                                    printDataFlowValuesForward(prev);
                                }

                                F a5 = getPurelyLocalComponentForward(prev);

                                setForwardComponentAtOutOfThisInstruction(
                                    inst, performMeetForward(
                                              getForwardComponentAtOutOfThisInstruction(inst),
                                              performMergeAtCallForward(outOfFunction, a5), "OUT"));

                                prev = getForwardComponentAtOutOfThisInstruction((inst));
                                if (debug) {
                                    llvm::outs() << "\nOUT: ";
                                    printDataFlowValuesForward(prev);
                                    printLine(current_context_label, 1);
                                }
                            } else {
                                int matching_context_label = 0;
                                F a2;
                                B d2;

                                F new_outflow_forward;
                                B new_outflow_backward;
                                int prev_callee_context_label =
                                    getSourceContextAtCallSiteForFunction(current_context_label, inst, target_function);

                                if (!calleeAnalysed) {
                                    pair<F, B> inflow_pair;
// #ifdef LFCPA
                                    prev = performMeetForward(
                                        getForwardComponentAtInOfThisInstruction(inst),
                                        prev);                                            // mod::AR
                                    setForwardComponentAtInOfThisInstruction(inst, prev); // compute IN from previous
                                                                                          // OUT-value
                                    /*llvm::outs() << "\n #VASCO TESTING...: PIN
                                    of instr"; inst->printInstruction();
                                    llvm::outs() << "\n";
                                    printDataFlowValuesForward(getForwardComponentAtInOfThisInstruction(inst));*/

                                    if (___e01) {
                                        // TODO
                                    } else {
                                        inflow_pair = CallInflowFunction(
                                            current_context_label, target_function, bb, prev, d1,
                                            prev_callee_context_label /*add2*/, return_operand_map);
                                    }

                                    // step 12
                                    a2 = inflow_pair.first;
                                    d2 = inflow_pair.second;

                                    // step 13
                                    if (debug) {
                                        llvm::outs() << "\n SET FORWARD----3:: ";
                                        printDataFlowValuesForward(prev);
                                    }

                                    matching_context_label = check_if_context_already_exists(
                                        target_function, inflow_pair,
                                        make_pair(new_outflow_forward, new_outflow_backward));

                                } else {
                                    // Callee is analysed, thus fetch the callee
                                    // context from Context transition graph
                                    matching_context_label = SLIM_transition_graph.getCalleeContext(
                                        current_context_label, inst, target_function);
                                }

                                if (matching_context_label > 0) {
#ifdef PRINTSTATS
                                    //+++++++++++++++++++++++++++++++++++++++++++
                                    contextReuseCount++;
#endif
                                    if (debug) {
                                        printLine(current_context_label, 0);
                                        llvm::outs() << "\nProcessing instruction at "
                                                        "INDEX = "
                                                     << index;
                                        inst->printInstruction();
                                        llvm::outs() << "\nIN: ";
                                    }
                                    // step 14
                                    pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                    // SRI--Commented
                                    // SLIM_context_transition_graph[mypair] =
                                    // matching_context_label;
                                    Function *targ_func =
                                        context_label_to_context_object_map[matching_context_label]->getFunction();
                                    if (debug)
                                        llvm::outs() << "\nAdding edge for matching "
                                                        "context in forwards  "
                                                     << matching_context_label;
                                    SLIM_transition_graph.addEdge(mypair, matching_context_label, targ_func);
#ifdef PRVASCO
                                    llvm::outs() << "\nUpdating context transition "
                                                    "graph (c"
                                                 << current_context_label << ", i" << index << ") -->"
                                                 << matching_context_label << "\n";
#endif
                                    if (debug) {
                                        printDataFlowValuesForward(prev);
                                    }

                                    // step 16 and 17
                                    F a3 = getForwardOutflowForThisContext(matching_context_label);
                                    B d3 = getBackwardOutflowForThisContext(matching_context_label);

                                    pair<F, B> outflow_pair;
                                    outflow_pair = CallOutflowFunction(
                                        current_context_label, inst, target_function, bb, a3, d3, prev, d1,
                                        return_operand_map);

                                    F value_to_be_meet_with_prev_out = outflow_pair.first;
                                    B d4 = outflow_pair.second;

                                    // step 18 and 19

                                    /*
                                    At the call instruction, the value at IN
                                    should be splitted into two components. The
                                    purely global component is given to the
                                    callee while the mixed component is
                                    propagated to OUT of this instruction after
                                    executing computeOUTfromIN() on it.
                                    */

                                    // #########
                                    F a5;
                                    if (___e01) {
                                        // TODO
                                    } else {
                                        a5 = getPurelyLocalComponentForward(prev);
                                        if (returnInCallee != nullptr)
                                            a5 = performCallReturnArgEffectForward(a5, return_operand_map);
                                    }

                                    /*
                                    At the OUT of this instruction, the value
                                    from END of callee procedure is to be merged
                                    with the local(mixed) value propagated from
                                    IN. Note that this merging "isn't" exactly
                                    (necessarily) the meet between these two
                                    values.
                                    */

                                    /*
                                    As explained in ip-vasco,pdf, we need to
                                    perform meet with the original value of OUT
                                    of this instruction to avoid the oscillation
                                    problem.
                                    */
                                    // setForwardComponentAtOutOfThisInstruction(&inst,
                                    // performMeetForward(value_to_be_meet_with_prev_out,
                                    //       getForwardComponentAtOutOfThisInstruction(inst)));

                                    // Merging value from END of callee with
                                    // value propagated from IN isn't
                                    // necessarily meet Thus, adding new
                                    // function performMergeAtCallForward()

                                    setForwardComponentAtOutOfThisInstruction(
                                        inst,
                                        performMeetForward(
                                            getForwardComponentAtOutOfThisInstruction(inst),
                                            performMergeAtCallForward(value_to_be_meet_with_prev_out, a5), "OUT"));

                                    // setForwardComponentAtOutOfThisInstruction(inst,
                                    // performMeetForward(
                                    //     performMeetForward(value_to_be_meet_with_prev_out,
                                    //                         getForwardComponentAtOutOfThisInstruction(inst),
                                    //                         "OUT"), a5,
                                    //                         "OUT"));
                                    prev = getForwardComponentAtOutOfThisInstruction((inst));
                                    if (debug) {
                                        llvm::outs() << "\nOUT: ";
                                        printDataFlowValuesForward(prev);
                                        printLine(current_context_label, 1);
                                    }

                                } else { // matching context label = 0

                                    /*
                                    If the call instruction is encountered
                                    before callee is analysed (i.e with
                                    calleeAnalysed as false) and there is no
                                    matching context existing maintain the
                                    caller basic block with calleeAnalysed as
                                    true and create new context
                                    */
                                    forward_worklist.workInsert(make_pair(current_pair, true)); // WHY?
#ifdef PRVASCO
                                    llvm::outs() << "\nInserting caller node into "
                                                    "Forward Worklist with "
                                                    "calleeAnalysed as TRUE (Context:"
                                                 << current_context_label << ")\n";
                                    for (Instruction &I : *bb)
                                        llvm::outs() << I << "\n";
#endif

                                    // creating a new context
                                    // int prev_callee_context_label =
                                    //     SLIM_transition_graph.getCalleeContext(
                                    //         current_context_label, inst,
                                    //         target_function);
                                    if (prev_callee_context_label == -1)
                                        INIT_CONTEXT(
                                            target_function, {a2, d2},
                                            {new_outflow_forward, new_outflow_backward}); // step 21
                                    else
                                        INIT_CONTEXT(
                                            target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward},
                                            prev_callee_context_label);

                                    // step 14
                                    // This step must be done after above step
                                    // because context label counter gets
                                    // updated after INIT-Context is invoked.
                                    pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                    // SRI--Commented
                                    // SLIM_context_transition_graph[mypair] =
                                    // getContextLabelCounter();
                                    if (debug)
                                        llvm::outs() << "\nAdding edge for new context "
                                                        "in forwards  "
                                                     << getContextLabelCounter();
                                    SLIM_transition_graph.addEdge(mypair, getContextLabelCounter(), target_function);
#ifdef PRVASCO
                                    llvm::outs() << "\nUpdating context transition "
                                                    "graph (c"
                                                 << current_context_label << ", i" << index << ") -->"
                                                 << getContextLabelCounter() << "\n";
#endif

#ifdef PRVASCO
                                    llvm::outs() << "\n";
                                    llvm::outs() << "After New Context Creation, IN "
                                                    "value at the "
                                                    "node in Forward Analysis (Context:"
                                                 << current_pair.first << ") ";
                                    printDataFlowValuesForward(getIn(current_pair.first, current_pair.second).first);
                                    llvm::outs() << "\nAfter New Context "
                                                    "Creation, OUT value at "
                                                    "the node in Forward "
                                                    "Analysis (Context: "
                                                 << current_pair.first << ") ";
                                    printDataFlowValuesForward(getOut(current_pair.first, current_pair.second).first);
                                    llvm::outs() << "\n";
#endif
                                    // context_label_to_context_object_map[getContextLabelCounter()]->setParent(mypair);
                                    // llvm::errs() << "\n" <<
                                    // SLIM_context_transition_graph.size();
                                    // MSCHANGE 3 --commented return
                                    // return;
                                    // Reverse_SLIM_context_transition_graph[getContextLabelCounter()]
                                    // = mypair;
                                } // else matching label context=0
                            } // else not intra
                        } // else direct call
                    } // if call
                    else { // non call instr
                        if (debug) {
                            printLine(current_context_label, 0);
                            llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                            inst->printInstruction();
                            llvm::outs() << "\nIN: ";
                            printDataFlowValuesForward(prev);
                            llvm::outs() << "\n SET FORWARD----4:: ";
                            printDataFlowValuesForward(prev);
                        }
// mod: AR
// #ifndef LFCPA
                        //------
                        /// llvm::outs() << "\n SET PIN 5";
                        /// inst->printInstruction();
                        setForwardComponentAtInOfThisInstruction(inst, prev); // compute IN from previous OUT-value

                        F new_prev;
                        if (isIgnorableInstruction(inst)) { // || inst->isIgnored()){
                            if (debug)
                                llvm::outs() << "\nIgnoring...\n";
                            new_prev = prev;
                        } else {
                            new_prev = computeOutFromIn(inst);
                        }
                        if (debug)
                            llvm::outs() << "Called ComputeOutFromIn-1";

                        setForwardComponentAtOutOfThisInstruction(inst, new_prev);
                        if (debug) {
                            llvm::outs() << "\nOUT: ";
                            printDataFlowValuesForward(new_prev);
                        }

                        prev = getForwardComponentAtOutOfThisInstruction(inst);
                        if (debug) {
                            printLine(current_context_label, 1);
                        }

                    } // end else
                    // MSCHANGE
                    // setForwardOut(current_pair.first, current_pair.second,
                    // prev);
                } // end for
            } else {
                // NOT SLIM
                F prev = getForwardComponentAtInOfThisInstruction(*(b.begin()));
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    Instruction &I = *inst;
                    if (CallInst *ci = dyn_cast<CallInst>(&I)) {
                        Function *target_function = ci->getCalledFunction();
                        if (not target_function || target_function->isDeclaration() ||
                            isAnIgnorableDebugInstruction(&I)) {
                            continue; // this is an inbuilt function so doesn't
                                      // need to be processed.
                        }

                        // TODO find caller - callee return arg mapping for NOT
                        // SLIM case
                        SLIMOperand *returnInCaller = nullptr;
                        SLIMOperand *returnInCallee = nullptr;
                        std::pair<SLIMOperand *, SLIMOperand *> return_operand_map =
                            make_pair(returnInCaller, returnInCallee);

                        /*
                        At the call instruction, the value at IN should be
                        splitted into two components: 1) Purely Global and 2)
                        Mixed. The purely global component is given to the start
                        of callee.
                        */

                        // step 12
                        pair<F, B> inflow_pair =
                            CallInflowFunction(current_context_label, target_function, bb, a1, d1, return_operand_map);
                        F a2 = inflow_pair.first;
                        B d2 = inflow_pair.second;

                        F new_outflow_forward;
                        B new_outflow_backward;

                        // step 13

                        setForwardComponentAtInOfThisInstruction(&(*inst),
                                                                 prev); // compute IN from previous OUT-value
                        int matching_context_label = 0;
                        matching_context_label = check_if_context_already_exists(
                            target_function, inflow_pair, make_pair(new_outflow_forward, new_outflow_backward));
                        if (matching_context_label > 0) // step 15
                        {
                            if (debug) {
                                printLine(current_context_label, 0);
                                llvm::outs() << *inst << "\n";
                                llvm::outs() << "IN: ";
                            }
                            // step 14
                            pair<int, Instruction *> mypair = make_pair(current_context_label, &(*inst));
                            context_transition_graph[mypair] = matching_context_label;
                            if (debug) {
                                printDataFlowValuesForward(a1);
                            }

                            // step 16 and 17
                            F a3 = getForwardOutflowForThisContext(matching_context_label);
                            B d3 = getBackwardOutflowForThisContext(matching_context_label);

                            pair<F, B> outflow_pair;
                            outflow_pair = CallOutflowFunction(
                                current_context_label, getCurrentInstruction(), target_function, bb, a3, d3, a1, d1,
                                return_operand_map);
                            F value_to_be_meet_with_prev_out = outflow_pair.first;
                            B d4 = outflow_pair.second;

                            // step 18 and 19

                            /*
                            At the call instruction, the value at IN should be
                            splitted into two components. The purely global
                            component is given to the callee while the mixed
                            component is propagated to OUT of this instruction
                            after executing computeOUTfromIN() on it.
                            */

                            F a5 = getPurelyLocalComponentForward(a1);
                            /*
                            At the OUT of this instruction, the value from END
                            of callee procedure is to be merged with the
                            local(mixed) value propagated from IN. Note that
                            this merging "isn't" exactly (necessarily) the meet
                            between these two values.
                            */

                            /*
                            As explained in ip-vasco,pdf, we need to perform
                            meet with the original value of OUT of this
                            instruction to avoid the oscillation problem.
                            */

                            // Merging value from END of callee with value
                            // propagated from IN isn't necessarily meet Thus,
                            // adding new function performMergeAtCallForward()
                            setForwardComponentAtOutOfThisInstruction(
                                &(*inst), performMeetForward(
                                              getForwardComponentAtOutOfThisInstruction(*inst),
                                              performMergeAtCallForward(value_to_be_meet_with_prev_out, a5), "OUT"));

                            // setForwardComponentAtOutOfThisInstruction(&(*inst),
                            // performMeetForward(
                            //         performMeetForward(value_to_be_meet_with_prev_out,
                            //                            getForwardComponentAtOutOfThisInstruction(*inst),
                            //                            "OUT"), a5, "OUT"));
                            prev = getForwardComponentAtOutOfThisInstruction((*inst));
                            if (debug) {
                                llvm::outs() << "\nOUT: ";
                                printDataFlowValuesForward(prev);
                                printLine(current_context_label, 1);
                            }
                        } else // step 20
                        {
                            // creating a new context
                            int prev_callee_context_label = getCalleeContext(current_context_label, &(*inst));
                            if (prev_callee_context_label == -1)
                                INIT_CONTEXT(
                                    target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward}); // step 21
                            else
                                INIT_CONTEXT(
                                    target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward},
                                    prev_callee_context_label);

                            // INIT_CONTEXT(target_function, {a2, d2},
                            // {new_outflow_forward, new_outflow_backward});
                            // //step 21

                            // step 14
                            // This step must be done after above step because
                            // context label counter gets updated after
                            // INIT-Context is invoked.
                            pair<int, Instruction *> mypair = make_pair(current_context_label, &(*inst));
                            context_transition_graph[mypair] = getContextLabelCounter();
                        }
                    } else {
                        if (debug) {
                            printLine(current_context_label, 0);
                            llvm::outs() << *inst << "\n";
                            llvm::outs() << "IN: ";
                            printDataFlowValuesForward(prev);
                        }
                        setForwardComponentAtInOfThisInstruction(&(*inst),
                                                                 prev); // compute IN from previous OUT-value
                        F new_prev = computeOutFromIn(*inst);
                        if (debug)
                            llvm::outs() << "Called ComputeOutFromIn-2";
                        setForwardComponentAtOutOfThisInstruction(&(*inst), new_prev);
                        if (debug) {
                            llvm::outs() << "\nOUT: ";
                            printDataFlowValuesForward(new_prev);
                        }
                        prev = getForwardComponentAtOutOfThisInstruction(*inst);
                        if (debug) {
                            printLine(current_context_label, 1);
                        }
                    }
                }
                // setForwardOut(current_pair.first, current_pair.second, prev);
            }
        } else { // Step 22
            setForwardOut(current_pair.first, current_pair.second, NormalFlowFunctionForward(current_pair));
            // Commented as NormalFlowFunction is printing the dataflow values
            // printDataFlowValuesForward(NormalFlowFunctionForward(current_pair));
        }

        bool changed = false;
        std::set<BasicBlock *> setBB_to_insert_in_FWL;
        // MSCHANGE 7
        // if (not
        // EqualDataFlowValuesForward(previous_value_at_out_of_this_node,
        // getOut(current_pair.first,
        //                                                                            current_pair.second).first)) {
        if (not EqualDataFlowValuesForward(
                getOut(current_pair.first, current_pair.second).first, previous_value_at_out_of_this_node)) {
            changed = true;
        }

#ifdef PRVASCO
        llvm::outs() << "\n";
        string chg = (changed == true) ? "TRUE" : "FALSE";
        llvm::outs() << "Is OUT value of the node changed in Forward Analysis: " << chg;
        llvm::outs() << "\n(Previous OUT value: ";
        printDataFlowValuesForward(previous_value_at_out_of_this_node);
        llvm::outs() << " , Current OUT value: ";
        printDataFlowValuesForward(getOut(current_pair.first, current_pair.second).first);
        llvm::outs() << ")\n";
#endif

        // Set flag in context object
        if (changed) {
            context_label_to_context_object_map[current_pair.first]->setDFValueChange(true);
        }

        if (changed && modeOfExec == FSFP) // step 24
        {
// #ifdef LFCPA
            // not yet converged
            for (auto succ_bb : successors(bb)) // step 25
            {
#ifdef PRVASCO
                llvm::outs() << "\n";
                llvm::outs() << "Inserting successors into Forward Worklist on change "
                                "in OUT value in Forward Analysis (Context:"
                             << current_context_label << ")\n";
                for (Instruction &I : *succ_bb)
                    llvm::outs() << I << "\n";
#endif
                // step 26
                forward_worklist.workInsert(make_pair(make_pair(current_context_label, succ_bb), false));
            }
        }
        bool changed1 = false;
        // MSCHANGE 8
        // if (not EqualDataFlowValuesForward(previous_value_at_in_of_this_node,
        // getIn(current_pair.first,
        //                                                                            current_pair.second).first)) {
        if (not EqualDataFlowValuesForward(
                getIn(current_pair.first, current_pair.second).first, previous_value_at_in_of_this_node)) {
            changed1 = true;
        }

#ifdef PRVASCO
        string chg1 = (changed1 == true) ? "TRUE" : "FALSE";
        llvm::outs() << "\nIs IN value of the node changed in Forward Analysis: " << chg1;
        llvm::outs() << "\n(Previous IN value: ";
        printDataFlowValuesForward(previous_value_at_in_of_this_node);
        llvm::outs() << " , Current IN value: ";
        printDataFlowValuesForward(getIn(current_pair.first, current_pair.second).first);
        llvm::outs() << ")\n";
#endif

        // Set flag in context object
        if (changed1) {
            context_label_to_context_object_map[current_pair.first]->setDFValueChange(true);
        }

        bool changed2 = false;

        if (changed1 && modeOfExec == FSFP) { // Step 24

#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Inserting node into Backward Worklist on change in IN "
                            "value in Forward Analysis (Context:"
                         << current_context_label << ")\n";
            for (Instruction &I : *bb)
                llvm::outs() << I << "\n";
#endif
            backward_worklist.workInsert(make_pair(make_pair(current_context_label, bb), false));
        }

        // Add all predecessor callers in the order in which it is called
        // If main() calls P() calls Q() and we are processing the inst in Q
        // We add them in same order(main first, then P, then Q)
        // because the current bb at Q should be on top to be analyzed first
        if (changed2 && modeOfExec == FSFP) { // Step 24

#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Inserting node and all of it's predecessor caller nodes "
                            "into Backward Worklist on change in IN value in Forward "
                            "Analysis (Context:"
                         << current_context_label << ")\n";
            for (Instruction &I : *bb)
                llvm::outs() << I << "\n";
#endif
            stack<pair<pair<int, BasicBlock *>, bool>> worklist_elements;
            stack<pair<pair<int, BasicBlock *>, bool>> reversed_worklist_elements;
            unordered_set<pair<int, BasicBlock *>, HashFunction> visited;
            worklist_elements.push(make_pair(make_pair(current_context_label, bb), true));
            visited.insert(make_pair(current_context_label, bb));
            while (!worklist_elements.empty()) {
                pair<pair<int, BasicBlock *>, bool> top_ele = worklist_elements.top();
                worklist_elements.pop();
                reversed_worklist_elements.push(top_ele);
                vector<pair<int, BasicBlock *>> caller_contexts =
                    SLIM_transition_graph.getCallers(current_context_label);
                for (pair<int, BasicBlock *> caller : caller_contexts) {
                    if (visited.find(caller) == visited.end()) {
                        visited.insert(caller);
                        worklist_elements.push(make_pair(caller, true));
                    }
                }
            }
            while (!reversed_worklist_elements.empty()) {
                backward_worklist.workInsert(reversed_worklist_elements.top());
                reversed_worklist_elements.pop();
            }
        }

        // if (bb == &function.back() /*and setBB_to_insert_in_FWL.size() == 0*/
        // /*END_Q issue*/) // step 27
        if (bb == optIR->getLastBasicBlock(f) /*and setBB_to_insert_in_FWL.size() == 0*/
            /*END_Q issue*/)                  // step 27
        {
            // last node
            // step 28

            setForwardOutflowForThisContext(
                current_context_label, getPurelyGlobalComponentForward(getOut(
                                                                           current_pair.first,
                                                                           current_pair.second)
                                                                           .first)); // setting forward outflow
            bool flag = false;
            if (SLIM) {
                if (debug)
                    llvm::outs() << "\nChecking CTG to insert callers into worklist\n";

                vector<pair<int, BasicBlock *>> caller_contexts =
                    SLIM_transition_graph.getCallers(current_context_label);

                // llvm::outs() << "\n Caller contexts vector size =
                // "<<caller_contexts.size();
                for (auto context_bb_pair : caller_contexts) {
                    // step 30
                    BasicBlock *bb = context_bb_pair.second;

                    // Checking if callee has change and computeGenerative is to
                    // be called again
                    // mod:AR No longer needed
                    bool changeInFunc = context_label_to_context_object_map[current_context_label]->isDFValueChanged();
                    bool isCalleeAnalysed = true;
                    if (changeInFunc) {
                        isCalleeAnalysed = false;
                    }
                    /// bool isCalleeAnalysed = true;//mod:AR retained to keep
                    /// code changes to minimum

#ifdef PRVASCO
                    llvm::outs() << "\nInserting caller node into Forward Worklist at "
                                    "end of callee in Forward Analysis (Context:"
                                 << current_context_label << " to " << context_bb_pair.first << ")\n";
                    for (Instruction &I : *bb)
                        llvm::outs() << I << "\n";
#endif
                    forward_worklist.workInsert(make_pair(context_bb_pair, isCalleeAnalysed));
                    if (direction == "bidirectional") {
#ifdef PRVASCO
                        llvm::outs() << "\nInserting caller node into Backward Worklist "
                                        "at "
                                        "end of callee in Forward Analysis (Context:"
                                     << current_context_label << " to " << context_bb_pair.first << ")\n";
                        for (Instruction &I : *bb)
                            llvm::outs() << I << "\n";
#endif
                        //@NOTE: Insert the caller into FWL for forwards
                        // analysis and not into BWL.
                        // backward_worklist.workInsert(make_pair(context_bb_pair,
                        // isCalleeAnalysed));
                    }
                } // for mod:AR No break present here.
            } else {
                for (auto context_inst_pairs : context_transition_graph) {
                    if (context_inst_pairs.second == current_context_label) {
                        // step 30
                        BasicBlock *bb = context_inst_pairs.first.second->getParent();
                        pair<int, BasicBlock *> context_bb_pair = make_pair(context_inst_pairs.first.first, bb);

                        // Checking if callee has change and computeGenerative
                        // is to be called again
                        bool changeInFunc =
                            context_label_to_context_object_map[current_context_label]->isDFValueChanged();
                        bool isCalleeAnalysed = true;
                        if (changeInFunc) {
                            isCalleeAnalysed = false;
                        }

                        forward_worklist.workInsert(make_pair(context_bb_pair, isCalleeAnalysed));
                        if (direction == "bidirectional") {
                            backward_worklist.workInsert(make_pair(context_bb_pair, isCalleeAnalysed));
                        }
                        // break;
                    }
                }
            }
        }
        process_mem_usage(this->vm, this->rss);
        this->total_memory = max(this->total_memory, this->vm);
    }
#ifdef PRINTSTATS
    mapforwardsRoundBB[forwardsRoundCount] = forwardsBBPerRound; // #BBCOUNTPERROUND
#endif
}

template <class F, class B>
F Analysis<F, B>::NormalFlowFunctionForward(pair<int, BasicBlock *> current_pair_of_context_label_and_bb) {
    BasicBlock &b = *(current_pair_of_context_label_and_bb.second);
    F prev = getIn(current_pair_of_context_label_and_bb.first, current_pair_of_context_label_and_bb.second).first;
    Context<F, B> *context_object = context_label_to_context_object_map[current_pair_of_context_label_and_bb.first];
    bool changed = false;
    // traverse a basic block in forward direction
    if (SLIM) {
        for (auto &index : optIR->func_bb_to_inst_id[{context_object->getFunction(), &b}]) {
            auto &inst = optIR->inst_id_to_object[index];

            F old_IN = getForwardComponentAtInOfThisInstruction(inst);
            // if change in data flow value at IN of any instruction, add this
            // basic block to backward worklist
            if (!EqualDataFlowValuesForward(prev, old_IN))
                changed = true;

            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 0);
                llvm::outs() << "\nProcessing instruction at INDEX = " << index << " " << inst;
                inst->printInstruction();
                llvm::outs() << "\nIN: ";
                printDataFlowValuesForward(prev);
                llvm::outs() << "\n SET FORWARD----5:::";
            }

// mod:AR
// #ifndef LFCPA
            //-----------------
            // llvm::outs() << "\n SET PIN 78";
            // inst->printInstruction();
            setForwardComponentAtInOfThisInstruction(inst, prev);

            F new_prev;
            if (isIgnorableInstruction(inst)) { // || inst->isIgnored()){
                if (debug)
                    llvm::outs() << "\nIgnoring...\n";
                new_prev = prev;
            } else {
                new_prev = computeOutFromIn(inst);
            }

            if (debug)
                llvm::outs() << "Called ComputeOutFromIn-3";
            setForwardComponentAtOutOfThisInstruction(inst, new_prev);
            if (debug) {
                llvm::outs() << "\nOUT: ";
                printDataFlowValuesForward(new_prev);
            }
            prev = getForwardComponentAtOutOfThisInstruction(inst);
            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 1);
            }
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }

        // Set flag in context object
        if (changed) {
            context_label_to_context_object_map[current_pair_of_context_label_and_bb.first]->setDFValueChange(true);
        }

        if (changed && modeOfExec == FSFP) {
#ifdef PRVASCO
            llvm::outs() << "\nInserting node into Backward Worklist on change in IN "
                            "value within BB in Forward Analysis (Context:"
                         << current_pair_of_context_label_and_bb.first << ")\n";
            for (Instruction &I : b)
                llvm::outs() << I << "\n";
#endif
            backward_worklist.workInsert(make_pair(current_pair_of_context_label_and_bb, false));
        }
    } else {
        for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 0);
                llvm::outs() << *inst << "\n";
                llvm::outs() << "IN: ";
                printDataFlowValuesForward(prev);
            }

            if (debug)
                llvm::outs() << "\n SET FORWARD----6:: ";
            // mod: AR

            F old_pin = getForwardComponentAtInOfThisInstruction(*inst);
// #ifndef LFCPA
            //------
            /// llvm::outs() << "\n SET PIN 8";
            setForwardComponentAtInOfThisInstruction(&(*inst), prev); // compute IN from previous OUT-value
            F new_prev = computeOutFromIn(*inst);

            if (debug)
                llvm::outs() << "Called ComputeOutFromIn-4";
            // check for change within BasicBlock
            setForwardComponentAtOutOfThisInstruction(&(*inst), new_prev);
            if (debug) {
                llvm::outs() << "\nOUT: ";
                printDataFlowValuesForward(new_prev);
            }
            prev = getForwardComponentAtOutOfThisInstruction(*inst);
            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 1);
            }
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
    }
    return prev;
}

template <class F, class B>
void Analysis<F, B>::doAnalysisBackward() {
    long backwardsBBPerRound = 0; // #BBCOUNTPERROUND
#ifdef PRINTSTATS
    backwardsRoundCount += 1;
    // llvm::outs() << "\n BACKWARD ANALYSIS ROUND: "<<backwardsRoundCount;
#endif

    while (not backward_worklist.empty()) // step 2
    {
#ifdef PRINTSTATS
        B_BBanalysedCount++;
        backwardsBBPerRound++;
#endif
        // step 3 and 4
        pair<pair<int, BasicBlock *>, bool> worklistElement = backward_worklist.workDelete();
        pair<int, BasicBlock *> current_pair = worklistElement.first;
        int current_context_label;
        BasicBlock *bb;
        current_context_label = current_pair.first;
        bool calleeAnalysed = worklistElement.second;
        setProcessingContextLabel(current_context_label);
        bb = current_pair.second;
        // mod:AR
        insertIntoBackwardsBBList(bb);

#ifdef PRVASCO
        llvm::outs() << "\n\nPopping from Backward Worklist (Context:" << current_context_label
                     << " calleeAnalysed: " << calleeAnalysed << ")\n";
        for (Instruction &I : *bb)
            llvm::outs() << I << "\n";
#endif
        BasicBlock &b = *bb;
        Function *f = context_label_to_context_object_map[current_context_label]->getFunction();
        Function &function = *f;

        // MSCHANGE 2
        B previous_value_at_in_of_this_node = getIn(
                                                  current_pair.first,
                                                  current_pair.second)
                                                  .second; // CS_BB_IN[current_pair].second;
        B previous_value_at_out_of_this_node = getOut(current_pair.first, current_pair.second).second; // prev Lout

        // step 5
        auto pos = optIR->getExitBBList(f)->find(bb);
        if (pos != optIR->getExitBBList(f)->end()) { // found exit block
            // copy OUT to IN
            setBackwardIn(
                current_pair.first, current_pair.second, getOut(current_pair.first, current_pair.second).second);
        } else if (bb != optIR->getLastBasicBlock(f)) {
            // step 6
            setBackwardOut(current_pair.first, current_pair.second, getInitialisationValueBackward());
            // step 7 and 8
            for (auto succ_bb : successors(bb)) {
#ifdef PRVASCO
                llvm::outs() << "Updating OUT by meet with :";
                printDataFlowValuesBackward(getBackwardIn(current_pair.first, succ_bb));
#endif
                setBackwardOut(
                    current_pair.first, current_pair.second,
                    performMeetBackward(
                        getOut(current_pair.first, current_pair.second).second,
                        getIn(current_pair.first, succ_bb).second));
                // auto &index :
                // optIR->getReverseInstList(optIR->func_bb_to_inst_id[{f,
                // bb}])[0];
            }
        } else {
            // Out value of this node is same as INFLOW value
            setBackwardOut(
                current_pair.first, current_pair.second, getBackwardInflowForThisContext(current_context_label));

            // Boundary information recomputation, if function last basic block
            // of 'main'
            if (current_pair.second->getParent()->getName() == "main") {
                BaseInstruction *lastIns =
                    optIR->getInstrFromIndex(optIR->getLastIns(current_pair.second->getParent(), current_pair.second));
                setBackwardOut(current_pair.first, current_pair.second, getMainBoundaryInformationBackward(lastIns));
            }
        }

        // step 9
        F a1 = getIn(current_pair.first, current_pair.second).first;   // CS_BB_IN[current_pair].first;
        B d1 = getOut(current_pair.first, current_pair.second).second; // CS_BB_OUT[current_pair].second;

#ifdef PRVASCO
        llvm::outs() << "\nPrevious IN value at the node in Backward Analysis";
        printDataFlowValuesBackward(previous_value_at_in_of_this_node);
        llvm::outs() << "\nPrevious OUT value at the node in Backward Analysis";
        printDataFlowValuesBackward(previous_value_at_out_of_this_node);
        llvm::outs() << "\nOUT value at the node in Backward Analysis current "
                        "(before processing the instr)";
        printDataFlowValuesBackward(d1);
        llvm::outs() << "\n";

#endif

        // step 10
        bool contains_dynamic_allocation = false;
        bool contains_a_method_call = false;
        if (SLIM) {
            for (auto &index : optIR->func_bb_to_inst_id[{f, bb}]) {
                auto &inst = optIR->inst_id_to_object[index];
                setCurrentInstruction(inst);
                if (inst->getCall()) {
                    Instruction *Inst = inst->getLLVMInstruction();

                    if (inst->getIsDynamicAllocation()) {
#ifndef IGNORE
                        contains_dynamic_allocation = true;

#endif

                        continue;
                    }
                    CallInstruction *cTempInst = new CallInstruction(Inst);
                    Function *target_function = cTempInst->getCalleeFunction();
                    if (target_function && (target_function->isDeclaration() || isAnIgnorableDebugInstruction(Inst))) {
                        if (debug) {
                            llvm::outs() << " Ignoring instruction: ";
                            inst->printInstruction();
                        }
                        continue; // this is an inbuilt function so doesn't need
                                  // to be processed.
                    }
                    contains_a_method_call = true;
                }
            }
        } else {
            for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                Instruction &I = *inst;
                if (CallInst *ci = dyn_cast<CallInst>(&I)) {
                    Function *target_function = ci->getCalledFunction();
                    if (not target_function || target_function->isDeclaration() || isAnIgnorableDebugInstruction(&I)) {
                        continue;
                    }
                    contains_a_method_call = true;
                    break;
                }
            }
        }
        if (contains_a_method_call) {
            B prevOut = getBackwardComponentAtOutOfThisInstruction(
                optIR->inst_id_to_object[optIR->func_bb_to_inst_id[{f, bb}].back()]);
            B prev = getBackwardComponentAtOutOfThisInstruction(
                optIR->inst_id_to_object[optIR->func_bb_to_inst_id[{f, bb}].back()]);
            if (debug) {
                llvm::outs() << "\n OUT at the instruction which is displayed as OUT ";
                printDataFlowValuesBackward(prev);
                printDataFlowValuesBackward(d1);
            }
            // step 11
            if (SLIM) {
                for (auto &index : optIR->getReverseInstList(optIR->func_bb_to_inst_id[{f, bb}])) {
                    auto &inst = optIR->inst_id_to_object[index];
                    a1 = getForwardComponentAtInOfThisInstruction(inst);
                    if (inst->getCall()) {
                        bool contains_indirect_call = false;
                        Instruction *Inst = inst->getLLVMInstruction();
                        CallInst *ci = dyn_cast<CallInst>(Inst);
                        CallInstruction *cTempInst = new CallInstruction(Inst);
                        set<Function *> indirect_functions;
                        if (cTempInst->isIndirectCall()) {
                            if (debug)
                                llvm::outs() << "\nINDIRET CALL DETECTED within "
                                                "doAnalysiBackward:contains a method "
                                                "call.\n";

                            contains_indirect_call = true;
                            SLIMOperand *function_operand = cTempInst->getIndirectCallOperand();
                            indirect_functions =
                                handleFunctionPointers(a1, function_operand, current_context_label, inst);
                        }
                        if (contains_indirect_call) {
                            if (indirect_functions.size() == 0) {
                                setBackwardComponentAtOutOfThisInstruction(inst, prevOut);
                                setBackwardComponentAtInOfThisInstruction(
                                    inst, performMeetBackward(
                                              computeInFromOutForIndirectCalls(inst),
                                              getBackwardComponentAtInOfThisInstruction(inst)));
                            }
                            for (Function *target_function : indirect_functions) {
                                // get the return variable in callee to map it
                                // with the variable in caller Example: z = call
                                // Q() and defintion of Q(){ ... return x;} ,
                                // pass pair <z, x> to compute Inflow/Outflow
                                SLIMOperand *returnInCaller = nullptr;
                                SLIMOperand *returnInCallee = nullptr;
                                std::pair<SLIMOperand *, int> callerRetOperand = inst->getLHS();
                                if (callerRetOperand.second >= 0) {
                                    SLIMOperand *calleeRet =
                                        callerRetOperand.first->getCalleeReturnOperand(target_function);
                                    if (!(calleeRet == nullptr || calleeRet->getOperandType() == NULL_OPERAND)) {
                                        returnInCaller = callerRetOperand.first;
                                        returnInCallee = calleeRet;
                                    }
                                }
                                std::pair<SLIMOperand *, SLIMOperand *> return_operand_map =
                                    make_pair(returnInCaller, returnInCallee);

                                if (intraprocFlag) {
                                    /*  If intraprocFlag is true, then target
                                       function is not analysed, instead the
                                       data flow value is taken from user based
                                       on how the effect is defined
                                    */
                                    setBackwardComponentAtOutOfThisInstruction(inst, prevOut);

                                    B inOfFunction = functionCallEffectBackward(target_function, prevOut);

                                    if (debug) {
                                        printLine(current_context_label, 0);
                                        llvm::outs() << "\nProcessing instruction at "
                                                        "INDEX = "
                                                     << index;
                                        inst->printInstruction();
                                        llvm::outs() << "\nOUT: ";
                                        printDataFlowValuesBackward(prevOut);
                                    }

                                    setBackwardComponentAtInOfThisInstruction(
                                        inst, performMeetBackward(
                                                  inOfFunction, getBackwardComponentAtInOfThisInstruction(inst)));

                                    prev = getBackwardComponentAtInOfThisInstruction(inst);
                                    if (debug) {
                                        llvm::outs() << "\nIN: ";
                                        printDataFlowValuesBackward(prev);
                                        printLine(current_context_label, 1);
                                    }

                                } else { // not intraprocedural

                                    int matching_context_label = 0;
                                    F a2;
                                    B d2;

                                    F new_outflow_forward;
                                    B new_outflow_backward;
                                    int prev_callee_context_label = getSourceContextAtCallSiteForFunction(
                                        current_context_label, inst, target_function);
                                    if (!calleeAnalysed) {
                                        pair<F, B> inflow_pair;
                                        if (___e01) {
                                            // TODO
                                        } else {
                                            inflow_pair = CallInflowFunction(
                                                current_context_label, target_function, bb, a1, prevOut,
                                                prev_callee_context_label /*add3*/
                                                ,
                                                return_operand_map);
                                        }

                                        // step 12
                                        a2 = inflow_pair.first;
                                        d2 = inflow_pair.second;

                                        // step 13

                                        setBackwardComponentAtOutOfThisInstruction(inst, prevOut);

                                        matching_context_label = check_if_context_already_exists(
                                            target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward});

                                    } else {
                                        // Callee is analysed, thus fetch the
                                        // callee context from Context
                                        // transition graph
                                        matching_context_label = SLIM_transition_graph.getCalleeContext(
                                            current_context_label, inst, target_function);
                                    }

                                    if (matching_context_label > 0) // step 15
                                    {
#ifdef PRINTSTATS
                                        contextReuseCount++;
#endif
                                        if (debug) {
                                            printLine(current_context_label, 0);
                                            llvm::outs() << "\nProcessing instruction "
                                                            "at INDEX = "
                                                         << index;
                                            inst->printInstruction();
                                            llvm::outs() << "\nOUT: ";
                                            printDataFlowValuesBackward(prevOut);
                                        }
                                        // step 14
                                        pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                        // SRI-- Commented
                                        // SLIM_context_transition_graph[mypair]
                                        // = matching_context_label;
                                        Function *targ_func =
                                            context_label_to_context_object_map[matching_context_label]->getFunction();
                                        if (debug)
                                            llvm::outs() << "\nAdding edge for matching "
                                                            "context in backwards  "
                                                         << matching_context_label;
                                        SLIM_transition_graph.addEdge(mypair, matching_context_label, targ_func);
#ifdef PRVASCO
                                        llvm::outs() << "\nUpdating context transition "
                                                        "graph (c"
                                                     << current_context_label << ", i" << index << ") -->"
                                                     << matching_context_label << "\n";
#endif

                                        // step 16 and 17
                                        F a3 = getForwardOutflowForThisContext(matching_context_label);
                                        B d3 = getBackwardOutflowForThisContext(matching_context_label);

                                        pair<F, B> outflow_pair;
                                        outflow_pair = CallOutflowFunction(
                                            current_context_label, inst, target_function, bb, a3, d3, a1, prevOut,
                                            return_operand_map);

                                        F a4 = outflow_pair.first;
                                        B value_to_be_meet_with_prev_in = outflow_pair.second;
                                        // step 18 and 19

                                        /*
                                        At the call instruction, the value at
                                        OUT should be splitted into two
                                        components. The purely global component
                                        is given to the callee while the mixed
                                        component is propagated to IN of this
                                        instruction after executing
                                        computeINfromOUT() on it.
                                        */

                                        B a5;
                                        if (___e01) {
                                            // TODO
                                        } else {
                                            a5 = getPurelyLocalComponentBackward(prevOut);
                                            if (returnInCallee != nullptr)
                                                a5 = performCallReturnArgEffectBackward(a5, return_operand_map);
                                        }

                                        /*
                                        At the IN of this instruction, the value
                                        from START of callee procedure is to be
                                        merged with the local(mixed) value
                                        propagated from OUT. Note that this
                                        merging "isn't" exactly (necessarily)
                                        the meet between these two values.
                                        */

                                        /*
                                        As explained in ip-vasco,pdf, we need to
                                        perform meet with the original value of
                                        IN of this instruction to avoid the
                                        oscillation problem.
                                        */

                                        if (debug) {
                                            llvm::outs() << "\nBefore merging in "
                                                            "Backward analysis:";
                                            printDataFlowValuesBackward(
                                                getBackwardComponentAtInOfThisInstruction(inst));
                                            llvm::outs() << "\n";
                                            printDataFlowValuesBackward(value_to_be_meet_with_prev_in);
                                            llvm::outs() << "\n";
                                            printDataFlowValuesBackward(a5);
                                        }
                                        a5 = performMergeAtCallBackward(computeInFromOutForIndirectCalls(inst), a5);
                                        // Parameter Mapping
                                        B LinStart_at_targetfunction =
                                            getBackwardOutflowForThisContext(SLIM_transition_graph.getCalleeContext(
                                                current_context_label, inst, target_function));

                                        pair<F, B> parameter_mapped_outflow = performParameterMapping(
                                            inst, target_function, a1, LinStart_at_targetfunction);

                                        setBackwardComponentAtInOfThisInstruction(
                                            inst, performMeetBackward(
                                                      getBackwardComponentAtInOfThisInstruction(inst),
                                                      performMergeAtCallBackward(parameter_mapped_outflow.second, a5)));

                                        prev = getBackwardComponentAtInOfThisInstruction(inst);
                                        if (debug) {
                                            llvm::outs() << "\nIN: ";
                                            printDataFlowValuesBackward(prev);
                                            printLine(current_context_label, 1);
                                        }

                                    } // matching context
                                    else // step 20
                                    {
                                        /*
                                        If the call instruction is encountered
                                        before callee is analysed (i.e with
                                        calleeAnalysed as false) and there is no
                                        matching context existing maintain the
                                        caller basic block with calleeAnalysed
                                        as true and create new context
                                        */
                                        backward_worklist.workInsert(make_pair(current_pair, true));
#ifdef PRVASCO
                                        llvm::outs() << "\nInserting caller node into "
                                                        "Backward Worklist "
                                                        "with calleeAnalysed as TRUE "
                                                        "(Context:"
                                                     << current_context_label << ")\n";
                                        for (Instruction &I : *bb)
                                            llvm::outs() << I << "\n";
#endif

                                        if (prev_callee_context_label == -1)
                                            INIT_CONTEXT(
                                                target_function, {a2, d2},
                                                {new_outflow_forward, new_outflow_backward}); // step
                                                                                              // 21
                                        else
                                            INIT_CONTEXT(
                                                target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward},
                                                prev_callee_context_label);

                                        pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                        // step 14
                                        if (debug)
                                            llvm::outs() << "\nAdding edge for new "
                                                            "context in backwards  "
                                                         << getContextLabelCounter();
                                        SLIM_transition_graph.addEdge(
                                            mypair, getContextLabelCounter(), target_function);
#ifdef PRVASCO
                                        llvm::outs() << "\nUpdating context transition "
                                                        "graph (c"
                                                     << current_context_label << ", i" << index << ") -->"
                                                     << getContextLabelCounter() << "\n";
#endif

#ifdef PRVASCO
                                        llvm::outs() << "\nAfter New Context Creation, "
                                                        "IN value at "
                                                        "the node in Backward Analysis "
                                                        "(Context: "
                                                     << current_pair.first << ") ";
                                        printDataFlowValuesBackward(
                                            getIn(current_pair.first, current_pair.second).second);
                                        llvm::outs() << "\nAfter New Context Creation, "
                                                        "OUT value at the "
                                                        "node in Backward Analysis "
                                                        "(Context: "
                                                     << current_pair.first << ") ";
                                        printDataFlowValuesBackward(
                                            getOut(current_pair.first, current_pair.second).second);
                                        llvm::outs() << "\n";
#endif
                                    }
                                }
                            }
                        } else {
                            // Not Indirect call
                            Function *target_function = cTempInst->getCalleeFunction();

                            if (target_function &&
                                (target_function->isDeclaration() || isAnIgnorableDebugInstruction(Inst))) {
                                if (debug) {
                                    llvm::outs() << " Ignoring instruction: ";
                                    inst->printInstruction();
                                }
                                continue; // this is an inbuilt function so
                                          // doesn't need to be processed.
                            }

                            // get the return variable in callee to map it with
                            // the variable in caller Example: z = call Q() and
                            // defintion of Q(){ ... return x;} , pass pair <z,
                            // x> to compute Inflow/Outflow
                            SLIMOperand *returnInCaller = nullptr;
                            SLIMOperand *returnInCallee = nullptr;
                            std::pair<SLIMOperand *, int> callerRetOperand = inst->getLHS();
                            if (callerRetOperand.second >= 0) {
                                SLIMOperand *calleeRet = callerRetOperand.first->getCalleeReturnOperand();
                                if (!(calleeRet == nullptr || calleeRet->getOperandType() == NULL_OPERAND)) {
                                    returnInCaller = callerRetOperand.first;
                                    returnInCallee = calleeRet;
                                }
                            }
                            std::pair<SLIMOperand *, SLIMOperand *> return_operand_map =
                                make_pair(returnInCaller, returnInCallee);

                            if (intraprocFlag) {
                                /*  If intraprocFlag is true, then target
                                   function is not analysed, instead the data
                                   flow value is taken from user based on how
                                   the effect is defined
                                */
                                setBackwardComponentAtOutOfThisInstruction(inst, prev);

                                B inOfFunction = functionCallEffectBackward(target_function, prev);

                                if (debug) {
                                    printLine(current_context_label, 0);
                                    llvm::outs() << "\nProcessing instruction "
                                                    "at INDEX = "
                                                 << index;
                                    inst->printInstruction();
                                    llvm::outs() << "\nOUT: ";
                                    printDataFlowValuesBackward(prev);
                                }

                                setBackwardComponentAtInOfThisInstruction(
                                    inst,
                                    performMeetBackward(inOfFunction, getBackwardComponentAtInOfThisInstruction(inst)));

                                prev = getBackwardComponentAtInOfThisInstruction(inst);
                                if (debug) {
                                    llvm::outs() << "\nIN: ";
                                    printDataFlowValuesBackward(prev);
                                    printLine(current_context_label, 1);
                                }

                            } else {
                                int matching_context_label = 0;
                                F a2;
                                B d2;

                                F new_outflow_forward;
                                B new_outflow_backward;
                                int prev_callee_context_label =
                                    getSourceContextAtCallSiteForFunction(current_context_label, inst, target_function);
                                if (!calleeAnalysed) {
                                    pair<F, B> inflow_pair;
                                    if (___e01) {
                                        // TODO
                                    } else {
                                        inflow_pair = CallInflowFunction(
                                            current_context_label, target_function, bb, a1, prev,
                                            prev_callee_context_label /*add4*/, return_operand_map);
                                    }

                                    // step 12
                                    a2 = inflow_pair.first;
                                    d2 = inflow_pair.second;

                                    // step 13

                                    setBackwardComponentAtOutOfThisInstruction(inst, prev);

                                    matching_context_label = check_if_context_already_exists(
                                        target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward});

                                } else {
                                    // Callee is analysed, thus fetch the callee
                                    // context from Context transition graph
                                    matching_context_label = SLIM_transition_graph.getCalleeContext(
                                        current_context_label, inst, target_function);
                                }

                                if (matching_context_label > 0) // step 15
                                {
#ifdef PRINTSTATS
                                    contextReuseCount++;
#endif
                                    if (debug) {
                                        printLine(current_context_label, 0);
                                        llvm::outs() << "\nProcessing instruction at "
                                                        "INDEX = "
                                                     << index;
                                        inst->printInstruction();
                                        llvm::outs() << "\nOUT: ";
                                        printDataFlowValuesBackward(prev);
                                    }
                                    // step 14
                                    pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                    // SRI--Commented
                                    // SLIM_context_transition_graph[mypair] =
                                    // matching_context_label;
                                    Function *targ_func =
                                        context_label_to_context_object_map[matching_context_label]->getFunction();
                                    if (debug)
                                        llvm::outs() << "\nAdding edge for matching "
                                                        "context in backwards  "
                                                     << matching_context_label;
                                    SLIM_transition_graph.addEdge(mypair, matching_context_label, targ_func);
#ifdef PRVASCO
                                    llvm::outs() << "\nUpdating context transition "
                                                    "graph (c"
                                                 << current_context_label << ", i" << index << ") -->"
                                                 << matching_context_label << "\n";
#endif

                                    // step 16 and 17
                                    F a3 = getForwardOutflowForThisContext(matching_context_label);
                                    B d3 = getBackwardOutflowForThisContext(matching_context_label);

                                    pair<F, B> outflow_pair;
                                    outflow_pair = CallOutflowFunction(
                                        current_context_label, inst, target_function, bb, a3, d3, a1, prev,
                                        return_operand_map);

                                    F a4 = outflow_pair.first;
                                    B value_to_be_meet_with_prev_in = outflow_pair.second;
                                    // step 18 and 19

                                    /*
                                    At the call instruction, the value at OUT
                                    should be splitted into two components. The
                                    purely global component is given to the
                                    callee while the mixed component is
                                    propagated to IN of this instruction after
                                    executing computeINfromOUT() on it.
                                    */

                                    B a5;
                                    if (___e01) {
                                        // TODO
                                    } else {
                                        a5 = getPurelyLocalComponentBackward(prev);
                                        if (returnInCallee != nullptr)
                                            a5 = performCallReturnArgEffectBackward(a5, return_operand_map);
                                    }

                                    /*
                                    At the IN of this instruction, the value
                                    from START of callee procedure is to be
                                    merged with the local(mixed) value
                                    propagated from OUT. Note that this merging
                                    "isn't" exactly (necessarily) the meet
                                    between these two values.
                                    */

                                    /*
                                    As explained in ip-vasco,pdf, we need to
                                    perform meet with the original value of IN
                                    of this instruction to avoid the oscillation
                                    problem.
                                    */

                                    setBackwardComponentAtInOfThisInstruction(
                                        inst, performMeetBackward(
                                                  getBackwardComponentAtInOfThisInstruction(inst),
                                                  performMergeAtCallBackward(value_to_be_meet_with_prev_in, a5)));

                                    prev = getBackwardComponentAtInOfThisInstruction(inst);
                                    if (debug) {
                                        llvm::outs() << "\nIN: ";
                                        printDataFlowValuesBackward(prev);
                                        printLine(current_context_label, 1);
                                    }

                                } // matching context
                                else // step 20
                                {
                                    /*
                                    If the call instruction is encountered
                                    before callee is analysed (i.e with
                                    calleeAnalysed as false) and there is no
                                    matching context existing maintain the
                                    caller basic block with calleeAnalysed as
                                    true and create new context
                                    */
                                    backward_worklist.workInsert(make_pair(current_pair, true));
#ifdef PRVASCO
                                    llvm::outs() << "\nInserting caller node into "
                                                    "Backward Worklist with "
                                                    "calleeAnalysed as TRUE (Context:"
                                                 << current_context_label << ")\n";
                                    for (Instruction &I : *bb)
                                        llvm::outs() << I << "\n";
#endif

                                    // creating a new context
                                    // int prev_callee_context_label =
                                    //     SLIM_transition_graph.getCalleeContext(
                                    //         current_context_label, inst,
                                    //         target_function);
                                    if (prev_callee_context_label == -1)
                                        INIT_CONTEXT(
                                            target_function, {a2, d2},
                                            {new_outflow_forward, new_outflow_backward}); // step 21
                                    else
                                        INIT_CONTEXT(
                                            target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward},
                                            prev_callee_context_label);

                                    pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                    // step 14
                                    // SRI-Commented
                                    //  SLIM_context_transition_graph[mypair] =
                                    //  getContextLabelCounter();
                                    if (debug)
                                        llvm::outs() << "\nAdding edge for new context "
                                                        "in Backwards "
                                                     << getContextLabelCounter();
                                    SLIM_transition_graph.addEdge(mypair, getContextLabelCounter(), target_function);
#ifdef PRVASCO
                                    llvm::outs() << "\nUpdating context transition "
                                                    "graph (c"
                                                 << current_context_label << ", i" << index << ") -->"
                                                 << getContextLabelCounter() << "\n";
#endif

#ifdef PRVASCO
                                    llvm::outs() << "\nAfter New Context "
                                                    "Creation, IN value at "
                                                    "the node in Backward "
                                                    "Analysis (Context: "
                                                 << current_pair.first << ") ";
                                    printDataFlowValuesBackward(getIn(current_pair.first, current_pair.second).second);
                                    llvm::outs() << "\nAfter New Context "
                                                    "Creation, OUT value at "
                                                    "the node in Backward "
                                                    "Analysis (Context: "
                                                 << current_pair.first << ") ";
                                    printDataFlowValuesBackward(getOut(current_pair.first, current_pair.second).second);
                                    llvm::outs() << "\n";
#endif
                                    // context_label_to_context_object_map[context_label_counter]->setParent(mypair);
                                    // llvm::errs() << "\n" <<
                                    // SLIM_context_transition_graph.size();
                                    // MSCHANGE 4 --commented return
                                    // return;
                                    // Reverse_SLIM_context_transition_graph[context_label_counter]
                                    // = mypair;
                                }
                            }
                        }
                    } else { // instrucion is not a call instr
                        if (debug) {
                            printLine(current_context_label, 0);
                            llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                            inst->printInstruction();
                            llvm::outs() << "\nOUT: ";
                            printDataFlowValuesBackward(prev);
                        }

                        /* mod: AR First check if LOUT of instr has changed due
                         * to changes in the OUTFLOW of call instr */
                        bool flagLoutChanged = false;
                        B old_LOUT = getBackwardComponentAtOutOfThisInstruction(inst);
                        if (!EqualDataFlowValuesBackward(prev, old_LOUT))
                            flagLoutChanged = true;
                        // Set flag in context object
                        if (flagLoutChanged)
                            context_label_to_context_object_map[current_pair.first]->setDFValueChange(true);
                        if (flagLoutChanged && modeOfExec == FSFP) {
#ifdef PRVASCO
                            llvm::outs() << "\nInserting node into Forward "
                                            "Worklist on change in IN "
                                            "value within BB in Backward "
                                            "Analysis (Context:"
                                         << current_context_label << ")\n";
                            for (Instruction &I : *bb)
                                llvm::outs() << I << "\n";
#endif
                            forward_worklist.workInsert(make_pair(make_pair(current_context_label, bb), false));
                        } // if flagLoutChanged and FSFP
                        //----------------------------------------------------------------------------------

                        setBackwardComponentAtOutOfThisInstruction(inst, prev); // compute OUT from previous IN-value
                        B new_prev;
                        if (isIgnorableInstruction(inst)) { // || inst->isIgnored()){
                            if (debug)
                                llvm::outs() << "\nIgnoring...\n";
                            new_prev = prev;
                        } else {
                            new_prev = computeInFromOut(inst);
                        }
                        /*******************************************************
            mod:AR bool flagChanged = false; B old_IN =
            getBackwardComponentAtInOfThisInstruction(inst); if
            (!EqualDataFlowValuesBackward(new_prev, old_IN)) flagChanged = true;
                            // Set flag in context object
                        if (flagChanged) {
                               context_label_to_context_object_map[current_pair.first]->setDFValueChange(true);
                        }

                        if (flagChanged && modeOfExec == FSFP) {
            #ifdef PRVASCO
                  llvm::outs() << "\nInserting node into Forward Worklist on
            change in IN " "value within BB in Backward Analysis (Context:"
                               << current_context_label << ")\n";
                              for (Instruction &I : *bb)
                                   llvm::outs() << I << "\n";
            #endif
                            forward_worklist.workInsert(make_pair(make_pair(current_context_label,
            bb), false));

                    setBackwardComponentAtInOfThisInstruction(inst, new_prev);
                        prev = getBackwardComponentAtInOfThisInstruction(inst);
                        if (debug) {
                          llvm::outs() << "\nIN: ";
                          printDataFlowValuesBackward(prev);
                          printLine(current_context_label, 1);
                        }
              } else { //following code commented for now.
               for (auto inst = &*(bb->rbegin()); inst != nullptr;
                     inst = inst->getPrevNonDebugInstruction()) {
                  if (debug) {
                    printLine(current_context_label, 0);
                    llvm::outs() << *inst << "\n";
                    llvm::outs() << "OUT: ";
                    printDataFlowValuesBackward(prev);
                  }
                  setBackwardComponentAtOutOfThisInstruction(
                      &(*inst), prev); // compute OUT from previous IN-value
                  B new_dfv = computeInFromOut(*inst);
                  // change within Basic Block
                  setBackwardComponentAtInOfThisInstruction(&(*inst), new_dfv);
                  if (debug) {
                    llvm::outs() << "\nIN: ";
                    printDataFlowValuesBackward(new_dfv);
                    printLine(current_context_label, 1);
                  }
                  prev = getBackwardComponentAtInOfThisInstruction(*inst);
                 }//for
                }//end else
                       *******************************************************/
                        setBackwardComponentAtInOfThisInstruction(inst, new_prev);
                        prev = getBackwardComponentAtInOfThisInstruction(inst);
                        if (debug) {
                            llvm::outs() << "\nIN: ";
                            printDataFlowValuesBackward(prev);
                            printLine(current_context_label, 1);
                        }
                    } // end else
                } // end for
                // MSCHANGE
                // setBackwardIn(current_pair.first, current_pair.second, prev);
            } else {
                for (auto inst = b.rbegin(); inst != b.rend(); inst++) {
                    Instruction &I = *inst;
                    if (CallInst *ci = dyn_cast<CallInst>(&I)) {

                        Function *target_function = ci->getCalledFunction();
                        if (not target_function || target_function->isDeclaration() ||
                            isAnIgnorableDebugInstruction(&I)) {
                            continue;
                        }

                        // TODO find caller - callee return arg mapping for NOT
                        // SLIM case
                        SLIMOperand *returnInCaller = nullptr;
                        SLIMOperand *returnInCallee = nullptr;
                        std::pair<SLIMOperand *, SLIMOperand *> return_operand_map =
                            make_pair(returnInCaller, returnInCallee);

                        /*
                        At the call instruction, the value at OUT should be
                        splitted into two components: 1) Purely Global and 2)
                        Mixed. The purely global component is given to the end
                        of callee.
                        */
                        // step 12

                        /* int prev_callee_context_label =
                                   getSourceContextAtCallSiteForFunction(
                                       current_context_label, &(*inst),
                           target_function);*/

                        pair<F, B> inflow_pair = CallInflowFunction(
                            current_context_label, target_function, bb, a1, prev,
                            /*prev_callee_context_label,*/ return_operand_map);
                        F a2 = inflow_pair.first;
                        B d2 = inflow_pair.second;

                        F new_outflow_forward;
                        B new_outflow_backward;

                        // step 13

                        setBackwardComponentAtOutOfThisInstruction(&(*inst), prev);

                        int matching_context_label = 0;
                        matching_context_label = check_if_context_already_exists(
                            target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward});
                        if (matching_context_label > 0) // step 15
                        {
                            if (debug) {
                                printLine(current_context_label, 0);
                                llvm::outs() << *inst << "\n";
                                llvm::outs() << "OUT: ";
                                printDataFlowValuesBackward(d1);
                            }
                            // step 14
                            pair<int, Instruction *> mypair = make_pair(current_context_label, &(*inst));
                            context_transition_graph[mypair] = matching_context_label;

                            // step 16 and 17
                            F a3 = getForwardOutflowForThisContext(matching_context_label);
                            B d3 = getBackwardOutflowForThisContext(matching_context_label);

                            pair<F, B> outflow_pair;
                            outflow_pair = CallOutflowFunction(
                                current_context_label, getCurrentInstruction(), target_function, bb, a3, d3, a1, d1,
                                return_operand_map);
                            F a4 = outflow_pair.first;
                            B value_to_be_meet_with_prev_in = outflow_pair.second;

                            // step 18 and 19

                            /*
                            At the call instruction, the value at OUT should be
                            splitted into two components. The purely global
                            component is given to the callee while the mixed
                            component is propagated to IN of this instruction
                            after executing computeINfromOUT() on it.
                            */

                            /*
                            At the IN of this instruction, the value from START
                            of callee procedure is to be merged with the
                            local(mixed) value propagated from OUT. Note that
                            this merging "isn't" exactly (necessarily) the meet
                            between these two values.
                            */

                            /*
                            As explained in ip-vasco,pdf, we need to perform
                            meet with the original value of IN of this
                            instruction to avoid the oscillation problem.
                            */

                            // setBackwardComponentAtInOfThisInstruction(&(*inst),
                            // performMeetBackward(value_to_be_meet_with_prev_in,
                            //                             getBackwardComponentAtInOfThisInstruction((*inst))));

                            setBackwardComponentAtInOfThisInstruction(
                                &(*inst),
                                performMeetBackward(
                                    value_to_be_meet_with_prev_in, getBackwardComponentAtInOfThisInstruction((*inst))));

                            prev = getBackwardComponentAtInOfThisInstruction((*inst));
                            if (debug) {
                                llvm::outs() << "\nIN: ";
                                printDataFlowValuesBackward(prev);
                                printLine(current_context_label, 1);
                            }

                        } else // step 20
                        {
                            // creating a new context
                            int prev_callee_context_label = getCalleeContext(current_context_label, &(*inst));
                            if (prev_callee_context_label == -1)
                                INIT_CONTEXT(
                                    target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward}); // step 21
                            else
                                INIT_CONTEXT(
                                    target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward},
                                    prev_callee_context_label);

                            // INIT_CONTEXT(target_function, {a2, d2},
                            // {new_outflow_forward,
                            // new_outflow_backward});//step 21

                            pair<int, Instruction *> mypair = make_pair(current_context_label, &(*inst));
                            // step 14
                            context_transition_graph[mypair] = context_label_counter;
                        }
                    } else {
                        if (debug) {
                            printLine(current_context_label, 0);
                            llvm::outs() << *inst << "\n";
                            llvm::outs() << "OUT: ";
                            printDataFlowValuesBackward(prev);
                        }
                        setBackwardComponentAtOutOfThisInstruction(
                            &(*inst),
                            prev); // compute OUT from previous IN-value
                        setBackwardComponentAtInOfThisInstruction(&(*inst), computeInFromOut(*inst));
                        prev = getBackwardComponentAtInOfThisInstruction((*inst));
                        if (debug) {
                            llvm::outs() << "\nIN: ";
                            printDataFlowValuesBackward(prev);
                            printLine(current_context_label, 1);
                        }
                    }
                }
                // setBackwardIn(current_pair.first, current_pair.second, prev);
            }
        } else // step 22
        {
            // step 23
            // NormalFlowFunction
            setBackwardIn(current_pair.first, current_pair.second, NormalFlowFunctionBackward(current_pair));
        }
        // Check if IN value of the BB has changed
        bool changed = false;
        // MSCHANGE 5
        // if (!EqualDataFlowValuesBackward(previous_value_at_in_of_this_node,
        //                                  getIn(current_pair.first,
        //                                  current_pair.second).second)) {
        if (!EqualDataFlowValuesBackward(
                getIn(current_pair.first, current_pair.second).second, previous_value_at_in_of_this_node)) {
            changed = true;
        }

#ifdef PRVASCO
        string chg = (changed == true) ? "TRUE" : "FALSE";
        llvm::outs() << "\nIs IN value of the node changed in Backward Analysis: " << chg;
        llvm::outs() << "\n(Previous IN value: ";
        printDataFlowValuesBackward(previous_value_at_in_of_this_node);
        llvm::outs() << " , Current IN value: ";
        printDataFlowValuesBackward(getIn(current_pair.first, current_pair.second).second);
        llvm::outs() << ")\n";
#endif

        // Set flag in context object
        if (changed) {
            context_label_to_context_object_map[current_pair.first]->setDFValueChange(true);
        }

        if (changed && modeOfExec == FSFP) // step 24
        {
// not yet converged
#ifdef PRVASCO
            llvm::outs() << "\nInserting node into Forward Worklist on change in IN "
                            "value in Backward Analysis (Context:"
                         << current_context_label << ")\n";
            for (Instruction &I : *bb)
                llvm::outs() << I << "\n";
#endif
            forward_worklist.workInsert(make_pair(make_pair(current_context_label, bb), false));
            for (auto pred_bb : predecessors(bb)) // step 25
            {
                // step 26
                // TODO check if this condition is needed
                if (direction == "bidirectional") {
#ifdef PRVASCO
                    llvm::outs() << "\nInserting predecessors into Backward Worklist on "
                                    "change in IN value in Backward Analysis (Context:"
                                 << current_context_label << ")\n";
                    for (Instruction &I : *pred_bb)
                        llvm::outs() << I << "\n";
#endif
                    backward_worklist.workInsert(make_pair(make_pair(current_context_label, pred_bb), false));
                }
            }
        }
        // Check if OUT value of the BB has changed
        bool changed1 = false;
        // MSCHANGE 6
        // if (!EqualDataFlowValuesBackward(previous_value_at_out_of_this_node,
        //                                  getOut(current_pair.first,
        //                                  current_pair.second).second)) {
        if (!EqualDataFlowValuesBackward(
                getOut(current_pair.first, current_pair.second).second, previous_value_at_out_of_this_node)) {
            changed1 = true;
        }

#ifdef PRVASCO
        string chg1 = (changed1 == true) ? "TRUE" : "FALSE";
        llvm::outs() << "\nIs OUT value of the node changed in Backward Analysis: " << chg1;
        llvm::outs() << "\n(Previous OUT value: ";
        printDataFlowValuesBackward(previous_value_at_out_of_this_node);
        llvm::outs() << " , Current OUT value: ";
        printDataFlowValuesBackward(getOut(current_pair.first, current_pair.second).second);
        llvm::outs() << ")\n";
#endif

        // Set flag in context object
        if (changed1) {
            context_label_to_context_object_map[current_pair.first]->setDFValueChange(true);
        }

        if (changed1 && modeOfExec == FSFP) { // step 24
#ifdef PRVASCO
            llvm::outs() << "Inserting node into Forward Worklist on change in OUT "
                            "value in Backward Analysis (Context:"
                         << current_context_label << ")\n";
            for (Instruction &I : *bb)
                llvm::outs() << I << "\n";
#endif
            forward_worklist.workInsert(make_pair(make_pair(current_context_label, bb), false));
        }
        //--------------------------------------------

        if (bb == &function.getEntryBlock()) // step 27
        {
            // step 28
            setBackwardOutflowForThisContext(
                current_context_label,
                getGlobalandFunctionArgComponentBackward(
                    getIn(current_pair.first, current_pair.second).second)); // setting backward outflow
            // setBackwardOutflowForThisContext(current_context_label,
            // getIn(current_pair.first, current_pair.second).second);

            if (SLIM) {
                // auto Parent =
                // context_label_to_context_object_map[current_context_label]->getParent();
                // if(Parent.first != -1 && Parent.second != nullptr) {
                //     BasicBlock *bb = Parent.second->getBB();
                //     pair<int, BasicBlock *> context_bb_pair =
                //     make_pair(Parent.first, bb); if (direction ==
                //     "bidirectional") {
                //         llvm::outs() << "\n Inserting into the forward
                //         worklist";
                //         forward_worklist.workInsert(context_bb_pair);
                //     }
                //     llvm::outs() << "\n Inserting into the backward
                //     worklist"; backward_worklist.workInsert(context_bb_pair);
                // }
                // auto result =
                // Reverse_SLIM_context_transition_graph.find(current_context_label);
                // if(result != Reverse_SLIM_context_transition_graph.end()) {
                //     BasicBlock *bb = result->second.second->getBB();
                //     pair<int, BasicBlock *> context_bb_pair =
                //     make_pair(result->second.first, bb); if (direction ==
                //     "bidirectional") {
                //         llvm::outs() << "\n Inserting into the forward
                //         worklist";
                //         forward_worklist.workInsert(context_bb_pair);
                //     }
                //     llvm::outs() << "\n Inserting into the backward
                //     worklist"; backward_worklist.workInsert(context_bb_pair);
                //     SLIM_context_transition_graph.erase(result->second);
                //     Reverse_SLIM_context_transition_graph.erase(result);
                // }
                if (debug)
                    llvm::outs() << "\nChecking CTG to insert callers into worklist\n";
                /*
                SRI--Commented
                for(auto context_inst_pairs : SLIM_context_transition_graph) {
                   //llvm::outs() << "context_inst_pairs.second" <<
                context_inst_pairs.second << "\n"; if (context_inst_pairs.second
                == current_context_label)//matching the called function
                   {
                       //step 30
                       BasicBlock *bb =
                context_inst_pairs.first.second->getBasicBlock(); pair<int,
                BasicBlock
                *> context_bb_pair = make_pair(context_inst_pairs.first.first,
                bb);

                        //Checking if callee has change and computeGenerative is
                to be called again bool changeInFunc =
                context_label_to_context_object_map[current_context_label]->isDFValueChanged();
                        bool isCalleeAnalysed = true;
                        if(changeInFunc){
                            isCalleeAnalysed = false;
                        }

                       if (direction == "bidirectional") {
                            #ifdef PRVASCO
                                llvm::outs() << "\nInserting caller node into
                Forward Worklist at entry of callee in Backward Analysis
                (Context:" << current_context_label << " to " <<
                context_inst_pairs.first.first <<
                ")\n"; for (Instruction &I : *bb) llvm::outs() << I << "\n";
                #endif forward_worklist.workInsert(make_pair(context_bb_pair,
                isCalleeAnalysed));
                       }
                       #ifdef PRVASCO
                            llvm::outs() << "\nInserting caller node into
                Backward Worklist at entry of callee in Backward Analysis
                (Context:" << current_context_label << " to " <<
                context_inst_pairs.first.first <<
                ")\n"; for (Instruction &I : *bb) llvm::outs() << I << "\n";
                #endif backward_worklist.workInsert(make_pair(context_bb_pair,
                isCalleeAnalysed));
                    }
                }*/
                vector<pair<int, BasicBlock *>> caller_contexts =
                    SLIM_transition_graph.getCallers(current_context_label);
                for (auto context_bb_pair : caller_contexts) {
                    // step 30
                    BasicBlock *bb = context_bb_pair.second;

                    // Checking if callee has change and computeGenerative is to
                    // be called again
                    // mod:AR No longer needed
                    bool changeInFunc = context_label_to_context_object_map[current_context_label]->isDFValueChanged();
                    bool isCalleeAnalysed = true;
                    if (changeInFunc) {
                        isCalleeAnalysed = false;
                    }
                    /// bool isCalleeAnalysed = true;//mod:AR retained to keep
                    /// code changes to minimum

                    if (direction == "bidirectional") {
#ifdef PRVASCO
                        llvm::outs() << "\nInserting caller node into Forward Worklist "
                                        "at "
                                        "entry of callee in Backward Analysis (Context:"
                                     << current_context_label << " to " << context_bb_pair.first << ")\n";
                        for (Instruction &I : *bb)
                            llvm::outs() << I << "\n";
#endif
                        //@NOTE: Insert the caller into BWL for backwards
                        // analysis and not into FWL.
                        //   forward_worklist.workInsert(make_pair(context_bb_pair,
                        //   isCalleeAnalysed));
                    }
#ifdef PRVASCO
                    llvm::outs() << "\nInserting caller node into Backward Worklist at "
                                    "entry of callee in Backward Analysis (Context:"
                                 << current_context_label << " to " << context_bb_pair.first << ")\n";
                    for (Instruction &I : *bb)
                        llvm::outs() << I << "\n";
#endif
                    backward_worklist.workInsert(make_pair(context_bb_pair, isCalleeAnalysed));
                    // break;  mod:AR Multiple callers are not processed.
                }
            } else {
                for (auto context_inst_pairs : context_transition_graph) // step 29
                {
                    if (context_inst_pairs.second == current_context_label) // matching the called function
                    {
                        // step 30

                        BasicBlock *bb = context_inst_pairs.first.second->getParent();
                        pair<int, BasicBlock *> context_bb_pair = make_pair(context_inst_pairs.first.first, bb);

                        // Checking if callee has change and computeGenerative
                        // is to be called again
                        bool changeInFunc =
                            context_label_to_context_object_map[current_context_label]->isDFValueChanged();
                        bool isCalleeAnalysed = true;
                        if (changeInFunc) {
                            isCalleeAnalysed = false;
                        }

                        if (direction == "bidirectional") {
                            forward_worklist.workInsert(make_pair(context_bb_pair, isCalleeAnalysed));
                        }
                        backward_worklist.workInsert(make_pair(context_bb_pair, isCalleeAnalysed));
                        // break;
                    }
                }
            }
        }
        process_mem_usage(this->vm, this->rss);
        this->total_memory = max(this->total_memory, this->vm);
    }
#ifdef PRINTSTATS
    mapbackwardsRoundBB[backwardsRoundCount] = backwardsBBPerRound; // #BBCOUNTPERROUND
#endif
}

template <class F, class B>
B Analysis<F, B>::NormalFlowFunctionBackward(pair<int, BasicBlock *> current_pair_of_context_label_and_bb) {
    BasicBlock &b = *(current_pair_of_context_label_and_bb.second);
    B prev = getOut(current_pair_of_context_label_and_bb.first, current_pair_of_context_label_and_bb.second).second;
    F prev_f = getOut(current_pair_of_context_label_and_bb.first, current_pair_of_context_label_and_bb.second).first;
    Context<F, B> *context_object = context_label_to_context_object_map[current_pair_of_context_label_and_bb.first];
    bool changed = false;
    // traverse a basic block in backward direction
    if (SLIM) {
        for (auto &index : optIR->getReverseInstList(optIR->func_bb_to_inst_id[{
                 context_object->getFunction(), current_pair_of_context_label_and_bb.second}])) {
            auto inst = optIR->inst_id_to_object[index];
            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 0);
                llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                // assert (inst != nullptr && "Inst isn a NULL ptr");
                // inst->printInstruction();
                llvm::outs() << "\nOUT: ";
                printDataFlowValuesBackward(prev);
            }
            setBackwardComponentAtOutOfThisInstruction(inst, prev); // compute OUT from previous IN-value
            B new_dfv;                          //= computeInFromOut(inst); only if non a builtin call
            if (isIgnorableInstruction(inst)) { // || inst->isIgnored()){
                if (debug)
                    llvm::outs() << "\nIgnoring...\n";
                new_dfv = prev;
            } else {
                new_dfv = computeInFromOut(inst);
            }

            ////if change in data flow value at IN of any instruction, add this
            /// basic
            /// block to forward worklist
            B old_IN = getBackwardComponentAtInOfThisInstruction(inst);
            if (!EqualDataFlowValuesBackward(new_dfv, old_IN))
                changed = true;

            setBackwardComponentAtInOfThisInstruction(inst, new_dfv);
            /* if (debug) {
               llvm::outs() << "\nIN: ";
              // printDataFlowValuesBackward(new_dfv);
               printLine(current_pair_of_context_label_and_bb.first, 1);
             }*/
            prev = getBackwardComponentAtInOfThisInstruction(inst);
            /* if (debug) {
               llvm::outs() << "\nPrev set as ";
               printDataFlowValuesBackward(prev);
             }*/
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }

        // Set flag in context object
        if (changed) {
            context_label_to_context_object_map[current_pair_of_context_label_and_bb.first]->setDFValueChange(true);
        }

        if (changed && modeOfExec == FSFP) {
#ifdef PRVASCO
            llvm::outs() << "\nInserting node into Forward Worklist on change in IN "
                            "value within BB in Backward Analysis (Context:"
                         << current_pair_of_context_label_and_bb.first << ")\n";
            for (Instruction &I : b)
                llvm::outs() << I << "\n";
#endif
            forward_worklist.workInsert(make_pair(current_pair_of_context_label_and_bb, false));
        }
    } else {
        for (auto inst = &*(b.rbegin()); inst != nullptr; inst = inst->getPrevNonDebugInstruction()) {
            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 0);
                llvm::outs() << *inst << "\n";
                llvm::outs() << "OUT: ";
                printDataFlowValuesBackward(prev);
            }
            setBackwardComponentAtOutOfThisInstruction(&(*inst), prev); // compute OUT from previous IN-value
            B new_dfv = computeInFromOut(*inst);
            // change within Basic Block
            setBackwardComponentAtInOfThisInstruction(&(*inst), new_dfv);
            if (debug) {
                llvm::outs() << "\nIN: ";
                printDataFlowValuesBackward(new_dfv);
                printLine(current_pair_of_context_label_and_bb.first, 1);
            }
            prev = getBackwardComponentAtInOfThisInstruction(*inst);
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
    }
    return prev;
}

// Global accessor functions

template <class F, class B>
pair<F, B> getAnalysisInAtInstruction(BaseInstruction *I, int context_label, Analysis<F, B> *analysisObj) {
    F forwardIn = analysisObj->getForwardComponentAtInOfThisInstruction(I, context_label);
    B backwardIn = analysisObj->getBackwardComponentAtInOfThisInstruction(I, context_label);
    return make_pair(forwardIn,
                     backwardIn); // analysisObj->SLIM_IN[context_label][I];
}

template <class F, class B>
pair<F, B> getAnalysisOutAtInstruction(BaseInstruction *I, int context_label, Analysis<F, B> *analysisObj) {
    F forwardOut = analysisObj->getForwardComponentAtOutOfThisInstruction(I, context_label);
    B backwardOut = analysisObj->getBackwardComponentAtOutOfThisInstruction(I, context_label);
    return make_pair(forwardOut,
                     backwardOut); // analysisObj->SLIM_OUT[context_label][I];
}

template <class F, class B>
pair<F, B> Analysis<F, B>::getInflowforContext(int label) {
    pair<F, B> inflow;
    if (context_label_to_context_object_map.find(label) != context_label_to_context_object_map.end()) {
        Context<F, B> *context_object = context_label_to_context_object_map[label];
        inflow = context_object->getInflowValue();
    }
    return inflow;
}

template <class F, class B>
pair<F, B> Analysis<F, B>::CallInflowFunction(
    int context_label, Function *Func, BasicBlock *bb, const F &a1, const B &d1, int source_context_label,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    llvm::outs() << "\nThis function CallInflowFunction() with Generative has "
                    "not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> Analysis<F, B>::CallOnflowFunction(
    int context_label, BaseInstruction *I, int source_context_label,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    llvm::outs() << "\nThis function CallOnflowFunction() has not been "
                    "implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> Analysis<F, B>::CallOnflowFunctionForIndirect(
    int context_label, BaseInstruction *I, int source_context_label,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map, Function *target_function) {
    llvm::outs() << "\nThis function CallOnflowFunctionForIndirect() has not "
                    "been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B Analysis<F, B>::getBackwardIn(int label, llvm::BasicBlock *BB) {
    if (SLIM) {
        return SLIM_IN[label][optIR->inst_id_to_object[optIR->getFirstIns(BB->getParent(), BB)]].second;
    }
    return IN[label][&(*(BB->begin()))].second;
}

template <class F, class B>
B Analysis<F, B>::getBackwardOut(int label, llvm::BasicBlock *BB) {
    if (SLIM) {
        return SLIM_OUT[label][optIR->inst_id_to_object[optIR->getLastIns(BB->getParent(), BB)]].second;
    }
    return OUT[label][&(BB->back())].second;
}

template <class F, class B>
F Analysis<F, B>::getForwardIn(int label, llvm::BasicBlock *BB) {
    if (SLIM) {
        return SLIM_IN[label][optIR->inst_id_to_object[optIR->getFirstIns(BB->getParent(), BB)]].first;
    }
    return IN[label][&(*(BB->begin()))].first;
}

template <class F, class B>
F Analysis<F, B>::getForwardOut(int label, llvm::BasicBlock *BB) {
    if (SLIM) {
        return SLIM_OUT[label][optIR->inst_id_to_object[optIR->getLastIns(BB->getParent(), BB)]].first;
    }
    return OUT[label][&(BB->back())].first;
}

template <class F, class B>
B Analysis<F, B>::getBackwardComponentAtInOfThisInstruction(llvm::Instruction *I, int label) {
    return IN[label][I].second;
}

template <class F, class B>
B Analysis<F, B>::getBackwardComponentAtOutOfThisInstruction(llvm::Instruction *I, int label) {
    return OUT[label][I].second;
}

template <class F, class B>
F Analysis<F, B>::getForwardComponentAtInOfThisInstruction(llvm::Instruction *I, int label) {
    return IN[label][I].first;
}

template <class F, class B>
F Analysis<F, B>::getForwardComponentAtOutOfThisInstruction(llvm::Instruction *I, int label) {
    return OUT[label][I].first;
}

template <class F, class B>
int Analysis<F, B>::getCalleeContext(int callerContext, llvm::Instruction *I) {
    pair<int, llvm::Instruction *> callerInfo = make_pair(callerContext, I);
    if (context_transition_graph.find(callerInfo) != context_transition_graph.end())
        return context_transition_graph[callerInfo];
    else
        return -1;
}

template <class F, class B>
void Analysis<F, B>::INIT_CONTEXT(
    llvm::Function *function, const std::pair<F, B> &Inflow, const std::pair<F, B> &Outflow, int source_context_label) {
    /*llvm::outs()
    << "\n Inside INIT_CONTEXT with source context label.............."
    << source_context_label << "\n";*/

    context_label_counter++;
    Context<F, B> *context_object =
        new Context<F, B>(context_label_counter, function, Inflow, Outflow, source_context_label);
    int current_context_label = context_object->getLabel();
    setProcessingContextLabel(current_context_label);
    Context<F, B> *source_context_object = context_label_to_context_object_map[source_context_label];
    if (std::is_same<B, NoAnalysisType>::value) {
        if (debug) {
            llvm::outs() << "INITIALIZING CONTEXT:-" << "\n";
            llvm::outs() << "LABEL: " << context_object->getLabel() << "\n";
            llvm::outs() << "FUNCTION: " << function->getName() << "\n";
            llvm::outs() << "Inflow Value: ";
            printDataFlowValuesForward(Inflow.first);
            llvm::outs() << "Outflow value: ";
            printDataFlowValuesForward(getInitialisationValueForward());
        }
        // forward analysis
        context_label_to_context_object_map[current_context_label] = context_object;

        setForwardOutflowForThisContext(
            current_context_label,
            getForwardOutflowForThisContext(source_context_label)); // setting outflow forward
        ProcedureContext.insert(current_context_label);

        for (BasicBlock *BB : post_order(&context_object->getFunction()->getEntryBlock())) {
            BasicBlock &b = *BB;
            forward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            if (direction == "bidirectional") {
                backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            }
            setForwardIn(current_context_label, &b, getForwardIn(source_context_label, &b));
            setForwardOut(current_context_label, &b, getForwardOut(source_context_label, &b));

            // initialise IN-OUT maps for every instruction
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{function, BB}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setForwardComponentAtInOfThisInstruction(
                        inst, getForwardComponentAtInOfThisInstruction(inst, source_context_label));
                    setForwardComponentAtOutOfThisInstruction(
                        inst, getForwardComponentAtOutOfThisInstruction(inst, source_context_label));
                }
            } else {
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    setForwardComponentAtInOfThisInstruction(
                        &(*inst), getForwardComponentAtInOfThisInstruction(&(*inst), source_context_label));
                    setForwardComponentAtOutOfThisInstruction(
                        &(*inst), getForwardComponentAtOutOfThisInstruction(&(*inst), source_context_label));
                }
            }
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
        if (current_context_label == 0) // main function with first invocation
        {
            setForwardInflowForThisContext(
                current_context_label,
                getBoundaryInformationForward()); // setting inflow forward
        } else {
            setForwardInflowForThisContext(current_context_label, context_object->getInflowValue().first);
        }
        setForwardIn(
            current_context_label, &context_object->getFunction()->getEntryBlock(),
            getForwardInflowForThisContext(current_context_label));
    } else if (std::is_same<F, NoAnalysisType>::value) {
        if (debug) {
            llvm::outs() << "INITIALIZING CONTEXT:-" << "\n";
            llvm::outs() << "LABEL: " << context_object->getLabel() << "\n";
            llvm::outs() << "FUNCTION: " << function->getName() << "\n";
            llvm::outs() << "Inflow Value: ";
            printDataFlowValuesBackward(Inflow.second);
        }
        // backward analysis
        context_label_to_context_object_map[current_context_label] = context_object;
        setBackwardOutflowForThisContext(
            current_context_label,
            getBackwardOutflowForThisContext(source_context_label)); // setting outflow backward
        ProcedureContext.insert(current_context_label);

        //  for (BasicBlock *BB :
        //         inverse_post_order(optIR->getLastBasicBlock(context_object->getFunction())))
        //         {
        for (BasicBlock *BB : inverse_post_order(&context_object->getFunction()->back())) {

            BasicBlock &b = *BB;
            backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            if (direction == "bidirectional") {
                backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            }
            setBackwardIn(current_context_label, &b, getBackwardIn(source_context_label, &b));
            setBackwardOut(current_context_label, &b, getBackwardOut(source_context_label, &b));

            // initialise IN-OUT maps for every instruction
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{function, BB}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setBackwardComponentAtInOfThisInstruction(
                        inst, getBackwardComponentAtInOfThisInstruction(inst, source_context_label));
                    setBackwardComponentAtOutOfThisInstruction(
                        inst, getBackwardComponentAtOutOfThisInstruction(inst, source_context_label));
                }
            } else {
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    setBackwardComponentAtInOfThisInstruction(
                        &(*inst), getBackwardComponentAtInOfThisInstruction(&(*inst), source_context_label));
                    setBackwardComponentAtOutOfThisInstruction(
                        &(*inst), getBackwardComponentAtOutOfThisInstruction(&(*inst), source_context_label));
                }
            }
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
        //@Traversing basic blocks with no successors
        std::set<llvm::BasicBlock *> *setTerminateBB = optIR->getTerminatingBBList(context_object->getFunction());
        for (BasicBlock *BB : *setTerminateBB) {
            BasicBlock &b = *BB;
            backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            if (direction == "bidirectional") {
                backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            }
            setBackwardIn(current_context_label, &b, getBackwardIn(source_context_label, &b));
            setBackwardOut(current_context_label, &b, getBackwardOut(source_context_label, &b));

            // initialise IN-OUT maps for every instruction
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{function, BB}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setBackwardComponentAtInOfThisInstruction(
                        inst, getBackwardComponentAtInOfThisInstruction(inst, source_context_label));
                    setBackwardComponentAtOutOfThisInstruction(
                        inst, getBackwardComponentAtOutOfThisInstruction(inst, source_context_label));
                }
            } else {
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    setBackwardComponentAtInOfThisInstruction(
                        &(*inst), getBackwardComponentAtInOfThisInstruction(&(*inst), source_context_label));
                    setBackwardComponentAtOutOfThisInstruction(
                        &(*inst), getBackwardComponentAtOutOfThisInstruction(&(*inst), source_context_label));
                }
            }
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        } // for

        if (current_context_label == 0) // main function with first invocation
        {
            setBackwardInflowForThisContext(
                current_context_label,
                getBoundaryInformationBackward()); // setting inflow backward
        } else {
            setBackwardInflowForThisContext(
                current_context_label,
                context_object->getInflowValue().second); // setting inflow backward
        }
        // setBackwardOut(current_context_label,
        // &context_object->getFunction()->back(),
        // getBackwardInflowForThisContext(current_context_label));
        setBackwardOut(
            current_context_label, &context_object->getFunction()->back(),
            getBackwardInflowForThisContext(current_context_label));
    } else {
        context_label_to_context_object_map[current_context_label] = context_object;

        if (debug) {
            llvm::outs() << "\nINITIALIZING CONTEXT:-" << "\n";
            llvm::outs() << "LABEL: " << context_object->getLabel() << "\n";
            llvm::outs() << "FUNCTION: " << function->getName() << "\n";
            llvm::outs() << "Inflow Value: ";
            llvm::outs() << "Forward:- ";
            printDataFlowValuesForward(Inflow.first);
            llvm::outs() << "Backward:- ";
            printDataFlowValuesBackward(Inflow.second);
#ifdef PRINTSTATS
            llvm::outs() << " \nTotal number of BB analysed during Forward Analysis = " << F_BBanalysedCount;
            llvm::outs() << " \nTotal number of BB analysed during Backward Analysis = " << B_BBanalysedCount;
            llvm::outs() << " \n Total forward rounds: " << forwardsRoundCount;
            llvm::outs() << " \n Total backwards rounds " << backwardsRoundCount;
            // #BBCOUNTPERROUND
            llvm::outs() << "\n Count of basic blocks per round: ";
            llvm::outs() << "\n FORWARD(Round, Count): \n";
            for (auto fb : mapforwardsRoundBB) {
                llvm::outs() << "\t (" << fb.first << ", " << fb.second << ")";
            }
            llvm::outs() << "\n BACKWARD(Round, Count): \n";
            for (auto bb : mapbackwardsRoundBB) {
                llvm::outs() << "\t (" << bb.first << ", " << bb.second << ")";
            }
            printStatistics();
            stat.dump();
#endif
        } //
        setForwardOutflowForThisContext(
            current_context_label,
            getForwardOutflowForThisContext(source_context_label)); // setting outflow forward
        setBackwardOutflowForThisContext(
            current_context_label,
            getBackwardOutflowForThisContext(source_context_label)); // setting outflow backward
        ProcedureContext.insert(current_context_label);

        //    for (BasicBlock *BB :
        //       inverse_post_order(optIR->getLastBasicBlock(context_object->getFunction())))
        //       {
        for (BasicBlock *BB : inverse_post_order(&context_object->getFunction()->back())) {

            // populate backward worklist
            BasicBlock &b = *BB;
#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Inserting into Backward Worklist in INIT_CONTEXT:" << current_context_label << "\n";
            for (Instruction &I : b)
                llvm::outs() << I << "\n";
#endif

            backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            setBackwardIn(current_context_label, &b, getBackwardIn(source_context_label, &b));
            setBackwardOut(current_context_label, &b, getBackwardOut(source_context_label, &b));

            // initialise IN-OUT maps for every instruction
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{function, BB}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setBackwardComponentAtInOfThisInstruction(
                        inst, getBackwardComponentAtInOfThisInstruction(inst, source_context_label));
                    setBackwardComponentAtOutOfThisInstruction(
                        inst, getBackwardComponentAtOutOfThisInstruction(inst, source_context_label));
                }
            } else {
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    setBackwardComponentAtInOfThisInstruction(
                        &(*inst), getBackwardComponentAtInOfThisInstruction(&(*inst), source_context_label));
                    setBackwardComponentAtOutOfThisInstruction(
                        &(*inst), getBackwardComponentAtOutOfThisInstruction(&(*inst), source_context_label));
                }
            }

            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
        //@Traversing basic blocks with no successors
        std::set<llvm::BasicBlock *> *setTerminateBB = optIR->getTerminatingBBList(context_object->getFunction());
        for (BasicBlock *BB : *setTerminateBB) {
            // populate backward worklist
            BasicBlock &b = *BB;
#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Inserting into Backward Worklist in INIT_CONTEXT:" << current_context_label << "\n";
            for (Instruction &I : b)
                llvm::outs() << I << "\n";
#endif

            backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            setBackwardIn(current_context_label, &b, getBackwardIn(source_context_label, &b));
            setBackwardOut(current_context_label, &b, getBackwardOut(source_context_label, &b));

            // initialise IN-OUT maps for every instruction
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{function, BB}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setBackwardComponentAtInOfThisInstruction(
                        inst, getBackwardComponentAtInOfThisInstruction(inst, source_context_label));
                    setBackwardComponentAtOutOfThisInstruction(
                        inst, getBackwardComponentAtOutOfThisInstruction(inst, source_context_label));
                }
            } else {
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    setBackwardComponentAtInOfThisInstruction(
                        &(*inst), getBackwardComponentAtInOfThisInstruction(&(*inst), source_context_label));
                    setBackwardComponentAtOutOfThisInstruction(
                        &(*inst), getBackwardComponentAtOutOfThisInstruction(&(*inst), source_context_label));
                }
            }

            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        } // for

        for (BasicBlock *BB : post_order(&context_object->getFunction()->getEntryBlock())) {
            // populate forward worklist
            BasicBlock &b = *BB;

#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Inserting into Forward Worklist in INIT_CONTEXT:" << current_context_label << "\n";
            for (Instruction &I : b)
                llvm::outs() << I << "\n";
#endif

            forward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), false));
            setForwardIn(current_context_label, &b, getForwardIn(source_context_label, &b));
            setForwardOut(current_context_label, &b, getForwardOut(source_context_label, &b));

            // initialise IN-OUT maps for every instruction
            if (SLIM) {
                for (auto &index : optIR->func_bb_to_inst_id[{function, BB}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setForwardComponentAtInOfThisInstruction(
                        inst, getForwardComponentAtInOfThisInstruction(inst, source_context_label));
                    setForwardComponentAtOutOfThisInstruction(
                        inst, getForwardComponentAtOutOfThisInstruction(inst, source_context_label));
                }
            } else {
                for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
                    setForwardComponentAtInOfThisInstruction(
                        &(*inst), getForwardComponentAtInOfThisInstruction(&(*inst), source_context_label));
                    setForwardComponentAtOutOfThisInstruction(
                        &(*inst), getForwardComponentAtOutOfThisInstruction(&(*inst), source_context_label));
                }
            }
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
        if (current_context_label == 0) { // main function with first invocation
            setForwardInflowForThisContext(
                current_context_label,
                getBoundaryInformationForward()); // setting inflow forward
            setBackwardInflowForThisContext(
                current_context_label,
                getBoundaryInformationBackward()); // setting inflow backward
        } else {
            setForwardInflowForThisContext(
                current_context_label,
                context_object->getInflowValue().first); // setting inflow forward
            setBackwardInflowForThisContext(
                current_context_label,
                context_object->getInflowValue().second); // setting inflow backward
        }
        setForwardIn(
            current_context_label, &context_object->getFunction()->getEntryBlock(),
            getForwardInflowForThisContext(current_context_label));
        setBackwardOut(
            current_context_label, &context_object->getFunction()->back(),
            getBackwardInflowForThisContext(current_context_label));
    }
}

template <class F, class B>
string Analysis<F, B>::getFileName(llvm::Instruction *I) {
    string sourcefile = I->getFunction()->getParent()->getSourceFileName();
    return sourcefile;
}

template <class F, class B>
string Analysis<F, B>::getFileName(BaseInstruction *I) {
    string sourcefile = I->getSourceFileName();
    return sourcefile;
}

template <class F, class B>
unsigned Analysis<F, B>::getSourceLineNumberForInstruction(llvm::Instruction *I) {
    unsigned line_num = I->getDebugLoc().getLine();
    return line_num;
}

template <class F, class B>
unsigned Analysis<F, B>::getSourceLineNumberForInstruction(BaseInstruction *I) {
    unsigned line_num = I->getSourceLineNumber();
    return line_num;
}

template <class F, class B>
set<Function *> Analysis<F, B>::getIndirectFunctionsAtCallSite(BaseInstruction *I) {
    string file_name = getFileName(I);
    long long line_num = (long long)getSourceLineNumberForInstruction(I);
    // llvm::outs()<<"\nFile name="<<file_name<< " Line number = "<<line_num;
    set<Function *> fun_list;
    vector<string> fun_names = indirect_functions_obj.getPointsToInfo({file_name, line_num});
    for (string name : fun_names) {
        //   llvm::outs()<<name<<" ";
        fun_list.insert(getFunctionWithName(name));
    }
    return fun_list;
}

template <class F, class B>
Function *Analysis<F, B>::getFunctionWithName(string fun_name) {
    Function *ret_func;
    if (SLIM) {
        for (auto &entry : optIR->func_bb_to_inst_id) {
            Function *func = entry.first.first;
            std::string funcName = func->getName().str();
            if (funcName == fun_name) {
                ret_func = func;
                break;
            }
        }
        if (!ret_func) {
            llvm::outs() << "No function exists with name " << fun_name;
        }
    } else {
        // TODO
    }
    return ret_func;
}

template <class F, class B>
bool Analysis<F, B>::isUseInstruction(BaseInstruction *I) {
    std::pair<SLIMOperand *, int> LHS = I->getLHS();
    std::vector<std::pair<SLIMOperand *, int>> RHS = I->getRHS();
    int indirLhs = LHS.second;

    if (I->getInstructionType() == RETURN) {
        return true;
    } else if (I->getInstructionType() == COMPARE) {
        // for (std::vector<std::pair<SLIMOperand *, int>>::iterator r =
        // RHS.begin();r != RHS.end(); r++) {
        //     SLIMOperand *rhsValue = r->first;
        //     if (rhsValue->isPointerInLLVM()){
        //         return true;
        //     }
        // }
        return true;
    } else if (
        (I->getInstructionType() == LOAD or I->getInstructionType() == STORE) and !LHS.first->isPointerInLLVM()) {
        for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end(); r++) {
            SLIMOperand *rhsVal = r->first;
            if (rhsVal->isPointerInLLVM() and !rhsVal->isAlloca()) {
                return true;
            }
        }
    } else if (LHS.second == 2) {
        return true;
    }
    return false;
}

template <class F, class B>
int Analysis<F, B>::getSourceContextAtCallSiteForFunction(
    int current_context_label, BaseInstruction *I, Function *target_function) {
    if (debug)
        llvm::outs() << "\n Inside function getSourceContextAtCallSiteForFunction\n";
    int callee_source_context_label = SLIM_transition_graph.getCalleeContext(current_context_label, I, target_function);
    if (debug)
        llvm::outs() << "callee_source_context_label=" << callee_source_context_label << " for function "
                     << target_function->getName() << "\n";
    if (callee_source_context_label == -1) {
        Context<F, B> *current_context_obj = context_label_to_context_object_map[current_context_label];
        if (debug)
            llvm::outs() << "Fetching the source context of caller " << current_context_obj->getSourceContextLabel();
        callee_source_context_label =
            SLIM_transition_graph.getCalleeContext(current_context_obj->getSourceContextLabel(), I, target_function);
    }
    return callee_source_context_label;
}
// End of Global accessor functions

#endif
