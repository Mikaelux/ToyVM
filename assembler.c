#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<limits.h>
#include"header.h"
#define MAX_LINES 100 
#define MAX_LINES_LENGTH 255

int u_program_size = 0;
Instr* u_program = NULL;

Instr_template lookup[OPCODE]= {
  {PSH, 1, 1, instr_psh},
  {ADD, 0, 0, instr_add},
  {SUB, 0, 0, instr_sub},
  {MUL, 0, 0, instr_mul},
  {DIV, 0, 0, instr_div},
  {POP, 0, 0, instr_pop},
  {SET, 2, 2, instr_set},
  {LOAD, 1, 1, instr_load},
  {HLT, 0, 0, instr_hlt},
  {LBL, 1, 1, instr_lbl},
  {JMP, 1, 1, instr_jmp},
  {JE, 1, 1, instr_je},
  {JNE, 1, 1, instr_jne},
  {JG, 1, 1, instr_jg},
  {JGE, 1, 1, instr_jge},
  {JL, 1, 1, instr_jl},
  {JLE, 1, 1, instr_jle},
  {CMP, 2, 2, instr_cmp},
  {CALL, 1, 1, instr_call},
  {RET, 0, 0, instr_ret}, 
  {INC, 1, 1, instr_inc},
  {DEC, 1, 1, instr_dec}
};

const char* operation_names[] = {
  "psh", "add", "sub", "mul", "div", "pop", "set", "load", "hlt", "label", "jmp", "je", "jne", "jg", "jge", "jl", "jle", "cmp", "call", "return", "inc", "dec"
} ;


void trim(char *str) {
    if (!str) return;
    char *end;

    while(isspace((unsigned char)*str)) str++;

    if(*str == 0) return;

    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    *(end + 1) = 0;
}

int reg_from_char(char *st){
  if(strlen(st) != 1) return -1;
  char s = toupper(st[0]);
  if(s < 'A' || s > (char)('A' + NUMOFREGS -1)) return -1;
  return s - 'A';
}

bool assess_number(char* op, int*out){
  char *token;
  long val = strtol(op, &token, 10);
  if(*token != '\0'){
    return false;
  } 
  if(val > INT_MAX || val< INT_MIN){
    fprintf(stderr, "not in integer range");
    return false;
  }

  *out = (int)val; // casting allowed, VM is structurally intended to only handle numbers smaller than the integer limit
  return true;
}
void parse_operands(char*token, Operand* operand){

  if(!token) {
        operand->type = NONE;
        return;
    }

    trim(token); 
  int num;
  if(assess_number(token, &num)){
    operand->type = IMM;
    operand->value.imm = num;
    return;
  }

  if(strlen(token) == 1 && isalpha(token[0])){
    int reg = reg_from_char(token);
    if(reg != -1){
      operand->type = REG;
      operand->value.reg = reg;
      return;
    }
  }

  operand->type = LABEL;
  operand->value.label = strdup(token);
}

char ** assess_file(FILE*file, int* out_count){
    char **lines = malloc(sizeof(char*) * MAX_LINES);
    if(!lines){ perror("dynamic allocation unsuccessful"); exit(1);}

    char buffer[MAX_LINES_LENGTH];
    int count = 0;

    while(fgets(buffer, MAX_LINES_LENGTH, file) && count < MAX_LINES){
      buffer[strcspn(buffer, "\n")] = '\0';
      lines[count] = malloc(strlen(buffer) + 1); //this breaks when I dont allocate memory, prioritize studying why
      if(!lines[count]){
        perror("Memory allocation failed");
      }
      strcpy(lines[count], buffer);
      count++;
    }
    *out_count = count;
    return lines;
}

Instr assess_instruction(char* line){
  Instr instr;
  instr.operand1.type = NONE;
  instr.operand2.type = NONE;

  char *words[3]={NULL, NULL, NULL};
  int i =0;
  
  char*token = strtok(line, " ");
  while(token!=NULL && i < 3){
    words[i++] = token;
    token = strtok(NULL, " ");
  }
  int found = 0;
  for(int j=0 ; j<OPCODE; j++){
    if(strcmp(words[0], operation_names[j]) == 0){
      instr.ID = lookup[j].ID;
      instr.execute = lookup[j].execute;
      int operand_count = i-1;
      
      if(operand_count > lookup[j].max_operand)exit(1);
      if(operand_count < lookup[j].min_operand)exit(1);
      if(operand_count >= 1)parse_operands(words[1], &instr.operand1);
      if(operand_count >= 2)parse_operands(words[2], &instr.operand2);
      found = 1;
      break;
    }
  }

  if(!found){
    printf("Unknown instruction: %s\n", words[0]);
    exit(1);
  }
  return instr;
}

void define_program(){
  FILE* file = fopen("code.txt", "r");
  if(!file){
    perror("file cant open");
    exit(1);
  }
  u_program_size = 0;
  char **lines = assess_file(file, &u_program_size);
  fclose(file);
  u_program = malloc(sizeof(Instr)*(u_program_size));
  if(!u_program){
    perror("Failed to allocate memory");
    exit(1);
  }
  for(int i=0; i<u_program_size ; i++){
    u_program[i] = assess_instruction(lines[i]);
    free(lines[i]);
  }
  free(lines);
}

