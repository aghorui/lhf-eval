/*******************************************************************************************************
                  "Flow-Insensitive Single Pass "
Author:      Aditi Raste
Created on:  15/2/2023
Description:
Last Updated:
Current Status:
**********************************************************************************************************/
#include "FISP_Gen.h"

FISP_Gen::FISP_Gen(
    std::unique_ptr<llvm::Module> &module, bool debug, bool SLIM, bool intraProc, ExecMode modeOfExec, bool byPass)
    : AnalysisGenKill<F2, B2>(module, debug, SLIM, intraProc, modeOfExec, byPass) {}

F2 FISP_Gen::getBoundaryInformationFIS() {
    // llvm::outs() << "\n Inside getBoundaryInformationFIS ";
    F2 F_TOP;
    return F_TOP;
}

F2 FISP_Gen::getInitialisationValueFIS() {
    // llvm::outs() << "\n Inside getInitialisationValueFIS ";
    F2 F_TOP;
    return F_TOP;
}

bool FISP_Gen::EqualDataFlowValuesFIS(const F2 &d1, const F2 &d2) const {
    llvm::outs() << "\n Inside FISP_Gen EqualDataFlowValuesFIS.................";
    llvm::outs() << "\n Current values...";
    printDataFlowValuesFIS(d1);
    llvm::outs() << "\n Previous values...";
    printDataFlowValuesFIS(d2);
    if (d1.first == d2.first and d1.second == d2.second)
        return true;
    return false;
}

F2 FISP_Gen::performMergeAtCallFIS(const F2 &d1, const F2 &d2) {
    return performMeetFIS(d1, d2);
}

F2 FISP_Gen::performMeetFIS(const F2 &d1, const F2 &d2) {
    // llvm::outs() << "\n Inside performMeetFIS...................";
    std::set<SLIMOperand *> DefSet, RefSet;
    RefSet = d1.first;
    DefSet = d1.second;

    for (auto ref : d2.first)
        RefSet.insert(ref);
    for (auto def : d2.second)
        DefSet.insert(def);

    F2 retMergedSet = std::make_pair(RefSet, DefSet);

    return retMergedSet;
}

F2 FISP_Gen::getPurelyGlobalComponentFIS(const F2 &dfv) const {
    // llvm::outs() << "\n Inside getPurelyGlobalComponentFIS...";
    std::set<SLIMOperand *> gRef, gDef;

    for (auto r : dfv.first) {
        if (r->isVariableGlobal())
            gRef.insert(r);
    }
    for (auto d : dfv.second) {
        if (d->isVariableGlobal())
            gDef.insert(d);
    }
    return std::make_pair(gRef, gDef);
}

void FISP_Gen::printDataFlowValuesFIS(const F2 &dfv) const {
    llvm::outs() << "\n Printing REF and DEF values: ";
    llvm::outs() << "\n Ref: ";
    llvm::outs() << "{ ";
    for (auto val : dfv.first)
        llvm::outs() << val->getName() << ", "; // check this
    llvm::outs() << " }";
    llvm::outs() << "\n Def: ";
    llvm::outs() << "{ ";
    for (auto val : dfv.second)
        llvm::outs() << val->getName() << ", "; // check this
    llvm::outs() << " }\n";
}

F2 FISP_Gen::getPurelyLocalComponentFIS(const F2 &dfv) const {
    // llvm::outs() << "\n Inside getPurelyLocalComponentFIS...";
    std::set<SLIMOperand *> locRef, locDef;
    for (auto r : dfv.first) {
        if (!r->isVariableGlobal())
            locRef.insert(r);
    }
    for (auto d : dfv.second) {
        if (!d->isVariableGlobal())
            locDef.insert(d);
    }
    return std::make_pair(locRef, locDef);
}

F2 FISP_Gen::getGenValforDepObjContext(
    std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, std::set<SLIMOperand *>> *, int> depObCon) {
    return mapDepObjGen[depObCon];
}

void FISP_Gen::setDependentObj(
    std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, std::set<SLIMOperand *>> *, int> depObj) {
    varDependentObj = depObj;
}

std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, std::set<SLIMOperand *>> *, int>
FISP_Gen::getDependentObj() {
    return varDependentObj;
}

/*int FISP_Gen::getDepAnalysisCalleeContext(int callerContext, BaseInstruction*
inst){ return varDependentObj.first->getCalleeContext(callerContext, inst);
}*/

