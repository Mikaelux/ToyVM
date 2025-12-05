#ifndef ERROR_H
#define ERROR_H

typedef enum {
    ERR_OK = 0,
    
    ERR_SYNTAX,                 // Generic syntax error (bad mnemonic, wrong line)
    ERR_INVALID_TOKEN,          // Unknown token (invalid mnemonic or operand)
    ERR_TOO_FEW_OPERANDS,       // Not enough operands for an instruction
    ERR_TOO_MANY_OPERANDS,      // Too many operands for an instruction
    ERR_LINE_TOO_LONG,          // Source code line exceeds MAX_LINES_LENGTH
    ERR_TOKEN_TOO_LONG,
    ERR_INVALID_REGISTER,       // Register name not recognized or out of bounds
    ERR_INVALID_LITERAL,        // Immediate is not a valid integer
    ERR_OPERAND_OUT_OF_RANGE,   // Immediate out of integer range
    ERR_TOO_MANY_LINES,

    // Label table
    ERR_DUPLICATE_LABEL,        // Label was defined twice
    ERR_UNRESOLVED_LABEL,       // Used label does not exist
    ERR_TOO_MANY_LABELS,        // Exceeded MAXLABELS
    ERR_LABEL_TOO_LONG,
    

    ERR_CALLSTACK_OVERFLOW,
    ERR_CALLSTACK_UNDERFLOW,
    
    // Allocation
    ERR_ALLOC_FAIL,             // Out of memory (malloc failure)

    // VM Execution
    ERR_UNKNOWN_OPCODE,         // Encountered an unknown opcode (program bug)
    ERR_PC_OUT_OF_BOUNDS,       // Instruction pointer out of bounds
    ERR_STACK_OVERFLOW,         // Stack overflow (push or load)
    ERR_STACK_UNDERFLOW,        // Stack underflow (pop or less-than-needed operands)
    ERR_DIVIDE_BY_ZERO,         // Division by zero
    ERR_REGISTER_OUT_OF_BOUNDS, // Register index is out of bounds
    ERR_MISSING_HALT,

    // Fuzzing/Testing
    ERR_TIMEOUT,                // VM ran for too long (timeout)
    ERR_MAX_INSTRUCTIONS,       // Exceeded MAXSTEPS in main loop
    ERR_EMPTY_PROGRAM,          // Assembler returned 0 instructions

    // General I/O
    ERR_IO,           

    ERR_UNKNOWN //catch-all error incase not defined
} Errors;

void report_vm_error(Errors err, int pc, const char* instr, const char* detail);
void report_asm_error(Errors err, int pc, const char* token, const char* detail);


#endif
