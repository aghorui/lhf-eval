/*******************************************************************************************************
                  "Flow-Sensitive Single Pass "
Author:      Aditi Raste
Created on:  3/2/2023
Description:
Last Updated:
Current Status:
**********************************************************************************************************/
#include "FSSP_Kill.h"

FSSP_Kill::FSSP_Kill(
    std::unique_ptr<llvm::Module> &module, bool debug, bool SLIM, bool intraProc, ExecMode modeOfExec, bool byPass)
    : AnalysisGenKill<F1, B1>(module, debug, SLIM, intraProc, modeOfExec, byPass) {}

B1 FSSP_Kill::getBoundaryInformationBackward() {
    // llvm::outs() << "\n Inside getBoundaryInformationBackward ";
    B1 B_TOP;
    return B_TOP;
}

B1 FSSP_Kill::getInitialisationValueBackward() {
    // llvm::outs() << "\n Inside getInitialisationValueBackward ";
    Value *kVal = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context1), 111111, 1);
    SLIMOperand *valKilled = new SLIMOperand(kVal);
    B1 setKilled;
    setKilled.insert(valKilled);
    return setKilled;
}

bool FSSP_Kill::EqualDataFlowValuesBackward(const B1 &d1, const B1 &d2) const {
    // llvm::outs() << "\n Inside FSSP_Kill Inside
    // EqualDataFlowValuesBackward.................";
    int V1, V2;
    bool flag1, flag2;
    flag1 = flag2 = false;

    if (d1.empty() and d2.empty())
        return true;
    else if ((d1.empty() and !d2.empty()) or (!d1.empty() and d2.empty()))
        return false;
    else if (!d1.empty() and !d2.empty()) {
        if (d1.size() == 1 and d2.size() == 1) {
            for (auto v1 : d1) {
                if (ConstantInt *CI = dyn_cast<ConstantInt>(v1->getValue())) {
                    if (CI->getBitWidth() <= 32)
                        V1 = CI->getSExtValue();
                } else
                    flag1 = true;
                for (auto v2 : d2) {
                    if (ConstantInt *CI = dyn_cast<ConstantInt>(v2->getValue())) {
                        if (CI->getBitWidth() <= 32)
                            V2 = CI->getSExtValue();

                        if (!flag1) {
                            if (V1 == 111111 and V2 == 111111)
                                return true; // equal both set of pointers
                            else
                                return false; // not equal
                        } else
                            return false;
                    } else
                        flag2 = true;

                    if (flag1 and flag2) {
                        if (v1->getValue() == v2->getValue())
                            return true;
                        else
                            return false;
                    }
                } // end inner for
            } // end outer for

        } // end if
        else {
            if (d1 == d2)
                return true;
            else
                return false;
        } // end else
    }
    return false;
}

bool FSSP_Kill::EqualContextValuesBackward(const B1 &d1, const B1 &d2) const {
    llvm::outs() << "\n Inside FSSP_Kill EqualContextValuesBackward...............";
    if (d1 == d2)
        return true;
    return false; // not equal
}

B1 FSSP_Kill::performMeetBackward(const B1 &d1, const B1 &d2) const {
    llvm::outs() << "\nBEGIN performMeetBackward...................";
    int constIntValue;

    /*llvm::outs() << "\n Printing d1: ";
    for (auto val : d1)
      llvm::outs() << val->getName() << ", " ;   //check this

    llvm::outs() << "\n Printing d2: ";
    for (auto val : d2)
      llvm::outs() << val->getName() << ", " ;   //check this

    */
    if (d1.size() == 1) {
        for (auto v1 : d1) {
            if (ConstantInt *CI = dyn_cast<ConstantInt>(v1->getValue())) {
                if (CI->getBitWidth() <= 32)
                    constIntValue = CI->getSExtValue();
            }
            if (constIntValue == 111111) {
                llvm::outs() << "\n Return d2 as d1 is set of all pointers";
                return d2;
            }
        }
    } else if (d2.size() == 1) {
        for (auto v2 : d2) {
            if (ConstantInt *CI = dyn_cast<ConstantInt>(v2->getValue())) {
                if (CI->getBitWidth() <= 32)
                    constIntValue = CI->getSExtValue();
            }
            if (constIntValue == 111111) {
                llvm::outs() << "\n Return d1 as d2 is set of all pointers";
                return d1;
            }
        }
    }

    // Performing intersection
    B1 mergedSet;
    std::set_intersection(d1.begin(), d1.end(), d2.begin(), d2.end(), std::inserter(mergedSet, mergedSet.begin()));

    llvm::outs() << "\n Merging the values ....";
    for (auto val : mergedSet)
        llvm::outs() << val->getName() << ", "; // check this

    llvm::outs() << "\nEND performMeetBackward...................";
    return mergedSet;
}

