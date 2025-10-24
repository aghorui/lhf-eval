#include "Operand.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/FileSystem.h"
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

// Types of SLIM instructions
typedef enum {
    ALLOCA,
    LOAD,
    STORE,
    FENCE,
    ATOMIC_COMPARE_CHANGE,
    ATOMIC_MODIFY_MEM,
    GET_ELEMENT_PTR,
    FP_NEGATION,
    BINARY_OPERATION,
    EXTRACT_ELEMENT,
    INSERT_ELEMENT,
    SHUFFLE_VECTOR,
    EXTRACT_VALUE,
    INSERT_VALUE,
    TRUNC,
    ZEXT,
    SEXT,
    FPEXT,
    FP_TO_INT,
    INT_TO_FP,
    PTR_TO_INT,
    INT_TO_PTR,
    BITCAST,
    ADDR_SPACE,
    COMPARE,
    PHI,
    SELECT,
    FREEZE,
    CALL,
    VAR_ARG,
    LANDING_PAD,
    CATCH_PAD,
    CLEANUP_PAD,
    RETURN,
    BRANCH,
    SWITCH,
    INDIRECT_BRANCH,
    INVOKE,
    CALL_BR,
    RESUME,
    CATCH_SWITCH,
    CATCH_RETURN,
    CLEANUP_RETURN,
    UNREACHABLE,
    OTHER,
    NOT_ASSIGNED
} InstructionType;

// Types of binary operations
typedef enum {
    ADD,
    SUB,
    MUL,
    DIV,
    REM,
    SHIFT_LEFT,
    LOGICAL_SHIFT_RIGHT,
    ARITHMETIC_SHIFT_RIGHT,
    BITWISE_AND,
    BITWISE_OR,
    BITWISE_XOR
} SLIMBinaryOperator;

// Types of input/scan statements (used to determine if an instruction takes an
// input)
typedef enum { NOT_APPLICABLE, SCANF, SSCANF, FSCANF } InputStatementType;

/**
 * Indirection for LLVM variables.
 *
 * For a variable `a`, the indirection level is:
 * 0 for `&a`
 * 1 for `a`
 * 2 for `*a`
 * There is no `**a` or above since this is for LLVM IR instructions.
 */
typedef int IndirectionLevel;

typedef std::pair<SLIMOperand *, IndirectionLevel> SLIMOperandIndirectionPair;

/*
    BaseInstruction class

    All the SLIM Instructions should inherit this class.
*/
class BaseInstruction {
  protected:
    // Type of the instruction
    InstructionType instruction_type;

    // Instruction id (unique for every instruction)
    long long instruction_id;

    // The corresponding LLVM instruction
    llvm::Instruction *instruction;

    // The function to which this instruction belongs
    llvm::Function *function;

    // The basic block to which this instruction belongs
    llvm::BasicBlock *basic_block;

    // The instruction is expected to be ignored in the analysis (if the flag is
    // set)
    bool is_ignored;

    // Operands of the instruction
    std::vector<SLIMOperandIndirectionPair> operands;

    // Result of the instruction
    SLIMOperandIndirectionPair result;

    // Variants info
    std::map<unsigned, std::map<llvm::Value *, unsigned>> variants;

    // Is the instruction an input statement
    bool is_input_statement;

    // Type of the input statement
    InputStatementType input_statement_type;

    // Starting index from where the input arguments start (applicable to a
    // valid input statement)
    unsigned starting_input_args_index;

    // Is the instruction a constant assignment
    bool is_constant_assignment;

    // Is the instruction an expression
    bool is_expression_assignment;

    // Does this SLIM instruction correspond to any statement in the original
    // source program
    bool has_source_line_number;

    // Does this SLIM instruction involve any pointer variable (with reference
    // to the source program)
    bool has_pointer_variables;

    // The source program line number corresponding to this instruction
    unsigned source_line_number;

    // dynamic allocation
    bool isDynamicAllocation;

    void mapOperandsToSLIMOperands(IndirectionLevel level);
    void mapResultOperandToSLIM(IndirectionLevel level);
    void mapAllOperandsToSLIM(IndirectionLevel level);

  public:
    // Constructor
    BaseInstruction(llvm::Instruction *instruction);
    virtual ~BaseInstruction() = default;

    // Sets the ID for this instruction
    void setInstructionId(long long id);

    // Returns the instruction ID
    long long getInstructionId();

    // Returns if Dynamic Allocation
    bool getIsDynamicAllocation();

    // Returns the instruction type
    InstructionType getInstructionType();

