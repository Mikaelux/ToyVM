#ifndef STATE_H
#define STATE_H
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include "../../header.h"
#include "../../error.h"

#define NUM_NUM_FEATURES 6
#define OPTYPE_COUNTERS 5

typedef struct State{
  //from assembler 
  float op_histogram[OPCODE]; //opcode historgram for instruction recc count 
  float numeric_features[NUM_NUM_FEATURES]; //numeric features like operand mean, max and min, number of zeroes 
  float optype_count[OPTYPE_COUNTERS]; //number of stack and arithmetic ops 
  //from fuzzer
  float vm_error_onehot[ERR_COUNT]; //errors for vm exec 
  float asm_error_onehot[ERR_COUNT]; //errors for asm exec 
  float coverage_delta; //coverage difference
  float crashes; //nb of crashes 
} State;

#define STATE_VECTOR_SIZE(s) \
    (sizeof((s)->numeric_features)/sizeof(float) + \
     sizeof((s)->op_histogram)/sizeof(float) + \
     sizeof((s)->optype_count)/sizeof(float) + \
     sizeof((s)->vm_error_onehot)/sizeof(float) + \
     sizeof((s)->asm_error_onehot)/sizeof(float) + \
     2)

extern State* current_state;

void state_init(State* s);
void state_update_histogram(State* s, int t_prog_size, Instr* t_prog);
void state_update_num_features(State *s, int t_prog_size, Instr* t_prog); //accounts for optpe and linecount 
                                                                        

void state_update_vm_error(State *s, Errors err);
void state_update_asm_error(State *s, Errors err);
void state_update_run_stats(State *s, uint32_t vm_cov, uint32_t asm_cov, int crashes); //accounts for coverage and crashes 

void state_reset(State *s);
void state_serialize(State* s, float* out_vector);
#endif
