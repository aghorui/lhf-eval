/*******************************************************************************************************
                  "VASCO-like Framework for Generative and Kill Computation (takes dependent analysis context
additionally)" Author:      Manjusree Created on:  Feb 2023

**********************************************************************************************************/

#ifndef ANALYSISGENKILL_H
#define ANALYSISGENKILL_H

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

// #include "IR.h"

using namespace llvm;
using namespace std;
using namespace std::chrono;
/*enum NoAnalysisType {
    NoAnalyisInThisDirection
};*/

/*inline void process_mem_usage(float &vm_usage, float &resident_set) {
    using std::ios_base;
    using std::ifstream;
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

    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
                >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
                >> utime >> stime >> cutime >> cstime >> priority >> nice
                >> O >> itrealvalue >> starttime >> vsize >> rss;

    stat_stream.close();
    long page_size_kb = sysconf(_SC_PAGE_SIZE) / 1024;
    vm_usage = static_cast<float>(vsize) / 1024.0f;
    resident_set = static_cast<float>(rss * page_size_kb);
}

inline void printMemory(float memory, std::ofstream& out) {
    out << fixed;
    out << setprecision(6);
    out << memory / 1024.0;
    out << " MB\n";
}*/

/*class HashFunction {
public:
    auto operator()(const pair<int, BasicBlock *> &P) const {
        // return hash<int>()(P.first) ^ std::hash<BasicBlock *>()(P.second);
        // Same as above line, but viewed the actual source code of C++ and then wrote it
        // Referred: functional_hash.h (line 114)
        // Referred: functional_hash.h (line 110)
        return static_cast<size_t>(P.first) ^ reinterpret_cast<size_t>(P.second);
    }

    auto operator()(const pair<int, Instruction *> &P) const {
        // return hash<int>()(P.first) ^ std::hash<Instruction *>()(P.second);
        // Same as above line, but viewed the actual source code of C++ and then wrote it
        // Referred: functional_hash.h (line 114)
        // Referred: functional_hash.h (line 110)
        return static_cast<size_t>(P.first) ^ reinterpret_cast<size_t>(P.second);
    }

    auto operator()(const pair<int, BaseInstruction *> &P) const {
        // return hash<int>()(P.first) ^ std::hash<Instruction *>()(P.second);
        // Same as above line, but viewed the actual source code of C++ and then wrote it
        // Referred: functional_hash.h (line 114)
        // Referred: functional_hash.h (line 110)
        return static_cast<size_t>(P.first) ^ reinterpret_cast<size_t>(P.second);
    }
};*/

/*typedef enum
{
    FSFP,
    FSSP,
    FIFP,
    FISP
}ExecMode;*/

template <class F, class B>
class AnalysisGenKill {
private:
    Module *current_module;
    int context_label_counter;
    int current_analysis_direction{}; // 0:initial pass, 1:forward, 2:backward
    int processing_context_label{};
    std::unordered_map<int, unordered_map<llvm::Instruction *, pair<F, B>>> IN, OUT;
    std::unordered_map<int, unordered_map<BaseInstruction *, pair<F, B>>> SLIM_IN, SLIM_OUT;
    std::unordered_map<int, unordered_map<Function *, F>> DFVALUE;

    std::string direction;
    unordered_map<int, Context<F, B> *> context_label_to_context_object_map;

    // mapping from context object to context label
    // mapping from function to  pair<inflow,outflow>
    // inflow and outflow are themselves pairs of forward and backward component values.
    // The forward and backward components are themselves pairs of G,L dataflow values.
    bool debug{};
    bool SLIM{};
    float total_memory{}, vm{}, rss{};
    std::chrono::milliseconds AnalysisTime, SLIMTime, SplittingBBTime;

    void printLine(int, int);

    BaseInstruction *current_instruction;

    // intraprocFlag : "true" : intraprocedural analysis; "false" : interprocedural analysis;
    bool intraprocFlag = false;
    ExecMode modeOfExec = FSFP;
    bool byPass = false;
    std::unique_ptr<llvm::Module> moduleUniquePtr;

protected:
    // List of contexts
    unordered_set<int> ProcedureContext;
    Worklist<pair<pair<int, BasicBlock *>, int>, HashFunction> backward_worklist, forward_worklist;

    // mapping from (context label,call site) to target context label
    unordered_map<pair<int, llvm::Instruction *>, int, HashFunction> context_transition_graph;
    unordered_map<pair<int, BaseInstruction *>, int, HashFunction> SLIM_context_transition_graph;
    // std::unordered_map<int, pair<int, fetchLR *>> Reverse_SLIM_context_transition_graph;

    // std::map<std::pair<llvm::Function *, llvm::BasicBlock *>, std::list<long long>> funcBBInsMap;
    // std::map<long long, BaseInstruction *> globalInstrIndexList;

    unordered_map<pair<int, BaseInstruction *>, pair<pair<F, B>, pair<F, B>>, HashFunction> GenerativeKill;

public:
    AnalysisGenKill(){};

    AnalysisGenKill(std::unique_ptr<llvm::Module> &, bool, bool, bool, ExecMode, bool);

    AnalysisGenKill(std::unique_ptr<llvm::Module> &, const string &, bool, bool, bool, ExecMode, bool);

    // AnalysisGenKill(bool,bool);

    // AnalysisGenKill(bool, const string &,bool);

    // AnalysisGenKill(std::unique_ptr<llvm::Module>&, bool, bool, bool);

    // AnalysisGenKill(std::unique_ptr<llvm::Module>&, const string &, bool, bool, bool);

    ~AnalysisGenKill();

    slim::IR *optIR;

    // void doAnalysis(Module &M);
    void doAnalysis(std::unique_ptr<llvm::Module> &M, slim::IR *optIR, int depAnalysisContext);

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

    void INIT_CONTEXT(llvm::Function *, const std::pair<F, B> &, const std::pair<F, B> &, int);

    int check_if_context_already_exists(llvm::Function *, const pair<F, B> &, const pair<F, B> &);

    void doAnalysisForward();

    void doAnalysisBackward();

    F NormalFlowFunctionForward(pair<int, BasicBlock *>, int);

    B NormalFlowFunctionBackward(pair<int, BasicBlock *>, int);

    void startSplitting(std::unique_ptr<llvm::Module> &);

    void performSplittingBB(Function &f);

    void setCurrentModule(Module *);

    Module *getCurrentModule();

    void setCurrentInstruction(BaseInstruction *);

    BaseInstruction *getCurrentInstruction() const;

    F getForwardComponentAtInOfThisInstruction(llvm::Instruction &I);
    F getForwardComponentAtInOfThisInstruction(BaseInstruction *I);

    F getForwardComponentAtInOfThisInstruction(BaseInstruction *I, int label); // #########

    F getForwardComponentAtOutOfThisInstruction(llvm::Instruction &I);
    F getForwardComponentAtOutOfThisInstruction(BaseInstruction *I);

    F getForwardComponentAtOutOfThisInstruction(BaseInstruction *I, int label); // #########

    B getBackwardComponentAtInOfThisInstruction(llvm::Instruction &I);
    B getBackwardComponentAtInOfThisInstruction(BaseInstruction *I);

    B getBackwardComponentAtInOfThisInstruction(BaseInstruction *I, int label); // #########

    B getBackwardComponentAtOutOfThisInstruction(llvm::Instruction &I);
    B getBackwardComponentAtOutOfThisInstruction(BaseInstruction *I);

    B getBackwardComponentAtOutOfThisInstruction(BaseInstruction *I, int label); // #########

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

    virtual pair<F, B> CallInflowFunction(int, Function *, BasicBlock *, const F &, const B &);

    virtual pair<F, B> CallOutflowFunction(int, Function *, BasicBlock *, const F &, const B &, const F &, const B &);

    virtual F computeOutFromIn(llvm::Instruction &I, int depContext);
    virtual F computeOutFromIn(BaseInstruction *I, int depContext);

    virtual F getBoundaryInformationForward(); //{}
    virtual F getInitialisationValueForward(); //{}
    // virtual F performMeetForward(const F& d1, const F& d2);//{}
    virtual F performMeetForward(const F &d1, const F &d2, const string pos);
    virtual bool EqualDataFlowValuesForward(const F &d1, const F &d2) const; //{}
    virtual bool EqualContextValuesForward(const F &d1, const F &d2) const;  //{}
    virtual F getPurelyLocalComponentForward(const F &dfv) const;
    virtual F getPurelyGlobalComponentForward(const F &dfv) const;
    virtual F getMixedComponentForward(const F &dfv) const;
    virtual F getCombinedValuesAtCallForward(const F &dfv1, const F &dfv2) const;
    virtual void printDataFlowValuesForward(const F &dfv) const {}

    virtual F performMergeAtCallForward(const F &d1, const F &d2); //{}

    virtual B computeInFromOut(llvm::Instruction &I, int depContext);
    virtual B computeInFromOut(BaseInstruction *I, int depContext);

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

    virtual std::vector<Function *> getIndirectCalleeFromIN(long int, F &);
    virtual B getFPandArgsBackward(long int, Instruction *);
    virtual F getPinStartCallee(long int, Instruction *, F &, Function *);

    virtual unsigned int getSize(F &);
    virtual unsigned int getSize(B &);

    virtual void printFileDataFlowValuesForward(const F &dfv, std::ofstream &out) const {}
    virtual void printFileDataFlowValuesBackward(const B &dfv, std::ofstream &out) const {}

    virtual B functionCallEffectBackward(Function *, const B &);
    virtual F functionCallEffectForward(Function *, const F &);

    // #########
    int getCalleeContext(int callerContext, BaseInstruction *I);
    bool isGenerativeKillInitialised(int context_label, BaseInstruction *I);
    pair<pair<F, B>, pair<F, B>> getGenerativeKillAtCall(int context_label, BaseInstruction *I);
    void setGenerativeAtCall(int context_label, BaseInstruction *I, pair<F, B> generativeAtCall);
    void setKillAtCall(int context_label, BaseInstruction *I, pair<F, B> killAtCall);
    void setInitialisationValueGenerativeKill(int context_label, BaseInstruction *I);

    virtual pair<pair<F, B>, pair<F, B>> getInitialisationValueGenerativeKill();
    virtual pair<F, B> computeGenerative(int context_label, Function *Func);
    virtual pair<F, B> computeKill(int context_label, Function *Func);
    virtual pair<F, B> CallInflowFunction(
        int context_label, Function *Func, BasicBlock *bb, const F &a1, const B &d1, pair<F, B> generativeAtCall);
    virtual pair<F, B> CallOnflowFunction(int context_label, BaseInstruction *I, pair<F, B> killAtCall);

    // For Flow-insensitive analysis
    void INIT_CONTEXT_FIS(llvm::Function *, const F &, const F &, int);
    void doAnalysisFIS(int context_label, Function *func);
    F NormalFlowFunctionFIS(pair<int, BasicBlock *>, int depAnalysisContext);

    void setDFValueFIS(int, Function *, const F &);
    F getDFValueFIS(int, Function *);

    virtual F getBoundaryInformationFIS();
    virtual F getInitialisationValueFIS();
    virtual void printDataFlowValuesFIS(const F &dfv) const {}
    virtual bool EqualDataFlowValuesFIS(const F &d1, const F &d2) const;
    virtual F computeDFValueFIS(llvm::Instruction &I, int depContext);
    virtual F computeDFValueFIS(BaseInstruction *I, int depContext);
    virtual F performMeetFIS(const F &d1, const F &d2);
    virtual F getPurelyLocalComponentFIS(const F &dfv) const;
    virtual F getPurelyGlobalComponentFIS(const F &dfv) const;
    virtual F CallInflowFunctionFIS(int, Function *, BasicBlock *, const F &);
    virtual F CallOutflowFunctionFIS(int, Function *, BasicBlock *, const F &, const F &);
    virtual F performMergeAtCallFIS(const F &d1, const F &d2);

    void doAnalysis(std::unique_ptr<llvm::Module> &M, slim::IR *optIR, Function *Func, int depAnalysisContext);
    virtual int getDepAnalysisCalleeContext(int callerContext, BaseInstruction *inst);
    virtual int getDepAnalysisCalleeContext(int callerContext, llvm::Instruction &inst);
    // #########

    // Temp added for testing
    BaseInstruction *getBaseInstruction(long long id);
    // BaseInstruction * getBaseIns(long long id, SLIM);
    void setSLIMIRPointer(slim::IR *slimIRptr);
    bool compareIndices(std::vector<SLIMOperand *>, std::vector<SLIMOperand *>) const;
};

