/*******************************************************************************************************
                      "Interprocedural LFCPA using VASCO"
Author:      Aditi Raste
Created on:  22/8/2022
Description:
             LFCPA pass performs the following taks:
             1. Interprocedural Strong Liveness Analysis
             2. Interprocedural Points-to Analysis
Last Updated:
Current Status: Change in compareOperands. Now address comparison instead of
string compare
SOURCE variables re-introduced - 26March2025
**********************************************************************************************************/

/* SLIM Operands with type ConstantInt and values
 *  marker = 999999
 *  universal set of pointers (kill) = 111111 --- no longer required
 *  question mark (def free paths) = 111111  (was: 000000 but conflicted with
 * insID=0 in heap analysis
 */

#include "lfcpa.h"
#include "Profiling.hpp"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/IRBuilder.h"
#include <cassert>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/raw_ostream.h>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

// Global declaration
string fileName = "samples/abc.txt";
slim::IR *optIR;
std::vector<SLIMOperand *> listGlobalVar;

static int total_instructions, gep_insturctions, nongep_instructions, ignored, ignored_gep, non_ignored_ins;

#ifdef PRINT
bool debugFlag = true;
#endif

#ifndef PRINT
bool debugFlag = false;
#endif

// Global Data Structure to store Pout_EndQ and Def_Q summary for every proc 'Q'
// <F, <PTA, DEF>>
// std::map<Function *, std::pair<std::map<SLIMOperand *, std::set<SLIMOperand
// *>>,
//                               std::set<SLIMOperand *>>>    mapFunPtaDefSum;

std::vector<std::unique_ptr<std::pair<SLIMOperand *, int>>> globalSourceStorage;
std::map<Function *, std::pair<LFPointsToSet, LFLivenessSet>> mapFunPtaDefSum;


IPLFCPA lfcpaObj(module, debugFlag, true, false, FSFP, false);

void printVector(std::vector<SLIMOperand *> ipVec) {
    for (int i = 0; i < ipVec.size(); i++) {
        llvm::outs() << "[" << *(ipVec[i]->getValue()) /*<< " obj "<< ipVec[i]*/ << "]";
    }
}

// IPLFCPA lfcpaObj(module,fileName,true,true,false,FSFP,true);
// IPLFCPA lfcpaObj(module,true/*false*/,true,false,FSFP,true);  /* with
// lfcpaObj(module,/*true*/false,true,true/*intra*/,FSFP,false);

IPLFCPA::IPLFCPA(
    std::unique_ptr<llvm::Module> &module, bool debug, bool SLIM, bool intraProc, ExecMode modeOfExec, bool ___e01)
    : Analysis<F, B>(module, debug, SLIM, intraProc, modeOfExec, ___e01) {}

IPLFCPA::IPLFCPA(
    std::unique_ptr<llvm::Module> &module, const string &filename, bool debug, bool SLIM, bool intraProc,
    ExecMode modeOfExec, bool ___e01)
    : Analysis<F, B>(module, filename, debug, SLIM, intraProc, modeOfExec, ___e01) {}

bool IPLFCPA::checkChangeinPointees(LFLivenessSet prev, LFLivenessSet curr) {
    return !(prev == curr);
}

bool IPLFCPA::isPointerSource(SLIMOperand *op) {

    llvm::Value *valOp = op->getValue();

    if (op->isGlobalOrAddressTaken() || op->isAlloca()) {
        assert(valOp != nullptr && "ValOP is null");
        if (valOp->getType()->getContainedType(0)->isPointerTy())
            return true;
    } else {
        if (valOp != nullptr) {
            if (valOp->getType()->isPointerTy()) // temporaries
                return true;
        }
    }
    return false;
}

void IPLFCPA::findNode(CallGraphNode *CN) {
    Function *F = CN->getFunction();
    if (F != nullptr) {
        /*if (debugFlag)
          llvm::outs() << "Call graph node for function: '" << F->getName() <<
          "'"
                       << "\n";
        */
        for (const auto &I : *CN) {
            // llvm::outs() << "  CS<" << I.first << "> calls ";
            if (Function *FI = I.second->getFunction()) {
                // llvm::outs() << "function '" << FI->getName() <<"'\n";
                // if (debugFlag)
                // llvm::outs() << "\nAdding pair " << F->getName() << " "
                //            << FI->getName() << "\n";
                call_graph_edges.push_back(std::make_pair(CN->getFunction(), FI));
            }
            /*else
                llvm::outs() << "external node\n";*/
        }
    }
    // llvm::outs() << "\nEND of one call\n";
}

void IPLFCPA::populateCallGraphEdges(std::unique_ptr<llvm::Module> &mod) {
    CallGraph CG = CallGraph(*mod);

    // CG.dump();

    std::vector<CallGraphNode *> functionNodes;
    // SmallVector<CallGraphNode *, 16> Nodes;
    // Nodes.reserve(FunctionMap.size());
    for (const auto &I : CG)
        functionNodes.push_back(I.second.get());

    for (CallGraphNode *CN : functionNodes) {
        findNode(CN);
    }
}

void IPLFCPA::addAllPredecessorFunctions(Function *fun) {
    for (pair<Function *, Function *> p : call_graph_edges) {
        if (p.second == fun) {
            if (pointerFunList.find(p.first) == pointerFunList.end()) {
                pointerFunList.insert(p.first);
                if (debugFlag)
                    llvm::outs() << "\nPred " << p.first->getName() << "\n";
                addAllPredecessorFunctions(p.first);
            }
        }
    }
}

void computeRelevantIns(slim::IR *optIR) {
    llvm::outs() << "\n Inside computeRelevantIns...........";
    std::map<Function *, std::set<long long>> func_ins_map;

    for (auto &entry : optIR->func_bb_to_inst_id) {
        Function *func = entry.first.first;
        for (long long instruction_id : entry.second) {
            // if(func_ins_map.find(func)!==func_ins_map.end())
            func_ins_map[func].insert(instruction_id);
        }
    }

    for (auto &entry : func_ins_map) {
        for (long long instruction_id : entry.second) {
            BaseInstruction *I = optIR->inst_id_to_object[instruction_id];
            total_instructions++;

            if (I->isIgnored()) {
                ignored++;
                if (I->getInstructionType() == GET_ELEMENT_PTR)
                    ignored_gep++;
            } else { // not ignored
                non_ignored_ins++;
                if (I->getInstructionType() == GET_ELEMENT_PTR)
                    gep_insturctions++;
                else
                    nongep_instructions++;
            } // else
        } // inner for
    } // for

    llvm::outs() << "\n TOTAL INSTRUCTIONS = " << total_instructions;
    llvm::outs() << "\n GEP INSTRUCTIONS = " << gep_insturctions;
    llvm::outs() << "\n NON-GEP INSTRUCTIONS = " << nongep_instructions;
    llvm::outs() << "\n IGNORED INSTRUCTIONS = " << ignored;
    llvm::outs() << "\n IGNORED GEP = " << ignored_gep;
    llvm::outs() << "\n NON-IGNORED INSTRUCTIONS = " << non_ignored_ins;
}

void IPLFCPA::identifyPtrFun(slim::IR *optIR) {
    bool chkPtrFlag = false;
    std::map<Function *, std::set<long long>> func_ins_map;

    for (auto &entry : optIR->func_bb_to_inst_id) {
        Function *func = entry.first.first;
        for (long long instruction_id : entry.second) {
            // if(func_ins_map.find(func)!==func_ins_map.end())
            func_ins_map[func].insert(instruction_id);
        }
    }

    for (auto &entry : func_ins_map) {
        chkPtrFlag = false;

        auto pos = pointerFunList.find(entry.first);
        if (pos == pointerFunList.end()) {

            for (long long instruction_id : entry.second) {

                BaseInstruction *I = optIR->inst_id_to_object[instruction_id];
                std::pair<SLIMOperand *, int> LHS = I->getLHS();
                std::vector<std::pair<SLIMOperand *, int>> RHS = I->getRHS();
                if (I->getInstructionType() != BRANCH && I->getInstructionType() != CALL &&
                    I->getInstructionType() != UNREACHABLE && I->getInstructionType() != SWITCH) {
                    if (I->getInstructionType() == RETURN) {
                        ReturnInstruction *RI = (ReturnInstruction *)I;
                        if (isPointerSource(RI->getReturnOperand())) {
                            chkPtrFlag = true;
                            break;
                        }
                    } else if (I->getInstructionType() == COMPARE) {
                        for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end();
                             r++) {
                            SLIMOperand *rhsVal = r->first;
                            if (isPointerSource(rhsVal)) {
                                chkPtrFlag = true;
                                break;
                            }
                        } // end for
                    } // end compare
                    else {
                        if (isPointerSource(LHS.first)) {
                            chkPtrFlag = true;
                            break;
                        } else {
                            for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end();
                                 r++) {

                                SLIMOperand *rhsVal = r->first;
                                if (isPointerSource(rhsVal)) {
                                    chkPtrFlag = true;
                                    break;
                                }
                            } // end for
                        } // end else
                    } // end outer else
                }
            }
            if (chkPtrFlag) {
                addAllPredecessorFunctions(entry.first);
                pointerFunList.insert(entry.first);
            }
        }
    } // end if not already in list
}

/* Functions for Backwards flow analysis */

B IPLFCPA::getMainBoundaryInformationBackward(BaseInstruction *I) {
    B B_TOP;
    return B_TOP;
}

B IPLFCPA::getBoundaryInformationBackward() {
    if (debugFlag) {
        //   llvm::outs() << "\n Inside getBoundaryInformationBackward ";
    }
    B B_TOP;
    return B_TOP;
}

B IPLFCPA::getInitialisationValueBackward() {
    if (debugFlag) {
        // llvm::outs() << "\n Inside getInitialisationValueBackward ";
    }
    B B_TOP;
    return B_TOP;
}

bool IPLFCPA::compareOperands(SLIMOperand *a1, SLIMOperand *a2) const {
    /*
     * This function two SLIM Operands. Indices are ignored if the operands are
     * generated due to dynamic memory allocation or they are arrays. Value of
     * array[0][1] is different from array[0][2]. Since are we over
     * approximating arrays by ignoring the  indices we maintain the main GEP
     * operand of arrays and compare two arrays based on the value returned by
     * the get_gep_main_operand() in SLIM.
     */

    /* if (debugFlag) {
       llvm::outs() << "\n Inside compareOPerands...";
       llvm::outs() << "\n a1 = " << a1->getOnlyName()
                    << " vec sz = " << a1->getIndexVector().size();
       llvm::outs() << "\n a2 = " << a2->getOnlyName()
                    << " vec sz = " << a2->getIndexVector().size();
       llvm::outs() << "\n a1 value = " << a1->getValue()
                    << " a2 value = " << a2->getValue();
       llvm::outs() << "\n a1 gep op = " << a1->getOperandType()
                    << " a2 gep op = " << a2->getOperandType();
       llvm::outs() << "a1->isDynamicAllocationType() =
     "<<a1->isDynamicAllocationType()
                    <<  "a2->isDynamicAllocationType() =
     "<<a2->isDynamicAllocationType(); llvm::outs() << "\n a1 vector";
       printVector(a1->getIndexVector());
       llvm::outs() << "\n a2 vector";
       printVector(a2->getIndexVector());


     }*/
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    if (a1 == a2) {
#ifdef PRINTSTATS
        stat.timerEnd(__func__);
#endif
        return true;
    }

    if (a1->isDynamicAllocationType() and a2->isDynamicAllocationType()) {
        // ignore indices
        //@ Indices were ignored but due the problem with malloc objects being
        // returned as an array we have to consider indices now. 21.7.25
        // if (a1->getValue() == a2->getValue()) {
        if (a1->getValue() == a2->getValue() and compareIndices(a1->getIndexVector(), a2->getIndexVector())) {
#ifdef PRINTSTATS
            stat.timerEnd(__func__);
#endif
            return true;
        }
    } else if (a1->isArrayElement() and a2->isArrayElement()) {
        llvm::Value *v1 = a1->getValueOfArray();
        llvm::Value *v2 = a2->getValueOfArray();
        if (v1 == v2) {
#ifdef PRINTSTATS
            stat.timerEnd(__func__);
#endif
            return true;
        }
    } else {
        if (a1->getValue() == a2->getValue() and compareIndices(a1->getIndexVector(), a2->getIndexVector())) {
#ifdef PRINTSTATS
            stat.timerEnd(__func__);
#endif
            return true;
        }
    }
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return false;
}

bool IPLFCPA::EqualDataFlowValuesBackward(const B &d1, const B &d2) const {
    return d1 == d2;
}

bool IPLFCPA::EqualContextValuesBackward(const B &d1, const B &d2) const {
    return d1 == d2;
}

B IPLFCPA::performMergeAtCallBackward(const B &d1, const B &d2) const {
    if (debugFlag) {
        llvm::outs() << "\n Inside performMergeAtCallBackward.............";
        llvm::outs() << "\n value_to_be_meet_with_prev_in: ";
        printDataFlowValuesBackward(d1);
        llvm::outs() << "\n a5: ";
        printDataFlowValuesBackward(d2);
    }

    return performMeetBackward(d1, d2);
}

B IPLFCPA::performMeetBackward(const B &d1_currOUT, const B &d2_succIN) const {
    return d1_currOUT.set_union(d2_succIN);
}

B IPLFCPA::getPurelyGlobalComponentBackward(const B &dfv) const {
    return dfv.get_purely_global();
}

void IPLFCPA::printDataFlowValuesBackward(const B &dfv) const {
    llvm::outs() << "{ ";
    for (auto val : dfv) {
        llvm::outs() << GET_KEY(val)->getOnlyName();
        printVector(GET_KEY(val)->getIndexVector());
        llvm::outs() << ", "; // check this
    }
    llvm::outs() << " }";
}

void IPLFCPA::printFileDataFlowValuesBackward(const B &dfv, std::ofstream &out) const {
    out << "{ ";
    for (auto val : dfv) {
        out << GET_KEY(val)->getOnlyName().str();
        printVector(GET_KEY(val)->getIndexVector());
        out << ", "; // check this
    }
    out << " }";
}

/// Merges the liveness information
// B IPLFCPA::backwardMerge(B prevOUT, B succIN) {
// std::set<SLIMOperand *> IPLFCPA::backwardMerge(std::set<SLIMOperand *>
// prevOUT, std::set<SLIMOperand *> succIN) {
LFLivenessSet IPLFCPA::backwardMerge(LFLivenessSet prevOUT, LFLivenessSet succIN) { // #modAR::LHF
    if (debugFlag)
        llvm::outs() << "\n Inside backwardMerge................";
    return prevOUT.set_union(succIN);
}

B IPLFCPA::getFPandArgsBackward(long int Index, Instruction *I) {
    // llvm::outs() << "\n Inside getFPandArgsBackward..............";
    B retLivenessInfo;
    return retLivenessInfo;
}

bool IPLFCPA::checkIsSource(SLIMOperand *operand) {
    if (operand->isVariableGlobal() or operand->isFormalArgument() or operand->isAlloca())
        return true;
    return false;
}

bool IPLFCPA::checkOperandInDEF(LFLivenessSet defset, SLIMOperand *op) {
    for (auto d : defset) {
        if (compareOperands(GET_KEY(d), op))
            return true;
    }
    return false;
}

SLIMOperand *IPLFCPA::fetchNewOperand(llvm::Value *value, std::vector<SLIMOperand *> vecIndex) {
    // creates a new operand if not exists else returns the existing operand.

    if (debugFlag) {
        llvm::outs() << "\n Inside fetchNewOperand......";
        llvm::outs() << "\n Value = " << value;
        llvm::outs() << "\n Index vector = ";
        printVector(vecIndex);
        llvm::outs() << "\n ===============";
    }
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    bool flagOpExists = false;
    for (auto op : mapNewOperand) {
        if (op.first.first == value and compareIndices(op.first.second, vecIndex)) {
            if (debugFlag)
                llvm::outs() << "\n OPERAND ALREADY EXISTS!!";
            flagOpExists = true;
#ifdef PRINTSTATS
            stat.timerEnd(__func__);
#endif
            return mapNewOperand[op.first];
        }
    } // end for

    if (!flagOpExists) { // operand does not exists in the MAP so create a new
                         // operand
        if (debugFlag)
            llvm::outs() << "\n OPERAND DOES NOT EXISTS. SO CREATE NEW.";
        SLIMOperand *newOperand = new SLIMOperand(value, true); // does not exists. So create new.
                                                                /*    SLIMOperand *newOperand =
                                                                            OperandRepository::getOrCreateSLIMOperand(value);*/

        newOperand->resetIndexVector();
        for (int indIter = 0; indIter < vecIndex.size(); indIter++) {
            newOperand->addIndexOperand(vecIndex[indIter]);
        }
        newOperand->setOperandType(GEP_OPERATOR);
        newOperand->setGEPMainOperand(value);
        mapNewOperand[make_pair(value, vecIndex)] = newOperand; // insert newOperand into global map
    }
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return mapNewOperand[make_pair(value, vecIndex)];
}

void IPLFCPA::handleArrayElementBackward(SLIMOperand *arrOperand, BaseInstruction *I) {

    // llvm::outs() << "\n Inside handleArrayElementBackward.......";
    // llvm::outs() << "\n slim op = "<<arrOperand->getOnlyName();
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    bool flgFound = false;
    // check if array is in global data structure else make an entry
    for (auto list : listFISArrayObjects) {

        if ((compareOperands(list->fetchSLIMArrayOperand(), arrOperand))) {
            flgFound = true; // array already in global data structure
            set<BasicBlock *> tempSet = list->getPtUse();
            auto pos = tempSet.find(I->getBasicBlock());
            if (pos == tempSet.end()) {
                list->setPtUse(I->getBasicBlock());
                list->setFlgChangeInUse(true);
            }
        } // end if
    } // end for

    if (!flgFound) {
        // array not found in the global str
        FISArray *newArray = new FISArray(arrOperand);
        newArray->setOnlyArrayName(arrOperand->getOnlyName());
        newArray->setPtUse(I->getBasicBlock());
        newArray->setFlgChangeInUse(true);
        listFISArrayObjects.push_back(newArray); // insert into global data str
    } // end if
/// llvm::outs() << "\n RETURNING.............";
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
}

pair<F, B> IPLFCPA::performParameterMapping(BaseInstruction *base_instruction, Function *callee, F PIN, B LIN_StartQ) {

    B LinCallInst = LIN_StartQ;

    F PinStartCallee = PIN;
    if (debugFlag)
        llvm::outs() << "\n Inside performParameterMapping.........";
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    std::map<int, SLIMOperand *> actual_formal_arg_mapping;
    Instruction *Inst = base_instruction->getLLVMInstruction();
    llvm::CallInst *call_instruction = llvm::dyn_cast<llvm::CallInst>(Inst);

    auto type = callee->getFunctionType();
    auto num = type->getNumParams();

    if (debugFlag)
        llvm::outs() << "\n Parameters = " << num;

    for (auto i = 0; i < num; i++) {
        llvm::Argument *formal = callee->getArg(i);
        SLIMOperand *formal_slim_argument = OperandRepository::getSLIMOperand(formal);

        if (debugFlag) {
            if (formal_slim_argument != nullptr)
                llvm::outs() << "\n Formal arg name = " << formal_slim_argument->getOnlyName();
        }

        // std::set<SLIMOperand *> LIN_StartQ_temp;
        LFLivenessSet LIN_StartQ_temp; // modAR::LHF

        if (formal_slim_argument != nullptr) { // only if formal arg is not null
            // auto pos = LIN_StartQ.find(formal_slim_argument);
            // if (pos != LIN_StartQ.end()) {  modAR::LHF

            if (LIN_StartQ.contains(formal_slim_argument)) {

                actual_formal_arg_mapping[i] = formal_slim_argument;
                if (debugFlag)
                    llvm::outs() << "\n Inserted pos into map: " << i;

                llvm::Value *arg_actual = Inst->getOperand(i);
                SLIMOperand *actual_operand = OperandRepository::getOrCreateSLIMOperand(arg_actual);

                if (actual_operand != nullptr) {
                    // LinCallInst.insert(actual_operand);   modAR::LHF

                    LinCallInst = LinCallInst.insert_single(actual_operand);

                    LinCallInst = eraseFromLin(formal_slim_argument, LinCallInst);
                    if (debugFlag)
                        llvm::outs() << "\n Actual arg inserted in LIN = " << actual_operand->getOnlyName();

                    for (auto pin : PIN) {
                        if (compareOperands(GET_PTSET_KEY(pin), actual_operand)) {
                            if (debugFlag)
                                llvm::outs() << "\n points-to information matching";
                            if (llvm::isa<llvm::GlobalVariable>(actual_operand->getValue()) or
                                actual_operand->isAlloca())
                                // PinStartCallee[formal_slim_argument].insert(actual_operand);
                                PinStartCallee = PinStartCallee.insert_pointee(formal_slim_argument, actual_operand);
                            else {
                                // PinStartCallee[formal_slim_argument] =
                                // pin.second;
                                PinStartCallee = PinStartCallee.update_pointees(formal_slim_argument, GET_VALUE(pin));
                            } // else
                        } // fi
                    } // for
                } // fi actual not null
            } // if
        } // if formal not null
    } // for
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    if (debugFlag) {
        llvm::outs() << "\n Printing the matched parameters before returning....";
        llvm::outs() << "\n PinStartCallee: ";
        printDataFlowValuesForward(PinStartCallee);
        llvm::outs() << "\n LinCallInst: ";
        printDataFlowValuesBackward(LinCallInst);
    }
    return std::make_pair(PinStartCallee, LinCallInst);
}

SLIMOperand *IPLFCPA::fetchMallocInsOperand(long long insID, BasicBlock *B) {
    // creates new or returns already created malloc operand using instruction
    // id
    if (debugFlag) {
        llvm::outs() << "\n Inside fetchMallocInsOperand..........";
        llvm::outs() << "\n Instruction ID: " << insID;
    }
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    auto pos = mapMallocInsOperand.find(insID); // check if exists
    if (pos == mapMallocInsOperand.end()) {
        IRBuilder<> IRB(B);
        std::stringstream S;
        S << "OB_" << insID;
        Value *v1 = IRB.CreateGlobalString(S.str());
        v1->setName(S.str());

        SLIMOperand *newMallocOperand = new SLIMOperand(v1, false);
        ////  SLIMOperand *newMallocOperand =
        ////      OperandRepository::getOrCreateSLIMOperand(v1);

        mapMallocInsOperand[insID] = newMallocOperand;
        newMallocOperand->setDynamicAllocationType();

        if (debugFlag) {
            llvm::outs() << "\n NEW MALLOC OPERAND CREATED name = " << newMallocOperand->getOnlyName()
                         << "\n Is newOperand array = " << newMallocOperand->isArrayElement()
                         << "\n Is newOperand alloca = " << newMallocOperand->isAlloca()
                         << "\n is newOperand global = " << newMallocOperand->isVariableGlobal();
        }
        /* Insert into mapNewOperand. Bcoz when the malloc operand will always
         * be compared as a GEP operand*/
        std::vector<SLIMOperand *> vecIndex;
        mapNewOperand[make_pair(v1, vecIndex)] = newMallocOperand;
    }
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return mapMallocInsOperand[insID];
}

SLIMOperand *IPLFCPA::getMallocInsOperand(long long insID) {
    return mapMallocInsOperand[insID];
}

void IPLFCPA::setMallocInsOperand(long long insID, SLIMOperand *op) {
    mapMallocInsOperand[insID] = op;
}

set<Function *>
IPLFCPA::handleFunctionPointers(F Pin, SLIMOperand *function_operand, int context_label, BaseInstruction *I) {
    //{P, Q, R}
    // returns pointee info of function pointer
    // Pin= fp->Q

    if (debugFlag) {
        llvm::outs() << "\n FUNCTION POINTER IDENTIFIED........";
        llvm::outs() << "\n Instruction:  ";
        I->printInstruction();
        /////llvm::outs() << "\n Source File name: "<<getFileName(I)
        /////        << " Line No: "<<getSourceLineNumberForInstruction(I);
        // llvm::outs() << "\n FP NAME : " << function_operand->getOnlyName();
        // llvm::outs() << "\n PIN values....";
        // printDataFlowValuesForward(Pin);
        // llvm::outs() << "\n POINTEES: ";
        /* llvm::outs() << "\n LIN values....";
         printDataFlowValuesBackward(getBackwardComponentAtInOfThisInstruction(I));
         llvm::outs() << "\n LOUT values....";
         printDataFlowValuesBackward(getBackwardComponentAtOutOfThisInstruction(I));*/
        llvm::outs() << "\n Context_label = " << context_label;
    } //
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    std::set<Function *> Function_Pointee;
    std::pair<LFLivenessSet, int> valDefBlock, calleeDefBlock, prevBlockCaller;

    // Fetch caller details
    Function *currFunction = getFunctionAssociatedWithThisContext(context_label /*caller*/);
    pair<F, B> currInflow = getInflowforContext(context_label);

    /* if (debugFlag) {
     llvm::outs() << "\n Printing caller details : "
                  << "\n Name = "<<currFunction->getName()
                  << "\n Printing Inflow: Forward ";
     printDataFlowValuesForward(currInflow.first);
     llvm::outs() << "\t Backward Inflow: :";
     printDataFlowValuesBackward(currInflow.second);
     }*/

    std::pair<std::pair<LFLivenessSet, int>, bool> fieldsTabContextDefBlock, fieldsCalleeDefBlock, fieldsCallerDefBlock;
    fieldsTabContextDefBlock = getTabContextDefBlock(make_pair(currFunction, currInflow));
    valDefBlock = fieldsTabContextDefBlock.first;

    bool flagFoundPointee = false;
    LFLivenessSet fpPointees;
    ///  for (auto pointer : Pin) {
    /// if (compareOperands(pointer.first, function_operand)) {

    CallInstruction *cTempInst = new CallInstruction(I->getLLVMInstruction());
    if (debugFlag)
        llvm::outs() << "\n call operand : " << cTempInst->getIndirectCallOperand()->getOnlyName();

    for (auto pointer : Pin) {
        if (compareOperands(GET_PTSET_KEY(pointer), cTempInst->getIndirectCallOperand())) {
            fpPointees = GET_VALUE(pointer);
#ifdef PRINTUSEPOINT
            llvm::outs() << "\n PRINT USE POINT 1: ID = " << I->getInstructionId();
            std::map<SLIMOperand *, LFLivenessSet> tempPT;
            tempPT[pointer.first] = fpPointees;
            setPointsToPairForUsePoint(I->getInstructionId(), pointer.first, tempPT);
#endif
        }
    } // for

    for (auto f : fpPointees) {
        SLIMOperand *orig_operand_or_within = GET_KEY(f);

        if (GET_KEY(f)->getOperandWithin() != nullptr) {
            // returns the operator of bitcast unary operation
            orig_operand_or_within = GET_KEY(f)->getOperandWithin();
        }

        Function *callee = orig_operand_or_within->getAsFunction();

        if (callee != nullptr) {
            if (debugFlag)
                llvm::outs() << "\n Indirect Callee name = " << callee->getName();

            Function_Pointee.insert(callee);
            flagFoundPointee = true;

            /* Fetch the previous context of callee at this call site*/
            int prevCalleeContext = getCalleeContext(context_label, I, callee);
            if (prevCalleeContext == -1) {
                calleeDefBlock = objDef.getMapFunDefBlock(callee);
            } else {
                pair<F, B> prevInflow = getInflowforContext(prevCalleeContext);
                fieldsCalleeDefBlock = getTabContextDefBlock(make_pair(callee, prevInflow));
                calleeDefBlock = fieldsCalleeDefBlock.first;
            }
            // Merge the callee Def Block with the callers
            std::pair<LFLivenessSet, int> mergedDefBlock;
            fieldsCallerDefBlock = getTabContextDefBlock(make_pair(currFunction, currInflow));
            prevBlockCaller = fieldsCallerDefBlock.first; // save previous block value of caller
            mergedDefBlock = objDef.performMeetFIS(fieldsCallerDefBlock.first, calleeDefBlock);
            valDefBlock = fieldsCallerDefBlock.first;
        } else {
            if (debugFlag)
                llvm::outs() << "\n ALERT!! Cast was unsuccessful!";
        }
    } // for pointee
    ///  }   // s not null
    ///}     // source

    // If pointees of functio pointer are found the decrement BLOCK of caller.
    // Set the BLOCK flag if BLOCK value is 0 or less.
    bool flagNoChange = false;
    if (flagFoundPointee and prevBlockCaller.second > 0) {
        // if (valDefBlock.second > 0)
        valDefBlock.second = valDefBlock.second - 1; // Decrement the Block set for caller
        if (valDefBlock.second == 0)
            flagNoChange = true; // set Block flag changed
    }

    if (flagNoChange) {
        if (debugFlag)
            llvm::outs() << "\n SETTING BLOCK FLAG to TRUE 2";
        setTabContextDefBlock(make_pair(currFunction, currInflow), make_pair(valDefBlock, true));
    } else {
        if (debugFlag)
            llvm::outs() << "\n SETTING BLOCK FLAG to FALSE 3";
        setTabContextDefBlock(make_pair(currFunction, currInflow), make_pair(valDefBlock, false));
    }

    if (debugFlag) {
        llvm::outs() << "\n Instruction ID: " << I->getInstructionId() << " Count = " << Function_Pointee.size();
    }

#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return Function_Pointee;
}

