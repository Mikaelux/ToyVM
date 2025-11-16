#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include "header.h"

#define MAXSTEPS 100000 //for catching infinite loops early and preventing DDOS 
// Include the comparison function and flags structures


int assess_operand(VM* vm, Operand op){
  OperandType type = op.type;
  switch(type){
    case IMM:
      return op.value.imm;
  case REG:
      return vm->registers[op.value.reg];
  case LABEL:
      printf("Invalid operand : label where num value is expected");
  case NONE:
      exit(1);
  default:
      printf("no op");
      exit(1);
  }
}

void instr_psh(VM*vm, const Instr* instrc){
  if(vm->sp>=STACKSIZE - 1){
    printf("Stack overflow, cannot add\n");
    exit(1);
  }
  vm->stack[++vm->sp] = instrc->operand1.value.imm;
}

void instr_add(VM*vm, const Instr* instrc){
  if(vm->sp<1){
    printf("Not enough operands\n");
    exit(1);
  }
  int op1 = vm->stack[vm->sp--];
  int op2 = vm->stack[vm->sp--];
  vm->stack[++vm->sp] = op1+op2;
}

void instr_sub(VM*vm, const Instr* instrc){ 
  if(vm->sp<1){
    printf("Not enough operands\n");
    exit(1);
  }

  int op1 = vm->stack[vm->sp--];
  int op2 = vm->stack[vm->sp--];
  vm->stack[++vm->sp] = op2-op1;
}

void instr_mul(VM*vm, const Instr* instrc){ 
  if(vm->sp<1){
    printf("Not enough operands\n");
    exit(1);
  }

  int op1 = vm->stack[vm->sp--];
  int op2 = vm->stack[vm->sp--];
  vm->stack[++vm->sp] = op1*op2;
}

void instr_div(VM*vm, const Instr* instrc){ 
  if(vm->sp<1){
    printf("Not enough operands\n");
    exit(1);
  }

  int op1 = vm->stack[vm->sp--];
  int op2 = vm->stack[vm->sp--];

  if(op2 == 0){
    printf("Division invalid\n");
    exit(1);
  }
  vm->stack[++vm->sp] = op2/op1;
}

void instr_pop(VM*vm, const Instr* instrc){
  if(vm->sp<0){
    printf("Stack underflow\n");
    exit(1);
  }
  printf("The value popped is %d\n", vm->stack[vm->sp--]);
}

void instr_set(VM*vm, const Instr* instrc){
  Regs reg = instrc->operand1.value.reg;
  if(reg>=NUMOFREGS || reg < 0){
    printf("Not in registers\n");
  }
  int value = instrc->operand2.value.imm;
  vm->registers[reg] = value;
}

void instr_load(VM*vm, const Instr* instrc){ 
  if(vm->sp > STACKSIZE - 1){
    printf("Stack overflow\n");
  }
  int value = instrc->operand1.value.reg;
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
      strcpy(vm->labels[vm->lb].name, vm->program[i].operand1.value.label);
      vm->labels[vm->lb].address = i;
      vm->lb++;
    }
}
}

void instr_lbl(VM *vm, const Instr* instrc){}

void instr_cmp(VM*vm, const Instr* instrc){
  int a = assess_operand(vm, instrc->operand1);
  int b = assess_operand(vm, instrc->operand2);
  int assess = a - b;

  vm->flags.zf = (assess == 0);
  vm->flags.sf = (assess < 0);
  vm->flags.of = ((a < 0 && b > 0 && assess > 0) ||
                  (a > 0 && b < 0 && assess < 0));

}

void instr_jmp(VM* vm, const Instr* instrc) {
  for (int i = 0; i < vm->lb; i++) {
    if (strcmp(vm->labels[i].name, instrc->operand1.value.label) == 0) {
      vm->ip = vm->labels[i].address;
      return;
    }
  }
  printf("Label not found: %s\n", instrc->operand1.value.label);
  exit(1);
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
  if ((vm->flags.sf==vm->flags.of) || (vm->flags.zf)){
    instr_jmp(vm, instrc);
  }
}

void instr_jl(VM*vm, const Instr* instrc){
   if(vm->flags.sf != vm->flags.of){
    instr_jmp(vm, instrc);
  }
}

void instr_jle(VM*vm, const Instr* instrc){
 if((vm->flags.sf != vm->flags.of) || (vm->flags.zf)){
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

void instr_inc(VM*vm, const Instr* instrc){
  int register_index = instrc->operand1.value.reg;
  vm->registers[register_index]++;
}

void instr_dec(VM*vm, const Instr* instrc){
  int register_index = instrc->operand1.value.reg;
  vm->registers[register_index]--;
}


/* //test 
const Instr program[] = {
   {PSH, {.type = IMM, .value.imm = 10}, {.type = NONE}, instr_psh},
    {PSH, {.type = IMM, .value.imm = 20}, {.type = NONE}, instr_psh},
    {ADD, {.type = NONE}, {.type = NONE}, instr_add},        // stack = [30]
    {POP, {.type = NONE}, {.type = NONE}, instr_pop},        // pop result

    // Register ops
    {SET, {.type = REG, .value.reg = A}, {.type = IMM, .value.imm = 5}, instr_set},
    {SET, {.type = REG, .value.reg = B}, {.type = IMM, .value.imm = 10}, instr_set},
    {LOAD, {.type = REG, .value.reg = A}, {.type = NONE}, instr_load},  // push 5
    {LOAD, {.type = REG, .value.reg = B}, {.type = NONE}, instr_load},  // push 10
    {MUL, {.type = NONE}, {.type = NONE}, instr_mul},        // stack = [50]
    {POP, {.type = NONE}, {.type = NONE}, instr_pop},

    // Comparison and jump test
    {SET, {.type = REG, .value.reg = C}, {.type = IMM, .value.imm = 3}, instr_set},
    {SET, {.type = REG, .value.reg = D}, {.type = IMM, .value.imm = 3}, instr_set},
    {CMP, {.type = REG, .value.reg = C}, {.type = REG, .value.reg = D}, instr_cmp},
    {JE, {.type = LABEL, .value.label = "equal_label"}, {.type = NONE}, instr_je},

    // If not equal (should skip)
    {PSH, {.type = IMM, .value.imm = 999}, {.type = NONE}, instr_psh},
    {POP, {.type = NONE}, {.type = NONE}, instr_pop},

    // Equal branch
    {LBL, {.type = LABEL, .value.label = "equal_label"}, {.type = NONE}, instr_lbl},
    {PSH, {.type = IMM, .value.imm = 123}, {.type = NONE}, instr_psh},
    {POP, {.type = NONE}, {.type = NONE}, instr_pop},

    // Call/Return test
    {CALL, {.type = LABEL, .value.label = "func"}, {.type = NONE}, instr_call},
    {HLT, {.type = NONE}, {.type = NONE}, instr_hlt},

    // Function definition
    {LBL, {.type = LABEL, .value.label = "func"}, {.type = NONE}, instr_lbl},
    {PSH, {.type = IMM, .value.imm = 42}, {.type = NONE}, instr_psh},
    {POP, {.type = NONE}, {.type = NONE}, instr_pop},
    {RET, {.type = NONE}, {.type = NONE}, instr_ret}
};
const int program_size = sizeof(program) / sizeof(program[0]);
*/

int main(){
  define_program();
  VM vm = {
    .call_sp = -1,
    .stepcount = 0,
    .lb = 0,
    .sp = -1,
    .ip= 0,
    .running = true,
    .program = u_program,
  };
  label_parse(&vm, u_program_size);
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