template <class F, class B>
bool AnalysisGenKill<F, B>::compareIndices(std::vector<SLIMOperand *> ipVec1, std::vector<SLIMOperand *> ipVec2) const {
    if (ipVec1.size() != ipVec2.size())
        return false;

    for (int i = 0; i < ipVec1.size(); i++) {
        if (ipVec1[i]->getName() != ipVec2[i]->getName())
            return false;
    }
    return true;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setSLIMIRPointer(slim::IR *slimIRptr) {
    this->optIR = slimIRptr;
}

template <class F, class B>
BaseInstruction *AnalysisGenKill<F, B>::getBaseInstruction(long long id) {
    BaseInstruction *ins = optIR->getInstrFromIndex(id);
    return ins;
}

// #########
template <class F, class B>
F AnalysisGenKill<F, B>::getForwardComponentAtInOfThisInstruction(BaseInstruction *I, int label) {
    // int label = getProcessingContextLabel();
    return SLIM_IN[label][I].first;
}

template <class F, class B>
F AnalysisGenKill<F, B>::getForwardComponentAtOutOfThisInstruction(BaseInstruction *I, int label) {
    // int label = getProcessingContextLabel();
    return SLIM_OUT[label][I].first;
}

template <class F, class B>
B AnalysisGenKill<F, B>::getBackwardComponentAtInOfThisInstruction(BaseInstruction *I, int label) {
    // int label = getProcessingContextLabel();
    return SLIM_IN[label][I].second;
}

template <class F, class B>
B AnalysisGenKill<F, B>::getBackwardComponentAtOutOfThisInstruction(BaseInstruction *I, int label) {
    // int label = getProcessingContextLabel();
    return SLIM_OUT[label][I].second;
}

template <class F, class B>
int AnalysisGenKill<F, B>::getCalleeContext(int callerContext, BaseInstruction *I) {
    pair<int, BaseInstruction *> callerInfo = make_pair(callerContext, I);
    if (SLIM_context_transition_graph.find(callerInfo) != SLIM_context_transition_graph.end())
        return SLIM_context_transition_graph[callerInfo];
    else
        return -1;
}

template <class F, class B>
bool AnalysisGenKill<F, B>::isGenerativeKillInitialised(int context_label, BaseInstruction *I) {
    pair<int, BaseInstruction *> keyGenerativeKill = make_pair(context_label, I);
    if (GenerativeKill.find(keyGenerativeKill) != GenerativeKill.end()) {
        // key is present, thus initialised, so return true
        return true;
    }
    return false;
}

template <class F, class B>
pair<pair<F, B>, pair<F, B>> AnalysisGenKill<F, B>::getGenerativeKillAtCall(int context_label, BaseInstruction *I) {
    pair<int, BaseInstruction *> keyGenerativeKill = make_pair(context_label, I);
    // TODO what to do if not present?
    return GenerativeKill[keyGenerativeKill];
}

template <class F, class B>
void AnalysisGenKill<F, B>::setGenerativeAtCall(int context_label, BaseInstruction *I, pair<F, B> generativeAtCall) {
    pair<int, BaseInstruction *> keyGenerativeKill = make_pair(context_label, I);
    // setting existing value of Kill and new value of Generative
    pair<pair<F, B>, pair<F, B>> valueGenerativeKill =
        make_pair(generativeAtCall, getGenerativeKillAtCall(context_label, I).second);
    GenerativeKill[keyGenerativeKill] = valueGenerativeKill;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setKillAtCall(int context_label, BaseInstruction *I, pair<F, B> killAtCall) {
    pair<int, BaseInstruction *> keyGenerativeKill = make_pair(context_label, I);
    // setting existing value of Generative and new value of Kill
    pair<pair<F, B>, pair<F, B>> valueGenerativeKill =
        make_pair(getGenerativeKillAtCall(context_label, I).first, killAtCall);
    GenerativeKill[keyGenerativeKill] = valueGenerativeKill;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setInitialisationValueGenerativeKill(int context_label, BaseInstruction *I) {
    pair<pair<F, B>, pair<F, B>> initGenerativeKill = getInitialisationValueGenerativeKill();
    pair<int, BaseInstruction *> keyGenerativeKill = make_pair(context_label, I);
    GenerativeKill[keyGenerativeKill] = initGenerativeKill;
}

//--virtual functions--
template <class F, class B>
pair<pair<F, B>, pair<F, B>> AnalysisGenKill<F, B>::getInitialisationValueGenerativeKill() {
    llvm::outs() << "\nThis function getInitialisationValueGenerativeKill() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> AnalysisGenKill<F, B>::computeGenerative(int context_label, Function *Func) {
    llvm::outs() << "\nThis function computeGenerative() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> AnalysisGenKill<F, B>::computeKill(int context_label, Function *Func) {
    llvm::outs() << "\nThis function computeKill() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> AnalysisGenKill<F, B>::CallInflowFunction(
    int context_label, Function *Func, BasicBlock *bb, const F &a1, const B &d1, pair<F, B> generativeAtCall) {
    llvm::outs() << "\nThis function CallInflowFunction() with Generative has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> AnalysisGenKill<F, B>::CallOnflowFunction(int context_label, BaseInstruction *I, pair<F, B> killAtCall) {
    llvm::outs() << "\nThis function CallOnflowFunction() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::getBoundaryInformationFIS() {
    llvm::outs() << "\nThis function getBoundaryInformationFIS() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::getInitialisationValueFIS() {
    llvm::outs() << "\nThis function getInitialisationValueFIS() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
bool AnalysisGenKill<F, B>::EqualDataFlowValuesFIS(const F &d1, const F &d2) const {
    llvm::outs() << "\nThis function EqualDataFlowValuesFIS() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::computeDFValueFIS(llvm::Instruction &I, int depContext) {
    llvm::outs() << "\nThis function computeDFValueFIS() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::computeDFValueFIS(BaseInstruction *I, int depContext) {
    llvm::outs() << "\nThis function computeDFValueFIS() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::performMeetFIS(const F &d1, const F &d2) {
    llvm::outs() << "\nThis function performMeetFIS() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::getPurelyGlobalComponentFIS(const F &dfv) const {
    llvm::outs() << "\nThis function getPurelyGlobalComponentFIS() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::getPurelyLocalComponentFIS(const F &dfv) const {
    llvm::outs() << "\nThis function getPurelyLocalComponentFIS() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::CallInflowFunctionFIS(
    int context_label, Function *target_function, BasicBlock *bb, const F &a1) {
    llvm::outs() << "\nThis function CallInflowFunctionFIS() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::CallOutflowFunctionFIS(
    int context_label, Function *target_function, BasicBlock *bb, const F &a3, const F &a1) {
    llvm::outs() << "\nThis function CallOutflowFunctionFIS() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::performMergeAtCallFIS(const F &d1, const F &d2) {
    llvm::outs() << "\nThis function performMergeAtCallFIS() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
int AnalysisGenKill<F, B>::getDepAnalysisCalleeContext(int callerContext, BaseInstruction *inst) {
    llvm::outs() << "\nThis function getDepAnalysisCalleeContext() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
int AnalysisGenKill<F, B>::getDepAnalysisCalleeContext(int callerContext, llvm::Instruction &inst) {
    llvm::outs() << "\nThis function getDepAnalysisCalleeContext() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
void AnalysisGenKill<F, B>::doAnalysis(
    std::unique_ptr<llvm::Module> &Mod, slim::IR *slimIRObj, Function *Func, int depAnalysisContext) {
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

    llvm::outs() << "Inside doAnalysis with SLIM parameter as " << SLIM << "\n";
    if (SLIM) {
        auto start = high_resolution_clock::now();

        // llvm::outs() << "\nPrinting inst_id_to_object Global Instructions List \n";
        // for(auto ins: optIR->inst_id_to_object)
        //     llvm::outs() << ins.first << ", ";
        // llvm::outs() << "\n";

        auto stop = high_resolution_clock::now();
        this->SLIMTime = duration_cast<milliseconds>(stop - start);

        // optIR->dumpIR();
    }
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
        INIT_CONTEXT(
            Func, {forward_inflow_bi, backward_inflow_bi}, {forward_outflow_bi, backward_outflow_bi},
            depAnalysisContext);

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
        }
    } else {
        // FISP or FIFP
        F flow_insensitive_inflow;
        F flow_insensitive_outflow;
        if (std::is_same<B, NoAnalysisType>::value) {
            flow_insensitive_inflow = getBoundaryInformationFIS();
        }
        setCurrentAnalysisDirection(0);
        INIT_CONTEXT_FIS(Func, flow_insensitive_inflow, flow_insensitive_outflow, depAnalysisContext);
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
void AnalysisGenKill<F, B>::INIT_CONTEXT_FIS(
    llvm::Function *function, const F &InflowFIS, const F &OutflowFIS, const int depAnalysisContext) {
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
            llvm::outs() << "INITIALIZING CONTEXT:-" << "\n";
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
            forward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), depAnalysisContext));

            // initialise IN-OUT maps for every instruction
            // Since flow insensitive, this step is not required
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
        if (current_context_label == 0) // main function with first invocation
        {
            setForwardInflowForThisContext(current_context_label, getBoundaryInformationFIS()); // setting inflow
                                                                                                // forward
        } else {
            setForwardInflowForThisContext(current_context_label, context_object->getInflowValue().first);
        }
        setDFValueFIS(current_context_label, function, getForwardInflowForThisContext(current_context_label));
    }
}

template <class F, class B>
void AnalysisGenKill<F, B>::doAnalysisFIS(int context_label, Function *func) {
    bool changed = true;
    while (changed) {
        F previous_dfvalue_of_this_function = getDFValueFIS(context_label, func);
        while (not forward_worklist.empty()) // step 2
        {
            // step 3 and 4
            pair<pair<int, BasicBlock *>, int> worklistElement = forward_worklist.workDelete();
            pair<int, BasicBlock *> current_pair = worklistElement.first;
            int depAnalysisContext = worklistElement.second;
            int current_context_label = current_pair.first;
            BasicBlock *bb = current_pair.second;
            setProcessingContextLabel(current_context_label);

#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Popping from Flow Insensitive Worklist (Context:" << current_context_label << ")\n";
            for (Instruction &I : *bb)
                llvm::outs() << I << "\n";
#endif
            BasicBlock &b = *bb;
            Function *f = context_label_to_context_object_map[current_context_label]->getFunction();
            Function &function = *f;

            // step 5
            /*if (bb != (&function.getEntryBlock())) {
                //step 6
                setForwardIn(current_pair.first, current_pair.second, getInitialisationValueForward());

                //step 7 and 8
                for (auto pred_bb:predecessors(bb)) {
                    llvm::outs()<<"\n";
                    //get first instruction of bb and setCurrentInstruction(inst);
                    BaseInstruction *firstInst = optIR->inst_id_to_object[optIR->getFirstIns(bb->getParent(),bb)];
                    setCurrentInstruction(firstInst);

                    printDataFlowValuesForward(getIn(current_pair.first, current_pair.second).first);
                    //llvm::outs() << "ISues in perform meet\n";
                    setForwardIn(current_pair.first, current_pair.second,
                                performMeetForward(getIn(current_pair.first, current_pair.second).first,
                                                    getOut(current_pair.first, pred_bb).first, "IN")); //
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
                        // Changed to fetch target (Callee) function from SLIM IR instead of LLVM IR
                        CallInstruction *cTempInst = new CallInstruction(Inst);
                        if (cTempInst->isIndirectCall())
                            continue;
                        Function *target_function = cTempInst->getCalleeFunction();
                        if (target_function &&
                            (target_function->isDeclaration() || isAnIgnorableDebugInstruction(Inst))) {
                            continue; // this is an inbuilt function so doesn't need to be processed.
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
                            continue; // this is an inbuilt function so doesn't need to be processed.
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
                            Function *target_function = ci->getCalledFunction(); */
                            // Changed to fetch target (Callee) function from SLIM IR instead of LLVM IR
                            CallInstruction *cTempInst = new CallInstruction(Inst);
                            if (cTempInst->isIndirectCall())
                                continue;
                            Function *target_function = cTempInst->getCalleeFunction();
                            if (not target_function || target_function->isDeclaration() ||
                                isAnIgnorableDebugInstruction(Inst)) {
                                continue; // this is an inbuilt function so doesn't need to be processed.
                            }

                            if (intraprocFlag) {
                                // TODO
                            } else {
                                F inflow;
                                if (byPass) {
                                    // TODO
                                } else {
                                    inflow =
                                        CallInflowFunctionFIS(current_context_label, target_function, bb, prevdfval);
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
                                        llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                                        inst->printInstruction();
                                        llvm::outs() << "\nDataflow value: ";
                                    }

                                    // step 14
                                    pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                    SLIM_context_transition_graph[mypair] = getContextLabelCounter();
                                    // step 16 and 17
                                    F a3 = getForwardOutflowForThisContext(matching_context_label);
                                    B d3 = getBackwardOutflowForThisContext(matching_context_label);

                                    F outflow = CallOutflowFunctionFIS(
                                        current_context_label, target_function, bb, a3, prevdfval);

                                    F outflowVal = outflow;
                                    if (debug) {
                                        llvm::outs() << "\nThe outflow value obtained for function "
                                                     << target_function->getName() << " for context "
                                                     << matching_context_label << " is ";
                                        printDataFlowValuesFIS(a3);
                                        llvm::outs() << "\nOutflow after CallOutflowFunctionFIS ";
                                        printDataFlowValuesFIS(outflowVal);
                                        llvm::outs() << "\n";
                                    }
                                    // B d4 = outflow_pair.second;

                                    // step 18 and 19

                                    // At the call instruction, the value at IN should be splitted into two components.
                                    // The purely global component is given to the callee while the mixed component is
                                    // propagated to OUT of this instruction after executing computeOUTfromIN() on it.

                                    // #########
                                    F a5;
                                    if (byPass) {
                                        // TODO
                                        // int calleeContext = getCalleeContext(current_context_label, inst);
                                        // pair<F,B> killAtCall = computeKill(calleeContext, target_function);
                                        // setKillAtCall(current_context_label, inst, killAtCall);
                                        // pair<F, B> onflow_pair = CallOnflowFunction(current_context_label, inst,
                                        // killAtCall); a5 = onflow_pair.first;
                                    } else {
                                        a5 = getPurelyLocalComponentFIS(prevdfval);
                                    }

                                    F dfval = performMeetFIS(prevdfval, performMergeAtCallFIS(outflowVal, a5));
                                    setDFValueFIS(current_pair.first, f, dfval);
                                    prevdfval = dfval;
                                    if (debug) {
                                        printDataFlowValuesFIS(prevdfval);
                                        printLine(current_context_label, 1);
                                    }
                                } else {
// For analysing caller again, insert caller into worklist here itself (since at a later point,
//  we are not able to get the dependent analysis context of the caller from the end of callee analysis)
#ifdef PRVASCO
                                    llvm::outs()
                                        << "\nInserting caller node into Worklist at end of callee in FIS Analysis "
                                           "(Context:"
                                        << current_context_label << " to " << worklistElement.first.first << ")\n";
                                    for (Instruction &I : *bb)
                                        llvm::outs() << I << "\n";
#endif
                                    forward_worklist.workInsert(worklistElement);

                                    // create new context
                                    int depAnalysisCalleeContext =
                                        getDepAnalysisCalleeContext(depAnalysisContext, inst);
                                    INIT_CONTEXT_FIS(target_function, a2, new_outflow_fis, depAnalysisCalleeContext);
                                    // step 14
                                    // This step must be done after above step because context label counter gets
                                    // updated after INIT-Context is invoked.
                                    pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                    SLIM_context_transition_graph[mypair] = getContextLabelCounter();
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

                            F newdfval = computeDFValueFIS(inst, depAnalysisContext);
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
                                continue; // this is an inbuilt function so doesn't need to be processed.
                            }

                            // At the call instruction, the value at IN should be splitted into two components:
                            // 1) Purely Global and 2) Mixed.
                            // The purely global component is given to the start of callee.

                            // step 12
                            F inflow = CallInflowFunctionFIS(current_context_label, target_function, bb, prevdfval);

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

                                F outflow =
                                    CallOutflowFunctionFIS(current_context_label, target_function, bb, a3, prevdfval);

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
// For analysing caller again, insert caller into worklist here itself (since at a later point,
//  we are not able to get the dependent analysis context of the caller from the end of callee analysis)
#ifdef PRVASCO
                                llvm::outs() << "\nInserting caller node into Worklist at end of callee in FIS "
                                                "Analysis (Context:"
                                             << current_context_label << " to " << worklistElement.first.first << ")\n";
                                for (Instruction &I : *bb)
                                    llvm::outs() << I << "\n";
#endif
                                forward_worklist.workInsert(worklistElement);

                                int depAnalysisCalleeContext = getDepAnalysisCalleeContext(depAnalysisContext, I);
                                INIT_CONTEXT_FIS(
                                    target_function, a2, new_outflow_fis, depAnalysisCalleeContext); // step 21

                                // step 14
                                // This step must be done after above step because context label counter gets updated
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

                            F newdfval = computeDFValueFIS(*inst, depAnalysisContext);
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
                // TODO meet with existing value
                F prevdfval = getDFValueFIS(context_label, func);
                F dfval = NormalFlowFunctionFIS(current_pair, depAnalysisContext);
                setDFValueFIS(current_pair.first, f, performMeetFIS(prevdfval, dfval));
            }

            // we need the context of the processed node, so adding inside while(worklist not empty)
            // Update outflow
            F outflowVal = getPurelyGlobalComponentFIS(getDFValueFIS(context_label, func));
            if (debug) {
                llvm::outs() << "\nUpdating outflow for function " << func->getName() << " for context "
                             << current_context_label << " as ";
                printDataFlowValuesFIS(outflowVal);
                llvm::outs() << "\n";
            }
            setForwardOutflowForThisContext(current_context_label, outflowVal); // setting forward outflow
            // TODO Caller not popped from Worklist
            /*if (bb == &function.back())//step 27
            {
                if(SLIM) {
                    for(auto context_inst_pairs : SLIM_context_transition_graph) {
                        if (context_inst_pairs.second == current_context_label) { //Matching called function
                        //step 30
                        BasicBlock *bb = context_inst_pairs.first.second->getBasicBlock();
                        pair<int, BasicBlock *> context_bb_pair = make_pair(context_inst_pairs.first.first, bb);

                            #ifdef PRVASCO
                                llvm::outs() << "\nInserting caller node into Worklist at end of callee in FIS Analysis
            (Context:" << current_context_label << " to " << context_inst_pairs.first.first << ")\n"; for (Instruction
            &I : *bb) llvm::outs() << I << "\n"; #endif forward_worklist.workInsert(context_bb_pair); break;
                        }
                    }
                }else{
                    for (auto context_inst_pairs : context_transition_graph) {
                        if (context_inst_pairs.second == current_context_label) {
                            //step 30
                            BasicBlock *bb = context_inst_pairs.first.second->getParent();
                            pair<int, BasicBlock *> context_bb_pair = make_pair(context_inst_pairs.first.first, bb);
                            forward_worklist.workInsert(context_bb_pair);
                            break;
                        }
                    }
                }
            }*/

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

        // TODO currently not needed since Gen and Kill needs only Single pass
        /*if (changed && modeOfExec==FIFP)//step 24
        {
            //not yet converged
            //Add all basic blocks to worklist again

            for (BasicBlock *BB:post_order(&func->getEntryBlock())) {
                BasicBlock &b = *BB;
                forward_worklist.workInsert(make_pair(make_pair(context_label,&b), depAnalysisContext));

                process_mem_usage(this->vm, this->rss);
                this->total_memory = max(this->total_memory, this->vm);
            }

            /*for (auto succ_bb:successors(bb))//step 25
            {
                #ifdef PRVASCO
                    llvm::outs() << "\n";
                    llvm::outs() << "Inserting successors into Forward Worklist on change in OUT value in Forward
        Analysis (Context:" << current_context_label << ")\n"; for (Instruction &I : *succ_bb) llvm::outs() << I <<
        "\n"; #endif
                //step 26
                forward_worklist.workInsert({current_context_label,succ_bb});
            }//end comment

        }*/
    }

    if (debug) {
        llvm::outs() << "Outflow value final at function " << func->getName() << " is ";
    }
    printDataFlowValuesFIS(getForwardOutflowForThisContext(context_label));
}

template <class F, class B>
F AnalysisGenKill<F, B>::NormalFlowFunctionFIS(
    pair<int, BasicBlock *> current_pair_of_context_label_and_bb, int depAnalysisContext) {
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

            F newdfvalue = computeDFValueFIS(inst, depAnalysisContext);
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
            dfvalue = computeDFValueFIS(*inst, depAnalysisContext);

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
void AnalysisGenKill<F, B>::setDFValueFIS(int label, Function *func, const F &dataflowvalue) {
    DFVALUE[label][func] = dataflowvalue;
    return;
}

template <class F, class B>
F AnalysisGenKill<F, B>::getDFValueFIS(int label, Function *func) {
    return DFVALUE[label][func];
}

// #########
template <class F, class B>
AnalysisGenKill<F, B>::AnalysisGenKill(
    std::unique_ptr<llvm::Module> &module, bool debug, bool SLIM, bool intraProc, ExecMode modeOfExec, bool byPass) {
    // bool intraProc :
    //   set true for intraprocedural analysis
    //   set false for interprocedural analysis
    llvm::outs() << "\nInside CONSTRUCTOR with slim as: " << SLIM;
    current_module = nullptr;
    context_label_counter = -1;
    this->debug = debug;
    this->direction = "";
    this->SLIM = SLIM;
    this->current_instruction = nullptr;
    this->intraprocFlag = intraProc;
    this->modeOfExec = modeOfExec;
    // bool byPass:
    //   set true for bypassing
    //   set false otherwise
    this->byPass = byPass;
}

template <class F, class B>
AnalysisGenKill<F, B>::AnalysisGenKill(
    std::unique_ptr<llvm::Module> &module, const string &fileName, bool debug, bool SLIM, bool intraProc,
    ExecMode modeOfExec, bool byPass) {
    // bool intraProc :
    //   set true for intraprocedural analysis
    //   set false for interprocedural analysis
    llvm::outs() << "\nInside CONSTRUCTOR with file output and slim as: " << SLIM;
    current_module = nullptr;
    context_label_counter = -1;
    this->debug = debug;
    freopen(fileName.c_str(), "w", stdout);
    this->direction = "";
    this->SLIM = SLIM;
    this->current_instruction = nullptr;
    this->intraprocFlag = intraProc;
    this->modeOfExec = modeOfExec;
    // bool byPass:
    //   set true for bypassing
    //   set false otherwise
    this->byPass = byPass;
}

// #########

/*template<class F, class B>
AnalysisGenKill<F,B>::AnalysisGenKill(bool debug,bool SLIM) {
    current_module = nullptr;
    context_label_counter = -1;
    this->debug = debug;
    this->direction = "";
    this->SLIM = SLIM;
    this->current_instruction = nullptr;
}

template<class F, class B>
AnalysisGenKill<F,B>::AnalysisGenKill(bool debug, const string &fileName, bool SLIM) {
    current_module = nullptr;
    context_label_counter = -1;
    this->debug = debug;
    freopen(fileName.c_str(), "w", stdout);
    this->direction = "";
    this->SLIM = SLIM;
    this->current_instruction = nullptr;
}

template<class F, class B>
AnalysisGenKill<F,B>::AnalysisGenKill(std::unique_ptr<llvm::Module> &module, bool debug, bool SLIM, bool intraProc) {
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
AnalysisGenKill<F,B>::AnalysisGenKill(std::unique_ptr<llvm::Module> &module, const string &fileName, bool debug, bool
SLIM, bool intraProc) {
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
AnalysisGenKill<F, B>::~AnalysisGenKill() {}

//=========================================================================================

template <class F, class B>
int AnalysisGenKill<F, B>::getContextLabelCounter() {
    return context_label_counter;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setContextLabelCounter(int new_context_label_counter) {
    context_label_counter = new_context_label_counter;
}

template <class F, class B>
int AnalysisGenKill<F, B>::getCurrentAnalysisDirection() {
    return current_analysis_direction;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setCurrentAnalysisDirection(int direction) {
    current_analysis_direction = direction;
}

template <class F, class B>
int AnalysisGenKill<F, B>::getProcessingContextLabel() const {
    return processing_context_label;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setProcessingContextLabel(int label) {
    processing_context_label = label;
}

template <class F, class B>
unsigned int AnalysisGenKill<F, B>::getNumberOfContexts() {
    return ProcedureContext.size();
}

template <class F, class B>
bool AnalysisGenKill<F, B>::isAnIgnorableDebugInstruction(llvm::Instruction *inst) {
    /* if (isa<DbgDeclareInst>(inst) || isa<DbgValueInst>(inst)) {
         return true;
     }*/
    return false;
}

template <class F, class B>
bool AnalysisGenKill<F, B>::isIgnorableInstruction(BaseInstruction *inst) {
    /*if (isa<DbgDeclareInst>(inst) || isa<DbgValueInst>(inst)) {
        return true;
    }*/
    Instruction *Inst = inst->getLLVMInstruction();
    if (inst->getInstructionType() == UNREACHABLE)
        return true;

    if (inst->getInstructionType() == SWITCH)
        return true;

    if (inst->getCall()) { //(llvm::CallInst *ci = dyn_cast<llvm::CallInst>(Inst)){
        /*CallInst *ci = dyn_cast<CallInst>(inst);
        Function *target_function = ci->getCalledFunction();*/
        // Changed to fetch target (Callee) function from SLIM IR instead of LLVM IR
        CallInstruction *cTempInst = new CallInstruction(inst->getLLVMInstruction());
        if (cTempInst->isIndirectCall())
            return true;
        Function *target_function = cTempInst->getCalleeFunction();
        if (not target_function || target_function->isDeclaration() || isAnIgnorableDebugInstruction(Inst)) {
            return true; // this is an inbuilt function so doesn't need to be processed.
        }
    }

    return false;
}

template <class F, class B>
bool AnalysisGenKill<F, B>::isAnIgnorableDebugInstruction(BaseInstruction *inst) {
    return false;
}

template <class F, class B>
void AnalysisGenKill<F, B>::startSplitting(std::unique_ptr<llvm::Module> &mod) {
    // for (Function &function : *(this->current_module)) {
    for (Function &function : *mod) {
        if (function.size() > 0) {
            performSplittingBB(function);
        }
    }
}

template <class F, class B>
void AnalysisGenKill<F, B>::performSplittingBB(Function &function) {
    int flag = 0;
    Instruction *I = nullptr;
    bool previousInstructionWasSplitted = false;
    bool previousInstructionWasCallInstruction = false;
    map<Instruction *, bool> isSplittedOnThisInstruction;
    for (BasicBlock *BB : inverse_post_order(&function.back())) {
        BasicBlock &b = *BB;
        previousInstructionWasSplitted = true;
        previousInstructionWasCallInstruction = false;
        for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
            I = &(*inst);
            if (inst == &*(b.begin())) {
                if (isa<CallInst>(*I)) {
                    CallInst *ci = dyn_cast<CallInst>(I);

                    Function *target_function = ci->getCalledFunction();
                    if (not target_function || target_function->isDeclaration() || isAnIgnorableDebugInstruction(I)) {
                        continue; // this is an inbuilt function so doesn't need to be processed.
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
                if (not target_function || target_function->isDeclaration() || isAnIgnorableDebugInstruction(I)) {
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
Module *AnalysisGenKill<F, B>::getCurrentModule() {
    return current_module;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setCurrentModule(Module *m) {
    current_module = m;
}

template <class F, class B>
BaseInstruction *AnalysisGenKill<F, B>::getCurrentInstruction() const {
    return this->current_instruction;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setCurrentInstruction(BaseInstruction *current_instruction) {
    this->current_instruction = current_instruction;
}

//=========================================================================================

template <class F, class B>
void AnalysisGenKill<F, B>::printLine(int label, int startend) {
    string Name = "";
    if (current_analysis_direction == 1) {
        Name = "FORWARDGenKill";
    } else {
        Name = "BACKWARDGenKill";
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
void AnalysisGenKill<F, B>::printModule(Module &M) {
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
void AnalysisGenKill<F, B>::printContext() {
    llvm::outs() << "\n";
    for (auto label : ProcedureContext) {
        llvm::outs()
            << "=================================================================================================="
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
        llvm::outs()
            << "=================================================================================================="
            << "\n";
    }
}

template <class F, class B>
void AnalysisGenKill<F, B>::printInOutMaps() {
    llvm::outs() << "\n";
}

template <class F, class B>
float AnalysisGenKill<F, B>::getTotalMemory() {
    return this->total_memory;
}

template <class F, class B>
void AnalysisGenKill<F, B>::printStats() {
    llvm::Module *M = this->getCurrentModule();
    string fileName = M->getName().str();
    fileName += "_stats.txt";
    std::ofstream fout(fileName);
    std::unordered_map<llvm::Function *, int> CountContext;
    fout << "\n=================-------------------Statistics of Analysis-------------------=================";
    fout << "\n Total number of Contexts: " << this->getNumberOfContexts();
    fout << "\n Total time taken in Splitting Basic Blocks: " << this->SplittingBBTime.count() << " milliseconds";
    fout << "\n Total time taken in SLIM Modelling: " << this->SLIMTime.count() << " milliseconds";
    fout << "\n Total time taken in Analysis: " << this->AnalysisTime.count() << " milliseconds";
    fout << "\n Total memory taken by Analysis: ";
    printMemory(this->total_memory, fout);
    fout << "\n------------------------List of all Contexts------------------------------------------";
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

    fout << "\n\n--------------------------Average size of Data Flow Values over "
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

//===============================Virtual functions=========================================

template <class F, class B>
B AnalysisGenKill<F, B>::getFPandArgsBackward(long int Index, Instruction *I) {
    llvm::outs() << "\nThis function getFPandArgs() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::getPinStartCallee(long int index, Instruction *I, F &dfv, Function *Func) {
    llvm::outs() << "\nThis function getPinStartCallee() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
std::vector<Function *> AnalysisGenKill<F, B>::getIndirectCalleeFromIN(long int Index, F &dfv) {
    llvm::outs() << "\nThis function getIndirectCalleeFromIN() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B AnalysisGenKill<F, B>::computeInFromOut(llvm::Instruction &I, int depContext) {
    llvm::outs() << "\nThis function computeInFromOut() has not been implemented. EXITING 1 !!\n";
    exit(-1);
}

template <class F, class B>
B AnalysisGenKill<F, B>::computeInFromOut(BaseInstruction *I, int depContext) {
    llvm::outs() << "\nThis function computeInFromOut() has not been implemented. EXITING 2 !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::computeOutFromIn(llvm::Instruction &I, int depContext) {
    llvm::outs() << "\nThis function computeOutFromIn() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::computeOutFromIn(BaseInstruction *I, int depContext) {
    llvm::outs() << "\nThis function computeOutFromIn() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::getBoundaryInformationForward() {
    llvm::outs() << "\nThis function getBoundaryInformationForward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B AnalysisGenKill<F, B>::getBoundaryInformationBackward() {
    llvm::outs() << "\nThis function getBoundaryInformationBackward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::getInitialisationValueForward() {
    llvm::outs() << "\nThis function getInitialisationValueForward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B AnalysisGenKill<F, B>::getInitialisationValueBackward() {
    llvm::outs() << "\nThis function getInitialisationValueBackward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::performMeetForward(const F &d1, const F &d2, const string pos) {
    llvm::outs() << "\nThis function performMeetForward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::performMergeAtCallForward(const F &d1, const F &d2) {
    llvm::outs() << "\nThis function performMergeAtCallForward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B AnalysisGenKill<F, B>::performMeetBackward(const B &d1, const B &d2) const {
    llvm::outs() << "\nThis function performMeetBackward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B AnalysisGenKill<F, B>::performMergeAtCallBackward(const B &d1, const B &d2) const {
    llvm::outs() << "\nThis function performMergeAtCallBackward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
bool AnalysisGenKill<F, B>::EqualDataFlowValuesForward(const F &d1, const F &d2) const {
    llvm::outs() << "\nThis function EqualDataFlowValuesForward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
bool AnalysisGenKill<F, B>::EqualContextValuesForward(const F &d1, const F &d2) const {
    llvm::outs() << "\nThis function EqualContextValuesForward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
bool AnalysisGenKill<F, B>::EqualDataFlowValuesBackward(const B &d1, const B &d2) const {
    llvm::outs() << "\nThis function EqualDataFlowValuesBackward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
bool AnalysisGenKill<F, B>::EqualContextValuesBackward(const B &d1, const B &d2) const {
    llvm::outs() << "\nThis function EqualContextValuesBackward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B AnalysisGenKill<F, B>::getPurelyGlobalComponentBackward(const B &dfv) const {
    llvm::outs() << "\nThis function getPurelyGlobalComponentBackward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::getPurelyGlobalComponentForward(const F &dfv) const {
    llvm::outs() << "\nThis function getPurelyGlobalComponentForward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::getPurelyLocalComponentForward(const F &dfv) const {
    llvm::outs() << "\nThis function getPurelyLocalComponentForward() has not been implemented. EXITING !!\n";
    exit(-1);
}
template <class F, class B>
B AnalysisGenKill<F, B>::getPurelyLocalComponentBackward(const B &dfv) const {
    llvm::outs() << "\nThis function getPurelyLocalComponentBackward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B AnalysisGenKill<F, B>::getMixedComponentBackward(const B &dfv) const {
    llvm::outs() << "\nThis function getMixedComponentBackward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::getMixedComponentForward(const F &dfv) const {
    llvm::outs() << "\nThis function getMixedComponentForward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B AnalysisGenKill<F, B>::getCombinedValuesAtCallBackward(const B &dfv1, const B &dfv2) const {
    llvm::outs() << "\nThis function getCombinedValuesAtCallBackward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::getCombinedValuesAtCallForward(const F &dfv1, const F &dfv2) const {
    llvm::outs() << "\nThis function getCombinedValuesAtCallForward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
unsigned int AnalysisGenKill<F, B>::getSize(F &dfv) {
    llvm::outs() << "\nThis function getSize() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
unsigned int AnalysisGenKill<F, B>::getSize(B &dfv) {
    llvm::outs() << "\nThis function getSize() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
B AnalysisGenKill<F, B>::functionCallEffectBackward(Function *target_function, const B &dfv) {
    llvm::outs() << "\nThis function functionCallEffectBackward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
F AnalysisGenKill<F, B>::functionCallEffectForward(Function *target_function, const F &dfv) {
    llvm::outs() << "\nThis function functionCallEffectForward() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> AnalysisGenKill<F, B>::CallInflowFunction(
    int context_label, Function *target_function, BasicBlock *bb, const F &a1, const B &d1) {
    llvm::outs() << "\nThis function CallInflowFunction() has not been implemented. EXITING !!\n";
    exit(-1);
}

template <class F, class B>
pair<F, B> AnalysisGenKill<F, B>::CallOutflowFunction(
    int context_label, Function *target_function, BasicBlock *bb, const F &a3, const B &d3, const F &a1, const B &d1) {
    llvm::outs() << "\nThis function CallOutflowFunction() has not been implemented. EXITING !!\n";
    exit(-1);
}

//=====================setter and getters for IN-OUT Maps==================================

template <class F, class B>
F AnalysisGenKill<F, B>::getForwardComponentAtInOfThisInstruction(llvm::Instruction &I) {
    int label = getProcessingContextLabel();
    return IN[label][&I].first;
}

template <class F, class B>
F AnalysisGenKill<F, B>::getForwardComponentAtInOfThisInstruction(BaseInstruction *I) {
    int label = getProcessingContextLabel();
    return SLIM_IN[label][I].first;
}

template <class F, class B>
F AnalysisGenKill<F, B>::getForwardComponentAtOutOfThisInstruction(llvm::Instruction &I) {
    int label = getProcessingContextLabel();
    return OUT[label][&I].first;
}

template <class F, class B>
F AnalysisGenKill<F, B>::getForwardComponentAtOutOfThisInstruction(BaseInstruction *I) {
    int label = getProcessingContextLabel();
    return SLIM_OUT[label][I].first;
}

template <class F, class B>
B AnalysisGenKill<F, B>::getBackwardComponentAtInOfThisInstruction(llvm::Instruction &I) {
    int label = getProcessingContextLabel();
    return IN[label][&I].second;
}

template <class F, class B>
B AnalysisGenKill<F, B>::getBackwardComponentAtInOfThisInstruction(BaseInstruction *I) {
    int label = getProcessingContextLabel();
    return SLIM_IN[label][I].second;
}

template <class F, class B>
B AnalysisGenKill<F, B>::getBackwardComponentAtOutOfThisInstruction(llvm::Instruction &I) {
    int label = getProcessingContextLabel();
    llvm::outs() << "label" << label;
    return OUT[label][&I].second;
}

template <class F, class B>
B AnalysisGenKill<F, B>::getBackwardComponentAtOutOfThisInstruction(BaseInstruction *I) {
    int label = getProcessingContextLabel();
    return SLIM_OUT[label][I].second;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setForwardComponentAtInOfThisInstruction(llvm::Instruction *I, const F &in_value) {
    int label = getProcessingContextLabel();
    IN[label][I].first = in_value;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setForwardComponentAtInOfThisInstruction(BaseInstruction *I, const F &in_value) {
    int label = getProcessingContextLabel();
    SLIM_IN[label][I].first = in_value;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setForwardComponentAtOutOfThisInstruction(llvm::Instruction *I, const F &out_value) {
    int label = getProcessingContextLabel();
    OUT[label][I].first = out_value;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setForwardComponentAtOutOfThisInstruction(BaseInstruction *I, const F &out_value) {
    int label = getProcessingContextLabel();
    SLIM_OUT[label][I].first = out_value;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setBackwardComponentAtInOfThisInstruction(llvm::Instruction *I, const B &in_value) {
    int label = getProcessingContextLabel();
    IN[label][I].second = in_value;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setBackwardComponentAtInOfThisInstruction(BaseInstruction *I, const B &in_value) {
    int label = getProcessingContextLabel();
    SLIM_IN[label][I].second = in_value;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setBackwardComponentAtOutOfThisInstruction(llvm::Instruction *I, const B &out_value) {
    int label = getProcessingContextLabel();
    OUT[label][I].second = out_value;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setBackwardComponentAtOutOfThisInstruction(BaseInstruction *I, const B &out_value) {
    int label = getProcessingContextLabel();
    SLIM_OUT[label][I].second = out_value;
}

//=====================setter and getters CS_BB============================================

template <class F, class B>
pair<F, B> AnalysisGenKill<F, B>::getIn(int label, llvm::BasicBlock *BB) {
    //    return IN[{label,&(*BB->begin())}];
    if (SLIM) {
        return SLIM_IN[label][optIR->inst_id_to_object[optIR->getFirstIns(BB->getParent(), BB)]];
    }
    return IN[label][&(*(BB->begin()))];
}

template <class F, class B>
pair<F, B> AnalysisGenKill<F, B>::getOut(int label, llvm::BasicBlock *BB) {
    if (SLIM) {
        return SLIM_OUT[label][optIR->inst_id_to_object[optIR->getLastIns(BB->getParent(), BB)]];
    }
    return OUT[label][&(BB->back())];
}

template <class F, class B>
void AnalysisGenKill<F, B>::setForwardIn(int label, llvm::BasicBlock *BB, const F &dataflowvalue) {
    if (SLIM) {
        SLIM_IN[label][optIR->inst_id_to_object[optIR->getFirstIns(BB->getParent(), BB)]].first = dataflowvalue;
        return;
    }
    IN[label][&(*(BB->begin()))].first = dataflowvalue;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setForwardOut(int label, llvm::BasicBlock *BB, const F &dataflowvalue) {
    if (SLIM) {
        SLIM_OUT[label][optIR->inst_id_to_object[optIR->getLastIns(BB->getParent(), BB)]].first = dataflowvalue;
        return;
    }
    OUT[label][&(BB->back())].first = dataflowvalue;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setBackwardIn(int label, llvm::BasicBlock *BB, const B &dataflowvalue) {
    if (SLIM) {
        SLIM_IN[label][optIR->inst_id_to_object[optIR->getFirstIns(BB->getParent(), BB)]].second = dataflowvalue;
        return;
    }
    IN[label][&(*(BB->begin()))].second = dataflowvalue;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setBackwardOut(int label, llvm::BasicBlock *BB, const B &dataflowvalue) {
    if (SLIM) {
        SLIM_OUT[label][optIR->inst_id_to_object[optIR->getLastIns(BB->getParent(), BB)]].second = dataflowvalue;
        return;
    }
    OUT[label][&(BB->back())].second = dataflowvalue;
}

//=========================================================================================

//=====================setter and getters for context objects==============================

template <class F, class B>
F AnalysisGenKill<F, B>::getForwardInflowForThisContext(int context_label) {
    //    return context_label_to_context_object_map[context_label].second.first.first;
    return context_label_to_context_object_map[context_label]->getInflowValue().first;
}

template <class F, class B>
B AnalysisGenKill<F, B>::getBackwardInflowForThisContext(int context_label) {
    //    return context_label_to_context_object_map[context_label].second.first.second;
    return context_label_to_context_object_map[context_label]->getInflowValue().second;
}

template <class F, class B>
F AnalysisGenKill<F, B>::getForwardOutflowForThisContext(int context_label) {
    //    return context_label_to_context_object_map[context_label].second.second.first;
    return context_label_to_context_object_map[context_label]->getOutflowValue().first;
}

template <class F, class B>
B AnalysisGenKill<F, B>::getBackwardOutflowForThisContext(int context_label) {
    //    return context_label_to_context_object_map[context_label].second.second.second;
    return context_label_to_context_object_map[context_label]->getOutflowValue().second;
}

template <class F, class B>
void AnalysisGenKill<F, B>::setForwardInflowForThisContext(int context_label, const F &forward_inflow) {
    //    context_label_to_context_object_map[context_label].second.first.first=forward_inflow;
    context_label_to_context_object_map[context_label]->setForwardInflow(forward_inflow);
}

template <class F, class B>
void AnalysisGenKill<F, B>::setBackwardInflowForThisContext(int context_label, const B &backward_inflow) {
    //    context_label_to_context_object_map[context_label].second.first.second=backward_inflow;
    context_label_to_context_object_map[context_label]->setBackwardInflow(backward_inflow);
}

template <class F, class B>
void AnalysisGenKill<F, B>::setForwardOutflowForThisContext(int context_label, const F &forward_outflow) {
    //    context_label_to_context_object_map[context_label].second.second.first=forward_outflow;
    context_label_to_context_object_map[context_label]->setForwardOutflow(forward_outflow);
}

template <class F, class B>
void AnalysisGenKill<F, B>::setBackwardOutflowForThisContext(int context_label, const B &backward_outflow) {
    //    context_label_to_context_object_map[context_label].second.second.second=backward_outflow;
    context_label_to_context_object_map[context_label]->setBackwardOutflow(backward_outflow);
}

template <class F, class B>
Function *AnalysisGenKill<F, B>::getFunctionAssociatedWithThisContext(int context_label) {
    return context_label_to_context_object_map[context_label]->getFunction();
}

//=========================================================================================

template <class F, class B>
void AnalysisGenKill<F, B>::doAnalysis(
    std::unique_ptr<llvm::Module> &Mod, slim::IR *slimIRObj, int depAnalysisContext) {
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

    llvm::outs() << "Inside doAnalysis with SLIM parameter as " << SLIM << "\n";
    if (SLIM) {
        auto start = high_resolution_clock::now();

        // llvm::outs() << "\nPrinting inst_id_to_object Global Instructions List \n";
        // for(auto ins: optIR->inst_id_to_object)
        //     llvm::outs() << ins.first << ", ";
        // llvm::outs() << "\n";

        auto stop = high_resolution_clock::now();
        this->SLIMTime = duration_cast<milliseconds>(stop - start);

        // optIR->dumpIR();
    }
    auto start = high_resolution_clock::now();
    int i = 0;

    if (modeOfExec == FSFP || modeOfExec == FSSP) {
        for (Function &function : M) {
            if (function.getName() == "main") {
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
                INIT_CONTEXT(
                    fptr, {forward_inflow_bi, backward_inflow_bi}, {forward_outflow_bi, backward_outflow_bi},
                    depAnalysisContext);
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
                INIT_CONTEXT_FIS(&function, flow_insensitive_inflow, flow_insensitive_outflow, depAnalysisContext);

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
        llvm::outs() << "\n==========================Displaying Data flow value obtained for all "
                        "contexts==========================\n";
        if (modeOfExec == FSFP || modeOfExec == FSSP) {
            for (int icontext = 0; icontext <= context_label_counter; icontext++) {

                for (auto &entry : optIR->func_bb_to_inst_id) {
                    llvm::Function *func = entry.first.first;
                    llvm::BasicBlock *basic_block = entry.first.second;

                    for (long long instruction_id : entry.second) {

                        llvm::outs() << "\n\n";
                        BaseInstruction *instruction = optIR->inst_id_to_object[instruction_id];
                        long long instid = instruction->getInstructionId();

                        llvm::outs() << "[Context : " << icontext << "]  Instruction : ";
                        instruction->printInstruction();
                        llvm::outs() << "Forward : \n";
                        llvm::outs() << "IN value : ";
                        printDataFlowValuesForward(getForwardComponentAtInOfThisInstruction(instruction, icontext));
                        llvm::outs() << "\nOUT value : ";
                        printDataFlowValuesForward(getForwardComponentAtOutOfThisInstruction(instruction, icontext));

                        llvm::outs() << "\nBackward : \n";
                        llvm::outs() << "OUT value : ";
                        printDataFlowValuesBackward(getBackwardComponentAtOutOfThisInstruction(instruction, icontext));
                        llvm::outs() << "\nIN value : ";
                        printDataFlowValuesBackward(getBackwardComponentAtInOfThisInstruction(instruction, icontext));
                        llvm::outs() << "\n\n";
                    }
                }
            }
        } else {
            // FISP or FIFP
            // TODO
        }
    }
}

template <class F, class B>
void AnalysisGenKill<F, B>::INIT_CONTEXT(
    llvm::Function *function, const std::pair<F, B> &Inflow, const std::pair<F, B> &Outflow, int depAnalysisContext) {
    // llvm::outs() << "\n Inside INIT_CONTEXT..............";
    context_label_counter++;
    Context<F, B> *context_object = new Context<F, B>(context_label_counter, function, Inflow, Outflow);
    int current_context_label = context_object->getLabel();
    setProcessingContextLabel(current_context_label);
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
            getInitialisationValueForward()); // setting outflow forward
        ProcedureContext.insert(current_context_label);

        for (BasicBlock *BB : post_order(&context_object->getFunction()->getEntryBlock())) {
            BasicBlock &b = *BB;
            forward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), depAnalysisContext));
            if (direction == "bidirectional") {
                backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), depAnalysisContext));
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
            getInitialisationValueBackward()); // setting outflow backward
        ProcedureContext.insert(current_context_label);

        for (BasicBlock *BB : inverse_post_order(&context_object->getFunction()->back())) {
            BasicBlock &b = *BB;
            backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), depAnalysisContext));
            if (direction == "bidirectional") {
                backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), depAnalysisContext));
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
        setBackwardOut(
            current_context_label, &context_object->getFunction()->back(),
            getBackwardInflowForThisContext(current_context_label));
    } else {
        context_label_to_context_object_map[current_context_label] = context_object;

        if (debug) {
            llvm::outs() << "INITIALIZING CONTEXT:-" << "\n";
            llvm::outs() << "LABEL: " << context_object->getLabel() << "\n";
            llvm::outs() << "FUNCTION: " << function->getName() << "\n";
            llvm::outs() << "Inflow Value: ";
            llvm::outs() << "Forward:- ";
            printDataFlowValuesForward(Inflow.first);
            llvm::outs() << "Backward:- ";
            printDataFlowValuesBackward(Inflow.second);
        }
        setForwardOutflowForThisContext(
            current_context_label, getInitialisationValueForward()); // setting outflow forward
        setBackwardOutflowForThisContext(
            current_context_label, getInitialisationValueBackward()); // setting outflow backward
        ProcedureContext.insert(current_context_label);
        for (BasicBlock *BB : inverse_post_order(&context_object->getFunction()->back())) {
            // populate backward worklist
            BasicBlock &b = *BB;
#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Inserting into Backward Worklist in INIT_CONTEXT:" << current_context_label << "\n";
            for (Instruction &I : b)
                llvm::outs() << I << "\n";
#endif

            backward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), depAnalysisContext));
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
        for (BasicBlock *BB : post_order(&context_object->getFunction()->getEntryBlock())) {
            // populate forward worklist
            BasicBlock &b = *BB;

#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Inserting into Forward Worklist in INIT_CONTEXT:" << current_context_label << "\n";
            for (Instruction &I : b)
                llvm::outs() << I << "\n";
#endif

            forward_worklist.workInsert(make_pair(make_pair(current_context_label, &b), depAnalysisContext));
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
                current_context_label, getBoundaryInformationForward()); // setting inflow forward
            setBackwardInflowForThisContext(
                current_context_label, getBoundaryInformationBackward()); // setting inflow backward
        } else {
            setForwardInflowForThisContext(
                current_context_label, context_object->getInflowValue().first); // setting inflow forward
            setBackwardInflowForThisContext(
                current_context_label, context_object->getInflowValue().second); // setting inflow backward
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
int AnalysisGenKill<F, B>::check_if_context_already_exists(
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
                        llvm::outs()
                            << "======================================================================================"
                            << "\n";
                        llvm::outs() << "Context found!!!!!" << "\n";
                        llvm::outs() << "LABEL: " << set_itr << "\n";
                        llvm::outs()
                            << "======================================================================================"
                            << "\n";
                    }
                    return set_itr;
                    process_mem_usage(this->vm, this->rss);
                    this->total_memory = max(this->total_memory, this->vm);
                }
            }
        } else {
            /*llvm::outs() << "Printing all Context for function " << function->getName();
            for (auto set_itr:ProcedureContext) {
                Context<F, B> *current_object = context_label_to_context_object_map[set_itr];
                F new_context_values = Inflow.first;
                F current_context_values = current_object->getInflowValue().first;

                if (function->getName() == current_object->getFunction()->getName()){
                    llvm::outs() << "\ncurrent_context_values \n label " << set_itr << " Inflow - ";
                    printDataFlowValuesFIS(current_context_values);
                    llvm::outs() << "\n";
                }

            }*/

            for (auto set_itr : ProcedureContext) {
                Context<F, B> *current_object = context_label_to_context_object_map[set_itr];
                F new_context_values = Inflow.first;
                F current_context_values = current_object->getInflowValue().first;

                if (function->getName() == current_object->getFunction()->getName() &&
                    EqualDataFlowValuesFIS(new_context_values, current_context_values)) {
                    if (debug) {
                        llvm::outs()
                            << "======================================================================================"
                            << "\n";
                        llvm::outs() << "Context found!!!!!" << "\n";
                        llvm::outs() << "LABEL: " << set_itr << "\n";
                        llvm::outs()
                            << "======================================================================================"
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
                    llvm::outs()
                        << "======================================================================================"
                        << "\n";
                    llvm::outs() << "Context found!!!!!" << "\n";
                    llvm::outs() << "LABEL: " << set_itr << "\n";
                    llvm::outs()
                        << "======================================================================================"
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
            //           if(function->getName() == current_object->getFunction()->getName() &&
            //         EqualDataFlowValuesForward(new_context_values_forward, current_context_values_forward) &&
            //       EqualDataFlowValuesBackward(new_context_values_backward, current_context_values_backward)) {
            // Changes on 16.8.22
            if (function->getName() == current_object->getFunction()->getName() &&
                EqualContextValuesForward(new_context_values_forward, current_context_values_forward) &&
                EqualContextValuesBackward(new_context_values_backward, current_context_values_backward)) {
                if (debug) {
                    llvm::outs()
                        << "\n======================================================================================"
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
                    llvm::outs()
                        << "\n======================================================================================"
                        << "\n";
                }
                process_mem_usage(this->vm, this->rss);
                this->total_memory = max(this->total_memory, this->vm);
                return set_itr;
            }
        }
    }
    if (debug) {
        llvm::outs() << "\n======================================================================================"
                     << "\n";
        llvm::outs() << "Context NOT found!!!!!" << "\n";
        llvm::outs() << "Forward Inflow Value:- ";
        printDataFlowValuesForward(Inflow.first);
        llvm::outs() << "Backward Inflow Value:- ";
        printDataFlowValuesBackward(Inflow.second);
        llvm::outs() << "\n======================================================================================"
                     << "\n";
    }
    return 0;
}

template <class F, class B>
void AnalysisGenKill<F, B>::doAnalysisForward() {
    while (not forward_worklist.empty()) // step 2
    {
        // step 3 and 4
        pair<pair<int, BasicBlock *>, int> worklistElement = forward_worklist.workDelete();
        pair<int, BasicBlock *> current_pair = worklistElement.first;
        int depAnalysisContext = worklistElement.second;
        int current_context_label = current_pair.first;
        BasicBlock *bb = current_pair.second;
        setProcessingContextLabel(current_context_label);

#ifdef PRVASCO
        llvm::outs() << "\n";
        llvm::outs() << "Popping from Forward Worklist (Context:" << current_context_label << ")\n";
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
        if (bb != (&function.getEntryBlock())) {
            // step 6
            setForwardIn(current_pair.first, current_pair.second, getInitialisationValueForward());

            // step 7 and 8
            for (auto pred_bb : predecessors(bb)) {
                llvm::outs() << "\n";
                // get first instruction of bb and setCurrentInstruction(inst);
                BaseInstruction *firstInst = optIR->inst_id_to_object[optIR->getFirstIns(bb->getParent(), bb)];
                setCurrentInstruction(firstInst);

                printDataFlowValuesForward(getIn(current_pair.first, current_pair.second).first);

                setForwardIn(
                    current_pair.first, current_pair.second,
                    performMeetForward(
                        getIn(current_pair.first, current_pair.second).first, getOut(current_pair.first, pred_bb).first,
                        "IN")); // CS_BB_OUT[make_pair(current_pair.first,pred_bb)].first
            }
        } else {
            // In value of this node is same as INFLOW value
            setForwardIn(
                current_pair.first, current_pair.second, getForwardInflowForThisContext(current_context_label));
        }

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

        bool contains_a_method_call = false;
        if (SLIM) {
            for (auto &index : optIR->func_bb_to_inst_id[{f, bb}]) {
                auto &inst = optIR->inst_id_to_object[index];
                if (inst->getCall()) { // getCall() present in new SLIM
                    Instruction *Inst = inst->getLLVMInstruction();
                    /*CallInst *ci = dyn_cast<CallInst>(Inst);
                    Function *target_function = ci->getCalledFunction();*/
                    // Changed to fetch target (Callee) function from SLIM IR instead of LLVM IR
                    CallInstruction *cTempInst = new CallInstruction(Inst);
                    if (cTempInst->isIndirectCall())
                        continue;
                    Function *target_function = cTempInst->getCalleeFunction();
                    if (target_function && (target_function->isDeclaration() || isAnIgnorableDebugInstruction(Inst))) {
                        continue; // this is an inbuilt function so doesn't need to be processed.
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
                        continue; // this is an inbuilt function so doesn't need to be processed.
                    }
                    contains_a_method_call = true;
                    break;
                }
            }
        }
        if (contains_a_method_call) {
            // step 11
            if (SLIM) {
                F prev = getForwardComponentAtInOfThisInstruction(
                    optIR->inst_id_to_object[optIR->func_bb_to_inst_id[{f, bb}].front()]);
                d1 = getBackwardComponentAtOutOfThisInstruction(
                    optIR->inst_id_to_object[optIR->func_bb_to_inst_id[{f, bb}].back()]);
                for (auto &index : optIR->func_bb_to_inst_id[{f, bb}]) {
                    auto &inst = optIR->inst_id_to_object[index];
                    setCurrentInstruction(inst);
                    d1 = getBackwardComponentAtOutOfThisInstruction(inst);
                    // if(inst->getInsFunPtr()) {
                    // llvm::outs() << "\nCall Instruction at INDEX = " << index << " in Forward Analysis is Function
                    // Pointer and Pointees are : "; std::vector<llvm::Function *> target_functions =
                    // getIndirectCalleeFromIN(index, prev); llvm::outs() << "\nSize of vector is: " <<
                    // target_functions.size() << " "; for(auto& function : target_functions) {
                    //     llvm::outs() << function->hasName() << "   ";
                    //     llvm::outs() << function->getName() << "   ";
                    // }
                    // llvm::outs() << "Prev DFV is: ";
                    // printDataFlowValuesBackward(getBackwardComponentAtInOfThisInstruction(inst));
                    // llvm::outs() << "\n";
                    // setForwardComponentAtOutOfThisInstruction(&inst,prev);
                    // for(auto& target_function : target_functions) {
                    //     if(target_function->isDeclaration()) {
                    //         continue;
                    //     }
                    //     Instruction* Inst = getInstforIndx(index);
                    //     F PinStartCallee = getPinStartCallee(index, Inst, prev, target_function);
                    //     PinStartCallee = performMeetForward(PinStartCallee,prev);
                    //     pair<F, B> inflow_pair = CallInflowFunction(current_context_label, target_function, bb,
                    //     PinStartCallee, d1); F a2 = inflow_pair.first; B d2 = inflow_pair.second; F
                    //     new_outflow_forward; B new_outflow_backward;
                    //     // setForwardComponentAtInOfThisInstruction(&inst, prev);
                    //     int matching_context_label = 0;
                    //     matching_context_label = check_if_context_already_exists(target_function, inflow_pair,
                    //                                                             make_pair(new_outflow_forward,
                    //                                                                     new_outflow_backward));
                    //     if (matching_context_label > 0) {
                    //         if (debug) {
                    //             printLine(current_context_label);
                    //             llvm::outs() << "\nProcessing instruction at INDEX = " << index << "\n";
                    //             llvm::outs() << "IN: ";
                    //         }
                    //         //step 14
                    //         pair<int, fetchLR *> mypair = make_pair(current_context_label, &inst);
                    //         SLIM_context_transition_graph[mypair] = matching_context_label;
                    //         if (debug) {
                    //             printDataFlowValuesForward(prev);
                    //         }

                    //         //step 16 and 17
                    //         F a3 = getForwardOutflowForThisContext(matching_context_label);
                    //         B d3 = getBackwardOutflowForThisContext(matching_context_label);

                    //         pair<F, B> outflow_pair = CallOutflowFunction(current_context_label, target_function, bb,
                    //         a3,
                    //                                                   d3, prev, d1);
                    //         F value_to_be_meet_with_prev_out = outflow_pair.first;
                    //         B d4 = outflow_pair.second;

                    //         //step 18 and 19

                    //         /*
                    //         At the call instruction, the value at IN should be splitted into two components.
                    //         The purely global component is given to the callee while the mixed component is
                    //         propagated to OUT of this instruction after executing computeOUTfromIN() on it.
                    //         */

                    //         F a5 = getPurelyLocalComponentForward(prev);
                    //         /*
                    //         At the OUT of this instruction, the value from END of callee procedure is to be merged
                    //         with the local(mixed) value propagated from IN. Note that this merging "isn't"
                    //         exactly (necessarily) the meet between these two values.
                    //         */

                    //         /*
                    //         As explained in ip-vasco,pdf, we need to perform meet with the original value of OUT
                    //         of this instruction to avoid the oscillation problem.
                    //         */
                    //         setForwardComponentAtOutOfThisInstruction(&inst, performMeetForward(
                    //             performMeetForward(value_to_be_meet_with_prev_out,
                    //                                 getForwardComponentAtOutOfThisInstruction(inst)), a5));
                    //         // prev = getForwardComponentAtOutOfThisInstruction((inst));
                    //         if (debug) {
                    //             llvm::outs() << "OUT: ";
                    //             printDataFlowValuesForward(prev);
                    //             printLine(current_context_label);
                    //         }
                    //     } else {
                    //         //creating a new context
                    //         INIT_CONTEXT(target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward});
                    //         //step 21

                    //         //step 14
                    //         //This step must be done after above step because context label counter gets updated
                    //         after INIT-Context is invoked. pair<int, fetchLR *> mypair =
                    //         make_pair(current_context_label, &inst); SLIM_context_transition_graph[mypair] =
                    //         getContextLabelCounter();
                    //     }
                    // }
                    // prev = getForwardComponentAtOutOfThisInstruction(inst);
                    //   continue;
                    //} else
                    if (inst->getCall()) {

                        Instruction *Inst = inst->getLLVMInstruction();
                        /*CallInst *ci = dyn_cast<CallInst>(Inst);
                        Function *target_function = ci->getCalledFunction(); */
                        // Changed to fetch target (Callee) function from SLIM IR instead of LLVM IR
                        CallInstruction *cTempInst = new CallInstruction(Inst);
                        if (cTempInst->isIndirectCall())
                            continue;
                        Function *target_function = cTempInst->getCalleeFunction();
                        if (not target_function || target_function->isDeclaration() ||
                            isAnIgnorableDebugInstruction(Inst)) {
                            continue; // this is an inbuilt function so doesn't need to be processed.
                        }

                        if (intraprocFlag) {
                            /*  If intraprocFlag is true, then target function is not analysed,
                                instead the data flow value is taken from user based on how the effect is defined
                            */

                            setForwardComponentAtInOfThisInstruction(inst, prev); // compute IN from previous OUT-value

                            F outOfFunction = functionCallEffectForward(target_function, prev);

                            if (debug) {
                                printLine(current_context_label, 0);
                                llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                                inst->printInstruction();
                                llvm::outs() << "\nIN: ";
                                printDataFlowValuesForward(prev);
                            }

                            F a5 = getPurelyLocalComponentForward(prev);

                            // setForwardComponentAtOutOfThisInstruction(inst, performMeetForward(
                            //         performMeetForward(outOfFunction,
                            //         getForwardComponentAtOutOfThisInstruction(inst), "OUT"),
                            //                             a5, "OUT"));
                            // Merging value from END of callee with value propagated from IN isn't necessarily meet
                            // Thus, adding new function performMergeAtCallForward()
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
                            /*
                            At the call instruction, the value at IN should be splitted into two components:
                            1) Purely Global and 2) Mixed.
                            The purely global component is given to the start of callee.
                            */

                            /*
                            Compute Generative and Kill information to perform bypassing
                            Compute Inflow based on this information
                            Compute Onflow, this is merged with Outflow
                            */
                            // #########
                            pair<F, B> inflow_pair;
                            if (byPass) {
                                bool isGenKill = isGenerativeKillInitialised(current_context_label, inst);
                                if (!isGenKill) {
                                    // initialise Generative Kill for this instruction, context
                                    setInitialisationValueGenerativeKill(current_context_label, inst);
                                }

                                int calleeContext = getCalleeContext(current_context_label, inst);
                                llvm::outs() << "\n--------- BEGIN COMPUTE GENERATIVE ---------\n";
                                pair<F, B> generativeAtCall = computeGenerative(calleeContext, target_function);
                                setGenerativeAtCall(current_context_label, inst, generativeAtCall);
                                llvm::outs() << "\n--------- END COMPUTE GENERATIVE ---------\n";

                                // pair<pair<F,B>, pair<F,B>> generativeKill =
                                // getGenerativeKillAtCall(current_context_label, inst);

                                inflow_pair = CallInflowFunction(
                                    current_context_label, target_function, bb, prev, d1,
                                    generativeAtCall); // TODO should we use getter function for generative?
                            } else {
                                inflow_pair = CallInflowFunction(current_context_label, target_function, bb, prev, d1);
                            }

                            // step 12
                            // pair<F, B> inflow_pair = CallInflowFunction(current_context_label, target_function, bb,
                            // prev, d1); pair<F, B> inflow_pair = CallInflowFunction(current_context_label,
                            // target_function, bb, prev, d1, generativeKill.first);
                            F a2 = inflow_pair.first;
                            B d2 = inflow_pair.second;

                            F new_outflow_forward;
                            B new_outflow_backward;

                            // step 13

                            setForwardComponentAtInOfThisInstruction(inst, prev); // compute IN from previous OUT-value
                            int matching_context_label = 0;
                            matching_context_label = check_if_context_already_exists(
                                target_function, inflow_pair, make_pair(new_outflow_forward, new_outflow_backward));

                            if (matching_context_label > 0) {
                                if (debug) {
                                    printLine(current_context_label, 0);
                                    llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                                    inst->printInstruction();
                                    llvm::outs() << "\nIN: ";
                                }
                                // step 14
                                pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                SLIM_context_transition_graph[mypair] = matching_context_label;
                                if (debug) {
                                    printDataFlowValuesForward(prev);
                                }

                                // step 16 and 17
                                F a3 = getForwardOutflowForThisContext(matching_context_label);
                                B d3 = getBackwardOutflowForThisContext(matching_context_label);

                                pair<F, B> outflow_pair =
                                    CallOutflowFunction(current_context_label, target_function, bb, a3, d3, prev, d1);
                                F value_to_be_meet_with_prev_out = outflow_pair.first;
                                B d4 = outflow_pair.second;

                                // step 18 and 19

                                /*
                                At the call instruction, the value at IN should be splitted into two components.
                                The purely global component is given to the callee while the mixed component is
                                propagated to OUT of this instruction after executing computeOUTfromIN() on it.
                                */

                                // #########
                                F a5;
                                if (byPass) {
                                    llvm::outs() << "\n--------- BEGIN COMPUTE KILL ---------\n";
                                    int calleeContext = getCalleeContext(current_context_label, inst);
                                    pair<F, B> killAtCall = computeKill(calleeContext, target_function);
                                    setKillAtCall(current_context_label, inst, killAtCall);
                                    llvm::outs() << "\n--------- END COMPUTE KILL ---------\n";
                                    pair<F, B> onflow_pair =
                                        CallOnflowFunction(current_context_label, inst, killAtCall);
                                    a5 = onflow_pair.first;
                                } else {
                                    a5 = getPurelyLocalComponentForward(prev);
                                }

                                /*
                                At the OUT of this instruction, the value from END of callee procedure is to be merged
                                with the local(mixed) value propagated from IN. Note that this merging "isn't"
                                exactly (necessarily) the meet between these two values.
                                */

                                /*
                                As explained in ip-vasco,pdf, we need to perform meet with the original value of OUT
                                of this instruction to avoid the oscillation problem.
                                */
                                // setForwardComponentAtOutOfThisInstruction(&inst,
                                // performMeetForward(value_to_be_meet_with_prev_out,
                                //       getForwardComponentAtOutOfThisInstruction(inst)));

                                // Merging value from END of callee with value propagated from IN isn't necessarily meet
                                // Thus, adding new function performMergeAtCallForward()

                                setForwardComponentAtOutOfThisInstruction(
                                    inst, performMeetForward(
                                              getForwardComponentAtOutOfThisInstruction(inst),
                                              performMergeAtCallForward(value_to_be_meet_with_prev_out, a5), "OUT"));

                                // setForwardComponentAtOutOfThisInstruction(inst, performMeetForward(
                                //     performMeetForward(value_to_be_meet_with_prev_out,
                                //                         getForwardComponentAtOutOfThisInstruction(inst), "OUT"), a5,
                                //                         "OUT"));
                                prev = getForwardComponentAtOutOfThisInstruction((inst));
                                if (debug) {
                                    llvm::outs() << "\nOUT: ";
                                    printDataFlowValuesForward(prev);
                                    printLine(current_context_label, 1);
                                }

                            } else {
// For analysing caller again, insert caller into worklist here itself (since at a later point,
//  we are not able to get the dependent analysis context of the caller from the end of callee analysis)
#ifdef PRVASCO
                                llvm::outs() << "\nInserting caller node into Forward Worklist at end of callee in "
                                                "Forward Analysis (Context:"
                                             << current_context_label << " to " << worklistElement.first.first << ")\n";
                                for (Instruction &I : *bb)
                                    llvm::outs() << I << "\n";
#endif
                                forward_worklist.workInsert(worklistElement);
                                if (direction == "bidirectional") {
#ifdef PRVASCO
                                    llvm::outs()
                                        << "\nInserting caller node into Backward Worklist at end of callee in Forward "
                                           "Analysis (Context:"
                                        << current_context_label << " to " << worklistElement.first.first << ")\n";
                                    for (Instruction &I : *bb)
                                        llvm::outs() << I << "\n";
#endif
                                    backward_worklist.workInsert(worklistElement);
                                }

                                // creating a new context
                                int depAnalysisCalleeContext = getDepAnalysisCalleeContext(depAnalysisContext, inst);
                                INIT_CONTEXT(
                                    target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward},
                                    depAnalysisCalleeContext); // step 21

                                // step 14
                                // This step must be done after above step because context label counter gets updated
                                // after INIT-Context is invoked.
                                pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                SLIM_context_transition_graph[mypair] = getContextLabelCounter();

#ifdef PRVASCO
                                llvm::outs() << "\n";
                                llvm::outs()
                                    << "After New Context Creation, IN value at the node in Forward Analysis (Context:"
                                    << current_pair.first << ") ";
                                printDataFlowValuesForward(getIn(current_pair.first, current_pair.second).first);
                                llvm::outs() << "\nAfter New Context Creation, OUT value at the node in Forward "
                                                "Analysis (Context: "
                                             << current_pair.first << ") ";
                                printDataFlowValuesForward(getOut(current_pair.first, current_pair.second).first);
                                llvm::outs() << "\n";
#endif
                                // context_label_to_context_object_map[getContextLabelCounter()]->setParent(mypair);
                                // llvm::errs() << "\n" << SLIM_context_transition_graph.size();
                                // MSCHANGE 3 --commented return
                                // return;
                                // Reverse_SLIM_context_transition_graph[getContextLabelCounter()] = mypair;
                            }
                        }
                    } else {
                        if (debug) {
                            printLine(current_context_label, 0);
                            llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                            inst->printInstruction();
                            llvm::outs() << "\nIN: ";
                            printDataFlowValuesForward(prev);
                        }
                        setForwardComponentAtInOfThisInstruction(inst, prev); // compute IN from previous OUT-value
                        F new_prev;
                        if (isIgnorableInstruction(inst)) {
                            new_prev = prev;
                        } else {
                            new_prev = computeOutFromIn(inst, depAnalysisContext);
                        }
                        // F new_prev = computeOutFromIn(inst, depAnalysisContext);
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
                    // setForwardOut(current_pair.first, current_pair.second, prev);
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
                            continue; // this is an inbuilt function so doesn't need to be processed.
                        }

                        /*
                        At the call instruction, the value at IN should be splitted into two components:
                        1) Purely Global and 2) Mixed.
                        The purely global component is given to the start of callee.
                        */

                        // step 12
                        pair<F, B> inflow_pair = CallInflowFunction(current_context_label, target_function, bb, a1, d1);
                        F a2 = inflow_pair.first;
                        B d2 = inflow_pair.second;

                        F new_outflow_forward;
                        B new_outflow_backward;

                        // step 13

                        setForwardComponentAtInOfThisInstruction(&(*inst), prev); // compute IN from previous OUT-value
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

                            pair<F, B> outflow_pair =
                                CallOutflowFunction(current_context_label, target_function, bb, a3, d3, a1, d1);
                            F value_to_be_meet_with_prev_out = outflow_pair.first;
                            B d4 = outflow_pair.second;

                            // step 18 and 19

                            /*
                            At the call instruction, the value at IN should be splitted into two components.
                            The purely global component is given to the callee while the mixed component is propagated
                            to OUT of this instruction after executing computeOUTfromIN() on it.
                            */

                            F a5 = getPurelyLocalComponentForward(a1);
                            /*
                            At the OUT of this instruction, the value from END of callee procedure is to be merged
                            with the local(mixed) value propagated from IN. Note that this merging "isn't"
                            exactly (necessarily) the meet between these two values.
                            */

                            /*
                            As explained in ip-vasco,pdf, we need to perform meet with the original value of OUT
                            of this instruction to avoid the oscillation problem.
                            */

                            // Merging value from END of callee with value propagated from IN isn't necessarily meet
                            // Thus, adding new function performMergeAtCallForward()
                            setForwardComponentAtOutOfThisInstruction(
                                &(*inst), performMeetForward(
                                              getForwardComponentAtOutOfThisInstruction(*inst),
                                              performMergeAtCallForward(value_to_be_meet_with_prev_out, a5), "OUT"));

                            // setForwardComponentAtOutOfThisInstruction(&(*inst), performMeetForward(
                            //         performMeetForward(value_to_be_meet_with_prev_out,
                            //                            getForwardComponentAtOutOfThisInstruction(*inst), "OUT"), a5,
                            //                            "OUT"));
                            prev = getForwardComponentAtOutOfThisInstruction((*inst));
                            if (debug) {
                                llvm::outs() << "\nOUT: ";
                                printDataFlowValuesForward(prev);
                                printLine(current_context_label, 1);
                            }
                        } else // step 20
                        {
// For analysing caller again, insert caller into worklist here itself (since at a later point,
//  we are not able to get the dependent analysis context of the caller from the end of callee analysis)
#ifdef PRVASCO
                            llvm::outs() << "\nInserting caller node into Forward Worklist at end of callee in Forward "
                                            "Analysis (Context:"
                                         << current_context_label << " to " << worklistElement.first.first << ")\n";
                            for (Instruction &I : *bb)
                                llvm::outs() << I << "\n";
#endif
                            forward_worklist.workInsert(worklistElement);
                            if (direction == "bidirectional") {
#ifdef PRVASCO
                                llvm::outs() << "\nInserting caller node into Backward Worklist at end of callee in "
                                                "Forward Analysis (Context:"
                                             << current_context_label << " to " << worklistElement.first.first << ")\n";
                                for (Instruction &I : *bb)
                                    llvm::outs() << I << "\n";
#endif
                                backward_worklist.workInsert(worklistElement);
                            }

                            // creating a new context
                            int depAnalysisCalleeContext = getDepAnalysisCalleeContext(depAnalysisContext, I);
                            INIT_CONTEXT(
                                target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward},
                                depAnalysisCalleeContext); // step 21

                            // step 14
                            // This step must be done after above step because context label counter gets updated after
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
                        setForwardComponentAtInOfThisInstruction(&(*inst), prev); // compute IN from previous OUT-value
                        F new_prev = computeOutFromIn(*inst, depAnalysisContext);
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
            setForwardOut(
                current_pair.first, current_pair.second, NormalFlowFunctionForward(current_pair, depAnalysisContext));
            // Commented as NormalFlowFunction is printing the dataflow values
            // printDataFlowValuesForward(NormalFlowFunctionForward(current_pair));
        }
        bool changed = false;
        // MSCHANGE 7
        // if (not EqualDataFlowValuesForward(previous_value_at_out_of_this_node, getOut(current_pair.first,
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

        if (changed && modeOfExec == FSFP) // step 24
        {
            // not yet converged
            for (auto succ_bb : successors(bb)) // step 25
            {
#ifdef PRVASCO
                llvm::outs() << "\n";
                llvm::outs()
                    << "Inserting successors into Forward Worklist on change in OUT value in Forward Analysis (Context:"
                    << current_context_label << ")\n";
                for (Instruction &I : *succ_bb)
                    llvm::outs() << I << "\n";
#endif
                // step 26
                forward_worklist.workInsert(make_pair(make_pair(current_context_label, succ_bb), depAnalysisContext));
            }
        }
        bool changed1 = false;
        // MSCHANGE 8
        // if (not EqualDataFlowValuesForward(previous_value_at_in_of_this_node, getIn(current_pair.first,
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

        if (changed1 && modeOfExec == FSFP) { // Step 24

#ifdef PRVASCO
            llvm::outs() << "\n";
            llvm::outs() << "Inserting node into Backward Worklist on change in IN value in Forward Analysis (Context:"
                         << current_context_label << ")\n";
            for (Instruction &I : *bb)
                llvm::outs() << I << "\n";
#endif
            backward_worklist.workInsert(make_pair(make_pair(current_context_label, bb), depAnalysisContext));
        }

        if (bb == &function.back()) // step 27
        {
            // last node
            // step 28
            setForwardOutflowForThisContext(
                current_context_label, getPurelyGlobalComponentForward(getOut(
                                                                           current_pair.first,
                                                                           current_pair.second)
                                                                           .first)); // setting forward outflow
            bool flag = false;
            /*if(SLIM) {
                // auto Parent = context_label_to_context_object_map[current_context_label]->getParent();
                // if(Parent.first != -1 && Parent.second != nullptr) {
                //     BasicBlock *bb = Parent.second->getBB();
                //     pair<int, BasicBlock *> context_bb_pair = make_pair(Parent.first, bb);
                //     llvm::outs() << "\n Inserting into forward worklist......";
                //     forward_worklist.workInsert(context_bb_pair);
                //     if (direction == "bidirectional") { llvm::outs() << "\n Inserting into backwards worklist...";
                //         backward_worklist.workInsert(context_bb_pair);
                //     }
                // }
                // auto result = Reverse_SLIM_context_transition_graph.find(current_context_label);
                // if(result != Reverse_SLIM_context_transition_graph.end()) {
                //     BasicBlock *bb = result->second.second->getBB();
                //     pair<int, BasicBlock *> context_bb_pair = make_pair(result->second.first, bb);
                //     forward_worklist.workInsert(context_bb_pair);
                //     if (direction == "bidirectional") {
                //         llvm::outs() << "\n Inserting into backwards worklist...";
                //         backward_worklist.workInsert(context_bb_pair);
                //     }
                //     SLIM_context_transition_graph.erase(result->second);
                //     Reverse_SLIM_context_transition_graph.erase(result);
                // }

               for(auto context_inst_pairs : SLIM_context_transition_graph) {
                   if (context_inst_pairs.second == current_context_label) { //Matching called function
                       //step 30
                       BasicBlock *bb = context_inst_pairs.first.second->getBasicBlock();
                       pair<int, BasicBlock *> context_bb_pair = make_pair(context_inst_pairs.first.first, bb);

                       #ifdef PRVASCO
                            llvm::outs() << "\nInserting caller node into Forward Worklist at end of callee in Forward
            Analysis (Context:" << current_context_label << " to " << context_inst_pairs.first.first << ")\n"; for
            (Instruction &I : *bb) llvm::outs() << I << "\n"; #endif forward_worklist.workInsert(context_bb_pair); if
            (direction == "bidirectional") { #ifdef PRVASCO llvm::outs() << "\nInserting caller node into Backward
            Worklist at end of callee in Forward Analysis (Context:" << current_context_label << " to " <<
            context_inst_pairs.first.first << ")\n"; for (Instruction &I : *bb) llvm::outs() << I << "\n"; #endif
                           backward_worklist.workInsert(context_bb_pair);
                       }
                       break;
                   }
               }
            } else {
                for (auto context_inst_pairs : context_transition_graph) {
                    if (context_inst_pairs.second == current_context_label) {
                        //step 30
                        BasicBlock *bb = context_inst_pairs.first.second->getParent();
                        pair<int, BasicBlock *> context_bb_pair = make_pair(context_inst_pairs.first.first, bb);
                        forward_worklist.workInsert(context_bb_pair);
                        if (direction == "bidirectional") {
                            backward_worklist.workInsert(context_bb_pair);
                        }
                        break;
                    }
                }
            }*/
        }
        process_mem_usage(this->vm, this->rss);
        this->total_memory = max(this->total_memory, this->vm);
    }
}

template <class F, class B>
F AnalysisGenKill<F, B>::NormalFlowFunctionForward(
    pair<int, BasicBlock *> current_pair_of_context_label_and_bb, int depAnalysisContext) {
    BasicBlock &b = *(current_pair_of_context_label_and_bb.second);
    F prev = getIn(current_pair_of_context_label_and_bb.first, current_pair_of_context_label_and_bb.second).first;
    Context<F, B> *context_object = context_label_to_context_object_map[current_pair_of_context_label_and_bb.first];
    bool changed = false;
    // traverse a basic block in forward direction
    if (SLIM) {
        for (auto &index : optIR->func_bb_to_inst_id[{context_object->getFunction(), &b}]) {
            auto &inst = optIR->inst_id_to_object[index];

            F old_IN = getForwardComponentAtInOfThisInstruction(inst);
            // if change in data flow value at IN of any instruction, add this basic block to backward worklist
            if (!EqualDataFlowValuesForward(prev, old_IN))
                changed = true;

            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 0);
                llvm::outs() << "\nProcessing instruction at INDEX = " << index << " " << inst;
                inst->printInstruction();
                llvm::outs() << "\nIN: ";
                printDataFlowValuesForward(prev);
            }
            setForwardComponentAtInOfThisInstruction(inst, prev);

            F new_prev;
            if (isIgnorableInstruction(inst)) {
                new_prev = prev;
            } else {
                new_prev = computeOutFromIn(inst, depAnalysisContext);
            }
            // F new_prev = computeOutFromIn(inst, depAnalysisContext);
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
        if (changed && modeOfExec == FSFP) {
#ifdef PRVASCO
            llvm::outs() << "\nInserting node into Backward Worklist on change in IN value within BB in Forward "
                            "Analysis (Context:"
                         << current_pair_of_context_label_and_bb.first << ")\n";
            for (Instruction &I : b)
                llvm::outs() << I << "\n";
#endif
            backward_worklist.workInsert(make_pair(current_pair_of_context_label_and_bb, depAnalysisContext));
        }
    } else {
        for (auto inst = &*(b.begin()); inst != nullptr; inst = inst->getNextNonDebugInstruction()) {
            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 0);
                llvm::outs() << *inst << "\n";
                llvm::outs() << "IN: ";
                printDataFlowValuesForward(prev);
            }
            setForwardComponentAtInOfThisInstruction(&(*inst), prev); // compute IN from previous OUT-value
            F new_prev = computeOutFromIn(*inst, depAnalysisContext);
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
void AnalysisGenKill<F, B>::doAnalysisBackward() {
    while (not backward_worklist.empty()) // step 2
    {
        // step 3 and 4
        pair<pair<int, BasicBlock *>, int> worklistElement = backward_worklist.workDelete();
        pair<int, BasicBlock *> current_pair = worklistElement.first;
        int depAnalysisContext = worklistElement.second;
        int current_context_label;
        BasicBlock *bb;
        current_context_label = current_pair.first;
        setProcessingContextLabel(current_context_label);
        bb = current_pair.second;

#ifdef PRVASCO
        llvm::outs() << "\nPopping from Backward Worklist (Context:" << current_context_label << ")\n";
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
        if (bb != (&function.back())) {
            // step 6
            setBackwardOut(current_pair.first, current_pair.second, getInitialisationValueBackward());
            // step 7 and 8
            for (auto succ_bb : successors(bb)) {
                setBackwardOut(
                    current_pair.first, current_pair.second,
                    performMeetBackward(
                        getOut(current_pair.first, current_pair.second).second,
                        getIn(current_pair.first, succ_bb).second));
            }
        } else {
            // Out value of this node is same as INFLOW value
            setBackwardOut(
                current_pair.first, current_pair.second, getBackwardInflowForThisContext(current_context_label));
        }

        // step 9
        F a1 = getIn(current_pair.first, current_pair.second).first;   // CS_BB_IN[current_pair].first;
        B d1 = getOut(current_pair.first, current_pair.second).second; // CS_BB_OUT[current_pair].second;

        // Moved up above step 5
        /*B previous_value_at_in_of_this_node = getIn(current_pair.first,
                                                    current_pair.second).second; //CS_BB_IN[current_pair].second;
        B previous_value_at_out_of_this_node = getOut(current_pair.first, current_pair.second).second; //prev Lout
        */

#ifdef PRVASCO
        llvm::outs() << "\nPrevious IN value at the node in Backward Analysis";
        printDataFlowValuesBackward(previous_value_at_in_of_this_node);
        llvm::outs() << "\nPrevious OUT value at the node in Backward Analysis";
        printDataFlowValuesBackward(previous_value_at_out_of_this_node);
        llvm::outs() << "\n";
#endif

        // step 10
        bool contains_a_method_call = false;
        if (SLIM) {
            for (auto &index : optIR->func_bb_to_inst_id[{f, bb}]) {
                auto &inst = optIR->inst_id_to_object[index];
                if (inst->getCall()) {
                    Instruction *Inst = inst->getLLVMInstruction();
                    /*CallInst *ci = dyn_cast<CallInst>(Inst);
                    Function *target_function = ci->getCalledFunction();*/
                    // Changed to fetch target (Callee) function from SLIM IR instead of LLVM IR
                    CallInstruction *cTempInst = new CallInstruction(Inst);
                    if (cTempInst->isIndirectCall())
                        continue;
                    Function *target_function = cTempInst->getCalleeFunction();
                    if (target_function && (target_function->isDeclaration() || isAnIgnorableDebugInstruction(Inst))) {
                        continue; // this is an inbuilt function so doesn't need to be processed.
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
            B prev = getBackwardComponentAtOutOfThisInstruction(
                optIR->inst_id_to_object[optIR->func_bb_to_inst_id[{f, bb}].back()]);
            // step 11
            if (SLIM) {
                for (auto &index : optIR->getReverseInstList(optIR->func_bb_to_inst_id[{f, bb}])) {
                    auto &inst = optIR->inst_id_to_object[index];
                    a1 = getForwardComponentAtInOfThisInstruction(inst);
                    // if(inst->getInsFunPtr()) {
                    // llvm::outs() << "\nCall Instruction at INDEX = " << index << " is Function Pointer and Pointees
                    // are : "; std::vector<llvm::Function *> target_functions = getIndirectCalleeFromIN(index, a1);
                    // for(auto function : target_functions) {
                    //     llvm::outs() << function->getName() << "   ";
                    // }
                    // llvm::outs() << "\n";
                    // Instruction* Inst = getInstforIndx(index);
                    // B FPArgs = getFPandArgsBackward(index, Inst);
                    // if(debug) {
                    //     printLine(current_context_label);
                    //     llvm::outs() << "\nProcessing instruction at INDEX = " << index << "\n";
                    //     llvm::outs() << "OUT: ";
                    //     printDataFlowValuesBackward(prev);
                    // }
                    // setBackwardComponentAtInOfThisInstruction(&inst,FPArgs);
                    // for(auto& target_function : target_functions) {
                    //     if(target_function->isDeclaration()) {
                    //         continue;
                    //     }
                    //     llvm::outs() << "\n CallInflowFunction .............";
                    //     pair<F, B> inflow_pair = CallInflowFunction(current_context_label, target_function, bb, a1,
                    //     prev); F a2 = inflow_pair.first; B d2 = inflow_pair.second; F new_outflow_forward; B
                    //     new_outflow_backward; setBackwardComponentAtOutOfThisInstruction(&inst, prev); int
                    //     matching_context_label = 0; matching_context_label =
                    //     check_if_context_already_exists(target_function, {a2, d2},
                    //                                                             {new_outflow_forward,
                    //                                                             new_outflow_backward});
                    //     if (matching_context_label > 0) { //Step 15
                    //         if (debug) {
                    //             printLine(current_context_label);
                    //             llvm::outs() << "\nProcessing instruction at INDEX = " << index << "\n";
                    //             llvm::outs() << "OUT: ";
                    //             printDataFlowValuesBackward(prev);
                    //         }
                    //         //step 14
                    //         pair<int, fetchLR *> mypair = make_pair(current_context_label, &inst);
                    //         SLIM_context_transition_graph[mypair] = matching_context_label;

                    //         //step 16 and 17
                    //         F a3 = getForwardOutflowForThisContext(matching_context_label);
                    //         B d3 = getBackwardOutflowForThisContext(matching_context_label);

                    //         pair<F, B> outflow_pair = CallOutflowFunction(current_context_label, target_function, bb,
                    //         a3,
                    //                                                     d3, a1, prev);
                    //         F a4 = outflow_pair.first;
                    //         B value_to_be_meet_with_prev_in = outflow_pair.second;
                    //         //step 18 and 19

                    //         /*
                    //         At the call instruction, the value at OUT should be splitted into two components.
                    //         The purely global component is given to the callee while the mixed component is
                    //         propagated to IN of this instruction after executing computeINfromOUT() on it.
                    //         */

                    //         /*
                    //         At the IN of this instruction, the value from START of callee procedure is to be merged
                    //         with the local(mixed) value propagated from OUT. Note that this merging "isn't"
                    //         exactly (necessarily) the meet between these two values.
                    //         */

                    //         /*
                    //         As explained in ip-vasco,pdf, we need to perform meet with the original value of IN
                    //         of this instruction to avoid the oscillation problem.
                    //         */
                    //         setBackwardComponentAtInOfThisInstruction(&inst,
                    //                                                 performMeetBackward(value_to_be_meet_with_prev_in,
                    //                                                                     getBackwardComponentAtInOfThisInstruction(
                    //                                                                             (inst))));

                    //         // prev = getBackwardComponentAtInOfThisInstruction(inst);
                    //         if (debug) {
                    //             llvm::outs() << "IN: ";
                    //             printDataFlowValuesBackward(prev);
                    //             printLine(current_context_label);
                    //         }
                    //     } else {
                    //         //creating a new context
                    //         INIT_CONTEXT(target_function, {a2, d2}, {new_outflow_forward,
                    //         new_outflow_backward});//step 21

                    //         pair<int, fetchLR *> mypair = make_pair(current_context_label, &inst);
                    //         //step 14
                    //         SLIM_context_transition_graph[mypair] = context_label_counter;
                    //     }
                    // }
                    // prev = getBackwardComponentAtInOfThisInstruction(inst);
                    // if(debug) {
                    //     llvm::outs() << "IN: ";
                    //     printDataFlowValuesBackward(prev);
                    // }
                    //   continue;
                    //} else
                    if (inst->getCall()) {
                        Instruction *Inst = inst->getLLVMInstruction();
                        /*CallInst *ci = dyn_cast<CallInst>(Inst);
                        Function *target_function = ci->getCalledFunction();*/
                        // Changed to fetch target (Callee) function from SLIM IR instead of LLVM IR
                        CallInstruction *cTempInst = new CallInstruction(Inst);
                        if (cTempInst->isIndirectCall())
                            continue;
                        Function *target_function = cTempInst->getCalleeFunction();
                        if (target_function &&
                            (target_function->isDeclaration() || isAnIgnorableDebugInstruction(Inst))) {
                            continue; // this is an inbuilt function so doesn't need to be processed.
                        }

                        if (intraprocFlag) {
                            /*  If intraprocFlag is true, then target function is not analysed,
                                instead the data flow value is taken from user based on how the effect is defined
                            */
                            setBackwardComponentAtOutOfThisInstruction(inst, prev);

                            B inOfFunction = functionCallEffectBackward(target_function, prev);

                            if (debug) {
                                printLine(current_context_label, 0);
                                llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                                inst->printInstruction();
                                llvm::outs() << "\nOUT: ";
                                printDataFlowValuesBackward(prev);
                            }

                            // setBackwardComponentAtInOfThisInstruction(inst, performMeetBackward(inOfFunction,
                            //                                     getBackwardComponentAtInOfThisInstruction(inst)));
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

                            /*
                            At the call instruction, the value at OUT should be splitted into two components:
                            1) Purely Global and 2) Mixed.
                            The purely global component is given to the end of callee.
                            */

                            pair<F, B> inflow_pair;
                            if (byPass) {
                                bool isGenKill = isGenerativeKillInitialised(current_context_label, inst);
                                if (!isGenKill) {
                                    // initialise Generative Kill for this instruction, context
                                    setInitialisationValueGenerativeKill(current_context_label, inst);
                                }

                                llvm::outs() << "\n--------- BEGIN COMPUTE GENERATIVE ---------\n";
                                int calleeContext = getCalleeContext(current_context_label, inst);
                                pair<F, B> generativeAtCall = computeGenerative(calleeContext, target_function);
                                setGenerativeAtCall(current_context_label, inst, generativeAtCall);
                                llvm::outs() << "\n--------- END COMPUTE GENERATIVE ---------\n";

                                // pair<pair<F,B>, pair<F,B>> generativeKill =
                                // getGenerativeKillAtCall(current_context_label, inst);

                                inflow_pair = CallInflowFunction(
                                    current_context_label, target_function, bb, a1, prev,
                                    generativeAtCall); // TODO should we use getter function for generative?
                            } else {
                                inflow_pair = CallInflowFunction(current_context_label, target_function, bb, a1, prev);
                            }

                            // step 12
                            // pair<F, B> inflow_pair = CallInflowFunction(current_context_label, target_function, bb,
                            // a1, prev);
                            F a2 = inflow_pair.first;
                            B d2 = inflow_pair.second;

                            F new_outflow_forward;
                            B new_outflow_backward;

                            // step 13

                            setBackwardComponentAtOutOfThisInstruction(inst, prev);

                            int matching_context_label = 0;
                            matching_context_label = check_if_context_already_exists(
                                target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward});
                            if (matching_context_label > 0) // step 15
                            {
                                if (debug) {
                                    printLine(current_context_label, 0);
                                    llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                                    inst->printInstruction();
                                    llvm::outs() << "\nOUT: ";
                                    printDataFlowValuesBackward(prev);
                                }
                                // step 14
                                pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                SLIM_context_transition_graph[mypair] = matching_context_label;

                                // step 16 and 17
                                F a3 = getForwardOutflowForThisContext(matching_context_label);
                                B d3 = getBackwardOutflowForThisContext(matching_context_label);

                                pair<F, B> outflow_pair =
                                    CallOutflowFunction(current_context_label, target_function, bb, a3, d3, a1, prev);
                                F a4 = outflow_pair.first;
                                B value_to_be_meet_with_prev_in = outflow_pair.second;
                                // step 18 and 19

                                /*
                                At the call instruction, the value at OUT should be splitted into two components.
                                The purely global component is given to the callee while the mixed component is
                                propagated to IN of this instruction after executing computeINfromOUT() on it.
                                */

                                B a5;
                                if (byPass) {
                                    llvm::outs() << "\n--------- BEGIN COMPUTE KILL ---------\n";
                                    int calleeContext = getCalleeContext(current_context_label, inst);
                                    pair<F, B> killAtCall = computeKill(calleeContext, target_function);
                                    setKillAtCall(current_context_label, inst, killAtCall);
                                    llvm::outs() << "\n--------- END COMPUTE KILL ---------\n";
                                    pair<F, B> onflow_pair =
                                        CallOnflowFunction(current_context_label, inst, killAtCall);
                                    a5 = onflow_pair.second;
                                } else {
                                    a5 = getPurelyLocalComponentBackward(prev);
                                }

                                /*
                                At the IN of this instruction, the value from START of callee procedure is to be merged
                                with the local(mixed) value propagated from OUT. Note that this merging "isn't"
                                exactly (necessarily) the meet between these two values.
                                */

                                /*
                                As explained in ip-vasco,pdf, we need to perform meet with the original value of IN
                                of this instruction to avoid the oscillation problem.
                                */
                                // setBackwardComponentAtInOfThisInstruction(inst,
                                // performMeetBackward(value_to_be_meet_with_prev_in,
                                //                                                             getBackwardComponentAtInOfThisInstruction(inst)));

                                // setBackwardComponentAtInOfThisInstruction(inst,performMeetBackward(
                                //         performMeetBackward(value_to_be_meet_with_prev_in,getBackwardComponentAtInOfThisInstruction(inst)),
                                //         a5));

                                // Merging value from END of callee with value propagated from IN isn't necessarily meet
                                // Thus, adding new function performMergeAtCallBackward()
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
                                // For analysing caller again, insert caller into worklist here itself (since at a later
                                // point,
                                //  we are not able to get the dependent analysis context of the caller from the end of
                                //  callee analysis)

#ifdef PRVASCO
                                llvm::outs() << "\nInserting caller node into Backward Worklist at end of callee in "
                                                "Forward Analysis (Context:"
                                             << current_context_label << " to " << worklistElement.first.first << ")\n";
                                for (Instruction &I : *bb)
                                    llvm::outs() << I << "\n";
#endif
                                backward_worklist.workInsert(worklistElement);
                                if (direction == "bidirectional") {
#ifdef PRVASCO
                                    llvm::outs()
                                        << "\nInserting caller node into Forward Worklist at end of callee in Forward "
                                           "Analysis (Context:"
                                        << current_context_label << " to " << worklistElement.first.first << ")\n";
                                    for (Instruction &I : *bb)
                                        llvm::outs() << I << "\n";
#endif
                                    forward_worklist.workInsert(worklistElement);
                                }

                                // creating a new context
                                int depAnalysisCalleeContext = getDepAnalysisCalleeContext(depAnalysisContext, inst);
                                INIT_CONTEXT(
                                    target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward},
                                    depAnalysisCalleeContext); // step 21

                                pair<int, BaseInstruction *> mypair = make_pair(current_context_label, inst);
                                // step 14
                                SLIM_context_transition_graph[mypair] = context_label_counter;

#ifdef PRVASCO
                                llvm::outs() << "\nAfter New Context Creation, IN value at the node in Backward "
                                                "Analysis (Context: "
                                             << current_pair.first << ") ";
                                printDataFlowValuesBackward(getIn(current_pair.first, current_pair.second).second);
                                llvm::outs() << "\nAfter New Context Creation, OUT value at the node in Backward "
                                                "Analysis (Context: "
                                             << current_pair.first << ") ";
                                printDataFlowValuesBackward(getOut(current_pair.first, current_pair.second).second);
                                llvm::outs() << "\n";
#endif
                                // context_label_to_context_object_map[context_label_counter]->setParent(mypair);
                                // llvm::errs() << "\n" << SLIM_context_transition_graph.size();
                                // MSCHANGE 4 --commented return
                                // return;
                                // Reverse_SLIM_context_transition_graph[context_label_counter] = mypair;
                            }
                        }
                    } else {
                        if (debug) {
                            printLine(current_context_label, 0);
                            llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                            inst->printInstruction();
                            llvm::outs() << "\nOUT: ";
                            printDataFlowValuesBackward(prev);
                        }
                        setBackwardComponentAtOutOfThisInstruction(inst, prev); // compute OUT from previous IN-value

                        B new_prev;
                        if (isIgnorableInstruction(inst)) {
                            new_prev = prev;
                        } else {
                            new_prev = computeInFromOut(inst, depAnalysisContext);
                        }

                        // B newIN = computeInFromOut(inst, depAnalysisContext);
                        /*llvm::outs()<< "\nSETTING IN as the kill value obtained\n";
                        printDataFlowValuesBackward(new_prev);*/
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

                        /*
                        At the call instruction, the value at OUT should be splitted into two components:
                        1) Purely Global and 2) Mixed.
                        The purely global component is given to the end of callee.
                        */
                        // step 12

                        pair<F, B> inflow_pair =
                            CallInflowFunction(current_context_label, target_function, bb, a1, prev);
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

                            pair<F, B> outflow_pair =
                                CallOutflowFunction(current_context_label, target_function, bb, a3, d3, a1, d1);
                            F a4 = outflow_pair.first;
                            B value_to_be_meet_with_prev_in = outflow_pair.second;

                            // step 18 and 19

                            /*
                            At the call instruction, the value at OUT should be splitted into two components.
                            The purely global component is given to the callee while the mixed component is propagated
                            to IN of this instruction after executing computeINfromOUT() on it.
                            */

                            /*
                            At the IN of this instruction, the value from START of callee procedure is to be merged
                            with the local(mixed) value propagated from OUT. Note that this merging "isn't"
                            exactly (necessarily) the meet between these two values.
                            */

                            /*
                            As explained in ip-vasco,pdf, we need to perform meet with the original value of IN
                            of this instruction to avoid the oscillation problem.
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

                            // For analysing caller again, insert caller into worklist here itself (since at a later
                            // point,
                            //  we are not able to get the dependent analysis context of the caller from the end of
                            //  callee analysis)

#ifdef PRVASCO
                            llvm::outs() << "\nInserting caller node into Backward Worklist at end of callee in "
                                            "Forward Analysis (Context:"
                                         << current_context_label << " to " << worklistElement.first.first << ")\n";
                            for (Instruction &I : *bb)
                                llvm::outs() << I << "\n";
#endif
                            backward_worklist.workInsert(worklistElement);
                            if (direction == "bidirectional") {
#ifdef PRVASCO
                                llvm::outs() << "\nInserting caller node into Forward Worklist at end of callee in "
                                                "Forward Analysis (Context:"
                                             << current_context_label << " to " << worklistElement.first.first << ")\n";
                                for (Instruction &I : *bb)
                                    llvm::outs() << I << "\n";
#endif
                                forward_worklist.workInsert(worklistElement);
                            }

                            // creating a new context
                            int depAnalysisCalleeContext = getDepAnalysisCalleeContext(depAnalysisContext, I);
                            INIT_CONTEXT(
                                target_function, {a2, d2}, {new_outflow_forward, new_outflow_backward},
                                depAnalysisCalleeContext); // step 21

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
                        setBackwardComponentAtOutOfThisInstruction(&(*inst), prev); // compute OUT from previous
                                                                                    // IN-value
                        setBackwardComponentAtInOfThisInstruction(
                            &(*inst), computeInFromOut(*inst, depAnalysisContext));
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
            setBackwardIn(
                current_pair.first, current_pair.second, NormalFlowFunctionBackward(current_pair, depAnalysisContext));
        }
        // Check if IN value of the BB has changed
        bool changed = false;
        // MSCHANGE 5
        // if (!EqualDataFlowValuesBackward(previous_value_at_in_of_this_node,
        //                                  getIn(current_pair.first, current_pair.second).second)) {
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

        if (changed && modeOfExec == FSFP) // step 24
        {
// not yet converged
#ifdef PRVASCO
            llvm::outs()
                << "\nInserting node into Forward Worklist on change in IN value in Backward Analysis (Context:"
                << current_context_label << ")\n";
            for (Instruction &I : *bb)
                llvm::outs() << I << "\n";
#endif
            forward_worklist.workInsert(make_pair(make_pair(current_context_label, bb), depAnalysisContext));
            for (auto pred_bb : predecessors(bb)) // step 25
            {
                // step 26
                // TODO check if this condition is needed
                if (direction == "bidirectional") {
#ifdef PRVASCO
                    llvm::outs() << "\nInserting predecessors into Backward Worklist on change in IN value in Backward "
                                    "Analysis (Context:"
                                 << current_context_label << ")\n";
                    for (Instruction &I : *pred_bb)
                        llvm::outs() << I << "\n";
#endif
                    backward_worklist.workInsert(
                        make_pair(make_pair(current_context_label, pred_bb), depAnalysisContext));
                }
            }
        }
        // Check if OUT value of the BB has changed
        bool changed1 = false;
        // MSCHANGE 6
        // if (!EqualDataFlowValuesBackward(previous_value_at_out_of_this_node,
        //                                  getOut(current_pair.first, current_pair.second).second)) {
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
        if (changed1 && modeOfExec == FSFP) { // step 24
#ifdef PRVASCO
            llvm::outs() << "Inserting node into Forward Worklist on change in OUT value in Backward Analysis (Context:"
                         << current_context_label << ")\n";
            for (Instruction &I : *bb)
                llvm::outs() << I << "\n";
#endif
            forward_worklist.workInsert(make_pair(make_pair(current_context_label, bb), depAnalysisContext));
        }
        //--------------------------------------------

        if (bb == &function.getEntryBlock()) // step 27
        {
            // step 28
            setBackwardOutflowForThisContext(
                current_context_label,
                getPurelyGlobalComponentBackward(
                    getIn(current_pair.first, current_pair.second).second)); // setting backward outflow
            // setBackwardOutflowForThisContext(current_context_label, getIn(current_pair.first,
            // current_pair.second).second);

            /*if(SLIM) {
                // auto Parent = context_label_to_context_object_map[current_context_label]->getParent();
                // if(Parent.first != -1 && Parent.second != nullptr) {
                //     BasicBlock *bb = Parent.second->getBB();
                //     pair<int, BasicBlock *> context_bb_pair = make_pair(Parent.first, bb);
                //     if (direction == "bidirectional") {
                //         llvm::outs() << "\n Inserting into the forward worklist";
                //         forward_worklist.workInsert(context_bb_pair);
                //     }
                //     llvm::outs() << "\n Inserting into the backward worklist";
                //     backward_worklist.workInsert(context_bb_pair);
                // }
                // auto result = Reverse_SLIM_context_transition_graph.find(current_context_label);
                // if(result != Reverse_SLIM_context_transition_graph.end()) {
                //     BasicBlock *bb = result->second.second->getBB();
                //     pair<int, BasicBlock *> context_bb_pair = make_pair(result->second.first, bb);
                //     if (direction == "bidirectional") {
                //         llvm::outs() << "\n Inserting into the forward worklist";
                //         forward_worklist.workInsert(context_bb_pair);
                //     }
                //     llvm::outs() << "\n Inserting into the backward worklist";
                //     backward_worklist.workInsert(context_bb_pair);
                //     SLIM_context_transition_graph.erase(result->second);
                //     Reverse_SLIM_context_transition_graph.erase(result);
                // }

               for(auto context_inst_pairs : SLIM_context_transition_graph) {
                   if (context_inst_pairs.second == current_context_label)//matching the called function
                   {
                       //step 30
                       BasicBlock *bb = context_inst_pairs.first.second->getBasicBlock();
                       pair<int, BasicBlock *> context_bb_pair = make_pair(context_inst_pairs.first.first, bb);
                       if (direction == "bidirectional") {
                            #ifdef PRVASCO
                                llvm::outs() << "\nInserting caller node into Forward Worklist at entry of callee in
            Backward Analysis (Context:" << current_context_label << " to " << context_inst_pairs.first.first << ")\n";
                                for (Instruction &I : *bb)
                                    llvm::outs() << I << "\n";
                            #endif
                           forward_worklist.workInsert(context_bb_pair);
                       }
                       #ifdef PRVASCO
                            llvm::outs() << "\nInserting caller node into Backward Worklist at entry of callee in
            Backward Analysis (Context:" << current_context_label << " to " << context_inst_pairs.first.first << ")\n";
                            for (Instruction &I : *bb)
                                llvm::outs() << I << "\n";
                        #endif
                       backward_worklist.workInsert(context_bb_pair);
                       break;
                   }
               }
            } else {
                for (auto context_inst_pairs : context_transition_graph)//step 29
                {
                    if (context_inst_pairs.second == current_context_label)//matching the called function
                    {
                        //step 30

                        BasicBlock *bb = context_inst_pairs.first.second->getParent();
                        pair<int, BasicBlock *> context_bb_pair = make_pair(context_inst_pairs.first.first, bb);
                        if (direction == "bidirectional") {
                            forward_worklist.workInsert(context_bb_pair);
                        }
                        backward_worklist.workInsert(context_bb_pair);
                        break;
                    }
                }
            }*/
        }
        process_mem_usage(this->vm, this->rss);
        this->total_memory = max(this->total_memory, this->vm);
    }
}

template <class F, class B>
B AnalysisGenKill<F, B>::NormalFlowFunctionBackward(
    pair<int, BasicBlock *> current_pair_of_context_label_and_bb, int depAnalysisContext) {
    BasicBlock &b = *(current_pair_of_context_label_and_bb.second);
    B prev = getOut(current_pair_of_context_label_and_bb.first, current_pair_of_context_label_and_bb.second).second;
    F prev_f = getOut(current_pair_of_context_label_and_bb.first, current_pair_of_context_label_and_bb.second).first;
    Context<F, B> *context_object = context_label_to_context_object_map[current_pair_of_context_label_and_bb.first];
    bool changed = false;
    // traverse a basic block in backward direction
    if (SLIM) {
        for (auto &index : optIR->getReverseInstList(optIR->func_bb_to_inst_id[{
                 context_object->getFunction(), current_pair_of_context_label_and_bb.second}])) {
            auto &inst = optIR->inst_id_to_object[index];
            if (debug) {
                printLine(current_pair_of_context_label_and_bb.first, 0);
                llvm::outs() << "\nProcessing instruction at INDEX = " << index;
                inst->printInstruction();
                llvm::outs() << "\nOUT: ";
                printDataFlowValuesBackward(prev);
            }
            setBackwardComponentAtOutOfThisInstruction(inst, prev); // compute OUT from previous IN-value
            B new_dfv; //= computeInFromOut(inst, depAnalysisContext); only if non a builtin call
            if (isIgnorableInstruction(inst)) {
                new_dfv = prev;
            } else {
                new_dfv = computeInFromOut(inst, depAnalysisContext);
            }

            ////if change in data flow value at IN of any instruction, add this basic block to forward worklist
            B old_IN = getBackwardComponentAtInOfThisInstruction(inst);

            if (!EqualDataFlowValuesBackward(new_dfv, old_IN))
                changed = true;

            setBackwardComponentAtInOfThisInstruction(inst, new_dfv);
            if (debug) {
                llvm::outs() << "\nIN: ";
                printDataFlowValuesBackward(new_dfv);
                printLine(current_pair_of_context_label_and_bb.first, 1);
            }
            prev = getBackwardComponentAtInOfThisInstruction(inst);
            process_mem_usage(this->vm, this->rss);
            this->total_memory = max(this->total_memory, this->vm);
        }
        if (changed && modeOfExec == FSFP) {
#ifdef PRVASCO
            llvm::outs() << "\nInserting node into Forward Worklist on change in IN value within BB in Backward "
                            "Analysis (Context:"
                         << current_pair_of_context_label_and_bb.first << ")\n";
            for (Instruction &I : b)
                llvm::outs() << I << "\n";
#endif
            forward_worklist.workInsert(make_pair(current_pair_of_context_label_and_bb, depAnalysisContext));
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
            B new_dfv = computeInFromOut(*inst, depAnalysisContext);
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
pair<F, B> getAnalysisInAtInstruction(BaseInstruction *I, int context_label, AnalysisGenKill<F, B> *analysisObj) {
    F forwardIn = analysisObj->getForwardComponentAtInOfThisInstruction(I, context_label);
    B backwardIn = analysisObj->getBackwardComponentAtInOfThisInstruction(I, context_label);
    return make_pair(forwardIn, backwardIn); // analysisObj->SLIM_IN[context_label][I];
}

template <class F, class B>
pair<F, B> getAnalysisOutAtInstruction(BaseInstruction *I, int context_label, AnalysisGenKill<F, B> *analysisObj) {
    F forwardOut = analysisObj->getForwardComponentAtOutOfThisInstruction(I, context_label);
    B backwardOut = analysisObj->getBackwardComponentAtOutOfThisInstruction(I, context_label);
    return make_pair(forwardOut, backwardOut); // analysisObj->SLIM_OUT[context_label][I];
}

// End of Global accessor functions

#endif
