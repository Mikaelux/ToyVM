#include<stdio.h>
#include<stdlib.h>
#include"error.h"

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

  exit(err);

}


