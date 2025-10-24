#include "IR.h"
#include "Passes/GlobalInit.cpp"
#include "Passes/MergeExit.cpp"
#include "SLIMDebug.h"

std::map<std::string, llvm::Value *> name_to_variable_object;
std::map<llvm::Value *, bool> debug_value_map;
namespace slim {
// Check if the SSA variable (created using MemorySSA) already exists or not
std::map<std::string, bool> is_ssa_version_available;
} // namespace slim

// Process the llvm instruction and return the corresponding SLIM instruction
BaseInstruction *
slim::convertLLVMToSLIMInstruction(llvm::Instruction &instruction) {
    BaseInstruction *base_instruction;
    
    if (llvm::isa<llvm::AllocaInst>(instruction)) {
        base_instruction = new AllocaInstruction(&instruction);
    } else if (llvm::isa<llvm::LoadInst>(instruction)) {
        base_instruction = new LoadInstruction(&instruction);
        //llvm::outs() << "\n INSTRUCTION::"<<instruction;
    } else if (llvm::isa<llvm::StoreInst>(instruction)) {
        base_instruction = new StoreInstruction(&instruction);
    } else if (llvm::isa<llvm::FenceInst>(instruction)) {
        base_instruction = new FenceInstruction(&instruction);
    } else if (llvm::isa<llvm::AtomicCmpXchgInst>(instruction)) {
        base_instruction = new AtomicCompareChangeInstruction(&instruction);
    } else if (llvm::isa<llvm::AtomicRMWInst>(instruction)) {
        base_instruction = new AtomicModifyMemInstruction(&instruction);
    } else if (llvm::isa<llvm::GetElementPtrInst>(instruction)) {
        base_instruction = new GetElementPtrInstruction(&instruction);
    } else if (llvm::isa<llvm::UnaryOperator>(instruction)) {
        base_instruction = new FPNegationInstruction(&instruction);
    } else if (llvm::isa<llvm::BinaryOperator>(instruction)) {
        base_instruction = new BinaryOperation(&instruction);
    } else if (llvm::isa<llvm::ExtractElementInst>(instruction)) {
        base_instruction = new ExtractElementInstruction(&instruction);
    } else if (llvm::isa<llvm::InsertElementInst>(instruction)) {
        base_instruction = new InsertElementInstruction(&instruction);
    } else if (llvm::isa<llvm::ShuffleVectorInst>(instruction)) {
        base_instruction = new ShuffleVectorInstruction(&instruction);
    } else if (llvm::isa<llvm::ExtractValueInst>(instruction)) {
        base_instruction = new ExtractValueInstruction(&instruction);
    } else if (llvm::isa<llvm::InsertValueInst>(instruction)) {
        base_instruction = new InsertValueInstruction(&instruction);
    } else if (llvm::isa<llvm::TruncInst>(instruction)) {
        base_instruction = new TruncInstruction(&instruction);
    } else if (llvm::isa<llvm::ZExtInst>(instruction)) {
        base_instruction = new ZextInstruction(&instruction);
    } else if (llvm::isa<llvm::SExtInst>(instruction)) {
        base_instruction = new SextInstruction(&instruction);
    } else if (llvm::isa<llvm::FPTruncInst>(instruction)) {
        base_instruction = new TruncInstruction(&instruction);
    } else if (llvm::isa<llvm::FPExtInst>(instruction)) {
        base_instruction = new FPExtInstruction(&instruction);
    } else if (llvm::isa<llvm::FPToUIInst>(instruction)) {
        base_instruction = new FPToIntInstruction(&instruction);
    } else if (llvm::isa<llvm::FPToSIInst>(instruction)) {
        base_instruction = new FPToIntInstruction(&instruction);
    } else if (llvm::isa<llvm::UIToFPInst>(instruction)) {
        base_instruction = new IntToFPInstruction(&instruction);
    } else if (llvm::isa<llvm::SIToFPInst>(instruction)) {
        base_instruction = new IntToFPInstruction(&instruction);
    } else if (llvm::isa<llvm::PtrToIntInst>(instruction)) {
        base_instruction = new PtrToIntInstruction(&instruction);
    } else if (llvm::isa<llvm::IntToPtrInst>(instruction)) {
        base_instruction = new IntToPtrInstruction(&instruction);
    } else if (llvm::isa<llvm::BitCastInst>(instruction)) {
        base_instruction = new BitcastInstruction(&instruction);
    } else if (llvm::isa<llvm::AddrSpaceCastInst>(instruction)) {
        base_instruction = new AddrSpaceInstruction(&instruction);
    } else if (llvm::isa<llvm::ICmpInst>(instruction)) {
        base_instruction = new CompareInstruction(&instruction);
    } else if (llvm::isa<llvm::FCmpInst>(instruction)) {
        base_instruction = new CompareInstruction(&instruction);
    } else if (llvm::isa<llvm::PHINode>(instruction)) {
        base_instruction = new PhiInstruction(&instruction);
    } else if (llvm::isa<llvm::SelectInst>(instruction)) {
        base_instruction = new SelectInstruction(&instruction);
    } else if (llvm::isa<llvm::FreezeInst>(instruction)) {
        base_instruction = new FreezeInstruction(&instruction);
    } else if (llvm::isa<llvm::CallInst>(instruction)) {
        base_instruction = new CallInstruction(&instruction);
    } else if (llvm::isa<llvm::VAArgInst>(instruction)) {
        base_instruction = new VarArgInstruction(&instruction);
    } else if (llvm::isa<llvm::LandingPadInst>(instruction)) {
        base_instruction = new LandingpadInstruction(&instruction);
    } else if (llvm::isa<llvm::CatchPadInst>(instruction)) {
        base_instruction = new CatchpadInstruction(&instruction);
    } else if (llvm::isa<llvm::CleanupPadInst>(instruction)) {
        base_instruction = new CleanuppadInstruction(&instruction);
    } else if (llvm::isa<llvm::ReturnInst>(instruction)) {
        base_instruction = new ReturnInstruction(&instruction);
    } else if (llvm::isa<llvm::BranchInst>(instruction)) {
        base_instruction = new BranchInstruction(&instruction);
    } else if (llvm::isa<llvm::SwitchInst>(instruction)) {
        base_instruction = new SwitchInstruction(&instruction);
    } else if (llvm::isa<llvm::IndirectBrInst>(instruction)) {
        base_instruction = new IndirectBranchInstruction(&instruction);
    } else if (llvm::isa<llvm::InvokeInst>(instruction)) {
        base_instruction = new InvokeInstruction(&instruction);
    } else if (llvm::isa<llvm::CallBrInst>(instruction)) {
        base_instruction = new CallbrInstruction(&instruction);
    } else if (llvm::isa<llvm::ResumeInst>(instruction)) {
        base_instruction = new ResumeInstruction(&instruction);
    } else if (llvm::isa<llvm::CatchSwitchInst>(instruction)) {
        base_instruction = new CatchswitchInstruction(&instruction);
    } else if (llvm::isa<llvm::CatchReturnInst>(instruction)) {
        base_instruction = new CatchreturnInstruction(&instruction);
    } else if (llvm::isa<llvm::CleanupReturnInst>(instruction)) {
        base_instruction = new CleanupReturnInstruction(&instruction);
    } else if (llvm::isa<llvm::UnreachableInst>(instruction)) {
        base_instruction = new UnreachableInstruction(&instruction);
    } else {
        base_instruction = new OtherInstruction(&instruction);
    }

    return base_instruction;
}

// Creates different SSA versions for global and address-taken local variables
// using Memory SSA
void slim::createSSAVersions(std::unique_ptr<llvm::Module> &module) {
    // Fetch the function list of module
    llvm::SymbolTableList<llvm::Function> &function_list =
        module->getFunctionList();

    // Contains the operand object corresponding to every global SSA variable
    std::map<std::string, llvm::Value *> ssa_variable_to_operand;

    // For each function in the module
    for (auto &function : function_list) {
        // Skip the function if it is intrinsic or is not defined in the
        // translation unit
        if (function.isIntrinsic() || function.isDeclaration()) {
            continue;
        }

        llvm::PassBuilder PB;
        llvm::FunctionAnalysisManager *function_analysis_manager =
            new llvm::FunctionAnalysisManager();

        // Register the FunctionAnalysisManager in the pass builder
        PB.registerFunctionAnalyses(*function_analysis_manager);

        llvm::AAManager aa_manager;

        // Add the Basic Alias Analysis provided by LLVM
        aa_manager.registerFunctionAnalysis<llvm::BasicAA>();

        auto alias_analysis =
            aa_manager.run(function, *function_analysis_manager);

        llvm::DominatorTree *dominator_tree =
            &(*function_analysis_manager)
                 .getResult<llvm::DominatorTreeAnalysis>(function);

        llvm::MemorySSA *memory_ssa =
            new llvm::MemorySSA(function, &alias_analysis, dominator_tree);

        // Get the MemorySSAWalker which will be used to query about the
        // clobbering memory definition
        llvm::MemorySSAWalker *memory_ssa_walker = memory_ssa->getWalker();

        std::map<llvm::Value *, bool> is_operand_stack_variable;

        // For each basic block in the function
        for (llvm::BasicBlock &basic_block : function.getBasicBlockList()) {
            std::vector<llvm::Instruction *> instructions;

            for (llvm::Instruction &instruction : basic_block.getInstList()) {
                instructions.push_back(&instruction);
            }

            // For each instruction in the basic block
            for (llvm::Instruction *instruction_ptr : instructions) {
                llvm::Instruction &instruction = *instruction_ptr;
                /*
                                        Check if the operand is a address-taken
                   stack variable This assumes that the IR has been transformed
                   by mem2reg. Since the only variables that are left in
                   "alloca" form (stack variables) after mem2reg are the
                   variables whose addresses have been taken in some form. The
                   rest of the local variables are promoted to SSA registers by
                   mem2reg.
                                */
                if (llvm::isa<llvm::AllocaInst>(instruction)) {
                    is_operand_stack_variable[(llvm::Value *)&instruction] =
                        true;
                }
                // Check if the instruction is a load instruction
                if (llvm::isa<llvm::LoadInst>(instruction)) {
                    // Get the clobbering memory access for this load
                    // instruction
                    llvm::MemoryAccess *clobbering_mem_access =
                        memory_ssa_walker->getClobberingMemoryAccess(
                            &instruction);

                    std::string ssa_variable_name = "";

                    // Check if this the memory access is a MemoryDef or not
                    if (llvm::isa<llvm::MemoryDef>(clobbering_mem_access) ||
                        llvm::isa<llvm::MemoryPhi>(clobbering_mem_access)) {
                        // Cast the MemoryAccess object to MemoryDef object
                        llvm::MemoryDef *memory_def =
                            llvm::dyn_cast<llvm::MemoryDef,
                                           llvm::MemoryAccess *>(
                                clobbering_mem_access);
                        llvm::MemoryPhi *memory_phi =
                            llvm::dyn_cast<llvm::MemoryPhi,
                                           llvm::MemoryAccess *>(
                                clobbering_mem_access);

                        unsigned int memory_def_id;

                        // Get the memory definition id
                        if (llvm::isa<llvm::MemoryDef>(clobbering_mem_access)) {
                            memory_def_id = memory_def->getID();
                        } else {
                            memory_def_id = memory_phi->getID();
                        }

                        // Fetch the source operand of the load instruction
                        llvm::Value *source_operand = instruction.getOperand(0);

                        // Check if the source operand is a global variable
                        if (llvm::isa<llvm::GlobalVariable>(source_operand) ||
                            is_operand_stack_variable[source_operand]) {
                            // Based on the memory definition id and global or
                            // address-taken local variable name, this is the
                            // expected SSA variable name
                            ssa_variable_name =
                                function.getName().str() + "_" +
                                source_operand->getName().str() + "_" +
                                std::to_string(memory_def_id);

                            // Check if the SSA variable (created using
                            // MemorySSA) already exists or not
                            if (slim::is_ssa_version_available.find(
                                    ssa_variable_name) !=
                                slim::is_ssa_version_available.end()) {
                                // If the expected SSA variable already exists,
                                // then replace the source operand with the
                                // corresponding SSA operand
                                instruction.setOperand(
                                    0,
                                    ssa_variable_to_operand[ssa_variable_name]);
                            } else {
                                // Fetch the basic block iterator
                                // llvm::BasicBlock::iterator
                                // basicblock_iterator =
                                //     basic_block.begin();

                                // Create a new load instruction which loads the
                                // value from the memory location to a temporary
                                // variable
                                llvm::LoadInst *new_load_instr =
                                    new llvm::LoadInst(
                                        source_operand->getType(),
                                        source_operand,
                                        "tmp." + ssa_variable_name,
                                        &instruction);

                                // Create a new alloca instruction for the new
                                // SSA version
                                llvm::AllocaInst *new_alloca_instr =
                                    new llvm::AllocaInst(
                                        ((llvm::Value *)new_load_instr)
                                            ->getType(),
                                        0, ssa_variable_name, new_load_instr);

                                // Create a new store instruction to store the
                                // value from the new temporary to the new SSA
                                // version of global or address-taken local
                                // variable
                                // llvm::StoreInst *new_store_instr =
                                //     new llvm::StoreInst(
                                //         (llvm::Value *)new_load_instr,
                                //         (llvm::Value *)new_alloca_instr,
                                //         &instruction);

                                // Update the map accordingly
                                slim::is_ssa_version_available
                                    [ssa_variable_name] = true;

                                // The value of a instruction corresponds to the
                                // result of that instruction
                                ssa_variable_to_operand[ssa_variable_name] =
                                    (llvm::Value *)new_alloca_instr;

                                // Replace the operand of the load instruction
                                // with the new SSA version
                                instruction.setOperand(
                                    0,
                                    ssa_variable_to_operand[ssa_variable_name]);
                            }
                        }
                    } else {
                        // This is not expected
                        llvm_unreachable("Clobbering access is not MemoryDef, "
                                         "which is unexpected!");
                    }
                }
            }
        }
    }
}

// Default constructor
slim::IR::IR() {
}


