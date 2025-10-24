#include "IR.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/SymbolTableListTraits.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <iostream>
#include <memory>

static llvm::LLVMContext TheContext;
void printAllRefersTo(slim::IR &slimIR) {
    for (auto &entry : slimIR.func_bb_to_inst_id) {
        const auto &f_bb_pair = entry.first;
        const auto &inst_id = entry.second;
        for (auto &inst : inst_id) {
            auto *baseInstruction = slimIR.inst_id_to_object[inst];
            llvm::outs() << *baseInstruction->getLLVMInstruction() << "\n";
            const auto &resultOperand = baseInstruction->getResultOperand();
            if (resultOperand.first) {
                const auto &refersToSources =
                    resultOperand.first->getRefersToSourceValues();
                for (auto &source : refersToSources) {
                    llvm::outs() << "     " << source->getName() << ", ";
                }
                const auto &transitiveIndices =
                    resultOperand.first->getTransitiveIndices();
                llvm::outs()
                    << "    tsize: " << transitiveIndices.size() << "\n";
                llvm::outs() << "\n";
            }
        }
    }
}

double run_stat_for_basic_blocks(slim::IR &slimIR) {
    std::vector<int> totals;
    unsigned call_nodes = 0;
    for (auto &entry : slimIR.func_bb_to_inst_id) {
        const auto &f_bb_pair = entry.first;
        const auto &inst_id = entry.second;
        int consecutive = 0;
        for (auto &inst : inst_id) {
            auto *baseInstruction = slimIR.inst_id_to_object[inst];
            if (baseInstruction->getInstructionType() ==
                InstructionType::CALL) {
                call_nodes++;
                totals.push_back(consecutive);
                consecutive = 0;
            } else {
                consecutive++;
            }
        }
        if (consecutive > 0) {
            totals.push_back(consecutive);
        }
    }
    // llvm::outs() << "Total: " << totals.size() << "\n";
    double average = std::accumulate(totals.begin(), totals.end(), 0) /
                     static_cast<double>(totals.size());
    llvm::outs() << "Average: " << average << "\n";
    return average;
}

unsigned numCallNodes() {
}
void printStructure(slim::IR &ir, std::string name) {
    auto function = ir.getLLVMModule()->getFunction(name);
    if (!function) {
        llvm::errs() << "Could not find the function\n";
        return;
    }
    auto value = function->getValueSymbolTable()->lookup(
        "mapping_writeSyntaxElement_UVLC");
    if (value) {
        llvm::outs() << "Found the value: " << *value << "\n";
        auto indices =
            OperandRepository::getSLIMOperand(value)->getTransitiveIndices();
        llvm::outs() << "Indices: " << indices.size() << "\n";
    }
}
int main(int argc, char *argv[]) {
    llvm::SMDiagnostic err;
    std::unique_ptr<llvm::Module> module =
        llvm::parseIRFile(argv[1], err, TheContext);
    if (!module) {
        llvm::errs() << "Error reading IR file\n";
        // err.print("main", llvm::errs());
        return 1;
    }
    slim::IR slimIR(module);
    // slimIR.dumpIR("filename.slim.ll");
    printStructure(slimIR, argv[2]);
    return 0;
    auto function = slimIR.getLLVMModule()->getFunction("std_eval");
    // get the value named %i282
    auto value = function->getValueSymbolTable()->lookup("i282_std_eval");
    if (value) {
        llvm::errs() << "Found the value: " << *value << "\n";
        auto inst = llvm::dyn_cast<llvm::Instruction>(value);
        if (inst) {
            auto slimInstruction = slimIR.LLVMInstructiontoSLIM[inst];
            auto lhs = slimInstruction->getLHS();
            auto rhs = slimInstruction->getRHS();
            auto lhs_main = lhs.first->getValueOfArray();
            auto rhs_main = rhs[0].first->getValueOfArray();
            if (lhs_main) {

                llvm::errs() << "LHS: " << *lhs_main << "\n";
            } else {
                llvm::errs() << "LHS: NULL\n";
            }
            if (rhs_main) {
                llvm::errs() << "RHS: " << *rhs_main << "\n";
            } else {
                llvm::errs() << "RHS: "
                             << "NULL"
                             << "\n";
            }
        }
    } else {
        llvm::errs() << "Could not find the value\n";
    }

    return 0;
    run_stat_for_basic_blocks(slimIR);
    double average = 0, i = 0;
    std::vector<std::string> files = {"mcf",   "lbm",     "bzip2",
                                      "hmmer", "h264ref", "gcc",
                                      "gobmk", "sjeng",   "libquantum"};
    // for (auto &file : files) {
    //     // if(file != "gobmk") continue;
    //     auto filename = "../cos3a/SPEC/" + file + ".ll";
    //     std::unique_ptr<llvm::Module> module =
    //         llvm::parseIRFile(filename, err, TheContext);
    //     if (!module) {
    //         llvm::errs() << "Error reading IR file\n";
    //         // err.print("main", llvm::errs());
    //         return 1;
    //     }
    //     slim::IR slimIR(module);
    //     average = average * i + run_stat_for_basic_blocks(slimIR);
    //     i++;
    //     average = average / i;
    // }
    // llvm::outs() << "Average: " << average << "\n";
    // run_stat_for_basic_blocks(slimIR);
    printAllRefersTo(slimIR);
    return 0;
}