std::set<SLIMOperand *> FISP_Gen::insertRhsintoRef(BaseInstruction *I, int depAnalysisContext) {
    llvm::outs() << "\n Inside function insertRhsintoRef............";

    std::set<SLIMOperand *> REF;
    // Fetch Lhs and Rhs of ins
    std::pair<SLIMOperand *, int> LHS = I->getLHS();
    std::vector<std::pair<SLIMOperand *, int>> RHS = I->getRHS();
    int indirLhs = LHS.second;

    std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, std::set<SLIMOperand *>> *, int> dObject =
        getDependentObj();
    std::map<SLIMOperand *, std::set<SLIMOperand *>> PIN;
    PIN = dObject.first->getForwardComponentAtInOfThisInstruction(
        I, depAnalysisContext); // depAnalysisContext instead of dbObject.second

    for (std::vector<std::pair<SLIMOperand *, int>>::iterator r = RHS.begin(); r != RHS.end(); r++) {
        SLIMOperand *rhsValue = r->first;
        int indirRhs = r->second;
        /* GEP instr check added on 1.3.2023 */
        if (indirRhs == 0) { // (n,1) = (i[0][0],0) def=n ref=i
            /* check if rhsValue is not a part of the index (9.3.2023) */
            if (I->getInstructionType() == GET_ELEMENT_PTR and !llvm::isa<llvm::Constant>(rhsValue->getValue())) {
                llvm::outs() << "\n FISP_Gen: Instr is a GEP:: " << rhsValue->getName();
                REF.insert(rhsValue);
            }
        } else if (indirRhs == 1) {
            llvm::outs() << "\n Rhs indir=1 val= " << rhsValue->getName();
            REF.insert(rhsValue);
        } else if (indirRhs == 2) {
            std::queue<SLIMOperand *> q;
            q.push(rhsValue);
            while (indirRhs > 1 and !q.empty()) {
                SLIMOperand *rhsTemp = q.front();
                llvm::outs() << "\n Rhs indir=2 val= " << rhsTemp->getName();
                REF.insert(rhsTemp);
                q.pop();
                for (auto fIN : PIN) {
                    if (rhsTemp->isArrayElement()) {
                        if ((fIN.first->getOnlyName() == rhsTemp->getOnlyName()) &&
                            (fIN.first->getIndexVector().size() == rhsTemp->getIndexVector().size())) {
                            std::set<SLIMOperand *> rhsPointees = fIN.second;
                            for (auto pointee : rhsPointees) {
                                // llvm::outs() << "\n Pointee of Rhs found in
                                // Pin: "<<pointee->getName();
                                q.push(pointee);
                            }
                        }
                    } else {
                        if (fIN.first->getOnlyName() == rhsTemp->getOnlyName() and
                            compareIndices(fIN.first->getIndexVector(), rhsTemp->getIndexVector())) {
                            std::set<SLIMOperand *> rhsPointees = fIN.second;
                            for (auto pointee : rhsPointees) {
                                // llvm::outs() << "\n Pointee of Rhs found in
                                // Pin: "<<pointee->getName();
                                q.push(pointee);
                            }
                        } // end if
                    }
                } // end inner for
                indirRhs--;
            } // end while

            while (!q.empty()) {
                SLIMOperand *rhsTemp = q.front();
                REF.insert(rhsTemp);
                q.pop();
            }
        } // end else 2
    } // end for

    llvm::outs() << "\n #Testing. Printing Ref...........";
    llvm::outs() << "\n REFSET: ";
    llvm::outs() << "{ ";
    for (auto val : REF)
        llvm::outs() << val->getName() << ", "; // check this
    llvm::outs() << " }";

    return REF;
}