void slim::extractRefersToIndirection(BaseInstruction *base_instruction) {
    // For Store/GEP/operations, Set the indirection count to 0 [Do not use the indirction level of the rhs_operand itself]
    // For load, check condition for pointer dereferecing if yes, increment the count of the rhs_operand by 1
    // For binary_op instructions add the refs to operands with indirection level set to 0 (might not be required but just in case)
    
    auto type = base_instruction->getInstructionType();
    if (type == InstructionType::CALL){
        const auto &result = base_instruction->getResultOperand();
        result.first->addRefersToSourceValuesIndirection(new std::pair<SLIMOperand*, int>(result.first, 0));
        return;
    }
    
    
    if ((base_instruction->isExpressionAssignment() ||
         type == InstructionType::GET_ELEMENT_PTR ||
         type == InstructionType::LOAD) &&
        (type != InstructionType::STORE)) {
        const auto &result = base_instruction->getResultOperand();
        const auto numOperands = base_instruction->getNumOperands();
        if (!result.first->isSource()) {
            if (type == InstructionType::GET_ELEMENT_PTR) {
	      GetElementPtrInstruction *get_element_ptr_ins =
                                            static_cast<GetElementPtrInstruction*>(base_instruction);
              auto *operand = get_element_ptr_ins->getMainOperand();   
                //    if (operand->getType()->isArrayTy()) // FIXME: Move this to maybe GEPInstruction constructor?
                //         operand->setArrayType();

            result.first->addRefersToSourceValuesIndirection(new std::pair<SLIMOperand*, int>(result.first, 0));
              ///  llvm::outs() << "(t)" << result.first->getName() << "\n";        
            } else if (type == InstructionType::PHI){
             result.first->addRefersToSourceValuesIndirection(new std::pair<SLIMOperand*, int>(result.first, 0));
//                llvm::outs() << "(t)" << result.first->getName() << " " << 0 << "\n";
            } else if (type == InstructionType::LOAD){
                   
                auto *operand = base_instruction->getOperand(0).first;

                // Ignore for Return value mapping
                // TODO: Find a better way of dealing with return value mapping instructions
                if (operand->getName().startswith("retval"))
                    return;

                int indirection = 1;

                if (operand->isGlobalOrAddressTaken() ||
                    operand->isGEPInInstr() ||
                    llvm::isa<llvm::GlobalValue>(operand->getValue()))
                    indirection = 0;

                if (base_instruction->getOperand(0)
                        .first->getType()
                        ->isArrayTy()) {
                    operand->setArrayType();
// llvm::outs() << "\n SETARR 4";
                }

                // if the operand is itself a source,
                // add it directly to the refers_to_source_values_map
                if (operand->isGEPInInstr()) {
                    auto* main_operand = OperandRepository::getOrCreateSLIMOperand(operand->get_gep_main_operand());
                    result.first->addRefersToSourceValuesIndirection(new std::pair<SLIMOperand*, int>(result.first, indirection));
  ///                  llvm::outs() << "(t)" << result.first->getName() << " " << 0 << "\n";
                } else if (operand->isSource()) {
                    if (base_instruction->getInstructionType() ==
                        InstructionType::PHI) {
                  if (base_instruction->getOperand(0).second != 0) // dont add source if base ins
                                // is t1=phi(@u, @v)
                            result.first->addRefersToSourceValuesIndirection(new std::pair<SLIMOperand*, int>(result.first, indirection));
                           // llvm::outs() << "(t)" << result.first << " " << 0 << "\n";
                        } else {
                            result.first->addRefersToSourceValuesIndirection(new std::pair<SLIMOperand*, int>(operand, indirection));
                            //llvm::outs() << "(t)" << operand->getName() << " " << 0 << "\n";
                        }
                        
                    /*result.first->addRefersToSourceValues(operand);*/
                    // llvm::outs() << "(s)" << operand->getName() << "\n";
                } else {
                    // otherwise add the refersTo set of the operand
                    // since it is not a source (thus is transitive )
                    auto &refersTo = operand->getRefersToSourceValuesIndirection();
                    bool containsOperand = false;
                    for (auto* op : refersTo){
                        if (op->first == operand){
                            containsOperand = true;
                        } 
                    }

                    if (containsOperand){
                        result.first->getRefersToSourceValuesIndirection().insert(new std::pair<SLIMOperand*, int>(result.first, 0));
                        //llvm::outs() << "(t)" << result.first->getName() << " " << 0 << "\n";
                    } else {
                        for (auto* ref : refersTo){
                            result.first->getRefersToSourceValuesIndirection().insert(new std::pair<SLIMOperand*, int>(ref->first, ref->second + indirection));
                          //      llvm::outs() << "(t)" << (ref->first)->getName() << " " << ref->second + indirection << "\n";
                        }
                    }
                    
                    
                }
            } else {
              for (unsigned i = 0; i < numOperands; i++) {   //   t1 = gep t2, idx   ==> t2
                auto *operand = base_instruction->getOperand(i).first;

                if (base_instruction->getOperand(i)
                        .first->getType()
                        ->isArrayTy()) {
// llvm::outs() << "\n SETARR 5";
                    operand->setArrayType();
                }

                // if the operand is itself a source,
                // add it directly to the refers_to_source_values_map
                if (operand->isSource()) {
                    if (base_instruction->getInstructionType() ==
                        InstructionType::PHI) {
                        if (base_instruction->getOperand(i).second !=
                            0){ // dont add source if base ins
                               // is t1=phi(@u, @v)
                            result.first->addRefersToSourceValuesIndirection(new std::pair<SLIMOperand*, int>(result.first, 0));
                           // llvm::outs() << "(t)" << result.first << " " << 0 << "\n";
                        }
                    } else {
                        result.first->addRefersToSourceValuesIndirection(new std::pair<SLIMOperand*, int>(operand, 0));
                        //llvm::outs() << "(t)" << operand->getName() << " " << 0 << "\n";
                    }
                    /*result.first->addRefersToSourceValues(operand);*/
                    // llvm::outs() << "(s)" << operand->getName() << "\n";
                    
                } else if (operand->isGEPInInstr()) {
                    auto* main_operand = OperandRepository::getOrCreateSLIMOperand(operand->get_gep_main_operand());
                    result.first->addRefersToSourceValuesIndirection(new std::pair<SLIMOperand*, int>(result.first, 0));
                    // llvm::outs() << "(t)" << main_operand->getName() << "\n";
                    //llvm::outs() << "(t)" << result.first->getName() << " " << 0 << "\n";
                } else {
                    // otherwise add the refersTo set of the operand
                    // since it is not a source (thus is transitive )
                    auto &refersTo = operand->getRefersToSourceValuesIndirection();
                    for (auto* ref : refersTo){
                        result.first->getRefersToSourceValuesIndirection().insert(new std::pair<SLIMOperand*, int>(result.first, 0));
                           // llvm::outs() << "(t)" << result.first->getName() << " " << 0 << "\n";
                    }
                    // llvm::outs() << "(t)" << operand->getName() << "\n";
                }
                // llvm::outs() << "==\n";
            }//for
          }//end else   
            /// base_instruction->setIgnore();
        }
    }
}

static void extractRefersTo(BaseInstruction *base_instruction) {
    auto type = base_instruction->getInstructionType();
    
    if (type == InstructionType::CALL){
        const auto &result = base_instruction->getResultOperand();
        result.first->addRefersToSourceValues(result.first);
        return;
    }
    
    
    if ((base_instruction->isExpressionAssignment() ||
         type == InstructionType::GET_ELEMENT_PTR ||
         type == InstructionType::LOAD) &&
        (type != InstructionType::STORE)) {
        const auto &result = base_instruction->getResultOperand();
        const auto numOperands = base_instruction->getNumOperands();
        if (!result.first->isSource()) {
            if (type == InstructionType::GET_ELEMENT_PTR) {
		GetElementPtrInstruction *get_element_ptr_ins = static_cast<GetElementPtrInstruction*>(base_instruction);
               auto *operand = get_element_ptr_ins->getMainOperand();
            //    if (operand->getType()->isArrayTy()) // FIXME: Move this to maybe GEPInstruction constructor?
            //         operand->setArrayType();
                result.first->addRefersToSourceValues(result.first);        
                // llvm::outs() << "(t)" << result.first->getName() << "\n";
            } else if (type == InstructionType::PHI){
                result.first->addRefersToSourceValues(result.first);
                // llvm::outs() << "(t)" << result.first->getName() << "\n";
            }//fi GEP
            else {
              for (unsigned i = 0; i < numOperands; i++) {   //   t1 = gep t2, idx   ==> t2
                auto *operand = base_instruction->getOperand(i).first;

                if (i == 0 && operand->getName().startswith("retval")){
                    return;
                }

                if (base_instruction->getOperand(i)
                        .first->getType()
                        ->isArrayTy()) {
                    operand->setArrayType();
// llvm::outs() << "\n SETARR 6";
               }

                // if the operand is itself a source,
                // add it directly to the refers_to_source_values_map
                if (operand->isSource()) {
                    result.first->addRefersToSourceValues(operand);
                    // llvm::outs() << "(t)" << operand->getName() << "\n";
                } else if (operand->isGEPInInstr()) {
                    // auto* main_operand = OperandRepository::getOrCreateSLIMOperand(operand->get_gep_main_operand());
                    result.first->addRefersToSourceValues(result.first);
                    // llvm::outs() << "(t)" << result.first->getName() << "\n";
                } else {
                    // otherwise add the refersTo set of the operand
                    // since it is not a source (thus is transitive )
                    auto &refersTo = operand->getRefersToSourceValues();
                    // llvm::outs() << "(t)" << refersTo.size() << "\n";
                    if (refersTo.find(operand) == refersTo.end()){
                        result.first->getRefersToSourceValues().insert(
                            refersTo.begin(), refersTo.end());
                    } else {
                        result.first->getRefersToSourceValues().insert(result.first);
                        // llvm::outs() << "(t)" << result.first->getName() << "\n";
                    }

                }
                // llvm::outs() << "==\n";
            }//for
          }//end else   
            /// base_instruction->setIgnore();
        }
    }
}

