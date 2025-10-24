#include "Instructions.h"
#include "SLIMDebug.h"

// Base instruction
BaseInstruction::BaseInstruction(llvm::Instruction *instruction) {
  this->instruction = instruction;
  this->instruction_type = NOT_ASSIGNED;
  this->has_pointer_variables = false;
  this->has_source_line_number = false;
  this->basic_block = instruction->getParent();
  this->function = this->basic_block->getParent();
  this->is_constant_assignment = false;
  this->is_expression_assignment = false;
  this->is_input_statement = false;
  this->is_ignored = false;
  this->input_statement_type = NOT_APPLICABLE;
  this->isDynamicAllocation = false;

  // Set the source line number
  if (instruction->getDebugLoc()) {
    this->source_line_number = instruction->getDebugLoc().getLine();
    has_source_line_number = true;
  }
}

void BaseInstruction::mapAllOperandsToSLIM(IndirectionLevel level) {
  mapResultOperandToSLIM(level);
  mapOperandsToSLIMOperands(level);
}

void BaseInstruction::mapResultOperandToSLIM(IndirectionLevel level) {
  llvm::Value *result_operand = (llvm::Value *)instruction;

  SLIMOperand *result_slim_operand =
      OperandRepository::getOrCreateSLIMOperand(result_operand);

  this->result = std::make_pair(result_slim_operand, level);

  OperandRepository::setSLIMOperand(result_operand, result_slim_operand);
}

void BaseInstruction::mapOperandsToSLIMOperands(IndirectionLevel level) {
  for (int i = 0; i < instruction->getNumOperands(); i++) {
    llvm::Value *operand_i = instruction->getOperand(i);

    SLIMOperand *slim_operand_i =
        OperandRepository::getOrCreateSLIMOperand(operand_i);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->operands.push_back(std::make_pair(slim_operand_i, level));
  }
}

void pretty_printer_k1(SLIMOperandIndirectionPair val) {
//----
  llvm::outs() << "<";
  val.first->printOperand(llvm::outs());
  llvm::outs() << ", " << val.second << ">";
  return;
//-- 
  if (val.second == 0)
    llvm::outs() << "&";
  else if (val.second == 2)
    llvm::outs() << "*";

  val.first->printOperand(llvm::outs());
}

void BaseInstruction::setInstructionId(long long id) {
  // The id should be greater than or equal to 0
  assert(id >= 0);

  this->instruction_id = id;
}

long long BaseInstruction::getInstructionId() {
  // The instruction id should be a non-negative integer
  assert(this->instruction_id >= 0);

  return this->instruction_id;
}

bool BaseInstruction::getIsDynamicAllocation() {
  return this->isDynamicAllocation;
}

// Returns the instruction type
InstructionType BaseInstruction::getInstructionType() {
  // Make sure that the instruction has been assigned a type during its
  // construction
  assert(this->instruction_type != InstructionType::NOT_ASSIGNED);

  return this->instruction_type;
}

// Returns the corresponding LLVM instruction
llvm::Instruction *BaseInstruction::getLLVMInstruction() {
  return this->instruction;
}

// Returns the function to which this instruction belongs
llvm::Function *BaseInstruction::getFunction() {
  // The function should not be NULL
  assert(this->function != nullptr);

  return this->function;
}

// Returns the basic block to which this instruction belongs
llvm::BasicBlock *BaseInstruction::getBasicBlock() {
  // The basic block should not be NULL
  assert(this->basic_block != nullptr);

  return this->basic_block;
}

// Returns true if the instruction is an input statement
bool BaseInstruction::isInputStatement() { return this->is_input_statement; }

// Returns the input statement type
InputStatementType BaseInstruction::getInputStatementType() {
  return this->input_statement_type;
}

// Returns the starting index of the input arguments (applicable only to valid
// input statements)
unsigned BaseInstruction::getStartingInputArgsIndex() {
  assert(this->input_statement_type != InputStatementType::NOT_APPLICABLE);

  return this->starting_input_args_index;
}

// Returns true if the instruction is a constant assignment
bool BaseInstruction::isConstantAssignment() {
  return this->is_constant_assignment;
}

// Returns true if the instruction is an expression assignment;
bool BaseInstruction::isExpressionAssignment() {
  return this->is_expression_assignment;
}

// Checks whether the instruction has any relation to a statement in the source
// program or not
bool BaseInstruction::hasSourceLineNumber() {
  return this->has_source_line_number;
}

// Returns the source program line number corresponding to this instruction
unsigned BaseInstruction::getSourceLineNumber() {
  // Check whether the instruction corresponds to a statement in the source
  // program or not
  assert(has_source_line_number);

  return this->source_line_number;
}

// sets the source program line number corresponding to this instruction //modSB
void BaseInstruction::setSourceLineNumber(unsigned srcLineNumber) {
  this->source_line_number = srcLineNumber;
}

// Returns the source file name corresponding to this instruction (to be used
// only for print purposes)
std::string BaseInstruction::getSourceFileName() {
  // Check whether the instruction corresponds to a statement in the source
  // program or not
  assert(has_source_line_number);

  return this->instruction->getDebugLoc().get()->getFilename().str();
}

// Returns the absolute source file name corresponding to this instruction (to
// be used only for print purposes)
std::string BaseInstruction::getAbsSourceFileName() {
  // Check whether the instruction corresponds to a statement in the source
  // program or not
  assert(has_source_line_number);
  std::string Filename =
      this->instruction->getDebugLoc().get()->getFilename().str();
  // llvm::SmallString<128> FilenameVec = llvm::StringRef(Filename);
  // llvm::sys::fs::make_absolute(FilenameVec);
  llvm::SmallString<128> AbsolutePath;
  llvm::sys::fs::real_path(Filename, AbsolutePath);

  return (std::string)AbsolutePath;
}

// Sets the ignore flag
void BaseInstruction::setIgnore() { this->is_ignored = true; }

// Returns true if the instruction is to be ignored (during analysis)
bool BaseInstruction::isIgnored() { return this->is_ignored; }

// Returns whether the instruction has pointer variables or not
bool BaseInstruction::hasPointerVariables() {
  return this->has_pointer_variables;
}

// Returns the result operand
SLIMOperandIndirectionPair BaseInstruction::getResultOperand() {
  return this->result;
}

// Sets the result operand
void BaseInstruction::setResultOperand(SLIMOperandIndirectionPair new_operand) {
  this->result = new_operand;
}

// Returns the number of operands
unsigned BaseInstruction::getNumOperands() { return this->operands.size(); }

// Sets the operand at the given index
void BaseInstruction::setOperand(unsigned index,
                                 SLIMOperandIndirectionPair new_operand) {
  // Check if the index is less than the total number of RHS operands of this
  // instruction
  assert(index < this->getNumOperands());

  this->operands[index] = new_operand;
}

// Sets the indirection level of RHS operand at the given index
void BaseInstruction::setRHSIndirection(unsigned index,
                                        unsigned new_indirection) {
  // Check if the index is less than the total number of RHS operands of this
  // instruction
  assert(index < this->getNumOperands());

  // Update the indirection
  this->operands[index].second = new_indirection;
}

// Create and return a variant of this instruction
/*BaseInstruction * BaseInstruction::createVariant(SLIMOperandIndirectionPair
result, std::vector<SLIMOperandIndirectionPair> operands)
{
    BaseInstruction *base_instruction;

    if (llvm::isa<llvm::AllocaInst>(instruction))
    {
        base_instruction = new AllocaInstruction(&instruction);
    }
    else if (llvm::isa<llvm::LoadInst>(instruction))
    {
        base_instruction = new LoadInstruction(&instruction);
    }
    else if (llvm::isa<llvm::StoreInst>(instruction))
    {
        base_instruction = new StoreInstruction(&instruction);
    }
    else if (llvm::isa<llvm::FenceInst>(instruction))
    {
        base_instruction = new FenceInstruction(&instruction);
    }
    else if (llvm::isa<llvm::AtomicCmpXchgInst>(instruction))
    {
        base_instruction = new AtomicCompareChangeInstruction(&instruction);
    }
    else if (llvm::isa<llvm::AtomicRMWInst>(instruction))
    {
        base_instruction = new AtomicModifyMemInstruction(&instruction);
    }
    else if (llvm::isa<llvm::GetElementPtrInst>(instruction))
    {
        base_instruction = new GetElementPtrInstruction(&instruction);
    }
    else if (llvm::isa<llvm::UnaryOperator>(instruction))
    {
        base_instruction = new FPNegationInstruction(&instruction);
    }
    else if (llvm::isa<llvm::BinaryOperator>(instruction))
    {
        base_instruction = new BinaryOperation(&instruction);
    }
    else if (llvm::isa<llvm::ExtractElementInst>(instruction))
    {
        base_instruction = new ExtractElementInstruction(&instruction);
    }
    else if (llvm::isa<llvm::InsertElementInst>(instruction))
    {
        base_instruction = new InsertElementInstruction(&instruction);
    }
    else if (llvm::isa<llvm::ShuffleVectorInst>(instruction))
    {
        base_instruction = new ShuffleVectorInstruction(&instruction);
    }
    else if (llvm::isa<llvm::ExtractValueInst>(instruction))
    {
        base_instruction = new ExtractValueInstruction(&instruction);
    }
    else if (llvm::isa<llvm::InsertValueInst>(instruction))
    {
        base_instruction = new InsertValueInstruction(&instruction);
    }
    else if (llvm::isa<llvm::TruncInst>(instruction))
    {
        base_instruction = new TruncInstruction(&instruction);
    }
    else if (llvm::isa<llvm::ZExtInst>(instruction))
    {
        base_instruction = new ZextInstruction(&instruction);
    }
    else if (llvm::isa<llvm::SExtInst>(instruction))
    {
        base_instruction = new SextInstruction(&instruction);
    }
    else if (llvm::isa<llvm::FPTruncInst>(instruction))
    {
        base_instruction = new TruncInstruction(&instruction);
    }
    else if (llvm::isa<llvm::FPExtInst>(instruction))
    {
        base_instruction = new FPExtInstruction(&instruction);
    }
    else if (llvm::isa<llvm::FPToUIInst>(instruction))
    {
        base_instruction = new FPToIntInstruction(&instruction);
    }
    else if (llvm::isa<llvm::FPToSIInst>(instruction))
    {
        base_instruction = new FPToIntInstruction(&instruction);
    }
    else if (llvm::isa<llvm::UIToFPInst>(instruction))
    {
        base_instruction = new IntToFPInstruction(&instruction);
    }
    else if (llvm::isa<llvm::SIToFPInst>(instruction))
    {
        base_instruction = new IntToFPInstruction(&instruction);
    }
    else if (llvm::isa<llvm::PtrToIntInst>(instruction))
    {
        base_instruction = new PtrToIntInstruction(&instruction);
    }
    else if (llvm::isa<llvm::IntToPtrInst>(instruction))
    {
        base_instruction = new IntToPtrInstruction(&instruction);
    }
    else if (llvm::isa<llvm::BitCastInst>(instruction))
    {
        base_instruction = new BitcastInstruction(&instruction);
    }
    else if (llvm::isa<llvm::AddrSpaceCastInst>(instruction))
    {
        base_instruction = new AddrSpaceInstruction(&instruction);
    }
    else if (llvm::isa<llvm::ICmpInst>(instruction))
    {
        base_instruction = new CompareInstruction(&instruction);
    }
    else if (llvm::isa<llvm::FCmpInst>(instruction))
    {
        base_instruction = new CompareInstruction(&instruction);
    }
    else if (llvm::isa<llvm::PHINode>(instruction))
    {
        base_instruction = new PhiInstruction(&instruction);
    }
    else if (llvm::isa<llvm::SelectInst>(instruction))
    {
        base_instruction = new SelectInstruction(&instruction);
    }
    else if (llvm::isa<llvm::FreezeInst>(instruction))
    {
        base_instruction = new FreezeInstruction(&instruction);
    }
    else if (llvm::isa<llvm::CallInst>(instruction))
    {
        base_instruction = new CallInstruction(&instruction);
    }
    else if (llvm::isa<llvm::VAArgInst>(instruction))
    {
        base_instruction = new VarArgInstruction(&instruction);
    }
    else if (llvm::isa<llvm::LandingPadInst>(instruction))
    {
        base_instruction = new LandingpadInstruction(&instruction);
    }
    else if (llvm::isa<llvm::CatchPadInst>(instruction))
    {
        base_instruction = new CatchpadInstruction(&instruction);
    }
    else if (llvm::isa<llvm::CleanupPadInst>(instruction))
    {
        base_instruction = new CleanuppadInstruction(&instruction);
    }
    else if (llvm::isa<llvm::ReturnInst>(instruction))
    {
        base_instruction = new ReturnInstruction(&instruction);
    }
    else
    {
        llvm_unreachable("[Error] Variant cannot be created for this
instruction!");
    }
}
*/

// createNewInstruction //modSB
// BaseInstruction* BaseInstruction::createNewInputInstruction(BaseInstruction
// *instruct, SLIMOperandIndirectionPair lhsop)
// {
//     // BaseInstruction *base_instruction;
//     // base_instruction = new CallInstruction(instruction);
//     BaseInstruction *newinstruction = instruct;
//     newinstruction->setResultOperand(lhsop);
//     llvm::outs()<<"\n\nip instruction: "<<lhsop.first->getName();

//     create new rhs operand
//     for(int positionOps = 1; positionOps < newinstruction->getRHS().size();
//     positionOps++)
//     {
//         newinstruction->getOperand(positionOps).first->getValue()->setName(".input");
//     }
//     SLIMOperand *SLIMOperand_new =
//     OperandRepository::getOrCreateSLIMOperand(lhsop);
//     SLIMOperand_new->getValue()->setName(".input");

//     newinstruction->setSourceLineNumber(instruct->getSourceLineNumber());

//     return newinstruction;
// }

// Courtesy: chatGPT
BaseInstruction *
BaseInstruction::createNewInputInstruction(BaseInstruction *instruct,
                                           SLIMOperandIndirectionPair lhsop) {
  // Create a new instance of BaseInstruction using the copy constructor
  BaseInstruction *newinstruction = new CallInstruction(instruction);

  SLIMOperand *SLIMOperand_new =
      OperandRepository::getOrCreateSLIMOperand(lhsop.first->getValue());

  // Modify the result operand
  newinstruction->setResultOperand(lhsop);

  // llvm::outs() << "\n\nip instruction: " << lhsop.first->getName();

  // Create new RHS operands
  for (int positionOps = 1; positionOps < newinstruction->getRHS().size();
       positionOps++) {
    // SLIMOperand *originalOperand =
    // newinstruction->getOperand(positionOps).first;

    // Create a new SLIMOperand with the same type as the original
    // SLIMOperand *newOperand = new
    // SLIMOperand(originalOperand->getValue());

    // Set the name of the new operand
    // newOperand->getValue()->setName(".input");

    // Update the instruction's operand with the new one
    newinstruction->setOperand(positionOps, newinstruction->getOperand(0));
  }

  // Create a new SLIMOperand for the result
  // SLIMOperand *SLIMOperand_new = new SLIMOperand(lhsop.first->getValue());
  // SLIMOperand_new->getValue()->setName(lhsop.first->getName());

  // Set the source line number
  if (instruct->has_source_line_number)
    newinstruction->setSourceLineNumber(instruct->getSourceLineNumber());

  return newinstruction;
}

// Returns the operand at a particular index
SLIMOperandIndirectionPair BaseInstruction::getOperand(unsigned index) {
  // The index should not be out-of-bounds
  assert(index >= 0 && index < this->getNumOperands());

  return this->operands[index];
}

// Prints the corresponding LLVM instruction
void BaseInstruction::printLLVMInstruction() {
#ifdef DEBUG
  this->instruction->print(llvm::outs());

  llvm::outs() << "\n";
#endif
}

// Prints the corresponding LLVM instruction to DOT File
void BaseInstruction::printLLVMInstructionDot(llvm::raw_ostream *dotFile) {
#ifdef DEBUG
  this->instruction->print(*dotFile);

  llvm::outs() << "\n";
#endif
}

void BaseInstruction::insertVariantInfo(unsigned result_ssa_version,
                                        llvm::Value *variable,
                                        unsigned variable_version) {
  // llvm::outs() << "result_ssa_version_insert = " << result_ssa_version
  //              << "\n";

  this->variants[result_ssa_version][variable] = variable_version;
  // llvm::outs() << "Inserted variant for...\n";
  this->printLLVMInstruction();
}

// Get number of variants
unsigned BaseInstruction::getNumVariants() { return this->variants.size(); }

// Print variants
void BaseInstruction::printMMVariants() {
#ifdef DEBUG
  if (this->variants.empty()) {
    llvm::outs() << "No variants for this instruction...\n";
    this->printInstruction();
    return;
  }

  llvm::outs() << "Number of variants : " << this->variants.size() << "\n";

  for (auto variant : this->variants) {
    if (this->getResultOperand().first) {
      unsigned result_ssa_version = variant.first;

      // llvm::outs() << "Result SSA Version: " << result_ssa_version <<
      // "\n";

      this->getResultOperand().first->setSSAVersion(result_ssa_version);

      for (unsigned i = 0; i < this->getNumOperands(); i++) {
        SLIMOperand *slim_operand_i = this->getOperand(i).first;

        llvm::Value *variable = slim_operand_i->getValue();

        slim_operand_i->setSSAVersion(variant.second[variable]);
      }

      this->printInstruction();

      for (unsigned i = 0; i < this->getNumOperands(); i++) {
        SLIMOperand *slim_operand_i = this->getOperand(i).first;

        slim_operand_i->resetSSAVersion();
      }
    } else {
      this->printInstruction();
      break;
    }
  }
#endif
}

