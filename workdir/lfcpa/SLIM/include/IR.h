#ifndef IR_H
#define IR_H
#include "Instructions.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>

namespace slim {
// Check if the SSA variable (created using MemorySSA) already exists or not
extern std::map<std::string, bool> is_ssa_version_available;

// Process the llvm instruction and return the corresponding SLIM instruction
BaseInstruction *convertLLVMToSLIMInstruction(llvm::Instruction &instruction);

// Creates different SSA versions for global and address-taken local variables
// using Memory SSA
void createSSAVersions(std::unique_ptr<llvm::Module> &module);

void normal();
void blue(int bold);
void cyan(int bold);
void green(int bold);
void white(int bold);
void magentaB(int bold);
void yellowB(int bold);
void yellow(int bold);
void redB(int bold);
void red(int bold);

 void extractRefersToIndirection(BaseInstruction *base_instruction);

// Creates the SLIM abstraction and provides APIs to interact with it
class IR {
  private:
    void renameTemporaryOperands(BaseInstruction *instruction,
                                 std::set<llvm::Value *> &renamed_temporaries,
                                 llvm::Function &function);

    void readAllDebugSources();
    void readGlobalsForFunctionPointersInArrays();
    void readDebugSourceVarInfo(llvm::Instruction &instruction);

  protected:
    std::unique_ptr<llvm::Module> llvm_module;
    long long total_instructions = 0;
    long long total_basic_blocks = 0;
    long long total_call_instructions = 0;
    long long total_direct_call_instructions = 0;
    long long total_indirect_call_instructions = 0;
    std::unordered_map<llvm::BasicBlock *, long long> basic_block_to_id;
    std::vector<llvm::Function *> functions;
    std::unordered_map<llvm::Function *, unsigned> num_call_instructions;
    // std::unordered_map<llvm::Value*,

  public:
    ~IR();
    std::map<std::pair<llvm::Function *, llvm::BasicBlock *>,
             std::list<long long>>
        func_bb_to_inst_id;
    std::unordered_map<long long, BaseInstruction *>
        inst_id_to_object; // original
    // std::unordered_map<long long, std::set<BaseInstruction *>>
    // inst_id_to_object;  //modSB

    std::map<unsigned, std::pair<bool, bool>>
        phiMMA; // modSB  // map for phi MMA

    // Map from original SLIM instruction to new SSA instruction list
    std::map<BaseInstruction *, std::list<BaseInstruction *>> InstructionMap;

    // std::map<unsigned, std::list<BaseInstruction *>> listMMA;// modSB  //
    // list of MMAs for a statement/ instruction source line number
    // std::map<unsigned, std::pair<BaseInstruction *,std::list<BaseInstruction
    // *>>>; mapInstoSSA  // modSB  // map instruction to its SSA form keyed
    // using instruction's source line number
    unsigned flag_cosSSA = 1; // modSB  // check whether CoS-SSA or normal CFG
    // std::map<unsigned, unsigned> phiMMA;// modSB  // map for phi MMA
    std::map<llvm::Instruction *, BaseInstruction *> LLVMInstructiontoSLIM;
    std::map<llvm::Function *, std::set<llvm::BasicBlock*>*> func_to_exit_bb_map;
    std::map<llvm::Function *, std::set<llvm::BasicBlock*>*> func_to_term_bb_map;
    std::map<llvm::Function *, llvm::BasicBlock*> return_bb_map;
    // Default constructor
    IR();

    // Construct the SLIM IR from module
    IR(std::unique_ptr<llvm::Module> &module);

    void printSLIMStats();

    void processCallInstruction(llvm::Function &function,
                                BaseInstruction *base_instruction,
                                std::set<llvm::Value *> &renamed_temporaries,
                                llvm::Instruction &instruction,
                                llvm::BasicBlock &basic_block);

    void calculateRefersToSourceValues();

    // void generateIR(std::unique_ptr<llvm::Module> &module);
    void generateIR();

    // Returns the LLVM module
    std::unique_ptr<llvm::Module> &getLLVMModule();

    // Return the total number of instructions (across all basic blocks of all
    // procedures)
    long long getTotalInstructions();

    // Return the total number of functions in the module
    unsigned getNumberOfFunctions();

    // Return the total number of basic blocks in the module
    long long getNumberOfBasicBlocks();

    // Returns the pointer to llvm::Function for the function at the given index
    llvm::Function *getLLVMFunction(unsigned index);

    // Add instructions for function-basicblock pair (used by the LegacyIR)
    void addFuncBasicBlockInstructions(llvm::Function *function,
                                       llvm::BasicBlock *basic_block);

    // Get the function-basicblock to instructions map (required by the
    // LegacyIR)
    std::map<std::pair<llvm::Function *, llvm::BasicBlock *>,
             std::list<long long>> &
    getFuncBBToInstructions();