void slim::IR::calculateRefersToSourceValues() {
    for (auto &entry : this->func_bb_to_inst_id) {
        auto *function = entry.first.first;
        auto *bb = entry.first.second;
        auto &inst_ids = entry.second;
        for (auto id : inst_ids) {
            auto *base_instruction = inst_id_to_object[id];
            extractRefersTo(base_instruction);
            extractRefersToIndirection(base_instruction);
            if (base_instruction->getInstructionType() ==
                InstructionType::GET_ELEMENT_PTR) {
                auto resOperandIndices =
                    base_instruction->getResultOperand().first->getNumIndices();
                // llvm::outs()
                //     << "GET_ELEMENT_PTR (res): " << resOperandIndices << " is
                //     "
                //     << base_instruction->getResultOperand()
                //            .first->getTransitiveIndices()
                //            .size()
                //     << "\n";

                // auto indices = base_instruction->getResultOperand()
                //                    .first->getTransitiveIndices();
                // for (auto index : indices) {
                //     auto *value = index->getValue();
                //     if (auto constant =
                //             llvm::dyn_cast<llvm::ConstantInt>(value)) {
                //         llvm::outs()
                //             << "   " << constant->getSExtValue() << "\n";
                //     }
                // }
            }
        }
    }
    // llvm::outs() << "Done refers to source:\n";
    // for (auto &entry : refers_to_source_values_map) {
    //     llvm::outs() << entry.first->getName() << " : \n";
    //     for (auto *val : entry.second) {
    //         llvm::outs() << "   " << val->getName() << "\n";
    //         assert(debug_source_var_map.find(val) !=
    //                    debug_source_var_map.end() &&
    //                "Refers to source value is not a source");
    //     }
    // }
    // llvm::outs() << "==over refersTo==\n";
}
void slim::IR::readAllDebugSources() {
    // For each basic block in the function
    for (llvm::Function &function : *llvm_module) {
        if (function.isIntrinsic() || function.isDeclaration()) {
            continue;
        }

        for (llvm::BasicBlock &basic_block : function) {
            this->basic_block_to_id[&basic_block] = this->total_basic_blocks++;
            // For each instruction in the basic block
            for (llvm::Instruction &instruction : basic_block.getInstList()) {
                if (instruction.isDebugOrPseudoInst()) {
                    readDebugSourceVarInfo(instruction);
                }
            }
        }
    }
}
void slim::IR::readGlobalsForFunctionPointersInArrays() {
    for (auto &global : llvm_module->getGlobalList()) {
        // check if the global is of array type
        if (!llvm::isa<llvm::StructType>(
                global.getType()->getContainedType(0)) &&
            !llvm::isa<llvm::ArrayType>(
                global.getType()->getContainedType(0))) {
            continue;
        }
        if (global.hasInitializer()) {
            auto *init = global.getInitializer();
            addFunctionPointersInConstantToValue(init, &global);
        }
    }
}
// Construct the SLIM IR from module
slim::IR::IR(std::unique_ptr<llvm::Module> &module)
    : llvm_module(std::move(module)) {
    unsigned total_pointer_assignments = 0;
    unsigned total_non_pointer_assignments = 0;
    unsigned total_other_instructions = 0;
    unsigned total_local_pointers = 0;
    unsigned total_local_scalers = 0;
    // Run the pass plugin GlobalInit from the Passes/GlobalInit.cpp file on
    // this module
    llvm::PassBuilder PB;
    llvm::ModuleAnalysisManager MAM;
    GlobalInit().run(*llvm_module, MAM);

// Create different SSA versions for globals and address-taken local variables
// if the MemorySSA flag is passed
#ifdef MemorySSAFlag
    slim::createSSAVersions(this->llvm_module);
#endif

    // Keeps track of the temporaries who are already renamed
    std::set<llvm::Value *> renamed_temporaries;
    // For each function in the module

    readAllDebugSources();
    readGlobalsForFunctionPointersInArrays();

    for (llvm::Function &function : *llvm_module) {
       llvm::FunctionAnalysisManager FAM;

        // Append the pointer to the function to the "functions" list
        if (!function.isIntrinsic() && !function.isDeclaration()) {
            this->functions.push_back(&function);
        } else {
            continue;
        }

#ifdef DiscardPointers
        std::unordered_set<llvm::Value *> discarded_result_operands;
#endif

#ifdef DiscardForSSA
        std::unordered_set<llvm::Value *> discarded_operands_for_ssa;
#endif

        // For each basic block in the function
        for (llvm::BasicBlock &basic_block : function) {
            this->basic_block_to_id[&basic_block] = this->total_basic_blocks++;

            // For each instruction in the basic block
            for (llvm::Instruction &instruction : basic_block.getInstList()) {
                if (instruction.isDebugOrPseudoInst()) {
                    readDebugSourceVarInfo(instruction);
                    continue;
                }

                BaseInstruction *base_instruction =
                    slim::convertLLVMToSLIMInstruction(instruction);
                renameTemporaryOperands(base_instruction, renamed_temporaries,
                                        function);

#ifdef DiscardPointers
                bool is_discarded = false;

                for (unsigned i = 0; i < base_instruction->getNumOperands();
                     i++) {
                    SLIMOperand *operand_i =
                        base_instruction->getOperand(i).first;

                    if (!operand_i || !operand_i->getValue())
                        continue;
                    if (operand_i->isPointerInLLVM() ||
                        (operand_i->getValue() &&
                         discarded_result_operands.find(
                             operand_i->getValue()) !=
                             discarded_result_operands.end()))
                    // if ((operand_i->getValue() &&
                    // discarded_result_operands.find(operand_i->getValue()) !=
                    // discarded_result_operands.end()))
                    {
                        is_discarded = true;
                        break;
                    }
                }

                if (base_instruction->getInstructionType() ==
                    InstructionType::GET_ELEMENT_PTR) {
                    is_discarded = true;
                }

                if (base_instruction->getInstructionType() ==
                    InstructionType::CALL) {
                    // Don't skip
                } else if (is_discarded &&
                           base_instruction->getResultOperand().first &&
                           base_instruction->getResultOperand()
                               .first->getValue()) {
                    discarded_result_operands.insert(
                        base_instruction->getResultOperand().first->getValue());
                    continue;
                } else if (is_discarded) {
                    // Ignore the instruction (because it is using the discarded
                    // value)
                    continue;
                }
#endif

#ifdef DiscardForSSA
                bool is_discarded = false;

                if (base_instruction->getInstructionType() ==
                    InstructionType::GET_ELEMENT_PTR) {
                    discarded_operands_for_ssa.insert(
                        base_instruction->getResultOperand().first->getValue());
                    this->total_instructions++;
                    continue;
                } else if (base_instruction->getInstructionType() ==
                           InstructionType::ALLOCA) {
                    llvm::Value *result_operand =
                        base_instruction->getResultOperand().first->getValue();

                    if (llvm::isa<llvm::ArrayType>(result_operand->getType()) ||
                        llvm::isa<llvm::StructType>(
                            result_operand->getType())) {
                        discarded_operands_for_ssa.insert(result_operand);
                        this->total_instructions++;
                        continue;
                    }

                    // Don't skip this instruction
                } else if (base_instruction->getInstructionType() ==
                           InstructionType::RETURN) {
                    // Don't skip this instruction
                } else if (base_instruction->getInstructionType() ==
                           InstructionType::LOAD) {
                    //llvm::outs() << "\n LOAD INS == "; base_instruction->printLLVMInstruction();
                    LoadInstruction *load_inst =
                        (LoadInstruction *)base_instruction;

                    SLIMOperand *rhs_operand = load_inst->getOperand(0).first;
                    SLIMOperand *result_operand =
                        load_inst->getResultOperand().first;

                    if (discarded_operands_for_ssa.find(
                            rhs_operand->getValue()) !=
                        discarded_operands_for_ssa.end()) { llvm::outs() << "\n LOOP 1";
                        discarded_operands_for_ssa.insert(
                            load_inst->getResultOperand().first->getValue());
                        this->total_instructions++;
                        continue;
                    }
                    if (llvm::isa<llvm::PointerType>(
                            rhs_operand->getValue()
                                ->getType()
                                ->getContainedType(0))) { llvm::outs() << "\n LOOP 2";
                        // Discard the instruction
                        this->total_instructions++;
                        continue;
                    }
                    if (llvm::isa<llvm::ArrayType>(result_operand->getType()) ||
                        llvm::isa<llvm::StructType>(
                            result_operand->getType())) {
                        discarded_operands_for_ssa.insert(
                            result_operand->getValue());
                        this->total_instructions++;
                        continue;
                    }
                    if (llvm::isa<llvm::GEPOperator>(rhs_operand->getValue()) ||
                        llvm::isa<llvm::BitCastOperator>(
                            rhs_operand->getValue())) {
                        discarded_operands_for_ssa.insert(
                            result_operand->getValue());
                        this->total_instructions++;
                        continue;
                    }
                } else if (base_instruction->getInstructionType() ==
                           InstructionType::STORE) {
                    // Discard the instruction if the result operand is of
                    // pointer type
                    StoreInstruction *store_inst =
                        (StoreInstruction *)base_instruction;

                    SLIMOperand *result_operand =
                        store_inst->getResultOperand().first;

                    if (discarded_operands_for_ssa.find(
                            result_operand->getValue()) !=
                        discarded_operands_for_ssa.end()) {
                        discarded_operands_for_ssa.insert(
                            result_operand->getValue());
                        this->total_instructions++;
                        continue;
                    } else if (discarded_operands_for_ssa.find(
                                   store_inst->getOperand(0)
                                       .first->getValue()) !=
                               discarded_operands_for_ssa.end()) {
                        discarded_operands_for_ssa.insert(
                            result_operand->getValue());
                        this->total_instructions++;
                        continue;
                    }

                    if (llvm::isa<llvm::PointerType>(
                            result_operand->getValue()
                                ->getType()
                                ->getContainedType(0))) {
                        // Discard the instruction
                        this->total_instructions++;
                        continue;
                    }

                    if (llvm::isa<llvm::ArrayType>(result_operand->getType()) ||
                        llvm::isa<llvm::StructType>(
                            result_operand->getType())) {
                        this->total_instructions++;
                        continue;
                    }

                    if (llvm::isa<llvm::GEPOperator>(
                            result_operand->getValue()) ||
                        llvm::isa<llvm::BitCastOperator>(
                            result_operand->getValue())) {
                        this->total_instructions++;
                        continue;
                    }
                } else if (base_instruction->getInstructionType() ==
                           InstructionType::CALL) {
                    // Don't skip this instruction
                } else if (base_instruction->getInstructionType() ==
                           InstructionType::BITCAST) {
                    llvm::Type *result_type =
                        base_instruction->getResultOperand()
                            .first->getValue()
                            ->getType();
                    llvm::Type *result_contained_type =
                        result_type->getContainedType(0);
                    llvm::Type *rhs_contained_type =
                        base_instruction->getOperand(0)
                            .first->getValue()
                            ->getType()
                            ->getContainedType(0);
                    bool is_dependent_on_aggregates =
                        llvm::isa<llvm::ArrayType>(result_contained_type) ||
                        llvm::isa<llvm::StructType>(result_contained_type);
                    is_dependent_on_aggregates =
                        is_dependent_on_aggregates ||
                        (llvm::isa<llvm::ArrayType>(rhs_contained_type) ||
                         llvm::isa<llvm::StructType>(rhs_contained_type));

                    if (is_dependent_on_aggregates &&
                        discarded_operands_for_ssa.find(
                            base_instruction->getResultOperand()
                                .first->getValue()) !=
                            discarded_operands_for_ssa.end()) {
                        discarded_operands_for_ssa.insert(
                            base_instruction->getResultOperand()
                                .first->getValue());
                    }

                    this->total_instructions++;
                    continue;
                } else {
                    if (base_instruction->getResultOperand().first &&
                        base_instruction->getResultOperand()
                            .first->getValue() &&
                        llvm::isa<llvm::PointerType>(
                            base_instruction->getResultOperand()
                                .first->getValue()
                                ->getType())) {
                        this->total_instructions++;
                        continue;
                    }
                    if (base_instruction->getResultOperand().first &&
                        base_instruction->getResultOperand()
                            .first->getValue() &&
                        llvm::isa<llvm::StructType>(
                            base_instruction->getResultOperand()
                                .first->getValue()
                                ->getType())) {
                        this->total_instructions++;
                        continue;
                    }
                    if (base_instruction->getResultOperand().first &&
                        base_instruction->getResultOperand()
                            .first->getValue() &&
                        llvm::isa<llvm::ArrayType>(
                            base_instruction->getResultOperand()
                                .first->getValue()
                                ->getType())) {
                        this->total_instructions++;
                        continue;
                    }
                    bool flag = false;

                    // Check if one of the operands is a pointer, discard the
                    // instruction if it is the case
                    for (unsigned i = 0; i < base_instruction->getNumOperands();
                         i++) {
                        SLIMOperand *operand_i =
                            base_instruction->getOperand(i).first;

                        if (base_instruction->getResultOperand().first &&
                            base_instruction->getResultOperand()
                                .first->getValue() &&
                            discarded_operands_for_ssa.find(
                                operand_i->getValue()) !=
                                discarded_operands_for_ssa.end()) {
                            discarded_operands_for_ssa.insert(
                                base_instruction->getResultOperand()
                                    .first->getValue());
                            is_discarded = true;
                            break;
                        }

                        // Check if the type of operand is a pointer
                        if (operand_i->getValue() &&
                            llvm::isa<llvm::PointerType>(
                                operand_i->getValue()->getType())) {
                            is_discarded = true;
                            break;
                        }
                    }

                    if (is_discarded) {
                        this->total_instructions++;
                        continue;
                    }
                }
#endif
                if (base_instruction->getInstructionType() !=
                        InstructionType::CALL &&
                    base_instruction->getResultOperand().first &&
                    base_instruction->getResultOperand().first->getValue() &&
                    !base_instruction->getResultOperand().first->isSource()) {
                    extractRefersTo(base_instruction);
                    extractRefersToIndirection(base_instruction);
                    // this->total_instructions++;
                    // continue;
                }

                // if (base_instruction->getInstructionType() ==
                // InstructionType::LOAD)
                // {
                //     LoadInstruction *load_inst = (LoadInstruction *)
                //     base_instruction;
                //
                //     SLIMOperand *rhs_operand =
                //     load_inst->getOperand(0).first; SLIMOperand
                //     *result_operand = load_inst->getResultOperand().first;
                //
                //     if
                //     (llvm::isa<llvm::PointerType>(rhs_operand->getValue()->getType()->getContainedType(0)))
                //     {
                //         total_pointer_assignments++;
                //     }
                //     else
                //     {
                //         total_non_pointer_assignments++;
                //     }
                // }
                // else if (base_instruction->getInstructionType() ==
                // InstructionType::STORE)
                // {
                //     // Discard the instruction if the result operand is of
                //     pointer type StoreInstruction *store_inst =
                //     (StoreInstruction *) base_instruction;
                //
                //     SLIMOperand *result_operand =
                //     store_inst->getResultOperand().first;
                //
                //     if
                //     (llvm::isa<llvm::PointerType>(result_operand->getValue()->getType()->getContainedType(0)))
                //     {
                //         total_pointer_assignments++;
                //     }
                //     else
                //     {
                //         total_non_pointer_assignments++;
                //     }
                // }
                // else if (base_instruction->getResultOperand().first !=
                // nullptr &&
                // base_instruction->getResultOperand().first->getValue() &&
                // base_instruction->getResultOperand().first->getValue()->hasName())
                // {
                //     SLIMOperand *result =
                //     base_instruction->getResultOperand().first;
                //
                //     if
                //     (llvm::isa<llvm::PointerType>(result->getValue()->getType()))
                //     {
                //         total_pointer_assignments++;
                //     }
                //     else
                //     {
                //         total_non_pointer_assignments++;
                //     }
                // }
                // else
                // {
                //     total_other_instructions++;
                // }
                //
                // if (base_instruction->getInstructionType() ==
                // InstructionType::LOAD)
                // {
                //     LoadInstruction *load_inst = (LoadInstruction *)
                //     base_instruction;
                //
                //     SLIMOperand *rhs_operand =
                //     load_inst->getOperand(0).first; SLIMOperand
                //     *result_operand = load_inst->getResultOperand().first;
                //
                //     if
                //     (llvm::isa<llvm::PointerType>(result_operand->getType()))
                //     {
                //         total_local_pointers++;
                //     }
                //     else if
                //     (!llvm::isa<llvm::StructType>(result_operand->getType())
                //     &&
                //     !llvm::isa<llvm::ArrayType>(result_operand->getType()))
                //     {
                //         total_local_scalers++;
                //     }
                // }
                // else if (base_instruction->getInstructionType() !=
                // InstructionType::STORE &&
                // base_instruction->getResultOperand().first != nullptr &&
                // base_instruction->getResultOperand().first->getValue() &&
                // base_instruction->getResultOperand().first->getValue()->hasName())
                // {
                //     SLIMOperand *result_operand =
                //     base_instruction->getResultOperand().first;
                //
                //     if
                //     (llvm::isa<llvm::PointerType>(result_operand->getType()))
                //     {
                //         total_local_pointers++;
                //     }
                //     else if
                //     (!llvm::isa<llvm::StructType>(result_operand->getType())
                //     &&
                //     !llvm::isa<llvm::ArrayType>(result_operand->getType()))
                //     {
                //         total_local_scalers++;
                //     }
                // }

                if (base_instruction->getInstructionType() ==
                    InstructionType::CALL) {
                    processCallInstruction(function, base_instruction,
                                           renamed_temporaries, instruction,
                                           basic_block);
                }
                insertInstrAtBack(base_instruction, &basic_block);

                if (base_instruction->getInstructionType() ==
                    InstructionType::CALL) {
                    addReturnArgumentForCall(function, base_instruction,
                                           renamed_temporaries, instruction,
                                           basic_block);
                }
                // Check if the instruction is a "Return" instruction
                if (base_instruction->getInstructionType() ==
                    InstructionType::RETURN) {
                    // As we are using the 'mergereturn' pass, there is only one
                    // return statement in every function and therefore, we will
                    // have only 1 return operand which we store in the
                    // function_return_operand map
                    ReturnInstruction *return_instruction =
                        (ReturnInstruction *)base_instruction;

                    bool isVoidReturn =
                        return_instruction->getNumOperands() == 0;

                    OperandRepository::setFunctionReturnOperand(
                        &function,
                        isVoidReturn ? nullptr
                                     : return_instruction->getReturnOperand());
                }
            }
        }

        // For each basic block in the function
        for (llvm::BasicBlock &basic_block : function.getBasicBlockList()) {
            // Create function-basicblock pair
            std::pair<llvm::Function *, llvm::BasicBlock *> func_basic_block{
                &function, &basic_block};

            if (this->func_bb_to_inst_id.find(func_basic_block) ==
                this->func_bb_to_inst_id.end()) {
                this->basic_block_to_id[&basic_block] =
                    this->total_basic_blocks;

                this->total_basic_blocks++;

                this->func_bb_to_inst_id[func_basic_block] =
                    std::list<long long>();
            }
        }
    }
    // llvm::outs() << "\n\told begin\n";
    calculateRefersToSourceValues();
    mapLLVMInstructiontoSLIM();

    SLIM_DEBUG(printSLIMStats());
}