    // Returns the corresponding LLVM instruction
    llvm::Instruction *getLLVMInstruction();

    // Returns the function to which this instruction belongs
    llvm::Function *getFunction();

    // Returns the basic block to which this instruction belongs
    llvm::BasicBlock *getBasicBlock();

    // Returns true if the instruction is an input statement
    bool isInputStatement();

    // Returns the input statement type
    InputStatementType getInputStatementType();

    // Returns the starting index of the input arguments (applicable only to
    // valid input statements)
    unsigned getStartingInputArgsIndex();

    // Returns true if the instruction is a constant assignment
    bool isConstantAssignment();

    // Returns true if the instruction is an expression assignment;
    bool isExpressionAssignment();

    // Checks whether the instruction has any relation to a statement in the
    // source program or not
    bool hasSourceLineNumber();

    // Returns the source program line number corresponding to this instruction
    unsigned getSourceLineNumber();

    // Sets the source program line number corresponding to this instruction
    // //modSB
    void setSourceLineNumber(unsigned srcLineNumber);

    // Returns the source file name corresponding to this instruction (to be
    // used only for print purposes)
    std::string getSourceFileName();

    // Returns the absolute source file name corresponding to this instruction
    // (to be used only for print purposes)
    std::string getAbsSourceFileName();

    // Sets the ignore flag
    void setIgnore();

    // Returns true if the instruction is to be ignored (during analysis)
    bool isIgnored();

    // Returns true if the instruction involves any pointer variable (with
    // reference to the source program)
    bool hasPointerVariables();

    // Prints the corresponding LLVM instruction
    void printLLVMInstruction();

    // Prints the corresponding LLVM instruction to DOT File
    void printLLVMInstructionDot(llvm::raw_ostream *dotFile);

    // Returns the result operand
    SLIMOperandIndirectionPair getResultOperand();

    // Sets the result operand
    void setResultOperand(SLIMOperandIndirectionPair new_operand);

    // Returns the number of operands
    unsigned getNumOperands();

    // Returns the operand at a particular index
    SLIMOperandIndirectionPair getOperand(unsigned index);

    // Sets the operand at the given index
    void setOperand(unsigned index, SLIMOperandIndirectionPair new_operand);

    // Sets the indirection level of RHS operand at the given index
    void setRHSIndirection(unsigned index, unsigned new_indirection);

    // Create and return a variant of this instruction
    BaseInstruction *
    createVariant(SLIMOperandIndirectionPair result,
                  std::vector<SLIMOperandIndirectionPair> operands);

    // Pure virtual function - every SLIM instruction class must implement this
    // function
    virtual void printInstruction() = 0;

    // print dot file
    virtual void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi,
                               bool isMMA) = 0; // modSB

    // Insert new variant info
    void insertVariantInfo(unsigned result_ssa_version, llvm::Value *variable,
                           unsigned variable_version);

    // Get number of variants
    unsigned getNumVariants();

    // Print variants
    void printMMVariants();

    // set instruction type
    void setInstructionType(InstructionType);

    // createNewInstruction //modSB
    BaseInstruction *
    createNewInputInstruction(BaseInstruction *instruct,
                              SLIMOperandIndirectionPair new_operand);

    // --------------- APIs for the Legacy SLIM --------------- //

    // Returns true if the instruction is a CALL instruction
    bool getCall();

    // Returns true if the instruction is a PHI instruction
    bool getPhi();

    // Return the LHS operand
    SLIMOperandIndirectionPair getLHS();

    // Return the RHS operand(s) list
    std::vector<SLIMOperandIndirectionPair> getRHS();
    // -------------------------------------------------------- //
};

// Alloca instruction
class AllocaInstruction : public BaseInstruction {
  public:
    AllocaInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Load instruction
class LoadInstruction : public BaseInstruction {
  public:
    LoadInstruction(llvm::Instruction *instruction);
    LoadInstruction(llvm::CallInst *call_instruction, SLIMOperand *result,
                    SLIMOperand *rhs_operand);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Store instruction
class StoreInstruction : public BaseInstruction {
  public:
    StoreInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Fence instruction
class FenceInstruction : public BaseInstruction {
  public:
    FenceInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Atomic compare and change instruction
class AtomicCompareChangeInstruction : public BaseInstruction {
  protected:
    SLIMOperandIndirectionPair pointer_operand;
    SLIMOperandIndirectionPair compare_operand;
    SLIMOperandIndirectionPair new_value;

