/*******************************************************************************************************
                  "Flow-Sensitive Fixed Point Pass for Def set summary
computation." Author:      Aditi Raste Created on:  8/8/2023 Description: Last
Updated: Current Status:
**********************************************************************************************************/
#include "FSFP_Def.h"

FSFP_Def::FSFP_Def(
    std::unique_ptr<llvm::Module> &module, bool debug, bool SLIM, bool intraProc, ExecMode modeOfExec, bool byPass)
    : AnalysisDef<F3, B3>(module, debug, SLIM, intraProc, modeOfExec, byPass) {}

F3 FSFP_Def::computeOutFromIn(llvm::Instruction &I, int depContext) {
    llvm::outs() << "\n Inside computeOutFromIn 1...........";
    F3 temp;
    return temp;
}

F3 FSFP_Def::computeOutFromIn(BaseInstruction *I, int depContext) {
    llvm::outs() << "\n Inside computeOutFromIn 2...........";
    F3 temp;
    return temp;
}

F3 FSFP_Def::performMeetForward(const F3 &d1, const F3 &d2, const string pos) {
    llvm::outs() << "\n Inside performMeetForward FSFP_Def...........";
    F3 temp;
    return temp;
}

bool FSFP_Def::EqualDataFlowValuesForward(const F3 &d1, const F3 &d2) const {
    llvm::outs() << "\n Inside EqualDataFlowValuesForward FSFP_Def...........";
    return true;
}

bool FSFP_Def::EqualContextValuesForward(const F3 &d1, const F3 &d2) const {
    llvm::outs() << "\n Inside EqualContextValuesForward FSFP_Def...........";
    return false;
}

F3 FSFP_Def::getPurelyLocalComponentForward(const F3 &dfv) const {
    llvm::outs() << "\n Inside getPurelyLocalComponentForward FSFP_Def...........";
    F3 temp;
    return temp;
}

F3 FSFP_Def::getPurelyGlobalComponentForward(const F3 &dfv) const {
    llvm::outs() << "\n Inside getPurelyGLobalComponentForward FSFP_Def...........";
    F3 temp;
    return temp;
}

F3 FSFP_Def::getMixedComponentForward(const F3 &dfv) const {
    llvm::outs() << "\n Inside getMixedGLobalComponentForward FSFP_Def...........";
    F3 temp;
    return temp;
}

F3 FSFP_Def::getCombinedValuesAtCallForward(const F3 &dfv1, const F3 &dfv2) const {
    llvm::outs() << "\n Inside getCombinedValuesAtCallForward FSFP_Def...........";
    F3 temp;
    return temp;
}

void FSFP_Def::printDataFlowValuesForwardFSFP(const F3 &dfv) const {
    llvm::outs() << "\n Inside printDataFlowValuesForward FSFP_Def...........";
}

F3 FSFP_Def::performMergeAtCallForward(const F3 &d1, const F3 &d2) {
    llvm::outs() << "\n Inside performMergeAtCallForward FSFP_Def...........";
    F3 temp;
    return temp;
}

F3 FSFP_Def::getBoundaryInformationForward() {
    llvm::outs() << "\n Inside getBoundaryInformationForward FSFP_DEF ";
    F3 F_TOP;
    return F_TOP;
}

F3 FSFP_Def::getInitialisationValueForward() {
    llvm::outs() << "\n Inside getInitialisationValueForward FSFP_DEF";
    F3 F_TOP;
    return F_TOP;
}

pair<F3, B3> FSFP_Def::CallInflowFunction(int context_label, Function *F, BasicBlock *B, const F3 &a1, const B3 &d1) {
    llvm::outs() << "\n Inside CallInflowFunction FSFP_Def...";
    std::pair<F3, B3> temp;
    return temp;
}

pair<F3, B3> FSFP_Def::CallOutflowFunction(
    int context_label, Function *F, BasicBlock *B, const F3 &a1, const B3 &d1, const F3 &a2, const B3 &d2) {
    llvm::outs() << "\n Inside CallOutflowFunction FSFP_Def...";
    std::pair<F3, B3> temp;
    return temp;
}