void slim::IR::printSLIMStats() {
    llvm::outs() << "Total number of functions: " << functions.size() << "\n";
    llvm::outs() << "Total number of basic blocks: " << total_basic_blocks
                 << "\n";
    llvm::outs() << "Total number of instructions: " << total_instructions
                 << "\n";
    llvm::outs() << "Total number of call instructions: "
                 << total_call_instructions << "\n";
    llvm::outs() << "Total number of direct-call instructions: "
                 << total_direct_call_instructions << "\n";
    llvm::outs() << "Total number of indirect-call instructions: "
                 << total_indirect_call_instructions << "\n";
    // llvm::outs() << "Total number of pointer assignments: " <<
    // total_pointer_assignments << "\n"; llvm::outs() << "Total number of
    // non-pointer assignments: " << total_non_pointer_assignments << "\n";
    // llvm::outs() << "Total number of other instructions: " <<
    // total_other_instructions << "\n"; llvm::outs() << "Total number of local
    // pointers: " << total_local_pointers << "\n"; llvm::outs() << "Total
    // number of local scaler variables: " << total_local_scalers << "\n";
}

void slim::IR::readDebugSourceVarInfo(llvm::Instruction &instruction) {
    if (auto *dbgInstr = llvm::dyn_cast<llvm::DbgValueInst>(&instruction)) {
        // below is the name of the LLVM variable
        // unnamed variables are ones that were
        // constant-folded away, so we ignore them here.
        if (dbgInstr->getValue()->hasName()) {
            // get the name from the source
            if (auto *diLocalVariable = dbgInstr->getVariable()) {
                OperandRepository::getOrCreateSLIMOperand(dbgInstr->getValue())
                    ->setDebugSourceVariable(diLocalVariable);
            }
        }
        // else we can handle the constant variables with a name
    } else if (auto *dbgInstr =
                   llvm::dyn_cast<llvm::DbgDeclareInst>(&instruction)) {
        if (auto *diLocalVariable = dbgInstr->getVariable()) {
            OperandRepository::getOrCreateSLIMOperand(dbgInstr->getAddress())
                ->setDebugSourceVariable(diLocalVariable);
        }
    }
}

BaseInstruction *slim::IR::getDefinitionInstruction(SLIMOperand *operand) {
    llvm::Instruction *instruction;
    if (instruction = llvm::dyn_cast<llvm::Instruction>(operand->getValue())) {
        return LLVMInstructiontoSLIM[instruction];
    }
    return nullptr;
}

void slim::IR::processCallInstruction(
    llvm::Function &function, BaseInstruction *base_instruction,
    std::set<llvm::Value *> &renamed_temporaries,
    llvm::Instruction &instruction, llvm::BasicBlock &basic_block) {

    total_call_instructions++;
    this->num_call_instructions[&function] += 1;
    CallInstruction *call_instruction = (CallInstruction *)base_instruction;
    if (call_instruction->isIndirectCall())
        total_indirect_call_instructions++;
    else
        total_direct_call_instructions++;

    // modSB----
    if (!call_instruction->isIndirectCall() &&
        call_instruction->getCalleeFunction()->isDeclaration()) {
        // Does deal with scanf/any input statemnents

        // llvm::outs() << "\nfound some scanf but new\n";

        if (call_instruction->getCalleeFunction()->getName() ==
                "__isoc99_sscanf" ||
            call_instruction->getCalleeFunction()->getName() ==
                "__isoc99_fscanf" ||
            call_instruction->getCalleeFunction()->getName() ==
                "__isoc99_scanf") {
            auto numInputops = base_instruction->getRHS().size();

            for (auto ops = 1; ops < numInputops; ops++) {
                auto newInputInstruction =
                    base_instruction->createNewInputInstruction(
                        base_instruction, base_instruction->getOperand(ops));
            }
        }
    }
    //---- //modSB

    if (!call_instruction->isIndirectCall() &&
        !call_instruction->getCalleeFunction()
             ->isDeclaration()) { // does not deal with
                                  // scanf/any input statemnents

        for (unsigned arg_i = 0;
             arg_i < call_instruction->getNumFormalArguments(); arg_i++) {
            llvm::Argument *formal_argument =
                call_instruction->getFormalArgument(arg_i);
            SLIMOperand *formal_slim_argument =
                OperandRepository::getSLIMOperand(formal_argument);

            // if
            // (llvm::isa<llvm::PointerType>(formal_argument->getType()))
            //     continue ;
            // if
            // (llvm::isa<llvm::ArrayType>(formal_argument->getType()))
            //     continue ;
            // if
            // (llvm::isa<llvm::StructType>(formal_argument->getType()))
            //     continue ;

            if (!formal_slim_argument) {
                formal_slim_argument = new SLIMOperand(formal_argument);
                OperandRepository::setSLIMOperand(formal_argument,
                                                  formal_slim_argument);

                // llvm::outs() << "\nfound some scanf\n";

                if (formal_argument->hasName() &&
                    renamed_temporaries.find(formal_argument) ==
                        renamed_temporaries.end()) {
                    llvm::StringRef old_name = formal_argument->getName();
                    formal_argument->setName(
                        old_name + "_" +
                        call_instruction->getCalleeFunction()->getName());
                    renamed_temporaries.insert(formal_argument);
                }

                formal_slim_argument->setFormalArgument();
            }
//llvm::outs() << "\n PRINTING BASE INST :: ";
//base_instruction->printLLVMInstruction();
            LoadInstruction *new_load_instr = new LoadInstruction(
                &llvm::cast<llvm::CallInst>(
                    *base_instruction->getLLVMInstruction()),
                formal_slim_argument,
                call_instruction->getOperand(arg_i).first);

            insertInstrAtBack(new_load_instr, &basic_block);

            // llvm::outs() << "\nfound some scanf fsacnf sscanf\n";
        }
    }
}


void slim::IR::addReturnArgumentForCall(llvm::Function &function, 
    BaseInstruction *base_instruction,
    std::set<llvm::Value *> &renamed_temporaries,
    llvm::Instruction &instruction, llvm::BasicBlock &basic_block){

    CallInstruction *call_instruction = (CallInstruction *)base_instruction;        

    if (!call_instruction->isIndirectCall() &&
        !call_instruction->getCalleeFunction()
             ->isDeclaration()) { // does not deal with
                                  // scanf/any input statemnents
        
        llvm::Function * callee_function = call_instruction->getCalleeFunction();
        llvm::BasicBlock* return_block = getLastBasicBlock(callee_function);

        if (return_block){

            llvm::Instruction *terminator = return_block->getTerminator();
            llvm::ReturnInst *returnInst = llvm::dyn_cast<llvm::ReturnInst>(terminator);
            
            // Return inside a void function
            int numOperands = returnInst->getNumOperands();
            if (numOperands == 0) return;
            
            llvm::Value* return_argument = returnInst->getOperand(0);


            SLIMOperand *return_slim_argument =
                OperandRepository::getSLIMOperand(return_argument);

            if (!return_slim_argument) {
                return_slim_argument = new SLIMOperand(return_argument);
                OperandRepository::setSLIMOperand(return_argument,
                                                  return_slim_argument);

                if (return_argument->hasName() &&
                    renamed_temporaries.find(return_argument) ==
                        renamed_temporaries.end()) {
                    llvm::StringRef old_name = return_argument->getName();
                    return_argument->setName(
                        old_name + "_" +
                        call_instruction->getCalleeFunction()->getName());
                    renamed_temporaries.insert(return_argument);
                }

                return_slim_argument->setFormalArgument();
            }

            LoadInstruction *new_load_instr = new LoadInstruction(
                &llvm::cast<llvm::CallInst>(
                    *base_instruction->getLLVMInstruction()),
                call_instruction->getResultOperand().first, return_slim_argument);

            insertInstrAtBack(new_load_instr, &basic_block);
        }
    }
}

// Ensure that all temporaries have unique name (globally) by
// appending the function name
// after the temporary name
void slim::IR::renameTemporaryOperands(
    BaseInstruction *base_instruction,
    std::set<llvm::Value *> &renamed_temporaries, llvm::Function &function) {
    llvm::Instruction *instruction = base_instruction->getLLVMInstruction();
    // llvm::outs() << "rename: " << *instruction << "\n";
    auto renameOperand = [&](llvm::Value *operand_i) {
        if (llvm::isa<llvm::GlobalValue>(operand_i))
            return;

        if (operand_i->hasName() &&
            renamed_temporaries.find(operand_i) == renamed_temporaries.end()) {
            llvm::StringRef old_name = operand_i->getName();
            operand_i->setName(old_name + "_" + function.getName());
            renamed_temporaries.insert(operand_i);
            name_to_variable_object[operand_i->getName().str()] = operand_i;
        }
    };
    auto resultOperand = base_instruction->getResultOperand();
    if (resultOperand.first && resultOperand.first->getValue()) {
        renameOperand(resultOperand.first->getValue());
    }
    for (unsigned i = 0; i < instruction->getNumOperands(); i++) {
        llvm::Value *operand_i = instruction->getOperand(i);
        renameOperand(operand_i);
    }
}

// Destructor
slim::IR::~IR() {
}

// Returns the total number of instructions across all the functions and basic
// blocks
long long slim::IR::getTotalInstructions() {
    return this->total_instructions;
}

// Return the total number of functions in the module
unsigned slim::IR::getNumberOfFunctions() {
    return this->functions.size();
}

// Return the total number of basic blocks in the module
long long slim::IR::getNumberOfBasicBlocks() {
    return this->total_basic_blocks;
}

// Returns the pointer to llvm::Function for the function at the given index
llvm::Function *slim::IR::getLLVMFunction(unsigned index) {
    // Make sure that the index is not out-of-bounds
    assert(index >= 0 && index < this->getNumberOfFunctions());

    return this->functions[index];
}

// Add instructions for function-basicblock pair (used by the LegacyIR)
void slim::IR::addFuncBasicBlockInstructions(llvm::Function *function,
                                             llvm::BasicBlock *basic_block) {
    // Create function-basicblock pair
    std::pair<llvm::Function *, llvm::BasicBlock *> func_basic_block{
        function, basic_block};

    // For each instruction in the basic block
    for (llvm::Instruction &instruction : basic_block->getInstList()) {
        BaseInstruction *base_instruction =
            slim::convertLLVMToSLIMInstruction(instruction);

        insertInstrAtBack(base_instruction, basic_block);
    }
}

// Return the function-basicblock to instructions map (required by the LegacyIR)
std::map<std::pair<llvm::Function *, llvm::BasicBlock *>, std::list<long long>>
    &slim::IR::getFuncBBToInstructions() {
    return this->func_bb_to_inst_id;
}

// Get the instruction id to SLIM instruction map (required by the LegacyIR)
// //original
std::unordered_map<long long, BaseInstruction *> &
slim::IR::getIdToInstructionsMap() {
    return this->inst_id_to_object;
}

// Returns the first instruction id in the instruction list of the given
// function-basicblock pair
long long slim::IR::getFirstIns(llvm::Function *function,
                                llvm::BasicBlock *basic_block) {
    // Make sure that the list corresponding to the function-basicblock pair
    // exists
    assert(this->func_bb_to_inst_id.find({function, basic_block}) !=
           this->func_bb_to_inst_id.end());

    auto result = func_bb_to_inst_id.find({function, basic_block});

#ifndef DISABLE_IGNORE_EFFECT
    auto it = result->second.begin();

    while (it != result->second.end() &&
           this->inst_id_to_object[*it]->isIgnored()) {
        it++;
    }

    return (it == result->second.end() ? -1 : (*it));
#else
    return result->second.front();
#endif
}

// Returns the last instruction id in the instruction list of the given
// function-basicblock pair
long long slim::IR::getLastIns(llvm::Function *function,
                               llvm::BasicBlock *basic_block) {
    // Make sure that the list corresponding to the function-basicblock pair
    // exists
    assert(this->func_bb_to_inst_id.find({function, basic_block}) !=
           this->func_bb_to_inst_id.end());

    auto result = func_bb_to_inst_id.find({function, basic_block});

#ifndef DISABLE_IGNORE_EFFECT
    auto it = result->second.rbegin();

    while (it != result->second.rend() &&
           this->inst_id_to_object[*it]->isIgnored()) {
        it++;
    }

    return (it == result->second.rend() ? -1 : (*it));
#else
    return result->second.back();
#endif
}

// Returns the reversed instruction list for a given function and a basic block
std::list<long long>
slim::IR::getReverseInstList(llvm::Function *function,
                             llvm::BasicBlock *basic_block) {
    // Make sure that the list corresponding to the function-basicblock pair
    // exists
    assert(this->func_bb_to_inst_id.find({function, basic_block}) !=
           this->func_bb_to_inst_id.end());

    std::list<long long> inst_list =
        this->func_bb_to_inst_id[{function, basic_block}];

    inst_list.reverse();

    return inst_list;
}

// Returns the reversed instruction list (for the list passed as an argument)
std::list<long long>
slim::IR::getReverseInstList(std::list<long long> inst_list) {
    inst_list.reverse();
    return inst_list;
}

// Get SLIM instruction from the instruction index
BaseInstruction *slim::IR::getInstrFromIndex(long long index) {
    return this->inst_id_to_object[index];
}

