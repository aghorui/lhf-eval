// File created by SRI
#ifndef TRANSITIONGRAPH_H
#define TRANSITIONGRAPH_H

#include <map>

#include "Context.h"
#include "IR.h"

using namespace llvm;
using namespace std;

class HashFunction {
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

    auto operator()(const pair<pair<int, BasicBlock *>, int> &P) const {
        // return hash<int>()(P.first) ^ std::hash<Instruction *>()(P.second);
        // Same as above line, but viewed the actual source code of C++ and then wrote it
        // Referred: functional_hash.h (line 114)
        // Referred: functional_hash.h (line 110)
        return static_cast<size_t>(P.first.first) ^ reinterpret_cast<size_t>(P.first.second);
    }

    auto operator()(const pair<pair<int, BasicBlock *>, bool> &P) const {
        // return hash<int>()(P.first) ^ std::hash<Instruction *>()(P.second);
        // Same as above line, but viewed the actual source code of C++ and then wrote it
        // Referred: functional_hash.h (line 114)
        // Referred: functional_hash.h (line 110)
        return static_cast<size_t>(P.first.first) ^ reinterpret_cast<size_t>(P.first.second);
    }
};

class TransitionGraph {
private:
    // Transition graph--map from the pair<source context id,instruction> to list of target context ids
    unordered_map<pair<int, BaseInstruction *>, vector<pair<Function *, int>>, HashFunction> context_transition_graph;

public:
    TransitionGraph();
    // add a new edge to the context graph

    unordered_map<pair<int, BaseInstruction *>, vector<pair<Function *, int>>, HashFunction> get_graph();
    void addEdge(pair<int, BaseInstruction *> key, int target_context, int contextIndex);

    void addEdge(pair<int, BaseInstruction *> key, int target_context, Function *func);

    // returns vector of contexts that are called by caller
    vector<int> getCalleeContexts(int callerContext, BaseInstruction *I);

    int getCalleeContext(int callerContext, BaseInstruction *I, Function *func);

    vector<pair<int, BasicBlock *>> getCallers(int context_label);

    void printGraph();

    void printInstructionDetails(BaseInstruction *inst);
};

#endif