B IPLFCPA::computeInFromOutForIndirectCalls(BaseInstruction *I) {
    /*  if (debugFlag)
        llvm::outs()
            << "\n Inside
       computeInFromOutForIndirectCalls...................";*/
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    Instruction *Inst = I->getLLVMInstruction();
    // B OUTofInst = getBackwardComponentAtOutOfThisInstruction(I);   //#TESTING
    B INofInst; // = OUTofInst;
    CallInstruction *cTempInst = new CallInstruction(Inst);
    /*if (debugFlag)
      llvm::outs() << "\n call operand : "
                   << cTempInst->getIndirectCallOperand()->getOnlyName();*/
    // INofInst.insert(cTempInst->getIndirectCallOperand()); modAR::LHF
    INofInst = LFLivenessSet::create_from_single(cTempInst->getIndirectCallOperand());

#ifdef PRINTUSEPOINT
    llvm::outs() << "\n PRINT USE POINT 2 ID=" << I->getInstructionId();
    setUsePoint(I->getInstructionId(), cTempInst->getIndirectCallOperand());
#endif

#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return INofInst;
}

// bool IPLFCPA::checkInBackwardsVector(SLIMOperand *op, long long id, LFInstLivenessSet Vector) {
//     return true;
//     //(x, id3)    {(x,{id1,id2}), (y),(z)}
//     /*if (debugFlag) {
//      llvm::outs() << "\n Inside checkInBackwardsVector..........";
//      ///llvm::outs() << "\n Operand = "<<op->getOnlyName();
//      ///llvm::outs() << "\n Printing Vector.....";
//      //printDataFlowValuesBackward(Vector);
//     }

//       for (auto lin : Vector) {
//         if (compareOperands(op, GET_KEY(lin))) {

//            for (auto i : GET_VALUE(lin)) {
//                if (id == i)
//                  return true;
//            }
//         }
//       }
//       return false;*/
// }

// LFInstLivenessSet IPLFCPA::addInLIN(SLIMOperand *op, long long id, LFInstLivenessSet currLin) {

//     //(x, id3)    {(x,{id1,id2}), (y),(z)} ===> {(x,{id1,id2, id3})}
//     /// llvm::outs() << "\n Inside addInLin.............";

//     /*  std::set<long long> setTempId;
//       for (auto lin : currLin) {
//         if (compareOperands(op, lin.first)) {
//           setTempId = lin.second;
//         }
//         else
//           newLin.insert(lin);
//       }
//       setTempId.insert(id);
//       newLin.insert(make_pair(op, setTempId));*/

// #ifdef PRINTUSEPOINT
//     llvm::outs() << "\n PRINT USE POINT 3 ID=" << id;
//     setUsePoint(id, op);
// #endif
//     // return newLin;
//     return currLin.insert_inst(op, id); // modAR::LHF
// }

std::pair<SLIMOperand *, int> IPLFCPA::getReferenceWithIndir(SLIMOperand *op, int level) {
    if (debugFlag) {
        llvm::outs() << "\n Inside getReferenceWithIndir..........";
        llvm::outs() << "\n Input operand =  " << op->getOnlyName() << "\n input level = " << level;
    }
    SLIMOperand *srcOperand;
    int in_level;

    std::set<std::pair<SLIMOperand *, int> *> setSourceVar = op->getRefersToSourceValuesIndirection();
    if (setSourceVar.empty()) {
        // operand is source itself
        if (debugFlag)
            llvm::outs() << "\n Set of source variables is empty";

        //@ If source variable is not found then we return the same operand then
        // this becomes the same case as the ELSE loop so reduce level-1
        return make_pair(op, level - 1);

        /// return make_pair(op, level);
    } else {
        // check if same operand is returned. This will happen if GEP is
        // encountered during source computation
        for (auto ref : setSourceVar) {
            srcOperand = ref->first; // copy source
            in_level = ref->second;

            if (compareOperands(op, srcOperand)) {
                if (debugFlag)
                    llvm::outs() << "\n Same operand is returned: srcOperand =" << srcOperand->getOnlyName()
                                 << "\n in_level = " << in_level << " indirection = " << level;
                return make_pair(op, level - 1); // same operand if gep   WHY level-1????
                                                 /// return make_pair(op, level);

            } else {
                if (debugFlag)
                    llvm::outs() << "\n Source variable computed."
                                 << "\n Printing srcOperand = " << srcOperand->getOnlyName()
                                 << "\n in_level = " << in_level;
                return make_pair(srcOperand, in_level); // actual source
            }
        } // for
    } // else

    llvm_unreachable("Encountered unreachable case for getReferenceWithIndir");
}

B IPLFCPA::generateLivenessOfSource(
    B INofInst, F forwardIN, std::set<std::pair<SLIMOperand *, int> *> setSourceVar, BaseInstruction *ins) {
    if (debugFlag)
        llvm::outs() << "\n Inside generateLivenessOfSource..........";

    std::set<SLIMOperand *> setSrcPointee;
    std::queue<SLIMOperand *> q;
    SLIMOperand *srcOperand;
    int in_level;

    for (auto ref : setSourceVar) {
        srcOperand = ref->first; // copy source
        in_level = ref->second;

        if (debugFlag) {
            llvm::outs() << "\n #Testing the source and reference levels:";
            llvm::outs() << "\n source var = " << srcOperand->getOnlyName() << "\n Dereference level = " << in_level;
        }
    }

    if (srcOperand->isArrayElement() and srcOperand->getOperandType() != CONSTANT_POINTER_NULL) {
        if (debugFlag)
            llvm::outs() << "\n Referenced variable is an array....";
        handleArrayElementBackward(srcOperand, ins);
    } else {
        if (debugFlag)
            llvm::outs() << "\n Referenced variable is NOT an array....";

        if (!srcOperand->isAlloca()) {

            //@ Addition the check if in_level = -1
            if (in_level == -1) {
                if (debugFlag)
                    llvm::outs() << "\n In_level is -1. Simply generate "
                                    "liveness of srcOperand....";

                if (srcOperand->getOperandType() != CONSTANT_POINTER_NULL or
                    srcOperand->getOperandType() != CONSTANT_INT) {
                    if (debugFlag)
                        llvm::outs() << "\n Inserting into INofInst = " << srcOperand->getOnlyName();
                    INofInst = INofInst.insert_single(srcOperand); // modAR::LHF
                } // fi
            } else {
                if (debugFlag)
                    llvm::outs() << "\n ELSE loop in_level not -1 so proceed "
                                    "normally....";
                q.push(srcOperand);
                SLIMOperand *marker = createDummyOperand(9);
                q.push(marker);

                while (in_level >= 0 and !q.empty()) { // CHECK >=0??  WHY??
                    // while (in_level > 0 and !q.empty()) {
                    SLIMOperand *listValuesTemp = q.front();
                    q.pop();

                    if (debugFlag)
                        llvm::outs() << "\n Popped from Q...listValuesTemp = " << listValuesTemp->getOnlyName();

                    if (listValuesTemp->getOperandType() != CONSTANT_POINTER_NULL) {
                        if (listValuesTemp->getOperandType() != CONSTANT_INT) {
                            if (listValuesTemp->isArrayElement() and !listValuesTemp->isAlloca()) {
                                if (debugFlag)
                                    llvm::outs() << "\n listValuesTemp is an "
                                                    "array and not alloca";
#ifndef IGNORE
                                handleArrayElementBackward(listValuesTemp, ins);
#endif
                            } else {

                                if (debugFlag)
                                    llvm::outs() << "\n listValuesTemp is an "
                                                    "NOT array and not alloca";

                                if (debugFlag)
                                    llvm::outs() << "\n Inserting into INofInst = " << listValuesTemp->getOnlyName();
                                INofInst = INofInst.insert_single(listValuesTemp); // modAR::LHF
                            } // end else

                            for (auto fIN : forwardIN) {
                                if (compareOperands(listValuesTemp, GET_PTSET_KEY(fIN))) {
                                    LFLivenessSet Pointee = GET_VALUE(fIN);
                                    for (auto p : Pointee) {
                                        q.push(GET_KEY(p));
                                        if (debugFlag)
                                            llvm::outs() << "\n Pointee pushed in Q = " << GET_KEY(p)->getOnlyName();
                                    } // inner for
                                    q.push(marker);
                                } // end outer if
                            } // end outer for
                        } // end if not constant_int
                        else {
                            if (debugFlag)
                                llvm::outs() << "\nEncountered Marker ----\n";
                            /* Following code updated for dummy=? on 28.2.2023
                             */
                            Value *vv = listValuesTemp->getValue();
                            ConstantInt *CI = dyn_cast<ConstantInt>(vv);
                            if (CI->getSExtValue() == 999999)
                                in_level--; // decrement only if marker and not
                                            // a ?
                        } // end else
                    } // end if not nullptr
                } // end while

            } // else in_level != -1
        } // fi not alloca
    } // else

    return INofInst;
}

B IPLFCPA::computeInFromOut(BaseInstruction *I) {
    if (debugFlag) {
        llvm::outs() << "\n Inside computeINfromOUT.........";
        llvm::outs() << "\n Computing for instruction ";
        I->printInstruction();
        llvm::outs() << "\n";
        llvm::outs() << "\n Instruction Type is: " << I->getInstructionType();
    }
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    B INofInst, OUTofInst, INofInstPrev;

    int src_var_indirection = 0;

    // Fetch the currLOUT of instr I
    OUTofInst = getBackwardComponentAtOutOfThisInstruction(I);
    if (debugFlag) {
        llvm::outs() << "\n OUT of instruction ";
        printDataFlowValuesBackward(OUTofInst);
    }
    // Fetch the currPIN of instr I
    LFPointsToSet forwardIN = getForwardComponentAtInOfThisInstruction(I);

    // Copy LOUT into LIN
    // for (auto outContents : OUTofInst)
    //   INofInst.insert(outContents);

    INofInst = OUTofInst;

    bool flagLhsLive = false;
    std::pair<SLIMOperand *, int> LHS = I->getLHS();
    std::vector<std::pair<SLIMOperand *, int>> RHS = I->getRHS();
    int indirLhs = LHS.second;

    // Instr is a use
    if (I->getIsDynamicAllocation()) {
        if (debugFlag)
            llvm::outs() << "\n Instruction is a dynamic memory allocation. ";
        bool lvFlag = false;

        for (auto out : OUTofInst) {
            if (compareOperands(LHS.first, GET_KEY(out)))
                lvFlag = true;
        } // for

        if (lvFlag)
            INofInst = eraseFromLin(LHS.first, OUTofInst);
#ifdef PRINTSTATS
        stat.timerEnd(__func__);
#endif
        return INofInst;
    } else if (I->getInstructionType() == RETURN) {
        if (debugFlag) {
            llvm::outs() << "\n Instruction is a return";
        }
        ReturnInstruction *RI = (ReturnInstruction *)I;
        SLIMOperand *RHSval = RI->getReturnOperand();
        if (RHSval->getValue() == nullptr) {
            if (debugFlag)
                llvm::outs() << "\n Return operand is NULL\n";
        } else {
            RHSval->setVariableGlobal(); /// marked global for return value
                                         /// mapping
            if (RHSval->getOperandType() != CONSTANT_INT and RHSval->getOperandType() != CONSTANT_FP and
                RHSval->isPointerInLLVM()) {
                if (RHSval->isArrayElement()) {
                    if (debugFlag)
                        llvm::outs() << "\n RHSval is an array = " << RHSval->getOnlyName();
                    handleArrayElementBackward(RHSval, I);
                } else { // not an array

                    if (debugFlag)
                        llvm::outs() << "\n Rhsval: " << RHSval->getOnlyName() << " is a not an array.#Nosource";
                    INofInst = INofInst.insert_single(RHSval); // insert references into LIN

#ifdef PRINTUSEPOINT
                    llvm::outs() << "\n PRINT USE POINT 4: ID=" << I->getInstructionId();
                    setUsePoint(I->getInstructionId(), RHSval);
#endif
                } // else not array
            } // fi
        } // end else
#ifdef PRINTSTATS
        stat.timerEnd(__func__);
#endif
        return INofInst;
    } // end return
    else if (I->getInstructionType() == COMPARE) {
        if (debugFlag)
            llvm::outs() << "\n Instruction is a compare";

        bool flagInsert = true;
        for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end(); r++) {
            SLIMOperand *rhsValue = r->first;
            if (!llvm::isa<llvm::Constant>(rhsValue->getValue()) and rhsValue->isPointerInLLVM()) {

                if (rhsValue->isArrayElement()) {
                    if (debugFlag)
                        llvm::outs() << "\n rhsValue is an array = " << rhsValue->getOnlyName();
                    handleArrayElementBackward(rhsValue, I);
                } else { // not an array

                    if (!rhsValue->isAlloca()) {
                        if (debugFlag)
                            llvm::outs() << "\n rhsValue: " << rhsValue->getOnlyName()
                                         << " in compare is not an array.";
                        INofInst = INofInst.insert_single(rhsValue); // modAR::LHF

#ifdef PRINTUSEPOINT
                        llvm::outs() << "\n PRINT USE POINT 5 ID = " << I->getInstructionId();
                        setUsePoint(I->getInstructionId(), rhsValue);
#endif
                    } // fi not alloca
                } // end else not array
            } // end if
        } // end for

#ifdef PRINTSTATS
        stat.timerEnd(__func__);
#endif
        return INofInst;
    } // end compare
    else if (I->getInstructionType() == BRANCH) {
        if (debugFlag)
            llvm::outs() << "\n Instruction is BRANCH";
#ifdef PRINTSTATS
        stat.timerEnd(__func__);
#endif
        return INofInst;
    } else if (I->isIgnored()) {
        if (debugFlag)
            llvm::outs() << "\n Instruction is IGNORED";
        INofInst = eraseFromLin(LHS.first, OUTofInst);
#ifdef PRINTSTATS
        stat.timerEnd(__func__);
#endif
        return INofInst;
    }
    // TODO Add code for scanf (KILL)
    else if (I->getInstructionType() == PHI) {
        if (debugFlag)
            llvm::outs() << "Instruction is PHI \n";

        /* Lhs of Phi may have indices in Lout. These indices should not be used
         * for comparison. For ex-- %i67 = phi %struct.arc* [ %i48, %bb64 ], [
         * %i153, %bb76 ] %i92 = getelementptr inbounds %struct.arc,
         * %struct.arc* %i67, i64 0, i32 1 %i97 = getelementptr inbounds
         * %struct.arc, %struct.arc* %i67, i64 0, i32 7
         */

        /* Check if Lhs is live then generate liveness of Rhs1 and Rhs2 */
        bool lvFlag = false;
        B newINofInst = INofInst;

        for (auto out : OUTofInst) {
            if (compareOperands(LHS.first, GET_KEY(out))) {
                lvFlag = true;
                bool flgFound = false;
                if (LHS.first->isArrayElement()) {
                    // Do not remove following code. DEF point is set.
                    // handleArrayElementBAckward sets the use point
                    if (debugFlag)
                        llvm::outs() << "\n PHI lhs operand is an array = " << LHS.first->getOnlyName();
                    // check if array is in global data structure else make an
                    // entry
                    for (auto list : listFISArrayObjects) {
                        if ((compareOperands(list->fetchSLIMArrayOperand(), LHS.first))) {
                            flgFound = true; // array already in global data structure
                            list->setPtDef(I->getBasicBlock());
                        } // end if
                    } // end for

                    if (!flgFound) {
                        // array not found in the global str
                        FISArray *newArray = new FISArray(LHS.first);
                        newArray->setOnlyArrayName(LHS.first->getOnlyName());
                        newArray->setPtDef(I->getBasicBlock());
                        listFISArrayObjects.push_back(newArray); // insert into global data str
                    } // end if
                } else { // not an array
                    newINofInst = eraseFromLin(LHS.first, OUTofInst);
                }
            }
        }
        INofInst = newINofInst;
        if (lvFlag) {

            if (debugFlag)
                llvm::outs() << "\n Lhs is live. Hence generate liveness of Rhs. ";

            for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end(); r++) {
                SLIMOperand *rhsVal = r->first;
                // llvm::outs() << "r.second PHI" << r->second << "\n";
                if (rhsVal->getOperandType() == GEP_OPERATOR and rhsVal->isPointerVariable()) {
                    if (rhsVal->isArrayElement()) {
                        if (debugFlag)
                            llvm::outs() << "\n RHS is an array here = " << rhsVal->getOnlyName();
                        handleArrayElementBackward(rhsVal, I);
                    } // end if array
                    else { // not array

                        if (debugFlag)
                            llvm::outs() << "\n ELSE loop rhs is not an array =" << rhsVal->getOnlyName();
                        INofInst = INofInst.insert_single(rhsVal); // modAR::LHF
#ifdef PRINTUSEPOINT
                        llvm::outs() << "\n PRINT USE POINT 6 ID = " << I->getInstructionId();
                        setUsePoint(I->getInstructionId(), rhsVal);
#endif
                    } // end else
                } // fi
                else if (r->second != 0) {
                    if (rhsVal->isArrayElement()) {

                        if (debugFlag)
                            llvm::outs() << "\n RHS 123 is an array = " << rhsVal->getOnlyName();
                        handleArrayElementBackward(rhsVal, I);
                    } // end if array
                    else {
                        if (debugFlag)
                            llvm::outs() << "\n PHI operand not an array = " << rhsVal->getOnlyName();

                        INofInst = INofInst.insert_single(rhsVal); // modAR::LHF
#ifdef PRINTUSEPOINT
                        llvm::outs() << "\n PRINT USE POINT 7 ID = " << I->getInstructionId();
                        setUsePoint(I->getInstructionId(), rhsVal);
#endif
                    } // end else not array
                } // end else indir !=0
            } // end for
        } // end if
    } // end phi
    else if (I->getInstructionType() == LOAD and !LHS.first->isPointerInLLVM()) {
        // LOAD to a scalar. Check is Rhs is a pointer
        for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end(); r++) {
            bool flg = false;
            SLIMOperand *rhsVal = r->first;
            if (debugFlag) {
                llvm::outs() << "\n LOAD instruction with LHS not a pointer.";
                llvm::outs() << "\n RHS name: " << rhsVal->getOnlyName();
                llvm::outs() << "\n Rhs is pointerLLVM: " << rhsVal->isPointerInLLVM();
            }
            if (rhsVal->isPointerInLLVM() and !rhsVal->isAlloca()) {
                if (debugFlag)
                    llvm::outs() << "\n Rhs is a pointer and not alloca";
                if (rhsVal->isArrayElement()) {
                    if (debugFlag)
                        llvm::outs() << "\n rhs is array = " << rhsVal->getOnlyName();
                    handleArrayElementBackward(rhsVal, I);
                } else { // not array
                    if (debugFlag)
                        llvm::outs() << "\n Rhs not an array: " << rhsVal->getOnlyName();

               // SOURCE is not defined
                    if (debugFlag)
                        llvm::outs() << "\n rhsVal is not an array.  " << rhsVal->getOnlyName();
                    INofInst = INofInst.insert_single(rhsVal); // modAR::LHF

#ifdef PRINTUSEPOINT
                    llvm::outs() << "\n PRINT USE POINT 8 ID = " << I->getInstructionId();
                    setUsePoint(I->getInstructionId(), rhsVal);
#endif
                } // else not array
            } // fi pointer and not alloca
        } // end for
        // Kill liveness of LHS
        if (!LHS.first->isArrayElement())
            if (debugFlag)
                llvm::outs() << "\n LHS is not array so erase from LIN = " << LHS.first->getOnlyName();

        INofInst = eraseFromLin(LHS.first, INofInst);
    } else if (I->getInstructionType() == STORE and !LHS.first->isPointerInLLVM()) {
        // STORE to a scalar. Check is Rhs is a pointer
        for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end(); r++) {
            bool flg = false;
            SLIMOperand *rhsVal = r->first;
            if (debugFlag) {
                llvm::outs() << "\n STORE instruction with LHS not a pointer.";
                llvm::outs() << "\n RHS name: " << rhsVal->getOnlyName();
                llvm::outs() << "\n Rhs ispointerLLVM: " << rhsVal->isPointerInLLVM();
            }

            if (rhsVal->isPointerInLLVM() and !rhsVal->isAlloca()) {
                if (rhsVal->isArrayElement()) { // dont insert if array name
                                                // already present in LIN
                    if (debugFlag)
                        llvm::outs() << "\n rhsValue is array = " << rhsVal->getOnlyName();
                    handleArrayElementBackward(rhsVal, I);
                } else { // not an array

                    if (debugFlag)
                        llvm::outs() << "\n rhsVal is a GEP OP (NOSOURCE)= " << rhsVal->getOnlyName();
                    INofInst = INofInst.insert_single(rhsVal); // modAR::LHF

#ifdef PRINTUSEPOINT
                    llvm::outs() << "\n PRINT USE POINT 9 ID = " << I->getInstructionId();
                    setUsePoint(I->getInstructionId(), rhsVal);
#endif
                } // else not array
            } // fi
        } // for
        // Kill liveness of LHS
        if (!LHS.first->isArrayElement())
            if (debugFlag)
                llvm::outs() << "\n LHS.first is not an array so erase from LIN = " << LHS.first->getOnlyName();
        INofInst = eraseFromLin(LHS.first, INofInst);
    } else {
        if (debugFlag)
            llvm::outs() << "\n NORMAL INSTRUCTION.......";
        // Normal Instruction
        if (debugFlag) {
            llvm::outs() << "Printing to check if indices are present in LHS ";
            printIndices(LHS.first);
            printIndices(RHS[0].first);
            llvm::outs() << "Instruction type " << I->getInstructionType() << "\n";
            llvm::outs() << "LHS Operand type " << LHS.first->getOperandType() << "\n";
            llvm::outs() << "\n Is LHS global: " << LHS.first->isVariableGlobal();
            llvm::outs() << "\n Computing LIN for a normal instruction. ";
            llvm::outs() << "\n RHS Operand type " << RHS[0].first->getOperandType()
                         << "Rhs value= " << RHS[0].first->getValue();
        }

        bool cmpFlg = false;

        if (LHS.second == 1) { /* lhs: x=... or tmp=... */
            if (debugFlag)
                llvm::outs() << "\n LHS indir is 1: " << LHS.first->getOnlyName();

            std::pair<SLIMOperand *, int> chkRhs = RHS[0];

            if (LHS.first->isArrayElement()) {
                if (debugFlag) {
                    llvm::outs() << "\n LHS is an array. Ignore indices.";
                    llvm::outs() << "\n Lhs name = " << LHS.first->getOnlyName()
                                 << " \n LHS GEP MAIN = " << LHS.first->get_gep_main_operand();

                    llvm::outs() << "\n RHS name =" << chkRhs.first->getOnlyName()
                                 << "\n is rhs array = " << chkRhs.first->isArrayElement();
                }
                bool flgFound = false;
                // check if array is in global data structure else make an entry
                for (auto list : listFISArrayObjects) {
                    if ((compareOperands(list->fetchSLIMArrayOperand(), LHS.first))) {
                        flgFound = true; // array already in global data structure
                        list->setPtDef(I->getBasicBlock());
                        if (list->isPtUseNotEmpty()) { // Lhsarray is live. So
                                                       // generate liveness of
                                                       // Rhs
                            flagLhsLive = true;
                            break;
                        }
                    } // end if
                } // end for
                if (!flgFound) {
                    // array not found in the global str
                    FISArray *newArray = new FISArray(LHS.first);
                    newArray->setOnlyArrayName(LHS.first->getOnlyName());
                    newArray->setPtDef(I->getBasicBlock());
                    listFISArrayObjects.push_back(newArray); // insert into
                                                             // global data str
                } // end if
            } // end if array
            else {
                // Lhs is not an array. Or indices not considered to check
                // liveness for GEP.
                if (debugFlag)
                    llvm::outs() << "\n Lhs is not an array.";
                for (auto out : OUTofInst) {
                    if (compareOperands(GET_KEY(out), LHS.first)) {
                        if (debugFlag)
                            llvm::outs() << "\n Lhs is found in LOUT. ";
                        cmpFlg = true;
                        break;
                    }
                }
                /* Erase LHS from LIN since it is defined here */
                if (cmpFlg) {
#ifdef PRINT
                    llvm::outs() << "\n LHS is live on exit. ";
#endif
                    INofInst = eraseFromLin(LHS.first, INofInst);
                    flagLhsLive = true; // set the flag
                } // end cmpFlg
            } // end else not array
        } // end if inlhs=1
        else if (LHS.second == 2) {
            SLIMOperand *source;
            bool flgLinCheck = false;
            bool flgArrFound = false;
            bool flgFound = false;
            /* lhs: *tmp=... */
            if (debugFlag) {
                llvm::outs() << "\n LHS is (temp, 2) ";
                /* Insert LHS into Lin and RHS in Lin if pointee of LHS is in
                 * Lout */
                llvm::outs() << "\n Insert LHS into Lin and RHS in Lin if pointee of "
                                "LHS is in Lout.";
            }
            //@case1: *x=y and x is an array
            if (LHS.first->isArrayElement()) {
                if (debugFlag) {
                    llvm::outs() << "\n LHS_orig is an array...= " << LHS.first->getOnlyName()
                                 << " Value = " << LHS.first->getValue();
                }
                std::queue<SLIMOperand *> q;
                for (auto list : listFISArrayObjects) {
                    if ((compareOperands(list->fetchSLIMArrayOperand(), LHS.first))) {
                        if (debugFlag)
                            llvm::outs() << "\n Lhs is an array in global DS";
                        flgArrFound = true; // array already in global data structure
                        set<BasicBlock *> tempSet = list->getPtUse();
                        auto pos = tempSet.find(I->getBasicBlock());
                        if (pos == tempSet.end()) {
                            list->setPtUse(I->getBasicBlock());
                            list->setFlgChangeInUse(true);
                        }

                        // Fetch pointees of the LhsArray
                        LFLivenessSet lhsArrPointees = list->getArrayPointees();
                        bool flgPointeeFound = false;
                        for (auto p : lhsArrPointees) {
                            if (GET_KEY(p)->isArrayElement()) {
                                if (debugFlag)
                                    llvm::outs() << "\n LHS pointee is array =" << GET_KEY(p)->getOnlyName();

                                for (auto list : listFISArrayObjects) {
                                    if ((compareOperands(list->fetchSLIMArrayOperand(), GET_KEY(p)))) {
                                        flgPointeeFound = true; // array already in global
                                                                // data structure
                                        list->setPtDef(I->getBasicBlock());
                                        flagLhsLive = true; // set the flag
                                        flgFound = true;
                                    } // end if
                                } // end for
                                if (!flgPointeeFound) {
                                    if (debugFlag)
                                        llvm::outs() << "\n Lhs Pointee is an array and "
                                                        "not found in DS";
                                    // array not found in the global str
                                    FISArray *newArray = new FISArray(GET_KEY(p));
                                    newArray->setOnlyArrayName(GET_KEY(p)->getOnlyName());
                                    newArray->setPtDef(I->getBasicBlock());
                                    listFISArrayObjects.push_back(newArray); // insert into global data
                                                                             // str
                                } // end if
                            } // end if pointee array
                            else {
                                // LhsArray pointee is not an array
                                // (arr[0],2)=... arr[0]->x
                                q.push(GET_KEY(p));
                            } // end else pointee not arr
                        } // end for

                        /* Check if the pointees are live in LOUT to generate
                         * the liveness of RHS */
                        while (!q.empty()) {
                            SLIMOperand *lhsPointee = q.front();
                            q.pop();
                            for (auto bOUT : OUTofInst) {
                                if (compareOperands(GET_KEY(bOUT), lhsPointee)) {
                                    flagLhsLive = true; // set the flag
                                    flgFound = true;
                                } // end if
                            } // end for
                        } // end while
                    } // end if
                } // end for

                if (!flgArrFound) {
                    if (debugFlag)
                        llvm::outs() << "\n Lhs not found in DS";
                    // array not found in the global str
                    FISArray *newArray = new FISArray(LHS.first);
                    newArray->setOnlyArrayName(LHS.first->getOnlyName());
                    newArray->setPtUse(I->getBasicBlock());
                    newArray->setFlgChangeInUse(true);
                    listFISArrayObjects.push_back(newArray); // insert into global data str
                } // end if
            } // end if array
            else if (!LHS.first->isAlloca() and LHS.first->isPointerVariable()) {
                if (debugFlag)
                    llvm::outs() << "\n Lhs_orig indir =2 and LHS is not "
                                    "alloca and not an array";

                    //@case2 : *x = y and x is not an array but a pointer
                source = LHS.first;

                //@subcases: After computing the source check if the source is
                // an array
                /* Following code should execute for both SOURCE and NOSOURCE*/
                if (!source->isAlloca()) {
                    // #Check if source is an array
                    if (source->isArrayElement()) {
#ifndef IGNORE
                        if (debugFlag)
                            llvm::outs() << "\n LHS Source is an array...= " << source->getOnlyName();
                        std::queue<SLIMOperand *> q;
                        for (auto list : listFISArrayObjects) {
                            if ((compareOperands(list->fetchSLIMArrayOperand(), source))) {
                                flgArrFound = true; // array already in global
                                                    // data structure
                                set<BasicBlock *> tempSet = list->getPtUse();
                                auto pos = tempSet.find(I->getBasicBlock());
                                if (pos == tempSet.end()) {
                                    list->setPtUse(I->getBasicBlock());
                                    list->setFlgChangeInUse(true);
                                }
                                // Fetch pointees of the LhsArray
                                LFLivenessSet lhsArrPointees = list->getArrayPointees();
                                bool flgPointeeFound = false;
                                for (auto p : lhsArrPointees) {
                                    if (GET_KEY(p)->isArrayElement()) {
                                        if (debugFlag)
                                            llvm::outs() << "\n LHS pointee is array = " << GET_KEY(p)->getOnlyName();

                                        for (auto list : listFISArrayObjects) {
                                            if ((compareOperands(list->fetchSLIMArrayOperand(), GET_KEY(p)))) {
                                                flgPointeeFound = true; // array already in
                                                                        // global data
                                                                        // structure
                                                list->setPtDef(I->getBasicBlock());
                                                flagLhsLive = true; // set the flag
                                                flgFound = true;
                                            } // end if
                                        } // end for
                                        if (!flgPointeeFound) {
                                            if (debugFlag)
                                                llvm::outs() << "\n Lhs Pointee is an "
                                                                "array and not found in "
                                                                "DS";
                                            // array not found in the global str
                                            FISArray *newArray = new FISArray(GET_KEY(p));
                                            newArray->setOnlyArrayName(GET_KEY(p)->getOnlyName());
                                            newArray->setPtDef(I->getBasicBlock());
                                            listFISArrayObjects.push_back(newArray); // insert into global
                                                                                     // data str
                                        } // end if
                                    } // end if pointee array
                                    else {
                                        // LhsArray pointee is not an array
                                        // (arr[0],2)=... arr[0]->x
                                        q.push(GET_KEY(p));
                                    } // end else pointee not arr
                                } // end for

                                /* Check if the pointees are live in LOUT to
                                 * generate the liveness of RHS */
                                while (!q.empty()) {
                                    SLIMOperand *lhsPointee = q.front();
                                    q.pop();
                                    for (auto bOUT : OUTofInst) {
                                        if (compareOperands(GET_KEY(bOUT), lhsPointee)) {
                                            flagLhsLive = true; // set the flag
                                            flgFound = true;
                                        } // end if
                                    } // end for
                                } // end while
                            } // end if
                        } // end for

                        if (!flgArrFound) {
                            if (debugFlag)
                                llvm::outs() << "\n Lhs not found in DS";
                            // array not found in the global str
                            FISArray *newArray = new FISArray(source);
                            newArray->setOnlyArrayName(source->getOnlyName());
                            newArray->setPtUse(I->getBasicBlock());
                            newArray->setFlgChangeInUse(true);
                            listFISArrayObjects.push_back(newArray); // insert into global data str
                        } // end if

#endif
                    } // end if array
                    else {
                        /* Source is not an array */
                        if (debugFlag)
                            llvm::outs() << "\n Lhs with indir = 2 not an array = " << source->getOnlyName();

#ifdef PRINTUSEPOINT
                        llvm::outs() << "\n PRINT USE POINT 10 ID = " << I->getInstructionId();
                        setUsePoint(I->getInstructionId(), source);
#endif

                        // @Redundant code
                        if (!source->isArrayElement())
                            INofInst = INofInst.insert_single(source);

                        LFLivenessSet srcPointees;
                        bool chkFlag = false;
                        for (auto fIN : forwardIN) {
                            if (compareOperands(source, GET_PTSET_KEY(fIN))) {
                                srcPointees = GET_VALUE(fIN);
                                chkFlag = true; // Pointees of source present in PIN
                            }
                        }

                        if (!chkFlag) {
                            if (debugFlag)
                                llvm::outs() << "\n Pointees of source NOT PIN. "
                                                "Blocking "
                                                "propagation of liveness information.";
                            LFLivenessSet INofInst;                    // Block propagation of liveness info
                            INofInst = INofInst.insert_single(source); // modAR::LHF
                                                                       // INofInst.insert(source);
#ifdef PRINTSTATS
                            stat.timerEnd(__func__);
#endif
                            return INofInst;
                        }

                        INofInstPrev = INofInst;
                        bool flgFound = false;
                        bool flgMustPointee = false;

                        if (srcPointees.size() == 1) {
                            if (debugFlag)
                                errs() << "\nSet the MUST pointee flag\n";
                            flgMustPointee = true;
                        }

                        for (auto lhsPointee : srcPointees) {
                            if (GET_KEY(lhsPointee)->isArrayElement()) {
#ifndef IGNORE
                                if (debugFlag)
                                    llvm::outs() << "\n LHS pointee is an array." << GET_KEY(lhsPointee)->getOnlyName();

                                bool flgArrFound = false;
                                for (auto list : listFISArrayObjects) {
                                    if ((compareOperands(list->fetchSLIMArrayOperand(), GET_KEY(lhsPointee)))) {
                                        flgArrFound = true; // array already in global
                                                            // data structure
                                        list->setPtDef(I->getBasicBlock());
                                        if (list->isPtUseNotEmpty()) { // Lhsarray
                                                                       // is
                                                                       // live.
                                                                       // So
                                                                       // generate
                                                                       // liveness
                                                                       // of Rhs
                                            flagLhsLive = true;
                                            flgFound = true;
                                        }
                                    } // end if
                                } // end for

                                if (!flgArrFound) {
                                    // array not found in the global str
                                    FISArray *newArray = new FISArray(GET_KEY(lhsPointee));
                                    newArray->setOnlyArrayName(GET_KEY(lhsPointee)->getOnlyName());
                                    newArray->setPtDef(I->getBasicBlock());
                                    listFISArrayObjects.push_back(newArray); // insert into global data
                                                                             // str
                                } // end if
#endif
                            } // end if lhsPOintee is array

                            for (auto bOUT : OUTofInst) {
                                /* if (debugFlag) {
                                   llvm::outs() << "\n LHS POINTEE NOT AN ARRAY
                                 2"; llvm::outs() << "\n bOUT =
                                 "<<bOUT->getOnlyName(); llvm::outs() << "\n
                                 lhsPointee = "<<lhsPointee->getOnlyName();
                                 }*/
                                if (compareOperands(GET_KEY(bOUT), GET_KEY(lhsPointee))) {
                                    if (debugFlag)
                                        llvm::outs() << "\n Pointee live at "
                                                        "LOUT.............\n";

                                    if (flgMustPointee) { // 2.9.22 *x = y
                                        if (debugFlag)
                                            errs() << "\n Lhs Pointee is a "
                                                      "MUST pointee";
                                        INofInst = eraseFromLin(GET_KEY(bOUT), INofInst);
                                    }
                                    flagLhsLive = true; // set the flag
                                    flgFound = true;
                                } // end if
                            } // end for bOUT

                            if (!flgFound) {
                                INofInst = INofInstPrev;
                                flagLhsLive = false;
                            }
                        } // end for pointees
                    } // end if  source not array
                } // if source not alloca
            } // if lhs is not alloca
        } // end if inlhs=2

        if (flagLhsLive) { /* Insert Rhs into Lin */
            if (debugFlag) {
                llvm::outs() << "\n LHS is Live so insert RHS into LIN.  ";
            }
            INofInst = insertRhsLin(INofInst, RHS, forwardIN, I);
        } // end if flag
    } // end normal else

    if (debugFlag)
        llvm::outs() << "\n Reached end of the function............";
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return INofInst;
}

