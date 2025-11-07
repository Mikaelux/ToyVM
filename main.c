#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include "header.h"

#define MAXSTEPS 100000 //for catching infinite loops early and preventing DDOS 
// Include the comparison function and flags structures
//
int assess_operand(VM* vm, const Instr *instrc, bool isoperand1){
  OperandType type = isoperand1? instrc->operand1_type : instrc->operand2_type;
  switch(type){
    case IMM:
      return isoperand1? instrc->operand1.imm : instrc->operand2.imm;
  case REG:
      return isoperand1? vm->registers[instrc->operand1.reg]: vm->registers[instrc->operand2.reg];
  case LABEL:
      printf("Invalid operand : label where num value is expected");
      exit(1);
  case NONE:
      exit(1);
  }
}

void instr_psh(VM*vm, const Instr* instrc){
  if(vm->sp>=255){
    printf("Stack overflow, cannot add\n");
    exit(1);
  }
  vm->stack[++vm->sp] = instrc->operand1.imm;
}

void instr_add(VM*vm, const Instr* instrc){
  if(vm->sp<1){
    printf("Not enough operands\n");
  }
  int op1 = vm->stack[vm->sp--];
  int op2 = vm->stack[vm->sp--];
  vm->stack[++vm->sp] = op1+op2;
}

void instr_sub(VM*vm, const Instr* instrc){ 
  if(vm->sp<1){
    printf("Not enough operands\n");
  }

  int op1 = vm->stack[vm->sp--];
  int op2 = vm->stack[vm->sp--];
  vm->stack[++vm->sp] = op2-op1;
}

void instr_mul(VM*vm, const Instr* instrc){ 
  if(vm->sp<1){
    printf("Not enough operands\n");
  }

  int op1 = vm->stack[vm->sp--];
  int op2 = vm->stack[vm->sp--];
  vm->stack[++vm->sp] = op1*op2;
}

void instr_div(VM*vm, const Instr* instrc){ 
  if(vm->sp<1){
    printf("Not enough operands\n");
  }

  int op1 = vm->stack[vm->sp--];
  int op2 = vm->stack[vm->sp--];

  if(op2 == 0){
    printf("Division invalid\n");
    exit(1);
  }
  vm->stack[++vm->sp] = op1/op2;
}

void instr_pop(VM*vm, const Instr* instrc){
  if(vm->sp<0){
    printf("Stack underflow\n");
    exit(1);
  }
  printf("The value popped is %d\n", vm->stack[vm->sp--]);
}

void instr_set(VM*vm, const Instr* instrc){
  Regs reg = instrc->operand1.reg;
  if(reg>256){
    printf("Not in registers\n");
  }
  int value = instrc->operand2.imm;
  vm->registers[reg] = value;
}

void instr_load(VM*vm, const Instr* instrc){ 
  if(vm->sp > 256){
    printf("Stack overflow\n");
  }
  int value = instrc->operand1.reg;
  int reg_value = vm->registers[value];
  vm->stack[++vm->sp] = reg_value;
}

void instr_hlt(VM*vm, const Instr* instrc){
  vm->running=false;
printf("Program has ended\n");
}

void label_parse(VM* vm, int program_size){
for(int i=0; i<program_size; i++){
    if (vm->program[i].ID == LBL){
      strcpy(vm->labels[vm->lb].name, vm->program[i].operand1.label);
      vm->labels[vm->lb].address = i;
      vm->lb++;
    }
}
}

void instr_lbl(VM *vm, const Instr* instrc){}

void instr_cmp(VM*vm, const Instr* instrc){
  int a = assess_operand(vm, instrc, true);
  int b = assess_operand(vm, instrc, false);
  int assess = a - b;

  vm->flags.zf = (assess == 0);
  vm->flags.sf = (assess < 0);
  vm->flags.of = ((a < 0 && b > 0 && assess > 0) ||
                  (a > 0 && b < 0 && assess < 0));

}

void instr_jmp(VM* vm, const Instr* instrc) {
  for (int i = 0; i < vm->lb; i++) {
    if (strcmp(vm->labels[i].name, instrc->operand1.label) == 0) {
      vm->ip = vm->labels[i].address;
      return;
    }
  }
  printf("Label not found: %s\n", instrc->operand1.label);
}

void instr_je(VM*vm, const Instr* instrc){
    if(vm->flags.zf)instr_jmp(vm, instrc);
}

void instr_jne(VM*vm, const Instr* instrc){
    if(!vm->flags.zf)instr_jmp(vm, instrc);
}

void instr_jg(VM*vm, const Instr* instrc){
  if(vm->flags.sf == vm->flags.of){
    instr_jmp(vm, instrc);
  }
}

void instr_jge(VM*vm, const Instr* instrc){
  if ((vm->flags.sf==vm->flags.of) || (vm->flags.zf == 0)){
    instr_jmp(vm, instrc);
  }
}

void instr_jl(VM*vm, const Instr* instrc){
   if(vm->flags.sf != vm->flags.of){
    instr_jmp(vm, instrc);
  }
}

void instr_jle(VM*vm, const Instr* instrc){
 if((vm->flags.sf != vm->flags.of) || (vm->flags.zf == 0)){
    instr_jmp(vm, instrc);
  }
}

void instr_call(VM*vm, const Instr* instrc){
  vm->callstack[++vm->call_sp] = vm->ip;
  instr_jmp(vm, instrc);
}

void instr_ret(VM*vm, const Instr* instrc){
  vm->ip = vm->callstack[vm->call_sp--];
}

const Instr program[] = {
  { .ID = LBL, .operand1_type = LABEL, .operand1.label = "main", .execute = instr_lbl },

  { .ID = PSH, .operand1_type = IMM, .operand1.imm = 5, .execute = instr_psh },
  { .ID = CALL, .operand1_type = LABEL, .operand1.label = "double_val", .execute = instr_call },
  { .ID = PSH, .operand1_type = IMM, .operand1.imm = 10, .execute = instr_psh },
  { .ID = ADD, .operand1_type = NONE, .operand2_type = NONE, .execute = instr_add },
  { .ID = POP, .operand1_type = NONE, .operand2_type = NONE, .execute = instr_pop },
  { .ID = HLT, .operand1_type = NONE, .operand2_type = NONE, .execute = instr_hlt },

  { .ID = LBL, .operand1_type = LABEL, .operand1.label = "double_val", .execute = instr_lbl},
  { .ID = PSH, .operand1_type = IMM, .operand1.imm = 2, .execute = instr_psh },
  { .ID = MUL, .operand1_type = NONE, .operand2_type = NONE, .execute = instr_mul },
  { .ID = RET, .operand1_type = NONE, .operand2_type = NONE, .execute = instr_ret }

};
const int program_size = sizeof(program) / sizeof(program[0]);


int main(){

  VM vm = {
    .call_sp = -1,
    .stepcount = 0,
    .lb = 0,
    .sp = -1,
    .ip= 0,
    .running = true,
    .program = program,
  };
  label_parse(&vm, program_size);

  while (vm.running) {
        const Instr* instr = &vm.program[vm.ip++];
        instr->execute(&vm, instr);
        vm.stepcount++;

        if(vm.stepcount == MAXSTEPS){
          vm.running = false;
    }
    }
  return 0;
}