void BaseInstruction::setInstructionType(InstructionType type) {
  this->instruction_type = type;
}
// --------------- APIs for the Legacy SLIM ---------------

// Returns true if the instruction is a call instruction
bool BaseInstruction::getCall() {
  return (this->getInstructionType() == InstructionType::CALL);
}

// Returns true if the instruction is a phi instruction
bool BaseInstruction::getPhi() {
  return (this->getInstructionType() == InstructionType::PHI);
}

// Return the LHS operand
SLIMOperandIndirectionPair BaseInstruction::getLHS() { return this->result; }

// Return the RHS operand(s) list
std::vector<SLIMOperandIndirectionPair> BaseInstruction::getRHS() {
  return this->operands;
}

// --------------------------------------------------------

// Alloca instruction (this instruction does not contain any RHS operands)
AllocaInstruction::AllocaInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to ALLOCA
  this->instruction_type = InstructionType::ALLOCA;

  SLIMOperand *new_operand = OperandRepository::getOrCreateSLIMOperand(
      (llvm::Value *)instruction, true);
  this->result = std::make_pair(new_operand, 1);
  this->result.first->setIsAlloca();
}

void AllocaInstruction::printInstruction() {}

// Load instruction (transformed like an assignment statement)
LoadInstruction::LoadInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to LOAD
  this->instruction_type = InstructionType::LOAD;

  this->is_expression_assignment = true;

  // The value of an instruction is the result operand
  llvm::Value *result_operand = (llvm::Value *)this->instruction;

  // Create the SLIM operand corresponding to the result operand
  SLIMOperand *result_slim_operand =
      OperandRepository::getOrCreateSLIMOperand(result_operand, false);

  this->result = std::make_pair(result_slim_operand, 1);

  llvm::Value *rhs_operand = this->instruction->getOperand(0);

  SLIMOperand *rhs_slim_operand =
      OperandRepository::getSLIMOperand(rhs_operand);

  if (!rhs_slim_operand) {
    // This means the variable is surely not an address-taken local variable
    // (otherwise it would be already present in the map because of alloca
    // instruction)
    if (llvm::isa<llvm::GlobalValue>(rhs_operand)) {
      rhs_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(rhs_operand, true);
    } else {
      rhs_slim_operand = OperandRepository::getOrCreateSLIMOperand(rhs_operand);
    }
  }

  if (rhs_slim_operand->isGlobalOrAddressTaken() ||
      rhs_slim_operand->isGEPInInstr() ||
      llvm::isa<llvm::GlobalValue>(rhs_slim_operand->getValue())) {
//#CHANGE
    this->operands.push_back(std::make_pair(rhs_slim_operand, 1));
  } else {
     this->operands.push_back(std::make_pair(rhs_slim_operand, 2));
  }

  if (result_slim_operand->getValue()->getType()->isPointerTy()) {
    this->has_pointer_variables = true;
  }
}

// Used for creating assignment statements of the formal-to-actual arguments (of
// a Call instruction)
LoadInstruction::LoadInstruction(llvm::CallInst *call_instruction,
                                 SLIMOperand *result, SLIMOperand *rhs_operand)
    : BaseInstruction(call_instruction) {
  // Set the instruction type to LOAD
  this->instruction_type = InstructionType::LOAD;

  this->is_expression_assignment = true;

  this->result = std::make_pair(result, 1);

  if (rhs_operand && (rhs_operand->getValue() != nullptr)) {
    llvm::Value *rhs_operand_after_strip = llvm::dyn_cast<llvm::Value>(
        rhs_operand->getValue()->stripPointerCasts());

    if (rhs_operand_after_strip) {
      rhs_operand =
          OperandRepository::getOrCreateSLIMOperand(rhs_operand_after_strip);
    }
  }

  // this->operands.push_back(std::make_pair(rhs_operand, 1));

  if (rhs_operand->isPointerVariable()) {
    this->has_pointer_variables = true;
  }

  if (rhs_operand->isGlobalOrAddressTaken() || rhs_operand->isGEPInInstr() ||
      llvm::isa<llvm::GlobalValue>(rhs_operand->getValue())) {
    this->operands.push_back(std::make_pair(rhs_operand, 0));
  } else {
    this->operands.push_back(std::make_pair(rhs_operand, 1));
  }
}

// Prints the load instruction
void LoadInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  pretty_printer_k1(this->result);

  // llvm::outs() << "<a";

  // this->result.first->printOperand(llvm::outs());

  // llvm::outs() << ", " << this->result.second << "> = ";

  // llvm::outs() << "<b";

  // this->operands[0].first->printOperand(llvm::outs());

  // llvm::outs() << ", " << this->operands[0].second << ">\n";

  llvm::outs() << " = ";
  pretty_printer_k1(this->operands[0]);
  llvm::outs() << "\n";
}
static bool isFunctionPointer(llvm::Value *value) {
    if (llvm::isa<llvm::ConstantExpr>(value)) {
        llvm::ConstantExpr *rhs_operand_expr =
            llvm::cast<llvm::ConstantExpr>(value);
        if (rhs_operand_expr->isCast()) {
            llvm::Value *rhs_operand_cast = rhs_operand_expr->getOperand(0);
            if (llvm::isa<llvm::Function>(rhs_operand_cast)) {
                return true;
            }
        }
    }
    // it can be void (...)* bitcast (void ()* @f1 to void (...)*) also
    if (llvm::isa<llvm::BitCastOperator>(value)) {
        llvm::Value *rhs_operand_cast =
            llvm::cast<llvm::BitCastOperator>(value)->getOperand(0);
        if (llvm::isa<llvm::Function>(rhs_operand_cast)) {
            return true;
        }
    }
    // or just a function pointer
    if (llvm::isa<llvm::Function>(value)) {
        return true;
    }
    return false;
}

static void addContainingFunctionPointers(StoreInstruction *instruction) {
  llvm::Value *result_operand =
      instruction->getLLVMInstruction()->getOperand(1);
  llvm::Value *rhs_value = instruction->getLLVMInstruction()->getOperand(0);
  // check if the rhs operand is a function pointer
  if (!isFunctionPointer(rhs_value)) {
    return;
  }
  // store can be to a GEP instruction
  if (llvm::isa<llvm::GEPOperator>(result_operand)) {
    llvm::Value *gep_operand =
        llvm::cast<llvm::GEPOperator>(result_operand)->getOperand(0);
    if (auto asUser = llvm::dyn_cast<llvm::Operator>(gep_operand)) {
      for (unsigned i = 0; i < asUser->getNumOperands(); i++) {  
        addFunctionPointersInConstantToValue(rhs_value, asUser->getOperand(i));
      }
    } else {
      SLIMOperand *result_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(gep_operand);
      if (result_slim_operand->isSource()) { 
        addFunctionPointersInConstantToValue(rhs_value, gep_operand);
      } else {
        for (auto &source : result_slim_operand->getRefersToSourceValues()) { 
          addFunctionPointersInConstantToValue(rhs_value, source->getValue());
        }
      }
    }
  } else {
    // or the array index might have been loaded previously, so it is
    // not a GEP
    // but it can be a bitcast, which is an Operator so check for that
    if (auto asUser = llvm::dyn_cast<llvm::Operator>(result_operand)) {
      for (unsigned i = 0; i < asUser->getNumOperands(); i++) { 
        addFunctionPointersInConstantToValue(rhs_value, asUser->getOperand(i));
      }
    } else {
      // else it is a pointer directly, so get its source and
      // add the function pointer to it
      SLIMOperand *result_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(result_operand);
      if (result_slim_operand->isSource()) { 
        addFunctionPointersInConstantToValue(rhs_value, result_operand);
      } else {
        for (auto &source : result_slim_operand->getRefersToSourceValues()) { 
          addFunctionPointersInConstantToValue(rhs_value, source->getValue());
        }
      }
    }
  }
}

// Store instruction
StoreInstruction::StoreInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to STORE
  this->instruction_type = InstructionType::STORE;

  // Get the result operand (operand corresponding to where the value is
  // stored)
  llvm::Value *result_operand = this->instruction->getOperand(1);
  SLIMOperand *result_slim_operand =
      OperandRepository::getSLIMOperand(result_operand);
  bool is_result_gep = llvm::isa<llvm::GEPOperator>(result_operand);

  if (!result_slim_operand) {
    // The variable is not an address-taken local variable (otherwise it
    // would be already present in the map because of alloca instruction)
    if (llvm::isa<llvm::GEPOperator>(result_operand)) {
      llvm::Value *gep_operand =
          llvm::cast<llvm::GEPOperator>(result_operand)->getOperand(0);

      if (llvm::isa<llvm::GlobalValue>(gep_operand)) {
        result_slim_operand =
            OperandRepository::getOrCreateSLIMOperand(result_operand, true);
      } else {
        result_slim_operand =
            OperandRepository::getOrCreateSLIMOperand(result_operand);
      }

      result_slim_operand->setIsPointerVariable();
    } else if (llvm::isa<llvm::GlobalValue>(result_operand)) {
      result_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(result_operand, true);
    } else if (!llvm::isa<llvm::GlobalVariable>(result_operand) &&
               llvm::isa<llvm::Constant>(result_operand)) {
      result_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(result_operand, false);
    } else if (llvm::isa<llvm::GlobalVariable>(result_operand)) {
      result_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(result_operand, true);
    } else {
      result_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(result_operand);
    }
  }

  // Operand can be either a constant, an address-taken local variable, a
  // function argument, a global variable or a temporary variable
  llvm::Value *rhs_operand = this->instruction->getOperand(0);
  bool is_rhs_gep = llvm::isa<llvm::GEPOperator>(rhs_operand);
  SLIMOperand *rhs_slim_operand =
      OperandRepository::getSLIMOperand(rhs_operand);

  if (!rhs_slim_operand) {
    // This means the variable is surely not an address-taken local variable
    // (otherwise it would be already present in the map because of alloca
    // instruction)
    if (!llvm::isa<llvm::GlobalVariable>(rhs_operand) &&
        llvm::isa<llvm::Constant>(rhs_operand)) {
      rhs_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(rhs_operand, false);
    } else if (llvm::isa<llvm::GlobalVariable>(rhs_operand) or
               llvm::isa<llvm::GlobalValue>(rhs_operand)) {
      rhs_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(rhs_operand, true);
    } else {
      rhs_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(rhs_operand, false);
    }
  }

  if (llvm::isa<llvm::Constant>(rhs_operand) && !rhs_operand->hasName() &&
      !rhs_slim_operand->isGEPInInstr()) {
    this->is_constant_assignment = true;

    if (llvm::isa<llvm::GlobalValue>(result_operand) ||
        result_slim_operand->isGEPInInstr() ||
        result_slim_operand->isAlloca()) {
      this->result = std::make_pair(result_slim_operand, 1);
    } else {
      this->result = std::make_pair(result_slim_operand, 2);
    }

    // 0 represents that the operand is a constant and not a memory location
    this->operands.push_back(std::make_pair(rhs_slim_operand, 0));
  } else {
    this->is_expression_assignment = true;
     bool fp = false;
     if (isFunctionPointer(rhs_operand)) {
///         this->result = std::make_pair(result_slim_operand, 2);
         if (result_slim_operand->isArrayElement())
          this->result = std::make_pair(result_slim_operand, 1); //modAR:sjeng
         else {
           if (result_slim_operand->isGlobalOrAddressTaken())
              this->result = std::make_pair(result_slim_operand, 1);//modAR:h264ref
           else
              this->result = std::make_pair(result_slim_operand, 2);//modAR:bzip2
         }
         this->operands.push_back(std::make_pair(rhs_slim_operand, 0));
         fp = true;
   
     }
     else if ((result_slim_operand->isGlobalOrAddressTaken() ||
         result_slim_operand->isGEPInInstr()) &&
        (rhs_slim_operand->isGlobalOrAddressTaken() ||
         rhs_slim_operand->isGEPInInstr() ||
         (llvm::isa<llvm::Function>(rhs_slim_operand->getValue())))) {
      this->result = std::make_pair(result_slim_operand, 1);
      this->operands.push_back(std::make_pair(rhs_slim_operand, 0));
    } else if ((result_slim_operand->isGlobalOrAddressTaken() ||
                result_slim_operand->isGEPInInstr()) &&
               (!(rhs_slim_operand->isGlobalOrAddressTaken() ||
                  rhs_slim_operand->isGEPInInstr()))) {
      this->result = std::make_pair(result_slim_operand, 1);
      this->operands.push_back(std::make_pair(rhs_slim_operand, 1));
    } else if ((!(result_slim_operand->isGlobalOrAddressTaken() ||
                  result_slim_operand->isGEPInInstr())) &&
               (rhs_slim_operand->isGlobalOrAddressTaken() ||
                rhs_slim_operand->isGEPInInstr())) {
      this->result = std::make_pair(result_slim_operand, 2);
      this->operands.push_back(std::make_pair(rhs_slim_operand, 0));
    } else if ((!(result_slim_operand->isGlobalOrAddressTaken() ||
                  result_slim_operand->isGEPInInstr())) &&
               (!(rhs_slim_operand->isGlobalOrAddressTaken() ||
                  rhs_slim_operand->isGEPInInstr()))) {
      this->result = std::make_pair(result_slim_operand, 2);
      this->operands.push_back(std::make_pair(rhs_slim_operand, 1));
    } else {
      // It should not be possible to reach this block of code
      llvm_unreachable("[StoreInstruction Error] Unexpected behaviour!\n");
    }
  }

  if (llvm::isa<llvm::ConstantData>(rhs_operand) &&
      !rhs_operand->getType()->isPointerTy()) {
    this->has_pointer_variables = false;
    result_slim_operand->unsetIsPointerVariable();
  } else if (result_slim_operand->isPointerVariable() ||
             rhs_slim_operand->isPointerVariable()) {
    this->has_pointer_variables = true;
  }
  addContainingFunctionPointers(this);
}

void StoreInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  pretty_printer_k1(this->result);

  // llvm::outs() << "<c";

  // this->result.first->printOperand(llvm::outs());

  // llvm::outs() << ", " << this->result.second << "> = ";

  // llvm::outs() << "<d";

  // this->operands[0].first->printOperand(llvm::outs());

  // llvm::outs() << ", " << this->operands[0].second << ">\n";

  llvm::outs() << " = ";
  pretty_printer_k1(this->operands[0]);
  llvm::outs() << "\n";
}

// Fence instruction
FenceInstruction::FenceInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to FENCE
  this->instruction_type = InstructionType::FENCE;

  if (llvm::isa<llvm::FenceInst>(this->instruction)) {
  } else
    llvm_unreachable("[FenceInstruction Error] The underlying LLVM "
                     "instruction is not a fence instruction!");
}

void FenceInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->printLLVMInstruction();
}

// Atomic compare and change instruction
AtomicCompareChangeInstruction::AtomicCompareChangeInstruction(
    llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to ATOMIC_COMPARE_CHANGE
  this->instruction_type = InstructionType::ATOMIC_COMPARE_CHANGE;

  llvm::AtomicCmpXchgInst *atomic_compare_change_inst;

  if (atomic_compare_change_inst =
          llvm::dyn_cast<llvm::AtomicCmpXchgInst>(this->instruction)) {
    llvm::Value *result_operand = (llvm::Value *)atomic_compare_change_inst;

    SLIMOperand *result_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(result_operand);

    // 0 means that either the operand is a constant or the indirection
    // level is not relevant
    this->result = std::make_pair(result_slim_operand, 0);

    OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

    llvm::Value *val_pointer_operand =
        atomic_compare_change_inst->getPointerOperand();

    SLIMOperand *pointer_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(val_pointer_operand);

    this->pointer_operand = std::make_pair(pointer_slim_operand, 1);

    llvm::Value *val_compare_operand =
        atomic_compare_change_inst->getCompareOperand();

    SLIMOperand *compare_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(val_compare_operand);

    this->compare_operand = std::make_pair(compare_slim_operand, 0);

    llvm::Value *val_new_operand =
        atomic_compare_change_inst->getNewValOperand();

    SLIMOperand *val_new_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(val_new_operand);

    this->new_value = std::make_pair(val_new_slim_operand, 0);
  } else {
    llvm_unreachable(
        "[AtomicCompareChangeInstruction Error] The underlying LLVM "
        "instruction is not an atomic compare and change instruction!");
  }
}

llvm::Value *AtomicCompareChangeInstruction::getPointerOperand() {
  return this->pointer_operand.first->getValue();
}

llvm::Value *AtomicCompareChangeInstruction::getCompareOperand() {
  return this->compare_operand.first->getValue();
}

llvm::Value *AtomicCompareChangeInstruction::getNewValue() {
  return this->new_value.first->getValue();
}

void AtomicCompareChangeInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->printLLVMInstruction();
}

// Atomic modify memory instruction
AtomicModifyMemInstruction::AtomicModifyMemInstruction(
    llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to ATOMIC_MODIFY_MEM
  this->instruction_type = InstructionType::ATOMIC_MODIFY_MEM;

  if (llvm::isa<llvm::AtomicRMWInst>(this->instruction)) {
  } else
    llvm_unreachable("[AtomicModifyMemInstruction Error] The underlying "
                     "LLVM instruction is not an AtomicRMW instruction!");
}

void AtomicModifyMemInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->printLLVMInstruction();
}

// Getelementptr instruction
GetElementPtrInstruction::GetElementPtrInstruction(
    llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to GET_ELEMENT_PTR
  this->instruction_type = InstructionType::GET_ELEMENT_PTR;

  llvm::GetElementPtrInst *get_element_ptr;

  if (get_element_ptr =
          llvm::dyn_cast<llvm::GetElementPtrInst>(this->instruction)) {

    mapResultOperandToSLIM(1);

    for (int i = 0; i < instruction->getNumOperands(); i++) {
      llvm::Value *operand_i = instruction->getOperand(i);

      if (operand_i->stripPointerCasts()) {
        operand_i = operand_i->stripPointerCasts();
      }

      SLIMOperand *slim_operand_i =
          OperandRepository::getOrCreateSLIMOperand(operand_i);

      
      if (operand_i->getType()->isArrayTy()) {
//         llvm::outs() << "\n SETARR 1";
         slim_operand_i->setArrayType(); //set the type as array
         // Get the variable operand (which is the first operand)
      ///   slim_operand_i->setGEPMainOperand(this->getMainOperand());      //set gep_main_operand
      }

      // 0 represents that either it is a constant or the indirection
      // level is not relevant
      this->operands.push_back(std::make_pair(slim_operand_i, 0));
    }

    SLIMOperand *gep_main_slim_operand;

    if (get_element_ptr->getPointerOperand()->stripPointerCasts()) {
      gep_main_slim_operand = OperandRepository::getOrCreateSLIMOperand(
          get_element_ptr->getPointerOperand()->stripPointerCasts());
   
     if (get_element_ptr->getPointerOperand()->stripPointerCasts()->getType()->isArrayTy()) {
         gep_main_slim_operand->setArrayType();
// llvm::outs() << "\n SETARR 2";
      }
    } else {
      gep_main_slim_operand = OperandRepository::getOrCreateSLIMOperand(
          get_element_ptr->getPointerOperand());

       if (get_element_ptr->getPointerOperand()->getType()->isArrayTy()) {
         gep_main_slim_operand->setArrayType();
// llvm::outs() << "\n SETARR 3";
       }
    }

    this->gep_main_operand = gep_main_slim_operand;

    // Create and store the index operands into the indices list
    for (int i = 1; i < get_element_ptr->getNumOperands(); i++) {
      llvm::Value *index_val = get_element_ptr->getOperand(i);

      if (llvm::isa<llvm::Constant>(index_val)) {
        if (llvm::isa<llvm::ConstantInt>(index_val)) {
          SLIMOperand *index_slim_operand =
              OperandRepository::getOrCreateSLIMOperand(index_val);
          this->indices.push_back(index_slim_operand);
        } else {
          llvm_unreachable("[GetElementPtrInstruction Error] The index is a "
                           "constant but not an integer constant!");
        }
      } else {
        if (index_val != nullptr) {
          llvm::Value *rhs_operand_after_strip =
              llvm::dyn_cast<llvm::Value>(index_val->stripPointerCasts());

          if (rhs_operand_after_strip) {
            index_val = rhs_operand_after_strip;
          }
        }

        SLIMOperand *index_slim_operand =
            OperandRepository::getOrCreateSLIMOperand(index_val);
        this->indices.push_back(index_slim_operand);
      }
    }
  } else {
    llvm_unreachable("[GetElementPtrInstruction Error] The underlying LLVM "
                     "instruction is not a getelementptr instruction!");
  }
}

// Returns the main operand (corresponding to the aggregate name)
SLIMOperand *GetElementPtrInstruction::getMainOperand() {
  return this->gep_main_operand;
}

// Returns the number of index operands
unsigned GetElementPtrInstruction::getNumIndexOperands() {
  return this->indices.size();
}

// Returns the operand corresponding to the index at the given position
// (0-based)
SLIMOperand *GetElementPtrInstruction::getIndexOperand(unsigned position) {
  // Make sure that the position is in bounds
  assert(position >= 0 && position < this->getNumIndexOperands());

  return this->indices[position];
}

void GetElementPtrInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  if (this->result.first->getValue()) {
    llvm::outs() << "<";

    this->result.first->printOperand(llvm::outs());

    llvm::outs() << ", " << this->result.second << ">";

    llvm::outs() << " = ";
  }

  llvm::GetElementPtrInst *get_element_ptr;

  if (get_element_ptr =
          llvm::dyn_cast<llvm::GetElementPtrInst>(this->instruction)) {
    llvm::outs() << "<";

    llvm::outs() << get_element_ptr->getPointerOperand()->getName();

    // if (gep_main_operand->getValue())
    // {
    //     gep_main_operand->printOperand(llvm::outs());
    //     llvm::outs() << "\n";
    // }

    for (int i = 1; i < get_element_ptr->getNumOperands(); i++) {
      llvm::Value *index_val = get_element_ptr->getOperand(i);

      if (llvm::isa<llvm::Constant>(index_val)) {
        if (llvm::isa<llvm::ConstantInt>(index_val)) {
          llvm::ConstantInt *constant_int =
              llvm::cast<llvm::ConstantInt>(index_val);

          llvm::outs() << "[" << constant_int->getSExtValue() << "]";
        } else {
          llvm_unreachable("[GetElementPtrInstruction Error] The index is a "
                           "constant but not an integer constant!");
        }
      } else {
        llvm::outs() << "[" << index_val->getName() << "]";
      }
    }

    // The indirection level (0) can be fetched from the individual
    // indirection of indices as well
    llvm::outs() << ", 0>";
    llvm::outs() << "\n";
  } else {
    llvm_unreachable("[GetElementPtrInstruction Error] The underlying LLVM "
                     "instruction is not a getelementptr instruction!");
  }
}

// Unary operations

// Floating-point negation instruction
FPNegationInstruction::FPNegationInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to FP_NEGATION
  this->instruction_type = InstructionType::FP_NEGATION;

  this->is_expression_assignment = true;

  llvm::Value *result_operand = (llvm::Value *)this->instruction;

  SLIMOperand *result_slim_operand =
      OperandRepository::getOrCreateSLIMOperand(result_operand);

  this->result = std::make_pair(result_slim_operand, 0);

  OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

  llvm::Value *operand = this->instruction->getOperand(0);

  SLIMOperand *slim_operand =
      OperandRepository::getOrCreateSLIMOperand(operand);

  // 0 represents that either it is a constant or the indirection level is not
  // relevant
  this->operands.push_back(std::make_pair(slim_operand, 0));
}

void FPNegationInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->result.first->printOperand(llvm::outs());

  llvm::outs() << " = ";

  this->operands[0].first->printOperand(llvm::outs());
  // Value *operand = this->operands[0].first->getValue();

  // if (isa<Constant>(operand))
  // {
  //     if (isa<ConstantFP>(operand))
  //     {
  //         ConstantFP *constant_fp = cast<ConstantFP>(operand);

  //         llvm::outs() << constant_fp->getValueAPF().convertToFloat();
  //     }
  //     else
  //     {
  //         llvm_unreachable("[FPNegationInstruction Error] The operand is a
  //         constant but not a floating-point constant!");
  //     }
  // }
  // else
  // {
  //     llvm::outs() << "-" << operand->getName();
  // }

  llvm::outs() << "\n";
}

// Binary operation
BinaryOperation::BinaryOperation(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to BINARY_OPERATION
  this->instruction_type = InstructionType::BINARY_OPERATION;

  this->is_expression_assignment = true;

  llvm::BinaryOperator *binary_operator;

  if (binary_operator =
          llvm::dyn_cast<llvm::BinaryOperator>(this->instruction)) {
    llvm::Value *result_operand = (llvm::Value *)binary_operator;

    SLIMOperand *result_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(result_operand);

    this->result = std::make_pair(result_slim_operand, 0);

    // Set the operation type
    switch (binary_operator->getOpcode()) {
    case llvm::Instruction::BinaryOps::Add:
    case llvm::Instruction::BinaryOps::FAdd:
      this->binary_operator = ADD;
      break;
    case llvm::Instruction::BinaryOps::Sub:
    case llvm::Instruction::BinaryOps::FSub:
      this->binary_operator = SUB;
      break;
    case llvm::Instruction::BinaryOps::Mul:
    case llvm::Instruction::BinaryOps::FMul:
      this->binary_operator = MUL;
      break;
    case llvm::Instruction::BinaryOps::UDiv:
    case llvm::Instruction::BinaryOps::SDiv:
    case llvm::Instruction::BinaryOps::FDiv:
      this->binary_operator = DIV;
      break;
    case llvm::Instruction::BinaryOps::URem:
    case llvm::Instruction::BinaryOps::SRem:
    case llvm::Instruction::BinaryOps::FRem:
      this->binary_operator = REM;
      break;
    case llvm::Instruction::BinaryOps::Shl:
      this->binary_operator = SHIFT_LEFT;
      break;
    case llvm::Instruction::BinaryOps::LShr:
      this->binary_operator = LOGICAL_SHIFT_RIGHT;
      break;
    case llvm::Instruction::BinaryOps::AShr:
      this->binary_operator = ARITHMETIC_SHIFT_RIGHT;
      break;
    case llvm::Instruction::BinaryOps::And:
      this->binary_operator = BITWISE_AND;
      break;
    case llvm::Instruction::BinaryOps::Or:
      this->binary_operator = BITWISE_OR;
      break;
    case llvm::Instruction::BinaryOps::Xor:
      this->binary_operator = BITWISE_XOR;
      break;

    default:
      llvm_unreachable("[BinaryOperation Error] Unexpected binary operator!");
    }

    for (int i = 0; i < instruction->getNumOperands(); i++) {
      llvm::Value *operand_i = instruction->getOperand(i);

      SLIMOperand *slim_operand_i =
          OperandRepository::getOrCreateSLIMOperand(operand_i);

      // 0 represents that either it is a constant or the indirection
      // level is not relevant
      this->operands.push_back(std::make_pair(slim_operand_i, 0));
    }
  } else {
    llvm_unreachable("[BinaryOperation Error] The underlying LLVM "
                     "instruction does not perform a binary operation!");
  }
}

SLIMBinaryOperator BinaryOperation::getOperationType() {
  return this->binary_operator;
}

void BinaryOperation::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << "<" << this->result.first->getValue()->getName() << ", "
               << this->result.second << "> = ";

  for (int i = 0; i < this->operands.size(); i++) {
    llvm::outs() << "<";
    this->operands[i].first->printOperand(llvm::outs());
    llvm::outs() << ", " << this->operands[i].second << ">";

    if (i != this->operands.size() - 1) {
      switch (this->getOperationType()) {
      case ADD:
        llvm::outs() << " + ";
        break;
      case SUB:
        llvm::outs() << " - ";
        break;
      case MUL:
        llvm::outs() << " * ";
        break;
      case DIV:
        llvm::outs() << " / ";
        break;
      case REM:
        llvm::outs() << " % ";
        break;

      case SHIFT_LEFT:
        llvm::outs() << " << ";
        break;
      case LOGICAL_SHIFT_RIGHT:
        llvm::outs() << " >>> ";
        break;
      case ARITHMETIC_SHIFT_RIGHT:
        llvm::outs() << " >> ";
        break;
      case BITWISE_AND:
        llvm::outs() << " & ";
        break;
      case BITWISE_OR:
        llvm::outs() << " | ";
        break;
      case BITWISE_XOR:
        llvm::outs() << " ^ ";
        break;

      default:
        llvm_unreachable("[BinaryOperation Error] Unexpected binary "
                         "operation type!");
      }
    } else {
      llvm::outs() << "\n";
    }
  }
}

// Vector operations

// Extract element instruction
ExtractElementInstruction::ExtractElementInstruction(
    llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to EXTRACT_ELEMENT
  this->instruction_type = InstructionType::EXTRACT_ELEMENT;

  llvm::Value *result_operand = (llvm::Value *)instruction;

  SLIMOperand *result_slim_operand =
      OperandRepository::getOrCreateSLIMOperand(result_operand);

  // 0 represents that either it is a constant or the indirection level is not
  // relevant
  this->result = std::make_pair(result_slim_operand, 0);

  OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

  for (int i = 0; i < instruction->getNumOperands(); i++) {
    llvm::Value *operand_i = instruction->getOperand(i);

    SLIMOperand *slim_operand_i =
        OperandRepository::getOrCreateSLIMOperand(operand_i);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->operands.push_back(std::make_pair(slim_operand_i, 0));
  }
}

void ExtractElementInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->result.first->printOperand(llvm::outs());

  llvm::outs() << " = ";

  // Value *operand_0 = this->operands[0].first->getValue();
  this->operands[0].first->printOperand(llvm::outs());

  llvm::outs() << "[";

  // Value *operand_1 = this->operands[1].first->getValue();

  this->operands[1].first->printOperand(llvm::outs());

  llvm::outs() << "]\n";
}

// Insert element instruction
InsertElementInstruction::InsertElementInstruction(
    llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to INSERT_ELEMENT
  this->instruction_type = InstructionType::INSERT_ELEMENT;

  llvm::Value *result_operand = (llvm::Value *)instruction;

  SLIMOperand *result_slim_operand =
      OperandRepository::getOrCreateSLIMOperand(result_operand);

  // 0 represents that either it is a constant or the indirection level is not
  // relevant
  this->result = std::make_pair(result_slim_operand, 0);

  OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

  for (int i = 0; i < instruction->getNumOperands(); i++) {
    llvm::Value *operand_i = instruction->getOperand(i);

    SLIMOperand *slim_operand_i =
        OperandRepository::getOrCreateSLIMOperand(operand_i);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->operands.push_back(std::make_pair(slim_operand_i, 0));
  }
}

void InsertElementInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->result.first->printOperand(llvm::outs());

  llvm::outs() << " = ";

  this->operands[0].first->printOperand(llvm::outs());

  llvm::outs() << ".insert(";

  this->operands[1].first->printOperand(llvm::outs());

  llvm::outs() << ", ";

  this->operands[2].first->printOperand(llvm::outs());

  llvm::outs() << ")\n";
}

// ShuffleVector instruction
ShuffleVectorInstruction::ShuffleVectorInstruction(
    llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to SHUFFLE_VECTOR
  this->instruction_type = InstructionType::SHUFFLE_VECTOR;

  llvm::Value *result_operand = (llvm::Value *)instruction;

  SLIMOperand *result_slim_operand =
      OperandRepository::getOrCreateSLIMOperand(result_operand);

  // 0 represents that either it is a constant or the indirection level is not
  // relevant
  this->result = std::make_pair(result_slim_operand, 0);

  OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

  for (int i = 0; i < instruction->getNumOperands(); i++) {
    llvm::Value *operand_i = instruction->getOperand(i);

    SLIMOperand *slim_operand_i =
        OperandRepository::getOrCreateSLIMOperand(operand_i);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->operands.push_back(std::make_pair(slim_operand_i, 0));
  }
}

void ShuffleVectorInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::outs() << "shufflevector(";

  for (int i = 0; i < this->getNumOperands() - 1; i++) {
    llvm::Value *operand = this->operands[i].first->getValue();
    llvm::outs() << operand->getName() << ", ";
  }

  llvm::Value *operand_2 = this->operands[this->getNumOperands() - 1].first->getValue();
  llvm::outs() << operand_2->getName() << ")\n";
}

// Operations for aggregates (structure and array) stored in registers

// ExtractValue instruction
ExtractValueInstruction::ExtractValueInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to EXTRACT_VALUE
  this->instruction_type = InstructionType::EXTRACT_VALUE;

  llvm::ExtractValueInst *extract_value_inst;

  if (extract_value_inst =
          llvm::dyn_cast<llvm::ExtractValueInst>(this->instruction)) {
    llvm::Value *result_operand = (llvm::Value *)instruction;

    SLIMOperand *result_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(result_operand);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->result = std::make_pair(result_slim_operand, 0);

    OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

    // Get aggregate operand
    llvm::Value *aggregate_operand = extract_value_inst->getAggregateOperand();

    // Get the SLIM operand object corresponding to the aggregate operand
    SLIMOperand *slim_aggregate_operand =
        OperandRepository::getOrCreateSLIMOperand(aggregate_operand);

    this->operands.push_back(std::make_pair(slim_aggregate_operand, 0));

    // Insert the indices
    for (auto i : extract_value_inst->getIndices()) {
      this->indices.push_back(i);
    }
  } else {
    llvm_unreachable("[ExtractValueInstruction Error] The underlying LLVM "
                     "instruction is not a extractvalue instruction!");
  }
}

unsigned ExtractValueInstruction::getNumIndices() {
  return this->indices.size();
}

unsigned ExtractValueInstruction::getIndex(unsigned index) {
  assert(index >= 0 && index < this->getNumIndices());

  return this->indices[index];
}

void ExtractValueInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->result.first->printOperand(llvm::outs());

  llvm::outs() << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  llvm::outs() << operand_0->getName();

  for (auto index : this->indices) {
    llvm::outs() << "[" << index << "]";
  }

  llvm::outs() << "\n";
}

