#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<ctype.h>
#include<limits.h>
#include"header.h"
#include"error.h"
#include"fuzzers/rl_bridge/state.h"
#define MAX_LINES 100 
#define MAX_LINES_LENGTH 255

Instr_template lookup[OPCODE]= {
  {PSH, 1, 1, instr_psh, { OP_IMM,  OP_NONE} },
  {ADD, 0, 0, instr_add, { OP_NONE,  OP_NONE} },
  {SUB, 0, 0, instr_sub, { OP_NONE,  OP_NONE} },
  {MUL, 0, 0, instr_mul, { OP_NONE,  OP_NONE} },
  {DIV, 0, 0, instr_div, { OP_NONE,  OP_NONE} },
  {POP, 0, 0, instr_pop, { OP_NONE,  OP_NONE} },
  {SET, 2, 2, instr_set, { OP_REG,  OP_IMM} },
  {LOAD, 1, 1, instr_load, { OP_REG,  OP_NONE} },
  {HLT, 0, 0, instr_hlt, { OP_NONE,  OP_NONE} },
  {LBL, 1, 1, instr_lbl, { OP_LABEL,  OP_NONE} },
  {JMP, 1, 1, instr_jmp, { OP_LABEL,  OP_NONE} },
  {JE, 1, 1, instr_je, { OP_LABEL,  OP_NONE} },
  {JNE, 1, 1, instr_jne, { OP_LABEL,  OP_NONE} },
  {JG, 1, 1, instr_jg, { OP_LABEL,  OP_NONE} },
  {JGE, 1, 1, instr_jge, { OP_LABEL,  OP_NONE} },
  {JL, 1, 1, instr_jl, { OP_LABEL,  OP_NONE} },
  {JLE, 1, 1, instr_jle, { OP_LABEL,  OP_NONE} },
  {CMP, 2, 2, instr_cmp, { OP_REG |  OP_IMM,  OP_REG |  OP_IMM} },
  {CALL, 1, 1, instr_call, { OP_LABEL,  OP_NONE} },
  {RET, 0, 0, instr_ret, { OP_NONE,  OP_NONE} }, 
  {INC, 1, 1, instr_inc, { OP_REG,  OP_NONE} },
  {DEC, 1, 1, instr_dec, { OP_REG,  OP_NONE} }
};

const char* operation_names[] = {
  "psh", "add", "sub", "mul", "div", "pop", "set", "load", "hlt", "label", "jmp", "je", "jne", "jg", "jge", "jl", "jle", "cmp", "call", "ret", "inc", "dec"
} ;
//general purpose aid function

void trim(char *str){
  if(!str) return;
  char*end;

  while(isspace((unsigned char)* str)) str++;

  if(*str == 0) return;

  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)* end)) end--;
  *(end+1) = 0;
}

int reg_from_char(char*st){
  if(!st || strlen(st)!= 1) return -1;
  char s = toupper((unsigned char)st[0]);
  if(s < 'A' || s > (char)('A' + NUMOFREGS - 1)) return -1;
  return s -'A';
}

bool assess_number(char* op, int *out){
  if(!op || !out) return false;
  char *token;
  long val = strtol(op, &token, 10);
  if(*token != '\0'){
    return false;
  }
  if(val > INT_MAX || val < INT_MIN){
    report_asm_error(ERR_INVALID_LITERAL, 72, op, "Not a valid integer");
  }

  *out = (int)val;
  return true;
}
 

int parse_operand(char*token){
  if(!token){
      return OP_NONE;
  }
  int num;

  if(assess_number(token, &num)){
      return OP_IMM;
  }

  if(isalpha((unsigned char)token[0])){
    if(strlen(token) == 1 && strchr("ABCDE", token[0])){
    int reg = reg_from_char(token);
      if(reg >= 0){
        return OP_REG;
      }
    } else if (strlen(token) > 1) {
        return OP_LABEL;
    }
  }
  report_asm_error(ERR_INVALID_TOKEN, 101, token, "Input Operand invalid");
}


//assembler step aid function: these are their own steps in their architecture, but for readability are implemented inside other functions 
//

