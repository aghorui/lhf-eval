#include "TransitionGraph.h"
#include <cstddef>

// Constructor
TransitionGraph::TransitionGraph() {
    context_transition_graph =
        unordered_map<pair<int, BaseInstruction *>, vector<pair<Function *, int>>, HashFunction>();
}

unordered_map<pair<int, BaseInstruction *>, vector<pair<Function *, int>>, HashFunction> TransitionGraph::get_graph() {
    return this->context_transition_graph;
}

// add a new edge to the context graph
/*
void TransitionGraph::addEdge(pair<int, BaseInstruction *> key,int target_context, int contextIndex)
{
    llvm::outs() <<contextIndex;
    vector<int> callees = context_transition_graph[key];
    if(contextIndex!=-1 and callees.size()>0)
    {
        callees.erase(callees.begin() + contextIndex);

    }
    callees.push_back(target_context);
    context_transition_graph[key]=callees;
    llvm::outs() << "\n After adding edge\n";
    printGraph();
    return;
}
*/

// add a new edge to the context graph
void TransitionGraph::addEdge(pair<int, BaseInstruction *> key, int target_context, Function *func) {
    // llvm::outs() <<func->getName();
    // llvm::outs() <<target_context;
    // llvm::outs() << "\n Before adding edge\n";
    // printGraph();
    vector<pair<Function *, int>> callees = context_transition_graph[key];
    bool func_found = false;
    for (int i = 0; i < callees.size(); i++) {
        auto func_context_label_pair = callees[i];
        if (func_context_label_pair.first == func) {
            // callees.erase(callees.begin()+i);
            func_found = true;
            callees[i].second = target_context;
        }
    }
    if (!func_found) {
        callees.push_back(make_pair(func, target_context));
    }
    context_transition_graph[key] = callees;
    // llvm::outs() << "\n After adding edge\n";
    // printGraph();
    return;
}

// returns vector of contexts that are called by caller
vector<int> TransitionGraph::getCalleeContexts(int callerContext, BaseInstruction *I) {
    vector<int> callees;
    pair<int, BaseInstruction *> callerInfo = make_pair(callerContext, I);
    for (auto func_context_label_pair : context_transition_graph[callerInfo]) {
        callees.push_back(func_context_label_pair.second);
    }
    return callees;
}

// returns context of function func that are called by caller
int TransitionGraph::getCalleeContext(int callerContext, BaseInstruction *I, Function *func) {
    pair<int, BaseInstruction *> callerInfo = make_pair(callerContext, I);
    for (auto func_context_label_pair : context_transition_graph[callerInfo]) {
        if (func_context_label_pair.first == func) {
            return func_context_label_pair.second;
        }
    }
    return -1;
}

vector<pair<int, BasicBlock *>> TransitionGraph::getCallers(int context_label) {
    vector<pair<int, BasicBlock *>> callers;
    // vector<pair<int, BaseInstruction *>> callers;

    for (auto context_inst_pairs : context_transition_graph) {
        for (auto func_context_label_pair : context_inst_pairs.second) {
            if (func_context_label_pair.second == context_label) {
                // matching the called function
                callers.push_back(
                    std::make_pair(context_inst_pairs.first.first, context_inst_pairs.first.second->getBasicBlock()));
                break;
            }
        }
    }

    return callers;
}

void TransitionGraph::printGraph() {
    for (auto context_inst_pairs : context_transition_graph) {
        llvm::outs() << "<Context ID, Instruction> -> Target Context IDs\n";
        llvm::outs() << context_inst_pairs.first.first << " , ";
        context_inst_pairs.first.second->printInstruction();
        llvm::outs() << " -> ";
        // printInstructionDetails(context_inst_pairs.first.second);
        vector<pair<Function *, int>> callees = context_transition_graph[context_inst_pairs.first];
        for (auto i : callees) {
            llvm::outs() << "( " << i.first->getName() << ", " << i.second << ") " << ",";
        }
        llvm::outs() << "\n";
    }
}
