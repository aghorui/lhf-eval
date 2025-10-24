/*******************************************************************************************************
                  "Pre-analysis Pass for Flow-Insensitive Single Pass Def and
Block Computation." Author:      Aditi Raste Created on:  8/8/2023 Description:
Last Updated:
Current Status:
**********************************************************************************************************/
#include "FISP_Def.h"

/*
 *  DEF set should contain only global variables but not alloca variables.
 * Locals and temporaries should not be added into DEF set.
 */

// to display all indirect calls
bool indirectCalls = /*true*/ false;

#ifdef PRINT
bool debugFlag1 = /*true*/ false;
#endif

#ifndef PRINT
bool debugFlag1 = false;
#endif

FISP_Def::FISP_Def(
    std::unique_ptr<llvm::Module> &module, bool debug, bool SLIM, bool intraProc, ExecMode modeOfExec, bool byPass)
    : AnalysisDef<F3, B3>(module, debug, SLIM, intraProc, modeOfExec, byPass) {}

// #ASK ANAMITRA
bool FISP_Def::checkInDef(SLIMOperand *curr, F3 prevSet) {
    for (auto p : prevSet.first) {
        if (compareDefValues(curr, GET_KEY(p)))
            return true;
    }
    return false;
}

bool FISP_Def::compareIndices_Def(std::vector<SLIMOperand *> ipVec1, std::vector<SLIMOperand *> ipVec2) {
    if (ipVec1.size() != ipVec2.size())
        return false;

    for (int i = 0; i < ipVec1.size(); i++) {
        if (ipVec1[i]->getValue() != ipVec2[i]->getValue())
            return false;
    }
    return true;
}
// #ASK ANAMITRA
bool FISP_Def::compareDefValues(SLIMOperand *a1, SLIMOperand *a2) {
    if (a1 == a2)
        return true;

    if (a1->isArrayElement() and a2->isArrayElement()) {
        llvm::Value *v1 = a1->getValueOfArray();
        llvm::Value *v2 = a2->getValueOfArray();
        if (v1 == v2)
            return true;
    } else {
        if (a1->getValue() == a2->getValue() and compareIndices_Def(a1->getIndexVector(), a2->getIndexVector()))
            return true;
    }
    return false;
}