char* lex_clean_line(const char* line) {
  if(!line) report_asm_error(ERR_ALLOC_FAIL, 110, line, "Issue with memory allocation");
  size_t line_len = strlen(line);
  for (size_t i = 0; i < line_len; i++) {
    if (line[i] == '\0' && i < line_len - 1) {
      report_asm_error(ERR_INVALID_TOKEN, 0, NULL, "Null byte in input");
    }
  }
  const char* comment_pos = strchr(line, ';');
  size_t useful_len = comment_pos ? (size_t)(comment_pos - line) : strlen(line);
    
  char* result = malloc(useful_len + 1);
  if(!result) report_asm_error(ERR_ALLOC_FAIL, 110, line, "Issue with memory allocation");
    
  int write_pos = 0;
  bool prev_space = false;
    
  for(size_t i = 0; i < useful_len; i++) {
    char c = line[i];
        
      if(c == '\n' || c == '\r' || isspace((unsigned char)c)) {
        if(!prev_space && write_pos > 0) {
          result[write_pos++] = ' ';
          prev_space = true;
        }
      } else {
        result[write_pos++] = c;
        prev_space = false;
      }
    }
    
  if(write_pos > 0 && result[write_pos-1] == ' ') write_pos--;
  result[write_pos] = '\0';

if (write_pos == 0) {
        free(result);
        return NULL;
    }
    
  return result;
}



bool isValidInstruction(char**words, int *index){
  if(!words || !words[0]) report_asm_error(ERR_SYNTAX, 148, *words, "token is empty");

  int word_count = 0;
  int found_opcode = -1;

  while(words[word_count] != NULL){
    word_count++;
  }


  for(int i=0; i<OPCODE; i++){
    if(strcmp(words[0], operation_names[i]) == 0){
      found_opcode = i;
      break;
    }
  }

  if(word_count == 0) return false;
  if(found_opcode == -1){
    report_asm_error(ERR_UNKNOWN_OPCODE, 190, words[0], "unknwon instruction");
  }
   //part where we validate operands 
  Instr_template operation_beta = lookup[found_opcode];

  int n_operands = word_count - 1;
   if(n_operands < operation_beta.min_operand){
    report_asm_error(ERR_TOO_FEW_OPERANDS, 167, words[0], "Operand count is too low for instruction used");
  } else if ( n_operands > operation_beta.max_operand){    
    report_asm_error(ERR_TOO_MANY_OPERANDS, 170, words[0], "Operand count too high for instruction used");
  }

  for(int i=0; i<n_operands; i++){
    int operand_type = parse_operand(words[i+1]);  // words[0] is opcode, words[1+] are operands
      
    if(operand_type == -1) {
      return false;
    }

    if( ( operation_beta.validOp[i] & operand_type ) == 0){
      return false;
    }
  }

  *index = found_opcode;
  return true;
}




char** split_lines(FILE* file, int *out){
  if(!file || !out){
    report_asm_error(ERR_IO, 199, NULL, "File entering hasn't been passed properly");
  }
  char **lines = malloc(sizeof(char*) * MAX_LINES);
  if(!lines){
    report_asm_error(ERR_ALLOC_FAIL, 204, NULL, "Memory allocation for line failed");
  }

  char buffer[MAX_LINES_LENGTH];
    int count = 0;

  while(fgets(buffer, sizeof(buffer), file) && count<MAX_LINES){

  size_t len = strlen(buffer);
    if(len == sizeof(buffer) - 1 && buffer[len - 1] != '\n'){
      report_asm_error(ERR_LINE_TOO_LONG, 209, buffer, "Line is longer than limit size");
    }
    char* cleaned_lines = lex_clean_line(buffer);
    if(!cleaned_lines){
      continue;
    }
    lines[count++] = cleaned_lines;
  }

  *out = count;
  return lines;
}

#define MAX_TOKEN_LENGTH 64