/// Erases liveness information from IN
B IPLFCPA::eraseFromLin(SLIMOperand *pointee, B INofInst) {
    // llvm::outs() << "\n Inside function eraseFromLin..........";
    return INofInst.remove_single(pointee); // modAR::LHF
}

/// Inserts the liveness information of the RHS into the IN
B IPLFCPA::insertRhsLin(
    B currentIN, std::vector<std::pair<SLIMOperand *, int>> rhslist, LFPointsToSet forwardIN, BaseInstruction *ins) {
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    B INofInst = currentIN;

    if (debugFlag) {
        llvm::outs() << "\n Printing ForwardIn for this instr ";
        printDataFlowValuesForward(forwardIN);
        llvm::outs() << "\n Printing currentLIN of this instr ...";
        printDataFlowValuesBackward(currentIN);
    }
    for (auto listValues : rhslist) {
        int listValuesIndir = listValues.second;

        if (listValuesIndir == 0) { /* Rhs: ...=&u */
            if (debugFlag)
                llvm::outs() << "\n Rhs indir = 0";
            InstructionType insType = ins->getInstructionType();
            if (insType != GET_ELEMENT_PTR) {
                if (debugFlag)
                    llvm::outs() << "\n Rhs is ...= &y "; // Not added to LIN
#ifdef PRINTSTATS
                stat.timerEnd(__func__);
#endif
                return INofInst;
            } else {
                if (debugFlag)
                    llvm::outs() << "\n Rhs = &GEP. Instr is GEP so generate "
                                    "liveness of Rhs.\n";

                bool isArraySource = false;

                // llvm::outs() << "\n RHS Value =
                // "<<listValues.first->getOnlyName()
                //            << "\n Is RHS Value an array =
                //            "<<listValues.first->isArrayElement();
                if (listValues.first != nullptr) {
                    if (listValues.first->isArrayElement()) { // dont insert if array name
                        if (debugFlag)
                            llvm::outs() << "\n Rhs listValues.first is an array...= "
                                         << listValues.first->getOnlyName();
#ifndef IGNORE
                        handleArrayElementBackward(listValues.first, ins);
#endif
                    } else if (
                        listValues.first->isPointerInLLVM() and
                        listValues.first->getOperandType() != CONSTANT_POINTER_NULL and !listValues.first->isAlloca()) {

                        if (debugFlag)
                            llvm::outs() << "\n Rhs not an array. listValues.first :: "
                                         << listValues.first->getOnlyName();

                        if (listValues.first->getOperandType() != CONSTANT_POINTER_NULL)
                            INofInst = INofInst.insert_single(listValues.first); // modAR::LHF

#ifdef PRINTUSEPOINT
                        llvm::outs() << "\n PRINT USE POINT 11 ID =" << ins->getInstructionId();
                        setUsePoint(ins->getInstructionId(), listValues.first);
#endif
                    } // outer if loop
                } // not nullptr
#ifdef PRINTSTATS
                stat.timerEnd(__func__);
#endif
                return INofInst;
            }
        } // end if indir=0
        else if (listValuesIndir == 1) { /* Rhs: ...=y */
            if (debugFlag)
                llvm::outs() << "\n Rhs is ...=y ";

            bool flg = false;
            if (listValues.first->isArrayElement()) { // dont insert if array name
                                                      // already present in LIN
                if (debugFlag)
                    llvm::outs() << "\n Rhs is an array...= " << listValues.first->getOnlyName();
#ifndef IGNORE
                handleArrayElementBackward(listValues.first, ins);
#endif
            } else { // RHS is not an array
                bool isArraySource = false;
                if ((listValues.first->getOperandType() != CONSTANT_INT) && !listValues.first->isAlloca()) {
                    INofInst = INofInst.insert_single(listValues.first); // modAR::LHF

#ifdef PRINTUSEPOINT
                    llvm::outs() << "\n PRINT USE POINT 12-AB ID = " << ins->getInstructionId();
                    setUsePoint(ins->getInstructionId(), listValues.first);
#endif

                } // outer fi
            } // end else not array
#ifdef PRINTSTATS
            stat.timerEnd(__func__);
#endif
            return INofInst;
        } else if (listValuesIndir == 2) { /* Rhs: ...=*y */
            if (debugFlag) {
                llvm::outs() << "\n Rhs indirec = 2";
                llvm::outs() << "\n Rhs name: " << listValues.first->getOnlyName()
                             << "\n is global: " << listValues.first->isVariableGlobal()
                             << "\n is array: " << listValues.first->isArrayElement();
            }

            SLIMOperand *RHS_orig = listValues.first;
            //@case: x=*y and y is an array
            bool flgArrFound = false;
            if (listValues.first->isArrayElement() and !listValues.first->isAlloca()) {

                std::queue<SLIMOperand *> q;
                for (auto list : listFISArrayObjects) {
                    if ((compareOperands(list->fetchSLIMArrayOperand(), listValues.first))) {
                        flgArrFound = true; // array already in global data structure
                        set<BasicBlock *> tempSet = list->getPtUse();
                        auto pos = tempSet.find(ins->getBasicBlock());
                        if (pos == tempSet.end()) {
                            list->setPtUse(ins->getBasicBlock());
                            list->setFlgChangeInUse(true);
                        }

                        // fetch pointees of Rhs
                        LFLivenessSet rhsArrPointees = list->getArrayPointees();
                        bool flgPointeeFound = false;
                        for (auto p : rhsArrPointees) {
                            if (debugFlag) {
                                llvm::outs() << "\n #TESTING: Rhs=>array. RhsPointee: " << GET_KEY(p)->getOnlyName();
                                printIndices(GET_KEY(p));
                                llvm::outs() << "\n Rhs pointee : array? " << GET_KEY(p)->isArrayElement();
                                llvm::outs() << "\n Rhs pointee : alloca? " << GET_KEY(p)->isAlloca();
                            }
                            if (GET_KEY(p)->isArrayElement() /*and !p->isAlloca()*/) { // rhs pointee is an array and
                                                                                       // not an alloca
                                if (debugFlag)
                                    llvm::outs() << "\n Rhs pointee is an array.." << GET_KEY(p)->getOnlyName();

#ifndef IGNORE
                                for (auto list : listFISArrayObjects) {
                                    if ((compareOperands(list->fetchSLIMArrayOperand(), GET_KEY(p)))) {
                                        flgPointeeFound = true; // array already in global
                                                                // data structure
                                        set<BasicBlock *> tempSet = list->getPtUse();
                                        auto pos = tempSet.find(ins->getBasicBlock());
                                        if (pos == tempSet.end()) {
                                            list->setPtUse(ins->getBasicBlock());
                                            list->setFlgChangeInUse(true);
                                        }
                                    } // end if
                                } // end for

                                if (!flgPointeeFound) {
                                    if (debugFlag)
                                        llvm::outs() << "\n Rhs pointee not "
                                                        "found in global list";
                                    FISArray *newArray = new FISArray(GET_KEY(p));
                                    newArray->setOnlyArrayName(GET_KEY(p)->getOnlyName());
                                    newArray->setPtUse(ins->getBasicBlock());
                                    newArray->setFlgChangeInUse(true);
                                    listFISArrayObjects.push_back(newArray); // insert into global data
                                                                             // str
                                } // end if
#endif
                            } // end if pointee array
                            else {
                                // llvm::outs() << "\n ELSE LOOP. Rhs pointee
                                // not an array"; RhsArray pointee is not an
                                // array ...=(arr[0],2) arr[0]->x
                                if (GET_KEY(p)->getOperandType() != CONSTANT_INT /*and !p->isAlloca()*/) {
                                    bool flgLinCheck = false;
                                    /* for (auto rep : INofInst) {
                                       if (compareOperands(rep, p))
                                         flgLinCheck = true;
                                     }
                                     if (!flgLinCheck) {
                                        //  INofInst.insert(p);*/
                                    INofInst = INofInst.insert_single(GET_KEY(p)); // modAR::LHF

#ifdef PRINTUSEPOINT
                                    llvm::outs() << "\n PRINT USE POINT 14 ID = " << ins->getInstructionId();
                                    setUsePoint(ins->getInstructionId(), p);
#endif

                                    /*
                                         arr[0]->t1->u where t1 is a temporary.
                                       Such case will never arise.
                                    */
                                } // constant_int
                            } // end else pointee not arr
                        } // end for rhsArrPointees
                    } // end if compare
                } // end for list

                if (!flgArrFound) {
                    // array not found in the global str
                    if (debugFlag)
                        llvm::outs() << "\n RHS array not found in DS";
                    if (!listValues.first->isAlloca()) {
                        FISArray *newArray = new FISArray(listValues.first);
                        newArray->setOnlyArrayName(listValues.first->getOnlyName());
                        newArray->setPtUse(ins->getBasicBlock());
                        newArray->setFlgChangeInUse(true);
                        listFISArrayObjects.push_back(newArray); // insert into global data str
                    } // end if not alloca
                }
            } // end if arr
            else {
                if (debugFlag)
                    llvm::outs() << "\n ELSE loop Rhs not an array...";

                bool flgSpecialCase = false;

                if (!listValues.first->isAlloca() and listValues.first->getOperandType() != CONSTANT_POINTER_NULL) {

                    std::queue<SLIMOperand *> q;
                    q.push(listValues.first);
                    SLIMOperand *marker = createDummyOperand(9);
                    q.push(marker);

                    while (listValuesIndir > 0 and !q.empty()) {
                        SLIMOperand *listValuesTemp = q.front();
                        q.pop();

                        if (listValuesTemp->getOperandType() != CONSTANT_POINTER_NULL) {
                            if (listValuesTemp->getOperandType() != CONSTANT_INT) {
                                if (listValuesTemp->isArrayElement() and !listValuesTemp->isAlloca()) {
                                    if (debugFlag)
                                        llvm::outs() << "\n listValuesTemp is an array "
                                                        "= "
                                                     << listValuesTemp->getOnlyName();

#ifndef IGNORE
                                    handleArrayElementBackward(listValuesTemp, ins);
#endif
                                } else {
                                    INofInst = INofInst.insert_single(listValuesTemp); // modAR::LHF

#ifdef PRINTUSEPOINT
                                    llvm::outs() << "\n PRINT USE POINT 15 ID = " << ins->getInstructionId();
                                    setUsePoint(ins->getInstructionId(), listValuesTemp);
#endif
                                } // end else

                                for (auto fIN : forwardIN) {
                                    /// llvm::outs() << "\n INSIDE FOR LOOP
                                    /// HERE....";
                                    if (compareOperands(listValuesTemp, GET_PTSET_KEY(fIN))) {
                                        LFLivenessSet Pointee = GET_VALUE(fIN); // B???
                                        // llvm::outs() << "\n CHECKPOINT-888";
                                        for (auto p : Pointee) {
                                            /// llvm::outs() << "\n #TESTING
                                            /// Printing pointeee value:: p =
                                            /// "<<p->getOnlyName();
                                            q.push(GET_KEY(p));
                                        } // inner for
                                        q.push(marker);
                                    } // end outer if
                                } // end outer for
                            } // end if not constant_int
                            else {
                                if (debugFlag)
                                    llvm::outs() << "\nEncountered Marker\n";
                                /* Following code updated for dummy=?
                                 * on 28.2.2023 */
                                Value *vv = listValuesTemp->getValue();
                                ConstantInt *CI = dyn_cast<ConstantInt>(vv);
                                if (CI->getSExtValue() == 999999)
                                    listValuesIndir--; // decrement only if
                                                       // marker and not a ?
                            } // end else
                        } // end if not nullptr
                    } // end while
                } // end if not alloca
            } // end else not arr
        } // end else if Rhs indir=2
    } // end for
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return INofInst;
}

B IPLFCPA::getPurelyLocalComponentBackward(const B &dfv) const {
    if (debugFlag)
        llvm::outs() << "\n Inside getPurelyLocalComponentBackward.............";
    return dfv.get_purely_local();
}

// ##################### FORWARD ###################################//
F IPLFCPA::getMainBoundaryInformationForward(BaseInstruction *I) {
    if (debugFlag)
        llvm::outs() << "\n Inside getMainBoundaryInformationForward";
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    B LIN = getBackwardComponentAtInOfThisInstruction(I);

    /* Below code initializes pointers live at Start_main to "?" */
    F PIN;
    SLIMOperand *qMark = createDummyOperand(0);

#ifdef PTA
    std::vector<SLIMOperand *> globalList;
    Function *F = I->getFunction();
    auto bb = F->begin();
    BasicBlock *bp = &(*bb);
    Module *M = bp->getModule();
    auto gList = &(M->getGlobalList());

    for (auto &global : *gList) {
        globalList.push_back(OperandRepository::getOrCreateSLIMOperand(&global));
    }

    if (debugFlag)
        llvm::outs() << "\n Global var list size = " << globalList.size();
    for (auto l : globalList) {
        if (!l->isArrayElement() and !l->isAlloca() and l->getOperandType() != CONSTANT_INT)) {
                if (l->isPointerInLLVM())
                    PIN = PIN.insert_pointee(l, qMark); // modAR::LHF
                // PIN[l].insert(qMark);
            }
    }
#endif

#ifndef PTA
    for (auto l : LIN) {
        if (!GET_KEY(l)->isArrayElement() and !GET_KEY(l)->isAlloca()) {
            if (GET_KEY(l)->isPointerInLLVM())
                PIN = PIN.insert_pointee(GET_KEY(l), qMark); // modAR::LHF
            // PIN[l].insert(qMark);
        }
    }
#endif
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return PIN;
}

F IPLFCPA::getBoundaryInformationForward() {
    if (debugFlag)
        llvm::outs() << "\n Inside getBoundaryInformationForward";
    F F_TOP;
    return F_TOP;
}

F IPLFCPA::getInitialisationValueForward() {
    if (debugFlag)
        ; // llvm::outs() << "\n Inside getInitialisationValueForward ";
    F F_TOP;
    return F_TOP;
}

bool IPLFCPA::EqualDataFlowValuesForward(const F &d1, const F &d2) const {
    // llvm::outs() << "\n Inside
    // EqualDataFlowValuesForward...............................";

    /*if (debugFlag) {
      llvm::outs() << "\n Prinitng d1.........";
      printDataFlowValuesForward(d1);
      llvm::outs() << "\n Prinitng d2.........";
      printDataFlowValuesForward(d2);
    }*/

    /*  if (d1.empty() and d2.empty())
        return true;
      else if ((d1.empty() and !d2.empty()) or (!d1.empty() and d2.empty()))
        return false;
      else if (!d1.empty() and !d2.empty()) {
        // Compare d1 with d2
        for (auto NEW : d1) {
          SLIMOperand *newPTR = NEW.first;
          std::set<SLIMOperand *> newPointee = NEW.second;
          bool change = true;
          /   llvm::outs() << "\n newPTR token: "<<newPTR->getOnlyName();
             llvm::outs() << "\n Index vector = ";
             printIndices(newPTR);/

          for (auto PREV : d2) {
            SLIMOperand *prevPTR = PREV.first;
            std::set<SLIMOperand *> prevPointee = PREV.second;

            / llvm::outs() << "\n prevPTR token: "<<prevPTR->getOnlyName();
             llvm::outs() << "\n Index vector = ";
             printIndices(prevPTR);/

            if (compareOperands(newPTR, prevPTR)) {
              if (newPointee.empty() and prevPointee.empty())
                change = false;
              else if ((!newPointee.empty() and prevPointee.empty()) or
                       (newPointee.empty() and !prevPointee.empty()))
                change = true;
              else {
                for (auto np : newPointee) {
                  change = true;
                  for (auto pp : prevPointee) {
                    // check for a "?"
                    if (compareOperands(np, pp))
                      change = false;
                    else if (np->getOperandType() == CONSTANT_POINTER_NULL and
                             pp->getOperandType() == CONSTANT_POINTER_NULL)
                      change = false;
                  } // end innner for
                  if (change)
                    return false; // not equal
                }                 // end for
              }                   // end else
            }                     // end outer if
          }                       // end inner for
          if (change)
            return false; // not equal
        }                 // end outer for
      }                   // end else

      return true;*/

    return d1 == d2; // modAR::LHF
}

bool IPLFCPA::EqualContextValuesForward(const F &d1, const F &d2) const {

    /* if (debugFlag) {
       llvm::outs() << "\n Inside EqualContextValuesForward...........";
       llvm::outs() << "\n Printing d1: ";
       printDataFlowValuesForward(d1);
       llvm::outs() << "\n Printing d2: ";
       printDataFlowValuesForward(d2);
     }*/

    /*  bool FLAG1 = true; // assume d1=d2
      bool FLAG2 = true; // assume d2=d1

      if (d1.empty() and d2.empty())
        return true;
      else if ((d1.empty() and !d2.empty()) or (!d1.empty() and d2.empty()))
        return false;
      else if (!d1.empty() and !d2.empty()) {
        // Compare d1 with d2
        for (auto NEW : d1) {
          SLIMOperand *newPTR = NEW.first;
          std::set<SLIMOperand *> newPointee = NEW.second;
          bool change = true;
        /  if (debugFlag) {
           llvm::outs() << "\n newPTR token: "<<newPTR->getOnlyName() << " index
    = "; printIndices(newPTR);
          }/
          for (auto PREV : d2) {
            SLIMOperand *prevPTR = PREV.first;
            std::set<SLIMOperand *> prevPointee = PREV.second;

           /if (debugFlag) {
           llvm::outs() << "\n prevPTR token: "<<prevPTR->getOnlyName() << "
    index = "; printIndices(prevPTR);
          }/
            // printIndices(prevPTR);
            if (compareOperands(newPTR, prevPTR)) {
             // llvm::outs() << "\n POINTERS MATCH. NOW CHECK POINTEES";
              if (newPointee.empty() and prevPointee.empty())
                change = false;
              else if ((!newPointee.empty() and prevPointee.empty()) or
                       (newPointee.empty() and !prevPointee.empty()))
                change = true;
              else {
                for (auto np : newPointee) {
                  change = true;

                  // llvm::outs() << "\nPrinting pointees \n";
                  // printIndices(np);
                  for (auto pp : prevPointee) {

                    // printIndices(pp);
                    // check for a "?"
                 /   if (debugFlag) {
                    llvm::outs() << "\n np: "<<np->getOnlyName() << " index = ";
                    printIndices(np);
                    llvm::outs() << "\n pp: "<<pp->getOnlyName() << " index = ";
                    printIndices(pp);
            }/

                    if (compareOperands(np, pp)) {
                      change = false;
                    }
                  } // end innner for
                  if (change)
                    FLAG1 = false; // not equal
                }                  // end for
              }                    // end else
            }                      // end outer if
          }                        // end inner for
          if (change)
            FLAG1 = false; // not equal

          // llvm::outs() <<
          // "\n--------------------------------------------------------\n";
        } // end outer for

        // Compare d2 with d1
        for (auto NEW : d2) {
          SLIMOperand *newPTR = NEW.first;
          std::set<SLIMOperand *> newPointee = NEW.second;
          bool change = true;
          // llvm::outs() << "\n newPTR token: "<<newPTR.first->getName()<<"
    field
          // indx: "<<newPTR.first->getFieldIndex();

          for (auto PREV : d1) {
            SLIMOperand *prevPTR = PREV.first;
            std::set<SLIMOperand *> prevPointee = PREV.second;

            if (compareOperands(newPTR, prevPTR)) {
              if (newPointee.empty() and prevPointee.empty())
                change = false;
              else if ((!newPointee.empty() and prevPointee.empty()) or
                       (newPointee.empty() and !prevPointee.empty()))
                change = true;
              else {
                for (auto np : newPointee) {
                  change = true;
                  for (auto pp : prevPointee) {
                    // check for a "?"
                    if (compareOperands(np, pp))
                      change = false;
                  } // end innner for
                  if (change)
                    FLAG2 = false; // not equal
                }                  // end for
              }                    // end else
            }                      // end outer if
          }                        // end inner for
          if (change)
            FLAG2 = false; // not equal
        }                  // end outer for
      }                    // end else

      if (FLAG1 and FLAG2) {
      /  if (debugFlag)
          llvm::outs() << "\n Contexts values are same. ";/
        return true;
      } else {
    /    if (debugFlag)
          llvm::outs() << "\n Contexts values are different ";
        return false;
      }*/
    return d1 == d2; // modAR::LHF
}