// InsertValue instruction
InsertValueInstruction::InsertValueInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to INSERT_VALUE
  this->instruction_type = InstructionType::INSERT_VALUE;

  llvm::Value *result_operand = (llvm::Value *)instruction;

  SLIMOperand *result_slim_operand =
      OperandRepository::getOrCreateSLIMOperand(result_operand);

  // 0 represents that either it is a constant or the indirection level is not
  // relevant
  this->result = std::make_pair(result_slim_operand, 0);

  OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

  for (int i = 0; i < instruction->getNumOperands(); i++) {
    llvm::Value *operand_i = instruction->getOperand(i);

    SLIMOperand *slim_operand_i =
        OperandRepository::getOrCreateSLIMOperand(operand_i);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->operands.push_back(std::make_pair(slim_operand_i, 0));
  }
}

void InsertValueInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::Value *operand_0_aggregate_name = this->operands[0].first->getValue();

  llvm::outs() << operand_0_aggregate_name->getName();

  llvm::Value *operand_1_value_to_insert = this->operands[1].first->getValue();

  for (int i = 2; i < this->operands.size(); i++) {
    llvm::Value *operand_i = this->operands[i].first->getValue();

    if (llvm::isa<llvm::ConstantInt>(operand_i)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_i);

      llvm::outs() << "[" << constant_int->getSExtValue() << "]";
    } else {
      llvm_unreachable("[InsertValueInstruction Error] Unexpected index");
    }
  }

  llvm::outs() << ".insert(";

  this->operands[1].first->printOperand(llvm::outs());

  llvm::outs() << ")\n";
}

// Conversion operations

// Trunc instruction
TruncInstruction::TruncInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to TRUNC
  this->instruction_type = InstructionType::TRUNC;

  this->is_expression_assignment = true;

  if (llvm::isa<llvm::TruncInst>(this->instruction) ||
      llvm::isa<llvm::FPTruncInst>(this->instruction)) {
    llvm::Value *result_operand = (llvm::Value *)instruction;

    SLIMOperand *result_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(result_operand);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->result = std::make_pair(result_slim_operand, 0);

    // Set the resulting type
    if (llvm::isa<llvm::TruncInst>(this->instruction))
      this->resulting_type =
          llvm::cast<llvm::TruncInst>(this->instruction)->getDestTy();
    else
      this->resulting_type =
          llvm::cast<llvm::FPTruncInst>(this->instruction)->getDestTy();

    OperandRepository::setSLIMOperand(result_operand, result_slim_operand);
    mapOperandsToSLIMOperands(0);
    // for (int i = 0; i < instruction->getNumOperands(); i++) {
    //     llvm::Value *operand_i = instruction->getOperand(i);

    //     SLIMOperand *slim_operand_i =
    //         OperandRepository::getSLIMOperand(operand_i);

    //     if (!slim_operand_i) {
    //         slim_operand_i =
    //         OperandRepository::getOrCreateSLIMOperand(operand_i);
    //         OperandRepository::setSLIMOperand(operand_i, slim_operand_i);
    //     }

    //     // 0 represents that either it is a constant or the indirection
    //     // level is not relevant
    //     this->operands.push_back(std::make_pair(slim_operand_i, 0));
    // }
  } else {
    llvm_unreachable("[TruncInstruction Error] The underlying LLVM "
                     "instruction is not a trunc instruction!");
  }
}

llvm::Type *TruncInstruction::getResultingType() {
  return this->resulting_type;
}

void TruncInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  llvm::outs() << "(";

  this->getResultingType()->print(llvm::outs());

  llvm::outs() << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      llvm::outs() << constant_int->getSExtValue();
    } else if (llvm::isa<llvm::ConstantFP>(operand_0)) {
      llvm::ConstantFP *constant_float =
          llvm::cast<llvm::ConstantFP>(operand_0);

      llvm::outs() << constant_float->getValueAPF().convertToFloat();
    } else {
      llvm::outs() << "[TruncInstruction Error] Unexpected constant!\n";
    }
  } else {
    llvm::outs() << operand_0->getName();
  }

  llvm::outs() << "\n";
}

// Zext instruction
ZextInstruction::ZextInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to ZEXT
  this->instruction_type = InstructionType::ZEXT;

  this->is_expression_assignment = true;

  llvm::ZExtInst *zext_inst;

  if (zext_inst = llvm::dyn_cast<llvm::ZExtInst>(this->instruction)) {
    llvm::Value *result_operand = (llvm::Value *)instruction;

    SLIMOperand *result_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(result_operand);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->result = std::make_pair(result_slim_operand, 0);

    // Set the resulting type
    this->resulting_type = zext_inst->getDestTy();

    for (int i = 0; i < instruction->getNumOperands(); i++) {
      llvm::Value *operand_i = instruction->getOperand(i);

      SLIMOperand *slim_operand_i =
          OperandRepository::getOrCreateSLIMOperand(operand_i);

      // 0 represents that either it is a constant or the indirection
      // level is not relevant
      this->operands.push_back(std::make_pair(slim_operand_i, 0));
    }
  } else {
    llvm_unreachable("[ZextInstruction Error] The underlying LLVM "
                     "instruction is not a zext instruction!");
  }
}

llvm::Type *ZextInstruction::getResultingType() { return this->resulting_type; }

void ZextInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  llvm::outs() << "(Zext-";

  this->getResultingType()->print(llvm::outs());

  llvm::outs() << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      llvm::outs() << constant_int->getSExtValue();
    } else if (llvm::isa<llvm::ConstantFP>(operand_0)) {
      llvm::ConstantFP *constant_float =
          llvm::cast<llvm::ConstantFP>(operand_0);

      llvm::outs() << constant_float->getValueAPF().convertToFloat();
    } else {
      llvm::outs() << "[ZextInstruction Error] Unexpected constant!\n";
    }
  } else {
    llvm::outs() << operand_0->getName();
  }

  llvm::outs() << "\n";
}

// Sext instruction
SextInstruction::SextInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to SEXT
  this->instruction_type = InstructionType::SEXT;

  this->is_expression_assignment = true;

  llvm::SExtInst *sext_inst;

  if (sext_inst = llvm::dyn_cast<llvm::SExtInst>(this->instruction)) {
    llvm::Value *result_operand = (llvm::Value *)instruction;

    SLIMOperand *result_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(result_operand);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->result = std::make_pair(result_slim_operand, 0);

    OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

    // Set the resulting type
    this->resulting_type = sext_inst->getDestTy();

    for (int i = 0; i < instruction->getNumOperands(); i++) {
      llvm::Value *operand_i = instruction->getOperand(i);

      SLIMOperand *slim_operand_i =
          OperandRepository::getOrCreateSLIMOperand(operand_i);

      // 0 represents that either it is a constant or the indirection
      // level is not relevant
      this->operands.push_back(std::make_pair(slim_operand_i, 0));
    }
  } else {
    llvm_unreachable("[SextInstruction Error] The underlying LLVM "
                     "instruction is not a sext instruction!");
  }
}

llvm::Type *SextInstruction::getResultingType() { return this->resulting_type; }

void SextInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  llvm::outs() << "(SignExt-";

  this->getResultingType()->print(llvm::outs());

  llvm::outs() << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      llvm::outs() << constant_int->getSExtValue();
    } else {
      llvm::outs() << "[SextInstruction Error] Unexpected constant!\n";
    }
  } else {
    llvm::outs() << operand_0->getName();
  }

  llvm::outs() << "\n";
}

// FPExt instruction
FPExtInstruction::FPExtInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to FPEXT
  this->instruction_type = InstructionType::FPEXT;

  this->is_expression_assignment = true;

  if (llvm::isa<llvm::FPExtInst>(this->instruction)) {
    llvm::FPExtInst *fp_ext_inst =
        llvm::cast<llvm::FPExtInst>(this->instruction);

    this->resulting_type = fp_ext_inst->getDestTy();
  } else {
    llvm_unreachable("[IntToFPInstruction Error] The underlying LLVM "
                     "instruction is not a fpext instruction!");
  }

  llvm::Value *result_operand = (llvm::Value *)instruction;

  SLIMOperand *result_slim_operand =
      OperandRepository::getOrCreateSLIMOperand(result_operand);

  // 0 represents that either it is a constant or the indirection level is not
  // relevant
  this->result = std::make_pair(result_slim_operand, 0);

  OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

  for (int i = 0; i < instruction->getNumOperands(); i++) {
    llvm::Value *operand_i = instruction->getOperand(i);

    SLIMOperand *slim_operand_i =
        OperandRepository::getOrCreateSLIMOperand(operand_i);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->operands.push_back(std::make_pair(slim_operand_i, 0));
  }
}

llvm::Type *FPExtInstruction::getResultingType() {
  return this->resulting_type;
}

void FPExtInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  llvm::outs() << "(FPExt-";

  this->getResultingType()->print(llvm::outs());

  llvm::outs() << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantFP>(operand_0)) {
      llvm::ConstantFP *constant_float =
          llvm::cast<llvm::ConstantFP>(operand_0);

      llvm::outs() << constant_float->getValueAPF().convertToFloat();
    } else {
      llvm::outs() << "[FPExtInstruction Error] Unexpected constant!\n";
    }
  } else {
    llvm::outs() << operand_0->getName();
  }

  llvm::outs() << "\n";
}

// FPToUi instruction
FPToIntInstruction::FPToIntInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to FP_TO_INT
  this->instruction_type = InstructionType::FP_TO_INT;

  this->is_expression_assignment = true;

  if (llvm::isa<llvm::FPToUIInst>(this->instruction)) {
    this->resulting_type =
        llvm::cast<llvm::FPToUIInst>(this->instruction)->getDestTy();
  } else if (llvm::isa<llvm::FPToSIInst>(this->instruction)) {
    this->resulting_type =
        llvm::cast<llvm::FPToSIInst>(this->instruction)->getDestTy();
  } else {
    llvm_unreachable("[FPToIntInstruction Error] The underlying LLVM "
                     "instruction is not a FPToUI or FPToSI instruction!");
  }

  mapAllOperandsToSLIM(0);
}

llvm::Type *FPToIntInstruction::getResultingType() {
  return this->resulting_type;
}

void FPToIntInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  llvm::outs() << "(";

  this->getResultingType()->print(llvm::outs());

  llvm::outs() << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      llvm::outs() << constant_int->getSExtValue();
    } else if (llvm::isa<llvm::ConstantFP>(operand_0)) {
      llvm::ConstantFP *constant_float =
          llvm::cast<llvm::ConstantFP>(operand_0);

      llvm::outs() << constant_float->getValueAPF().convertToFloat();
    } else {
      llvm::outs() << "[FPToIntInstruction Error] Unexpected constant!\n";
    }
  } else {
    llvm::outs() << operand_0->getName();
  }

  llvm::outs() << "\n";
}

// IntToFP instruction
IntToFPInstruction::IntToFPInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to INT_TO_FP
  this->instruction_type = InstructionType::INT_TO_FP;

  this->is_expression_assignment = true;

  if (llvm::isa<llvm::UIToFPInst>(this->instruction)) {
    llvm::UIToFPInst *ui_to_fp_inst =
        llvm::cast<llvm::UIToFPInst>(this->instruction);

    this->resulting_type = ui_to_fp_inst->getDestTy();
  } else if (llvm::isa<llvm::SIToFPInst>(this->instruction)) {
    llvm::SIToFPInst *si_to_fp_inst =
        llvm::cast<llvm::SIToFPInst>(this->instruction);

    this->resulting_type = si_to_fp_inst->getDestTy();
  } else {
    llvm_unreachable(
        "[IntToFPInstruction Error] The underlying LLVM instruction is not "
        "a integer-to-floating-point conversion instruction!");
  }

  mapAllOperandsToSLIM(0);
}

llvm::Type *IntToFPInstruction::getResultingType() {
  return this->resulting_type;
}

void IntToFPInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  llvm::outs() << "(";

  this->getResultingType()->print(llvm::outs());

  llvm::outs() << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      llvm::outs() << constant_int->getSExtValue();
    } else {
      llvm::outs() << "[IntToFPInstruction Error] Unexpected constant!\n";
    }
  } else {
    llvm::outs() << operand_0->getName();
  }

  llvm::outs() << "\n";
}

// PtrToInt instruction
PtrToIntInstruction::PtrToIntInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to PTR_TO_INT
  this->instruction_type = InstructionType::PTR_TO_INT;

  this->is_expression_assignment = true;

  if (llvm::isa<llvm::PtrToIntInst>(this->instruction)) {
    llvm::Value *result_operand = (llvm::Value *)instruction;

    SLIMOperand *result_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(result_operand);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->result = std::make_pair(result_slim_operand, 0);

    OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

    this->resulting_type =
        llvm::cast<llvm::PtrToIntInst>(this->instruction)->getDestTy();

    mapOperandsToSLIMOperands(0);
  } else {
    llvm_unreachable("[PtrToIntInstruction Error] The underlying LLVM "
                     "instruction is not a PtrToInt instruction!");
  }
}

llvm::Type *PtrToIntInstruction::getResultingType() {
  return this->resulting_type;
}

void PtrToIntInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  llvm::outs() << "(";

  this->getResultingType()->print(llvm::outs());

  llvm::outs() << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      llvm::outs() << constant_int->getSExtValue();
    } else {
      llvm::outs() << "[PtrToIntInstruction Error] Unexpected constant!\n";
    }
  } else {
    llvm::outs() << operand_0->getName();
  }

  llvm::outs() << "\n";
}

// IntToPtr instruction
IntToPtrInstruction::IntToPtrInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to INT_TO_PTR
  this->instruction_type = InstructionType::INT_TO_PTR;

  this->is_expression_assignment = true;

  if (llvm::isa<llvm::IntToPtrInst>(this->instruction)) {
    mapResultOperandToSLIM(0);

    // Set the resulting type
    this->resulting_type =
        llvm::cast<llvm::IntToPtrInst>(this->instruction)->getDestTy();

    mapOperandsToSLIMOperands(0);
  } else {
    llvm_unreachable("[IntToPtrInstruction Error] The underlying LLVM "
                     "instruction is not a IntToPtr instruction!");
  }
}

llvm::Type *IntToPtrInstruction::getResultingType() {
  return this->resulting_type;
}

void IntToPtrInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  llvm::outs() << "(";

  this->getResultingType()->print(llvm::outs());

  llvm::outs() << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      llvm::outs() << constant_int->getSExtValue();
    } else {
      llvm::outs() << "[IntToPtrInstruction Error] Unexpected constant!\n";
    }
  } else {
    llvm::outs() << operand_0->getName();
  }

  llvm::outs() << "\n";
}

// Bitcast instruction
BitcastInstruction::BitcastInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to BITCAST
  this->instruction_type = InstructionType::BITCAST;

  this->is_expression_assignment = true;

  llvm::BitCastInst *bitcast_inst;

  if (bitcast_inst = llvm::dyn_cast<llvm::BitCastInst>(this->instruction)) {
    llvm::Value *result_operand = (llvm::Value *)bitcast_inst;

    SLIMOperand *result_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(result_operand);

    this->result = std::make_pair(result_slim_operand, 1);

    OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

    llvm::Value *operand_0 = instruction->getOperand(0);

    SLIMOperand *slim_operand_0 =
        OperandRepository::getOrCreateSLIMOperand(operand_0);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->operands.push_back(std::make_pair(slim_operand_0, 1));

    this->resulting_type = bitcast_inst->getDestTy();
  } else {
    llvm_unreachable("[BitcastInstruction Error] The underlying LLVM "
                     "instruction is not a bitcast instruction!");
  }
}

llvm::Type *BitcastInstruction::getResultingType() {
  return this->resulting_type;
}

void BitcastInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  llvm::outs() << "(";

  this->getResultingType()->print(llvm::outs());

  llvm::outs() << ") ";

  this->operands[0].first->printOperand(llvm::outs());

  llvm::outs() << "\n";
}

// Address space instruction
AddrSpaceInstruction::AddrSpaceInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to ADDR_SPACE
  this->instruction_type = InstructionType::ADDR_SPACE;

  mapAllOperandsToSLIM(0);
}

void AddrSpaceInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  llvm::outs() << "(address-space-cast-";

  llvm::Type *operand_1 = this->operands[1].first->getValue()->getType();

  operand_1->print(llvm::outs());

  llvm::outs() << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      llvm::outs() << constant_int->getSExtValue();
    } else {
      llvm::outs() << "[AddrSpaceInstruction Error] Unexpected constant!\n";
    }
  } else {
    llvm::outs() << operand_0->getName();
  }

  llvm::outs() << "\n";
}

// Other important instructions

// Compare instruction
CompareInstruction::CompareInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to COMPARE
  this->instruction_type = InstructionType::COMPARE;

  this->is_expression_assignment = true;

  mapAllOperandsToSLIM(0);
}

void CompareInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = ";

  llvm::Value *condition_operand = this->operands[0].first->getValue();

  llvm::Value *operand_1 = this->operands[0].first->getValue();

  llvm::Value *operand_2 = this->operands[1].first->getValue();

  this->operands[0].first->printOperand(llvm::outs());
  // if (isa<Constant>(operand_1))
  // {
  //     if (cast<Constant>(operand_1)->isNullValue())
  //     {
  //         llvm::outs() << "null";
  //     }
  //     else if (isa<ConstantInt>(operand_1))
  //     {
  //         ConstantInt *constant_int = cast<ConstantInt>(operand_1);

  //         llvm::outs() << constant_int->getSExtValue();
  //     }
  //     else if (isa<ConstantFP>(operand_1))
  //     {
  //         ConstantFP *constant_float = cast<ConstantFP>(operand_1);

  //         llvm::outs() << constant_float->getValueAPF().convertToFloat();
  //     }
  //     else
  //     {
  //         llvm_unreachable("[CompareInstruction Error] Unexpected
  //         constant!");
  //     }
  // }
  // else
  // {
  //     llvm::outs() << operand_1->getName();
  // }

  if (llvm::isa<llvm::ICmpInst>(this->instruction)) {
    llvm::ICmpInst *icmp_instruction =
        llvm::cast<llvm::ICmpInst>(this->instruction);

    llvm::CmpInst::Predicate predicate = icmp_instruction->getPredicate();

    switch (predicate) {
    case llvm::CmpInst::ICMP_EQ:
      llvm::outs() << " == ";
      break;

    case llvm::CmpInst::ICMP_NE:
      llvm::outs() << " != ";
      break;

    case llvm::CmpInst::ICMP_UGT:
    case llvm::CmpInst::ICMP_SGT:
      llvm::outs() << " > ";
      break;

    case llvm::CmpInst::ICMP_UGE:
    case llvm::CmpInst::ICMP_SGE:
      llvm::outs() << " >= ";
      break;

    case llvm::CmpInst::ICMP_ULT:
    case llvm::CmpInst::ICMP_SLT:
      llvm::outs() << " < ";
      break;

    case llvm::CmpInst::ICMP_ULE:
    case llvm::CmpInst::ICMP_SLE:
      llvm::outs() << " <= ";
      break;

    default:
      llvm_unreachable("[CompareInstruction Error] Unexpected predicate!");
    }
  } else if (llvm::isa<llvm::FCmpInst>(this->instruction)) {
    llvm::FCmpInst *fcmp_instruction =
        llvm::cast<llvm::FCmpInst>(this->instruction);

    llvm::FCmpInst::Predicate predicate = fcmp_instruction->getPredicate();

    switch (predicate) {
    case llvm::CmpInst::FCMP_OEQ:
    case llvm::CmpInst::FCMP_UEQ:
      llvm::outs() << " == ";
      break;

    case llvm::CmpInst::FCMP_ONE:
    case llvm::CmpInst::FCMP_UNE:
      llvm::outs() << " != ";
      break;

    case llvm::CmpInst::FCMP_OGT:
    case llvm::CmpInst::FCMP_UGT:
      llvm::outs() << " > ";
      break;

    case llvm::CmpInst::FCMP_OGE:
    case llvm::CmpInst::FCMP_UGE:
      llvm::outs() << " >= ";
      break;

    case llvm::CmpInst::FCMP_OLT:
    case llvm::CmpInst::FCMP_ULT:
      llvm::outs() << " < ";
      break;

    case llvm::CmpInst::FCMP_OLE:
    case llvm::CmpInst::FCMP_ULE:
      llvm::outs() << " <= ";
      break;

    // True if both the operands are not QNAN
    case llvm::CmpInst::FCMP_ORD:
      llvm::outs() << " !QNAN";
      break;

    // True if either of the operands is/are a QNAN
    case llvm::CmpInst::FCMP_UNO:
      llvm::outs() << " EITHER-QNAN ";
      break;

    case llvm::CmpInst::FCMP_FALSE:
      llvm::outs() << " false ";
      break;

    case llvm::CmpInst::FCMP_TRUE:
      llvm::outs() << " true ";
      break;

    default:
      llvm_unreachable("[CompareInstruction Error] Unexpected predicate!");
    }
  } else {
    llvm_unreachable(
        "[CompareInstruction Error] Unexpected compare instruction type!");
  }

  // if (isa<Constant>(operand_2))
  // {
  //     if (cast<Constant>(operand_2)->isNullValue())
  //     {
  //         llvm::outs() << "null";
  //     }
  //     else if (isa<ConstantInt>(operand_2))
  //     {
  //         ConstantInt *constant_int = cast<ConstantInt>(operand_2);

  //         llvm::outs() << constant_int->getSExtValue();
  //     }
  //     else if (isa<ConstantFP>(operand_2))
  //     {
  //         ConstantFP *constant_float = cast<ConstantFP>(operand_2);

  //         llvm::outs() << constant_float->getValueAPF().convertToFloat();
  //     }
  //     else
  //     {
  //         llvm_unreachable("[CompareInstruction Error] Unexpected
  //         constant!");
  //     }
  // }
  // else
  // {
  //     llvm::outs() << operand_2->getName();
  // }

  this->operands[1].first->printOperand(llvm::outs());

  llvm::outs() << "\n";
}

// Phi instruction
PhiInstruction::PhiInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to PHI
  this->instruction_type = InstructionType::PHI;

  this->is_expression_assignment = true;

  mapResultOperandToSLIM(1);

  llvm::PHINode *phi_inst = llvm::cast<llvm::PHINode>(instruction);

  for (int i = 0; i < phi_inst->getNumIncomingValues(); i++) {
    llvm::Value *operand_i = phi_inst->getIncomingValue(i);

    SLIMOperand *slim_operand_i =
        OperandRepository::getOrCreateSLIMOperand(operand_i);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    // if (slim_operand_i->isPointerVariable()) {
    if (slim_operand_i->isPointerInLLVM()) {
      this->operands.push_back(std::make_pair(slim_operand_i, 1));
    } else {
      this->operands.push_back(std::make_pair(slim_operand_i, 0));
    }
  }
}

void PhiInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = phi(";

  for (int i = 0; i < this->operands.size(); i++) {
    this->operands[i].first->printOperand(llvm::outs());

    if (i != this->operands.size() - 1) {
      llvm::outs() << ", ";
    } else {
      llvm::outs() << ")\n";
    }
  }
}

// Select instruction
SelectInstruction::SelectInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to SELECT
  this->instruction_type = InstructionType::SELECT;

  this->is_expression_assignment = true;

  mapAllOperandsToSLIM(0);
}

void SelectInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::SelectInst *select_instruction;

  if (select_instruction =
          llvm::dyn_cast<llvm::SelectInst>(this->instruction)) {
    // Has three operands : condition, true-operand (operand that will be
    // assigned when the condition is true) and false-operand (operand that
    // will be assigned when the condition is false)
    SLIMOperand *condition = this->operands[0].first;

    SLIMOperand *true_operand = this->operands[1].first;

    SLIMOperand *false_operand = this->operands[2].first;

    llvm::outs() << this->result.first->getValue()->getName() << " = ";

    // Print the condition operand
    condition->printOperand(llvm::outs());

    llvm::outs() << " ? ";

    // Print the "true" operand
    true_operand->printOperand(llvm::outs());

    llvm::outs() << " : ";

    // Print the "false" operand
    false_operand->printOperand(llvm::outs());

    llvm::outs() << "\n";
  } else {
    llvm_unreachable("[SelectInstruction Error] The corresponding LLVM "
                     "instruction is not a select instruction!");
  }
}

// Freeze instruction
FreezeInstruction::FreezeInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to FREEZE
  this->instruction_type = InstructionType::FREEZE;

  mapResultOperandToSLIM(0);

  // Has only single operand
  llvm::Value *operand = instruction->getOperand(0);

  SLIMOperand *slim_operand =
      OperandRepository::getOrCreateSLIMOperand(operand);

  // 0 represents that either it is a constant or the indirection level is not
  // relevant
  this->operands.push_back(std::make_pair(slim_operand, 0));
}

void FreezeInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << this->result.first->getValue()->getName() << " = freeze(";

  llvm::Value *operand = this->operands[0].first->getValue();

  llvm::outs() << operand->getName();

  llvm::outs() << ")\n";
}

// Call instruction
CallInstruction::CallInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to CALL
  this->instruction_type = InstructionType::CALL;

  llvm::CallInst *call_instruction;

  if (call_instruction = llvm::dyn_cast<llvm::CallInst>(this->instruction)) {
    llvm::Value *result_operand = (llvm::Value *)call_instruction;

    // Store the callee function (returns NULL if it is an indirect call)
    this->callee_function = call_instruction->getCalledFunction();

    if (!this->callee_function) {
      this->callee_function = llvm::dyn_cast<llvm::Function>(
          call_instruction->getCalledOperand()->stripPointerCasts());
    }

    if (!this->callee_function) {
      if (call_instruction->isIndirectCall()) {
        this->indirect_call = true;
        llvm::Value *called_operand = call_instruction->getCalledOperand();
        this->indirect_call_operand =
            OperandRepository::getOrCreateSLIMOperand(called_operand);

      } else {
        llvm_unreachable(
            "[CallInstruction Error] This call instruction is neither "
            "a direct call not an indirect call (unexpected error)!");
      }
    }

    if (!this->indirect_call) {
      // Check if this call instruction corresponds to either scanf,
      // sscanf, or fscanf
      SLIM_DEBUG(llvm::outs() << "\nIP stmt detected!!\n");
      if (this->callee_function->getName() == "__isoc99_sscanf") {
        SLIM_DEBUG(llvm::outs() << "\nSscanf detected!!\n");
        this->input_statement_type = InputStatementType::SSCANF;
        // this->is_input_statement = true;
        this->starting_input_args_index = 2;

        SLIM_DEBUG(llvm::outs()
                   << "\nINPUT STATEMENT SSCANF OPERANDS COMING RIGHT UP!:\n");
        SLIM_DEBUG(llvm::outs() << this->getNumOperands() << "\n");
        // this->printInstruction();
      } else if (this->callee_function->getName() == "__isoc99_fscanf") {
        SLIM_DEBUG(llvm::outs() << "\nFscanf detected!!\n");
        this->input_statement_type = InputStatementType::FSCANF;
        // this->is_input_statement = true;
        this->starting_input_args_index = 2;

        SLIM_DEBUG(llvm::outs()
                   << "\nINPUT STATEMENT FSCANF OPERANDS COMING RIGHT UP!:\n");
        SLIM_DEBUG(llvm::outs() << this->getNumOperands() << "\n");
        // this->printInstruction();
      } else if (this->callee_function->getName() == "__isoc99_scanf") {
        SLIM_DEBUG(llvm::outs() << "\njust scanf detected!!\n");
        this->input_statement_type = InputStatementType::SCANF;
        // this->is_input_statement = true;
        this->starting_input_args_index = 1;

        SLIM_DEBUG(llvm::outs()
                   << "\nINPUT STATEMENT SCANF OPERANDS COMING RIGHT UP!:\n");
        SLIM_DEBUG(llvm::outs() << this->getNumOperands() << "\n");
        // this->printInstruction();
      } else if (this->callee_function->getName() == "malloc" or
                 this->callee_function->getName() == "calloc" or
                 this->callee_function->getName() == "realloc") {
        this->isDynamicAllocation = true;
      }

      SLIMOperand *result_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(result_operand, false,
                                                    this->callee_function);
      this->result = std::make_pair(result_slim_operand, 0);
      OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

      for (auto arg = this->callee_function->arg_begin();
           arg != this->callee_function->arg_end(); arg++) {
        this->formal_arguments_list.push_back(arg);
      }
    } else {
      SLIMOperand *result_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(result_operand);
      this->result = std::make_pair(result_slim_operand, 0);
      OperandRepository::setSLIMOperand(result_operand, result_slim_operand);
    }

    for (unsigned i = 0; i < call_instruction->arg_size(); i++) {
      // Get the ith argument of the call instruction
      llvm::Value *arg_i = call_instruction->getArgOperand(i);

      // Check whether the corresponding SLIM operand already exists or
      // not (and store, if it exists)
      SLIMOperand *arg_i_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(arg_i);

      // Push the argument operand in the operands vector
      // The indirection level is not relevant so it is 0 for now
      this->operands.push_back(std::make_pair(arg_i_slim_operand, 0));
    }
  } else {
    llvm_unreachable("[CallInstruction Error] The underlying LLVM "
                     "instruction is not a call instruction!");
  }
}

// Returns whether the call is an indirect call or a direct call
bool CallInstruction::isIndirectCall() { return this->indirect_call; }

// Returns the indirect call SLIM operand
SLIMOperand *CallInstruction::getIndirectCallOperand() {
  return this->indirect_call_operand;
}

// Return the callee function
llvm::Function *CallInstruction::getCalleeFunction() {
  return this->callee_function;
}

// Returns the number of formal arguments in this call
unsigned CallInstruction::getNumFormalArguments() {
  return this->formal_arguments_list.size();
}

// Returns the formal argument at a particular index
llvm::Argument *CallInstruction::getFormalArgument(unsigned index) {
  // The index should be not be out-of-bounds
  assert(index >= 0 && index < this->getNumFormalArguments());

  return this->formal_arguments_list[index];
}

void CallInstruction::printInstruction() {
  const std::string dbg_declare = "llvm.dbg.declare";
  const std::string dbg_value = "llvm.dbg.value";
  
  // SCANF ESCAPE
  if (this->callee_function &&
      this->callee_function->getName() == "__isoc99_scanf" &&
      this->result.first->getValue()->getName() == "call")
    return;

  // If callee_function is NULL then it is an indirect calll
  if (!this->isIndirectCall()) {
    llvm::StringRef callee_function_name = this->callee_function->getName();

    if (callee_function_name.str() == (dbg_declare) ||
        callee_function_name.str() == (dbg_value)) {
      return;
    }
  }
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[src: " << this->getSourceLineNumber() << "] ";
  }
  if (!this->result.first->getValue()->getName().empty()) {
    llvm::outs() << this->result.first->getValue()->getName() << " = call ";
  }
  // If callee_function is NULL then it is an indirect calll
  if (!this->isIndirectCall()) {
    llvm::outs() << this->callee_function->getName() << "(";
  } else {
    llvm::outs() << "(";

    this->getIndirectCallOperand()->printOperand(llvm::outs());

    llvm::outs() << ") (";
  }
  if (this->operands.empty()) {
    llvm::outs() << ")\n";
  }

  if (this->callee_function &&
      this->callee_function->getName() == "__isoc99_scanf") {
    llvm::outs() << this->result.first->getValue()->getName() << ")\n";
    return;
  }
  for (int i = 0; i < this->operands.size(); i++) {
    this->operands[i].first->printOperand(llvm::outs());

    if (i != this->operands.size() - 1) {
      llvm::outs() << ", ";
    } else {
      llvm::outs() << ")\n";
    }
  }
}

// Variable argument instruction
VarArgInstruction::VarArgInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to VAR_ARG
  this->instruction_type = InstructionType::VAR_ARG;

  mapAllOperandsToSLIM(0);
}

void VarArgInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->printLLVMInstruction();
}

// Landingpad instruction
LandingpadInstruction::LandingpadInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to LANDING_PAD
  this->instruction_type = InstructionType::LANDING_PAD;

  mapAllOperandsToSLIM(0);
}

void LandingpadInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->printLLVMInstruction();
}

// Catchpad instruction
CatchpadInstruction::CatchpadInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to CATCH_PAD
  this->instruction_type = InstructionType::CATCH_PAD;

  mapAllOperandsToSLIM(0);
}

void CatchpadInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->printLLVMInstruction();
}

// Cleanuppad instruction
CleanuppadInstruction::CleanuppadInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to CLEANUP_PAD
  this->instruction_type = InstructionType::CLEANUP_PAD;

  mapAllOperandsToSLIM(0);
}

void CleanuppadInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->printLLVMInstruction();
}

// Return instruction
ReturnInstruction::ReturnInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to RETURN
  this->instruction_type = InstructionType::RETURN;

  llvm::ReturnInst *return_inst;

  if (return_inst = llvm::dyn_cast<llvm::ReturnInst>(this->instruction)) {
    llvm::Value *temp_return_value = return_inst->getReturnValue();

    SLIMOperand *slim_return_value =
        OperandRepository::getOrCreateSLIMOperand(temp_return_value);

    this->return_value = slim_return_value;

    this->operands.push_back(std::make_pair(slim_return_value, 0));
  } else {
    llvm_unreachable("[ReturnInstruction Error] The underlying LLVM "
                     "instruction is not a return instruction!");
  }
}

// Returns the SLIMOperand
SLIMOperand *ReturnInstruction::getReturnOperand() {
  return this->return_value;
}

// Returns the Value * return operand
llvm::Value *ReturnInstruction::getReturnValue() {
  return this->return_value->getValue();
}

void ReturnInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << "return ";

  llvm::Value *value = this->getReturnValue();

  if (!value) {
    llvm::outs() << "\n";
    return;
  }
  SLIMOperand *value_slim_operand =
      OperandRepository::getOrCreateSLIMOperand(value);

  // Print the return operand
  value_slim_operand->printOperand(llvm::outs());

  llvm::outs() << "\n";
}

// Branch instruction
BranchInstruction::BranchInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to BRANCH
  this->instruction_type = InstructionType::BRANCH;

  llvm::BranchInst *branch_instruction;

  if (branch_instruction =
          llvm::dyn_cast<llvm::BranchInst>(this->instruction)) {
    if (branch_instruction->isUnconditional()) {
      this->is_conditional = false;
    } else {
      this->is_conditional = true;

      llvm::Value *condition_operand = branch_instruction->getCondition();

      SLIMOperand *condition_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(condition_operand);

      this->operands.push_back(std::make_pair(condition_slim_operand, 0));
    }
  } else {
    llvm_unreachable("[BranchInstruction Error] The underlying LLVM "
                     "instruction is not a branch instruction!");
  }
}

void BranchInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::BranchInst *branch_inst;

  llvm::outs() << "branch ";

  if (branch_inst = llvm::dyn_cast<llvm::BranchInst>(this->instruction)) {
    if (this->is_conditional) {
      // The branch instruction is conditional, meaning there is only one
      // successor

      llvm::Value *condition = branch_inst->getCondition();

      llvm::outs() << "(" << condition->getName() << ") ";
      llvm::outs() << branch_inst->getSuccessor(0)->getName();
      llvm::outs() << ", ";
      llvm::outs() << branch_inst->getSuccessor(1)->getName();
    } else {
      llvm::outs() << branch_inst->getSuccessor(0)->getName();
    }

    llvm::outs() << "\n";
  } else {
    llvm_unreachable("[BranchInstruction Error] The underlying LLVM "
                     "instruction is not a branch instruction!");
  }
}

// Switch instruction
SwitchInstruction::SwitchInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to SWITCH
  this->instruction_type = InstructionType::SWITCH;

  llvm::SwitchInst *switch_instruction;

  if (switch_instruction =
          llvm::dyn_cast<llvm::SwitchInst>(this->instruction)) {
    // First operand is the comparison value
    llvm::Value *comparison_value = switch_instruction->getCondition();

    SLIMOperand *comparison_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(comparison_value);

    // Set the condition value
    this->condition_value = comparison_slim_operand;

    this->operands.push_back(std::make_pair(condition_value, 0));

    // Set the default destination
    this->default_case = switch_instruction->getDefaultDest();

    for (unsigned int i = 0; i <= switch_instruction->getNumCases(); i++) {
      llvm::BasicBlock *case_destination = switch_instruction->getSuccessor(i);

      llvm::ConstantInt *case_value =
          switch_instruction->findCaseDest(case_destination);

      if (case_value) {
        this->other_cases.push_back(
            std::make_pair(case_value, case_destination));
      }
    }
  } else {
    llvm_unreachable("[SwitchInstruction Error] The underlying LLVM "
                     "instruction is not a switch instruction!");
  }
}

SLIMOperand *SwitchInstruction::getConditionOperand() {
  return this->condition_value;
}

llvm::BasicBlock *SwitchInstruction::getDefaultDestination() {
  return this->default_case;
}

unsigned SwitchInstruction::getNumberOfCases() {
  return this->other_cases.size();
}

llvm::ConstantInt *SwitchInstruction::getConstantOfCase(unsigned case_number) {
  return this->other_cases[case_number].first;
}

llvm::BasicBlock *
SwitchInstruction::getDestinationOfCase(unsigned case_number) {
  return this->other_cases[case_number].second;
}

void SwitchInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::SwitchInst *switch_instruction;

  if (switch_instruction =
          llvm::dyn_cast<llvm::SwitchInst>(this->instruction)) {
    llvm::outs() << "switch(";

    llvm::Value *comparison_value = this->getConditionOperand()->getValue();

    if (llvm::isa<llvm::Constant>(comparison_value)) {
      if (llvm::isa<llvm::ConstantInt>(comparison_value)) {
        llvm::ConstantInt *constant_int =
            llvm::cast<llvm::ConstantInt>(comparison_value);

        llvm::outs() << constant_int->getSExtValue();
      } else {
        llvm_unreachable("[SwitchInstruction Error] The comparison "
                         "value is not an integer!");
      }
    } else {
      llvm::outs() << comparison_value->getName();
    }

    llvm::outs() << ") {\n";

    // Print non-default cases
    for (unsigned i = 0; i < this->getNumberOfCases(); i++) {
      llvm::ConstantInt *case_value = this->getConstantOfCase(i);

      llvm::outs() << "case " << case_value->getSExtValue() << ": branch-to ";

      llvm::BasicBlock *case_destination = this->getDestinationOfCase(i);

      llvm::outs() << case_destination->getName() << "; break; \n";
    }

    // Print default case
    llvm::outs() << "default: branch-to "
                 << this->getDefaultDestination()->getName() << "\n";
  } else {
    llvm_unreachable("[SwitchInstruction Error] The underlying LLVM "
                     "instruction is not a switch instruction!");
  }

  llvm::outs() << "}\n";
}

// Indirect branch instruction
IndirectBranchInstruction::IndirectBranchInstruction(
    llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to INDIRECT_BRANCH
  this->instruction_type = InstructionType::INDIRECT_BRANCH;

  llvm::IndirectBrInst *indirect_br_inst;

  if (indirect_br_inst =
          llvm::dyn_cast<llvm::IndirectBrInst>(this->instruction)) {
    this->address = indirect_br_inst->getAddress();

    for (unsigned i = 0; i < indirect_br_inst->getNumDestinations(); i++) {
      llvm::BasicBlock *possible_destination =
          indirect_br_inst->getDestination(i);
      this->possible_destinations.push_back(possible_destination);
    }
  } else {
    llvm_unreachable(
        "[IndirectBranchInstruction Error] The underlying LLVM instruction "
        "is not a indirect branch instruction!");
  }
}

llvm::Value *IndirectBranchInstruction::getBranchAddress() {
  return this->address;
}

unsigned IndirectBranchInstruction::getNumPossibleDestinations() {
  return this->possible_destinations.size();
}

llvm::BasicBlock *
IndirectBranchInstruction::getPossibleDestination(unsigned index) {
  // Check whether the index lies in the correct bounds or not
  assert(index >= 0 && index < this->possible_destinations.size());

  return this->possible_destinations[index];
}

void IndirectBranchInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << "indirect branch to " << this->getBranchAddress()->getName();

  llvm::outs() << " : [";

  unsigned total_possible_dests = this->getNumPossibleDestinations();

  for (unsigned i = 0; i < total_possible_dests; i++) {
    llvm::outs() << this->getPossibleDestination(i)->getName();

    if (i != total_possible_dests - 1) {
      llvm::outs() << ", ";
    }
  }

  llvm::outs() << "]\n";
}

// Invoke instruction
InvokeInstruction::InvokeInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to INVOKE
  this->instruction_type = InstructionType::INVOKE;

  llvm::InvokeInst *invoke_inst;

  if (invoke_inst = llvm::dyn_cast<llvm::InvokeInst>(this->instruction)) {
    llvm::Value *result_operand = (llvm::Value *)invoke_inst;

    SLIMOperand *result_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(result_operand);

    this->result = std::make_pair(result_slim_operand, 0);

    OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

    // Store the callee function (returns NULL if it is an indirect call)
    this->callee_function = invoke_inst->getCalledFunction();

    if (!this->callee_function) {
      this->callee_function = llvm::dyn_cast<llvm::Function>(
          invoke_inst->getCalledOperand()->stripPointerCasts());
    }

    this->indirect_call = false;
    this->indirect_call_operand = NULL;

    if (!this->callee_function) {
      this->indirect_call = true;
      llvm::Value *called_operand = invoke_inst->getCalledOperand();
      this->indirect_call_operand =
          OperandRepository::getOrCreateSLIMOperand(called_operand);
    }

    // Store the normal destination
    this->normal_destination = invoke_inst->getNormalDest();

    // Store the exception destination (destination in case of exception
    // handling)
    this->exception_destination = invoke_inst->getUnwindDest();

    for (unsigned i = 0; i < invoke_inst->arg_size(); i++) {
      // Get the ith argument of the invoke instruction
      llvm::Value *arg_i = invoke_inst->getArgOperand(i);

      // Check whether the corresponding SLIM operand already exists or
      // not (and store, if it exists)
      SLIMOperand *arg_i_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(arg_i);

      // Push the argument operand in the operands vector
      // The indirection level is not relevant so it is 0 for now
      this->operands.push_back(std::make_pair(arg_i_slim_operand, 0));
    }
  } else {
    llvm_unreachable("[InvokeInstruction Error] The underlying LLVM "
                     "instruction is not an invoke instruction!");
  }
}

// Returns whether the call is an indirect call or not
bool InvokeInstruction::isIndirectCall() { return this->indirect_call; }

// Returns a non-NULL SLIMOperand if the call is an indirect call
SLIMOperand *InvokeInstruction::getIndirectCallOperand() {
  return this->indirect_call_operand;
}

// Returns a non-NULL llvm::Function* if the call is a direct call
llvm::Function *InvokeInstruction::getCalleeFunction() {
  return this->callee_function;
}

llvm::BasicBlock *InvokeInstruction::getNormalDestination() {
  return this->normal_destination;
}

llvm::BasicBlock *InvokeInstruction::getExceptionDestination() {
  return this->exception_destination;
}

void InvokeInstruction::printInstruction() {
  const std::string dbg_declare = "llvm.dbg.declare";
  const std::string dbg_value = "llvm.dbg.value";

  // If callee_function is NULL then it is an indirect calll
  if (!this->isIndirectCall()) {
    llvm::StringRef callee_function_name = this->callee_function->getName();

    if (callee_function_name.str() == (dbg_declare) ||
        callee_function_name.str() == (dbg_value)) {
      return;
    }
  }

  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  if (!this->result.first->getValue()->getName().empty())
    llvm::outs() << this->result.first->getValue()->getName() << " = invoke ";

  // If callee_function is NULL then it is an indirect calll
  if (!this->isIndirectCall()) {
    llvm::outs() << this->callee_function->getName() << "(";
  } else {
    llvm::outs() << "(";

    this->getIndirectCallOperand()->printOperand(llvm::outs());

    llvm::outs() << ") (";
  }

  if (this->operands.empty()) {
    llvm::outs() << ")\n";
  }

  for (int i = 0; i < this->operands.size(); i++) {
    this->operands[i].first->printOperand(llvm::outs());

    if (i != this->operands.size() - 1) {
      llvm::outs() << ", ";
    } else {
      llvm::outs() << ") ";
    }
  }

  llvm::outs() << this->getNormalDestination()->getName() << ", "
               << this->getExceptionDestination()->getName();

  llvm::outs() << "\n";
}

// Callbr instruction
CallbrInstruction::CallbrInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to CALL_BR
  this->instruction_type = InstructionType::CALL_BR;

  llvm::CallBrInst *callbr_instruction;

  if (callbr_instruction =
          llvm::dyn_cast<llvm::CallBrInst>(this->instruction)) {
    llvm::Value *result_operand = (llvm::Value *)callbr_instruction;

    SLIMOperand *result_slim_operand =
        OperandRepository::getOrCreateSLIMOperand(result_operand);

    this->result = std::make_pair(result_slim_operand, 0);

    OperandRepository::setSLIMOperand(result_operand, result_slim_operand);

    // Store the callee function
    this->callee_function = callbr_instruction->getCalledFunction();

    // Store the default destination
    this->default_destination = callbr_instruction->getDefaultDest();

    // Store the indirect destinations
    for (unsigned i = 0; i < callbr_instruction->getNumIndirectDests(); i++) {
      this->indirect_destinations.push_back(
          callbr_instruction->getIndirectDest(i));
    }

    for (unsigned i = 0; i < callbr_instruction->arg_size(); i++) {
      // Get the ith argument of the call instruction
      llvm::Value *arg_i = callbr_instruction->getArgOperand(i);

      SLIMOperand *arg_i_slim_operand =
          OperandRepository::getOrCreateSLIMOperand(arg_i);

      // Push the argument operand in the operands vector
      // The indirection level is not relevant so it is 0 for now
      this->operands.push_back(std::make_pair(arg_i_slim_operand, 0));
    }
  } else {
    llvm_unreachable("[CallbrInstruction Error] The underlying LLVM "
                     "instruction is not a call instruction!");
  }
}

llvm::Function *CallbrInstruction::getCalleeFunction() {
  return this->callee_function;
}

llvm::BasicBlock *CallbrInstruction::getDefaultDestination() {
  return this->default_destination;
}

unsigned CallbrInstruction::getNumIndirectDestinations() {
  return this->indirect_destinations.size();
}

llvm::BasicBlock *CallbrInstruction::getIndirectDestination(unsigned index) {
  assert(index >= 0 && index < this->getNumIndirectDestinations());

  return this->indirect_destinations[index];
}

void CallbrInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  if (!this->result.first->getValue()->getName().empty())
    llvm::outs() << this->result.first->getValue()->getName() << " = callbr ";

  const std::string dbg_declare = "llvm.dbg.declare";
  const std::string dbg_value = "llvm.dbg.value";

  llvm::StringRef callee_function_name = this->callee_function->getName();

  if (callee_function_name.str() == (dbg_declare) ||
      callee_function_name.str() == (dbg_value))
    return;

  llvm::outs() << this->callee_function->getName() << "(";

  for (int i = 0; i < this->operands.size(); i++) {
    llvm::Value *operand_i = this->operands[i].first->getValue();

    if (llvm::isa<llvm::Constant>(operand_i)) {
      llvm::GEPOperator *gep_operator;

      if (llvm::isa<llvm::ConstantInt>(operand_i)) {
        llvm::ConstantInt *constant_int =
            llvm::cast<llvm::ConstantInt>(operand_i);

        llvm::outs() << constant_int->getSExtValue();
      } else if (llvm::isa<llvm::ConstantFP>(operand_i)) {
        llvm::ConstantFP *constant_float =
            llvm::cast<llvm::ConstantFP>(operand_i);

        llvm::outs() << constant_float->getValueAPF().convertToFloat();
      } else if (gep_operator = llvm::dyn_cast<llvm::GEPOperator>(operand_i)) {
        llvm::Value *operand_i = gep_operator->getOperand(0);

        llvm::outs() << operand_i->getName();
      } else if (llvm::isa<llvm::GlobalValue>(operand_i)) {
        llvm::outs() << llvm::cast<llvm::GlobalValue>(operand_i)->getName();
      } else {
        // llvm_unreachable("[CallInstruction Error] Unexpected
        // constant!\n");
        llvm::outs() << "[InvokeInstruction Error] Unexpected constant!\n";
      }
    } else {
      llvm::outs() << operand_i->getName();
    }

    if (i != this->operands.size() - 1) {
      llvm::outs() << ", ";
    } else {
      llvm::outs() << ")";
    }
  }

  llvm::outs() << this->getDefaultDestination()->getName() << ", [";

  for (unsigned i = 0; i < this->getNumIndirectDestinations(); i++) {
    llvm::outs() << this->getIndirectDestination(i)->getName();

    if (i != this->getNumIndirectDestinations() - 1) {
      llvm::outs() << ", ";
    }
  }

  llvm::outs() << "]\n";
}

// Resume instruction - resumes propagation of an existing exception
ResumeInstruction::ResumeInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to RESUME
  this->instruction_type = InstructionType::RESUME;

  llvm::ResumeInst *resume_inst;

  if (resume_inst = llvm::dyn_cast<llvm::ResumeInst>(this->instruction)) {
    llvm::Value *operand = resume_inst->getValue();

    SLIMOperand *slim_operand =
        OperandRepository::getOrCreateSLIMOperand(operand);

    // 0 represents that either it is a constant or the indirection level is
    // not relevant
    this->operands.push_back(std::make_pair(slim_operand, 0));
  } else {
    llvm_unreachable("[ResumeInstruction Error] The underlying LLVM "
                     "instruction is not a resume instruction!");
  }
}

void ResumeInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  llvm::outs() << "resume ";

  llvm::outs() << this->operands[0].first->getValue()->getName();
}

// Catchswitch instruction
CatchswitchInstruction::CatchswitchInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to CATCH_SWITCH
  this->instruction_type = InstructionType::CATCH_SWITCH;

  if (llvm::isa<llvm::CatchSwitchInst>(this->instruction)) {
  } else
    llvm_unreachable("[CatchswitchInstruction Error] The underlying LLVM "
                     "instruction is not a catchswitch instruction!");
}

void CatchswitchInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->printLLVMInstruction();
}

// Catchreturn instruction
CatchreturnInstruction::CatchreturnInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  if (llvm::isa<llvm::CatchReturnInst>(this->instruction)) {
  } else
    llvm_unreachable("[CatchreturnInstruction Error] The underlying LLVM "
                     "instruction is not a catchreturn instruction!");
}

void CatchreturnInstruction::printInstruction() {
  this->printLLVMInstruction();
}

// CleanupReturn instruction
CleanupReturnInstruction::CleanupReturnInstruction(
    llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to CLEANUP_RETURN
  this->instruction_type = InstructionType::CLEANUP_RETURN;

  if (llvm::isa<llvm::CleanupReturnInst>(this->instruction)) {
  } else
    llvm_unreachable("[CleanupReturnInstruction Error] The underlying LLVM "
                     "instruction is not a cleanupreturn instruction!");
}

void CleanupReturnInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->printLLVMInstruction();
}

// Unreachable instruction
UnreachableInstruction::UnreachableInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to UNREACHABLE
  this->instruction_type = InstructionType::UNREACHABLE;

  if (llvm::isa<llvm::UnreachableInst>(this->instruction)) {
  } else
    llvm_unreachable("[UnreachableInstruction Error] The underlying LLVM "
                     "instruction is not an unreachable instruction!");
}

void UnreachableInstruction::printInstruction() {
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
  }

  this->printLLVMInstruction();
}

