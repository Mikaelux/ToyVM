#ifndef VM_TYPES_H
#define VM_TYPES_H
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>

typedef enum {IMM, REG, LABEL, NONE} OperandType;
typedef enum {PSH, ADD, SUB, MUL, DIV, POP, SET, LOAD, HLT, LBL, JMP, JE, JNE, JG, JGE, JL, JLE, CMP, CALL, RET, OPCODE} Operations;
typedef enum {A, B, C, D, E, NUMOFREGS} Regs;
typedef struct VM VM;
typedef struct Instr Instr;
typedef struct Label Label;
typedef void (*InstrFunc)(VM*, const Instr*);

typedef struct Operand{
   OperandType type;
    union{
      int imm;
      int reg;
      const char* label;
    } value;
}Operand;

typedef struct Instr{

  Operations ID;
  Operand operand1;
  Operand operand2;
  InstrFunc execute;

} Instr;

#endif