bool IPLFCPA::compareIndices(std::vector<SLIMOperand *> ipVec1, std::vector<SLIMOperand *> ipVec2) const {
    /* return ipVec1 == ipVec2;
     if (ipVec1.size() != ipVec2.size())
        return false;

      for (int i = 0; i < ipVec1.size(); i++) {
        if (ipVec1[i]->getName() != ipVec2[i]->getName())
          return false;
      }
      return true;*/

    if (ipVec1.size() != ipVec2.size())
        return false;

    for (int i = 0; i < ipVec1.size(); i++) {
        if (ipVec1[i]->getValue() != ipVec2[i]->getValue())
            return false;
    }
    return true;
}

// Restricts points-to information at a program point by the corresponding
// liveness information
F IPLFCPA::restrictByLivness(F valPointsTo, B valLiveness) {
    if (debugFlag) {
        llvm::outs() << "\n Inside restrictByLiveness......";
        llvm::outs() << "\n LOUT values";
        printDataFlowValuesBackward(valLiveness);
    }
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    F resPointsTo;

    for (auto lv : valLiveness) {
        for (auto pt : valPointsTo) {
            SLIMOperand *key = GET_PTSET_KEY(pt);
            LFLivenessSet value = GET_VALUE(pt);

            if (key->isArrayElement()) {

                if (debugFlag)
                    llvm::outs() << "\n Key is an array = " << key->getOnlyName();
                if (lv == key and (GET_KEY(lv)->getIndexVector().size() == key->getIndexVector().size())) {
                    LFLivenessSet prevValue =
                        resPointsTo.get_pointees(GET_KEY(lv)); // multiple instances of array in lout
                    resPointsTo = resPointsTo.update_pointees(
                        GET_KEY(lv), backwardMerge(
                                         prevValue,
                                         value)); // merge all array pointees modAR::LHF
                    // std::set<SLIMOperand *> prevValue = resPointsTo[lv]; //
                    // multiple instances of array in lout resPointsTo[lv] =
                    // backwardMerge(prevValue, value); // merge all array
                    // pointees
                }
            } else {
                if (compareOperands(GET_KEY(lv), key)) {
                    resPointsTo = resPointsTo.update_pointees(key, value); // modAR::LHF
                    // resPointsTo[key] = value;
                }
            } // end else
        } // end inner for
    } // end outer for
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return resPointsTo;
}

F IPLFCPA::performMergeAtCallForward(const F &d1, const F &d2) {
    return performMeetForward(d1, d2, "OUT");
}

F IPLFCPA::performMeetForwardWithParameterMapping(const F &d1, const F &d2, const string pos) {
//  llvm::outs() << "\n Size of d1 " << d1.size();
// llvm::outs() << "\n Size of d2 " << d2.size();
/*if (debugFlag) {
   llvm::outs()<< "\n Inside performMeetForwardWithParameterMapping\n";
   llvm::outs() << "\n Printing values of d1";
   printDataFlowValuesForward(d1);

   llvm::outs() << "\n Printing values of d2";
   printDataFlowValuesForward(d2);
   llvm::outs() << "\n Position is = "<<pos;
}*/
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
//     F retMeetINF = d1;
//     BaseInstruction *inst = getCurrentInstruction();

//     for (auto it : d2) {
//         SLIMOperand *key = GET_KEY(it);
//         LFLivenessSet value = GET_VALUE(it);

//         // LFLivenessSet prevValue = retMeetINF[key]; modAR::LHF
//         LFLivenessSet prevValue = retMeetINF.get_pointees(key);
//         bool flag = false;

//         if (!prevValue.empty()) {
//             for (auto pv : prevValue) {
//                 for (auto nv : value) {
//                     if (!compareOperands(GET_KEY(nv), GET_KEY(pv)))
//                         flag = true;
//                 }
//             }
//         } else {
// #ifdef PRVASCO
//             llvm::outs() << "\n Key not in prevPOUT";
// #endif
//             flag = true;
//         }
//         if (flag) {
//             for (auto itValues : value)
//                 retMeetINF = retMeetINF.insert_pointee(key, GET_KEY(itValues)); // modAR::LHF
//             // retMeetINF[key].insert(itValues);
//         }
//     } // end for

    F retMeetINF = d1.set_union(d2);
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return retMeetINF;
}

F IPLFCPA::performMeetForward(const F &d1, const F &d2, const string pos) {
    /*  if (debugFlag) {
         llvm::outs()<< "\n Inside performMeetForward\n";
         llvm::outs() << "\n Printing values of d1";
         printDataFlowValuesForward(d1);
         llvm::outs() << "\n Printing values of d2";
         printDataFlowValuesForward(d2);
         llvm::outs() << "\n Position is = "<<pos;
      }*/
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    F retMeetINF = d1;
    BaseInstruction *inst = getCurrentInstruction();

//     // REPLACEMENT
//     for (auto it : d2) {
//         SLIMOperand *key = GET_KEY(it);
//         LFLivenessSet value = GET_VALUE(it);
//         LFLivenessSet prevValue = retMeetINF.get_pointees(key); // modAR::LHF
//         // std::set<SLIMOperand *> prevValue = retMeetINF[key];
//         bool flag = false;

//         if (!prevValue.empty()) {
//             for (auto pv : prevValue) {
//                 for (auto nv : value) {
//                     if (!compareOperands(GET_KEY(nv), GET_KEY(pv)))
//                         flag = true;
//                 }
//             }
//         } else {
// #ifdef PRVASCO
//             llvm::outs() << "\n Key not in prevPOUT";
// #endif
//             flag = true;
//         }
//         if (flag) {
//             for (auto itValues : value)
//                 retMeetINF = retMeetINF.insert_pointee(key, GET_KEY(itValues)); // modAR::LHF
//             // retMeetINF[key].insert(itValues);
//         }
//     } // end for

    retMeetINF = d1.set_union(d2);

#ifndef PTA
    B bwComponent;

    if (pos == "IN") {
        bwComponent = getBackwardComponentAtInOfThisInstruction(inst);
    } else if (pos == "OUT") {
        bwComponent = getBackwardComponentAtOutOfThisInstruction(inst);
    }

    F retMeetINFnew = restrictByLivness(retMeetINF, bwComponent);
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return retMeetINFnew;
#endif

#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return retMeetINF;
}

F IPLFCPA::getPurelyGlobalComponentForward(const F &dfv) const {
    //  llvm::outs() << "\n Inside getPurelyGlobalComponentForward...........";
    F globalComponent;
    for (auto d : dfv) {
        SLIMOperand *key = GET_PTSET_KEY(d);
        LFLivenessSet value = GET_VALUE(d);
        if (key->isVariableGlobal()) {
            for (auto i : value) {
                if (GET_KEY(i)->isVariableGlobal())
                    globalComponent = globalComponent.insert_pointee(key, GET_KEY(i)); // modAR::LHF
                // globalComponent[key].insert(i);
                else {
                    GET_KEY(i)->setVariableGlobal();
                    globalComponent = globalComponent.insert_pointee(key, GET_KEY(i)); // modAR::LHF
                    // globalComponent[key].insert(i);
                }
                // TODO check alternative for dangling
                // else
                //   globalComponent[key].insert(dangling);
            }
        }
    }
    return globalComponent;
}

void IPLFCPA::printDataFlowValuesForward(const F &dfv) const {
    for (auto inIt : dfv) {
        llvm::outs() << "{ ";
        llvm::outs() << "(" << GET_PTSET_KEY(inIt)->getOnlyName() /*<<" "<< inIt.second */;
        if (GET_PTSET_KEY(inIt)->getNumIndices() > 0) {
            for (int indIter = 0; indIter < GET_PTSET_KEY(inIt)->getNumIndices(); indIter++) {
                llvm::Value *index_val = GET_PTSET_KEY(inIt)->getIndexOperand(indIter)->getValue();
                if (llvm::isa<llvm::Constant>(index_val)) {
                    if (llvm::isa<llvm::ConstantInt>(index_val)) {
                        llvm::ConstantInt *constant_int = llvm::cast<llvm::ConstantInt>(index_val);

                        llvm::outs() << "[" << constant_int->getSExtValue() << "]";
                    } else {
                        llvm_unreachable("[GetElementPtrInstruction Error] The index is a "
                                         "constant but not an integer constant!");
                    }
                } else {
                    llvm::outs() << "[" << index_val->getName() << "]";
                }
            }
        }

        llvm::outs() << ")->(";

        int cnttemp = 0;
        for (auto it : LFLivenessSet(GET_VALUE(inIt))) {
            // if(cnttemp==0) {
            llvm::outs() << "(" << GET_KEY(it)->getOnlyName();
            if (GET_KEY(it)->getNumIndices() > 0) {
                for (int indIter = 0; indIter < GET_KEY(it)->getNumIndices(); indIter++) {
                    llvm::Value *index_val = GET_KEY(it)->getIndexOperand(indIter)->getValue();
                    if (llvm::isa<llvm::Constant>(index_val)) {
                        if (llvm::isa<llvm::ConstantInt>(index_val)) {
                            llvm::ConstantInt *constant_int = llvm::cast<llvm::ConstantInt>(index_val);
                            llvm::outs() << "[" << constant_int->getSExtValue() << "]";
                        } else {
                            llvm_unreachable("[GetElementPtrInstruction Error] The index is "
                                             "a constant but not an integer constant!");
                        }
                    } else {
                        llvm::outs() << "[" << index_val->getName() << "]";
                    }
                } // end for
            } // end if

            llvm::outs() << ")";

            /*} else {
                                   llvm::outs() << " , " << "(" <<
               it->getOnlyName()
               <<")";
               //TODO check if indices to be printed here also
                           }*/
            cnttemp++;
        }
        llvm::outs() << ") }";
    } // end for
}

void IPLFCPA::printFileDataFlowValuesForward(const F &dfv, std::ofstream &out) const {
    for (auto inIt : dfv) {
        out << "{ ";
        out << "(" << GET_PTSET_KEY(inIt)->getOnlyName().str() /*<<" "<< inIt.second */;
        if (GET_PTSET_KEY(inIt)->getNumIndices() > 0) {
            for (int indIter = 0; indIter < GET_PTSET_KEY(inIt)->getNumIndices(); indIter++) {
                llvm::Value *index_val = GET_PTSET_KEY(inIt)->getIndexOperand(indIter)->getValue();
                if (llvm::isa<llvm::Constant>(index_val)) {
                    if (llvm::isa<llvm::ConstantInt>(index_val)) {
                        llvm::ConstantInt *constant_int = llvm::cast<llvm::ConstantInt>(index_val);

                        out << "[" << constant_int->getSExtValue() << "]";
                    } else {
                        llvm_unreachable("[GetElementPtrInstruction Error] The index is a "
                                         "constant but not an integer constant!");
                    }
                } else {
                    out << "[" << index_val->getName().str() << "]";
                }
            }
        }

        out << ")->(";

        int cnttemp = 0;
        for (auto it : LFLivenessSet(GET_VALUE(inIt))) {
            // if(cnttemp==0) {
            out << "(" << GET_KEY(it)->getOnlyName().str();
            if (GET_KEY(it)->getNumIndices() > 0) {
                for (int indIter = 0; indIter < GET_KEY(it)->getNumIndices(); indIter++) {
                    llvm::Value *index_val = GET_KEY(it)->getIndexOperand(indIter)->getValue();
                    if (llvm::isa<llvm::Constant>(index_val)) {
                        if (llvm::isa<llvm::ConstantInt>(index_val)) {
                            llvm::ConstantInt *constant_int = llvm::cast<llvm::ConstantInt>(index_val);
                            out << "[" << constant_int->getSExtValue() << "]";
                        } else {
                            llvm_unreachable("[GetElementPtrInstruction Error] The index is "
                                             "a constant but not an integer constant!");
                        }
                    } else {
                        out << "[" << index_val->getName().str() << "]";
                    }
                } // end for
            } // end if

            out << ")";
            cnttemp++;
        }
        out << ") }";
    } // end for
}

F IPLFCPA::forwardMerge(F a1, F a2) {
/*if (debugFlag) {
  llvm::outs() << "\n Inside forwardMerge.................";
  llvm::outs() << "\n Printing a1: ";
  printDataFlowValuesForward(a1);
  llvm::outs() << "\n Printing a2: ";
  printDataFlowValuesForward(a2);
}*/
// #CHECK
// #ifdef PRINTSTATS
//     stat.timerStart(__func__);
// #endif
//     if (a1.empty() and !a2.empty()) {
// #ifdef PRINTSTATS
//         stat.timerEnd(__func__);
// #endif
//         return a2;
//     }
//     if (!a1.empty() and a2.empty()) {
// #ifdef PRINTSTATS
//         stat.timerEnd(__func__);
// #endif
//         return a1;
//     } else if (!a1.empty() and !a2.empty()) {

//         for (auto it : a2) {
//             SLIMOperand *key = GET_KEY(it);
//             LFLivenessSet value = GET_VALUE(it);
//             LFLivenessSet prevValue = a1.get_pointees(key);
//             // std::set<SLIMOperand *> prevValue = a1[key]; modAR::LHF
//             bool flag = false;

//             if (!prevValue.empty()) {
//                 for (auto pv : prevValue) {
//                     for (auto nv : value) {
//                         if (!compareOperands(GET_KEY(nv), GET_KEY(pv)))
//                             flag = true;
//                         /*else if (!(nv.second == pv.second))
//                                 flag = true;	*/
//                     }
//                 }
//             } else {
// #ifdef PRINT
//                 llvm::outs() << "\n Key not in prevPOUT";
// #endif
//                 flag = true;
//             }

//             if (flag) {
//                 for (auto itValues : value) {
//                     a1 = a1.insert_pointee(key, GET_KEY(itValues));
//                     //  a1[key].insert(itValues); modAR::LHF
//                 }
//             }
//         } // end for
//     } // end if a1 a2 not empty
    F result = a1.set_union(a2);
// #ifdef PRINTSTATS
//     stat.timerEnd(__func__);
// #endif
    return result;
}

LFLivenessSet
IPLFCPA::generatePointeeSetOfSource(SLIMOperand *srcOperand, int in_level, F forwardIN, BaseInstruction *I) {
    if (debugFlag)
        llvm::outs() << "\n Inside generatePointeeSetOfSource.....";

    LFLivenessSet rhsSet;

    if (srcOperand->isArrayElement()) {
        if (debugFlag)
            llvm::outs() << "\n srcOperand is an array: " << srcOperand->getOnlyName();

        bool flgArrFound = false;
        LFLivenessSet setofPointees, rhsPointees;
        for (auto list : listFISArrayObjects) {
            if ((compareOperands(list->fetchSLIMArrayOperand(), srcOperand))) {
                flgArrFound = true; // array already in global data structure
                rhsPointees = list->getArrayPointees();
            } // end if
        } // end for

        bool flagPointeeinDS = false;

        for (auto point : rhsPointees) { // arr[0]->x, arr[0]->bArr[0]
            if (debugFlag)
                llvm::outs() << "\n Printing pointee of source array: " << GET_KEY(point)->getOnlyName();
            if (GET_KEY(point)->isArrayElement()) { // arr[0]->bArr[0]->v
                if (debugFlag)
                    llvm::outs() << "\n Pointee is also an array "
                                    "arr[0]->bArr[0]->v pointee is "
                                 << GET_KEY(point)->getOnlyName() << "\n";
                for (auto list : listFISArrayObjects) {
                    if ((compareOperands(list->fetchSLIMArrayOperand(), GET_KEY(point)))) {
                        flagPointeeinDS = true;
                        LFLivenessSet tmpPointee = list->getArrayPointees();
                        // set use point of the pointee
                        set<BasicBlock *> tempSet = list->getPtUse();
                        auto pos = tempSet.find(I->getBasicBlock());
                        if (pos == tempSet.end()) {
                            list->setPtUse(I->getBasicBlock());
                            list->setFlgChangeInUse(true);
                        }

                        for (auto tp : tmpPointee) { // pointees of rhsPointees
                            if (debugFlag)
                                llvm::outs() << "\n Inserting : " << GET_KEY(tp)->getOnlyName();
                            // setofPointees.insert(tp); // insert v
                            setofPointees = setofPointees.insert_single(GET_KEY(tp)); // modAR::LHF
                            // foundPointee = true;
                        }
                    } // end if
                } // end inner for

                if (!flagPointeeinDS) {
                    // pointee not found in global DS
                    FISArray *newArray = new FISArray(GET_KEY(point));
                    newArray->setOnlyArrayName(GET_KEY(point)->getOnlyName());
                    newArray->setPtUse(I->getBasicBlock());
                    newArray->setFlgChangeInUse(true);
                    listFISArrayObjects.push_back(newArray); // insert into global data str
                }
            } // end if pointee is an array
            else { // arr[0]->x->u
                if (debugFlag)
                    llvm::outs() << "\n Pointee not an array: arr[0]->x->u";

                std::queue<SLIMOperand *> q;
                q.push(GET_KEY(point));
                int Temp = 1;

                while (Temp == 1 and !q.empty()) {
                    SLIMOperand *rhsTemp = q.front();
                    q.pop();
                    if (rhsTemp->isPointerInLLVM()) {
                        for (auto IN : forwardIN) {
                            // notPINEmpty = true;
                            if (compareOperands(rhsTemp, GET_PTSET_KEY(IN))) {
                                LFLivenessSet pointeeSet = GET_VALUE(IN);

                                for (auto pointee : pointeeSet) {
                                    q.push(GET_KEY(pointee));
                                } // end for
                            } // end if
                        } // end outer for
                    } // end if
                    Temp--;
                } // end while

                /* Loop to fetch pointees of RHS */
                while (!q.empty()) {
                    SLIMOperand *rhsValue = q.front();
                    q.pop();
                    // foundPointee = true;
                    setofPointees = setofPointees.insert_single(rhsValue); // modAR::LHF
                    // setofPointees.insert(rhsValue); // insert u
                }
            } // else pointee not array
        } // end for rhspointees

        for (auto sp : setofPointees) {
            if (debugFlag)
                llvm::outs() << "\n Inserting into rhsSet: " << GET_KEY(sp)->getOnlyName();
            rhsSet = rhsSet.insert_single(GET_KEY(sp)); // modAR::LHF
        }

    } // fi is array
    else { // srcOperand is not array
        if (debugFlag)
            llvm::outs() << "\n srcOperand is not an array: " << srcOperand->getOnlyName()
                         << "\n dereference level = " << in_level;

        if (in_level == -1) {
            if (debugFlag)
                llvm::outs() << "\n in_level is -1 so simple generate "
                                "points-to info of srcOperand....";

            if (srcOperand->getOperandType() != CONSTANT_INT)
                rhsSet = rhsSet.insert_single(srcOperand);
        } else {
            if (debugFlag)
                llvm::outs() << "\n in_level is not -1 so proceed normally.....";

            SLIMOperand *marker = createDummyOperand(9);
            std::queue<SLIMOperand *> q;
            q.push(srcOperand);
            q.push(marker);

            //@ indirection level of SLIMOperand is different from the
            // indirection of SOURCE. So the loop with >= 0 is correct.
            // Indirection of SOURCE is equal to no of '*' associated with the
            // operand.

            while (in_level >= 0 and !q.empty()) { /// WHY??
                ///  while (in_level > 0 and !q.empty()) {
                SLIMOperand *rhsTemp = q.front();
                q.pop();

                if (debugFlag)
                    llvm::outs() << "\n Fetch pointees of rhsTemp from Pin:  " << rhsTemp->getOnlyName();

                if (rhsTemp->getOperandType() != CONSTANT_INT) {
                    for (auto IN : forwardIN) {
                        if (compareOperands(rhsTemp, GET_PTSET_KEY(IN))) {
#ifdef PRINT
                            llvm::outs() << "\n Pointees of Rhstemp found in Pin. ";
#endif
                            LFLivenessSet pointeeSet = GET_VALUE(IN);
                            for (auto pointee : pointeeSet) {
                                q.push(GET_KEY(pointee));
                                // foundPointee = true;
                            } // end for
                            q.push(marker);
                        } // end if
                    } // end outer for
                } // end if not marker Null token
                else {
                    if (debugFlag)
                        llvm::outs() << "\n Encountered a Marker operand. "
                                        "Check if ? or marker.";
                    Value *vv = rhsTemp->getValue();
                    ConstantInt *CI = dyn_cast<ConstantInt>(vv);
                    if (CI->getSExtValue() == 999999) {
                        in_level--; // decrement only if marker and not a ?
                    } else if (CI->getSExtValue() == 000000) {
                        rhsSet = rhsSet.insert_single(rhsTemp);
                    }
                }
            } // end while

            // Loop to fetch pointees of RHS
            while (!q.empty()) {
                SLIMOperand *rhsValue = q.front();
                q.pop();
                if (rhsValue->getOperandType() == CONSTANT_INT) {
                    Value *vv = rhsValue->getValue();
                    ConstantInt *CI = dyn_cast<ConstantInt>(vv);
                    if (CI->getSExtValue() == 000000) {
                        // rhsSet.insert(rhsValue); // just add ? and not marker
                        rhsSet = rhsSet.insert_single(rhsValue); // modAR::LHF
                    }
                } else {
                    rhsSet = rhsSet.insert_single(rhsValue);
                }
            } // second while
        } // else in_level != -1
    } // else not array
    return rhsSet;
}

