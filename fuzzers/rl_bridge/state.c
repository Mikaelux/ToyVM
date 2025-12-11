#include<string.h>
#include "state.h"
#include<float.h>
#include"../../header.h"
State* current_state = NULL;

void state_init(State* s){
  memset(s->op_histogram, 0, sizeof(s->op_histogram));
  memset(s->numeric_features, 0, sizeof(s->numeric_features));
  memset(s->optype_count, 0, sizeof(s->optype_count));
  memset(s->vm_error_onehot, 0, sizeof(s->vm_error_onehot));
  memset(s->asm_error_onehot, 0, sizeof(s->asm_error_onehot));
  s->coverage_delta = 0;
  s->crashes = 0;
}


void state_update_histogram(State* s, int t_prog_size, Instr* t_prog){
  if(!s || !t_prog || t_prog_size == 0) return;
  int index = -1;
  for(int i=0; i<t_prog_size; i++){
    index = t_prog[i].ID;
    s->op_histogram[index]++;
  }
}

void state_update_num_features(State *s, int t_prog_size, Instr* t_prog){
//account for: [0]: number of operands, mean of numericals, min num, max num, num of labels, num of jumps, prog length.
  float sumofops = 0, sumofimm = 0, min= FLT_MAX, max=-FLT_MAX; 
  float count = 0;
  float arith_op_counter = 0, stack_op_counter = 0, lb_op_counter = 0, jmp_op_counter = 0, callst_op_counter = 0;

  if(!s) return;
  for(int i=0; i<t_prog_size; i++){
     Operand op1 = t_prog[i].operand1, op2 = t_prog[i].operand2;
      if(op1.type != NONE) sumofops+= 1.0;
      if(op2.type != NONE) sumofops+= 1.0;
      
      if(op1.type == IMM){
        if(op1.value.imm >= -100000 && op1.value.imm <= 100000){
          count+= 1.0;
          sumofimm += op1.value.imm; 
          if(op1.value.imm < min) min = op1.value.imm;
          if(op1.value.imm > max) max = op1.value.imm;
        }
      }

      if(op2.type == IMM){
        if(op1.value.imm >= -100000 && op1.value.imm <= 100000){
          count+=1.0;
          sumofimm += op2.value.imm;
          if(op2.value.imm < min) min = op2.value.imm;
          if(op2.value.imm > max) max = op2.value.imm;
        }
      }
    
    if(t_prog[i].ID == LBL) lb_op_counter+=1.0;
    if(t_prog[i].ID == JMP || t_prog[i].ID == JE || t_prog[i].ID == JNE || t_prog[i].ID == JG || t_prog[i].ID == JGE || t_prog[i].ID == JL || t_prog[i].ID == JLE) jmp_op_counter+=1.0;
    if(t_prog[i].ID == PSH || t_prog[i].ID == POP) stack_op_counter+=1.0;
    if(t_prog[i].ID == ADD || t_prog[i].ID == SUB || t_prog[i].ID == MUL || t_prog[i].ID == DIV) arith_op_counter+=1.0;
    if(t_prog[i].ID == CALL || t_prog[i].ID == RET) callst_op_counter+=1.0;
    }

  float num_mean = 0;
  if(count > 0){
      num_mean = sumofimm/count;
    } else {
    num_mean = 0;
    min = 0;
    max = 0;
  }

  s->optype_count[0] = stack_op_counter;
  s->optype_count[1] = arith_op_counter;
  s->optype_count[2] = lb_op_counter;
  s->optype_count[3] = jmp_op_counter;
  s->optype_count[4] = callst_op_counter;

  s->numeric_features[0] = sumofops;
  s->numeric_features[1] = sumofimm;
  s->numeric_features[2] = num_mean;
  s->numeric_features[3] = min;
  s->numeric_features[4] = max;
  s->numeric_features[5] = t_prog_size;
}

void state_update_vm_error(State *s, Errors err){
  if(!s) return;
  if(err < 0 || err >= ERR_COUNT) return;
  s->vm_error_onehot[err] = 1.0f;
}

void state_update_asm_error(State *s, Errors err){
  if(!s) return;
  if(err < 0 || err >= ERR_COUNT) return;
  s->asm_error_onehot[err] = 1.0f;
}

void state_update_run_stats(State *s, uint32_t vm_cov, uint32_t asm_cov, int crashes){
  //crashes and coverage delta 
  if(!s) return;
  s->crashes = (float)crashes;
  s->coverage_delta = (float)vm_cov + (float)asm_cov;
}

void state_reset(State *s){
  state_init(s);
}

void state_serialize(State* s, float* out_vector){
  int offset = 0;
  
  memcpy(out_vector + offset, s->numeric_features, sizeof(s->numeric_features));
  offset += sizeof(s->numeric_features)/sizeof(s->numeric_features[0]);

  memcpy(out_vector + offset, s->op_histogram, sizeof(s->op_histogram));
  offset += sizeof(s->op_histogram)/sizeof(s->op_histogram[0]);

  memcpy(out_vector + offset, s->optype_count, sizeof(s->optype_count));
  offset += sizeof(s->optype_count)/sizeof(s->optype_count[0]);

  memcpy(out_vector + offset, s->vm_error_onehot, sizeof(s->vm_error_onehot));
  offset += sizeof(s->vm_error_onehot)/sizeof(s->vm_error_onehot[0]);

  memcpy(out_vector + offset, s->asm_error_onehot, sizeof(s->asm_error_onehot));
  offset += sizeof(s->asm_error_onehot)/sizeof(s->asm_error_onehot[0]);
  
  out_vector[offset++] = s->coverage_delta;
  out_vector[offset++] = s->crashes;
}