BaseInstruction *slim::IR::getInstrFromOperand(SLIMOperand *operand) {
    assert(operand != nullptr && "Operand must not be null");
    llvm::Value *value = operand->getValue();
    if (value) {
        if (llvm::Instruction *instruction =
                llvm::dyn_cast<llvm::Instruction>(value)) {
            return LLVMInstructiontoSLIM[instruction];
        }
    }
    return nullptr;
}

// Get basic block id
long long slim::IR::getBasicBlockId(llvm::BasicBlock *basic_block) {
    assert(this->basic_block_to_id.find(basic_block) !=
           this->basic_block_to_id.end());

    return this->basic_block_to_id[basic_block];
}

// Inserts instruction at the front of the basic block (only in this
// abstraction)
void slim::IR::insertInstrAtFront(BaseInstruction *instruction,
                                  llvm::BasicBlock *basic_block) {
    assert(instruction != nullptr && basic_block != nullptr);

    instruction->setInstructionId(this->total_instructions);

    this->func_bb_to_inst_id[std::make_pair(basic_block->getParent(),
                                            basic_block)]
        .push_front(this->total_instructions);

    this->inst_id_to_object[this->total_instructions] = instruction;

    this->total_instructions++;
}

// Inserts instruction at the end of the basic block (only in this abstraction)
void slim::IR::insertInstrAtBack(BaseInstruction *instruction,
                                 llvm::BasicBlock *basic_block) {
    assert(instruction != nullptr && basic_block != nullptr);

    instruction->setInstructionId(this->total_instructions);

    this->func_bb_to_inst_id[std::make_pair(basic_block->getParent(),
                                            basic_block)]
        .push_back(this->total_instructions);

    this->inst_id_to_object[this->total_instructions] = instruction;

    this->total_instructions++;
}

// Optimize the IR (please use only when you are using the MemorySSAFlag)
slim::IR *slim::IR::optimizeIR() {
    // Create the new slim::IR object which would contain the IR instructions
    // after optimization
    slim::IR *optimized_slim_ir = new slim::IR();

    // errs() << "funcBBInsMap size: " << funcBBInsMap.size() << "\n";

    // Now, we are ready to do the load-store optimization
    for (auto func_basicblock_instr_entry : this->func_bb_to_inst_id) {
        // Add the function-basic-block entry in optimized_slim_ir
        optimized_slim_ir
            ->func_bb_to_inst_id[func_basicblock_instr_entry.first] =
            std::list<long long>{};

        std::list<long long> &instruction_list =
            func_basicblock_instr_entry.second;

        // errs() << "instruction_list size: " << instruction_list.size() <<
        // "\n";

        long long temp_instr_counter = 0;
        std::map<llvm::Value *, BaseInstruction *> temp_token_to_instruction;
        std::map<long long, BaseInstruction *> temp_instructions;
        std::map<BaseInstruction *, long long> temp_instruction_ids;

        for (auto instruction_id : instruction_list) {
            // errs() << "Instruction id : " << instruction_id << "\n";
            BaseInstruction *slim_instruction =
                this->inst_id_to_object[instruction_id];
            // Check if the corresponding LLVM instruction is a Store
            // Instruction
            if (slim_instruction->getInstructionType() ==
                    InstructionType::STORE &&
                slim_instruction->getNumOperands() == 1 &&
                slim_instruction->getResultOperand().first != nullptr) {
                // Retrieve the RHS operand of the SLIM instruction
                // (corresponding to the LLVM IR store instruction)
                SLIMOperandIndirectionPair slim_instr_rhs =
                    slim_instruction->getOperand(0);

                // Extract the value and its indirection level from
                // slim_instr_rhs
                llvm::Value *slim_instr_rhs_value =
                    slim_instr_rhs.first->getValue();
                int token_indirection = slim_instr_rhs.second;

                // Check if the RHS Value is defined in an earlier SLIM
                // statement
                if (temp_token_to_instruction.find(slim_instr_rhs_value) !=
                    temp_token_to_instruction.end()) {
                    // Get the instruction (in the unoptimized SLIM IR)
                    BaseInstruction *value_def_instr =
                        temp_token_to_instruction[slim_instr_rhs_value];
                    long long value_def_index =
                        temp_instruction_ids[value_def_instr];

                    // Check if the statement is a load instruction
                    bool is_load_instr = (llvm::isa<llvm::LoadInst>(
                        value_def_instr->getLLVMInstruction()));
                    //llvm::outs() << "\n OPTIMIZE IR:: ";
                    value_def_instr->printLLVMInstruction();

                    // Get the indirection level of the LHS operand in the load
                    // instruction
                    int map_entry_indirection =
                        value_def_instr->getResultOperand().second;

                    // Get the indirection level of the RHS operand in the load
                    // instruction
                    int map_entry_rhs_indirection =
                        value_def_instr->getOperand(0).second;

                    // Adjust the indirection level
                    int distance = token_indirection - map_entry_indirection +
                                   map_entry_rhs_indirection;

                    // Check if the RHS is a SSA variable (created using
                    // MemorySSA)
                    bool is_rhs_global_ssa_variable =
                        (slim::is_ssa_version_available.find(
                             slim_instr_rhs_value->getName().str()) !=
                         slim::is_ssa_version_available.end());

                    // Modify the RHS operand with the new indirection level if
                    // it does not exceed 2
                    if (is_load_instr && (distance >= 0 && distance <= 2) &&
                        !is_rhs_global_ssa_variable) {
                        // errs() << slim_instr_rhs.first->getName() << " =
                        // name\n";

                        // Set the indirection level of the RHS operand to the
                        // adjusted indirection level
                        value_def_instr->setRHSIndirection(0, distance);

                        // Update the RHS operand of the store instruction
                        slim_instruction->setOperand(
                            0, value_def_instr->getOperand(0));

                        // Remove the existing entries
                        temp_token_to_instruction.erase(slim_instr_rhs_value);
                        temp_instructions.erase(value_def_index);
                    }
                } else {
                    // errs() << "RHS is not present as LHS!\n";
                }

                // Check whether the LHS operand can be replaced
                SLIMOperandIndirectionPair slim_instr_lhs =
                    slim_instruction->getResultOperand();

                // Extract the value and its indirection level from
                // slim_instr_rhs
                llvm::Value *slim_instr_lhs_value =
                    slim_instr_lhs.first->getValue();
                token_indirection = slim_instr_lhs.second;

                // Check if the LHS Value is defined in an earlier SLIM
                // statement
                if (temp_token_to_instruction.find(slim_instr_lhs_value) !=
                    temp_token_to_instruction.end()) {
                    // Get the instruction (in the unoptimized SLIM IR)
                    BaseInstruction *value_def_instr =
                        temp_token_to_instruction[slim_instr_lhs_value];
                    long long value_def_index =
                        temp_instruction_ids[value_def_instr];

                    // Check if the statement is a load instruction
                    bool is_load_instr = (llvm::isa<llvm::LoadInst>(
                        value_def_instr->getLLVMInstruction()));
//llvm::outs() << "\n OPTIMIZE IR 1:: ";
                    value_def_instr->printLLVMInstruction();

                    // Get the indirection level of the LHS operand in the load
                    // instruction
                    int map_entry_indirection =
                        value_def_instr->getResultOperand().second;

                    // Get the indirection level of the RHS operand in the load
                    // instruction
                    int map_entry_rhs_indirection =
                        value_def_instr->getOperand(0).second;

                    // Adjust the indirection level
                    int distance = token_indirection - map_entry_indirection +
                                   map_entry_rhs_indirection;

                    // Check if the RHS is a SSA variable (created using
                    // MemorySSA)
                    bool is_rhs_global_ssa_variable =
                        (slim::is_ssa_version_available.find(
                             slim_instr_lhs_value->getName().str()) !=
                         slim::is_ssa_version_available.end());

                    // Modify the RHS operand with the new indirection level if
                    // it does not exceed 2
                    if (is_load_instr && (distance >= 0 && distance <= 2) &&
                        !is_rhs_global_ssa_variable) {
                        // errs() << slim_instr_rhs.first->getName() << " =
                        // name\n";

                        // Set the indirection level of the RHS operand to the
                        // adjusted indirection level
                        value_def_instr->setRHSIndirection(0, distance);

                        // Update the result operand of the store instruction
                        slim_instruction->setResultOperand(
                            value_def_instr->getOperand(0));

                        // Remove the existing entries
                        temp_token_to_instruction.erase(slim_instr_lhs_value);
                        temp_instructions.erase(value_def_index);
                    }
                }

                // Add the SLIM instruction (whether modified or not)
                if (slim_instruction->getInstructionType() ==
                    InstructionType::LOAD)
                    temp_token_to_instruction[slim_instruction
                                                  ->getResultOperand()
                                                  .first->getValue()] =
                        slim_instruction;
                temp_instructions[temp_instr_counter] = slim_instruction;
                temp_instruction_ids[slim_instruction] = temp_instr_counter;
                temp_instr_counter++;
            } else {
                // errs() << "Size != 1\n";
                //  Add the SLIM instruction
                //  Add the SLIM instruction (whether modified or not)
                if (slim_instruction->getInstructionType() ==
                    InstructionType::LOAD)
                    temp_token_to_instruction[slim_instruction
                                                  ->getResultOperand()
                                                  .first->getValue()] =
                        slim_instruction;
                temp_instructions[temp_instr_counter] = slim_instruction;
                temp_instruction_ids[slim_instruction] = temp_instr_counter;
                temp_instr_counter++;
            }
        }

        // Now, we have the final list of optimized instructions in this basic
        // block. So, we insert the instructions in the optimized global
        // instructions list and the instruction indices (after the
        // optimization) in the func_basic_block_optimized_instrs map
        for (auto temp_instruction : temp_instructions) {
            temp_instruction.second->setInstructionId(
                optimized_slim_ir->total_instructions);
            optimized_slim_ir
                ->func_bb_to_inst_id[func_basicblock_instr_entry.first]
                .push_back(optimized_slim_ir->total_instructions);
            optimized_slim_ir
                ->inst_id_to_object[optimized_slim_ir->total_instructions] =
                temp_instruction.second;
            optimized_slim_ir->total_instructions++;
        }

        optimized_slim_ir
            ->basic_block_to_id[func_basicblock_instr_entry.first.second] =
            optimized_slim_ir->total_basic_blocks++;
    }
    optimized_slim_ir->mapLLVMInstructiontoSLIM();
    return optimized_slim_ir;
}

void slim::IR::phiMMAprint() {
    SLIM_DEBUG(llvm::outs() << "\nnode phi mma";

               for (auto it
                    : phiMMA) {
                   llvm::outs() << "\n"
                                << it.first << "     " << it.second.first
                                << "      " << it.second.second;
               });
}

// reset InstructionMap value to NULL
void slim::IR::resetInstructionMap() {
    // Keeps track whether the function details have been printed or not
    std::unordered_map<llvm::Function *, bool> func_visited;

    // Iterate over the function basic block map
    for (auto &entry : this->func_bb_to_inst_id) {
        llvm::Function *func = entry.first.first;
        llvm::BasicBlock *basic_block = entry.first.second;

        // Check if the function details are already printed and if not, print
        // the details and mark the function as visited
        if (func_visited.find(func) == func_visited.end()) {
            // Mark the function as visited
            func_visited[func] = true;
        }

        for (long long instruction_id : entry.second) {
            BaseInstruction *instruction = inst_id_to_object[instruction_id];
            InstructionMap[instruction].push_back(NULL);
            instruction->printInstruction();
        }
    }
}

// reset InstructionMap value equal to key value
void slim::IR::setInstructionMap() {
    // Keeps track whether the function details have been printed or not
    std::unordered_map<llvm::Function *, bool> func_visited;

    // Iterate over the function basic block map
    for (auto &entry : this->func_bb_to_inst_id) {
        llvm::Function *func = entry.first.first;
        llvm::BasicBlock *basic_block = entry.first.second;

        // Check if the function details are already printed and if not, print
        // the details and mark the function as visited
        if (func_visited.find(func) == func_visited.end()) {
            // Mark the function as visited
            func_visited[func] = true;
        }

        for (long long instruction_id : entry.second) {
            BaseInstruction *instruction = inst_id_to_object[instruction_id];
            InstructionMap[instruction].push_back(instruction);
            instruction->printInstruction();
        }
    }
}

void slim::IR::printInstructionMap() {
    SLIM_DEBUG(for (auto instr
                    : InstructionMap) {
        llvm::outs() << "\n***************\n";
        instr.first->printInstruction();
        llvm::outs() << "    ";
        for (auto ins :
             instr.second) // instr.second.begin()->printInstruction();
        {
            if (ins == NULL)
                llvm::outs() << "\nNULL";
            else
                ins->printInstruction();
        }
        llvm::outs() << "\n***************";
    });
}

// Dump the IR
void slim::IR::dumpIR() {
    llvm::outs() << "\n SLIM IR dump 1..............";
    // Keeps track whether the function details have been printed or not
    std::unordered_map<llvm::Function *, bool> func_visited;

    // Iterate over the function basic block map
    for (auto &entry : this->func_bb_to_inst_id) {
        llvm::Function *func = entry.first.first;
        llvm::BasicBlock *basic_block = entry.first.second;

        // Check if the function details are already printed and if not, print
        // the details and mark the function as visited
        if (func_visited.find(func) == func_visited.end()) {
            if (func->getSubprogram())
                llvm::outs()
                    << "[" << func->getSubprogram()->getFilename() << "] ";

            llvm::outs() << "Function: " << func->getName() << "\n";
            llvm::outs() << "-------------------------------------"
                         << "\n";

            // Mark the function as visited
            func_visited[func] = true;
        }

        // Print the basic block name
        llvm::outs() << "Basic block " << this->getBasicBlockId(basic_block)
                     << ": " << basic_block->getName() << " (Predecessors: ";
        llvm::outs() << "[";

        // Print the names of predecessor basic blocks
        for (auto pred = llvm::pred_begin(basic_block);
             pred != llvm::pred_end(basic_block); pred++) {
            llvm::outs() << (*pred)->getName();

            if (std::next(pred) != ((llvm::pred_end(basic_block)))) {
                llvm::outs() << ", ";
            }
        }

        llvm::outs() << "])\n";
        
        for (long long instruction_id : entry.second) {
            BaseInstruction *instruction = inst_id_to_object[instruction_id];
            llvm::outs() << " [" << instruction_id << "]";

            instruction->printInstruction();
        }

        llvm::outs() << "\n\n";
    }
}