char** tokenizer(char*beta_token){
  
  if(!beta_token) report_asm_error(ERR_IO, 199, NULL, "File entering hasn't been passed properly");
;
  const int max_token = 4;
  char **words = malloc(sizeof(char*) * (max_token + 1));
  if(!words){
    report_asm_error(ERR_ALLOC_FAIL, 204, NULL, "Memory allocation for line failed");
   
  }

  for(int i = 0; i < max_token + 1; i++) words[i] = NULL;

  int count = 0;
  char*token = strtok(beta_token, " ");

  while(token != NULL && count< max_token){
    if(strlen(token) >= MAX_TOKEN_LENGTH) report_asm_error(ERR_TOKEN_TOO_LONG, 243, NULL, "Token exceeds maximum length");
    words[count] = strdup(token);
    if(!words[count]){
      for(int i = 0; i < count; i++) free(words[i]);
        free(words);
        report_asm_error(ERR_ALLOC_FAIL, 247, NULL, "strdup failed in tokenizer");
    }
    token = strtok(NULL, " ");
    count++;
  }

  if(count == 0) {
    free(words);
    return NULL;
  }

  if(count > 3){
    for(int i = 0; i < count; i++) free(words[i]);
    free(words);
    report_asm_error(ERR_TOO_MANY_OPERANDS, 259, beta_token, "Too many words in instruction");
    }
  return words;
}



Instr Encoder(char**validated_words){

  int index = -1;

  if(!isValidInstruction(validated_words, &index)) report_asm_error(ERR_INVALID_TOKEN, 273, *validated_words, "Input Instruction is not valid");
 
  if(index == -1) report_asm_error(ERR_INVALID_TOKEN, 272, validated_words[0], "Instruction not valid");

  Instr alpha_instr;
  alpha_instr.ID = lookup[index].ID;
  alpha_instr.execute = lookup[index].execute;

  alpha_instr.operand1.type = NONE;
  alpha_instr.operand2.type = NONE;

  int operand_count = 0;
  while(validated_words[operand_count] != NULL){
    operand_count++;
  }
  operand_count--;

   if(operand_count >= 1) {
      char* op1 = validated_words[1];
      int num_val;
        
      if(assess_number(op1, &num_val)) {
        alpha_instr.operand1.type = IMM;
        alpha_instr.operand1.value.imm = num_val;
      } else if(reg_from_char(op1) >= 0) {
        alpha_instr.operand1.type = REG;
        alpha_instr.operand1.value.reg = reg_from_char(op1);
      } else if(strlen(op1) > 1) {
        alpha_instr.operand1.type = LABEL;
        alpha_instr.operand1.value.label = strdup(op1);
      }
    }

  if(operand_count >= 2) {
    char* op2 = validated_words[2];
    int num_val;
        
    if(assess_number(op2, &num_val)) {
      alpha_instr.operand2.type = IMM;
      alpha_instr.operand2.value.imm = num_val;
    } else if(reg_from_char(op2) >= 0) {
      alpha_instr.operand2.type = REG;
      alpha_instr.operand2.value.reg = reg_from_char(op2);
    } else if(strlen(op2) > 1) {
      alpha_instr.operand2.type = LABEL;
      alpha_instr.operand2.value.label = strdup(op2);  // Need to free this later!
    }
  }
    
  return alpha_instr;
    
}



void free_tokens(char** tokens) {
    if(!tokens) return;
    
    for(int i = 0; tokens[i] != NULL; i++) {
        free(tokens[i]);
    }
    free(tokens);
}



void free_program(Instr* program, int program_size) {
    if (!program) return;
    for (int i = 0; i < program_size; ++i) {
        if (program[i].operand1.type == LABEL && program[i].operand1.value.label) {
            free((void*)program[i].operand1.value.label); // cast to void* to discard const-ness
        }
        if (program[i].operand2.type == LABEL && program[i].operand2.value.label) {
            free((void*)program[i].operand2.value.label);
        }
    }
    free(program); // finally free the whole array
}