F IPLFCPA::computeOutFromIn(BaseInstruction *I) {
    if (debugFlag) {
        llvm::outs() << "\n Inside computeOutFromIn......................";
        llvm::outs() << "\n Computing for instruction ";
        I->printInstruction();
        llvm::outs() << "\n";
        llvm::outs() << "\n Instruction Type is: " << I->getInstructionType();
    }
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif

    F INofInst, OUTofInst;

    /* Fetch the label of the instruction I */
    int currLabel = getProcessingContextLabel();
    pair<F, B> currInflow = getInflowforContext(currLabel);
    std::pair<std::set<SLIMOperand *>, int> valDefBlock;
    valDefBlock.second = 0; // initialize
    Function *currFunction = I->getFunction();

#ifdef PREANALYSIS
    if (debugFlag) {
        llvm::outs() << "\n ProcessingContextLabel : " << currLabel;
        llvm::outs() << "\n Printing inflow for current label..........";
        printDataFlowValuesForward(currInflow.first);
        printDataFlowValuesBackward(currInflow.second);
        llvm::outs() << "\n #Testing Previous Def and Block values for curr inflow "
                        "FIRST TIME: ";
        std::pair<std::pair<std::set<SLIMOperand *>, int>, bool> temp;
        temp = getTabContextDefBlock(make_pair(currFunction, currInflow));
        //    objDef.printDataFlowValuesFIS(temp.first);
    }
#endif

    F newOutofInst, prevNewOutofInst;
    // F INofInst, OUTofInst;
    LFLivenessSet rhsSet; // modAR::LHF
    F tempMergeOutofInst;
    LFLivenessSet emptyset;
    SLIMOperand *lhsVal;
    std::set<SLIMOperand *> setOfGenVariables;

    // Fetch the previous OUT of the instr
    prevNewOutofInst = getForwardComponentAtOutOfThisInstruction(I);
    B backwardOUT = getBackwardComponentAtOutOfThisInstruction(I);

    if (debugFlag) {
        llvm::outs() << "\n Printing LOUT of instr: ";
        printDataFlowValuesBackward(backwardOUT);
    }

    B Lin = getBackwardComponentAtInOfThisInstruction(I);

    if (debugFlag) {
        llvm::outs() << "\n Printing Lin of instr: ";
        printDataFlowValuesBackward(Lin);
    }

// mod:AR Code added to propagate points-to information reaching PIN_StartQ to
// appropriate USE points

    if (I->isIgnored()) {
        INofInst = getForwardComponentAtInOfThisInstruction(I);
        // Copy IN into OUT
        OUTofInst = INofInst;
        newOutofInst = OUTofInst;

#ifdef PREANALYSIS
        std::pair<SLIMOperand *, int> LHS = I->getLHS();
        if (LHS.second == 2) {
            valDefBlock.second = valDefBlock.second - 1; // reduce the block value
            if (debugFlag)
                llvm::outs() << "\n Instruction is ignored. Lhs indir=2. Block-- ";
        }
#endif

    } else if (I->getInstructionType() == RETURN) {

        INofInst = getForwardComponentAtInOfThisInstruction(I);

        // Copy IN into OUT
        OUTofInst = INofInst;
        newOutofInst = OUTofInst;
    } else {
        // Fetch the LHS and the RHS of instr I
        std::pair<SLIMOperand *, int> LHS = I->getLHS();
        std::vector<std::pair<SLIMOperand *, int>> RHS = I->getRHS();

        lhsVal = LHS.first;
        int indirLhs = LHS.second;
        if (debugFlag)
            llvm::outs() << "\nINDIR of LHS is " << indirLhs << "\n";
        bool pFlag = false; // to mark if must pointee

        // need to remove the following if condition
        if (indirLhs == 0) {
            // ignore the instruction
            // Copy IN into OUT
            if (debugFlag)
                llvm::outs() << "\nLHS indirection is 0; continue\n";

            INofInst = getForwardComponentAtInOfThisInstruction(I);
            OUTofInst = INofInst;

            if (I->getIsDynamicAllocation()) {
                if (debugFlag)
                    llvm::outs() << "\n Instruction is a dynamic memory "
                                    "allocation forwards";
                BasicBlock *BB = I->getBasicBlock();
                SLIMOperand *rhsVal = fetchMallocInsOperand(I->getInstructionId(), BB);
                rhsSet = rhsSet.insert_single(rhsVal);
                // rhsSet.insert(rhsVal); modAR::LHF

                bool flagLhsLive = false;
                B Lout = getBackwardComponentAtOutOfThisInstruction(I);
                for (auto out : Lout) {
                    if (compareOperands(GET_KEY(out), lhsVal))
                        flagLhsLive = true;
                }
                if (flagLhsLive) {
                    // OUTofInst[lhsVal] = rhsSet;  modAR::LHF
                    OUTofInst = OUTofInst.update_pointees(lhsVal, rhsSet);
                }
                // notPINEmpty = true;
                // foundPointee = true;
            }
            newOutofInst = OUTofInst;
        } else {

            // Fetch current IN of the instr
            INofInst = getForwardComponentAtInOfThisInstruction(I);

            // Copy IN into OUT
            OUTofInst = INofInst;

            bool notPINEmpty = false;  // check if PIN is empty
            bool foundPointee = false; // check if pointee of Rhs is found in PIN

            if (I->getInstructionType() == PHI) {
#ifdef PRINT
                llvm::outs() << "\n Instr is PHI. ";
#endif
                /// std::set<SLIMOperand *> rhsSet;

                for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end(); r++) {
                    std::pair<SLIMOperand *, int> rhsVal = *r;

                    if (rhsVal.second == 0) { //..=&u  and rhsVal.first->getName() != "NULL"
#ifdef PRINT
                        llvm::outs() << "\n Rhs indir is 0. ";
#endif
                        if (rhsVal.first->getOperandType() == GEP_OPERATOR) { // #TODO GEP operand in PHI is an
                                                                              // array
                            if (debugFlag)
                                llvm::outs() << "\n Rhs is a PHI GEP Operand. ";

                            if (rhsVal.first->isAlloca() || !rhsVal.first->isPointerInLLVM()) {
#ifdef PRINT
                                llvm::outs() << "\n PHI GEP operand is an "
                                                "(array or) alloca. ";
#endif
                                rhsSet = rhsSet.insert_single(rhsVal.first);
                                // rhsSet.insert(rhsVal.first); modAR::LHF
                                notPINEmpty = true;
                                foundPointee = true;
                            } else {
                                if (debugFlag)
                                    llvm::outs() << "\n PHI GEP operand is not "
                                                    "an array. ";

                                bool isArrFlg = false; // set if pointee in Pin is array
                                                       // then newToken after transfer
                                                       // should be an array
                                for (auto p : INofInst) {
                                    notPINEmpty = true;
                                    SLIMOperand *Pointer = GET_PTSET_KEY(p);
                                    LFLivenessSet Pointee = GET_VALUE(p);

                                    if (compareOperands(rhsVal.first, Pointer)) {
                                        foundPointee = true; // pointee of rhs in PIN

#ifdef PRINTUSEPOINT
                                        llvm::outs() << "\n PRINT USE POINT 16: ID=" << I->getInstructionId();
                                        std::map<SLIMOperand *, std::set<SLIMOperand *>> tempPT;
                                        tempPT[rhsVal.first] = Pointee;
                                        setPointsToPairForUsePoint(I->getInstructionId(), rhsVal.first, tempPT);
#endif

                                        for (auto s : Pointee) {

                                            if (debugFlag)
                                                llvm::outs() << "\n Nested structures "
                                                                "in source code. ";
                                            SLIMOperand *newOperand =
                                                fetchNewOperand(GET_KEY(s)->getValue(), GET_KEY(s)->getIndexVector());

                                            if (GET_KEY(s)->isDynamicAllocationType())
                                                newOperand->setDynamicAllocationType();

                                            if (debugFlag)
                                                llvm::outs() << "\n New operand value: " << newOperand->getName();
                                            // rhsSet.insert(newOperand);
                                            // modAR::LHF
                                            rhsSet = rhsSet.insert_single(newOperand);
                                        } // for pointee
                                    } // if compare
                                } // for InofInst
                            } // else GEP not array
                        } // end if phiGEPOpd
                        else {

                            if (debugFlag)
                                llvm::outs() << "\n Non-GEP PHI operand. ";
                            if (rhsVal.first->getOperandType() != CONSTANT_POINTER_NULL)
                                rhsSet = rhsSet.insert_single(rhsVal.first);
                            // rhsSet.insert(rhsVal.first); modAR::LHF
                            notPINEmpty = true;
                            foundPointee = true;
                        }
                        if (notPINEmpty) {
                            if (foundPointee) {
                                if (lhsVal->isPointerInLLVM()) {
                                    OUTofInst = OUTofInst.update_pointees(lhsVal, rhsSet);
                                    // OUTofInst[lhsVal] = rhsSet; modAR::LHF
                                }
                            }
                        }
                    } // end if rhsindir=0
                    else if (rhsVal.second == 1) { // ..=y. Find pointees from
                                                   // Pin
                        if (debugFlag)
                            llvm::outs() << "\n Rhs Indir is 1. ";

                        if (rhsVal.first->isArrayElement()) {
                            if (debugFlag)
                                llvm::outs() << "\n rhsVal.first is array = " << rhsVal.first->getOnlyName();
                            // rhs is an array
                            bool flgArrFound = false;
                            for (auto list : listFISArrayObjects) {
                                if ((compareOperands(list->fetchSLIMArrayOperand(), rhsVal.first))) {
                                    flgArrFound = true;
                                    LFLivenessSet aPointee = list->getArrayPointees();
                                    for (auto point : aPointee)
                                        rhsSet = rhsSet.insert_single(GET_KEY(point));
                                    // rhsSet.insert(point); modAR::LHF
                                } // end if
                            } // end for

                            if (flgArrFound) {
                                if (lhsVal->isArrayElement()) {
                                    if (debugFlag)
                                        llvm::outs() << "\n lhsVal is array = " << lhsVal->getOnlyName();

                                    for (auto list : listFISArrayObjects) {
                                        if ((compareOperands(list->fetchSLIMArrayOperand(), lhsVal))) {
                                            if (list->getFlgChangeInUse() and indirLhs == 1) {
                                                LFLivenessSet prevP = list->getArrayPointees();
                                                bool flgNewPointees = checkChangeinPointees(prevP, rhsSet);

                                                if (flgNewPointees) {
                                                    list->setArrayPointees(rhsSet);
                                                    list->setFlgChangeInPointees(true);
                                                }
                                            } // indir=1
                                        } // fi
                                    } // for
                                } // end if arr
                                else {
                                    if (lhsVal->isPointerInLLVM()) {
                                        OUTofInst = OUTofInst.update_pointees(lhsVal, rhsSet); // modAR::LHF
                                                                                               //  OUTofInst[lhsVal]
                                                                                               //  = rhsSet;
                                    }
                                } // end else
                            } // if flgArrfound
                        } // end if arr
                        else {
                            if (debugFlag)
                                llvm::outs() << "\n PHI RHS indirec = 1. RHS "
                                                "not an array.";

                            // rhs not an array
                            std::queue<SLIMOperand *> q;
                            q.push(rhsVal.first);

                            while (rhsVal.second == 1 and !q.empty()) {
                                SLIMOperand *rhsTemp = q.front();
                                q.pop();

                                // tempIndx = fetchRhsIndxfrmPin(INofInst,
                                // rhsTemp);
                                if (debugFlag)
                                    llvm::outs() << "\n Fetch pointees of t1 from Pin. ";

                                for (auto IN : INofInst) {
                                    notPINEmpty = true;
                                    if (compareOperands(rhsTemp, GET_PTSET_KEY(IN))) {
#ifdef PRINT
                                        llvm::outs() << "\n Pointees of Rhs "
                                                        "found in Pin. ";
#endif
                                        LFLivenessSet pointeeSet = GET_VALUE(IN);
#ifdef PRINTUSEPOINT
                                        llvm::outs() << "\n PRINT USE POINT 17: ID=" << I->getInstructionId();
                                        std::map<SLIMOperand *, std::set<SLIMOperand *>> tempPT;
                                        tempPT[rhsTemp] = pointeeSet;
                                        setPointsToPairForUsePoint(I->getInstructionId(), rhsTemp, tempPT);
#endif

                                        for (auto pointee : pointeeSet) {
                                            q.push(GET_KEY(pointee));
                                            foundPointee = true;
                                        } // end for
                                    } // end if
                                } // end outer for
                                rhsVal.second--;
                            } // end while
                            // Loop to fetch pointees of RHS
                            while (!q.empty()) {
                                SLIMOperand *rhsValue = q.front();
                                q.pop();
                                rhsSet = rhsSet.insert_single(rhsValue);
                                // rhsSet.insert(rhsValue);  modAR::LHF
                            }
                            if (notPINEmpty) {
                                if (foundPointee) {
                                    if (lhsVal->isPointerInLLVM()) {
                                        OUTofInst = OUTofInst.update_pointees(lhsVal, rhsSet); // modAR::LHF
                                        // OUTofInst[lhsVal] = rhsSet;
                                    }
                                }
                            }
                        } // end else not arr
                    } // end else rhsindir 1
                } // end for

            } // end if phi
            else if (I->getInstructionType() == BITCAST) {
                if (debugFlag)
                    llvm::outs() << "\n Instr is a Bitcast. ";

                bool flgBitcast = true;
                LFLivenessSet rhsSet;
                SLIMOperand *rhsValue = RHS[0].first;
                int rhsIndir = RHS[0].second;

                if (rhsIndir == 1) {
                    for (auto IN : INofInst) {
                        notPINEmpty = true;
                        if (compareOperands(rhsValue, GET_PTSET_KEY(IN))) {
                            foundPointee = true;

#ifdef PRINTUSEPOINT
                            B LOUT = getBackwardComponentAtOutOfThisInstruction(I);
                            bool lhsLive = false;
                            for (auto l : LOUT) {
                                if (compareOperands(l, lhsVal))
                                    lhsLive = true;
                            }
                            if (lhsLive) { // rhs is a use point only if lhs is
                                           // live
                                llvm::outs() << "\n PRINT USE POINT 18: ID=" << I->getInstructionId();
                                std::map<SLIMOperand *, std::set<SLIMOperand *>> tempPT;
                                tempPT[rhsValue] = IN.second;
                                setPointsToPairForUsePoint(I->getInstructionId(), rhsValue, tempPT);
                            }
#endif

                            rhsSet = rhsSet.set_union(GET_VALUE(IN)); // mod::AR
                            /* for (auto in : IN.second) {
                                rhsSet.insert(in);
                             }  modAR::LHF*/
                            /// for (auto in : LFLivenessSet(GET_VALUE(IN)))
                            ///    rhsSet = rhsSet.insert_single(in);
                        } // fi
                    } // for
                } // end else if=1

                if (notPINEmpty) { // necessary condition to avoid i23->() in
                                   // POUT
                    if (foundPointee) {
                        if (lhsVal->isPointerInLLVM()) {
                            OUTofInst = OUTofInst.update_pointees(lhsVal, rhsSet);
                            // OUTofInst[lhsVal] = rhsSet; modAR::LHF
                        }
                    }
                }
            } // end if Bitcast
            else if (I->getInstructionType() == GET_ELEMENT_PTR) {
                if (debugFlag) {
                    llvm::outs() << "\n Instr is a GEP x = gep y[indx]. ";
                    I->printInstruction();
                }

                /// std::set<SLIMOperand*> rhsSet;

                SLIMOperand *rhsValue = RHS[0].first;

                if (debugFlag)
                    llvm::outs() << "\n RHS:: " << rhsValue->getOnlyName() << "Number of indices "
                                 << rhsValue->getNumIndices() << "\n Is RHS an array = " << rhsValue->isArrayElement()
                                 << "\n";

                GetElementPtrInstruction *GEPI = (GetElementPtrInstruction *)I;
                if (debugFlag)
                    llvm::outs() << "\n"
                                 << "Number of indices for instruction " << GEPI->getNumIndexOperands() << "\n";

                /* Check if LHS live at OUT*/
                bool flagIsLhsLive = false;
                B LOUT = getBackwardComponentAtOutOfThisInstruction(I);
                for (auto lout : LOUT) {
                    if (compareOperands(GET_KEY(lout), lhsVal))
                        flagIsLhsLive = true;
                }

                if (flagIsLhsLive or lhsVal->isArrayElement()) { // code should execute if lhs is
                                                                 // an array
                    /* Perform the following computation only if LHS is live. */
                    // Check if gep operand is an array or alloca ptr
                    // If GEP operand is not a pointer the instr should behave
                    // as a normal x=&y instr Indices associated with
                    // instruction, hence checking numIndices from instr(instead
                    // of rhsValue) for 1

                    if (debugFlag)
                        llvm::outs() << "\n flagIsLhsLive or lhsVal is array = " << lhsVal->getOnlyName();

                    if (rhsValue->getName() != "NULL") {
                        if (GEPI->getNumIndexOperands() == 1) {
#ifdef PRINT
                            llvm::outs() << "\n GEP has single index. No "
                                            "transfer of links required.";
#endif

                            if (rhsValue->isArrayElement()) {
                                if (debugFlag)
                                    llvm::outs() << "\n rhsValue is array = " << rhsValue->getOnlyName();

                                for (auto list : listFISArrayObjects) {
                                    if (compareOperands(list->fetchSLIMArrayOperand(), rhsValue)) {
                                        LFLivenessSet rhsPointees = list->getArrayPointees();
                                        for (auto rp : rhsPointees)
                                            rhsSet = rhsSet.insert_single(GET_KEY(rp));
                                        // rhsSet.insert(rp); modAR::LHF
                                        notPINEmpty = true;
                                        foundPointee = true;
                                    } // fi
                                } // for
                            } // end if arr
                            else {
                                for (auto p : INofInst) {
                                    notPINEmpty = true; // Pin is non-empty
                                    SLIMOperand *Pointer = GET_PTSET_KEY(p);
                                    LFLivenessSet Pointee = GET_VALUE(p);
                                    if (compareOperands(rhsValue, Pointer)) {
                                        foundPointee = true; // pointee of rhs in PIN

#ifdef PRINTUSEPOINT
                                        llvm::outs() << "\n PRINT USE POINT 19: ID=" << I->getInstructionId();
                                        std::map<SLIMOperand *, std::set<SLIMOperand *>> tempPT;
                                        tempPT[rhsValue] = Pointee;
                                        setPointsToPairForUsePoint(I->getInstructionId(), rhsValue, tempPT);
#endif

                                        for (auto s : Pointee) {
                                            rhsSet = rhsSet.insert_single(GET_KEY(s));
                                            // rhsSet.insert((s)); modAR::LHF
                                        } // for
                                    } // end if
                                } // end outer for
                            } // else not arr
                        } // end if OneGepIndx
                        // TODO Array
                        else if (
                            (/*rhsVal.first->getIsArray() and*/ rhsValue->isVariableGlobal()) or rhsValue->isAlloca()) {

                            //----------------
                            SLIMOperand *newOperand =
                                fetchNewOperand(rhsValue->getValue(), LHS.first->getIndexVector());

                            if (rhsValue->isDynamicAllocationType())
                                newOperand->setDynamicAllocationType(); // CHECK if
                                                                        // this is
                                                                        // required
                                                                        // (18.7.25)

                            if (debugFlag) {
                                llvm::outs() << "\n After xfer Rhs value = " << newOperand->getOnlyName()
                                             << "\t index = " << newOperand->getIndexVector().size() << "::";
                                printVector(newOperand->getIndexVector());
                                llvm::outs() << "\n Is newOperand an atrray = " << newOperand->isArrayElement();
                            }

                            if (debugFlag)
                                llvm::outs() << "\n RHS value =" << rhsValue->getOnlyName()
                                             << "\n is RHS an array = " << rhsValue->isArrayElement();

                            if (rhsValue->isArrayElement())
                                newOperand->setArrayType();

                            rhsSet = rhsSet.insert_single(newOperand);

                            //-------------

                            /*
                            (g,1) = (s1[0][0], 0) ==> here s1 is an alloca.
                            g->s1[0][0] but the s.indexvector.size is 0. Hence
                            transfering the index vector of lhs to rhs is
                            required.
                            */

                            ////rhsSet = rhsSet.insert_single(rhsValue);
                            /////modAR::LHF

                            /*rhsSet.insert(rhsValue);*/ /*commented on 4Oct24.
                               Issue in h264ref. No idea why below code was
                               commented.*/

                            /********************************************
                            @ Following code working for h264ref but not for
                            LBM. Hence it is commented for now. #TODO CHECK
                            h264ref

                                        SLIMOperand *newOperand =
                            fetchNewOperand( rhsValue->getValue(),
                            LHS.first->getIndexVector());

                                        if (debugFlag) {
                                          llvm::outs() << "\n After xfer Rhs
                            value = "
                                                       <<
                            newOperand->getOnlyName() << "\t index = "
                                                       <<
                            newOperand->getIndexVector().size() << "::";
                                          printVector(newOperand->getIndexVector());
                                        }
                                        rhsSet.insert(newOperand); // commented
                            on 11July24. uncommented on 4Oct24.
                            ******************************/
                            notPINEmpty = true;
                            foundPointee = true;
                        } else if (rhsValue->isPointerInLLVM()) {
                            if (debugFlag) {
                                llvm::outs() << "\n GEP operand is a pointer. ";
                                llvm::outs() << "\n RHS value is = " << rhsValue->getOnlyName();
                            }
                            if (rhsValue->isArrayElement()) {
                                if (debugFlag)
                                    llvm::outs() << "\n Rhs is array = " << rhsValue->getOnlyName();

                                for (auto list : listFISArrayObjects) {
                                    if ((compareOperands(list->fetchSLIMArrayOperand(), rhsValue))) {
                                        LFLivenessSet setPointee = list->getArrayPointees();
                                        for (auto PT : setPointee) {
                                            // #NOTE Below 2 for loops kept as
                                            // it is
                                            std::vector<SLIMOperand *> vecIndex;
                                            for (int indIter = 0; indIter < GET_KEY(PT)->getNumIndices(); indIter++) {
                                                vecIndex.push_back(GET_KEY(PT)->getIndexOperand(indIter));
                                            }
                                            for (int indIter = 0; indIter < GEPI->getNumIndexOperands(); indIter++) {
                                                vecIndex.push_back(GEPI->getIndexOperand(indIter));
                                            }

                                            SLIMOperand *newOperand =
                                                fetchNewOperand(GET_KEY(PT)->getValue(), vecIndex);

                                            if (GET_KEY(PT)->isDynamicAllocationType())
                                                newOperand->setDynamicAllocationType();

                                            if (debugFlag)
                                                llvm::outs() << "\nNew operand created " << newOperand->getValue()
                                                             << " " << newOperand->getName()
                                                             << "\n new operand is an "
                                                                "array? = "
                                                             << newOperand->isArrayElement();

                                            /* If RHS is an array then
                                             * newOperand should also be an
                                             * array. */
                                            newOperand->setArrayType();
                                            // rhsSet.insert(newOperand);
                                            // modAR::LHF
                                            rhsSet = rhsSet.insert_single(newOperand);
                                            notPINEmpty = true;
                                            foundPointee = true;
                                        } // for
                                    } // fi
                                } // for
                            } // end if
                            else { // rhs not an array
                                if (debugFlag)
                                    llvm::outs() << "\n Rhs value not array: " << rhsValue->getOnlyName();
                                /* Following code should execute for both SOURCE
                                 * and NOSOURCE */
                                for (auto p : INofInst) {
                                    notPINEmpty = true; // Pin is non-empty
                                    SLIMOperand *Pointer = GET_PTSET_KEY(p);
                                    LFLivenessSet Pointee = GET_VALUE(p);

                                    if (debugFlag) {
                                        llvm::outs() << "\n rhsValue = " << rhsValue->getOnlyName()
                                                     << " Pointer nm: " << Pointer->getOnlyName();
                                    }

                                    if (compareOperands(rhsValue, Pointer)) {
                                        foundPointee = true; // pointee of rhs in PIN

#ifdef PRINTUSEPOINT
                                        llvm::outs() << "\n PRINT USE POINT 20: ID=" << I->getInstructionId();
                                        B LOUT = getBackwardComponentAtOutOfThisInstruction(I);
                                        bool lhsLive = false;
                                        for (auto l : LOUT) {
                                            if (compareOperands(l, lhsVal))
                                                lhsLive = true;
                                        }
                                        if (lhsLive) { // rhs is a use point
                                                       // only if lhs is live
                                            std::map<SLIMOperand *, std::set<SLIMOperand *>> tempPT;
                                            tempPT[rhsValue] = Pointee;
                                            setPointsToPairForUsePoint(I->getInstructionId(), rhsValue, tempPT);
                                        }
#endif

                                        for (auto s : Pointee) {
                                            if (debugFlag)
                                                llvm::outs() << "\n Nested structures "
                                                                "in source code. ";

                                            std::vector<SLIMOperand *> vecIndex;
                                            for (int indIter = 0; indIter < GET_KEY(s)->getNumIndices(); indIter++) {
                                                vecIndex.push_back(GET_KEY(s)->getIndexOperand(indIter));
                                            }
                                            for (int indIter = 0; indIter < GEPI->getNumIndexOperands(); indIter++) {
                                                vecIndex.push_back(GEPI->getIndexOperand(indIter));
                                            }
                                            SLIMOperand *newOperand = fetchNewOperand(GET_KEY(s)->getValue(), vecIndex);

                                            // @Adding following code since
                                            // isArrayElement is treating
                                            // operands created due to dynamic
                                            // memory allocation as arrays. Case
                                            // identified in bzip2
                                            // OB_7276[0][4][0][9] is returned
                                            // as an array. (18.7.2025)

                                            if (GET_KEY(s)->isDynamicAllocationType())
                                                newOperand->setDynamicAllocationType();

                                            if (debugFlag) {
                                                llvm::outs() << "\nNew operand created " << newOperand->getValue()
                                                             << " " << newOperand->getName() << "\n";
                                                printIndices(newOperand);
                                                llvm::outs() << "\n Size of index "
                                                                "vector of new operand: "
                                                             << newOperand->getIndexVector().size();
                                                llvm::outs() << "\n IS newOperand AN "
                                                                "ARRAY = "
                                                             << newOperand->isArrayElement();
                                            }
                                            // rhsSet.insert(newOperand);
                                            // modAR::LHF
                                            rhsSet = rhsSet.insert_single(newOperand);
                                        } // for
                                    } // if
                                } // end for
                            } // end else not arr
                        } // end if pointer
                        else {
#ifdef PRINT
                            llvm::outs() << "\n GEP operand is not a pointer";
#endif
                            // rhsSet.insert(rhsValue); modAR::LHF
                            rhsSet = rhsSet.insert_single(rhsValue);
                        } // not a pointer

                        // Update the pointees of LHS
                        if (lhsVal->isArrayElement()) {
                            if (debugFlag)
                                llvm::outs() << "\n Lhs is an array = " << lhsVal->getOnlyName();
                            bool flgFound = false;
                            // check if array is in global data structure else
                            // make an entry
                            for (auto list : listFISArrayObjects) {
                                if ((compareOperands(list->fetchSLIMArrayOperand(), lhsVal))) {
                                    if (debugFlag)
                                        llvm::outs() << "\n Lhs found in "
                                                        "global data str....";
                                    flgFound = true; // array already in global
                                                     // data structure
                                    if (!list->getPtUse().empty()) {
                                        if (debugFlag)
                                            llvm::outs() << "\n Add pointees only if "
                                                            "there is a USE";
                                        LFLivenessSet prevP = list->getArrayPointees();
                                        bool flgNewPointees = checkChangeinPointees(prevP, rhsSet);
                                        if (flgNewPointees) {
                                            list->setArrayPointees(rhsSet);
                                            list->setFlgChangeInPointees(true);
                                        }
                                        if (debugFlag) {
                                            llvm::outs() << "\n #Testing...printing "
                                                            "pointees of Lhs....";
                                            for (auto r : list->getArrayPointees())
                                                llvm::outs() << "\n : " << GET_KEY(r)->getOnlyName() << " :";
                                        }
                                    }
                                } // fi
                            } // for
                        } else {
                            if (debugFlag)
                                llvm::outs() << "\n GEP Lhs not an array";
                            if (notPINEmpty) {
                                if (foundPointee) {
                                    if (lhsVal->isPointerInLLVM()) {
                                        OUTofInst = OUTofInst.update_pointees(lhsVal, rhsSet); // modAR::LHF
                                        // OUTofInst[lhsVal] = rhsSet;
                                    }
                                }
                            }
                        }
                    } // end if not null
                } // if lhs is live
            } // end gep
            else if (
                I->getInstructionType() == BRANCH or I->getInstructionType() == ALLOCA or
                I->getInstructionType() == COMPARE) {
                // llvm::outs()<< "\nBranch/ALLOCA Instruction\n";
                /// F INofInst, OUTofInst;
                INofInst = getForwardComponentAtInOfThisInstruction(I);

                // Copy IN into OUT
                OUTofInst = INofInst;
                newOutofInst = OUTofInst;
            } else {
                if (debugFlag)
                    llvm::outs() << "\n Normal instruction. ";

                SLIMOperand *rhsValue = RHS[0].first;
                int rhsIndir = RHS[0].second;
                int rhsIndirTemp = rhsIndir;
                SLIMOperand *slim_function_op; // stores SLIM OP for function

                if (debugFlag)
                    llvm::outs() << "\n RHS value: " << rhsValue->getOnlyName() << " indir: " << rhsIndir;
                // First get points-to pairs from Rhs
                // case 1: x=&u
                if (rhsIndir == 0 and rhsValue->getOperandType() != CONSTANT_INT and
                    rhsValue->getOperandType() != CONSTANT_POINTER_NULL) {
                    foundPointee = true;
                    notPINEmpty = true;
                    // #ADDCODE for GEP
                    if (I->getInstructionType() == GET_ELEMENT_PTR) { // this check is not required here
                                                                      // since there
                        // is separate check for this condition
                        if (debugFlag)
                            llvm::outs() << "\n Inst is a GEP. ";
                        // rhsSet.insert(rhsValue); modAR::LHF
                        rhsSet = rhsSet.insert_single(rhsValue);
                    } else {
                        if (debugFlag)
                            llvm::outs() << "\n indir =0 Normal instruction. " << rhsValue->getName();

                        llvm::Function *fun_name;
                        fun_name = rhsValue->extractFunction();
                        if (fun_name) {
                            slim_function_op = rhsValue->getOperandForFunction(fun_name);
                            if (debugFlag)
                                llvm::outs() << "\n function name here = " << slim_function_op->getOnlyName();
                            // rhsSet.insert(slim_function_op);    modAR::LHF
                            rhsSet = rhsSet.insert_single(slim_function_op);
                        } else {
                            if (debugFlag)
                                llvm::outs() << "\n No function name present";
                            // rhsSet.insert(rhsValue); modAR::LHF
                            rhsSet = rhsSet.insert_single(rhsValue);
                        }
                    }
                } // end if
                else if (rhsIndir == 1 and rhsValue->getOperandType() != CONSTANT_INT) {
                    /* case 2: x=t1 or t1=x or *t2=t1 */
                    if (debugFlag)
                        llvm::outs() << "\n case 2: x=t1 or t1=x or *t2=t1 ";
                    if (rhsValue->isArrayElement()) {
                        if (debugFlag)
                            llvm::outs() << "\n Rhs is an array....= " << rhsValue->getOnlyName();
                        bool flgArrFound = false;
                        for (auto list : listFISArrayObjects) {
                            if ((compareOperands(list->fetchSLIMArrayOperand(), rhsValue))) {
                                if (debugFlag)
                                    llvm::outs() << "\n Rhs found in global "
                                                    "data str....";
                                flgArrFound = true;
                                LFLivenessSet aPointee = list->getArrayPointees();
                                if (debugFlag)
                                    llvm::outs() << "\n Printing pointees of Rhs...";
                                if (!aPointee.empty()) {
                                    foundPointee = true;
                                    for (auto point : aPointee) {
                                        if (debugFlag)
                                            llvm::outs() << "\n : " << GET_KEY(point)->getOnlyName() << " :";
                                        // rhsSet.insert(point); modAR::LHF
                                        rhsSet = rhsSet.insert_single(GET_KEY(point));
                                    }
                                    if (debugFlag)
                                        llvm::outs() << "\n Printing size of rhsSet= " << rhsSet.size();
                                } // fi
                                break;
                            } // end if
                        } // end for

                        if (!flgArrFound) {
                            if (debugFlag)
                                llvm::outs() << "\n ALERT!!! Array not found "
                                                "in global DS";
                            /* do nothing. This can happen if there is no use of
                             * this array during liveness analysis. */
                        }
                    } // end if array
                    else { // rhs not array
                        SLIMOperand *rhsValue_orig = rhsValue;
                        if (debugFlag) {
                            llvm::outs() << "\n Rhs not an array---";
                            llvm::outs() << "\n #TESTING before RHS = " << rhsValue->getOnlyName();
                            llvm::outs() << "\n Is Rhs global = " << rhsValue->isVariableGlobal();
                        }

                        /* Check if pointees of RHS are in PIN */
                        bool flagPointeeinPin = false;
                        F PIN = getForwardComponentAtInOfThisInstruction(I);
                        for (auto pin : PIN) {
                            if (compareOperands(GET_PTSET_KEY(pin), rhsValue))
                                flagPointeeinPin = true;
                        }

                        std::queue<SLIMOperand *> q;
                        foundPointee = false;
                        q.push(rhsValue);

                        while (rhsIndirTemp == 1 and !q.empty()) {
                            SLIMOperand *rhsTemp = q.front();
                            q.pop();
                            if (rhsTemp->isPointerInLLVM()) {
                                if (debugFlag) {
                                    llvm::outs() << "Rhs value is a pointer: " << rhsTemp->getName();
                                    llvm::outs() << "\n Fetch pointees of t1 from Pin. ";
                                }

                                for (auto IN : INofInst) {
                                    notPINEmpty = true;
                                    if (compareOperands(rhsTemp, GET_PTSET_KEY(IN))) {
                                        if (debugFlag)
                                            llvm::outs() << "\n Pointees of Rhs found "
                                                            "in Pin. ";
                                        // #endif
                                        LFLivenessSet pointeeSet = GET_VALUE(IN);

#ifdef PRINTUSEPOINT
                                        B LOUT = getBackwardComponentAtOutOfThisInstruction(I);
                                        bool lhsLive = false;
                                        for (auto l : LOUT) {
                                            if (compareOperands(l, lhsVal))
                                                lhsLive = true;
                                        }
                                        if (lhsLive) { // rhs is a use point
                                                       // only if lhs is live
                                            llvm::outs() << "\n PRINT USE POINT 21: ID=" << I->getInstructionId();
                                            std::map<SLIMOperand *, std::set<SLIMOperand *>> tempPT;
                                            tempPT[rhsTemp] = pointeeSet;
                                            setPointsToPairForUsePoint(I->getInstructionId(), rhsTemp, tempPT);
                                        }
#endif

                                        for (auto pointee : pointeeSet) {
                                            q.push(GET_KEY(pointee));
                                            if (debugFlag)
                                                llvm::outs() << "\n Pushing pointee on "
                                                                "the queue: "
                                                             << GET_KEY(pointee)->getName();
                                        } // end for
                                    } // end if
                                } // end outer for
                            } // end if
                            rhsIndirTemp--;
                        } // end while
                        /* Loop to fetch pointees of RHS */
                        while (!q.empty()) {
                            if (debugFlag)
                                llvm::outs() << "\n Second while loop";
                            SLIMOperand *rhsValue = q.front();
                            q.pop();

                            if (debugFlag)
                                llvm::outs() << "\n Verify pointee of rhs: " << rhsValue->getName();

                            // rhsSet.insert(rhsValue); modAR::LHF
                            rhsSet = rhsSet.insert_single(rhsValue);
                            foundPointee = true;
                        } // while
                        //}                       // end else not arr
                        //}                         // end else if=1
                    } // end else not arr
                } // end else if=1
                else if (rhsIndir == 2) { /* case 3: x=*t or t1=*t2 */
                    if (debugFlag)
                        llvm::outs() << "\n Case 3: x=*t1 or t1=*t2 ";
                    if (rhsValue->isArrayElement()) { // ...=*arr[0]
                        if (debugFlag)
                            llvm::outs() << "\n rhsvalue is an array = " << rhsValue->getOnlyName();
                        bool flgArrFound = false;
                        LFLivenessSet setofPointees, rhsPointees;
                        for (auto list : listFISArrayObjects) {
                            if ((compareOperands(list->fetchSLIMArrayOperand(), rhsValue))) {
                                flgArrFound = true; // array already in global
                                                    // data structure
                                rhsPointees = list->getArrayPointees();
                            } // end if
                        } // end for

                        bool flagPointeeinDS = false;

                        for (auto point : rhsPointees) {            // arr[0]->x, arr[0]->bArr[0]
                            if (GET_KEY(point)->isArrayElement()) { // arr[0]->bArr[0]->v
                                if (debugFlag) {
                                    llvm::outs() << "\n Pointee is also an array "
                                                    "arr[0]->bArr[0]->v pointee is "
                                                 << GET_KEY(point)->getOnlyName() << "\n";
                                    // llvm::outs() << "\n GEP MAIN FOR POINTEE
                                    // = "<<point->get_gep_main_operand();
                                    // llvm::outs() << "\n Pointee
                                    // getValueofArray =
                                    // "<<point->getValueOfArray();
                                }
                                for (auto list : listFISArrayObjects) {
                                    if ((compareOperands(list->fetchSLIMArrayOperand(), GET_KEY(point)))) {
                                        flagPointeeinDS = true;
                                        LFLivenessSet tmpPointee = list->getArrayPointees();

                                        // set use point of the pointee
                                        set<BasicBlock *> tempSet = list->getPtUse();
                                        auto pos = tempSet.find(I->getBasicBlock());
                                        if (debugFlag) {
                                            llvm::outs()
                                                << "\nPtUse for array " << list->fetchSLIMArrayOperand()->getOnlyName()
                                                << " Use point size: " << list->getPtUse().size();
                                        }
                                        if (pos == tempSet.end()) {
                                            if (debugFlag)
                                                llvm::outs() << "\nSetting setPtUse for "
                                                                "array "
                                                             << list->fetchSLIMArrayOperand()->getOnlyName() << "\n";
                                            list->setPtUse(I->getBasicBlock());
                                            list->setFlgChangeInUse(true);
                                        }

                                        if (debugFlag)
                                            llvm::outs() << "\n Size of tmpPointee: " << tmpPointee.size();
                                        for (auto tp : tmpPointee) { // pointees of
                                                                     // rhsPointees
                                            if (debugFlag)
                                                llvm::outs() << "\n Inserting : " << GET_KEY(tp)->getOnlyName();
                                            // setofPointees.insert(tp); //
                                            // insert v  modAR::LHF
                                            setofPointees = setofPointees.insert_single(GET_KEY(tp));
                                            foundPointee = true;
                                        }
                                    } // end if
                                } // end inner for
                                if (!flagPointeeinDS) {
                                    // pointee not found in global DS
                                    if (debugFlag)
                                        llvm::outs()
                                            << "\n CREATING array object for " << GET_KEY(point)->getOnlyName() << "\n";
                                    FISArray *newArray = new FISArray(GET_KEY(point));
                                    newArray->setOnlyArrayName(GET_KEY(point)->getOnlyName());
                                    newArray->setPtUse(I->getBasicBlock());
                                    newArray->setFlgChangeInUse(true);
                                    listFISArrayObjects.push_back(newArray); // insert into global data
                                                                             // str
                                }
                            } // end if pointee is an array
                            else { // arr[0]->x->u
                                if (debugFlag) {
                                    llvm::outs() << "\n Pointee not an array: "
                                                    "arr[0]->x->u = "
                                                 << GET_KEY(point)->getOnlyName();
                                    printIndices(GET_KEY(point));
                                }
                                std::queue<SLIMOperand *> q;
                                q.push(GET_KEY(point));
                                int Temp = 1;

                                while (Temp == 1 and !q.empty()) {
                                    SLIMOperand *rhsTemp = q.front();

                                    q.pop();
                                    if (rhsTemp->isPointerInLLVM() or rhsTemp->isPointerVariable()) {
                                        for (auto IN : INofInst) {
                                            notPINEmpty = true;
                                            if (compareOperands(rhsTemp, GET_PTSET_KEY(IN))) {
                                                LFLivenessSet pointeeSet = GET_VALUE(IN);

#ifdef PRINTUSEPOINT
                                                llvm::outs() << "\n PRINT USE POINT 22: "
                                                                "ID="
                                                             << I->getInstructionId();
                                                setUsePoint(I->getInstructionId(), rhsTemp);
                                                std::map<SLIMOperand *, std::set<SLIMOperand *>> tempPT;
                                                tempPT[rhsTemp] = pointeeSet;
                                                setPointsToPairForUsePoint(I->getInstructionId(), rhsTemp, tempPT);
#endif

                                                for (auto pointee : pointeeSet) {
                                                    q.push(GET_KEY(pointee));
                                                    llvm::outs() << "\n Pointee = " << GET_KEY(pointee)->getOnlyName();
                                                } // end for
                                            } // end if
                                        } // end outer for
                                    } // end if
                                    Temp--;
                                } // end while

                                /* Loop to fetch pointees of RHS */
                                while (!q.empty()) {
                                    SLIMOperand *rhsValue = q.front();
                                    llvm::outs() << "\n rhsValue again = " << rhsValue->getOnlyName();
                                    q.pop();
                                    foundPointee = true;
                                    setofPointees = setofPointees.insert_single(rhsValue); // modAR::LHF
                                    // setofPointees.insert(rhsValue); // insert
                                    // u
                                }
                            } // end else pointee not array
                        } // end for
                        for (auto sp : setofPointees)
                            rhsSet = rhsSet.insert_single(GET_KEY(sp));
                        // rhsSet.insert(sp); // insert u, v modAR::LHF
                    } // end if arr
                    else {
                        // if rhs is not an array
                        // then first check if it has a source
                        if (debugFlag)
                            llvm::outs() << "\n RHS VALUE NOT AN ARRAY BUT IS "
                                            "A GEP OPERATOR:  "
                                         << rhsValue->getOnlyName();
                        // not an arr  ...= *x
                        SLIMOperand *marker = createDummyOperand(9);
                        std::queue<SLIMOperand *> q;

                        if (!INofInst.empty()) {
                            q.push(rhsValue);
                            q.push(marker);

                            while (rhsIndirTemp > 0 and !q.empty()) {
                                SLIMOperand *rhsTemp = q.front();
                                q.pop();
                                if (debugFlag)
                                    llvm::outs() << "\n Rhstemp.getValue() = " << rhsTemp->getValue() << " "
                                                 << rhsTemp->getOnlyName();

                                if (rhsTemp->getOperandType() != CONSTANT_INT) {
                                    if (rhsTemp->isArrayElement()) {
                                        if (debugFlag)
                                            llvm::outs() << "\n rhsTemp is an array = " << rhsTemp->getOnlyName();

                                        for (auto list : listFISArrayObjects) {
                                            if ((compareOperands(list->fetchSLIMArrayOperand(), rhsTemp))) {
                                                LFLivenessSet tempPointee = list->getArrayPointees();
                                                for (auto t : tempPointee) {
                                                    if (debugFlag)
                                                        llvm::outs() << "\nPointee of "
                                                                        "pointee in RHS "
                                                                        "where pointee "
                                                                        "is an Array "
                                                                     << GET_KEY(t)->getOnlyName() << "\n";
                                                    // rhsSet.insert(t);
                                                    // modAR::LHF
                                                    rhsSet = rhsSet.insert_single(GET_KEY(t));
                                                    foundPointee = true;
                                                } // for
                                            } // if compare
                                        } // for
                                    } // end if pointee is arr
                                    else { // if pointee not an array
                                        for (auto IN : INofInst) {
                                            notPINEmpty = true;
                                            if (compareOperands(rhsTemp, GET_PTSET_KEY(IN))) {
#ifdef PRINT
                                                llvm::outs() << "\n Pointees of Rhs "
                                                                "found in Pin. ";
#endif
                                                LFLivenessSet pointeeSet = GET_VALUE(IN);

#ifdef PRINTUSEPOINT
                                                llvm::outs() << "\n PRINT USE POINT 23: "
                                                                "ID="
                                                             << I->getInstructionId();
                                                setUsePoint(
                                                    I->getInstructionId(),
                                                    rhsTemp); // set use point
                                                              // for pointee
                                                std::map<SLIMOperand *, std::set<SLIMOperand *>> tempPT;
                                                tempPT[rhsTemp] = pointeeSet;
                                                setPointsToPairForUsePoint(I->getInstructionId(), rhsTemp, tempPT);
#endif

                                                for (auto pointee : pointeeSet) {
                                                    if (debugFlag) {
                                                        llvm::outs() << "\n Name of "
                                                                        "pointee = "
                                                                     << GET_KEY(pointee)->getOnlyName();
                                                        llvm::outs() << "\n Value of "
                                                                        "pointee = "
                                                                     << GET_KEY(pointee)->getValue();
                                                    }
                                                    q.push(GET_KEY(pointee));
                                                } // end for
                                                q.push(marker);
                                            } // end if
                                        } // end outer for
                                    } // end else not arr
                                } // end if not marker Null token
                                else {
                                    if (debugFlag)
                                        llvm::outs() << "\n Encountered a "
                                                        "Marker operand. "
                                                        "Check if ? or marker.";
                                    /* changes added on 28.2.2023 */
                                    Value *vv = rhsTemp->getValue();
                                    ConstantInt *CI = dyn_cast<ConstantInt>(vv);
                                    if (CI->getSExtValue() == 999999) {
                                        rhsIndirTemp--; // decrement only if
                                                        // marker and not a ?
                                    } else if (CI->getSExtValue() == 000000) {
                                        rhsSet = rhsSet.insert_single(rhsTemp);
                                        // rhsSet.insert(rhsTemp); //#TODO
                                        // testing. modAR::LHF
                                    }
                                }
                            } // end while
                        } // end if PIN not empty

                        /* Loop to fetch pointees of RHS */
                        // llvm::outs()<<q.size();
                        while (!q.empty()) {
#ifdef PRINT
                            llvm::outs() << "\n Fetching pointees of RHS. ";
#endif
                            SLIMOperand *rhsValue = q.front();
                            q.pop();
                            if (rhsValue->getOperandType() == CONSTANT_INT) {
                                Value *vv = rhsValue->getValue();
                                ConstantInt *CI = dyn_cast<ConstantInt>(vv);
                                if (CI->getSExtValue() == 000000) {
                                    // rhsSet.insert(rhsValue); // just add ?
                                    // and not marker
                                    rhsSet = rhsSet.insert_single(rhsValue); // modAR::LHF
                                    foundPointee = true;                     // only if x=*t2 P(P(t2)) are
                                                                             // found in PIN
                                }
                            } else {
                                // rhsSet.insert(rhsValue); modAR::LHF
                                rhsSet = rhsSet.insert_single(rhsValue);
                                foundPointee = true; // only if x=*t2 P(P(t2))
                                                     // are found in PIN
                            }
                        } // second while
                    } // else Rhs is not an array
                } // end else if=2
                if (debugFlag)
                    llvm::outs() << "\n Now fetching the pointees of LHS: " << lhsVal->getName();
                /* Fetch the pointees of Lhs */
                if (lhsVal->isArrayElement()) {
                    if (debugFlag) {
                        llvm::outs() << "\n Lhs is an array = " << lhsVal->getOnlyName();
                        // llvm::Value *v1 = lhsVal->get_gep_main_operand();
                        // llvm::outs() << "\n LHS gep main = "<<v1;
                        // llvm::outs() << "\n Lhs value =
                        // "<<lhsVal->getValue(); llvm::outs() << "\n LHS
                        // getValueofArray = "<<lhsVal->getValueOfArray();
                    }

                    bool flgFound = false;
                    // check if array is in global data structure else make an
                    // entry
                    for (auto list : listFISArrayObjects) {
                        if ((compareOperands(list->fetchSLIMArrayOperand(), lhsVal))) {
                            if (debugFlag) {
                                llvm::outs() << "\n Lhs found in global data str....= "
                                             << list->fetchSLIMArrayOperand()->getOnlyName();
                                llvm::Value *v2 = list->fetchSLIMArrayOperand()->get_gep_main_operand();
                                llvm::outs() << "\n list->fetchSLIMArrayOperand() gep "
                                                "main = "
                                             << v2;
                            }

                            flgFound = true; // array already in global data structure
                            if (indirLhs == 1) {
                                if (debugFlag)
                                    llvm::outs() << "\n Lhs indir is 1....";
                                if (!list->getPtUse().empty()) {
                                    if (debugFlag)
                                        llvm::outs() << "\n Add pointees only "
                                                        "if there is a USE";
                                    LFLivenessSet prevP = list->getArrayPointees();
                                    bool flgNewPointees = checkChangeinPointees(prevP, rhsSet);

                                    /*  if (debugFlag) {
                                          llvm::outs() << "\n #ARRAY
                                      TESTING............"; llvm::outs() << "\n
                                      Previous pointees: "; for (auto r : prevP)
                                             llvm::outs() << "\n : " <<
                                      r->getOnlyName() << " :"; llvm::outs() <<
                                      "\n Current pointee set : "; for (auto r :
                                      rhsSet) llvm::outs() << "\n : " <<
                                      r->getOnlyName() << " :"; llvm::outs() <<
                                      "\n =======================";
                                      }*/

                                    if (flgNewPointees) {
                                        list->setArrayPointees(rhsSet);
                                        list->setFlgChangeInPointees(true);
                                    }

                                    /* if (debugFlag) {
                                       llvm::outs()
                                           << "\n #Testing...printing pointees
                                     of Lhs...."; for (auto r :
                                     list->getArrayPointees()) llvm::outs() <<
                                     "\n : " << r->getOnlyName() << " :";
                                     }*/
                                }
                            } // indir=1
                            else if (indirLhs == 2) {
                                if (debugFlag)
                                    llvm::outs() << "\n Indirection of LHS is 2";
                                LFLivenessSet lhsPoint = list->getArrayPointees();
                                for (auto lp : lhsPoint) {
                                    bool flgPFound = false;
                                    if (GET_KEY(lp)->isArrayElement()) {
                                        if (debugFlag)
                                            llvm::outs() << "\n Lhs pointee is "
                                                            "an array....: "
                                                         << GET_KEY(lp)->getOnlyName();
                                        for (auto list1 : listFISArrayObjects) {
                                            if ((compareOperands(list1->fetchSLIMArrayOperand(), GET_KEY(lp)))) {
                                                flgPFound = true;
                                                list1->setPtDef(I->getBasicBlock());
                                                LFLivenessSet prevP = list1->getArrayPointees();
                                                bool flgNewPointees = checkChangeinPointees(prevP, rhsSet);
                                                if (debugFlag)
                                                    llvm::outs() << "\n Is change in "
                                                                    "pointees: "
                                                                 << flgNewPointees;
                                                if (flgNewPointees) {
                                                    list1->setArrayPointees(rhsSet);
                                                    list1->setFlgChangeInPointees(true);
                                                }
                                            } // end if
                                        } // end for
                                        if (!flgPFound) {
                                            if (debugFlag)
                                                llvm::outs() << "\n Pointee not found "
                                                                "in global DS, so "
                                                                "creating obj \n";
                                            FISArray *newArray = new FISArray(GET_KEY(lp));
                                            newArray->setOnlyArrayName(GET_KEY(lp)->getOnlyName());
                                            newArray->setPtDef(I->getBasicBlock());
                                            newArray->setArrayPointees(rhsSet);
                                            newArray->setFlgChangeInPointees(true);
                                            listFISArrayObjects.push_back(newArray); // insert into global
                                                                                     // data str
                                        }
                                    } // fi
                                    else {
                                        if (debugFlag)
                                            llvm::outs() << "\n LHS pointee is "
                                                            "not an array: "
                                                         << GET_KEY(lp)->getOnlyName();
                                        // lhs pointee not an array
                                        //@case: *arr=&a, arr->x ==> x->a.
                                        // Pointees(x) may not be in PIN.
                                        // First insert the computed RHS
                                        // pointees and then merge the incoming
                                        // pointees from PIN.

                                        OUTofInst = OUTofInst.update_pointees(GET_KEY(lp), rhsSet);
                                        for (auto p : INofInst) {
                                            notPINEmpty = true;
                                            SLIMOperand *Pointer = GET_PTSET_KEY(p);
                                            LFLivenessSet Pointee = GET_VALUE(p);
                                            if (compareOperands(GET_KEY(lp), Pointer)) {
                                                // OUTofInst[lp] =
                                                // backwardMerge(Pointee,
                                                // rhsSet); modAR::LHF
                                                OUTofInst = OUTofInst.update_pointees(
                                                    GET_KEY(lp), backwardMerge(Pointee, rhsSet));

#ifdef PRINTUSEPOINT
                                                llvm::outs() << "\n PRINT USE POINT 24: "
                                                                "ID="
                                                             << I->getInstructionId();
                                                std::map<SLIMOperand *, std::set<SLIMOperand *>> tempPT;
                                                tempPT[lp] = backwardMerge(Pointee, rhsSet);
                                                setPointsToPairForUsePoint(I->getInstructionId(), lp, tempPT);
#endif

#ifdef PREANALYSIS
                                                // #TAG
                                                // if lhsPointee is not an array
                                                // then update the Def and Block
                                                // set
                                                if ((lp->isVariableGlobal() or lp->isFormalArgument() or
                                                     lp->isDynamicAllocationType()) and
                                                    lp->isPointerInLLVM() and !lp->isAlloca()) {

                                                    if (!checkOperandInDEF(valDefBlock.first, lp))
                                                        valDefBlock.first.insert(lp);

                                                    valDefBlock.second = valDefBlock.second - 1; // reduce the block
                                                                                                 // value since
                                                                                                 // pointee is
                                                                                                 // discovered
                                                    if (debugFlag) {
                                                        llvm::outs()
                                                            << "\n Inserted " << lp->getOnlyName() << " in DEF.";
                                                        llvm::outs() << "\n (1) Added "
                                                                        "Def. Block-- ";
                                                    }
                                                } // end if global and pointer
#endif
                                            } // end if
                                        }
                                    } // end else
                                } // for
                            } // indir=2
                        } // end if
                    } // end for
                    if (!flgFound) {
                        /* this case will never occur */
                        /* do nothing */
                    }
                } else {
                    // lhsVal is not an array
                    SLIMOperand *LHS_orig = lhsVal;

                    if (debugFlag) {
                        llvm::outs() << "\n Lhs is not an array with indir = " << indirLhs;
                        llvm::outs() << "\n Again size of rhsSet = " << rhsSet.size();
                        I->printInstruction();
                    }

                    bool flgLinCheck = false; // Keep this check as it is. It is for LOUT
                    for (auto rep : backwardOUT) {
                        if (compareOperands(GET_KEY(rep), lhsVal)) {
                            flgLinCheck = true; // points-to info needs to be
                                                // generated for
                            if (debugFlag)
                                llvm::outs() << "\n LHS is live at OUT";
                        }
                        // lhs and not its source
                    }

                    if (indirLhs == 1 and flgLinCheck == true) {
                        if (debugFlag) {
                            llvm::outs() << "\n LHS is live at out so "
                                            "inserting rhsSet in POUT"
                                         << lhsVal->getOnlyName();
                            llvm::outs() << "\n Size of rhsSET here = " << rhsSet.size();

                            llvm::outs() << "\n Prinitng OUTofInst for testing...";
                            printDataFlowValuesForward(OUTofInst);
                        }
                        if (!rhsSet.empty()) {
                            OUTofInst = OUTofInst.update_pointees(lhsVal, rhsSet); // modAR::LHF
                            // OUTofInst[lhsVal] = rhsSet; // compute PTA only
                            // if LHS_Orig is live
                        }
                    } else if (indirLhs == 2) {
                        if (debugFlag)
                            llvm::outs() << "\n ELSE LOOP indir = 2";

                        /* Following code should execute for both SOURCE and
                         * NOSOURCE */
                        std::queue<SLIMOperand *> qLhs;
                        qLhs.push(lhsVal);

                        while (indirLhs > 1 and !qLhs.empty() and (I->getInstructionType() != GET_ELEMENT_PTR)) {
                            SLIMOperand *lhsTemp = qLhs.front();
                            qLhs.pop();
                            if (debugFlag)
                                llvm::outs() << "\n while loop LhsTemp: " << lhsTemp->getOnlyName()
                                             << "lhsTemp->isPointerVariable() = " << lhsTemp->isPointerVariable();

                            if (/*lhsTemp->isPointerVariable()*/ lhsTemp->isPointerInLLVM() or
                                lhsTemp->isFormalArgument() or lhsTemp->isDynamicAllocationType()) {
                                for (auto p : INofInst) {
                                    notPINEmpty = true;
                                    SLIMOperand *Pointer = GET_PTSET_KEY(p);
                                    LFLivenessSet Pointee = GET_VALUE(p);

                                    if (compareOperands(lhsTemp, Pointer)) {
                                        if (debugFlag)
                                            llvm::outs() << "\n Pointer found in Pin. ";

#ifdef PRINTUSEPOINT
                                        llvm::outs() << "\n PRINT USE POINT 25: ID=" << I->getInstructionId();
                                        std::map<SLIMOperand *, std::set<SLIMOperand *>> tempPT;
                                        tempPT[lhsTemp] = Pointee;
                                        setPointsToPairForUsePoint(I->getInstructionId(), lhsTemp, tempPT);
#endif

                                        for (auto s : Pointee) {
                                            qLhs.push(GET_KEY(s)); // not GEP
                                        } // end inner for
                                    } // fi compareoperands
                                } // end for
                            } // end if
                            indirLhs--;
                        } // end while

                        bool flgMustPointee = false;
                        if (qLhs.size() == 1)
                            flgMustPointee = true;

                        while (!qLhs.empty()) {
                            if (debugFlag)
                                llvm::outs() << "\n Inside while loop for Lhs ";

                            F prevPOUT = OUTofInst;
                            SLIMOperand *pointeeValue = qLhs.front();
                            qLhs.pop();

                            // TODO testing for pointee=?
                            if (pointeeValue->getOperandType() != CONSTANT_INT) { // do something only if the
                                                                                  // pointee is not "?"
                                if (pointeeValue->isArrayElement()) {
                                    if (debugFlag)
                                        llvm::outs() << "\n LHS is an array.........= " << pointeeValue->getOnlyName();

                                    bool flgFound = false;
                                    for (auto list : listFISArrayObjects) {
                                        if ((compareOperands(list->fetchSLIMArrayOperand(), pointeeValue))) {
                                            flgFound = true; // array already in global
                                                             // data structure
                                            LFLivenessSet prevP = list->getArrayPointees();
                                            bool flgNewPointees = checkChangeinPointees(prevP, rhsSet);

                                            if (flgNewPointees) {
                                                list->setArrayPointees(rhsSet);
                                                list->setFlgChangeInPointees(true);
                                            }
                                        } // end if
                                    } // end for

                                    if (!flgFound) {
                                        FISArray *newArray = new FISArray(pointeeValue);
                                        newArray->setOnlyArrayName(pointeeValue->getOnlyName());
                                        newArray->setPtDef(I->getBasicBlock());
                                        listFISArrayObjects.push_back(newArray); // insert into global
                                                                                 // data str
                                    }
                                } // end if array
                                else {
                                    // not an array
                                    if (debugFlag) {
                                        llvm::outs() << "\n ELSE loop lhs not "
                                                        "an array...";
                                        llvm::outs() << "\n #Testing: foundPointee=" << foundPointee
                                                     << " flgMustPointee=" << flgMustPointee;
                                    }

#ifdef PREANALYSIS
                                    /*  (*)Commented the condition. BLOCK should
                              decrement if pointees are found.
                                        (*)Def should be updated irrespective of
                              its type. if ((pointeeValue->isVariableGlobal() or
                              pointeeValue->isFormalArgument() or
                                                 pointeeValue->isDynamicAllocationType())
                                                 and
                              pointeeValue->isPointerInLLVM() and
                              !pointeeValue->isAlloca()) {*/
                                    /* i1=*i, i1 is marked gobal in Slim. Hence
                                     * the condition*/
                                    if (LHS.second == 2) {
                                        valDefBlock.second = valDefBlock.second - 1; // reduce the block value
                                                                                     // since pointee is discovered

                                        if (!checkOperandInDEF(valDefBlock.first, pointeeValue))
                                            valDefBlock.first.insert(pointeeValue);

                                        if (debugFlag) {
                                            llvm::outs() << "\n INserted " << pointeeValue->getOnlyName() << " in DEF.";
                                            llvm::outs() << "\n (2) Added Def. "
                                                            "Block-- == "
                                                         << valDefBlock.second;
                                        }
                                    }
#endif

                                    if (foundPointee) {
                                        if (flgMustPointee) { // 2.9.22
                                            if (debugFlag) {
                                                llvm::outs() << "\n MUST pointee found " << pointeeValue->getName();
                                                llvm::outs() << "\n #Testing...printing "
                                                                "pointee if empty: "
                                                             << rhsSet.size();
                                                for (auto r : rhsSet)
                                                    llvm::outs() << "\n : " << GET_KEY(r)->getOnlyName() << " :";
                                            }
                                            if (!rhsSet.empty()) {
                                                // OUTofInst[pointeeValue] =
                                                // rhsSet; modAR::LHF
                                                OUTofInst = OUTofInst.update_pointees(pointeeValue, rhsSet);
                                            } // fi
                                        } else {
                                            if (debugFlag)
                                                llvm::outs() << "\n May pointee "
                                                                "found...pointeeValue=  "
                                                             << pointeeValue->getName();

                                            bool flgPointeeinPin = false;
                                            for (auto p : INofInst) {
                                                notPINEmpty = true;
                                                SLIMOperand *Pointer = GET_PTSET_KEY(p);
                                                LFLivenessSet Pointee = GET_VALUE(p);

                                                if (debugFlag)
                                                    llvm::outs() << "\n Checking the "
                                                                    "may Pointer: "
                                                                 << Pointer->getName();

                                                if (compareOperands(pointeeValue, Pointer)) {
                                                    if (debugFlag)
                                                        errs() << "\n May Pointer "
                                                                  "found in Pin. ";

                                                    flgPointeeinPin = true;
                                                    OUTofInst = OUTofInst.update_pointees(
                                                        pointeeValue, backwardMerge(Pointee, rhsSet));
/*OUTofInst[pointeeValue] =
    backwardMerge(Pointee, rhsSet);*/  //modAR::LHF
                                                } // end if
                                            } // end for

                                            if (!flgPointeeinPin and !rhsSet.empty()) {
                                                OUTofInst = OUTofInst.update_pointees(
                                                    pointeeValue,
                                                    rhsSet); // required
                                                             // when may
                                                             // pointee has
                                                             // no pta in
                                                             // PIN
                                                             //                        OUTofInst[pointeeValue]
                                                             //                        =
                                                             //                        rhsSet;
                                                             //                        modAR::LHF
                                            } // fi
                                        } // end else
                                    } // end if found pointee
                                } // end else not array
                            } // end outer if
                            else
                                llvm::outs() << "\n Pointee is a ?...hence do nothing.";
                        } // end while
                    } // indireLhs=2
                } // else lhs not an array
            } // end else normal
        } // end else
    } // end else

    /*llvm::outs() << "\n Printing the POUT of the instr....";
    printDataFlowValuesForward(OUTofInst);*/

    // F newOutofInst;

    if (debugFlag) {
        llvm::outs() << "\n Merging prev and curr POUT values";
        llvm::outs() << "\n Printing prevNewOutofInst (before).........";
        printDataFlowValuesForward(prevNewOutofInst);
        llvm::outs() << "\n ----------------------------";
        llvm::outs() << "\n Printing OUTofInst............\n";
        printDataFlowValuesForward(OUTofInst);
        llvm::outs() << "\n ----------------------------";
    }

    tempMergeOutofInst = forwardMerge(prevNewOutofInst, OUTofInst); /* merge new and previous POUT values */

    newOutofInst = tempMergeOutofInst; /* set the value of variable newOutofInst */

    /*
        llvm::outs() << "\n Printing tempMergeOutofInst............\n";
        printDataFlowValuesForward(tempMergeOutofInst);

        llvm::outs() << "\n Printing newOutofInst before............\n";
        printDataFlowValuesForward(newOutofInst);*/

