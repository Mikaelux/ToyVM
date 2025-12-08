#ifndef HEADER_H
#define HEADER_H
#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<stddef.h>

#define MAXSTEPS 100000
#define CALLSIZE 124
#define MAXLABELS 124
#define STACKSIZE 256
#define MEMSIZE 1024
typedef enum {IMM, REG, LABEL, NONE} OperandType;
typedef enum {PSH, ADD, SUB, MUL, DIV, POP, SET, LOAD, HLT, LBL, JMP, JE, JNE, JG, JGE, JL, JLE, CMP, CALL, RET, INC, DEC, OPCODE} Operations;
typedef struct VM VM;
typedef struct Instr Instr;
typedef struct Label Label;
typedef void (*InstrFunc)(VM*, const Instr*);
typedef struct Flags{
  bool of; // substraction overflow,
  bool sf; // sign + - of substract result
  bool zf; // is the result zero
}Flags;

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

typedef enum {A, B, C, D, E, NUMOFREGS} Regs;

typedef struct Label{
  char name[64];
  int address;
} Label;

typedef struct VM {
  int call_sp;
  int callstack[CALLSIZE];
  int stack[STACKSIZE];
  int sp;
  int ip;
  int lb;
  int stepcount;
  Flags flags;
  bool running;
  int registers[NUMOFREGS];
  const Instr *program;
  Label labels[MAXLABELS];
} VM;


//helper function
void label_parse(VM* vm, int program_size);
int assess_operand(VM*vm, Operand op);
//instruction
void instr_psh(VM*vm, const Instr* instrc);
void instr_add(VM*vm, const Instr* instrc);
void instr_sub(VM*vm, const Instr* instrc);
void instr_mul(VM*vm, const Instr* instrc);
void instr_div(VM*vm, const Instr* instrc);
void instr_pop(VM*vm, const Instr* instrc);
void instr_set(VM*vm, const Instr* instrc);
void instr_load(VM*vm, const Instr* instrc);
void instr_hlt(VM*vm, const Instr* instrc);
void instr_jmp(VM*vm, const Instr* instrc);
void instr_cmp(VM*vm, const Instr* instrc);
void instr_lbl(VM*vm, const Instr* instrc);
void instr_je(VM*vm, const Instr* instrc);
void instr_jne(VM*vm, const Instr* instrc);
void instr_jg(VM*vm, const Instr* instrc);
void instr_jge(VM*vm, const Instr* instrc);
void instr_jl(VM*vm, const Instr* instrc);
void instr_jle(VM*vm, const Instr* instrc);
void instr_call(VM*vm, const Instr* instrc);
void instr_ret(VM*vm, const Instr* instrc);
void instr_inc(VM*vm, const Instr* instrc);
void instr_dec(VM*vm, const Instr* instrc);



//ASSEMBLER ONLY DEFINITIONS


#define OP_NONE  (0 << 0)  // 0b000
#define OP_IMM   (1 << 0)  // 0b001
#define OP_REG   (1 << 1)  // 0b010
#define OP_LABEL (1 << 2)  // 0b100

typedef struct Instr_template{
  Operations ID;
  int min_operand;
  int max_operand;
  InstrFunc execute;
  int validOp[2];
} Instr_template;

extern Instr_template lookup[];
extern const char* operation_names[];


void define_program(Instr **out_program, int *out_size, Label **out_labels, int *out_label_count);
void free_program(Instr* program, int program_size);

//label stuff, just for reference
Label* parse_labels(Instr* program, int program_size, int*out_lb_count, int max_labels);

int vm_main(void);

#endif