void slim::IR::dumpIR(std::string source_file_name) {
#ifndef DEBUG
    // modSB
    std::error_code ec;
    // llvm::sys::fs::OpenFlags llvm::sys::fs::OF_None ;
    llvm::raw_fd_ostream *dotFile;
    llvm::outs() << "\n"
                 << " writing to file: ";
    if (flag_cosSSA) // modSB //remove after SLIM is empowered
    {
#ifndef UNCONDITIONAL_MUTATION
        dotFile =
            new llvm::raw_fd_ostream("slim-isCoS_" + source_file_name + ".dot",
                                     ec, llvm::sys::fs::OF_None);
        llvm::outs() << "slim-isCoS_" << source_file_name << ".dot"
                     << "\n";
#else
        dotFile = new llvm::raw_fd_ostream("UCM_slim-isCoS_" +
                                               source_file_name + ".dot",
                                           ec, llvm::sys::fs::OF_None);
        llvm::outs() << "UCM_slim-isCoS_" << source_file_name << ".dot"
                     << "\n";
#endif
    } else {
#ifndef UNCONDITIONAL_MUTATION
        dotFile =
            new llvm::raw_fd_ostream("slim-notCoS_" + source_file_name + ".dot",
                                     ec, llvm::sys::fs::OF_None);
        llvm::outs() << "slim-notCoS_" << source_file_name << ".dot"
                     << "\n";
#else
        dotFile = new llvm::raw_fd_ostream("UCM_slim-notCoS_" +
                                               source_file_name + ".dot",
                                           ec, llvm::sys::fs::OF_None);
        llvm::outs() << "UCM_slim-notCoS_" << source_file_name << ".dot"
                     << "\n";
#endif
    }

    // //modSB
    // /* open a DOT file to write to */
    // std::ofstream dotFile;

    // llvm::StringRef fileName= "slimCFG_llvm.dot";
    // std::error_code EC;

    // // llvm::raw_fd_ostream(llvm:(fileName, &EC));
    // dotFile.open("slimCFG.dot");

    *dotFile << "digraph S";
    *dotFile << "{\n";

    // printInstruction(*out);

    int temp_count = -1;   // temp variable to count BB number
    std::string temp_func; // temp variable to store function name

    // Keeps track whether the function details have been printed or not
    std::unordered_map<llvm::Function *, bool> func_visited;

    // Iterate over the function basic block map
    for (auto &entry : this->func_bb_to_inst_id) {
        temp_count++; // holds the current basic block number
        llvm::Function *func = entry.first.first;
        llvm::BasicBlock *basic_block = entry.first.second;

        // Check if the function details are already printed and if not, print
        // the details and mark the function as visited
        if (func_visited.find(func) == func_visited.end()) {
            if (func->getSubprogram())
                llvm::outs()
                    << "[" << func->getSubprogram()->getFilename() << "] ";

            llvm::outs() << "Function: " << func->getName() << "\n";
            llvm::outs() << "-------------------------------------"
                         << "\n";

            // modSB
            // ensures that the function entered is not the first function in
            // the program, and connect last BB of previous function, with its
            // exit. here temp_func holds the name of the previous function.
            if (temp_count > 0)
                *dotFile << "\n\"" << temp_count - 1 << "\"->"
                         << "\"exit " << temp_func << "\"\n";

            temp_func = (std::string)func->getName(); //-> .. f
            *dotFile << "\"" << temp_func << "\""
                     << "\n";              // f
            *dotFile << temp_func << "->"; // to build the flow graph

            // Mark the function as visited
            func_visited[func] = true;
        }

        // Print the basic block name
        llvm::outs() << "Basic block " << this->getBasicBlockId(basic_block)
                     << ": " << basic_block->getName() << " (Predecessors: ";
        llvm::outs() << "[";

        // modSB
        //  Write BB name to dot file
        long long int temp_pred = 0;
        // temp_i3 = this->getBasicBlockId(basic_block);
        *dotFile << "\"" << this->getBasicBlockId(basic_block) << "\""
                 << "\n";
        *dotFile << "\"" << this->getBasicBlockId(basic_block) << "\""
                 << "[shape=box]\n"; // BB [shape = box]
        *dotFile << "\"" << this->getBasicBlockId(basic_block) << "\""
                 << "[label = \"";

        // test mod SB
        auto pred1 = llvm::pred_begin(basic_block);
        // if((*pred1)->getName()==[])
        // {
        //     llvm::outs()<<"No predecessors null\n";
        // }
        // test mod SB
        // check if pred = 0
        if (pred1 == llvm::pred_end(basic_block)) {
            temp_pred = -1;
        }

        // Print the names of predecessor basic blocks
        for (auto pred = llvm::pred_begin(basic_block);
             pred != llvm::pred_end(basic_block); pred++) {
            temp_pred++;
            llvm::outs() << (*pred)->getName();

            if (std::next(pred) != ((llvm::pred_end(basic_block)))) {
                llvm::outs() << ", ";
            }
        }

        llvm::outs() << "])\n";

        for (long long instruction_id : entry.second) {
            BaseInstruction *instruction = inst_id_to_object[instruction_id];

            llvm::outs() << " [" << instruction_id << "]";
            if (instruction->isIgnored()) {
                llvm::outs() << " [x]";
            }
            instruction->printInstruction();

            // print instructions to llvm::outs()
            //  instruction->printInstruction();

            // //print CFG to dotFile //modSB
            // if(flag_cosSSA)  // modified CFG (with SSA names) //modSB
            // {
            //     // auto su = InstructionMap[instruction].begin();
            //     // auto sm= *su;
            //     for(auto ins : InstructionMap[instruction])
            //     {
            //         if(ins==NULL)
            //             continue;
            //         else
            //             ins->printInstrDot(dotFile,
            //             phiMMA[instruction_id].first,
            //             phiMMA[instruction_id].second); //modSB //remove
            //             after SLIM is empowered
            //     }

            // }
            // else    // original CFG //modSB
            // {
            instruction->printInstrDot(
                dotFile, phiMMA[instruction_id].first,
                phiMMA[instruction_id]
                    .second); // modSB //remove after SLIM is empowered
            // }
        }

        // modSB
        *dotFile << "\"]\n";

        if (temp_pred > 0) {
            for (auto pred = llvm::pred_begin(basic_block);
                 pred != llvm::pred_end(basic_block); pred++) {
                // temp_i1 = (this->getBasicBlockId(*pred));
                *dotFile << "\n\"" << this->getBasicBlockId(*pred) << "\""
                         << "->";
                *dotFile << "\"" << this->getBasicBlockId(basic_block) << "\"";

                if (std::next(pred) != ((llvm::pred_end(basic_block)))) {
                    *dotFile << "\n";
                }
            }
        }

        if (temp_count >= (func_bb_to_inst_id.size() - 1))
            *dotFile << "\n\"" << this->total_basic_blocks - 1 << "\"->"
                     << "\"exit " << temp_func << "\"\n";

        llvm::outs() << "\n\n";
    }
    // modSB
    *dotFile << "\n}";
    dotFile->close();
    llvm::outs() << "\nDONE writing to file\n";
#endif
}

unsigned slim::IR::getNumCallInstructions(llvm::Function *function) {
    return this->num_call_instructions[function];
}

void slim::IR::generateIR() { // this OG function is unused

    // Fetch the function list of the module
    llvm::SymbolTableList<llvm::Function> &function_list =
        this->llvm_module->getFunctionList();

    // For each function in the module
    for (llvm::Function &function : function_list) {
        // Append the pointer to the function to the "functions" list
        if (!function.isIntrinsic() && !function.isDeclaration()) {
            this->functions.push_back(&function);
        } else {
            continue;
        }

        // For each basic block in the function
        for (llvm::BasicBlock &basic_block : function.getBasicBlockList()) {
            // Create function-basicblock pair
            std::pair<llvm::Function *, llvm::BasicBlock *> func_basic_block{
                &function, &basic_block};

            this->basic_block_to_id[&basic_block] = this->total_basic_blocks;

            this->total_basic_blocks++;

            // For each instruction in the basic block
            for (llvm::Instruction &instruction : basic_block.getInstList()) {
                if (instruction.hasMetadataOtherThanDebugLoc() ||
                    instruction.isDebugOrPseudoInst()) {
                    continue;
                }

                BaseInstruction *base_instruction =
                    slim::convertLLVMToSLIMInstruction(instruction);

                // modSB----
                if (base_instruction->getInstructionType() ==
                    InstructionType::CALL) {
                    CallInstruction *call_instruction =
                        (CallInstruction *)base_instruction;

                    if (!call_instruction->isIndirectCall() &&
                        call_instruction->getCalleeFunction()
                            ->isDeclaration()) {
                        if (call_instruction->getCalleeFunction()->getName() ==
                                "__isoc99_sscanf" ||
                            call_instruction->getCalleeFunction()->getName() ==
                                "__isoc99_fscanf" ||
                            call_instruction->getCalleeFunction()->getName() ==
                                "__isoc99_scanf") {
                            auto numInputops =
                                base_instruction->getRHS().size();
                            for (auto ops : base_instruction->getRHS()) {
                                base_instruction->createNewInputInstruction(
                                    base_instruction, ops);
                            }
                        }
                    }
                    //---- //modSB
                    if (!call_instruction->isIndirectCall() &&
                        !call_instruction->getCalleeFunction()
                             ->isDeclaration()) {
                        for (unsigned arg_i = 0;
                             arg_i < call_instruction->getNumFormalArguments();
                             arg_i++) {
                            llvm::Argument *formal_argument =
                                call_instruction->getFormalArgument(arg_i);
                            SLIMOperand *formal_slim_argument =
                                OperandRepository::getSLIMOperand(
                                    formal_argument);

                            if (!formal_slim_argument) {
                                formal_slim_argument =
                                    new SLIMOperand(formal_argument);
                                OperandRepository::setSLIMOperand(
                                    formal_argument, formal_slim_argument);
                            }

                            LoadInstruction *new_load_instr =
                                new LoadInstruction(
                                    &llvm::cast<llvm::CallInst>(instruction),
                                    formal_slim_argument,
                                    call_instruction->getOperand(arg_i).first);

                            insertInstrAtBack(new_load_instr, &basic_block);
                        }
                    }
                }

                insertInstrAtBack(base_instruction, &basic_block);
            }
        }
    }
}

// Returns the LLVM module
std::unique_ptr<llvm::Module> &slim::IR::getLLVMModule() {
    return this->llvm_module;
}

// Provides APIs similar to the older implementation of SLIM in order to support
// the implementations that are built using the older SLIM as a base
slim::LegacyIR::LegacyIR() {
    slim_ir = new slim::IR();
}

// Add the instructions of a basic block (of a given function) in the IR
void slim::LegacyIR::simplifyIR(llvm::Function *function,
                                llvm::BasicBlock *basic_block) {
    this->slim_ir->addFuncBasicBlockInstructions(function, basic_block);
}

// Get the repository (in the form of function-basicblock to instructions
// mappings) of all the SLIM instructions
std::map<std::pair<llvm::Function *, llvm::BasicBlock *>, std::list<long long>>
    &slim::LegacyIR::getfuncBBInsMap() {
    return this->slim_ir->getFuncBBToInstructions();
}

// Get the instruction id to SLIM instruction map
// std::unordered_map<long long, BaseInstruction *>
// &slim::LegacyIR::getGlobalInstrIndexList()
std::unordered_map<long long, BaseInstruction *> &
slim::LegacyIR::getGlobalInstrIndexList() // modSB
{
    return this->slim_ir->getIdToInstructionsMap();
}

// Returns the corresponding LLVM instruction for the instruction id
llvm::Instruction *slim::LegacyIR::getInstforIndx(long long index) {
    BaseInstruction *slim_instruction = this->slim_ir->getInstrFromIndex(index);

    return slim_instruction->getLLVMInstruction();
}