#ifndef PTA
    newOutofInst = restrictByLivness(newOutofInst, backwardOUT);
#endif

    /*    llvm::outs() << "\n Printing newOutofInst after............\n";
        printDataFlowValuesForward(newOutofInst);*/

#ifdef PREANALYSIS
    if (debugFlag) {
        llvm::outs() << "\n Printing inflow for current label..........";
        printDataFlowValuesForward(currInflow.first);
        printDataFlowValuesBackward(currInflow.second);
        llvm::outs() << "\n #Testing Previous Def and Block values for curr "
                        "inflow AGAIN: ";

        objDef.printDataFlowValuesFIS(getTabContextDefBlock(make_pair(currFunction, currInflow)).first);
        llvm::outs() << "\n Prinitng values of current Def and Block.....";
        objDef.printDataFlowValuesFIS(valDefBlock);
    }

    /* Update the table tabContextDefBlock with the newly computed def-block
     * values */
    if (debugFlag) {
        llvm::outs() << "\n Setting the DEF/BLOCK values now...";
        objDef.printDataFlowValuesFIS(valDefBlock);
    }

    // First merge the values
    std::pair<std::set<SLIMOperand *>, int> mergedDefBlock;
    std::pair<std::pair<std::set<SLIMOperand *>, int>, bool> prevDefBlock;
    prevDefBlock = getTabContextDefBlock(make_pair(currFunction, currInflow));

    if (debugFlag) {
        llvm::outs() << "\n PrevDefBlock values :";
        objDef.printDataFlowValuesFIS(prevDefBlock.first);
    }

    mergedDefBlock = objDef.performMeetFIS(prevDefBlock.first, valDefBlock);

    if (debugFlag) {
        llvm::outs() << "\n COPYING SAME BLOCK FLAG to  5 = " << prevDefBlock.second;
        llvm::outs() << "\n after merged DefBlock values :";
        objDef.printDataFlowValuesFIS(mergedDefBlock);
        llvm::outs() << "\n mergeddefblocksecond = " << mergedDefBlock.second;
        llvm::outs() << "\n prevDefBlock.first.second = " << prevDefBlock.first.second;
    }

    // #BLOCK
    if (prevDefBlock.first.second == 1) {
        if (debugFlag) {
            llvm::outs() << "\n Previous flag value is true. So set true";
            llvm::outs() << "\n Previous block value = " << prevDefBlock.first.second;
            llvm::outs() << "\n After merge block value = " << mergedDefBlock.second;
            llvm::outs() << "\n Setting isBlock change flag to true";
        }
        /// llvm::outs() << "\n setTabContextDefBlock-1";
        setTabContextDefBlock(
            make_pair(currFunction, currInflow), make_pair(mergedDefBlock, true)); // set change in BLOCK as true
    }
    if (mergedDefBlock.second == 0 and prevDefBlock.first.second != mergedDefBlock.second) {
        /// llvm::outs() << "\n setTabContextDefBlock-2";
        setTabContextDefBlock(
            make_pair(currFunction, currInflow), make_pair(mergedDefBlock, true)); // set change in BLOCK as true
        if (debugFlag) {
            llvm::outs() << "\n Previous block value-1 = " << prevDefBlock.first.second;
            llvm::outs() << "\n After merge block value = " << mergedDefBlock.second;
            llvm::outs() << "\n Setting isBlock change flag to true";
        }
    } else if (mergedDefBlock.second == prevDefBlock.first.second) {
        /// llvm::outs() << "\n setTabContextDefBlock-3";
        setTabContextDefBlock(make_pair(currFunction, currInflow), make_pair(mergedDefBlock, prevDefBlock.second));
        if (debugFlag) {
            llvm::outs() << "\n Previous block value-2 = " << prevDefBlock.first.second;
            llvm::outs() << "\n After merge block value = " << mergedDefBlock.second;
            llvm::outs() << "\n Setting isBlock change flag to true";
        }
    } else {
        /// llvm::outs() << "\n setTabContextDefBlock-4";
        setTabContextDefBlock(
            make_pair(currFunction, currInflow),
            make_pair(mergedDefBlock, false /*prevDefBlock.second*/)); // table update
        if (debugFlag) {
            llvm::outs() << "\n Else loop Previous block value = " << prevDefBlock.first.second;
            llvm::outs() << "\n After merge block value = " << mergedDefBlock.second;
            llvm::outs() << "\n Setting isBlock change flag to false";
            llvm::outs() << "\n Printing inflow for current label IN THE ELSE "
                            "LOOP FOR TESTING..........";
            printDataFlowValuesForward(currInflow.first);
            printDataFlowValuesBackward(currInflow.second);
        }
    }

    if (debugFlag) {
        llvm::outs() << "\n After Setting the DEF/BLOCK values";
        objDef.printDataFlowValuesFIS(mergedDefBlock);
    }

    if (debugFlag) {
        llvm::outs() << "\n Printing inflow for current label AGAIN FOR "
                        "TESTING..........";
        printDataFlowValuesForward(currInflow.first);
        printDataFlowValuesBackward(currInflow.second);

        llvm::outs() << "\n #Testing Def and Block values after merged: ";
        std::pair<std::pair<std::set<SLIMOperand *>, int>, bool> temp;
        temp = getTabContextDefBlock(make_pair(currFunction, currInflow));
        objDef.printDataFlowValuesFIS(temp.first);
    }