    // Get the instruction id to SLIM instruction map (required by the LegacyIR)
    std::unordered_map<long long, BaseInstruction *> &getIdToInstructionsMap();

    // Returns the first instruction id in the instruction list of the given
    // function-basicblock pair
    long long getFirstIns(llvm::Function *function,
                          llvm::BasicBlock *basic_block);

    // Returns the last instruction id in the instruction list of the given
    // function-basicblock pair
    long long getLastIns(llvm::Function *function,
                         llvm::BasicBlock *basic_block);

    // Returns the reversed instruction list for a given function and a basic
    // block
    std::list<long long> getReverseInstList(llvm::Function *function,
                                            llvm::BasicBlock *basic_block);

    // Returns the reversed instruction list (for the list passed as an
    // argument)
    std::list<long long> getReverseInstList(std::list<long long> inst_list);

    // Get SLIM instruction from the instruction index
    BaseInstruction *getInstrFromIndex(long long index);

    /**
     * Gets the definition of the operand.
     * @return nullptr if no instruction is found.
     * @example
     * 24: %i = add i32 5, 3
     * getInstrFromOperand(%i) will give the Instruction[24]
     */
    BaseInstruction *getInstrFromOperand(SLIMOperand *operand);

    // Get basic block id
    long long getBasicBlockId(llvm::BasicBlock *basic_block);

    // Inserts instruction at the front of the basic block (only in this
    // abstraction)
    void insertInstrAtFront(BaseInstruction *instruction,
                            llvm::BasicBlock *basic_block);

    // Inserts instruction at the end of the basic block (only in this
    // abstraction)
    void insertInstrAtBack(BaseInstruction *instruction,
                           llvm::BasicBlock *basic_block);

    // Optimize the IR (please use only when you are using the MemorySSAFlag)
    slim::IR *optimizeIR();

    // Dump the IR
    void dumpIR();
    void dumpIR(std::string source_file_name);

    unsigned getNumCallInstructions(llvm::Function *function);

    // reset InstructionMap value to NULL
    void resetInstructionMap();

    // print phiMMA
    void phiMMAprint();

    // print InstructionMap
    void printInstructionMap();

    // reset InstructionMap value equal to key value
    void setInstructionMap();

    // Removes temp from pointer instruction, replacing it by the original
    // referred variable // modSB
    void removeTempFromPtr();

    // print vector //modSB
    void printVector(std::vector<long long> vect);

    // find original pointer var and return indirection level // modSB
    std::pair<llvm::Value *, int>
    findOGptr(llvm::Instruction *instruction, int countIndirection,
              std::vector<long long> &temp_instrid, int position);

    // map llvm instructions to slim instructions //modSB
    void mapLLVMInstructiontoSLIM();

    // print LLVMInstructiontoSLIM map to debug //modSB
    void printLLVMInstructiontoSLIM();
    /**
     * Get the definition of the operand.
     * @example
     * Instruction [24] is:
     * [24] %i = add i32 5, 3
     * then getDefinitionInstruction(%i)
     * returns BaseInstruction of [24]
     */
    BaseInstruction *getDefinitionInstruction(SLIMOperand *operand);

    // remove instructions from OG data structure //modSB
    void removeInstructions(std::vector<long long> temp_instrid);

    // get instruction operands printed //modSB
    void getInstructionDetails();

    std::set<llvm::BasicBlock*>* getExitBBList(llvm::Function* function);
    llvm::BasicBlock* getLastBasicBlock (llvm::Function* function);
    std::set<llvm::BasicBlock*>* getTerminatingBBList(llvm::Function* function);
    
    
    void addReturnArgumentForCall(llvm::Function &function, 
                                  BaseInstruction *base_instruction,
                                  std::set<llvm::Value *> &renamed_temporaries,
                                  llvm::Instruction &instruction, 
                                  llvm::BasicBlock &basic_block);
};

// Provides APIs similar to the older implementation of SLIM in order to support
// the implementations that are built using the older SLIM as a base
class LegacyIR {
  protected:
    slim::IR *slim_ir;

  public:
    LegacyIR();
    void simplifyIR(llvm::Function *, llvm::BasicBlock *);
    std::map<std::pair<llvm::Function *, llvm::BasicBlock *>,
             std::list<long long>> &
    getfuncBBInsMap();

    // Get the instruction id to SLIM instruction map
    // std::unordered_map<long long, BaseInstruction *>
    // &getGlobalInstrIndexList();
    std::unordered_map<long long, BaseInstruction *> &
    getGlobalInstrIndexList(); // modSB

    // Returns the corresponding LLVM instruction for the instruction id
    llvm::Instruction *getInstforIndx(long long index);
};
} // namespace slim
#endif