// Removes temp from pointer instruction, replacing it by the original referred
// variable // modSB
void slim::IR::removeTempFromPtr() {
    int countIndirection = 0;
    std::vector<long long> temp_instrid;

    // Keeps track whether the function details have been printed or not
    std::unordered_map<llvm::Function *, bool> func_visited;
    std::pair<llvm::Value *, int> tempRemRes;

    // Iterate over the function basic block map
    for (auto &entry : this->func_bb_to_inst_id) {
        llvm::Function *func = entry.first.first;
        llvm::BasicBlock *basic_block = entry.first.second;

        // Check if the function details are already printed and if not, print
        // the details and mark the function as visited
        if (func_visited.find(func) == func_visited.end()) {
            // if (func->getSubprogram())
            //     llvm::outs() << "[" << func->getSubprogram()->getFilename()
            //     << "] ";

            // Mark the function as visited
            func_visited[func] = true;
        }

        // ITERATE OVER INSTRUCTIONS IN A BASIC BLOCK OF A FUNCTION: This is
        // where our job should be done.

        for (long long instruction_id : entry.second) {
            BaseInstruction *instruction = inst_id_to_object[instruction_id];

            auto numOps =
                instruction
                    ->getNumOperands(); // get number of operands of instruction

            // 1. if LHS indirection is 1
            // 2. check RHS as below:
            // if(instruction->result.second == )
            // llvm::outs()<<"\n\n"; instruction->printInstruction();
            // llvm::outs()<<"\n";

            // Print Instruction
            cyan(1);
            // llvm::outs() << "\nOriginal instruction: ";
            instruction->printInstruction();
            // llvm::outs() << "with ops = " << numOps;
            normal();

            // no rhs : skip
            if (numOps < 1 || instruction->getOperand(0).first == 0 ||
                !(instruction->getOperand(0).first->hasName())) {
                yellowB(1);
                // llvm::outs() << "\nThis instruction has no RHS\n";
                normal();
                continue;
            } else {
                // check LHS
                // none proceed
                //  yes: check
                //  non-ptr
                //  else exit
                if (instruction->getLHS().first == 0 ||
                    !(instruction->getLHS()
                          .first->hasName())) // handle for non LHS/ use
                                              // statements [TO-DO]
                {
                    red(0);
                    // llvm::outs() << "\nThis instruction: ";
                    instruction->printInstruction();
                    // llvm::outs() << " has no LHS\n";
                    normal();
                    magentaB(1);
                    // llvm::outs() << "And has " << numOps << " operands \n";
                    normal();
                } else {
                    green(1);
                    // llvm::outs()
                    //     << "\nThis instruction has an LHS: "
                    //     << instruction->getLHS().first->getName() << " \n";
                    normal();
                    if (instruction->getLHS()
                            .first
                            ->isPointerInLLVM()) // 1. Check and allow ahead
                                                 // only if LHS is a scalar (and
                                                 // not a pointer)
                    {
                        // lhs is a ptr : skip
                        // llvm::outs() << "and this LHS is a ptr.\n\n";
                        continue;
                    } else {
                        for (auto positionRHS = 0; positionRHS < numOps;
                             positionRHS++) {
                            auto operand = instruction->getOperand(positionRHS);
                            // Skip if RHS operand is a user global/ user local
                            // / isEmpty(eg.return;
                            // print(str[0][0])==print("STRING"))
                            // if(!(operand.first->getValue()) ||
                            // !(operand.first->hasName()))
                            //     continue;
                            // if(llvm::isa<llvm::GlobalValue>(operand.first->getValue()))
                            //     continue;
                            // if(llvm::isa<llvm::AllocaInst>(operand.first->getValue()))
                            //     continue;

                            if (!(operand.first->getValue()) ||
                                !(operand.first->hasName())) {
                                // llvm::outs() << "\n\n This is a unnamed "
                                //                 "operand at poistion: "
                                //              << positionRHS << "\n\n";
                                continue;
                            }
                            if (llvm::isa<llvm::GlobalValue>(
                                    operand.first->getValue())) {
                                // llvm::outs()
                                //     << "\n\n"
                                //     << operand.first->getName()
                                //     << " This is a GLOBAL operand at
                                //     poistion: "
                                //     << positionRHS << "\n\n";
                                continue;
                            }
                            if (llvm::isa<llvm::AllocaInst>(
                                    operand.first->getValue())) {
                                // llvm::outs() << "\n\n"
                                //              << operand.first->getName()
                                //              << " This is a USER LOCAL
                                //              operand "
                                //                 "at poistion: "
                                //              << positionRHS << "\n\n";
                                continue;
                            }
                            if (operand.first->isFormalArgument()) {
                                // llvm::outs() << "\n\n"
                                //              << operand.first->getName()
                                //              << " This is a FORMAL ARGUMENT "
                                //                 "operand at poistion: "
                                //              << positionRHS << "\n\n";
                                continue;
                            }
                            // if(llvm.ssa.copy.)

                            yellowB(1);
                            // llvm::outs()
                            //     << "\n\n"
                            //     << operand.first->getName()
                            //     << " This is a TEMPORARY operand at poistion:
                            //     "
                            //     << positionRHS << "\n\n";
                            normal();
                            if (operand.first
                                    ->isPointerInLLVM()) // 2. check if RHS
                                                         // operand is a pointer
                            {
                                // llvm::outs() << "\n\nREM TEMP PTR:
                                // instruction "
                                //                 "passed to function is -->";
                                instruction->printInstruction();
                                // llvm::outs() << "\n\n";
                                tempRemRes = findOGptr(
                                    instruction->getLLVMInstruction(),
                                    countIndirection, temp_instrid,
                                    positionRHS); // 3. make a call to a
                                                  // recursive function, and get
                                                  // the user local/global
                                                  // replacement for temp, and
                                                  // resultant indirection/ptr
                                                  // level
                                if (tempRemRes.second == -1) {
                                    continue;
                                }
                                // modify the instruction
                                // llvm::outs() << "\nOriginal instruction: ";
                                instruction->printInstruction();
                                if (OperandRepository::getSLIMOperand(
                                        tempRemRes.first) != nullptr) {
                                    instruction->setOperand(
                                        positionRHS,
                                        std::make_pair(
                                            OperandRepository::getSLIMOperand(
                                                tempRemRes.first),
                                            tempRemRes.second));
                                    // llvm::outs() << "\nNOT a null";
                                } else {
                                    white(1);
                                    // llvm::outs() << "\nIS A NULLPTR!!";
                                    normal();
                                }
                                // llvm::outs()
                                //     << "\ntempres.first: "
                                //     << OperandRepository::getSLIMOperand(
                                //            tempRemRes.first)
                                //     << "\ntempres.second: " <<
                                //     tempRemRes.second
                                //     << "\n\n";
                                // llvm::outs()<<"\nModified instruction: ";
                                // instruction->printInstruction();
                                // llvm::outs()<<"\n-----x-----\n\n";
                            } else {
                                // llvm::outs() << operand.first->getName()
                                //  << " is a non-ptr RHS operand\n\n";
                            }
                        }
                    }
                }
            }
        }
    }
    llvm::outs() << "\n\n---------------------\n\n";
    printVector(temp_instrid);
    removeInstructions(temp_instrid);
}

// print vector //modSB
void slim::IR::printVector(std::vector<long long> vect) {
#ifdef DEBUG
    for (auto it : vect) {
        llvm::outs() << "\n Instruction ID: " << it << "\n";
    }
    llvm::outs() << "\n--------------\n\n";
#endif
}

// find original pointer var and return indirection level // requires use-def
// chains here //modSB
std::pair<llvm::Value *, int>
slim::IR::findOGptr(llvm::Instruction *instruction, int countIndirection,
                    std::vector<long long> &temp_instrid, int position) {

    countIndirection++;
    llvm::outs() << "\n\nFIND OG PTR: instruction passed in is -->"
                 << instruction->getName() << "\n\n";
    int indirectionLevel = 0;
    llvm::Value *opValue;
    int tempCount = -1;
    llvm::Value *lhsOp;
    llvm::outs() << "\n\n1749";
    for (llvm::Use &U : instruction->operands()) {
        tempCount++;
        llvm::outs() << "\n\n1752";

        if (tempCount != position)
            continue;

        llvm::outs() << "\n\n1756";
        llvm::Value *v = U.get();
        llvm::outs() << "\n\n1760";
        assert(instruction != nullptr);

        llvm::Instruction *defIns = llvm::dyn_cast<llvm::Instruction>(v);
        // To skip instruction if there's none. Happens when we find
        // instruction/ defniition for variable 1/2/3.. i.e. compile time
        // constants
        if (defIns == nullptr) {
            llvm::outs() << "\nIS NULL "
                            "DEF\n---------------------------------------------"
                            "---------------------------------\n";
            continue;
        }

        // get LHS op from LLVM Instruction
        if (llvm::isa<llvm::StoreInst>(defIns))
            lhsOp = defIns->getOperand(1);
        else
            lhsOp = (llvm::Value *)defIns;

        // if(v)
        for (auto opN = 0; opN < instruction->getNumOperands(); opN++) {
            if (instruction->getOperand(opN) == nullptr)
                llvm::outs() << "\noperand[" << opN << "] : is NULL\n\n";
            else
                llvm::outs()
                    << "\noperand[" << opN
                    << "] : " << instruction->getOperand(opN)
                    << " with name: " << instruction->getOperand(opN)->getName()
                    << " has name?: " << instruction->getOperand(opN)->hasName()
                    << " |\n\n";
        }
        // printLLVMInstructiontoSLIM();
        if (LLVMInstructiontoSLIM[instruction] != nullptr) {
            llvm::outs() << "\n"
                         << instruction->getName() << " "
                         << LLVMInstructiontoSLIM[instruction];
            llvm::outs() << "\n\n";
            assert(LLVMInstructiontoSLIM.find(instruction) !=
                   LLVMInstructiontoSLIM.end());
            assert(LLVMInstructiontoSLIM[instruction] != nullptr);
            llvm::outs()
                << "\n["
                << LLVMInstructiontoSLIM[instruction]->getInstructionId()
                << "] Name of USE/Original instruction:";
            convertLLVMToSLIMInstruction(*instruction)->printInstruction();
            llvm::outs() << "\n";

            llvm::outs() << "numOP in USE/Original instruction: "
                         << instruction->getNumOperands() << "\n\n";
        } else {
            llvm::outs() << "\n\nInstruction not present in SLIM.\nLHS: ";
        }
        llvm::outs() << "\n1782";

        // OTHERS: result_operand = (llvm::Value *) defIns
        // STORE: llvm::isa<llvm::StoreInst>(defIns) : getOperand(1)

        assert(defIns != nullptr);
        llvm::outs() << "\n1789";
        // seg faults here
        assert(lhsOp != nullptr);
        llvm::outs() << "\n1792";

        assert(lhsOp != 0);
        // llvm::outs()<<"\n Name of DEF instruction:";
        // LLVMInstructiontoSLIM[defIns]->printInstruction();
        llvm::outs() << "\n Name of DEF instruction LHS: " << lhsOp->getName();
        llvm::outs() << "\n Name of DEF instruction variable: [tempCount: "
                     << tempCount << "] " << v->getName() << " $ " << v << "\n";
        llvm::outs() << "numOP in DEF instruction: " << defIns->getNumOperands()
                     << "\n";

        // if(!(OperandRepository::getSLIMOperand(v)->getValue()) ||
        // !(OperandRepository::getSLIMOperand(v)->hasName()))
        // {
        //     llvm::outs()<<"\n\n This is a unnamed operand \n\n";
        //     continue;
        // }
        // if(llvm::isa<llvm::GlobalValue>(OperandRepository::getSLIMOperand(v)->getValue()))
        // {
        //     llvm::outs()<<"\n\n"<<OperandRepository::getSLIMOperand(v)->getName()<<"
        //     This is a GLOBAL operand \n\n"; continue;
        // }
        // if(llvm::isa<llvm::AllocaInst>(OperandRepository::getSLIMOperand(v)->getValue()))
        // {
        //     llvm::outs()<<"\n\n"<<OperandRepository::getSLIMOperand(v)->getName()<<"
        //     This is a USER LOCAL operand \n\n"; continue;
        // }
        // if(OperandRepository::getSLIMOperand(v)->isFormalArgument())
        // {
        //     llvm::outs()<<"\n\n"<<OperandRepository::getSLIMOperand(v)->getName()<<"
        //     This is a USER LOCAL operand \n\n"; continue;
        // }

        // llvm::outs()<<"\n here now 1\n\n";
        // assert(defIns != nullptr);

        // llvm::outs()<<"\n Name of USE instruction:";
        // processLLVMInstruction(*instruction)->printInstruction();
        // llvm::outs()<<"\n\n"; llvm::outs()<<"\n Name of DEF instruction:";
        // processLLVMInstruction(*defIns)->printInstruction();
        // llvm::outs()<<"\n";

        // assert(defIns != nullptr);

        assert(defIns->getNumOperands() > 0);
        llvm::outs() << "\n here now 1\n\n";
        // if(llvm::isa<llvm::StoreInst>(defIns))
        opValue = defIns->getOperand(0);
        llvm::outs() << "\n here now 2: " << lhsOp->getName() << "\n\n";
        // to debug
        // llvm::outs()<<"\n here now 2\n\n";
        // auto numOp = defIns->getNumOperands();
        // for(auto ops = 0; ops<numOp; ops++)
        llvm::outs() << "\ndef operand: " << opValue->getName()
                     << "  has a name?: " << opValue->hasName()
                     << "\nisStore?: " << llvm::isa<llvm::StoreInst>(defIns)
                     << "\n";

        if (llvm::isa<llvm::CallInst>(defIns) ||
            ((!(llvm::isa<llvm::StoreInst>(defIns))) &&
             defIns->getNumOperands() > 1)) {
            opValue = lhsOp;
            llvm::outs() << "\nDid Not push instr: ";
            if (LLVMInstructiontoSLIM[defIns] != nullptr) {
                assert(LLVMInstructiontoSLIM[defIns] != nullptr);
                convertLLVMToSLIMInstruction(*defIns)->printInstruction();
                llvm::outs()
                    << " at ID: "
                    << LLVMInstructiontoSLIM[defIns]->getInstructionId();
                llvm::outs()
                    << "\nIS a CALL Giving variable " << opValue->getName()
                    << " to modify your instruction\n\n";
            }
            return std::make_pair(opValue, countIndirection + 1);
        }
        llvm::outs() << "\n here now 3\n\n";
        // processLLVMInstruction(*defIns); //->getOperand(0);
        llvm::outs() << "\n here now 3.1\n\n";
        // if(LLVMInstructiontoSLIM[defIns]->getNumOperands() < 1)
        //     continue;
        // Check if the def ins in use-def chain is outside of the BB
        llvm::outs() << "\ncondition 1: "
                     << llvm::isa<llvm::AllocaInst>(opValue);
        llvm::outs() << "\ncondition 2: "
                     << llvm::isa<llvm::GlobalValue>(opValue);
        llvm::outs() << "\nNum of ops: " << defIns->getNumOperands();
        // llvm::outs()<<"\nInstruction: ";
        // LLVMInstructiontoSLIM[defIns]->printInstruction();
        llvm::outs()
            << "\ncondition 3.0: "
            << convertLLVMToSLIMInstruction(*defIns)->getOperand(0).first;
        llvm::outs() << "\ncondition 3.1: "
                     << convertLLVMToSLIMInstruction(*defIns)
                            ->getOperand(0)
                            .first->isFormalArgument();

        if (!(llvm::isa<llvm::AllocaInst>(opValue)) &&
            !(llvm::isa<llvm::GlobalValue>(opValue)) &&
            !(convertLLVMToSLIMInstruction(*defIns)
                  ->getOperand(0)
                  .first->isFormalArgument())) {
            llvm::outs() << "\n here now 4.1\n\n";
            llvm::outs() << "\nThis temp: " << opValue->getName()
                         << " is a temporary made by LLVM\n\n";
            if (LLVMInstructiontoSLIM[defIns] != nullptr) {
                temp_instrid.push_back(
                    LLVMInstructiontoSLIM[defIns]->getInstructionId());
                llvm::outs() << "\npushed instr: ";
                convertLLVMToSLIMInstruction(*defIns)->printInstruction();
                llvm::outs()
                    << " at ID: "
                    << LLVMInstructiontoSLIM[defIns]->getInstructionId();
            }
            llvm::outs()
                << "\nneed not push instruction. Instruction is already absent "
                   "in SLIM due to DiscardForSSA option";

            llvm::outs() << "\n------------------------------------------------"
                            "------------------\n";
            return findOGptr(defIns, countIndirection, temp_instrid, 0);
        } else {
            llvm::outs() << "\n here now 4.2\n\n";
            if (llvm::isa<llvm::AllocaInst>(opValue))
                llvm::outs() << "\nThis is an Alloca: [" << opValue << "]  "
                             << opValue->getName();
            else if (llvm::isa<llvm::GlobalValue>(opValue))
                llvm::outs() << "\nThis is a Global: [" << opValue << "]  "
                             << opValue->getName();
            else if (convertLLVMToSLIMInstruction(*defIns)
                         ->getOperand(0)
                         .first->isFormalArgument())
                llvm::outs() << "\nThis is a Formal Argument: [" << opValue
                             << "]  " << opValue->getName();
            else {
                red(1);
                llvm::outs() << "\nWrongly put here..[" << opValue << "]  "
                             << opValue->getName() << "\n\n";
                normal();
            }
            // to debug
            llvm::outs() << "\nThis notTemp: " << opValue->getName()
                         << " is NOT A temporary made by LLVM\n\n";
            // llvm::outs()<<"\ntempRemRes: "<<opValue->getName()<<"\nValue:
            // "<<opValue<<"\n\n\n";

            if (llvm::isa<llvm::CallInst>(defIns) ||
                ((!(llvm::isa<llvm::StoreInst>(defIns))) &&
                 defIns->getNumOperands() > 1)) {
                opValue = lhsOp;
                normal();
                llvm::outs() << "\nDid Not push instruction: ";
                if (LLVMInstructiontoSLIM[defIns] != nullptr) {
                    assert(LLVMInstructiontoSLIM[defIns] != nullptr);
                    convertLLVMToSLIMInstruction(*defIns)->printInstruction();
                    llvm::outs()
                        << " at ID: "
                        << LLVMInstructiontoSLIM[defIns]->getInstructionId();
                    llvm::outs()
                        << "\nIS a CALL Giving variable " << opValue->getName()
                        << " to modify your instruction\n\n";
                }
            } else {
                llvm::outs() << "\n\nIn the else part rn\n\n";
                if (LLVMInstructiontoSLIM[defIns] != nullptr) {
                    assert(LLVMInstructiontoSLIM[defIns] != nullptr);
                    temp_instrid.push_back(
                        LLVMInstructiontoSLIM[defIns]->getInstructionId());
                    llvm::outs() << "\nNOt a CALL pushed instr: ";
                    // processLLVMInstruction(*defIns)->printInstruction();
                    llvm::outs()
                        << " at ID: "
                        << LLVMInstructiontoSLIM[defIns]->getInstructionId();
                }
                llvm::outs()
                    << "\nneed not push instruction. Instruction is already "
                       "absent in SLIM due to DiscardForSSA option";
            }
            return std::make_pair(opValue, countIndirection + 1);
        }
        llvm::outs() << "\n here now 5\n\n";
    }

    // to debug
    // findOGptr()
    // return std::make_pair(opValue, countIndirection);
    return (std::make_pair((llvm::Value *)(-1), -1));
}