// Other instruction (currently not supported)
OtherInstruction::OtherInstruction(llvm::Instruction *instruction)
    : BaseInstruction(instruction) {
  // Set the instruction type to OTHER
  this->instruction_type = InstructionType::OTHER;

  SLIMOperand *result_slim_operand = OperandRepository::getOrCreateSLIMOperand(
      (llvm::Value *)this->instruction);
  this->result = std::make_pair(result_slim_operand, 0);

  for (int i = 0; i < this->instruction->getNumOperands(); i++) {
    SLIMOperand *temp_slim_operand = OperandRepository::getOrCreateSLIMOperand(
        this->instruction->getOperand(i));
    this->operands.push_back(std::make_pair(temp_slim_operand, 0));
  }
}

void OtherInstruction::printInstruction() {
  llvm::outs() << "Not supported!\n";
}

//-------------------------------------------PRINTING TO DOT
// FILE----------------------------------------------

// Prints the load instruction to DOT file

void AllocaInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                      bool isMMA) {}

void LoadInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                    bool isMMA) {
  assert(this != NULL);
  // llvm::outs() << "[Test] Printing the instruction..\n";
  // this->printInstruction();
  // llvm::outs() << "\n";

  // *dotFile << //label for DOT coming up soon...
  // *dotFile << "[label=\"";
  *dotFile << "<";

  this->result.first->printOperandDot(dotFile);

  *dotFile << ", " << this->result.second << "> = ";

  *dotFile << "<";

  this->operands[0].first->printOperandDot(dotFile);

  *dotFile << ", " << this->operands[0].second << ">";

  *dotFile << "\n"; // *dotFile << "\"]\n";  //edit BB

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void StoreInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                     bool isMMA) {
  assert(this != NULL);
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    /* llvm::outs()<<"FLAG-A:"<<flagInstr;
    if((flagInstr) != this->getSourceLineNumber())  //edit1
        *dotFile << this->getSourceLineNumber()<<"\n"; */  //edit BB

    // *dotFile << this->getSourceLineNumber()<<" ";   //changed
    // temp2//*dotFile << "[" << this->getSourceLineNumber() << "] ";
    // //changed temp2
  }

  // *dotFile << "[label=\"";
  *dotFile << "<";
  this->result.first->printOperandDot(dotFile);

  *dotFile << ", " << this->result.second << "> = ";

  *dotFile << "<";

  this->operands[0].first->printOperandDot(dotFile);

  *dotFile << ", " << this->operands[0].second << ">";

  *dotFile << "\n"; // *dotFile << "\"]\n";   //edit BB

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

// print Fence instruction to DOT file
void FenceInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                     bool isMMA) {
  assert(this != NULL);

  this->printLLVMInstructionDot(dotFile); // modSB ;

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

// print AtomicCompareChangeInstruction to DOT file
void AtomicCompareChangeInstruction::printInstrDot(
    llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA) {
  assert(this != NULL);
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    /* llvm::outs()<<"FLAG-A:"<<flagInstr;
    if((flagInstr) != this->getSourceLineNumber())  //edit1
        *dotFile << this->getSourceLineNumber()<<"\n"; */  //edit BB

    // *dotFile << this->getSourceLineNumber()<<" ";   //changed
    // temp2//*dotFile << "[" << this->getSourceLineNumber() << "] ";
    // //changed temp2
  }

  this->printLLVMInstructionDot(dotFile); // modSB ;

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void AtomicModifyMemInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                               bool isPhi, bool isMMA) {
  assert(this != NULL);
  *dotFile << "INSIDE ATOMIC";

  this->printLLVMInstructionDot(dotFile); // modSB ;

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

// print instr to dot file

void GetElementPtrInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                             bool isPhi, bool isMMA) {
  assert(this != NULL);
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    // llvm::outs() << "[" << this->getSourceLineNumber() << "] ";
    // *dotFile << "\n"<<this->getSourceLineNumber();
  }

  if (this->result.first->getValue()) {
    // llvm::outs() << "<";

    // this->result.first->printOperand(llvm::outs());

    // llvm::outs() << ", " << this->result.second << ">";

    // llvm::outs() << " = ";

    *dotFile << "<";

    // this->result.first->printOperand(llvm::outs());
    this->result.first->printOperandDot(dotFile);

    *dotFile << ", " << this->result.second << ">";

    *dotFile << " = ";
  }

  llvm::GetElementPtrInst *get_element_ptr;

  if (get_element_ptr =
          llvm::dyn_cast<llvm::GetElementPtrInst>(this->instruction)) {
    // llvm::outs() << "<";

    // llvm::outs() << get_element_ptr->getPointerOperand()->getName();

    *dotFile << "<";
    std::string temp =
        (std::string)get_element_ptr->getPointerOperand()->getName();
    *dotFile << temp;

    for (int i = 1; i < get_element_ptr->getNumOperands(); i++) {
      llvm::Value *index_val = get_element_ptr->getOperand(i);

      if (llvm::isa<llvm::Constant>(index_val)) {
        if (llvm::isa<llvm::ConstantInt>(index_val)) {
          llvm::ConstantInt *constant_int =
              llvm::cast<llvm::ConstantInt>(index_val);

          // llvm::outs() << "[" << constant_int->getSExtValue() <<
          // "]";
          *dotFile << "[" << constant_int->getSExtValue() << "]";
        } else {
          llvm_unreachable("[GetElementPtrInstruction Error] The index is a "
                           "constant but not an integer constant!");
        }
      } else {
        temp = (std::string)index_val->getName();
        // llvm::outs() << "[" << index_val->getName() << "]";
        *dotFile << "[" << temp << "]";
      }
    }

    // The indirection level (0) can be fetched from the individual
    // indirection of indices as well llvm::outs() << ", 0>"; llvm::outs()
    // << "\n";
    *dotFile << ", 0>";
    *dotFile << "\n";
  } else {
    llvm_unreachable("[GetElementPtrInstruction Error] The underlying LLVM "
                     "instruction is not a getelementptr instruction!");
  }

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void FPNegationInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                          bool isPhi, bool isMMA) {
  assert(this != NULL);

  this->result.first->printOperandDot(dotFile);

  *dotFile << " = ";

  this->operands[0].first->printOperandDot(dotFile);
  // Value *operand = this->operands[0].first->getValue();

  // if (isa<Constant>(operand))
  // {
  //     if (isa<ConstantFP>(operand))
  //     {
  //         ConstantFP *constant_fp = cast<ConstantFP>(operand);

  //         *dotFile << constant_fp->getValueAPF().convertToFloat();
  //     }
  //     else
  //     {
  //         llvm_unreachable("[FPNegationInstruction Error] The operand is a
  //         constant but not a floating-point constant!");
  //     }
  // }
  // else
  // {
  //     *dotFile << "-" << operand->getName();
  // }

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void BinaryOperation::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                    bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  for (int i = 0; i < this->operands.size(); i++) {
    this->operands[i].first->printOperandDot(dotFile);

    if (i != this->operands.size() - 1) {
      switch (this->getOperationType()) {
      case ADD:
        *dotFile << " + ";
        break;
      case SUB:
        *dotFile << " - ";
        break;
      case MUL:
        *dotFile << " * ";
        break;
      case DIV:
        *dotFile << " / ";
        break;
      case REM:
        *dotFile << " % ";
        break;

      case SHIFT_LEFT:
        *dotFile << " << ";
        break;
      case LOGICAL_SHIFT_RIGHT:
        *dotFile << " >>> ";
        break;
      case ARITHMETIC_SHIFT_RIGHT:
        *dotFile << " >> ";
        break;
      case BITWISE_AND:
        *dotFile << " & ";
        break;
      case BITWISE_OR:
        *dotFile << " | ";
        break;
      case BITWISE_XOR:
        *dotFile << " ^ ";
        break;

      default:
        llvm_unreachable("[BinaryOperation Error] Unexpected binary "
                         "operation type!");
      }
    } else {
      *dotFile << "\n";
    }
  }

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void ExtractElementInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                              bool isPhi, bool isMMA) {
  assert(this != NULL);

  this->result.first->printOperandDot(dotFile);

  *dotFile << " = ";

  // Value *operand_0 = this->operands[0].first->getValue();
  this->operands[0].first->printOperandDot(dotFile);

  *dotFile << "[";

  // Value *operand_1 = this->operands[1].first->getValue();

  this->operands[1].first->printOperandDot(dotFile);

  *dotFile << "]\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void InsertElementInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                             bool isPhi, bool isMMA) {
  assert(this != NULL);

  this->result.first->printOperandDot(dotFile);

  *dotFile << " = ";

  this->operands[0].first->printOperandDot(dotFile);

  *dotFile << ".insert(";

  this->operands[1].first->printOperandDot(dotFile);

  *dotFile << ", ";

  this->operands[2].first->printOperandDot(dotFile);

  *dotFile << ")\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

// ShuffleVector instruction
void ShuffleVectorInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                             bool isPhi, bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  *dotFile << "shufflevector(";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  temp = (std::string)operand_0->getName();
  *dotFile << temp << ", ";

  llvm::Value *operand_1 = this->operands[1].first->getValue();

  temp = (std::string)operand_1->getName();
  *dotFile << temp << ", ";

  llvm::Value *operand_2 = this->operands[2].first->getValue();

  temp = (std::string)operand_2->getName();
  *dotFile << temp << ")\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

// Operations for aggregates (structure and array) stored in registers

// ExtractValue instruction
void ExtractValueInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                            bool isPhi, bool isMMA) {
  assert(this != NULL);
  std::string temp;

  this->result.first->printOperandDot(dotFile);

  *dotFile << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  temp = (std::string)operand_0->getName();
  *dotFile << temp;

  for (auto index : this->indices) {
    *dotFile << "[" << index << "]";
  }

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void InsertValueInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                           bool isPhi, bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  llvm::Value *operand_0_aggregate_name = this->operands[0].first->getValue();

  temp = (std::string)operand_0_aggregate_name->getName();
  *dotFile << temp;

  llvm::Value *operand_1_value_to_insert = this->operands[1].first->getValue();

  for (int i = 2; i < this->operands.size(); i++) {
    llvm::Value *operand_i = this->operands[i].first->getValue();

    if (llvm::isa<llvm::ConstantInt>(operand_i)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_i);

      *dotFile << "[" << constant_int->getSExtValue() << "]";
    } else {
      llvm_unreachable("[InsertValueInstruction Error] Unexpected index");
    }
  }

  *dotFile << ".insert(";

  this->operands[1].first->printOperandDot(dotFile);

  *dotFile << ")\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

// Conversion operations

void TruncInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                     bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  *dotFile << "(";

  // this->getResultingType()  //temp change->print(dotFile);
  // temp = (std::string)// this->getResultingType()  //temp change;
  // *dotFile << temp;

  *dotFile << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      *dotFile << constant_int->getSExtValue();
    } else if (llvm::isa<llvm::ConstantFP>(operand_0)) {
      llvm::ConstantFP *constant_float =
          llvm::cast<llvm::ConstantFP>(operand_0);

      *dotFile << constant_float->getValueAPF().convertToFloat();
    } else {
      *dotFile << "[TruncInstruction Error] Unexpected constant!\n";
    }
  } else {
    temp = (std::string)operand_0->getName();
    *dotFile << temp;
  }

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void ZextInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                    bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  *dotFile << "(Zext-";

  // this->getResultingType()  //temp change->print(dotFile);

  *dotFile << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      *dotFile << constant_int->getSExtValue();
    } else if (llvm::isa<llvm::ConstantFP>(operand_0)) {
      llvm::ConstantFP *constant_float =
          llvm::cast<llvm::ConstantFP>(operand_0);

      *dotFile << constant_float->getValueAPF().convertToFloat();
    } else {
      *dotFile << "[ZextInstruction Error] Unexpected constant!\n";
    }
  } else {
    temp = (std::string)operand_0->getName();
    *dotFile << temp;
  }

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void SextInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                    bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  *dotFile << "(SignExt-";

  // this->getResultingType()  //temp change->print(dotFile);

  *dotFile << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      *dotFile << constant_int->getSExtValue();
    } else {
      *dotFile << "[SextInstruction Error] Unexpected constant!\n";
    }
  } else {
    temp = (std::string)operand_0->getName();
    *dotFile << temp;
  }

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void FPExtInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                     bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  *dotFile << "(FPExt-";

  // this->getResultingType()  //temp change->print(dotFile);

  *dotFile << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantFP>(operand_0)) {
      llvm::ConstantFP *constant_float =
          llvm::cast<llvm::ConstantFP>(operand_0);

      *dotFile << constant_float->getValueAPF().convertToFloat();
    } else {
      *dotFile << "[FPExtInstruction Error] Unexpected constant!\n";
    }
  } else {
    temp = (std::string)operand_0->getName();
    *dotFile << temp;
  }

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void FPToIntInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                       bool isPhi, bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  *dotFile << "(";

  // this->getResultingType()  //temp change->print(dotFile);

  *dotFile << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      *dotFile << constant_int->getSExtValue();
    } else if (llvm::isa<llvm::ConstantFP>(operand_0)) {
      llvm::ConstantFP *constant_float =
          llvm::cast<llvm::ConstantFP>(operand_0);

      *dotFile << constant_float->getValueAPF().convertToFloat();
    } else {
      *dotFile << "[FPToIntInstruction Error] Unexpected constant!\n";
    }
  } else {
    temp = (std::string)operand_0->getName();
    *dotFile << temp;
  }

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void IntToFPInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                       bool isPhi, bool isMMA) {
  assert(this != NULL);
  std::string temp;

  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    /* llvm::outs()<<"FLAG-A:"<<flagInstr;
    if((flagInstr) != this->getSourceLineNumber())  //edit1
        *dotFile << this->getSourceLineNumber()<<"\n"; */  //edit BB

    // *dotFile << this->getSourceLineNumber()<<" ";   //changed
    // temp2//*dotFile << "[" << this->getSourceLineNumber() << "] ";
    // //changed temp2
  }

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  *dotFile << "(";

  // this->getResultingType()  //temp change->print(dotFile);

  *dotFile << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      *dotFile << constant_int->getSExtValue();
    } else {
      *dotFile << "[IntToFPInstruction Error] Unexpected constant!\n";
    }
  } else {
    temp = (std::string)operand_0->getName();
    *dotFile << temp;
  }

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void PtrToIntInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                        bool isPhi, bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  *dotFile << "(";

  // this->getResultingType()  //temp change->print(dotFile);

  *dotFile << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      *dotFile << constant_int->getSExtValue();
    } else {
      *dotFile << "[PtrToIntInstruction Error] Unexpected constant!\n";
    }
  } else {
    temp = (std::string)operand_0->getName();
    *dotFile << temp;
  }

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void IntToPtrInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                        bool isPhi, bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  *dotFile << "(";

  // this->getResultingType()  //temp change->print(dotFile);

  *dotFile << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      *dotFile << constant_int->getSExtValue();
    } else {
      *dotFile << "[IntToPtrInstruction Error] Unexpected constant!\n";
    }
  } else {
    temp = (std::string)operand_0->getName();
    *dotFile << temp;
  }

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void BitcastInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                       bool isPhi, bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  *dotFile << "(";

  // this->getResultingType()  //temp change->print(dotFile);

  *dotFile << ") ";

  this->operands[0].first->printOperandDot(dotFile);

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void AddrSpaceInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                         bool isPhi, bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  llvm::Value *operand_0 = this->operands[0].first->getValue();

  *dotFile << "(address-space-cast-";

  llvm::Type *operand_1 = this->operands[1].first->getValue()->getType();

  // operand_1->print(dotFile);  //changed temp

  *dotFile << ") ";

  if (llvm::isa<llvm::Constant>(operand_0)) {
    if (llvm::isa<llvm::ConstantInt>(operand_0)) {
      llvm::ConstantInt *constant_int =
          llvm::cast<llvm::ConstantInt>(operand_0);

      *dotFile << constant_int->getSExtValue();
    } else {
      *dotFile << "[AddrSpaceInstruction Error] Unexpected constant!\n";
    }
  } else {
    temp = (std::string)operand_0->getName();
    *dotFile << temp;
  }

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

// Other important instructions

// Compare instruction

void CompareInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                       bool isPhi, bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = ";

  llvm::Value *condition_operand = this->operands[0].first->getValue();

  llvm::Value *operand_1 = this->operands[0].first->getValue();

  llvm::Value *operand_2 = this->operands[1].first->getValue();

  this->operands[0].first->printOperandDot(dotFile);
  // if (isa<Constant>(operand_1))
  // {
  //     if (cast<Constant>(operand_1)->isNullValue())
  //     {
  //         *dotFile << "null";
  //     }
  //     else if (isa<ConstantInt>(operand_1))
  //     {
  //         ConstantInt *constant_int = cast<ConstantInt>(operand_1);

  //         *dotFile << constant_int->getSExtValue();
  //     }
  //     else if (isa<ConstantFP>(operand_1))
  //     {
  //         ConstantFP *constant_float = cast<ConstantFP>(operand_1);

  //         *dotFile << constant_float->getValueAPF().convertToFloat();
  //     }
  //     else
  //     {
  //         llvm_unreachable("[CompareInstruction Error] Unexpected
  //         constant!");
  //     }
  // }
  // else
  // {
  //     *dotFile << operand_1->getName();
  // }

  if (llvm::isa<llvm::ICmpInst>(this->instruction)) {
    llvm::ICmpInst *icmp_instruction =
        llvm::cast<llvm::ICmpInst>(this->instruction);

    llvm::CmpInst::Predicate predicate = icmp_instruction->getPredicate();

    switch (predicate) {
    case llvm::CmpInst::ICMP_EQ:
      *dotFile << " == ";
      break;

    case llvm::CmpInst::ICMP_NE:
      *dotFile << " != ";
      break;

    case llvm::CmpInst::ICMP_UGT:
    case llvm::CmpInst::ICMP_SGT:
      *dotFile << " > ";
      break;

    case llvm::CmpInst::ICMP_UGE:
    case llvm::CmpInst::ICMP_SGE:
      *dotFile << " >= ";
      break;

    case llvm::CmpInst::ICMP_ULT:
    case llvm::CmpInst::ICMP_SLT:
      *dotFile << " < ";
      break;

    case llvm::CmpInst::ICMP_ULE:
    case llvm::CmpInst::ICMP_SLE:
      *dotFile << " <= ";
      break;

    default:
      llvm_unreachable("[CompareInstruction Error] Unexpected predicate!");
    }
  } else if (llvm::isa<llvm::FCmpInst>(this->instruction)) {
    llvm::FCmpInst *fcmp_instruction =
        llvm::cast<llvm::FCmpInst>(this->instruction);

    llvm::FCmpInst::Predicate predicate = fcmp_instruction->getPredicate();

    switch (predicate) {
    case llvm::CmpInst::FCMP_OEQ:
    case llvm::CmpInst::FCMP_UEQ:
      *dotFile << " == ";
      break;

    case llvm::CmpInst::FCMP_ONE:
    case llvm::CmpInst::FCMP_UNE:
      *dotFile << " != ";
      break;

    case llvm::CmpInst::FCMP_OGT:
    case llvm::CmpInst::FCMP_UGT:
      *dotFile << " > ";
      break;

    case llvm::CmpInst::FCMP_OGE:
    case llvm::CmpInst::FCMP_UGE:
      *dotFile << " >= ";
      break;

    case llvm::CmpInst::FCMP_OLT:
    case llvm::CmpInst::FCMP_ULT:
      *dotFile << " < ";
      break;

    case llvm::CmpInst::FCMP_OLE:
    case llvm::CmpInst::FCMP_ULE:
      *dotFile << " <= ";
      break;

    // True if both the operands are not QNAN
    case llvm::CmpInst::FCMP_ORD:
      *dotFile << " !QNAN";
      break;

    // True if either of the operands is/are a QNAN
    case llvm::CmpInst::FCMP_UNO:
      *dotFile << " EITHER-QNAN ";
      break;

    case llvm::CmpInst::FCMP_FALSE:
      *dotFile << " false ";
      break;

    case llvm::CmpInst::FCMP_TRUE:
      *dotFile << " true ";
      break;

    default:
      llvm_unreachable("[CompareInstruction Error] Unexpected predicate!");
    }
  } else {
    llvm_unreachable(
        "[CompareInstruction Error] Unexpected compare instruction type!");
  }

  // if (isa<Constant>(operand_2))
  // {
  //     if (cast<Constant>(operand_2)->isNullValue())
  //     {
  //         *dotFile << "null";
  //     }
  //     else if (isa<ConstantInt>(operand_2))
  //     {
  //         ConstantInt *constant_int = cast<ConstantInt>(operand_2);

  //         *dotFile << constant_int->getSExtValue();
  //     }
  //     else if (isa<ConstantFP>(operand_2))
  //     {
  //         ConstantFP *constant_float = cast<ConstantFP>(operand_2);

  //         *dotFile << constant_float->getValueAPF().convertToFloat();
  //     }
  //     else
  //     {
  //         llvm_unreachable("[CompareInstruction Error] Unexpected
  //         constant!");
  //     }
  // }
  // else
  // {
  //     *dotFile << operand_2->getName();
  // }

  this->operands[1].first->printOperandDot(dotFile);

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void PhiInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                   bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = \u03A6(";

  for (int i = 0; i < this->operands.size(); i++) {
    this->operands[i].first->printOperandDot(dotFile);

    if (i != this->operands.size() - 1) {
      *dotFile << ", ";
    } else {
      *dotFile << ")\n";
    }
  }

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void SelectInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                      bool isMMA) {
  assert(this != NULL);
  std::string temp;

  llvm::SelectInst *select_instruction;

  if (select_instruction =
          llvm::dyn_cast<llvm::SelectInst>(this->instruction)) {
    // Has three operands : condition, true-operand (operand that will be
    // assigned when the condition is true) and false-operand (operand that
    // will be assigned when the condition is false)
    SLIMOperand *condition = this->operands[0].first;

    SLIMOperand *true_operand = this->operands[1].first;

    SLIMOperand *false_operand = this->operands[2].first;

    temp = (std::string)this->result.first->getValue()->getName();
    *dotFile << temp << " = ";

    // Print the condition operand
    condition->printOperandDot(dotFile);

    *dotFile << " ? ";

    // Print the "true" operand
    true_operand->printOperandDot(dotFile);

    *dotFile << " : ";

    // Print the "false" operand
    false_operand->printOperandDot(dotFile);

    *dotFile << "\n";
  } else {
    llvm_unreachable("[SelectInstruction Error] The corresponding LLVM "
                     "instruction is not a select instruction!");
  }

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void FreezeInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                      bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->result.first->getValue()->getName();
  *dotFile << temp << " = freeze(";

  llvm::Value *operand = this->operands[0].first->getValue();

  temp = (std::string)operand->getName();
  *dotFile << temp;

  *dotFile << ")\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void CallInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                    bool isMMA) {
  assert(this != NULL);
  std::string temp;

  const std::string dbg_declare = "llvm.dbg.declare";
  const std::string dbg_value = "llvm.dbg.value";

  // If callee_function is NULL then it is an indirect calll
  if (!this->isIndirectCall()) {
    llvm::StringRef callee_function_name = this->callee_function->getName();

    if (callee_function_name.str() == (dbg_declare) ||
        callee_function_name.str() == (dbg_value)) {
      return;
    }
  }

  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    /* llvm::outs()<<"FLAG-A:"<<flagInstr;
    if((flagInstr) != this->getSourceLineNumber())  //edit1
        *dotFile << this->getSourceLineNumber()<<"\n"; */  //edit BB

    // *dotFile << this->getSourceLineNumber()<<" ";   //changed
    // temp2//*dotFile << "[" << this->getSourceLineNumber() << "] ";
    // //changed temp2
  }

  if (!this->result.first->getValue()->getName().empty()) {
    temp = (std::string)this->result.first->getValue()->getName();
    // *dotFile << "[label=\"";
    *dotFile << temp << " = call ";
  }

  // If callee_function is NULL then it is an indirect calll
  if (!this->isIndirectCall()) {
    temp = (std::string)this->callee_function->getName();
    *dotFile << temp << "(";
  } else {
    *dotFile << "(";

    this->getIndirectCallOperand()->printOperandDot(dotFile);

    *dotFile << ") (";
  }

  if (this->operands.empty()) {
    *dotFile << ")"; //\"]\n";  //edit BB
  }

  for (int i = 0; i < this->operands.size(); i++) {
    this->operands[i].first->printOperandDot(dotFile);

    if (i != this->operands.size() - 1) {
      *dotFile << ", ";
    } else {
      *dotFile << ")"; //\"]\n";//edit BB
    }
  }
  *dotFile << "\n"; // edit BB

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void VarArgInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                      bool isMMA) {
  assert(this != NULL);

  this->printLLVMInstructionDot(dotFile); // modSB ;

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void LandingpadInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                          bool isPhi, bool isMMA) {
  assert(this != NULL);

  this->printLLVMInstructionDot(dotFile); // modSB ;

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void CatchpadInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                        bool isPhi, bool isMMA) {
  assert(this != NULL);

  this->printLLVMInstructionDot(dotFile); // modSB ;

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void CleanuppadInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                          bool isPhi, bool isMMA) {
  assert(this != NULL);

  this->printLLVMInstructionDot(dotFile); // modSB ;

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void ReturnInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                      bool isMMA) {
  assert(this != NULL);
  llvm::Value *value = this->getReturnValue();

  if (!value) {
    *dotFile << "\n"; // *dotFile << "\"]\n"; //edit BB
    return;
  }

  SLIMOperand *value_slim_operand =
      OperandRepository::getOrCreateSLIMOperand(value);

  // Print the return operand
  *dotFile << "\nreturn ";
  value_slim_operand->printOperandDot(dotFile);

  *dotFile << "\n"; // *dotFile << "\"]\n"; //edit BB
  // *dotFile << this->getSourceLineNumber()<<"->";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void BranchInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                      bool isMMA) {
  assert(this != NULL);
  std::string temp;

  llvm::BranchInst *branch_inst;

  *dotFile << "branch ";

  if (branch_inst = llvm::dyn_cast<llvm::BranchInst>(this->instruction)) {
    if (this->is_conditional) {
      // The branch instruction is conditional, meaning there is only one
      // successor

      llvm::Value *condition = branch_inst->getCondition();

      temp = (std::string)condition->getName();
      *dotFile << "(" << temp << ") ";

      temp = (std::string)branch_inst->getSuccessor(0)->getName();
      *dotFile << temp;
      *dotFile << ", ";

      temp = (std::string)branch_inst->getSuccessor(1)->getName();
      *dotFile << temp;
    } else {
      temp = (std::string)branch_inst->getSuccessor(0)->getName();
      *dotFile << temp;
    }

    *dotFile << "\n";
  } else {
    llvm_unreachable("[BranchInstruction Error] The underlying LLVM "
                     "instruction is not a branch instruction!");
  }

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

// Switch instruction

void SwitchInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                      bool isMMA) {
  assert(this != NULL);
  std::string temp;

  llvm::SwitchInst *switch_instruction;

  if (switch_instruction =
          llvm::dyn_cast<llvm::SwitchInst>(this->instruction)) {
    *dotFile << "switch(";

    llvm::Value *comparison_value = this->getConditionOperand()->getValue();

    if (llvm::isa<llvm::Constant>(comparison_value)) {
      if (llvm::isa<llvm::ConstantInt>(comparison_value)) {
        llvm::ConstantInt *constant_int =
            llvm::cast<llvm::ConstantInt>(comparison_value);

        *dotFile << constant_int->getSExtValue();
      } else {
        llvm_unreachable("[SwitchInstruction Error] The comparison "
                         "value is not an integer!");
      }
    } else {
      temp = (std::string)comparison_value->getName();
      *dotFile << temp;
    }

    *dotFile << ") {\n";

    // Print non-default cases
    for (unsigned i = 0; i < this->getNumberOfCases(); i++) {
      llvm::ConstantInt *case_value = this->getConstantOfCase(i);

      *dotFile << "case " << case_value->getSExtValue() << ": branch-to ";

      llvm::BasicBlock *case_destination = this->getDestinationOfCase(i);

      temp = (std::string)case_destination->getName();
      *dotFile << temp << "; break; \n";
    }

    // Print default case
    temp = (std::string)this->getDefaultDestination()->getName();
    *dotFile << "default: branch-to " << temp << "\n";
  } else {
    llvm_unreachable("[SwitchInstruction Error] The underlying LLVM "
                     "instruction is not a switch instruction!");
  }

  *dotFile << "}\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void IndirectBranchInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                              bool isPhi, bool isMMA) {
  assert(this != NULL);
  std::string temp;

  temp = (std::string)this->getBranchAddress()->getName();
  *dotFile << "indirect branch to " << temp;

  *dotFile << " : [";

  unsigned total_possible_dests = this->getNumPossibleDestinations();

  for (unsigned i = 0; i < total_possible_dests; i++) {
    temp = (std::string)this->getPossibleDestination(i)->getName();
    *dotFile << temp;

    if (i != total_possible_dests - 1) {
      *dotFile << ", ";
    }
  }

  *dotFile << "]\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void InvokeInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                      bool isMMA) {
  assert(this != NULL);
  std::string temp;

  const std::string dbg_declare = "llvm.dbg.declare";
  const std::string dbg_value = "llvm.dbg.value";

  // If callee_function is NULL then it is an indirect calll
  if (!this->isIndirectCall()) {
    llvm::StringRef callee_function_name = this->callee_function->getName();

    if (callee_function_name.str() == (dbg_declare) ||
        callee_function_name.str() == (dbg_value)) {
      return;
    }
  }

  if (!this->result.first->getValue()->getName().empty()) {
    temp = (std::string)this->result.first->getValue()->getName();
    *dotFile << temp << " = invoke ";
  }

  // If callee_function is NULL then it is an indirect calll
  if (!this->isIndirectCall()) {
    temp = (std::string)this->callee_function->getName();
    *dotFile << temp << "(";
  } else {
    *dotFile << "(";

    this->getIndirectCallOperand()->printOperandDot(dotFile);

    *dotFile << ") (";
  }

  if (this->operands.empty()) {
    *dotFile << ")\n";
  }

  for (int i = 0; i < this->operands.size(); i++) {
    this->operands[i].first->printOperandDot(dotFile);

    if (i != this->operands.size() - 1) {
      *dotFile << ", ";
    } else {
      *dotFile << ") ";
    }
  }

  temp = (std::string)this->getNormalDestination()->getName();
  *dotFile << temp << ", ";
  temp = (std::string)this->getExceptionDestination()->getName();
  *dotFile << temp;

  *dotFile << "\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void CallbrInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                      bool isMMA) {
  assert(this != NULL);
  std::string temp;

  if (!this->result.first->getValue()->getName().empty()) {
    temp = (std::string)this->result.first->getValue()->getName();
    *dotFile << temp << " = callbr ";
  }

  const std::string dbg_declare = "llvm.dbg.declare";
  const std::string dbg_value = "llvm.dbg.value";

  llvm::StringRef callee_function_name = this->callee_function->getName();

  if (callee_function_name.str() == (dbg_declare) ||
      callee_function_name.str() == (dbg_value))
    return;

  temp = (std::string)this->callee_function->getName();
  *dotFile << temp << "(";

  for (int i = 0; i < this->operands.size(); i++) {
    llvm::Value *operand_i = this->operands[i].first->getValue();

    if (llvm::isa<llvm::Constant>(operand_i)) {
      llvm::GEPOperator *gep_operator;

      if (llvm::isa<llvm::ConstantInt>(operand_i)) {
        llvm::ConstantInt *constant_int =
            llvm::cast<llvm::ConstantInt>(operand_i);

        *dotFile << constant_int->getSExtValue();
      } else if (llvm::isa<llvm::ConstantFP>(operand_i)) {
        llvm::ConstantFP *constant_float =
            llvm::cast<llvm::ConstantFP>(operand_i);

        *dotFile << constant_float->getValueAPF().convertToFloat();
      } else if (gep_operator = llvm::dyn_cast<llvm::GEPOperator>(operand_i)) {
        llvm::Value *operand_i = gep_operator->getOperand(0);

        temp = (std::string)operand_i->getName();
        *dotFile << temp;
      } else if (llvm::isa<llvm::GlobalValue>(operand_i)) {
        temp = (std::string)llvm::cast<llvm::GlobalValue>(operand_i)->getName();
        *dotFile << temp;
      } else {
        // llvm_unreachable("[CallInstruction Error] Unexpected
        // constant!\n");
        *dotFile << "[InvokeInstruction Error] Unexpected constant!\n";
      }
    } else {
      temp = (std::string)operand_i->getName();
      *dotFile << temp;
    }

    if (i != this->operands.size() - 1) {
      *dotFile << ", ";
    } else {
      *dotFile << ")";
    }
  }

  temp = (std::string)this->getDefaultDestination()->getName();
  *dotFile << temp << ", [";

  for (unsigned i = 0; i < this->getNumIndirectDestinations(); i++) {
    temp = (std::string)this->getIndirectDestination(i)->getName();
    *dotFile << temp;

    if (i != this->getNumIndirectDestinations() - 1) {
      *dotFile << ", ";
    }
  }

  *dotFile << "]\n";

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

// Resume instruction - resumes propagation of an existing exception

void ResumeInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                      bool isMMA) {
  assert(this != NULL);
  std::string temp;

  *dotFile << "resume ";

  temp = (std::string)this->operands[0].first->getValue()->getName();
  *dotFile << temp;

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

// Catch Switch instruction
void CatchswitchInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                           bool isPhi, bool isMMA) {
  assert(this != NULL);

  this->printLLVMInstructionDot(dotFile); // modSB ;

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void CatchreturnInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                           bool isPhi, bool isMMA) {
  assert(this != NULL);
  this->printLLVMInstructionDot(dotFile); // modSB ;

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void CleanupReturnInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                             bool isPhi, bool isMMA) {
  assert(this != NULL);

  this->printLLVMInstructionDot(dotFile); // modSB ;

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void UnreachableInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile,
                                           bool isPhi, bool isMMA) {
  assert(this != NULL);
  if (this->hasSourceLineNumber() && this->getSourceLineNumber() != 0) {
    *dotFile << "[" << this->getSourceLineNumber() << "] "; // changed temp2
  }

  this->printLLVMInstructionDot(dotFile); // modSB ;

  if (isPhi) {
    *dotFile << "    \u03A6 here\n";
  }
  if (isMMA) {
    *dotFile << "    MMA here\n";
  }
}

void OtherInstruction::printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                                     bool isMMA) {
  assert(this != NULL);
  llvm::outs() << "Not supported!\n";
}