#endif
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif

    return newOutofInst;
}

void IPLFCPA::setPropagatePinFlagForFunContextPair(Function *function, int context, B Lin_Start) {
    mapPropagatePointstoInfoToFunction[make_tuple(function, context, Lin_Start)] = true;
}

bool IPLFCPA::getPropagatePinFlagForFunContextPair(Function *function, int context, B Lin_Start) {
    return mapPropagatePointstoInfoToFunction[make_tuple(function, context, Lin_Start)];
}

void IPLFCPA::propagatePINFromStart(BaseInstruction *I) {

}

std::pair<std::pair<LFLivenessSet, int>, bool> IPLFCPA::performMergeDefBlock(
    std::pair<std::pair<LFLivenessSet, int>, bool> prev, std::pair<std::pair<LFLivenessSet, int>, bool> curr) {

    if (debugFlag) {
        llvm::outs() << "\n Inside performMergeDefBlock............";
        llvm::outs() << "\n Previous value:";
        objDef.printDataFlowValuesFIS(prev.first);
        llvm::outs() << "\n Prev::BLOCK::Flag = " << prev.second;
        llvm::outs() << "\n Current value:";
        objDef.printDataFlowValuesFIS(curr.first);
        llvm::outs() << "\n Curr::BLOCK::Flag = " << curr.second;
    }
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    // prev: callee
    // curr: caller
    std::pair<std::pair<LFLivenessSet, int>, bool> returnValues;
    returnValues.first = curr.first;

    std::pair<LFLivenessSet, int> prevDefBlock = prev.first;
    std::pair<LFLivenessSet, int> returnDefBlock = returnValues.first;

    for (auto a : prevDefBlock.first) {
        returnDefBlock.first = returnDefBlock.first.insert_single(GET_KEY(a)); // modAR::LHF
        // returnDefBlock.first.insert(a); // insert callees def into callers
        if (debugFlag)
            llvm::outs() << "\n Inserted in DEF = " << GET_KEY(a)->getOnlyName();
    }

    int old_value = returnDefBlock.second;
    if (debugFlag)
        llvm::outs() << "\n Old block value = " << old_value;
    if (prev.second) {
        returnDefBlock.second = returnDefBlock.second - 1;
        if (debugFlag) {
            llvm::outs() << "\n Callees BLOCK has changed. Decrement callers block";
            llvm::outs() << "\n New value after decrement = " << returnDefBlock.second;
        }
    }

    if (returnDefBlock.second < 0) {
        returnDefBlock.second = 0; // set to 0
        if (debugFlag)
            llvm::outs() << "\n return value set to 0";
    }

    if (debugFlag) {
        llvm::outs() << "\n Old value is = " << old_value;
        llvm::outs() << "\n New value is = " << returnDefBlock.second;
    }

    if (old_value == 0 or old_value != returnDefBlock.second) // if there is a change
    {
        returnValues.second = true;
        if (debugFlag)
            llvm::outs() << "\n Value set to true";
    } else {
        returnValues.second = false;
        if (debugFlag)
            llvm::outs() << "\n Value set to false";
    }

    returnValues = make_pair(returnDefBlock, returnValues.second);
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return returnValues;
}

F IPLFCPA::getPurelyLocalComponentForward(const F &dfv) const {
    if (debugFlag)
        llvm::outs() << "\n Inside getPurelyLocalComponentForward.............";
    F fwdLocalVal;
    for (auto d : dfv) {
        SLIMOperand *pointer = GET_PTSET_KEY(d);
        LFLivenessSet pointee = GET_VALUE(d);

        if (!pointer->isVariableGlobal()) {
            for (auto p : pointee) {
                /// if (!p->isVariableGlobal())  local->global or local->local
                fwdLocalVal = fwdLocalVal.insert_pointee(pointer, GET_KEY(p)); // modAR::LHF
                                                                               // fwdLocalVal[pointer].insert(p);
            }
        } // end if
    } // end for

    if (debugFlag) {
        llvm::outs() << "\n Printing the forward local componenets";
        printDataFlowValuesForward(fwdLocalVal);
    }
    return fwdLocalVal;
}

pair<F, B> IPLFCPA::CallInflowFunction(
    int current_context_label, Function *target_Function, BasicBlock *bb, const F &a1, const B &d1,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    if (debugFlag)
        llvm::outs() << "\n Inside CallInflowFunction for function =" << target_Function->getName();
    pair<F, B> calleeInflowPair;
    B calleeLOUT;
    F calleePIN;
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif

    if (debugFlag) {
        llvm::outs() << "\n Printing LOUT of call node::";
        printDataFlowValuesBackward(d1);
    }

    // set the backward value
    for (auto d : d1) {
        if (GET_KEY(d)->isVariableGlobal())
            calleeLOUT = calleeLOUT.insert_single(GET_KEY(d)); // modAR::LHF
        // calleeLOUT.insert(d);
        if (return_operand_map.second != nullptr && compareOperands(GET_KEY(d), return_operand_map.first))
            calleeLOUT =
                calleeLOUT.insert_single(return_operand_map.second); // modAR::LHF
                                                                     // calleeLOUT.insert(return_operand_map.second);
    } // for

    if (debugFlag) {
        llvm::outs() << "\n Printing calleee inflow: LOUT ";
        printDataFlowValuesBackward(calleeLOUT);
        llvm::outs() << "\n --------------------------------------------";
    }
    /* *Updated on 27 April 22*
     * Pure local component: local->local
     * Pure global component: global->global
     * Mix component: global->local or local->global
     * No need of global->dangling in callee procedure
     */

    if (debugFlag) {
        llvm::outs() << "\n Printing callee inflow: PIN before ";
        printDataFlowValuesForward(a1);
        llvm::outs() << "\n --------------------------------------------";
    }

    //	llvm::outs() << "\n Checking forward values now......";
    // set the forward value
    for (auto a : a1) {
        if (GET_PTSET_KEY(a)->isVariableGlobal() || GET_PTSET_KEY(a)->isFormalArgument()) {
            // llvm::outs() << "\n Ptr is global: : "<<a.first.first->getName();
            // ptr is global now check pointees
            //   for (auto p : a.second) { modAR::LHF

            for (auto p : LFLivenessSet(GET_VALUE(a))) {
                // llvm::outs() << "\n Checking Pointeess....";
                // if (p.first->isGlobalVar()) {	llvm::outs() << "\n Pointee
                // is global: "<<p.first->getName();

                // calleePIN[a.first].insert(p); modAR::LHF
                calleePIN =
                    calleePIN.insert_pointee(GET_PTSET_KEY(a), GET_KEY(p)); // #ASK ANAMITRA insert_pointee or insert_single?
                // else
                // calleePIN[a.first].insert(std::make_pair(dangling,"-1"));
            } // end inner for
        } // end if
    } // end for

    if (debugFlag) {
        llvm::outs() << "\n Printing PIN_call_node after";
        printDataFlowValuesForward(a1);
        llvm::outs() << "\n Printing calleee inflow: PIN after ";
        printDataFlowValuesForward(calleePIN);
        llvm::outs() << "\n --------------------------------------------";
        llvm::outs() << "\n Caller LOUT";
        printDataFlowValuesBackward(d1);
        llvm::outs() << "\n Callee LOUT_ENdQ ";
        printDataFlowValuesBackward(calleeLOUT);
    }
    calleeInflowPair = std::make_pair(calleePIN, calleeLOUT);
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return calleeInflowPair;
}

pair<F, B> IPLFCPA::CallOutflowFunction(
    int current_context_label, BaseInstruction *I, Function *target_Function, BasicBlock *bb, const F &a3, const B &d3,
    const F &a1, const B &d1, pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    if (debugFlag) {
        llvm::outs() << "\n Inside CallOutflowFunction.....................";
        llvm::outs() << "\n Current context label: " << current_context_label;
        llvm::outs() << "\n Target function : " << target_Function->getName();
        llvm::outs() << "\nPrinting data flow values : OUTFLOW backward ";
        printDataFlowValuesBackward(d3);
        llvm::outs() << "\nPrinting data flow values : at LOUT of instr ";
        printDataFlowValuesBackward(d1);
        llvm::outs() << "\n Printing a3: ";
        printDataFlowValuesForward(a3);
        llvm::outs() << "\n Printing a1: ";
        printDataFlowValuesForward(a1);
    }
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    pair<F, B> retOUTflow;
    B callnodeLin;
    callnodeLin = d3;

    B LocalComponent = getPurelyLocalComponentBackward(d1);

    if (debugFlag) {
        llvm::outs() << "\n Printing the purely local components...";
        printDataFlowValuesBackward(LocalComponent);
        llvm::outs() << "\n Printing callnodeLin...";
        printDataFlowValuesBackward(callnodeLin);
    }

    // Merge the backward outflow with the local components
    SLIMOperand *CallerRetArg = return_operand_map.first;
    for (auto i : LocalComponent) {
        if (CallerRetArg != nullptr) {
            if (!compareOperands(GET_KEY(i), CallerRetArg)) // x = Q(a,b); If x is local and is live
                                                            // after call then its liveness should be
                                                            // killed. return_operand_map contains the
                                                            // mapping x===retVal_in_Q
                // callnodeLin.insert(i); modAR::LHF
                callnodeLin = callnodeLin.insert_single(GET_KEY(i));
        } // outer if
    } // for

    retOUTflow.second = callnodeLin; // imp step
    if (debugFlag) {
        llvm::outs() << "\n #TESTING Backwrad computed outflow: ";
        printDataFlowValuesBackward(retOUTflow.second);
    }
    //---------------  Forward Outflow of Callee. Replacement of Instruction IDs
    retOUTflow.first = a3; // check once!!

    //@CHeck if indirect call then only perform return argument mapping
    Instruction *Inst = I->getLLVMInstruction();
    CallInstruction *cTempInst = new CallInstruction(Inst);
    if (cTempInst->isIndirectCall()) {
        if (debugFlag)
            llvm::outs() << "\n CALL IS INDIRECT. PERFORM RETURN ARGUMENT "
                            "MAPPING........";
        // Return value mapping in forwards analysis
        SLIMOperand *argCaller = return_operand_map.first;
        SLIMOperand *argCallee = return_operand_map.second;
        F retForwardOutflow;
        // a3:forward_outflow d1: LOUT_callnode
        if (argCallee != nullptr) { // do this only for functions with return argument
            for (auto pin : a3) {
                if (compareOperands(GET_PTSET_KEY(pin), argCallee)) { // x=call fun() and return x' in
                                                                // callee then x'->a ==> x->a
                    // retForwardOutflow[argCaller] = GET_VALUE(pin);
                    retForwardOutflow = retForwardOutflow.update_pointees(argCaller, GET_VALUE(pin));
                } else {
                    // retForwardOutflow.insert(pin); // if y->b  in a3 ==>
                    // retForwardOutflow contain y->b
                    retForwardOutflow = retForwardOutflow.update_pointees(GET_PTSET_KEY(pin), GET_VALUE(pin)); // modAR::LHF
                }
            }
            if (debugFlag) {
                llvm::outs() << "\nForward Outflow after updating callee "
                                "return arg with "
                                "caller arg : ";
                printDataFlowValuesForward(retForwardOutflow);
            }
            retOUTflow.first = retForwardOutflow; // restrictByLivness(retForwardOutflow,
                                                  // d1); // retForwardOutflow:
                                                  // with return arg mapping
        } else
            retOUTflow.first = a3; // restrictByLivness(a3, d1); // a3:
                                   // forward_outflow   d1:LOUT_Callnode
    }

//@Commenting following code since SLIM has return argument mapping instructions
// now. 9.6.25
/*  // Return value mapping in forwards analysis
  SLIMOperand *argCaller = return_operand_map.first;
  SLIMOperand *argCallee = return_operand_map.second;
  F retForwardOutflow;
  //a3:forward_outflow d1: LOUT_callnode
  if (argCallee != nullptr) { // do this only for functions with return argument
    for (auto pin : a3) {
      if (compareOperands(GET_KEY(pin), argCallee)) {//x=call fun() and return x' in
  callee then x'->a ==> x->a
        //retForwardOutflow[argCaller] = pin.second; modAR::LHF
        retForwardOutflow = retForwardOutflow.update_pointees(argCaller,
  GET_VALUE(pin)); } else {
        //retForwardOutflow.insert(pin);// if y->b  in a3 ==> retForwardOutflow
  contain y->b retForwardOutflow = retForwardOutflow.update_pointees(GET_KEY(pin),
  GET_VALUE(pin)); //modAR::LHF
      }
    }
    if (debugFlag) {
      llvm::outs() << "\nForward Outflow after updating callee return arg with "
                      "caller arg : ";
      printDataFlowValuesForward(retForwardOutflow);
    }
    retOUTflow.first = restrictByLivness(retForwardOutflow, d1);
  //retForwardOutflow: with return arg mapping
  }
  else
    retOUTflow.first = restrictByLivness(a3, d1);  //a3: forward_outflow
  d1:LOUT_Callnode
*/

#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif

    if (debugFlag) {
        llvm::outs() << "\n Printing the computed OUTFLOW &&&&&&";
        llvm::outs() << "\n Forward computed outflow: ";
        printDataFlowValuesForward(retOUTflow.first);
        llvm::outs() << "\n Backwrad computed outflow: ";
        printDataFlowValuesBackward(retOUTflow.second);
    }
    return retOUTflow;
}

Value *v1 = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 000000,
                                   1); // treat as undefined

SLIMOperand *dummyOper0 = new SLIMOperand(v1, true);
/*SLIMOperand *dummyOper0 = OperandRepository::getOrCreateSLIMOperand(v1);*/

Value *v = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 999999, 1);
SLIMOperand *dummyOper9 = new SLIMOperand(v, true);
/*SLIMOperand *dummyOper9 = OperandRepository::getOrCreateSLIMOperand(v);*/

SLIMOperand *IPLFCPA::createDummyOperand(int code) {

    if (code == 9) { // for marker
        //       Value *v =
        //       llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 999999,
        //       1);
        //      SLIMOperand* dummyOper = new SLIMOperand(v, true);
        return dummyOper9;
    } else if (code == 0) { // for question mark
        //       Value *v =
        //       llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 000000,
        //       1);
        //      SLIMOperand* dummyOper = new SLIMOperand(v, true);
        return dummyOper0;
    } else {
        llvm_unreachable("Encountered unreachable case for dummy operand");
    }
}

F IPLFCPA::functionCallEffectForward(Function *target_function, const F &forwardIN) {
    return forwardIN;
}

B IPLFCPA::functionCallEffectBackward(Function *target_function, const B &backwardOUT) {

    return backwardOUT;
}

void IPLFCPA::printIndices(SLIMOperand *slimOp) const {
    // llvm::outs() << slimOp->getOnlyName();
    if (slimOp->getNumIndices() > 0) {
        for (int indIter = 0; indIter < slimOp->getNumIndices(); indIter++) {
            llvm::Value *index_val = slimOp->getIndexOperand(indIter)->getValue();
            if (llvm::isa<llvm::Constant>(index_val)) {
                if (llvm::isa<llvm::ConstantInt>(index_val)) {
                    llvm::ConstantInt *constant_int = llvm::cast<llvm::ConstantInt>(index_val);
                    llvm::outs() << "[" << constant_int->getSExtValue() << "]";
                } else {
                    llvm_unreachable("[GetElementPtrInstruction Error] The index is a "
                                     "constant but not an integer constant!");
                }
            } else {
                llvm::outs() << "[" << index_val->getName() << "]";
            }
        }
    } else {
        llvm::outs() << ":No indices\n";
    }
}

// New methods added on 3.2.23
pair<pair<F, B>, pair<F, B>> IPLFCPA::getInitialisationValueGenerativeKill() {
    // llvm::outs() << "\n Inside getInitialisationValueGenerativeKill";
    F F_TOP;
    B B_TOP;
    pair<pair<F, B>, pair<F, B>> initGenKill;
    return initGenKill;
}

pair<F, B> IPLFCPA::computeGenerative(int context_label, Function *Func) {
    pair<F, B> retVal;
    return retVal;
}

F IPLFCPA::performMeetForward(const F &d1, const F &d2) {
    // F retMeetINF = d1;
    // for (auto it : d2) {
    //     SLIMOperand *key = GET_KEY(it);
    //     LFLivenessSet value = GET_VALUE(it);
    //     LFLivenessSet prevValue = retMeetINF.get_pointees(key);
    //     bool flag = false;

    //     if (!prevValue.empty()) {
    //         for (auto pv : prevValue) {
    //             for (auto nv : value) {
    //                 if (!compareOperands(GET_KEY(nv), GET_KEY(pv)))
    //                     flag = true;
    //             }
    //         }
    //     } else {
    //         flag = true;
    //     }
    //     if (flag) {
    //         for (auto itValues : value)
    //             retMeetINF = retMeetINF.insert_pointee(key, GET_KEY(itValues));
    //     }
    // }

    F retMeetINF = d1.set_union(d2);
    return retMeetINF;
    // return d1.set_union(d2);
}

F IPLFCPA::performMeetForwardTemp(F d1, F d2) {
//    F retMeetINF = d1;
//     for (auto it : d2) {
//         SLIMOperand *key = GET_KEY(it);
//         LFLivenessSet value = GET_VALUE(it);
//         LFLivenessSet prevValue = retMeetINF.get_pointees(key);
//         bool flag = false;
//         // llvm::outs()<< "\n New value " << key->getName() << "\n";
//         if (!prevValue.empty()) {
//             for (auto pv : prevValue) {
//                 for (auto nv : value) {
//                     if (!compareOperands(GET_KEY(nv), GET_KEY(pv)))
//                         flag = true;
//                     /*else if (!(nv.second == pv.second))
//                        flag = true;	*/
//                 }
//             }
//         } else {
// #ifdef PRVASCO
//             llvm::outs() << "\n Key not in prevPOUT";
// #endif
//             flag = true;
//         }
//         if (flag) {
//             for (auto itValues : value)
//                 retMeetINF = retMeetINF.insert_pointee(key, GET_KEY(itValues));
//         }
//     }
    F retMeetINF = d1.set_union(d2);
    return retMeetINF;
    // return d1.set_union(d2);
}

pair<F, B> IPLFCPA::performMeetGenerative(const pair<F, B> &g1, const pair<F, B> &g2) {
    // Create an object of Gen class and compute Gen flow insensitively for Func
    // FISP_Gen objGen(module, true, true, false, FISP, false);
    // std::pair<std::set<SLIMOperand*>,std::set<SLIMOperand*>> gen1, gen2;
    F resFwd; // = performMeetForwardTemp(g1.first, g2.first);
    B resBwd; // = performMeetBackward(g1.second, g2.second);
    return make_pair(resFwd, resBwd);
}

pair<F, B> IPLFCPA::computeKill(int context_label, Function *Func) {
    pair<F, B> retVal;
    return retVal;
}

pair<F, B> IPLFCPA::CallInflowFunction(
    int context_label, Function *Func, BasicBlock *bb, const F &a1, const B &d1, pair<F, B> generativeAtCall,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    B inflowLv;
    F inflowPta;
    return make_pair(inflowPta, inflowLv);
}
/***************************** New Function *************/

void IPLFCPA::setTabContextDefBlock(
    std::pair<Function *, std::pair<F, B>> pairFunInflow,
    std::pair<std::pair<LFLivenessSet, int>, bool> DefBlockValues) {
    if (debugFlag) {
        llvm::outs() << "\n Inside setTabContextDefBlock..............";
        llvm::outs() << "\n BLOCK FLG = " << DefBlockValues.second;
    }
    tabContextDefBlock[pairFunInflow] = DefBlockValues;
}

