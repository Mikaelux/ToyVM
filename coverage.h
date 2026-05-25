#ifndef COVERAGE_H
#define COVERAGE_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<limits.h>
#include<stdint.h>

//Coverage-related

#define VM_COVERAGE_MAP_SIZE 4096

extern uint8_t *vm_coverage_map;

extern uint32_t __prev_vm_loc;

static inline uint32_t hash_edge(uint32_t prev, uint32_t cur) { //murmur hash
    uint32_t h = prev ^ (cur * 2654435761u);
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

static inline void record_vm(uint32_t loc){
  uint32_t edge = hash_edge(__prev_vm_loc, loc) % VM_COVERAGE_MAP_SIZE;
  vm_coverage_map[edge]++;
  __prev_vm_loc = loc >> 1;
}


void vm_coverage_reset();
void vm_coverage_write(const char* path);
uint32_t vm_coverage_count_bits();




#endif
