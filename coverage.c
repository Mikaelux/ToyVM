#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<limits.h>
#include <sys/mman.h>
#include"coverage.h"

uint8_t *vm_coverage_map;
uint8_t *asm_coverage_map;

uint32_t __prev_vm_loc = 0;
uint32_t __prev_asm_loc = 0;

void coverage_init_shared(void) {
    if (vm_coverage_map != NULL) return;  // Already initialized
    
    vm_coverage_map = mmap(NULL, VM_COVERAGE_MAP_SIZE,
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    asm_coverage_map = mmap(NULL, ASM_COVERAGE_MAP_SIZE,
                            PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    
    if (vm_coverage_map == MAP_FAILED || asm_coverage_map == MAP_FAILED) {
        perror("mmap failed for coverage maps");
        exit(1);
    }
    
    memset(vm_coverage_map, 0, VM_COVERAGE_MAP_SIZE);
    memset(asm_coverage_map, 0, ASM_COVERAGE_MAP_SIZE);
}

void asm_coverage_reset(){
    memset(asm_coverage_map, 0, ASM_COVERAGE_MAP_SIZE);
  __prev_asm_loc = 0;
}

void asm_coverage_write(const char* path){
  FILE *f = fopen(path, "wb");
  if(!f) return;
  fwrite(asm_coverage_map, 1, ASM_COVERAGE_MAP_SIZE, f);
  fclose(f);
}

uint32_t asm_coverage_count_bits(){
  uint32_t count = 0;
  for(int i=0; i< ASM_COVERAGE_MAP_SIZE; i++){
    if(asm_coverage_map[i] > 0) count ++;
  }
  return count;
}


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