Label* parse_labels(Instr* program, int program_size, int*out_lb_count, int max_labels){
  *out_lb_count = 0;
  if(program_size < 1 || max_labels < 1) report_asm_error(ERR_IO, 350, NULL, "Program or Max labels improperly allocated");
  Label* temp_lb_array = malloc(sizeof(Label) * max_labels);
  if(!temp_lb_array) report_asm_error(ERR_ALLOC_FAIL, 352, NULL, "Label array allocation failed");
  int lb_index = 0;
  for(int i=0; i<program_size; i++){
    if (program[i].ID == LBL){
      for(int j=0; j<lb_index; j++){
        if(strcmp(temp_lb_array[j].name, program[i].operand1.value.label) == 0){
          report_asm_error(ERR_DUPLICATE_LABEL, i, temp_lb_array[j].name, "Label has a duplicate declared in code");
        }
      }
      if(lb_index == max_labels){
        free(temp_lb_array);
        report_asm_error(ERR_TOO_MANY_LABELS, 364, NULL, "Too many labels defined in code");
      }
      const char* label_name = program[i].operand1.value.label;
      size_t label_len = strlen(label_name);

      if(label_len >= sizeof(temp_lb_array[lb_index].name)){
        report_asm_error(ERR_LABEL_TOO_LONG, 373, label_name, "Label is too long");
      }
      strncpy(temp_lb_array[lb_index].name, label_name, sizeof(temp_lb_array[lb_index].name) - 1);
      temp_lb_array[lb_index].name[sizeof(temp_lb_array[lb_index].name) - 1] = '\0';
      temp_lb_array[lb_index].address = i;
      lb_index++;
    } 
  }
  
  bool has_halt = false;
  for(int i=0; i<program_size; i++){
    if(program[i].ID == HLT){
      has_halt = true;
      break;
    }
  }
  if(!has_halt){
    report_asm_error(ERR_MISSING_HALT, 391, "halt", "Program missing a halt.");
  }
  *out_lb_count = lb_index;
  return temp_lb_array;
}



void define_program(Instr **out_program, int *out_size, Label **out_labels, int *out_label_count) {
    int a_program_size = 0;
    Instr *a_program = NULL;

    // Phase 1: Lexical Analysis
    FILE* code = fopen("fuzz_input.txt", "r");
    if(!code) {
        report_asm_error(ERR_IO, 335, NULL, "Couldn't open code file");
    }
    
    int linecount = 0;
    char **lines = split_lines(code, &linecount);
    fclose(code);
    
    if(!lines) {
      report_asm_error(ERR_IO, 343, NULL, "Couldn't read the file");
    }
    
    if(linecount == 0) {
      free(lines);
      report_asm_error(ERR_IO, 343, NULL, "File is empty");
       
    }
    
    // Phase 2: Tokenization
    char ***tokens = malloc(sizeof(char**) * linecount);
    if(!tokens) {
        for(int i = 0; i < linecount; i++) free(lines[i]);
        free(lines);
        report_asm_error(ERR_ALLOC_FAIL, 357, NULL, "Couldn't allocate space for tokens");
    }
    
    for(int i = 0; i < linecount; i++) {
        tokens[i] = tokenizer(lines[i]);
        
        // Critical check: tokenizer returns NULL for invalid lines
        if(!tokens[i] || !tokens[i][0]) {
            // Cleanup
            for(int j = 0; j < i; j++) {
                if(tokens[j]) free_tokens(tokens[j]);
            }
            free(tokens);
            for(int j = 0; j < linecount; j++) free(lines[j]);
            free(lines);
            report_asm_error(ERR_INVALID_TOKEN, 375, NULL, "Tokenization failed for line");
        }
    }
    
    // Phase 3: Code Generation (Encoding)
    a_program_size = linecount;
    a_program = malloc(sizeof(Instr) * a_program_size);
    if(!a_program) {
        // Cleanup
        for(int i = 0; i < linecount; i++) {
            free(lines[i]);
            if(tokens[i]) free_tokens(tokens[i]);
        }
        free(lines);
        free(tokens);
        report_asm_error(ERR_ALLOC_FAIL, 387, NULL, "Memory alocation for program failed");
    }
    
    for(int i = 0; i < linecount; i++) {
        a_program[i] = Encoder(tokens[i]);
           }
      state_update_histogram(current_state, a_program_size, a_program);
      state_update_num_features(current_state, a_program_size, a_program); //separate both functions depending on what they fill up #separation_of_church_and_state
    int label_count = 0;
    Label *lb_array = parse_labels(a_program, a_program_size, &label_count, MAXLABELS);
    if(!lb_array){
      free_program(a_program, a_program_size);
      for(int i=0; i < linecount; i++){
        free(lines[i]);
        if(tokens[i]) free(tokens[i]);
      }
      free(lines);
      free(tokens);
      report_asm_error(ERR_ALLOC_FAIL, 446, "LABELS", "Label array allocation and/or creation failed");

  }
    
    *out_program = a_program;
    *out_size = a_program_size;
    *out_label_count = label_count;
    *out_labels = lb_array;
    // Phase 4: Cleanup intermediate structures
    for(int i = 0; i < linecount; i++) {
        free(lines[i]);
        if(tokens[i]) free_tokens(tokens[i]);
    }
    free(lines);
    free(tokens);
}




