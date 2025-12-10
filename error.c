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
  state_update_vm_error(current_state, err);
  exit(err);
}

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
  state_update_asm_error(current_state, err);
  exit(err);

}

//test 
/* int main(){
  report_asm_error(ERR_DIVIDE_BY_ZERO, 2, "token", "detail");
  return 0;
}
*/