F3 FISP_Def::computeDFValueFIS(BaseInstruction *I, int dummyContext, F3 prevdfv) {
    // if (debugFlag1)
    // llvm::outs() << "\n Inside computeDFValueFIS...........";

    Function *F = I->getFunction();
    F3 retDefBlockValues;

    if (indirectCalls) {
        if (I->getCall()) { // getCall() present in new SLIM
            Instruction *Inst = I->getLLVMInstruction();
            CallInstruction *cTempInst = new CallInstruction(Inst);
            if (cTempInst->isIndirectCall()) {
                llvm::outs() << "\n -----------------------------------------";
                llvm::outs() << "\n INDIRECT CALL FOUND ";
                llvm::outs() << "\n Instruction ID: " << I->getInstructionId() << "\n";
                I->printInstruction();
                llvm::outs() << "\n Source File name: " << _getFileName(I)
                             << " Line No: " << _getSourceLineNumberForInstruction(I);
                llvm::outs() << "\n -----------------------------------------";
            }
        }
    }
    // copy prevdfv into retDefBlockValues
    retDefBlockValues = prevdfv;

    if (I->getInstructionType() != RETURN and I->getInstructionType() != ALLOCA) {
        std::pair<SLIMOperand *, int> LHS = I->getLHS();
        int indirLhs = LHS.second;

        if (LHS.second == 1) {
            if (LHS.first->isVariableGlobal() and !LHS.first->isAlloca()) {
                if (LHS.first->isArrayElement() and LHS.first->isPointerInLLVM()) {
#ifndef COMPUTE_CIS_FIS
                    if (debugFlag1)
                        llvm::outs() << "LHS is an array. COMPUTE_CIS_FIS not "
                                        "set. Insert LHS.";
                    // retDefBlockValues.first.insert(LHS.first); modAR::LHF
                    retDefBlockValues.first = retDefBlockValues.first.insert_single(LHS.first);
#endif
                } // array
                else {
                    if (LHS.first->isPointerVariable()) {
                        if (debugFlag1)
                            llvm::outs() << "\n LHS is not an array. Inserting Lhs.";
                        ///// if (!checkInDef(LHS.first,retDefBlockValues))
                        // retDefBlockValues.first.insert(LHS.first); modAR::LHF
                        retDefBlockValues.first = retDefBlockValues.first.insert_single(LHS.first);
                    }
                    // else
                    //  llvm::outs() << "\n Lhs is not a pointer in LLVM.";
                }
            } // if global
        } // end indir=1
        else if (LHS.second == 2) {
            //  llvm::outs() << "\n Instruction ID: " << I->getInstructionId()
            //  << "\n";
            // I->printInstruction();

            if (debugFlag1)
                llvm::outs() << "\n Lhs indir = 2. Increment Block counter.....";
            if (!LHS.first->isArrayElement() and LHS.first->isPointerVariable()) // update BLOCK only if LHS is
                                                                                 // not an array and is a pointer
                retDefBlockValues.second += 1;
        } // end indir=2

        /*  if (I->getCall()) {
       Instruction* Inst = I->getLLVMInstruction();
           CallInstruction* cTempInst = new CallInstruction(Inst);
           if(cTempInst->isIndirectCall())
         retDefBlockValues.second += 1;
          }*/
    } // end outer if

    if (debugFlag1) {
        llvm::outs() << "\n #Testing. Printing Def............";
        llvm::outs() << "\n DEFSET: ";
        llvm::outs() << "{ ";
        for (auto val : retDefBlockValues.first)
            llvm::outs() << GET_KEY(val)->getName() << ", "; // check this
        llvm::outs() << " }";
        llvm::outs() << "\n BLOCKSET = " << retDefBlockValues.second;
    }

    // Update the def/block values in global map
    setMapFunDefBlock(F, retDefBlockValues);

    return retDefBlockValues;
}

F3 FISP_Def::performMeetFIS(const F3 &d1, const F3 &d2) {
    if (debugFlag1) {
        llvm::outs() << "\n Inside performMeetFIS...........";
        llvm::outs() << "\n Printing Value 1: ";
        printDataFlowValuesFIS(d1);
        llvm::outs() << "\n Printing Value 2: ";
        printDataFlowValuesFIS(d2);
    }
    // std::set<SLIMOperand *> DefSet;
    LFLivenessSet DefSet = LFLivenessSet(d1.first);

    // long int BlockSet = max(d1.second, d2.second);
    long int BlockSet = d1.second + d2.second;

    for (auto def : d2.first)
        //  DefSet = DefSet.insert_single(def); //modAR::LHF
        DefSet = DefSet.insert_single(GET_KEY(def));

    if (debugFlag1) {
        llvm::outs() << "\n #Testing after merge..........";
        llvm::outs() << "\n DEF: ";
        for (auto a : DefSet)
            llvm::outs() << GET_KEY(a)->getOnlyName() << ", ";
        llvm::outs() << "\n BLOCK: " << BlockSet;
        llvm::outs() << "\n ======================================";
    }
    return std::make_pair(DefSet, BlockSet);
}

bool FISP_Def::EqualDataFlowValuesFIS(const F3 &d1, const F3 &d2) const {
    //  if (debugFlag1)
    //   llvm::outs() << "\n Inside EqualDataFlowValuesFIS FISP_Def...........";

    if (d1.first == d2.first and d1.second == d2.second)
        return true;
    return false;
}

F3 FISP_Def::getPurelyLocalComponentFIS(const F3 &dfv) const {
    if (debugFlag1)
        llvm::outs() << "\n Inside getPurelyLocalComponentFIS FISP_Def...........";

    LFLivenessSet locDef;
    for (auto d : dfv.first) {
        if (!GET_KEY(d)->isVariableGlobal() || !GET_KEY(d)->isFormalArgument())
            // locDef = locDef.insert_single(d); //modAR::LHF
            locDef = locDef.insert_single(GET_KEY(d));
    }
    return std::make_pair(locDef, dfv.second);
}