F2 FISP_Gen::computeDFValueFIS(BaseInstruction *I, int depAnalysisContext) {

    llvm::outs() << "\n Inside function computeDFValueFIS............";
    std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, std::set<SLIMOperand *>> *, int> dObject =
        getDependentObj();
    std::map<SLIMOperand *, std::set<SLIMOperand *>> PIN;
    std::set<SLIMOperand *> LOUT;

    // long long ID = I->getInstructionId();
    // BaseInstruction* ins = dObject.first->getBaseInstruction(ID);

    llvm::outs() << "\n INSTRUCTION TYPE : " << I->getInstructionType();
    llvm::outs() << "\n***************";

    llvm::outs() << "\n Fetching PIN and LOUT for context : " << depAnalysisContext << " to compute Generative ";
    PIN = dObject.first->getForwardComponentAtInOfThisInstruction(
        I, depAnalysisContext); // depAnalysisContext instead of dObject.second
    LOUT = dObject.first->getBackwardComponentAtOutOfThisInstruction(I, depAnalysisContext);

    if (LOUT.empty())
        llvm::outs() << "\n LOUT is EMPTY...........";
    else {
        llvm::outs() << "\n LOUT is NOT EMPTY...........";
        for (auto l : LOUT)
            llvm::outs() << l->getName() << ", ";
        llvm::outs() << "\n $$$$$$$$$$$$$$$$$$$$$$";
    }
    if (PIN.empty())
        llvm::outs() << "\n PIN is EMPTY...........";
    else {
        llvm::outs() << "\n PIN is NOT EMPTY...........";
        for (auto i : PIN)
            llvm::outs() << "\n pin.first = " << i.first->getName();
    }
    F2 infoGenerative;
    std::set<SLIMOperand *> REF, DEF;

    if (I->getInstructionType() != RETURN and I->getInstructionType() != ALLOCA) {
        std::pair<SLIMOperand *, int> LHS = I->getLHS();
        std::vector<std::pair<SLIMOperand *, int>> RHS = I->getRHS();
        int indirLhs = LHS.second;

        if (LHS.second == 1) {
            DEF.insert(LHS.first);
            llvm::outs() << "\n Lhs value indir=1....DEF set entry...";
            if (LHS.first->isArrayElement()) {
                for (auto out : LOUT) {
                    if ((out->getOnlyName() == LHS.first->getOnlyName()) &&
                        (out->getIndexVector().size() == LHS.first->getIndexVector().size()))
                        REF = insertRhsintoRef(I, depAnalysisContext);
                }
            } else {
                auto pos = LOUT.find(LHS.first);
                if (pos != LOUT.end()) {
                    llvm::outs() << "\n Entered loop LHS= " << LHS.first->getName();
                    REF = insertRhsintoRef(I, depAnalysisContext);
                }
            }
        } else if (LHS.second == 2) {
            llvm::outs() << "\n Lhs indir=2...";
            REF.insert(LHS.first);
            std::queue<SLIMOperand *> q;
            q.push(LHS.first);
            while (indirLhs > 1 and !q.empty()) {
                SLIMOperand *lhsTemp = q.front();
                q.pop();
                for (auto fIN : PIN) {
                    if (LHS.first->isArrayElement()) {
                        if ((fIN.first->getOnlyName() == LHS.first->getOnlyName()) &&
                            (fIN.first->getIndexVector().size() == LHS.first->getIndexVector().size())) {
                            std::set<SLIMOperand *> pointees = fIN.second;
                            for (auto point : pointees)
                                q.push(point);
                        }
                    } else {

                        if (fIN.first->getOnlyName() == LHS.first->getOnlyName() and
                            compareIndices(fIN.first->getIndexVector(), LHS.first->getIndexVector())) {
                            std::set<SLIMOperand *> pointees = fIN.second;
                            for (auto point : pointees)
                                q.push(point);
                        } // end if
                    }
                }
                indirLhs--;
            } // end while

            bool flgCheck = false;
            while (!q.empty()) {
                SLIMOperand *pointee = q.front();
                q.pop();
                /* Below condition added on 10.3.2023 */
                if (pointee->getOperandType() != CONSTANT_INT) {
                    DEF.insert(pointee);
                    if (!flgCheck) {
                        // auto pos = LOUT.find(pointee);
                        // if (pos != LOUT.end()) {
                        for (auto lout : LOUT) {
                            if (pointee->isArrayElement()) {
                                if ((lout->getOnlyName() == pointee->getOnlyName()) &&
                                    (lout->getIndexVector().size() == pointee->getIndexVector().size())) {
                                    std::set<SLIMOperand *> tmpRef = insertRhsintoRef(I, depAnalysisContext);
                                    REF.insert(tmpRef.begin(), tmpRef.end());
                                    flgCheck = true;
                                }
                            } else {
                                if (lout->getOnlyName() == pointee->getOnlyName() and
                                    compareIndices(lout->getIndexVector(), pointee->getIndexVector())) {
                                    std::set<SLIMOperand *> tmpRef = insertRhsintoRef(I, depAnalysisContext);
                                    REF.insert(tmpRef.begin(), tmpRef.end());
                                    flgCheck = true;
                                } // end if
                            }
                        } // end for
                    } // end if flgchk
                } // end if not const
            } // end while
        } // end else indir=2
    } // end if not return

    llvm::outs() << "\n #Testing. Printing Def............";
    llvm::outs() << "\n DEFSET: ";
    llvm::outs() << "{ ";
    for (auto val : DEF)
        llvm::outs() << val->getName() << ", "; // check this
    llvm::outs() << " }";

    infoGenerative = std::make_pair(REF, DEF);
    // set Generative info for the dependent object + context pair in
    // mapDepObjKill
    llvm::outs() << "\n Updating the Generative value for the dependent object "
                    "and context pair in the map.";
    printDataFlowValuesFIS(infoGenerative);
    mapDepObjGen[dObject] = performMeetFIS(mapDepObjGen[dObject], infoGenerative);
    return infoGenerative;
}

F2 FISP_Gen::CallInflowFunctionFIS(int context_label, Function *F, BasicBlock *B, const F2 &dfv) {
    // llvm::outs() << "\n Inside CallInflowFunctionFIS...";
    F2 retVal;
    return retVal;
}

F2 FISP_Gen::CallOutflowFunctionFIS(
    int context_label, Function *F, BasicBlock *B, const F2 &outflow, const F2 &currOUT) {
    // llvm::outs() << "Inside CallOutflowFunctionFIS";
    return performMeetFIS(outflow, currOUT);
}