std::pair<std::pair<LFLivenessSet, int>, bool>
IPLFCPA::getTabContextDefBlock(pair<Function *, pair<F, B>> pairFunInflow) {
    return tabContextDefBlock[pairFunInflow];
}

long long IPLFCPA::getLastInsIDForFunction(Function *Func) {
    Function &function = *Func;
    BasicBlock *bb = &function.back();
    return optIR->getLastIns(Func, bb);
}

bool IPLFCPA::isFirstInstruction(BaseInstruction *I) {

    llvm::Function *func = I->getFunction();
    llvm::BasicBlock *bb = I->getBasicBlock();
    Function &function = *func;
    BasicBlock &b = *bb;

    if (debugFlag) {
        llvm::outs() << "\n Inside isFirstInstruction......";
        llvm::outs() << "\n Function name = " << func->getName();
        llvm::outs() << "\n I->getInstructionId() = " << I->getInstructionId()
                     << "   optIR->getFirstIns(bb->getParent(),bb)  = " << optIR->getFirstIns(bb->getParent(), bb);
    }

    if (bb == (&function.getEntryBlock())) {
        if (I->getInstructionId() == optIR->getFirstIns(bb->getParent(), bb))
            return true;
    }
    return false;
}

pair<F, B> IPLFCPA::CallInflowFunction(
    int context_label, Function *Func, BasicBlock *bb, const F &a1, const B &d1, int source_context_label,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    if (debugFlag)
        llvm::outs() << "\n Inside CallInflowFunction for LFCPA function = " << Func->getName();
    pair<F, B> calleeInflowPair;
    B calleeLOUT;
    F calleePIN;
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif

    if (debugFlag) {
        llvm::outs() << "\n Printing LOUT of call node::";
        printDataFlowValuesBackward(d1);
    }

    // set the backward value
    for (auto d : d1) {
        if (GET_KEY(d)->isVariableGlobal())
            calleeLOUT = calleeLOUT.insert_single(GET_KEY(d)); // modAR::LHF
        // calleeLOUT.insert(d);
        if (return_operand_map.second != nullptr && compareOperands(GET_KEY(d), return_operand_map.first))
            calleeLOUT = calleeLOUT.insert_single(return_operand_map.second); // modAR::LHF
    } // for

    if (debugFlag) {
        llvm::outs() << "\n Printing calleee inflow: LOUT ";
        printDataFlowValuesBackward(calleeLOUT);
        llvm::outs() << "\n --------------------------------------------";
    }
    /* *Updated on 27 April 22*
     * Pure local component: local->local
     * Pure global component: global->global
     * Mix component: global->local or local->global
     * No need of global->dangling in callee procedure
     */

    if (debugFlag) {
        llvm::outs() << "\n Printing callee inflow: PIN before ";
        printDataFlowValuesForward(a1);
        llvm::outs() << "\n --------------------------------------------";
    }

    //	llvm::outs() << "\n Checking forward values now......";
    // set the forward value
    for (auto a : a1) {
        if (GET_PTSET_KEY(a)->isVariableGlobal() || GET_PTSET_KEY(a)->isFormalArgument()) {
            for (auto p : LFLivenessSet(GET_VALUE(a))) {
                calleePIN =
                    calleePIN.insert_pointee(GET_PTSET_KEY(a), GET_KEY(p)); // #ASK ANAMITRA insert_pointee or insert_single?
            } // end inner for
        } // end if
    } // end for

    if (debugFlag) {
        llvm::outs() << "\n Printing PIN_call_node after";
        printDataFlowValuesForward(a1);
        llvm::outs() << "\n Printing calleee inflow: PIN after ";
        printDataFlowValuesForward(calleePIN);
        llvm::outs() << "\n --------------------------------------------";
        llvm::outs() << "\n Caller LOUT";
        printDataFlowValuesBackward(d1);
        llvm::outs() << "\n Callee LOUT_ENdQ ";
        printDataFlowValuesBackward(calleeLOUT);
    }
    calleeInflowPair = std::make_pair(calleePIN, calleeLOUT);
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return calleeInflowPair;
}

pair<F, B> IPLFCPA::CallOnflowFunction(
    int context_label, BaseInstruction *I, pair<F, B> killAtCall,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    // /*if (debugFlag)
    //   llvm::outs() << "\n Inside CallOnflowFunction";
    // B bOUTofInst = getBackwardComponentAtOutOfThisInstruction(I);
    // F fINofInst = getForwardComponentAtInOfThisInstruction(I);
    // B setKillValues = killAtCall.second;
    // return computeSetDifference(fINofInst, bOUTofInst, setKillValues,
    //                             return_operand_map);*/
    // pair<F, B> retVal;
    // return retVal;
    llvm_unreachable("Should not get called");
}

pair<F, B> IPLFCPA::CallOnflowFunctionForIndirect(
    int context_label, BaseInstruction *I, int source_context_label,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map, Function *target_function) {
    llvm_unreachable("Should not get called");
}

bool IPLFCPA::isBlockChangedForForward(int context_label, BaseInstruction *I) {
    std::pair<std::pair<LFLivenessSet, int>, bool> temp;
    temp = getTabContextDefBlock(make_pair(I->getFunction(), getInflowforContext(context_label)));

    std::pair<std::pair<LFLivenessSet, int>, bool> fieldsTabContextDefBlock;
    std::pair<LFLivenessSet, int> valDefBlock, valDefBlockCaller;

    if (debugFlag) {
        llvm::outs() << "\n Inside isBlockChangedForForward...";
        llvm::outs() << "\n Function name= " << I->getFunction();
    }
    pair<F, B> prevInflow = getInflowforContext(context_label);
    fieldsTabContextDefBlock = getTabContextDefBlock(make_pair(I->getFunction(), prevInflow));
    valDefBlock = fieldsTabContextDefBlock.first;

    /* if (debugFlag) {
       llvm::outs() << "\n Printing the prevInflow........";
       printDataFlowValuesForward(prevInflow.first);
       printDataFlowValuesBackward(prevInflow.second);
       llvm::outs() << "\n DEF BLOCK values; ";
      // objDef.printDataFlowValuesFIS(valDefBlock);
       llvm::outs() << "\n Flag = " << temp.second;
       llvm::outs() << "\n Flag agian = " << fieldsTabContextDefBlock.second;
     }*/
    return temp.second;
}

pair<F, B> IPLFCPA::CallOnflowFunction(
    int context_label, BaseInstruction *I, int source_context_label,
    pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    llvm_unreachable("Should not get called");
}

pair<F, B> IPLFCPA::computeSetDifference(
    F setIN, B setOUT, LFLivenessSet setDef, pair<SLIMOperand *, SLIMOperand *> return_operand_map) {
    if (debugFlag) {
        llvm::outs() << "\n Inside computeSetDifference.....";
        llvm::outs() << "\n #Testing printing the value......";
        llvm::outs() << "\n Forward setIN: ";
        printDataFlowValuesForward(setIN);
        llvm::outs() << "\n Backward setOUT: ";
        printDataFlowValuesBackward(setOUT);
        // llvm::outs() << "\n Def values: ";
        // printDataFlowValuesBackward(setDef);
    }
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    SLIMOperand *callerArg = return_operand_map.first;
    SLIMOperand *calleeArg = return_operand_map.second;

    // setOut - setKill
    // setIN- setKill

    pair<F, B> result, retVal;
    int constIntValue;
    bool flgDoNothing = false;

    if (debugFlag) {
        if (calleeArg == nullptr)
            llvm::outs() << "\n Calleee Arg is nullptr";
        else
            llvm::outs() << "\n Calleee Arg is NOT nullptr";
    }
    // if (!flgDoNothing) {
    for (auto o : setOUT) {
        bool flgMatch = false;
        if (calleeArg == nullptr || !compareOperands(callerArg, GET_KEY(o))) {
            for (auto k : setDef) {
                if (compareOperands(GET_KEY(k), GET_KEY(o))) {
                    if (debugFlag)
                        llvm::outs() << "\n k->getValue: " << GET_KEY(k)->getName();
                    flgMatch = true;
                    break;
                }
            }
            if (!flgMatch) {
                result.second = result.second.insert_single(GET_KEY(o));
            }
            // result.second.insert(o); modAR::LHF
        }
    }
    //}

    if (debugFlag) {
        llvm::outs() << "\n LOut-Def values: ONFLOW ";
        printDataFlowValuesBackward(result.second);
        llvm::outs() << "\n Calling restrictbyLivenes...........";
    }

    result.first = restrictByLivness(setIN, result.second);
    if (debugFlag)
        llvm::outs() << "Called restrictByLivness in computeSetDiff";

    if (debugFlag) {
        llvm::outs() << "\n Pin|Lin_start values: ";
        printDataFlowValuesForward(result.first);
        llvm::outs() << "\n ONFLOW INFORMATION ";
        llvm::outs() << "\n Forward PIN: ";
        printDataFlowValuesForward(setIN);
        llvm::outs() << "\n Backward LOUT: ";
        printDataFlowValuesBackward(setOUT);
        llvm::outs() << "\n DEF: ";
        for (auto s : setDef)
            llvm::outs() << "\n defValue: " << GET_KEY(s)->getOnlyName();
        llvm::outs() << "\n LOut-Def values: ONFLOW ";
        printDataFlowValuesBackward(result.second);
        llvm::outs() << "\n Pin|Lin_start values: ";
        printDataFlowValuesForward(result.first);
    }
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
    return result;
}

bool IPLFCPA::isFunctionIgnored(llvm::Function *Fun) {

    // if by error, pointers are not idenitified for any functions
    // OR if identifyPtrFun() method is called, then pointerFunList is empty.
    // thus, no function is ignored since this might not be the correct result
    if (pointerFunList.size() == 0)
        return false;

    auto pos = pointerFunList.find(Fun);
    if (pos == pointerFunList.end())
        return true;
    return false;
}

void IPLFCPA::simplifyIR(slim::IR *optIR) {
    BaseInstruction *I;
    for (auto &entry : optIR->func_bb_to_inst_id) {
        for (long long instruction_id : entry.second) {
            I = optIR->inst_id_to_object[instruction_id];
            Instruction *Inst = I->getLLVMInstruction();
            std::pair<SLIMOperand *, int> LHS = I->getLHS();
            std::vector<std::pair<SLIMOperand *, int>> RHS = I->getRHS();

            if (I->getInstructionType() == 8 or I->getInstructionType() == BRANCH or
                I->getInstructionType() == ALLOCA) {
                I->setIgnore();
            } else if (I->getInstructionType() != CALL and I->getInstructionType() != RETURN and LHS.second == 0) {
                I->setIgnore();

            } else if (I->getInstructionType() == STORE) {
                if (!LHS.first->isPointerInLLVM()) {
                    for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end(); r++) {
                        SLIMOperand *rhsVal = r->first;
                        if (rhsVal->getOperandType() == CONSTANT_INT) {
                            I->setIgnore();
                        } else if (!rhsVal->isPointerInLLVM()) {
                            I->setIgnore();
                        }
                    }
                } else if (LHS.first->isPointerInLLVM()) {
                    for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end(); r++) {
                        SLIMOperand *rhsVal = r->first;

                        if (!rhsVal->isPointerInLLVM() and r->second == 1) {
                            I->setIgnore();
                        }
                    } // end for
                } // end else
            } // end store
            else if (I->getInstructionType() == RETURN) {
                ReturnInstruction *RI = (ReturnInstruction *)I;
                if (RI->getReturnOperand()->getOperandType() == CONSTANT_INT) {
                    I->setIgnore();
                }
            } else if (I->getInstructionType() == PHI) {
                if (!LHS.first->isPointerInLLVM())
                    I->setIgnore();
            } else if (I->getInstructionType() == GET_ELEMENT_PTR) {
#ifdef IGNORE
                I->setIgnore();
#endif
            } else if (LHS.first->getOperandType() == GEP_OPERATOR) {
#ifdef IGNORE
                I->setIgnore();
#endif
            } else if (LHS.first->getOperandType() != GEP_OPERATOR) {
                for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end(); r++) {
                    SLIMOperand *rhsVal = r->first;

                    if (rhsVal->getOperandType() == GEP_OPERATOR) {
#ifdef IGNORE
                        I->setIgnore();
#endif
                    }
                } // end for
            } else if (LHS.first->isArrayElement()) {
#ifdef IGNORE
                I->setIgnore();
#endif
            } else if (!LHS.first->isArrayElement()) {
                for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end(); r++) {
                    SLIMOperand *rhsVal = r->first;

                    if (rhsVal->isArrayElement()) {

#ifdef IGNORE
                        I->setIgnore();
#endif
                    }
                } // end for
            }
        } // end inner for
    } // outer for
}

F IPLFCPA::performCallReturnArgEffectForward(
    const F &dfv, const pair<SLIMOperand *, SLIMOperand *> return_operand_map) const {
    return dfv;
}

B IPLFCPA::performCallReturnArgEffectBackward(
    const B &dfv, const pair<SLIMOperand *, SLIMOperand *> return_operand_map) const {
    if (debugFlag)
        llvm::outs() << "\n Inside performCallReturnArgEffectBackward...............";
    // Kill the liveness of w, in stmt w = call Q() explicitly
    B result;
    SLIMOperand *callerArg = return_operand_map.first;
    SLIMOperand *calleeArg = return_operand_map.second;
    if (calleeArg == nullptr || callerArg == nullptr)
        return dfv;

    for (auto o : dfv) {
        if (!compareOperands(callerArg, GET_KEY(o)))
            result = result.insert_single(GET_KEY(o));
    } // for

    /*  if (debugFlag) {
        llvm::outs() << "\n Printing the result of callreturnArg effect: ";
        for (auto a : result)
              llvm::outs() << a->getOnlyName()<<", ";
      }*/
    return result;
}

F IPLFCPA::getGlobalandFunctionArgComponentForward(const F &dfv) const {
    // llvm::outs() << "\n Inside
    // getGlobalandFunctionArgComponentForward...............";

    return getPurelyGlobalComponentForward(dfv);
}

B IPLFCPA::getGlobalandFunctionArgComponentBackward(const B &dfv) const {
    // llvm::outs() << "\n Inside
    // getGlobalandFunctionArgComponentBackward..............."; add liveness of
    // function arguments of the callee in the outflow irrespective of being
    // local to the callee.
    B global_component;
    if (!dfv.empty()) {
        for (auto v : dfv) {
            if (GET_KEY(v)->isVariableGlobal() || GET_KEY(v)->isFormalArgument() ||
                GET_KEY(v)->isDynamicAllocationType()) {
                global_component = global_component.insert_single(GET_KEY(v)); // modAR::LHF
            }
        } // end for
    }
    return global_component;
}

// Functions to return basic blocks for reprocessing for
// flow-insensitive array computation
set<BasicBlock *> IPLFCPA::getMixedFISBasicBlocksBackward() {
    set<BasicBlock *> usePtBBs;

    for (auto arrObj : listFISArrayObjects) {
        if (arrObj->getFlgChangeInPointees()) {
            set<BasicBlock *> usePtforArr = arrObj->getPtUse();
            if (usePtforArr.size() != 0)
                usePtBBs.insert(usePtforArr.begin(), usePtforArr.end());
            arrObj->setFlgChangeInPointees(false);
        }
    }

    return usePtBBs;
}

set<BasicBlock *> IPLFCPA::getMixedFISBasicBlocksForward() {
    set<BasicBlock *> defPtBBs;

    for (auto arrObj : listFISArrayObjects) {
        if (arrObj->getFlgChangeInUse()) {
            set<BasicBlock *> defPtforArr = arrObj->getPtDef();
            if (defPtforArr.size() != 0)
                defPtBBs.insert(defPtforArr.begin(), defPtforArr.end());
            arrObj->setFlgChangeInUse(false); // #TODO reset only if defset not empty
        }
    }

    return defPtBBs;
}

// Flow insensitive array computation
FISArray::FISArray(SLIMOperand *op) {
    this->opdArray = op;
}

SLIMOperand *FISArray::fetchSLIMArrayOperand() {
    return this->opdArray;
}

void FISArray::setOnlyArrayName(llvm::StringRef aName) {
    this->nmArray = aName;
}

llvm::StringRef FISArray::getOnlyArrayName() {
    return this->nmArray;
}

void FISArray::setPtUse(BasicBlock *bb) {
    this->ptUse.insert(bb);
}

std::set<BasicBlock *> FISArray::getPtUse() {
    return this->ptUse;
}

void FISArray::setPtDef(BasicBlock *bb) {
    this->ptDef.insert(bb);
}

std::set<BasicBlock *> FISArray::getPtDef() {
    return this->ptDef;
}

void FISArray::setFlgChangeInPointees(bool v) {
    this->flgChangeInPointees = v;
}

bool FISArray::getFlgChangeInPointees() {
    return this->flgChangeInPointees;
}

void FISArray::setFlgChangeInUse(bool v) {
    this->flgChangeInUse = v;
}

bool FISArray::getFlgChangeInUse() {
    return this->flgChangeInUse;
}

bool FISArray::isPtUseNotEmpty() {
    if (this->ptUse.empty())
        return false;
    return true;
}

LFLivenessSet FISArray::getArrayPointees() {
    return this->arrPointees;
}

void FISArray::setArrayPointees(LFLivenessSet currPointee) {

    LFLivenessSet prevPointee = this->getArrayPointees();

    if (debugFlag) {
        llvm::outs() << "\n Inside  setArrayPointees............ for " << this->getOnlyArrayName() << "\n";
        llvm::outs() << "\n Printing prev pointees: ";
        for (auto p : prevPointee)
            llvm::outs() << "\t: " << GET_KEY(p)->getOnlyName() << " :";
        llvm::outs() << "\n Printing curr pointees: ";
        for (auto p : currPointee)
            llvm::outs() << "\t: " << GET_KEY(p)->getOnlyName() << " :";
    }
    for (auto val : currPointee)                               // prevOUT merge succIN
        prevPointee = prevPointee.insert_single(GET_KEY(val)); // modAR::LHF
    // prevPointee.insert(val);

    this->arrPointees = prevPointee;

    if (debugFlag) {
        llvm::outs() << "\n Printing pointees after merging: ";
        for (auto p : this->getArrayPointees())
            llvm::outs() << "\t: " << GET_KEY(p)->getOnlyName() << " :";
    }
}

void IPLFCPA::setForwardINForNonEntryBasicBlock(pair<int, BasicBlock *> current_pair) {
    // step 6
    setForwardIn(current_pair.first, current_pair.second, getInitialisationValueForward());

    BasicBlock *bb = current_pair.second;
    // step 7 and 8
    for (auto pred_bb : predecessors(bb)) {
        // llvm::outs() << "\n";
        //  get first instruction of bb and setCurrentInstruction(inst);
        BaseInstruction *firstInst = optIR->inst_id_to_object[optIR->getFirstIns(bb->getParent(), bb)];
        setCurrentInstruction(firstInst);

        setForwardIn(
            current_pair.first, current_pair.second,
            performMeetForward(
                getIn(current_pair.first, current_pair.second).first, getOut(current_pair.first, pred_bb).first,
                "IN")); // CS_BB_OUT[make_pair(current_pair.first,pred_bb)].first
    }

}

// std::set<BasicBlock *>
// IPLFCPA::manageWorklistForward(LFInstLivenessSet LOUT_BB, BasicBlock *currBB, Function *currFunction) {
//     if (debugFlag) {
//         llvm::outs() << "\n Inside manageWorklistForward................";
//         // llvm::outs() << "\n PRINTING LOUT_BB....";
//         // printDataFlowValuesBackward(LOUT_BB);
//         llvm::outs() << "\n currBB = " << currBB->getName();
//         llvm::outs() << "\n currFunction = " << currFunction->getName();
//     }
//     std::set<BasicBlock *> ret_BB;
//     for (auto lout : LOUT_BB) {
//         for (auto id : LFInstructionSet(GET_VALUE(lout))) {
//             BaseInstruction *I = optIR->getInstrFromIndex(GET_KEY(id)); // get instruction from ins_id
//             ///   llvm::outs() << "\n #TESTING Printing inst for id = ["<<id<<"]
//             ///   ";
//             /// I->printInstruction();

//             // mod::AR If currBB==lastBB and use_point==lastins then dont add
//             // into the worklist
//             Function &function = *currFunction;
//             BasicBlock *lastBB = &function.back();
//             BasicBlock &b = *currBB;

//             //   if (currBB != lastBB and id !=
//             //   getLastInsIDForFunction(currFunction)){
//             if (currBB != lastBB or GET_KEY(id) != getLastInsIDForFunction(currFunction)) {
//                 ret_BB.insert(I->getBasicBlock()); // get basicblock from instr

//                 if (debugFlag) {
//                     llvm::outs() << "\n INSTRUCTION ID = " << GET_KEY(id) << " INSTRUCTION = ";
//                     I->printInstruction();
//                     llvm::outs() << "\n BASIC BLOCK = " << I->getBasicBlock();
//                 } // fi
//             } // fi
//         } // for
//     } // for
//     return ret_BB;
// }

/**********  Functions to compute USE points and corresponding points-to
 * information for each instruction *****/

void IPLFCPA::setUsePoint(long long insID, SLIMOperand *Operand) {
    std::pair<LFLivenessSet, LFPointsToSet> temp = mapUsePointAndPtpairs[insID];

    llvm::outs() << "\n Inside setUsePoint.... inserting operand = " << Operand->getOnlyName();
    temp.first = temp.first.insert_single(Operand); // modAR::LHF
    mapUsePointAndPtpairs[insID] = temp;
}
//----------
void IPLFCPA::setPointsToPairForUsePoint(long long insID, SLIMOperand *operand, LFPointsToSet pointsToPairs) {

    if (debugFlag) {
        llvm::outs() << "\n Inside setPointsToPairForUsePoint.............";
        llvm::outs() << "\n Instruction ID == " << insID;
    }
    std::pair<LFLivenessSet, LFPointsToSet> temp = mapUsePointAndPtpairs[insID];
    for (auto ptr : temp.first) {
        if (compareOperands(GET_KEY(ptr), operand)) {
            if (debugFlag) {
                llvm::outs() << "\n ptr name = " << GET_KEY(ptr)->getOnlyName();
                llvm::outs() << "\n operand name = " << operand->getOnlyName();
                llvm::outs() << "\n Before merging...orig pairs ";
                //   printDataFlowValuesForward(temp.second);
                llvm::outs() << "\n new points-to pairs ";
                //  printDataFlowValuesForward(pointsToPairs);
            }
            temp.second = performMeetForwardTemp(temp.second, pointsToPairs);
            mapUsePointAndPtpairs[insID] = temp;

            if (debugFlag) {
                llvm::outs() << "\n After merging ";
                printDataFlowValuesForward(temp.second);
            }
        } // if
    } // for
}

void IPLFCPA::printUsePointAndPtpairs() {
    llvm::outs() << "\n\n Displaying the USE points and the Points-to "
                    "Information reaching the Instructions...";
    for (auto i : mapUsePointAndPtpairs) {
        llvm::outs() << "\n\n [" << i.first << "]" << " INSTRUCTION: ";
        optIR->getInstrFromIndex(i.first)->printInstruction();
        std::pair<LFLivenessSet, LFPointsToSet> temp = i.second;
        llvm::outs() << " USE POINTS: ";
        for (auto u : temp.first)
            llvm::outs() << GET_KEY(u)->getOnlyName() << ", ";
        llvm::outs() << "\n POINTS-TO PAIRS: ";
        printDataFlowValuesForward(temp.second);
        llvm::outs() << "\n -----------------------------------------";
    }
}

void IPLFCPA::preAnalysis(std::unique_ptr<llvm::Module> &mod, slim::IR *optIR) {

    // if (debugFlag)
    // llvm::outs() << "\n Inside preAnalysis function...............";
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    int dummyContext = -99;

    CallGraph CG = CallGraph(*mod);
    // CG.dump();

    for (scc_iterator<CallGraph *> SCCI = scc_begin(&CG); !SCCI.isAtEnd(); ++SCCI) {
        const std::vector<CallGraphNode *> &nextSCC = *SCCI;
        if (nextSCC.size() > 1) {
            std::vector<Function *> vfun;
            for (std::vector<CallGraphNode *>::const_iterator I = nextSCC.begin(), E = nextSCC.end(); I != E; ++I) {
                vfun.push_back((*I)->getFunction());
            }
            objDef.doAnalysisVec(module, optIR, vfun, dummyContext);
        } else if (nextSCC.size() == 1 and nextSCC[0]->getFunction() != nullptr) {
            /* if (debugFlag) {
               llvm::outs() << "\n Single Function name = "
                            << nextSCC[0]->getFunction()->getName();
             }*/
            objDef.doAnalysis(module, optIR, nextSCC[0]->getFunction(), dummyContext);
        }
    } // end for
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
#endif
#ifdef PRINT
    llvm::outs() << "\n Printing PREANALYSIS values.........";
    objDef.printMapDefBlock();
#endif
}

//--------------------------------
int main(int argc, char *argv[]) {
    // llvm::outs() << "\n POINT 1";
    if (argc < 2 || argc > 2) {
        llvm::errs() << "We expect exactly one argument - the name of the LLVM "
                        "IR file!\n";
        exit(1);
    }
#ifdef PRINTSTATS
    stat.timerStart(__func__);
#endif
    /// stat.timerStart("testprofiling");
    llvm::SMDiagnostic smDiagnostic;

    module = parseIRFile(argv[1], smDiagnostic, context);

    if (!module.get()) {
        llvm::outs() << "Error: Could not load '" << argv[1] << "'. Exiting.\n";
        return 1;
    }

    // IPLFCPA lfcpaObj(true,true);
    /*string*/ fileName = argv[1];
    fileName += ".txt";

    llvm::outs() << "\nPopulating CallGraph START\n";
    lfcpaObj.populateCallGraphEdges(module);
    llvm::outs() << "\nPopulating CallGraph END\n";

    // IPLFCPA lfcpaObj(module,fileName,true,true,false,FSFP,true); ---globally
    // declared IPLFCPA lfcpaObj(module,true,true,false,FIFP);
    lfcpaObj.startSplitting(module);

    // Generate SLIM IR
    /* With Optimized IR
      slim::IR *transformIR = new slim::IR(module);
      optIR = transformIR->optimizeIR();
      optIR->dumpIR();
      exit(0);
      */

    /* Without using optimizedIR*/
    slim::IR *optIR = new slim::IR(module);
#ifdef SLIMIR
    optIR->dumpIR();
    exit(0);
#endif

    DBG_total_insts = optIR->getTotalInstructions();
    // lfcpaObj.identifyNoPtrFun(optIR);
    lfcpaObj.identifyPtrFun(optIR);

    // llvm::outs() << "\nPointer Functions identified END\n";

    // simplify SLIM IR. Mark ignored instructions
    lfcpaObj.simplifyIR(optIR);

#ifdef RELV_INS
    computeRelevantIns(optIR);
    exit(0);
#endif

#ifdef PREANALYSIS
    // compute the Defset summary for every procedure
    // std::unique_ptr<llvm::Module> &module1 = transformIR->getLLVMModule();
    std::unique_ptr<llvm::Module> &module1 = optIR->getLLVMModule();
    lfcpaObj.preAnalysis(module1, optIR);
#endif

    //  std::unique_ptr<llvm::Module> &module = transformIR->getLLVMModule();
    std::unique_ptr<llvm::Module> &module = optIR->getLLVMModule();
    lfcpaObj.doAnalysis(module, optIR);
    llvm::outs() << "\n\n@@@@ OPERATION COUNT: " << STAT_operation_count << "\n";
    /// lfcpaObj.printBasicBlockLists();
    lfcpaObj.printStatistics();
    llvm::outs() << "\n\n";

#ifdef PRINTUSEPOINT
    lfcpaObj.printUsePointAndPtpairs();
#endif

/// stat.timerEnd("testprofiling");
#ifdef PRINTSTATS
    stat.timerEnd(__func__);
    ///  stat.dump();
#endif
    /*   llvm::outs() << "Liveness: \n";
       llvm::outs() << livenessLHF.dump() << "\n" << livenessLHF.dump_perf();

       llvm::outs() << "PointstoSets: \n";
       llvm::outs() << pointsToLHF.dump() << "\n" << pointsToLHF.dump_perf();
   */
    return 0;
}