F3 FISP_Def::getPurelyGlobalComponentFIS(const F3 &dfv) const {
    // if (debugFlag1)
    //  llvm::outs() << "\n Inside getPurelyGlobalComponentFIS
    //  FISP_Def...........";

    LFLivenessSet gDef;

    for (auto d : dfv.first) {
        if (GET_KEY(d)->isVariableGlobal() || GET_KEY(d)->isFormalArgument())
            // gDef = gDef.insert_single(d); //modAR::LHF
            gDef = gDef.insert_single(GET_KEY(d));
    }
    return std::make_pair(gDef, dfv.second);
}

void FISP_Def::printDataFlowValuesFIS(const F3 &dfv) const {
    //  llvm::outs() << "\n Inside printDataFlowValuesFIS FISP_Def...........";
    //  llvm::outs() << "\n Printing  DEF and Block values: ";
    llvm::outs() << "\n Def: ";
    llvm::outs() << "{ ";
    for (auto val : dfv.first)
        llvm::outs() << GET_KEY(val)->getOnlyName() << ", "; // check this
    llvm::outs() << " }";
    llvm::outs() << "\n Block count =  " << dfv.second << "\n";
}

F3 FISP_Def::performMergeAtCallFIS(const F3 &d1, const F3 &d2) {
    //  if (debugFlag1)
    //   llvm::outs() << "\n Inside performMergeAtCallFIS FISP_Def...........";
    return performMeetFIS(d1, d2);
}

F3 FISP_Def::getBoundaryInformationFIS() {
    // llvm::outs() << "\n Inside getBoundaryInformationFIS FISP_DEF ";
    F3 F_TOP;
    return F_TOP;
}

F3 FISP_Def::getInitialisationValueFIS() {
    // llvm::outs() << "\n Inside getInitialisationValueFIS FISP_DEF";
    F3 F_TOP;
    F_TOP.second = 0;
    return F_TOP;
}

F3 FISP_Def::CallInflowFunctionFIS(int context_label, Function *F, BasicBlock *B, const F3 &a1) {
    // llvm::outs() << "\n Inside CallInflowFunctionFIS FISP_Def...";
    F3 temp;
    return temp;
}

F3 FISP_Def::CallOutflowFunctionFIS(
    int context_label, Function *F, BasicBlock *B, const F3 &outflow, const F3 &currOUT) {
    if (debugFlag1) {
        llvm::outs() << "Inside CallOutflowFunctionFIS FISP_Def..";
        llvm::outs() << "\n Function = " << F->getName();
        llvm::outs() << "\n Printing outflow: ";
        printDataFlowValuesFIS(outflow);
        llvm::outs() << "\n Printing currOUT: ";
        printDataFlowValuesFIS(currOUT);
    }
    return performMeetFIS(outflow, currOUT);
}

int FISP_Def::getDepAnalysisCalleeContext(int callerContext, BaseInstruction *inst) {
    return -99;
}

void FISP_Def::setMapFunDefBlock(Function *F, std::pair<LFLivenessSet, int> valDefBlock) {
    mapFunDefBlock[F] = valDefBlock;
}

std::pair<LFLivenessSet, int> FISP_Def::getMapFunDefBlock(Function *F) {
    return mapFunDefBlock[F];
}

void FISP_Def::printMapDefBlock() {
    llvm::outs() << "\n Inside printMapDefBlock............";
    for (auto M : mapFunDefBlock) {
        llvm::outs() << "\n Function: " << M.first->getName();
        llvm::outs() << "\n Def: ";
        for (auto d : M.second.first)
            llvm::outs() << GET_KEY(d)->getOnlyName() << ", ";
        llvm::outs() << "\n Block: " << M.second.second;
    }
    llvm::outs() << "\n ---------------------------------";
}
