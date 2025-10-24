#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <set>
#include <string>

#include <fstream>
#include <iostream>

// Types of SLIM operands
typedef enum {
    VARIABLE,
    GEP_OPERATOR,
    ADDR_SPACE_CAST_OPERATOR,
    BITCAST_OPERATOR,
    PTR_TO_INT_OPERATOR,
    ZEXT_OPERATOR,
    FP_MATH_OPERATOR,
    BLOCK_ADDRESS,
    CONSTANT_AGGREGATE,
    CONSTANT_DATA_SEQUENTIAL,
    CONSTANT_POINTER_NULL,
    CONSTANT_TOKEN_NONE,
    UNDEF_VALUE,
    CONSTANT_INT,
    CONSTANT_FP,
    DSO_LOCAL_EQUIVALENT,
    GLOBAL_VALUE,
    NO_CFI_VALUE,
    NOT_SUPPORTED_OPERAND,
    NULL_OPERAND,
    INPUT_OPERAND // modSB
} OperandType;
/**
 * Adds the function pointers present in the possibly struct or array type
 * `constant` to the `value` operand.
 *
 * @example
 * @list is an array of struct node where
 * struct node {
 *    void (*fp)();
 * }
 * has a function pointer.
 * the global initializer of list is:
 * @list global { {&f1}, {&f2}, {&f3} }
 * Then this function is called as `addFunctionPointersInConstantToValue({
 * {&f1}, {&f2}, {&f3} }, @list)`
 * This will add f1, f2, f3 to the @list->getContainingFunctionPointers() set.
 */
void addFunctionPointersInConstantToValue(llvm::Value *constant,
                                          llvm::Value *value);

// // An empty class to signify input operand of type INPUT_OPERAND  //modSB
// class InputOperand
// {

// };
// Holds operand and some other useful information
class SLIMOperand {
  protected:
    // Type of the operand
    OperandType operand_type{OperandType::NOT_SUPPORTED_OPERAND};

    std::set<SLIMOperand *> contains_function_pointers;

    // LLVM value object corresponding to the operand
    llvm::Value *value{nullptr};

    // llvm::Value * of the main aggregate (if the operand is a GEPOperator)
    llvm::Value *gep_main_operand{nullptr};

    // Is the operand a global variable or an address-taken local variable
    bool is_global_or_address_taken{false};

    // Holds the value "true" if the operand corresponds to the formal argument
    // of a function
    bool is_formal_argument{false};
    bool is_alloca = false;

    // Is the operand a pointer variable
    bool is_pointer_variable{false};

    // Does the operand contain indices
    bool has_indices{false};

    // Does the operand have a name
    bool has_name{false};

    // Set the SSA version
    bool is_ssa_version{false};
    unsigned ssa_version_number;

    // To manually keep track whether the operand is of array type
    bool is_array_type{false};

    bool is_dynamic_allocation_type{false}; // for malloc, calloc etc

    // Contains the llvm::Value * object corresponding to the indices present in
    // the operand
    std::vector<SLIMOperand *> indices;

    // Contains pointer to the function object (corresponding to the callee) if
    // the operand is the result of a call instruction
    llvm::Function *direct_callee_function{nullptr};

    /// @brief Non empty only for non-sources
    std::set<SLIMOperand *> refers_to_source_values;

    llvm::DILocalVariable *debug_source_var = nullptr;

  private:
    // Internal function to be used only in case of print related tasks
    std::string _getOperandName();

    // Internal function which returns the operand type of a llvm::Value object
    static OperandType getOperandTypeOfValue(llvm::Value *value);

  public:
    // Constructors
    SLIMOperand(llvm::Value *value);
    SLIMOperand(llvm::Value *value, bool is_global_or_address_taken,
                llvm::Function *direct_callee_function = nullptr);

    void addContainingFunctionPointer(SLIMOperand *function_pointer);
    std::set<SLIMOperand *> &getContainingFunctionPointers();

    // Returns the operand type
    OperandType getOperandType();
    void setOperandType(OperandType);
    // Get the pointer level of this operand.
    // This counts the number of stars in a C type declaration.
    // int** is 2
    // int*** is 3
    // struct S**** is 4.
    unsigned getPointerIndirectionLevel();