B1 FSSP_Kill::getPurelyGlobalComponentBackward(const B1 &dfv) const {
    // llvm::outs() << "\n Inside
    // getPurelyGlobalComponentBackward...............";
    B1 global_component;
    if (!dfv.empty()) {
        for (auto v : dfv) {
            // llvm::outs() << v->getName();
            if (v->isVariableGlobal()) {
                global_component.insert(v);
            }
        } // end for
    }
    return global_component;
}

void FSSP_Kill::printDataFlowValuesBackward(const B1 &dfv) const {
    llvm::outs() << "\n Printing Kill values.................";
    llvm::outs() << "{ ";
    for (auto val : dfv)
        llvm::outs() << val->getName() << ", "; // check this
    llvm::outs() << " }";
}

B1 FSSP_Kill::getPurelyLocalComponentBackward(const B1 &dfv) const {
    //	llvm::outs() << "\n Inside
    // getPurelyLocalComponentBackward.............";
    B1 LocalValues;
    for (auto d : dfv) {
        if (!d->isVariableGlobal())
            LocalValues.insert(d);
    }
    return LocalValues;
}

B1 FSSP_Kill::getKillValforDepObjContext(
    std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, std::set<SLIMOperand *>> *, int> depObCon) {
    return mapDepObjKill[depObCon];
}

void FSSP_Kill::setDependentObj(
    std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, B1> *, int> depObj) {
    varDependentObj = depObj;
}

std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, B1> *, int> FSSP_Kill::getDependentObj() {
    return varDependentObj;
}

/*int FSSP_Kill::getDepAnalysisCalleeContext(int callerContext, BaseInstruction*
inst){ return varDependentObj.first->getCalleeContext(callerContext, inst);
}*/

B1 FSSP_Kill::performMergeAtCallBackward(const B1 &d1, const B1 &d2) const {
    B1 retValue = backwardMerge(d1, d2);
    return retValue;
}

B1 FSSP_Kill::backwardMerge(B1 kill, B1 killOUT) const {
    llvm::outs() << "\n Merging Kill with KillOUT to compute KillIN....";
    int constIntValue;

    /* llvm::outs() << "\n #Testing printing Kill and Killout values:";
     llvm::outs() << "{ ";
       for (auto val : kill)
           llvm::outs() << val->getName() << ", " ;   //check this
       llvm::outs() << " }";
     llvm::outs() << "{ ";
       for (auto val : killOUT)
           llvm::outs() << val->getName() << ", " ;   //check this
       llvm::outs() << " }";*/

    if (kill.size() == 1) {
        for (auto v1 : kill) {
            if (ConstantInt *CI = dyn_cast<ConstantInt>(v1->getValue())) {
                if (CI->getBitWidth() <= 32)
                    constIntValue = CI->getSExtValue();
            }
            if (constIntValue == 111111)
                return kill;
            // llvm::outs() << "\n REACHED HERE";
        }
    }

    // llvm::outs() << "\n killOUT.size() = "<<killOUT.size();

    if (killOUT.size() == 1) {
        for (auto v2 : killOUT) {
            if (ConstantInt *CI = dyn_cast<ConstantInt>(v2->getValue())) {
                if (CI->getBitWidth() <= 32)
                    constIntValue = CI->getSExtValue();
            }
            if (constIntValue == 111111)
                return killOUT;
            // llvm::outs() << "\n REACHED HERE 123";
        }
    }

    // Merge kill with killOUT
    for (auto k : kill)
        killOUT.insert(k);

    llvm::outs() << "\n Completed Merging Kill with KillOUT to compute KillIN....";
    return killOUT;
}