  public:
    AtomicCompareChangeInstruction(llvm::Instruction *instruction);
    llvm::Value *getPointerOperand();
    llvm::Value *getCompareOperand();
    llvm::Value *getNewValue();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Atomic modify memory instruction
class AtomicModifyMemInstruction : public BaseInstruction {
  public:
    AtomicModifyMemInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Getelementptr instruction
class GetElementPtrInstruction : public BaseInstruction {
    SLIMOperand *gep_main_operand;
    std::vector<SLIMOperand *> indices;

  public:
    GetElementPtrInstruction(llvm::Instruction *instruction);

    // Returns the main operand (corresponding to the aggregate name)
    SLIMOperand *getMainOperand();

    // Returns the number of index operands
    unsigned getNumIndexOperands();

    // Returns the operand corresponding to the index at the given position
    // (0-based)
    SLIMOperand *getIndexOperand(unsigned position);

    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Unary operation

// Floating-point negation instruction
class FPNegationInstruction : public BaseInstruction {
  public:
    FPNegationInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Binary operations
class BinaryOperation : public BaseInstruction {
    SLIMBinaryOperator binary_operator;

  public:
    BinaryOperation(llvm::Instruction *instruction);
    SLIMBinaryOperator getOperationType();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Vector operations

// Extractelement instruction
class ExtractElementInstruction : public BaseInstruction {
  public:
    ExtractElementInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Insertelement instruction
class InsertElementInstruction : public BaseInstruction {
  public:
    InsertElementInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// ShuffleVector instruction
class ShuffleVectorInstruction : public BaseInstruction {
  public:
    ShuffleVectorInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Operations for aggregates (structure and array) stored in registers

// ExtractValue instruction
class ExtractValueInstruction : public BaseInstruction {
  protected:
    std::vector<unsigned> indices;

  public:
    ExtractValueInstruction(llvm::Instruction *instruction);
    unsigned getNumIndices();
    unsigned getIndex(unsigned index);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// InsertValue instruction
class InsertValueInstruction : public BaseInstruction {
  public:
    InsertValueInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Conversion operations

// Trunc instruction
class TruncInstruction : public BaseInstruction {
  protected:
    llvm::Type *resulting_type;

  public:
    TruncInstruction(llvm::Instruction *instruction);
    llvm::Type *getResultingType();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Zext instruction
class ZextInstruction : public BaseInstruction {
  protected:
    llvm::Type *resulting_type;

  public:
    ZextInstruction(llvm::Instruction *instruction);
    llvm::Type *getResultingType();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Sext instruction
class SextInstruction : public BaseInstruction {
  protected:
    llvm::Type *resulting_type;

  public:
    SextInstruction(llvm::Instruction *instruction);
    llvm::Type *getResultingType();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// FPExt instruction
class FPExtInstruction : public BaseInstruction {
  protected:
    llvm::Type *resulting_type;

  public:
    FPExtInstruction(llvm::Instruction *instruction);
    llvm::Type *getResultingType();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// FPToInt instruction
class FPToIntInstruction : public BaseInstruction {
  protected:
    llvm::Type *resulting_type;

  public:
    FPToIntInstruction(llvm::Instruction *instruction);
    llvm::Type *getResultingType();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// IntToFP instruction
class IntToFPInstruction : public BaseInstruction {
  protected:
    llvm::Type *resulting_type;

  public:
    IntToFPInstruction(llvm::Instruction *instruction);
    llvm::Type *getResultingType();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// PtrToInt instruction
class PtrToIntInstruction : public BaseInstruction {
  protected:
    llvm::Type *resulting_type;

  public:
    PtrToIntInstruction(llvm::Instruction *instruction);
    llvm::Type *getResultingType();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// IntToPtr instruction
class IntToPtrInstruction : public BaseInstruction {
  protected:
    llvm::Type *resulting_type;

  public:
    IntToPtrInstruction(llvm::Instruction *instruction);
    llvm::Type *getResultingType();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Bitcast instruction
class BitcastInstruction : public BaseInstruction {
  protected:
    llvm::Type *resulting_type;

  public:
    BitcastInstruction(llvm::Instruction *instruction);
    llvm::Type *getResultingType();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// AddrSpace instruction
class AddrSpaceInstruction : public BaseInstruction {
  public:
    AddrSpaceInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Other important instructions

// Compare instruction
class CompareInstruction : public BaseInstruction {
  public:
    CompareInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Phi instruction
class PhiInstruction : public BaseInstruction {
  public:
    PhiInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Select instruction
class SelectInstruction : public BaseInstruction {
  public:
    SelectInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Freeze instruction
class FreezeInstruction : public BaseInstruction {
  public:
    FreezeInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Call instruction
class CallInstruction : public BaseInstruction {
  protected:
    llvm::Function *callee_function = {nullptr};
    SLIMOperand *indirect_call_operand{nullptr};
    bool indirect_call{false};
    std::vector<llvm::Argument *> formal_arguments_list;