// map llvm instructions to slim instructions //modSB
void slim::IR::mapLLVMInstructiontoSLIM() {
    // Keeps track whether the function details have been printed or not
    std::unordered_map<llvm::Function *, bool> func_visited;

    // Iterate over the function basic block map
    for (auto &entry : this->func_bb_to_inst_id) {
        llvm::Function *func = entry.first.first;
        llvm::BasicBlock *basic_block = entry.first.second;

        // Check if the function details are already printed and if not, print
        // the details and mark the function as visited
        if (func_visited.find(func) == func_visited.end()) {
            func_visited[func] = true;
        }

        for (long long instruction_id : entry.second) {
            BaseInstruction *instruction = inst_id_to_object[instruction_id];
            auto LLVMInstruction = instruction->getLLVMInstruction();
            LLVMInstructiontoSLIM[LLVMInstruction] = instruction;
        }
    }
}

// print LLVMInstructiontoSLIM map to debug //modSB
void slim::IR::printLLVMInstructiontoSLIM() {
#ifdef DEBUG
    auto s = 0;
    for (auto it : LLVMInstructiontoSLIM) {
        s++;
        llvm::outs() << "\n" << s << ") LLVM: [" << it.first << "] ";
        if (it.first != nullptr)
            llvm::outs() << it.first->getName();
        else
            llvm::outs() << "NULL";
        llvm::outs() << "\nSLIM: [" << it.second << "] ";
        if (it.first != nullptr)
            it.second->printInstruction();
        else
            llvm::outs() << "NULL";
        llvm::outs() << "\n";
    }
#endif
}

// remove instructions from OG data structure //modSB
void slim::IR::removeInstructions(std::vector<long long> temp_instrid) {

    for (auto it : temp_instrid) {
        // 1. id->instruction: inst_bb_obj...
        BaseInstruction *instruction = inst_id_to_object[it];

        // 2. instruction->Func and BB;
        auto function = instruction->getFunction();
        auto basicblock = instruction->getBasicBlock();

        // 3. use func&BB in func_bb_... to get list and
        auto listInstr =
            func_bb_to_inst_id[std::make_pair(function, basicblock)];

        for (auto itIns : listInstr) {
            if (itIns == it) {
                // 4. remove instruction from list
                func_bb_to_inst_id[std::make_pair(function, basicblock)].remove(
                    it);
                break;
            }
        }
    }
}


llvm::BasicBlock * slim::IR::getLastBasicBlock(llvm::Function * function){
    if (!function) return nullptr;
    if (return_bb_map.find(function) != return_bb_map.end()){
        return return_bb_map[function];
    }
    
    llvm::BasicBlock *lastBB = nullptr;
    for (auto &BB : *function) {

        Instruction *terminator = BB.getTerminator();
        
        // Check for return instructions
        if (isa<ReturnInst>(terminator)) {
            lastBB = &BB;
        }
    }

    return_bb_map[function] = lastBB;
    return lastBB;
}

bool isExitBlock(BasicBlock *BB) {
    Instruction *terminator = BB->getTerminator();
    if (!terminator) return false;
    
    // Check for function calls that don't return (like exit(), abort())
    for (auto &I : *BB) {
        if (auto *CI = dyn_cast<CallInst>(&I)) {
            if (Function *calledFunc = CI->getCalledFunction()) {
                StringRef funcName = calledFunc->getName();
                if (funcName == "exit" || funcName == "abort" || 
                    funcName == "_Exit" || funcName == "quick_exit") {
                    return true;
                }
                // Check for functions marked as noreturn
                if (calledFunc->hasFnAttribute(Attribute::NoReturn)) {
                    return true;
                }
            }
        }
    }
    
    return false;
}



std::set<llvm::BasicBlock*>* slim::IR::getExitBBList(llvm::Function* function){
    if (!function) return nullptr;
    if (func_to_exit_bb_map.find(function) != func_to_exit_bb_map.end()){
        return func_to_exit_bb_map[function];
    }

    std::set<llvm::BasicBlock*> * exit_bb_set = new std::set<llvm::BasicBlock*>();

    for (auto &BB : *function) {
        if (isExitBlock(&BB)) {
            exit_bb_set->insert(&BB);
        }
    }

    func_to_exit_bb_map[function] = exit_bb_set;
    return exit_bb_set;
}

std::set<llvm::BasicBlock*>* slim::IR::getTerminatingBBList(llvm::Function* function){
    if (!function) return nullptr;
    if (func_to_term_bb_map.find(function) != func_to_term_bb_map.end()){
        return func_to_term_bb_map[function]; 
    }

    std::set<llvm::BasicBlock*> * term_bb_set = new std::set<llvm::BasicBlock*>();

    for (auto &BB: *function){
        bool hasSuccessor = false;
        for (auto BB_succ: successors(&BB)){
            hasSuccessor = true;
            break;
        }

        if (!hasSuccessor){
            term_bb_set->insert(&BB);
        }
    }

    func_to_term_bb_map[function] = term_bb_set;
    return term_bb_set;

}   



// get instruction operands printed //modSB
void slim::IR::getInstructionDetails() {
#ifdef DEBUG
    llvm::outs() << "\nGET INSTRUCTION "
                    "DETAILS\n------------------------------------------\n";
    std::unordered_map<llvm::Function *, bool> func_visited;

    // Iterate over the function basic block map
    for (auto &entry : this->func_bb_to_inst_id) {
        llvm::Function *func = entry.first.first;
        llvm::BasicBlock *basic_block = entry.first.second;

        // Check if the function details are already printed and if not, print
        // the details and mark the function as visited
        if (func_visited.find(func) == func_visited.end()) {
            // Mark the function as visited
            func_visited[func] = true;
        }

        // ITERATE OVER INSTRUCTIONS IN A BASIC BLOCK OF A FUNCTION: This is
        // where our job should be done.

        for (long long instruction_id : entry.second) {
            BaseInstruction *instruction = inst_id_to_object[instruction_id];

            auto numOps =
                instruction
                    ->getNumOperands(); // get number of operands of instruction

            // no rhs : skip
            magentaB(1);
            llvm::outs() << "\n\nInstruction: ";
            instruction->printInstruction();
            normal();
            if (instruction->getInstructionType() == InstructionType::CALL) {
                llvm::outs() << "\nCALL INSTRUCTION";
                CallInstruction *call_instruction =
                    (CallInstruction *)instruction;

                if (call_instruction->isIndirectCall() == false) {
                    llvm::outs() << "\n IS A DIRECT CALL and DECLARATION";
                    if (call_instruction->getCalleeFunction()->isDeclaration())
                        llvm::outs() << "\nis Declaration ";
                    else
                        llvm::outs() << "\nis NOT Declaration ";

                    if (call_instruction->getCalleeFunction()->getName() ==
                        "__isoc99_sscanf") {
                        llvm::outs() << "to SSCANF\n";
                    } else if (call_instruction->getCalleeFunction()
                                   ->getName() == "__isoc99_fscanf") {
                        llvm::outs() << "to FSCANF\n";
                    } else if (call_instruction->getCalleeFunction()
                                   ->getName() == "__isoc99_scanf") {
                        llvm::outs() << "to SCANF\n";
                    } else {
                        llvm::outs() << "to NONE\n";
                    }
                }
            }
            if (numOps < 2) // || instruction->getOperand(0).first == 0 ||
                            // !(instruction->getOperand(0).first->hasName()))
            {
                yellowB(1);
                llvm::outs() << "\nThis instruction has no RHS\n";
                normal();
                if (instruction->getLHS().first != nullptr)
                    llvm::outs() << "\nBut LHS = "
                                 << instruction->getLHS().first->getName();
                else
                    llvm::outs() << "\nBut no LHS\n";

                llvm::outs()
                    << "\nNumber of RHS ops: " << instruction->getRHS().size();
                if (instruction->getRHS().size() > 1) {
                    for (auto ops : instruction->getRHS()) {
                        llvm::outs()
                            << "\nrhs operand: " << ops.first->getName();
                    }
                }
                // if (instruction->isCallNode())
                //     llvm::outs()<<"\nis input\n";
                // continue;
            } else {
                for (auto positionRHS = 0; positionRHS < numOps;
                     positionRHS++) {
                    auto operand = instruction->getOperand(positionRHS);

                    if (!(operand.first->getValue()) ||
                        !(operand.first->hasName())) {
                        llvm::outs()
                            << "\n\n This is a unnamed operand at position: "
                            << positionRHS << "\n\n";
                        continue;
                    } else {
                        llvm::outs()
                            << "\nOperand Name: " << operand.first->getName()
                            << " at position: " << positionRHS << "\n\n";
                    }

                    if (llvm::isa<llvm::GlobalValue>(
                            operand.first->getValue())) {
                        llvm::outs()
                            << "\n\n"
                            << operand.first->getName()
                            << " This is a GLOBAL operand at position: "
                            << positionRHS << "\n\n";
                        continue;
                    }
                    if (llvm::isa<llvm::AllocaInst>(
                            operand.first->getValue())) {
                        llvm::outs()
                            << "\n\n"
                            << operand.first->getName()
                            << " This is a USER LOCAL operand at position: "
                            << positionRHS << "\n\n";
                        continue;
                    }
                    if (operand.first->isFormalArgument()) {
                        llvm::outs() << "\n\n"
                                     << operand.first->getName()
                                     << " This is a FORMAL ARGUMENT operand at "
                                        "position: "
                                     << positionRHS << "\n\n";
                        continue;
                    }
                }
            }
        }
    }

    llvm::outs()
        << "\n------------------------------------------\nDONE INSTRUCTION "
           "DETAILS\n------------------------------------------\n";
#endif
}

// COLORS:
// https://stackoverflow.com/questions/4053837/colorizing-text-in-the-console-with-c

// to change color of text to cyan
void slim::cyan(int bold) {
    llvm::outs() << "\033[" << bold << ";36m";
}

// to change color of text to blue
void slim::blue(int bold) {
    llvm::outs() << "\033[" << bold << ";34m";
}

// to change color of text to green
void slim::green(int bold) {
    llvm::outs() << "\033[" << bold << ";32m";
}

// to change color of text to red
void slim::red(int bold) {
    llvm::outs() << "\033[" << bold << ";31m";
}

// to change color of text to red bright
void slim::redB(int bold) {
    llvm::outs() << "\033[" << bold << ";91m";
}

// to change color of text to magenta Bright
void slim::magentaB(int bold) {
    llvm::outs() << "\033[" << bold << ";95m";
}

// to change color of text to yellow
void slim::yellow(int bold) {
    llvm::outs() << "\033[" << bold << ";33m";
}

// to change color of text to bright yellow
void slim::yellowB(int bold) {
    llvm::outs() << "\033[" << bold << ";93m";
}

// to change color of text to white
void slim::white(int bold) {
    llvm::outs() << "\033[" << bold << ";37m";
}

// to change color of text back to normal
void slim::normal() {
    llvm::outs() << "\033[0m";
}