    // Returns true if the operand is a global variable or an address-taken
    // local variable
    bool isGlobalOrAddressTaken();
    void setIsAlloca();
    llvm::Function *getDirectCalleeFunction();

    // Returns true if the operand is a formal argument of a function
    bool isFormalArgument();

    // Returns true if the operand is a global variable or an address-taken
    // local variable (considers only the struct if the operand is a GEP
    // operator)
    bool isVariableGlobal();

    // Returns true if the operand is a result of an alloca instruction
    bool isAlloca();

    // Returns true if the operand is a pointer variable (with reference to the
    // LLVM IR)
    bool isPointerInLLVM();

    // Returns true if the operand is a pointer variable (with reference to the
    // source program)
    bool isPointerVariable();

    // Returns true if the operand is of array type
    bool isArrayElement();

    // Sets the is_array_type to true
    void setArrayType();

    // Sets the gep_main_operand
    void setGEPMainOperand(SLIMOperand *operand);
    void setGEPMainOperand(llvm::Value *val);

    // Returns true if the operand is a GetElementPtr operand inside an
    // instruction
    bool isGEPInInstr();
    bool isBitcastInInstr();

    // Sets the is_pointer_variable to true
    void setIsPointerVariable();

    // Sets the is_pointer_variable to false
    void unsetIsPointerVariable();

    // Returns the pointer to the corresponding llvm::Value object
    llvm::Value *getValue();

    // Returns the type of the operand
    llvm::Type *getType();

    // Returns the number of indices
    unsigned getNumIndices();

    // Does the operand have a name
    bool hasName();

    // Returns the index operand at the specified position (0-based position)
    SLIMOperand *getIndexOperand(unsigned position);

    // Returns the vector of indices
    const std::vector<SLIMOperand *> &getIndexVector();

    void addIndexOperand(SLIMOperand *indOperand);

    // Prints the SLIM operand
    void printOperand(llvm::raw_ostream &stream);

    // Prints the SLIM operand to DOT file
    void printOperandDot(llvm::raw_ostream *stream); // modSB

    // Sets the SSA version
    void setSSAVersion(unsigned ssa_version);

    // Reset the SSA version
    void resetSSAVersion();

    // Returns the "return operand" of the callee function if this operand is
    // the result of a "direct" call instruction
    SLIMOperand *getCalleeReturnOperand();
    SLIMOperand *getCalleeReturnOperand(llvm::Function *);

    /**
     * @brief Returns the set of source value SLIMOperands that this operand
     * refers to. This is transitive. This set will be empty for source values.
     *
     * @details
     * Source variables are defined by either those representing a variable in
     * the source progam (C++ etc) or an LLVM alloca/global/formal argument.
     *
     * See `isSource()` for more details about source variables.
     * See `hasDebugSourceVariable()`
     * See `addRefersToSourceValue()`
     * @example
     * The set next to the instructions are the refersTo set
     * for the result value of the instruction.
     *
     * %a = load i32, i32* @g  ; => {} if %a is a source value
     * %i = load i32, i32* %a  ; => {%a}
     * %i2 = load i32, i32* @h ; => {@h}
     * %i2 = add nsw i32 %i, %i2 ; => {%a, @h}
     */
    std::set<SLIMOperand *> &getRefersToSourceValues();

    /**
     * Simply add the provided value into the refersToSourceValues set
     */
    void addRefersToSourceValues(SLIMOperand *sourceValue);

    /**
     * @brief
     * Returns the indices of this operand from the beginning of the current GEP
     * chain. If a GEP instruction has only one index, that index is
     * not included in this list.
     *
     * @details
     * This is defined for all kinds of variables (not just temporaries or
     * sources)
     *
     * @example
     * %a = getelementptr i32, i32* %b, i32 1, i32 2 ; -> [1, 2]
     * %c = getelementptr i32, i32* %a, i32 3        ; -> [1, 2] (skipped)
     * %d = getelementptr i32, i32* %c, i32 4, i32 5 ; -> [1, 2, 4, 5]
     *
     */
    std::vector<SLIMOperand *> getTransitiveIndices();