  public:
    CallInstruction(llvm::Instruction *instruction);
    bool isIndirectCall();
    SLIMOperand *getIndirectCallOperand();
    llvm::Function *getCalleeFunction();
    unsigned getNumFormalArguments();
    llvm::Argument *getFormalArgument(unsigned index);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Variable argument instruction
class VarArgInstruction : public BaseInstruction {
  public:
    VarArgInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Landingpad instruction
class LandingpadInstruction : public BaseInstruction {
  public:
    LandingpadInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Catchpad instruction
class CatchpadInstruction : public BaseInstruction {
  public:
    CatchpadInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Cleanuppad instruction
class CleanuppadInstruction : public BaseInstruction {
  public:
    CleanuppadInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Terminator instructions

// Return instruction
class ReturnInstruction : public BaseInstruction {
  protected:
    SLIMOperand *return_value = nullptr;

  public:
    ReturnInstruction(llvm::Instruction *instruction);
    SLIMOperand *getReturnOperand();
    llvm::Value *getReturnValue();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Branch instruction
class BranchInstruction : public BaseInstruction {
  protected:
    bool is_conditional;

  public:
    BranchInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Switch instruction
class SwitchInstruction : public BaseInstruction {
  protected:
    SLIMOperand *condition_value;
    llvm::BasicBlock *default_case;
    std::vector<std::pair<llvm::ConstantInt *, llvm::BasicBlock *>> other_cases;

  public:
    SwitchInstruction(llvm::Instruction *instruction);
    SLIMOperand *getConditionOperand();
    llvm::BasicBlock *getDefaultDestination();

    // Returns the number of cases excluding default case
    unsigned getNumberOfCases();

    llvm::ConstantInt *getConstantOfCase(unsigned case_number);
    llvm::BasicBlock *getDestinationOfCase(unsigned case_number);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Indirect branch instruction
class IndirectBranchInstruction : public BaseInstruction {
  protected:
    llvm::Value *address;
    std::vector<llvm::BasicBlock *> possible_destinations;

  public:
    IndirectBranchInstruction(llvm::Instruction *instruction);
    llvm::Value *getBranchAddress();
    unsigned getNumPossibleDestinations();
    llvm::BasicBlock *getPossibleDestination(unsigned index);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Invoke instruction
class InvokeInstruction : public BaseInstruction {
  protected:
    llvm::Function *callee_function;
    SLIMOperand *indirect_call_operand;
    bool indirect_call;
    llvm::BasicBlock *normal_destination;
    llvm::BasicBlock *exception_destination;

  public:
    InvokeInstruction(llvm::Instruction *instruction);
    bool isIndirectCall();
    SLIMOperand *getIndirectCallOperand();
    llvm::Function *getCalleeFunction();
    llvm::BasicBlock *getNormalDestination();
    llvm::BasicBlock *getExceptionDestination();
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Callbr instruction
class CallbrInstruction : public BaseInstruction {
  protected:
    llvm::Function *callee_function;
    llvm::BasicBlock *default_destination;
    std::vector<llvm::BasicBlock *> indirect_destinations;

  public:
    CallbrInstruction(llvm::Instruction *instruction);
    llvm::Function *getCalleeFunction();
    llvm::BasicBlock *getDefaultDestination();
    unsigned getNumIndirectDestinations();
    llvm::BasicBlock *getIndirectDestination(unsigned index);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Resume instruction - resumes propagation of an existing exception
class ResumeInstruction : public BaseInstruction {
  public:
    ResumeInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Catchswitch instruction
class CatchswitchInstruction : public BaseInstruction {
  public:
    CatchswitchInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Catchreturn instruction
class CatchreturnInstruction : public BaseInstruction {
  public:
    CatchreturnInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// CleanupReturn instruction
class CleanupReturnInstruction : public BaseInstruction {
  public:
    CleanupReturnInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Unreachable instruction
class UnreachableInstruction : public BaseInstruction {
  public:
    UnreachableInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};

// Other instruction (not handled as of now)
class OtherInstruction : public BaseInstruction {
  public:
    OtherInstruction(llvm::Instruction *instruction);
    void printInstruction();
    void printInstrDot(llvm::raw_fd_ostream *dotFile, bool isPhi, bool isMMA);
};