B1 FSSP_Kill::computeInFromOut(BaseInstruction *I, int depAnalysisContext) {
    llvm::outs() << "\n Inside computeINfromOUT for FSSP_Kill............";

    B1 KillINofIns, KillOUTofIns, KillSet;

    pair<std::map<SLIMOperand *, std::set<SLIMOperand *>>, std::set<SLIMOperand *>> pairInFB;

    std::map<SLIMOperand *, std::set<SLIMOperand *>> fwdINofIns;

    KillOUTofIns = getBackwardComponentAtOutOfThisInstruction(I);

    if (KillOUTofIns.empty())
        llvm::outs() << "\n KILLOUT is empty...";

    std::pair<Analysis<std::map<SLIMOperand *, std::set<SLIMOperand *>>, B1> *, int> dObject = getDependentObj();

    std::pair<SLIMOperand *, int> LHS = I->getLHS();
    std::vector<std::pair<SLIMOperand *, int>> RHS = I->getRHS();
    int indirLhs = LHS.second;

    llvm::outs() << "\n Fetching PIN for context : " << /*dObject.second*/ depAnalysisContext << " to compute Kill ";
    // long long ID = I->getInstructionId();
    // BaseInstruction* ins = dObject.first->getBaseInstruction(ID);
    fwdINofIns = dObject.first->getForwardComponentAtInOfThisInstruction(
        I, depAnalysisContext); // depAnalysisContext instead of dObject.second

    if (I->getInstructionType() != RETURN and I->getInstructionType() != COMPARE and
        I->getInstructionType() != BRANCH and I->getInstructionType() != ALLOCA) {

        if (LHS.second == 1) {
            llvm::outs() << "\n Lhs indir = 1"; //<< " Lhs name =
                                                //"<<LHS.first->getName();
            if (!LHS.first->isArrayElement())   // insert into Kill only if not array
                KillSet.insert(LHS.first);
        } else if (LHS.second == 2) {
            llvm::outs() << "\n LHS indirection is 2";
            // fetch pointees of LHS

            if (fwdINofIns.empty()) {
                llvm::outs() << "\n fwdINofIns is EMPTY...........";
                // set kill to all pointers (P) if Pin is empty
                Value *kVal = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context1), 111111, 1);
                SLIMOperand *valKilled = new SLIMOperand(kVal);
                KillSet.insert(valKilled);
                mapDepObjKill[dObject] = KillSet;
                return KillSet;
            } else {
                llvm::outs() << "\n fwdINofIns is NOT EMPTY...........";
                std::queue<SLIMOperand *> q;
                q.push(LHS.first);
                while (indirLhs > 1 and !q.empty()) {
                    SLIMOperand *lhsTemp = q.front();
                    q.pop();
                    for (auto fIN : fwdINofIns) {
                        if (LHS.first->isArrayElement()) {
                            if ((fIN.first->getOnlyName() == LHS.first->getOnlyName()) &&
                                (fIN.first->getIndexVector().size() == LHS.first->getIndexVector().size())) {
                                std::set<SLIMOperand *> pointees = fIN.second;

                                for (auto point : pointees)
                                    q.push(point);
                            } // end if
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
                }
                if (q.size() == 1) {
                    SLIMOperand *pointee = q.front();
                    q.pop();
                    int constIntValue;
                    if (ConstantInt *CI = dyn_cast<ConstantInt>(pointee->getValue())) {
                        if (CI->getBitWidth() <= 32) {
                            constIntValue = CI->getSExtValue();
                            if (constIntValue != 000000)
                                KillSet.insert(pointee);
                        }
                    } else {
                        if (!pointee->isArrayElement()) // insert only if
                                                        // pointee not an array
                            KillSet.insert(pointee);    // pointee not a "?"
                    }
                }
            } // end else not empty
        } // end else
    } // end if

    llvm::outs() << "\n CHECKPOINT 1 ";
    llvm::outs() << "\n #Testing Before merge. Printing the Kill values.......";
    llvm::outs() << "{ ";
    for (auto val : KillSet)
        llvm::outs() << val->getName() << ", "; // check this
    llvm::outs() << " }";

    llvm::outs() << "\n #Testing Before merge. Printing the KillOUT values.......";
    llvm::outs() << "{ ";
    for (auto val : KillOUTofIns)
        llvm::outs() << val->getName() << ", "; // check this
    llvm::outs() << " }";

    // merge killOUT with kill
    // KillSet.insert(KillOUTofIns.begin(), KillOUTofIns.end());
    // KillSet = backwardMerge(KillSet, KillOUTofIns);

    // testing
    KillINofIns = backwardMerge(KillSet, KillOUTofIns);

    llvm::outs() << "\n CHECKPOINT 2............";
    llvm::outs() << "\n #Testing after merge. Printing the Kill values.......";
    llvm::outs() << "{ ";
    for (auto val : KillINofIns)
        llvm::outs() << val->getName() << ", "; // check this
    llvm::outs() << " }";

    // set kill for the dependent object + context pair in mapDepObjKill
    llvm::outs() << "\n Updating the kill for the dependent object and context "
                    "pair in the map.";
    mapDepObjKill[dObject] = KillINofIns;
    return KillINofIns;
}

pair<F1, B1>
FSSP_Kill::CallInflowFunction(int current_context_label, Function *Fun, BasicBlock *bb, const F1 &a1, const B1 &d1) {
    llvm::outs() << "\n Inside FSSP_Kill CallInflowFunction...............\n";
    pair<F1, B1> calleeInflowPair;
    return calleeInflowPair;
}

pair<F1, B1> FSSP_Kill::CallOutflowFunction(
    int current_context_label, Function *Fun, BasicBlock *bb, const F1 &a3, const B1 &d3, const F1 &a1, const B1 &d1) {
    llvm::outs() << "\n Inside FSSP_Kill CallOutflowFunction...............";
    pair<F1, B1> retOUTflow;
    llvm::outs() << "\n Printing values d3:";
    for (auto val : d3)
        llvm::outs() << val->getName() << ", "; // check this

    llvm::outs() << "\n Printing values d1:";
    for (auto val : d1)
        llvm::outs() << val->getName() << ", "; // check this

    B1 finalKill = backwardMerge(d3, d1);
    retOUTflow.second = finalKill;
    llvm::outs() << "\n Printing values retOUTflow.second:";
    for (auto val : retOUTflow.second)
        llvm::outs() << val->getName() << ", "; // check this

    return retOUTflow;
}
