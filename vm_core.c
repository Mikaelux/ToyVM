#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include "header.h"
#include"error.h"


void vm_coverage_reset(){
    memset(vm_coverage_map, 0, VM_COVERAGE_MAP_SIZE);
  __prev_vm_loc = 0;
}

void vm_coverage_write(const char* path){
  FILE *f = fopen(path, "wb");
  if(!f) return;
  fwrite(vm_coverage_map, 1, VM_COVERAGE_MAP_SIZE, f);
  fclose(f);
}

uint32_t vm_coverage_count_bits(){
  uint32_t count = 0;
  for(int i=0; i< VM_COVERAGE_MAP_SIZE; i++){
    if(vm_coverage_map[i] > 0) count ++;
  }
  return count;
}

int assess_operand(VM* vm, Operand op){
  if(!vm){
    report_vm_error(ERR_IO, vm->ip, NULL, "input processing failed: VM-assessOperand");
  }

  OperandType type = op.type;
  switch(type){
    case IMM:
      return op.value.imm;
  case REG:
      if(op.value.reg < 0 || op.value.reg >= NUMOFREGS){
        report_vm_error(ERR_REGISTER_OUT_OF_BOUNDS, vm->ip, NULL, "Register index used invalid");
      }
      return vm->registers[op.value.reg];
  case LABEL:
      report_vm_error(ERR_INVALID_TOKEN, vm->ip, NULL, "Label operand invalid in this position");
  case NONE:
      report_vm_error(ERR_SYNTAX, vm->ip, NULL, "Operation requires a REG/IMM operand");
  default:
       report_vm_error(ERR_SYNTAX, vm->ip, NULL, "Operation requires a REG/IMM operand");
  }
}

void instr_psh(VM*vm, const Instr* instrc){
  (void)instrc;
  if(vm->sp>=STACKSIZE - 1){
     report_vm_error(ERR_STACK_OVERFLOW, vm->ip, "PSH", "Stack overflow, can't push further\n");
  }
  vm->stack[++vm->sp] = instrc->operand1.value.imm;
}

void instr_add(VM*vm, const Instr* instrc){
  (void)instrc;
  if(vm->sp<1) report_vm_error(ERR_STACK_UNDERFLOW, vm->ip, "ADD", "Stack doesn't contain enough operands for stack operation");
  int op1 = vm->stack[vm->sp--];
  int op2 = vm->stack[vm->sp--];
  vm->stack[++vm->sp] = op1+op2;
}

void instr_sub(VM*vm, const Instr* instrc){  
  (void)instrc;
if(vm->sp<1) report_vm_error(ERR_STACK_UNDERFLOW, vm->ip, "SUB", "Stack doesn't contain enough operands for stack operation");
  int op1 = vm->stack[vm->sp--];
  int op2 = vm->stack[vm->sp--];
  vm->stack[++vm->sp] = op2-op1;
}

void instr_mul(VM*vm, const Instr* instrc){  (void)instrc;

 if(vm->sp<1) report_vm_error(ERR_STACK_UNDERFLOW, vm->ip, "MUL", "Stack doesn't contain enough operands for stack operation");
  int op1 = vm->stack[vm->sp--];
  int op2 = vm->stack[vm->sp--];
  vm->stack[++vm->sp] = op1*op2;
}

void instr_div(VM*vm, const Instr* instrc){  (void)instrc;

 if(vm->sp<1) report_vm_error(ERR_STACK_UNDERFLOW, vm->ip, "DIV", "Stack doesn't contain enough operands for stack operation");
  int op1 = vm->stack[vm->sp--];
  int op2 = vm->stack[vm->sp--];

  if(op1 == 0){
    report_vm_error(ERR_DIVIDE_BY_ZERO, vm->ip, "DIV","Division can't be done by zero\n");
  }
  vm->stack[++vm->sp] = op2/op1;
}

void instr_pop(VM*vm, const Instr* instrc){ (void)instrc;

  if(vm->sp<0){
    report_vm_error(ERR_STACK_UNDERFLOW, vm->ip, "POP","Stack is empty, can't pop\n");

  }
  vm->sp--;
}

void instr_set(VM*vm, const Instr* instrc){
  Regs reg = instrc->operand1.value.reg;
  if(reg < 0 || reg >= NUMOFREGS) report_vm_error(ERR_REGISTER_OUT_OF_BOUNDS, vm->ip, "SET", "Register in input is invalid");
  int value = instrc->operand2.value.imm;
  vm->registers[reg] = value;
}

void instr_load(VM*vm, const Instr* instrc){ 
  if(vm->sp >= STACKSIZE - 1){
    report_vm_error(ERR_STACK_OVERFLOW, vm->ip, "LOAD", "Can't load anny more elements");
  }
  int value = instrc->operand1.value.reg;
  if(value < 0 || value >= NUMOFREGS) report_vm_error(ERR_REGISTER_OUT_OF_BOUNDS, vm->ip, "SET", "Register in input is invalid");
  int reg_value = vm->registers[value];
  vm->stack[++vm->sp] = reg_value;
}

void instr_hlt(VM*vm, const Instr* instrc){ (void)instrc;

  vm->running=false;
}

void instr_lbl(VM *vm, const Instr* instrc){
  (void)vm;
  (void)instrc;
}

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
  report_vm_error(ERR_UNRESOLVED_LABEL, vm->ip, "JMP", "label to jump not found");
}

void instr_je(VM*vm, const Instr* instrc){
   if (vm->flags.zf) instr_jmp(vm, instrc);
}
void instr_jne(VM*vm, const Instr* instrc){
   if (!vm->flags.zf) instr_jmp(vm, instrc);
}

void instr_jg(VM*vm, const Instr* instrc){
 if (vm->flags.sf==vm->flags.of){
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
  if(vm->call_sp + 1 >= CALLSIZE) report_vm_error(ERR_CALLSTACK_OVERFLOW, vm->ip, "CALL", "Call stack too full");
  vm->callstack[++vm->call_sp] = vm->ip;
  instr_jmp(vm, instrc);
}

void instr_ret(VM*vm, const Instr* instrc){
  (void)instrc;
  if(vm->call_sp < 0) report_vm_error(ERR_CALLSTACK_UNDERFLOW, vm->ip, "CALL", "Call stack empty");
  vm->ip = vm->callstack[vm->call_sp--];
}

void instr_inc(VM*vm, const Instr* instrc){
  int register_index = instrc->operand1.value.reg;
  if(register_index < 0 || register_index >= NUMOFREGS){
    report_vm_error(ERR_REGISTER_OUT_OF_BOUNDS, vm->ip, "INC", "Invalid register");
  }
  vm->registers[register_index]++;
}

void instr_dec(VM*vm, const Instr* instrc){
  int register_index = instrc->operand1.value.reg;
  if(register_index < 0 || register_index >= NUMOFREGS){
    report_vm_error(ERR_REGISTER_OUT_OF_BOUNDS, vm->ip, "INC", "Invalid register");
  }
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


