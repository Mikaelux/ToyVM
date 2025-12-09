#ifndef COVERAGE_H
#define COVERAGE_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<limits.h>
#include<stdint.h>

//Coverage-related

#define VM_COVERAGE_MAP_SIZE 65536
#define ASM_COVERAGE_MAP_SIZE 65536

extern uint8_t *vm_coverage_map;
extern uint8_t *asm_coverage_map;

extern uint32_t __prev_vm_loc;
extern uint32_t __prev_asm_loc;

void coverage_init_shared(void);

static inline uint32_t hash_edge(uint32_t prev, uint32_t cur) {
    uint32_t x = cur ^ (prev >> 1);
    x ^= x >> 4;
    x ^= x << 10;
    x ^= x >> 7;
    return x;
}

static inline void record_vm(uint32_t loc){
  uint32_t edge = hash_edge(__prev_vm_loc, loc) % VM_COVERAGE_MAP_SIZE;
  vm_coverage_map[edge]++;
  __prev_vm_loc = loc >> 1;
}

static inline void record_asm(uint32_t loc){
  uint32_t edge = hash_edge(__prev_asm_loc, loc) % ASM_COVERAGE_MAP_SIZE;
  asm_coverage_map[edge]++;
  __prev_asm_loc = loc >> 1;
}

void vm_coverage_reset();
void vm_coverage_write(const char* path);
uint32_t vm_coverage_count_bits();

void asm_coverage_reset();
void asm_coverage_write(const char* path);
uint32_t asm_coverage_count_bits();



#endif
