#include<stdio.h>
#include<stdlib.h>
#include"error.h"
#include"fuzzers/rl_bridge/state.h"

void report_vm_error(Errors err, int pc, 
                     const char* instr, const char* detail){
fprintf(stderr, 
"{"
         "\"stage\":\"runtime\","
         "\"error\":%d,"
         "\"ip\":%d,"
         "\"instruction\":\"%s\","
         "\"msg\":\"%s\""
        "}\n",
    err, pc, instr, detail
        );
  switch(err) {
        case ERR_STACK_OVERFLOW:
        case ERR_STACK_UNDERFLOW:
        case ERR_UNRESOLVED_LABEL:
        case ERR_DIVIDE_BY_ZERO:
        case ERR_CALLSTACK_OVERFLOW:
        case ERR_CALLSTACK_UNDERFLOW:
        case ERR_PC_OUT_OF_BOUNDS:
        case ERR_MAX_INSTRUCTIONS:
        case ERR_REGISTER_OUT_OF_BOUNDS:
        case ERR_UNKNOWN_OPCODE:
            exit(err);

        default:
            abort();
    }}

void report_asm_error(Errors err, int pc, 
                      const char* token, const char* detail){

  fprintf(stderr, 
"{"
         "\"stage\":\"runtime\","
         "\"error\":%d,"
         "\"ip\":%d,"
         "\"instruction\":\"%s\","
         "\"msg\":\"%s\""
        "}\n",
    err, pc, token, detail
        );
  exit(err);


}

//test 
/* int main(){
  report_asm_error(ERR_DIVIDE_BY_ZERO, 2, "token", "detail");
  return 0;
}
*/