    /**
     * Returns true if this operand represents a variable in the source program.
     *
     * This can return true for values without a name (if this value was
     * constant folded).
     *
     * Data for this function is collected from the LLVM debug metadata.
     * @details
     * Example:
     * globals @a, @g will be present in the map.
     * %result = alloca i32, align 4
     * call void @llvm.dbg.value(metadata i32* %result, metadata !0, ...)
     *
     * here %result will also be present as a local source and will
     * return true for representsSourceVariable.
     *
     * @see debug_source_var
     */
    bool hasDebugSourceVariable();

    /**
     * Returns true if this operand satisfies any one of:
     * 1. has a debug source variable (see hasDebugSourceVariable)
     * 2. is a global variable (although there is no debug info available)
     * 3. is a formal argument
     * 4. is an "alloca" variable (debug info may be available)
     */
    bool isSource();

    /// @see hasDebugSourceVariable
    void setDebugSourceVariable(llvm::DILocalVariable *debug_source_var);

    /// see hasDebugSourceVariable
    llvm::DILocalVariable *getDebugSourceVariable();

    // Clears the index vector
    void resetIndexVector();

    // Sets 'is_global_or_address_taken' to be true for this operand
    void setVariableGlobal();

    // Sets 'is_formal_argument' to be true for this operand
    void setFormalArgument();

    // --------------------------- APIs for the Legacy SLIM
    // ---------------------------

    // Returns the name of the operand
    llvm::StringRef getName();

    // Returns only name for structures (and not indices) in string format and
    // returns the same value as getName for other type of operands
    llvm::StringRef getOnlyName();

    // Get the main operand of an array. If it is not an array, return
    // the value itself.
    // This is a null-safe wrapper for gep_gep_main_operand.
    llvm::Value *getValueOfArray();

    // Returns the main operand of GEP_OPERATOR
    llvm::Value *get_gep_main_operand();
    // --------------------------------------------------------------------------------

    // indirect calls

    llvm::Function *getAsFunction();
    /**
     * @brief Extracts the function from this operand
     *
     * Operand can be a function or an constant aggregate type containing a
     * function
     */
    llvm::Function *extractFunction();

    SLIMOperand *getOperandWithin();
    SLIMOperand *getOperandForFunction(llvm::Function *); //returns SLIM Operand for function name

    // dynamic allocation
    bool isDynamicAllocationType();
    void setDynamicAllocationType();

    std::set<std::pair<SLIMOperand *, int> *> refers_to_source_values_with_indirection;
    void addRefersToSourceValuesIndirection(std::pair<SLIMOperand*, int> *sourceValue);
    std::set<std::pair<SLIMOperand *, int>*> &getRefersToSourceValuesIndirection();
};

namespace OperandRepository {
// Check whether a SLIMOperand object corresponds to a global or a
// address-taken local variable or not
extern std::map<llvm::Value *, SLIMOperand *> value_to_slim_operand;

// Contains the return operand of every function
extern std::map<llvm::Function *, SLIMOperand *> function_return_operand;

/**
 * Returns the SLIMOperand object if already exists, otherwise returns a
 * nullptr Use isSLIMOperandAvailable or getOrCreateSLIMOperand
 */
SLIMOperand *getSLIMOperand(llvm::Value *value);

SLIMOperand *getOrCreateSLIMOperand(llvm::Value *value);

SLIMOperand *
getOrCreateSLIMOperand(llvm::Value *value, bool is_global_or_address_taken,
                       llvm::Function *direct_callee_function = nullptr);

bool isSLIMOperandAvailable(llvm::Value *value);

// Set the SLIMOperand object corresponding to a LLVM Value object
void setSLIMOperand(llvm::Value *value, SLIMOperand *slim_operand);

/**
 * Returns the return operand of a function
 */
SLIMOperand *getFunctionReturnOperand(llvm::Function *function);

// Sets the return operand of a function
void setFunctionReturnOperand(llvm::Function *function,
                              SLIMOperand *return_operand);
}; // namespace OperandRepository
