#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include<unistd.h>
#include<sys/random.h>
#include<errno.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<stdint.h>
#include<time.h>
#include"../header.h"
#include"fuzzer_util.h"
//utility functions

static uint64_t state;
static uint64_t inc;

void init_rg(){
  getrandom(&state, sizeof(state), 0);
  getrandom(&inc, sizeof(inc), 0);
  inc |= 1;
}

void init_rg_state(uint64_t seed){
  state = seed;
  inc = seed | 1;
}

uint32_t pgc32() {
  uint64_t oldstate = state;
  state = oldstate * 6364136223846793005ULL + inc;

  uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
  uint32_t rot = oldstate >> 59u;

  return (xorshifted >> rot) | (xorshifted << ((- rot) & 31));
}

int rand_int(){
    return (int)(pgc32());
}

int rand_range(int min, int max){
  if(min > max){
    int t=min; min=max; max = t;
  }
  uint32_t r = pgc32();
  return min + (r % (int)(max - min + 1));
}

size_t rand_index(size_t size){
  if(size == 0) return 0;
  return pgc32() % size;
}

bool rand_bool(){
  return pgc32() & 1;
}

int rand_chance(int percent){
  if(percent >=100) return true;
  if(percent <= 0) return false;
  return rand_range(0, 99) <= percent;
}

double rand_dbl(){
  return (double)pgc32()/(double)UINT32_MAX;  
}
char rand_ascii(){
  return (char)rand_range(32, 126);
}

uint8_t rand_byte(){
    return (uint8_t)(pgc32() & 0xFF);
}


Buffer* buf_new(size_t initial_capacity){
  Buffer* buf = malloc(sizeof(Buffer));
  if(!buf) return NULL;

  buf->capacity = initial_capacity > 0 ? initial_capacity : 64;
  buf->data = malloc(buf->capacity);
  if(!buf->data){
    free(buf);
    return NULL;
  }
  buf->length = 0;
  buf->data[0] = '\0';
  return buf;
}

void buf_free(Buffer *buf){
  if(buf){
    free(buf->data);
    free(buf);
  }
}


bool buf_grow(Buffer *buf, size_t min_capacity){
  if(!buf) return false;
  if(buf->capacity >= min_capacity) return true;
  
  size_t new_capacity = buf->capacity;
  while(new_capacity < min_capacity){
    new_capacity *= 2;
  }

  char *new_data = realloc(buf->data, new_capacity);
  if(!new_data) return false;
  buf->data = new_data;
  buf->capacity = new_capacity;
  return true;

}

bool buf_insert(Buffer* buf, size_t pos, char* data, size_t len){
  if (!buf) return false;
  if(pos > buf->length) return false;

  if(!buf_grow(buf, buf->length + len + 1)) return false;
  memmove( buf->data + pos + len,
          buf->data + pos,
          buf->length - pos + 1);
  
  memcpy(buf->data + pos, data, len);
  buf->length += len;
  buf->data[buf->length] = '\0';
  return true;
}

bool buf_delete(Buffer *buf, size_t pos, size_t len){
  if(pos > buf->length) return false;
  if(pos + len > buf->length) {
    len = buf->length - pos;
  }

  memmove(buf->data+pos,
          buf->data + pos + len,
          buf->length - pos - len + 1);
  buf->length -= len;
  return true;

}

bool buf_append(Buffer *buf, char* data, size_t len){
  return buf_insert(buf, buf->length, data, len);
}

bool buf_append_str(Buffer* buf, char* str) {
    return buf_insert(buf, buf->length, str, strlen(str));
}

void buf_clear(Buffer* buf) {
    buf->length = 0;
    buf->data[0] = '\0';
}

bool buf_replace(Buffer* buf, size_t pos, size_t old_len, char* new_data, size_t new_len){
  if(pos > buf->length)return false;
  if(pos + old_len > buf->length){
    old_len = buf->length - pos;
  }

  size_t new_size = buf->length - old_len + new_len;
  if(!buf_grow(buf, new_size + 1)) return false;

  memmove( buf->data + pos + new_len,
          buf->data + pos + old_len,
          buf->length - pos - old_len + 1);

  memcpy(buf->data+pos, new_data, new_len);

  buf->length = new_size;
  return true;
}

bool file_write(const char* path, char* data, size_t len){
  FILE*f = fopen(path, "wb");
  if(!f) return false;
  size_t written = fwrite(data, sizeof(char), len, f);
  fclose(f);
  return written == len;
}

char* file_read(const char* path, size_t *out_len){
  FILE *f = fopen(path, "rb");
  if(!f) return NULL;
  if(fseek(f, 0, SEEK_END) != 0){
    fclose(f);
    return NULL;
  }
  long size = ftell(f);
  if(size < 0){
    fclose(f);
    return NULL;
  }
  rewind(f);

  char *buf = malloc(size + 1);
  if(!buf) {
    fclose(f);
    return NULL;
  }
    size_t read = fread(buf, 1, size, f);
    fclose(f);

    if (read != (size_t)size) {
        free(buf);
        return NULL;
    }

    buf[size] = '\0';
    if (out_len) *out_len = size;

    return buf;

}


bool dir_create(const char* path){
  mode_t mode = S_IRWXU | S_IRWXG;
  if(mkdir(path, mode) == -1){
    if(errno = EEXIST) return true;
    return false;
  }
  return true;
}


bool file_exists(const char* path){
if (access(path, F_OK) == 0) {
    return true;
} else {
    return false;
}
}

bool str_find_line(Buffer *buf, int line_num, size_t *out_line_start, size_t *out_line_length){
  if(!buf || buf->length == 0) return false;
  
  size_t index = 0;
  int current_line = 0;

  while(current_line < line_num && index < buf->length){
    if(buf->data[index] == '\n'){
      current_line++;
    }
    index++;
  }

  if(current_line != line_num){
    return false;
  }

  *out_line_start = index;
  while(index < buf->length && buf->data[index] != '\n'){
    index++;
  }
  *out_line_length = index - *out_line_start;
  return true;
}

int str_count_lines(Buffer* buf){
  size_t index = 0;
  int line_count = 0;
  while(index < buf->length){
    if(buf->data[index] == '\n') line_count++;
    index++;
  }
  if(buf->length > 0 && buf->data[buf->length - 1] != '\n') line_count++;

  return line_count;
}

char* str_find_substr(Buffer* buf, char*needle){
  if(!buf || !needle) return NULL;
  size_t needle_len = strlen(needle);
  size_t buf_len = buf->length;
  if(buf_len < needle_len) return NULL;
  for(size_t i = 0 ; i <= buf_len - needle_len; i++){
    if(memcmp(buf->data + i, needle, needle_len) == 0){
      return buf->data + i;
    }
  }
  return NULL;
}



uint64_t time_now_ms(){
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

double time_elapsed(uint64_t start_ms){
  uint64_t curtime = time_now_ms();
  uint64_t telap = curtime - start_ms;
  return (double)telap;
}



//MUTATION functions
//

bool mut_flip_bit(Buffer* buf){
  if(!buf || buf->length == 0) return false;
  size_t index = rand_index(buf->length);
  int bit_to_flip = rand_range(0, 7);

  uint8_t mask = (1U << bit_to_flip);
  buf->data ^= (char)mask;
  return true;
}

bool mut_flip_byte(Buffer* buf){
  if(!buf || buf->length == 0) return false;
  size_t index = rand_index(buf->length);
  buf->data[index] = rand_byte();
  return true;

}

bool mut_insert_byte(Buffer* buf){
  if(!buf || buf->length ==0) return false;
  size_t index = rand_range(0, buf->length);
  uint8_t byte = rand_byte();

  bool result = buf_insert(buf, index, (char*)&byte, 1);
  return result;
}

bool mut_delete_byte(Buffer* buf){
  if(!buf || buf->length == 0) return false;
  size_t index = rand_index(buf->length);
  buf_delete(buf, index, 1);
return true;
}

bool mut_duplicate_chunk(Buffer* buf){
  if(buf->length < 2 )return false;
  size_t chunk_start = rand_index(buf->length);
  size_t max_length = buf->length - chunk_start;
  if(max_length == 0) return false;
  size_t chunk_length = rand_range(1, max_length > 32 ? 32 : max_length);

  size_t insert_pos = rand_range(0, buf->length);
  char *chunk_copy = malloc(chunk_length);
  if(!chunk_copy) return false;

  memcpy(chunk_copy, buf->data + chunk_start, chunk_length);

  bool result = buf_insert(buf, insert_pos, chunk_copy, chunk_length);
  free(chunk_copy);
  return result;
}

char* opcodes[] = {
    "psh", "add", "sub", "mul", "div", "pop", 
    "set", "load", "hlt", "label", "jmp", 
    "je", "jne", "jg", "jge", "jl", "jle",
    "cmp", "call", "return", "inc", "dec",
  NULL
};
int num_opcodes = sizeof(opcodes) / sizeof(opcodes[0]) - 1;



bool get_opcode_on_line(Buffer* buf, size_t line_start, size_t* out_start, size_t* out_length){
  if(!buf || buf->length == 0) return false;
  size_t pos = line_start;
  while (pos < buf->length && (buf->data[pos] == ' ' || buf->data[pos] == '\t')){
    pos++;
  }
  if(pos >= buf->length || buf->data[pos] == '\n'){
    return false;
  }

  *out_start = pos;

  while(pos < buf->length && buf->data[pos] != ' '
                          && buf->data[pos] != '\t'
                          && buf->data[pos] != '\n'){
    pos++;
  }

  *out_length = pos - *out_start;
  return (*out_length > 0);
}

bool mut_swap_opcode(Buffer* buf){
  if(!buf || buf->length == 0) return false;
  int num_lines = str_count_lines(buf);
  if(num_lines == 0) return false;
  int line = rand_range(0, num_lines-1);

  size_t l_start; size_t l_length;
  if(!str_find_line(buf, line, &l_start, &l_length)) return false;

  size_t op_start, op_len;
  if(!get_opcode_on_line(buf, l_start, &op_start, &op_len)){
    return false;
  }
  char* new_opcode = opcodes[rand_index(num_opcodes)];

  return buf_replace(buf, op_start, op_len, new_opcode, strlen(new_opcode)); 
}


bool mut_corrupt_opcode(Buffer* buf){
  if(!buf || buf->length == 0) return false;
  int num_lines = str_count_lines(buf);
  if(num_lines == 0){
    return false;
  }
  int line = rand_range(0, num_lines-1);
  size_t line_start, line_length;
  if(!str_find_line(buf, line, &line_start, &line_length)) return false;

  size_t op_start, op_len;
  if(!get_opcode_on_line(buf, line_start, &op_start, &op_len)) return false;

  int corruption = rand_range(0, 3);

  switch (corruption){
    case 0:
      if(op_len > 1){
        size_t cut_len = rand_range(1, op_len - 1);
        siez_t cut_pos = rand_range(0, op_len - cut_len);
        return buf_delete(buf, op_start + cut_pos, cut_len);
      }
    case 1:
      {
        size_t char_pos = op_start + rand_index(op_len);
        char c = buf->data[char_pos];
        if (c >= 'a' && c <= 'z'){
          buf->data[char_pos] = c - 32;
        } else if (c >= 'A' && c <= 'Z'){
          buf->data[char_pos] = c + 32;
        } else {
          buf->data[char_pos] = rand_ascii();
        }
        return true;
      }
    case 2:
      {
        size_t char_pos = op_start + rand_index(op_len);
        char c = buf->data[char_pos];
        return buf_insert(buf, char_pos, &c, 1);
      }
    case 3:
      {
        size_t char_pos = op_start + rand_index(op_len);
        char c = buf->data[char_pos];
        int offset = rand_range(-2, 2);
        int new_val = c + offset;

        /*if(new_val < 32) new_val = 32;
        if(new_val > 126) new_val = 126;

        buf->data[char_pos] = (char)new_val;*/ 
        buf->data[char_pos] = (char)((unsigned char)buf->data[char_pos]+offset);
        return true;
      }
  }
return false;
}


bool find_next_num(Buffer* buf, size_t from, NumberCor* numz){
  if(!buf || buf->length == 0) return false;
  size_t i = from;

  while(i < buf->length){
    char c = buf-> data[i];
    if(c == '\n' || c == ';'){
      i++;
      continue;
    }

    bool is_start = false;
    bool is_negative = false;


    if(c >= '0' && c <= '9'){
      if (i == 0 || buf->data[i-1] == ' ' 
        || buf->data[i-1] == '\t' 
        || buf->data[i-1] == '\n'){
          is_start = true;
      }
    }
    else if (c == '-' && i + 1 < buf->length) {
      char next = buf->data[i + 1];
      if (next >= '0' && next <= '9') {
        if (i == 0 || 
          buf->data[i-1] == ' ' || 
          buf->data[i-1] == '\t' || 
          buf->data[i-1] == '\n') {
          is_start = true;
          is_negative = true;
          }
      }
    }

    if(is_start){
      numz->start = i;

      if(is_negative) i++;

      while(i < buf->length && buf->data[i] >= '0' && buf->data[i] <= '9'){
        i++;
      }
      numz->len = i - numz->start;
      return true;
    }
    i++;
  }
  return false;
}


char* boundary_vals[] = {
  "0", "1", "-1",
  "127", "-128", "128", "-129",
  "255", "256",
  "32767", "-32768", "32768", "-32769", "65535", "65536",
  "2147483647", "-2147483648","2147483648", "-2147483649",
  "99999999999999", "0x10", "1.5", NULL
};

int boundary_size = sizeof(boundary_vals) / sizeof(boundary_vals[0]) - 1;

bool mut_boundary_value(Buffer* buf){
  NumberCor numbers[128];
  int count = 0;
  size_t pos = 0;

  while(count < 128 && find_next_num(buf, pos, &numbers[count])){
    pos = numbers[count].start + numbers[count].len;
    count++;
  }

  if(count == 0) return false;

  int choice = rand_index(count);
  NumberCor* target = &numbers[choice];
  char* boundary_choice = boundary_vals[rand_index(boundary_size)];
  return buf_replace(buf, target->start, target->len, boundary_choice, strlen(boundary_choice));

}

bool find_next_reg(Buffer *buf, size_t from, size_t *reg_start){
  if(!buf || buf->length == 0) return false;
  size_t i = from;

  while(i < buf->length){
    if(buf->data[i] == '\n' || buf->data[i] == ';') return false;
    bool is_start = false;
    bool is_one = false;
    char c = buf->data[i];
    bool is_register_char = (c >= 'A' && c<= 'E');
    if(is_register_char){
      is_start = (i == 0 || buf->data[i-1] == ' ' || buf->data[i-1] == '\n' || buf->data[i-1] == '\t');
      is_one = (i == buf->length - 1  || buf->data[i+1] == ' ' || buf->data[i+1] == '\n' || buf->data[i+1] == '\t');
      if(is_start && is_one){
        *reg_start = i;
        return true;
      }
    }
    i++;
  }
  return false;
}

const char regnames[] = { 'A', 'B', 'C', 'D', 'E'};
size_t reg_size =sizeof(regnames) / sizeof(regnames[0]);

bool mut_invalid_register(Buffer* buf){
  size_t reg_positions[64];
  int count = 0;
  size_t pos = 0;
   while(count < 64 && find_next_reg(buf, pos, &reg_positions[count])){
    pos = reg_positions[count] + 1;
    count++;
  }

  if(count == 0) return false;

  int choice = rand_index(count);
  size_t target = reg_positions[choice];
  char invalid_regs[] = {'F', 'G', 'X', 'Z', '1', '@', ' '};
  size_t inval_reg_size = sizeof(invalid_regs)/sizeof(invalid_regs[0]);
  char regchoice;
  if(rand_chance(70)){
    regchoice = reg_names[rand_index(reg_size)];
  } else {
    regchoice = invalid_regs[rand_index(inval_reg_size)];
  }

  return buf_replace(buf, target, 1, &reg_choice, 1);
}

bool mut_swap_operands(Buffer* buf){
  if(!buf) return false;
  int line_count = str_count_lines(buf);
  if(line_count == 0) return false;

  int chosen_line = rand_range(0, line_count - 1);

  size_t c_line_start, c_line_len;
  if(!str_find_line(buf, chosen_line, &c_line_start, &c_line_len)) return false;

  size_t op_start, op_len;
  if(!get_opcode_on_line(buf, c_line_start, &op_start, &op_len)) return false;

  size_t line_end = c_line_start + c_line_len;
  size_t pos = op_start+op_len;

  while(pos < line_end && (buf->data[pos] == ' '|| buf->data[pos] == '\t')) pos++;

  size_t op1_start = pos;
  while(pos < line_end && buf->data[pos] != ' ' && buf->data[pos] != '\t'&& buf->data[pos] == '\n') pos++;
  size_t op1_len = pos - op1_start;


  while(pos < line_end && (buf->data[pos] == ' ' || buf->data[pos] == '\t')) pos++;

  size_t op2_start = pos;
  while(pos < line_end && buf->data[pos] != ' ' && buf->data[pos] != '\t'&& buf->data[pos] == '\n') pos++;
  size_t op2_len = pos - op2_start;

  if(op1_len == 0 || op2_len == 0) return false;

  size_t new_len = op1_len + op2_len + 1;
  char* new_operands = malloc(new_len + 1);
  if(!new_operands) return false;

  memcpy(new_operands, buf->data + op2_start, op2_len);
  new_operands[op2_len]= ' ';
  memcpy(new_operands + op2_len + 1, buf->data + op1_start, op1_len);
  new_operands[new_len] = '\0';
  
  size_t old_len = (op2_start + op2_len) - op1_start;

  bool result =  buf_replace(buf, op1_start, old_len, new_operands, new_len);
  free(new_operands);

  return result;
}

//instruction-level

int generate_imm_value(){
  if(rand_chance(20)){
    const char* c = boundary_vals[rand_index(boundary_size)];
    char* endptr;
    long val = strtol(c, &endptr, 0);
    if(val > INT_MAX) return INT_MAX;
    if(val < INT_MIN) return INT_MIN;
    return (int)val;
  } 

  return rand_int();
}

char*generate_valid_instruction(){
  size_t op_index = rand_index(OPCODE);
  Instr_template* tmpi = &lookup[op_index];
  const char* opcode_name = operation_names[op_index];

  char result[512];
  int pos = 0;

  pos += sprintf(result + pos, "%s", opcode_name);

  for(size_t i = 0; i<tmpi->max_operand; i++){
    //preconditions
    int valid = tmpi->validOp[i];
    if(valid == OP_NONE) break;
    if(pos >= 480) break;
    result[pos++] = ' ';
    //decide optype 
    int type;
    if((valid & OP_IMM) && (valid & OP_REG)){
      type = rand_bool()? OP_IMM : OP_REG;
    } else if (valid & OP_IMM){
      type = OP_IMM;
    } else if (valid & OP_REG){
      type = OP_REG;
    } else if (valid & OP_LABEL){
      type = OP_LABEL;
    } else {
      break;
    }
    //generate the actual value
    switch(type){
      case OP_REG:
        //my regs are defined A->E
        result[pos++] = 'A' + rand_range(0, 4);
        break;
      case OP_IMM:
        pos+=sprintf(result+pos, "%d", generate_imm_value());
        break;
      case OP_LABEL:
        pos+=sprintf(result+pos, "lbl_%d", rand_range(0, 99));
        break;
    }

  }
  result[pos++] = '\n';
  result[pos] = '\0';
  return strdup(result);

}

char* generate_chaos_instruction(){
  char result[512];
  int pos = 0;
  
  if(rand_chance(80)){
    size_t op_index = rand_index(OPCODE);
    pos += sprintf(result+pos, "%s", operation_names[op_index]);
  } else {
    const char* garbage[] = {
        "uefi", "iuzqefh", "pfeoah", "eife", "142", "AdD", "", "a"
    };
    pos+= sprintf(result+pos, "%s", garbage[rand_index(8)]);
  }
  
  int num_operands = rand_range(0, 4);
  for(size_t i = 0; i<num_operands; i++){
    if(pos >= 480) break;
    result[pos++] = ' ';
     int type = rand_range(0, 5);
        
      switch (type) {
        case 0:
          result[pos++] = 'A' + rand_range(0, 4);
          break;        
        case 1:
          { const char bad_regs[] = {'F', 'Z', 'a', '0', '@', '!'};
            result[pos++] = bad_regs[rand_index(6)];
          }
          break;
        case 2:
          pos += sprintf(result + pos, "%d", rand_range(-100, 100));
          break;
        case 3:
          pos += sprintf(result + pos, "%s", boundary_vals[rand_index(boundary_size)]);
          break;
        case 4:
          pos += sprintf(result + pos, "label_%d", rand_range(0, 99));
          break;
        case 5:
          { const char* garbage[] = {"@#$", "", "   ", "1.5", "0x10"};
            pos += sprintf(result + pos, "%s", garbage[rand_index(5)]);
          }
          break;
        }
  }
  result[pos++] = '\n';
  result[pos] = '\0';
  return strdup(result);
}

char* generate_instr(){
  if(rand_chance(70)){
    return generate_valid_instruction();
  } else {
    return generate_chaos_instruction();
  }
}

bool mut_insert_instruction(Buffer* buf){
  if(!buf) return false;

  char *instr = generate_instr();
  if(!instr) return false;

  int line_count = str_count_lines(buf);
  int target_line = rand_range(0, line_count-1);

  size_t insert_pos;
  if(target_line == 0){
    insert_pos = 0;
  } else {
    size_t line_start, line_len;
    if(!str_find_line(buf, target_line, &line_start, &line_len)){
      free(instr);
      return false;
    }
    insert_pos = line_start + line_len;
    if(insert_pos < buf->length && buf->data[insert_pos] == '\n'){
      insert_pos++;
    }
  }
  bool result = buf_insert(buf, insert_pos, instr, strlen(instr));
  free(instr);
  return result;
}

bool mut_delete_instruction(Buffer* buf){
  if(!buf) return false;

  int line_count = str_count_lines(buf);
  if(line_count == 0) return false;
  int target_del = rand_range(0, line_count-1);
  size_t line_start, line_len;
  if(!str_find_line(buf, target_del, &line_start, &line_len)) return false;
  
  size_t delete_len = line_len;
  if(line_start + line_len < buf->length && buf->data[line_start + line_len] == '\n'){
    delete_len++;
  }
return buf_delete(buf, line_start, delete_len);
}

bool mut_duplicate_instruction(Buffer* buf){
  if(!buf) return false;
  int line_count = str_count_lines(buf);
  if(line_count == 0) return false;
  int target_dup = rand_range(0, line_count-1);
  size_t line_start, line_len;
  if(!str_find_line(buf, target_dup, &line_start, &line_len)) return false;
  
  size_t dup_len = line_len;
  if(line_start + line_len < buf->length && buf->data[line_start + line_len] == '\n'){
    dup_len++;
  }
  char*line_copy = malloc(dup_len);
  if(!line_copy) return false;
  memcpy(line_copy, buf->data + line_start, dup_len);

  int dest = rand_range(0, line_count-1);
  size_t insert_pos;

  if(dest == 0){
    insert_pos =0;
  } else {
    size_t dest_start, dest_len;
    if(!str_find_line(buf, dest - 1, &dest_start, &dest_len)){
      free(line_copy);
      return false;
    }
    insert_pos = dest_start + dest_len;
    if(insert_pos < buf->length && buf->data[insert_pos] == '\n'){
      insert_pos++;
    }
  }
  
  bool result = buf_insert(buf, insert_pos, line_copy, dup_len);
  free(line_copy);
  return result;
}

bool mut_shuffle_instructions(Buffer* buf){
  if(!buf) return false;
  int line_count = str_count_lines(buf);
  if(line_count < 2) return false;

  char**lines = malloc(sizeof(char*) * line_count);
  if(!lines) return false;

  for(int i=0; i<line_count; i++){
    size_t start, len;
    if(!str_find_line(buf, i, &start, &len)){
      for(int j=0; j < i; j++) free(lines[j]);
      free(lines);
      return false;
    }
    size_t copy_len = len;
    if(start+len < buf->length && buf->data[start+len] == '\n'){
      copy_len++;
    }

    lines[i] = malloc(copy_len + 1);
    if(!lines[i]){
      for(int j=0; j<i; j++) free(lines[j]);
      free(lines);
      return false;
    }
    memcpy(lines[i], buf->data + start, copy_len);
    lines[i][copy_len] = '\0';
  }

  //shufflinf using fihseryates

  for(int i=line_count - 1; i > 0; i--){
    int j = rand_range(0, i);
    char *temp = lines[i];
    lines[i] = lines[j];
    lines[j] = temp;
  }

  buf_clear(buf);
  for(int i=0; i < line_count; i++){
    buf_append(buf, lines[i], strlen(lines[i]));
    free(lines[i]);
  }
  free(lines);

  return true;
}

//runtime-level

bool mut_stack_overflow(Buffer* buf){
  if(!buf) return false;
  int line_count = str_count_lines(buf);
  if(line_count == 0) return false;

  int target_line =  rand_range(0, line_count-1);
  size_t start_pos, start_len;
  if(!str_find_line(buf, target_line, &start_pos, &start_len)) return false;

  size_t pos = 0;
  for(int i=0; i < STACKSIZE + 10; i++){
    char instr[32];
    int len = sprintf(instr, "psh %d\n", i%100);

    buf_insert(buf, start_pos + pos, instr, len);
    pos += len;
  }
  return true;
}

bool mut_stack_underflow(Buffer* buf){
  if(!buf) return false;
  int line_count = str_count_lines(buf);
  if(line_count == 0) return false;
  
  char firecrack[128];
  int written = snprintf(firecrack, sizeof(firecrack), "pop\npop\npop\npop\npop\npop\npop\n");
  return buf_insert(buf, 0, firecrack, written);
}

bool mut_callstack_overflow(Buffer* buf){
  if(!buf || buf->length == 0) return false;

  char label_name[64];
  sprintf(label_name, "recur_%d", rand_int() % 1000);

  char rec_func[128];
  int written = snprintf(rec_func, sizeof(rec_func), "label %s\ncall %s\n", label_name, label_name);
  if(written < 0 || written >= (int)sizeof(rec_func)){
    return false;
  }

  if(!buf_insert(buf, 0, label_def, strlen(label_def))) return false;

  char call_instr[128];
  written = snprintf(call_instr, sizeof(call_instr), "call %s\n", label_name);
  if(written < 0 || written >= (int)sizeof(call_instr)){
    return false;
  }
  
  size_t call_insert_pos = strlen(rec_func);
 
  return  (!buf_insert(buf, call_insert_pos, call_instr, strlen(call_instr)));

}

bool mut_divide_by_zero(Buffer* buf){
  if(!buf || buf->length == 0) return false;

  int line_count = str_count_lines(buf);
  if(line_count == 0) return false;

  size_t insert_pos = buf->length;

  for(int i=0; i<line_count; i++){
    size_t line_start, line_len;
    if(!str_find_line(buf, i, &line_start, &line_len)) continue;

    size_t op_start, op_len;
    if(!get_opcode_on_line(buf, line_start, &op_start, &op_len)) continue;

    if(op_len == 3 && strncmp(buf->data + op_start, "hlt", 3) == 0){
      insert_pos = line_start;
      break;
    }
  }

  char div_sequence[128];
  int dividend = rand_range(1, 1000);
  int written = sprintf(div_sequence, "psh %d\npsh 0\ndiv\n", dividend);
  if(written < 0) return false;
  return buf_insert(buf, insert_pos, div_sequence, written);
}

bool mut_integer_overflow(Buffer* buf){
  char overflow[] = 
    "psh 2147483647\n"  // INT_MAX
    "psh 1\n"
    "add\n";
  return buf_insert(buf, 0, overflow, strlen(overflow));
}

bool mut_uninitialized_register(Buffer* buf){
  // Use register without setting it first
  char uninit[] = "add A B\n";  // A and B not initialized
  return buf_insert(buf, 0, uninit, strlen(uninit));
}

bool mut_invalid_jump(Buffer* buf){
  char jump[] = "jmp nonexistent_label\n";
  return buf_insert(buf, 0, jump, strlen(jump));
}

bool mut_lonely_return(Buffer* buf){
  char ret[] = "return\n";
  return buf_insert(buf, 0, ret, strlen(ret));
}


bool mut_break_label(Buffer* buf){
  if(!buf || buf->length == 0) return false;

  int line_count = str_count_lines(buf);
  if(line_count == 0) return false;

  size_t insert_pos = buf->length;

  for(int i=0; i < line_count; i++){
    size_t line_start, line_len;
    if(!str_find_line(buf, i, &line_start, &line_len)) continue;

    size_t op_start, op_len;
    if(!get_opcode_on_line(buf, line_start, &op_start, &op_len)) continue;

    if(op_len == 3 && strncmp(buf->data + op_start, "hlt", 3) == 0){
      insert_pos = line_start;
      break;
    }
  }

  char bad_label[64];
  snprintf(bad_label, sizeof(bad_label), "fake_lbl_%d_%d", rand_int()%1000, rand_int()%1000);

  char jmp_instruction[128];
  int written = snprintf(jmp_instruction, sizeof(jmp_instruction), "jmp %s\n", bad_label);

  if(written < 0 || written >= (int)sizeof(jmp_instruction)){
    return false;
  }
  bool result = buf_insert(buf, insert_pos, jmp_instruction, written);
  return result;

}
bool mut_duplicate_label(Buffer* buf){
  if(!buf || buf->length == 0) return false;

  int line_count = str_count_lines(buf);
  if(line_count < 2) return false;

  char found_label[64] = {0};

  for(int i=0; i<line_count; i++){
    size_t line_start, line_len;
    if(!str_find_line(buf, i, &line_start, &line_len)) continue;

    size_t op_start, op_len;
    if(!get_opcode_on_line(buf, line_start, &op_start, &op_len)) continue;

    if(op_len == 5 && strncmp(buf->data + op_start, "label", 5) == 0){
      size_t op_end = op_start + op_len;

      while(op_end < line_start + line_len && (buf->data[op_end] == ' ' || buf->data[op_end] == '\t')){
        op_end++;
      }
      
      size_t label_start = op_end;
      while(op_end < line_start + line_count && buf->data[op_end] != ' ' && buf->data[op_end] != '\t' && buf->data[op_end] != '\n'){
        op_end++;
      }
      size_t label_len = op_end - label_start;

      if(label_len > 0 && label_len < sizeof(found_label)){
        memcpy(found_label, buf->data + label_start, label_len);
        found_label[label_len] = '\0';
        break;
      }
    }
  }
  if(found_label[0] == '\0') return false;

  int insert_line = rand_range(0, line_count-1);
  size_t  insert_line_start, insert_line_len;

  if(!str_find_line(buf, insert_line, &insert_line_start, &insert_line_len)) return false;

  size_t insert_pos = insert_line_start;

  char dup_label[128];
  int written = snprintf(dup_label, sizeof(dup_label), "label %s\n", found_label);

  if(written < 0 || written > (int)sizeof(dup_label)) return false;

  return buf_insert(buf, insert_pos, dup_label, written);
}


bool mut_infinite_loop(Buffer* buf){
  if(!buf || buf->length == 0) return false;

  char loop_label[64];
  snprintf(loop_label, sizeof(loop_label), "inf_loop_%d", rand_int()%100000);

  char label_def[64];
  int written = snprintf(label_def, sizeof(label_def), "label %s\n", loop_label);
  if(written < 0 || written > (int)sizeof(label_def)) return false;

  char jmp_instr[64];
  written = snprintf(jmp_instr, sizeof(jmp_instr), "jmp %s\n", loop_label);
  if(written < 0 || written >= (int)sizeof(jmp_instr)) return false;

  if(!buf_insert(buf, 0, label_def, strlen(label_def))) return false;
  if(!buf_insert(buf, strlen(label_def), jmp_instr, strlen(jmp_instr))) return false;

  return true;
}


bool mut_missing_halt(Buffer* buf){
  if(!buf || buf->length == 0) return false;
  int line_count = str_count_lines(buf);
  if(line_count == 0) return false;
  bool found_halt = false;

  for(int i=line_count - 1; i>=0; i--){
    size_t line_start, line_len;
    if(!str_find_line(buf, i, &line_start, &line_len)) continue;

    size_t op_start, op_len;
    if(!get_opcode_on_line(buf, line_start, &op_start, &op_len)) continue;

    if(op_len == 3 && strncmp(buf->data + op_start, "hlt", 3) == 0){
      size_t delete_len = line_len;

      if(line_start + line_len < buf->length && buf->data[line_start + line_len] == '\n'){
        delete_len++;
      }

      if(!buf_delete(buf, line_start, delete_len)){
        return false;
      }

      found_halt = true;
    }
  }
  return found_halt;
}

//formatting-level

bool mut_excess_whitespace(Buffer* buf){
  if(!buf || buf->length == 0) return false;

  int line_count = str_count_lines(buf);
  if(line_count == 0) return false;

  int target_line = rand_range(0, line_count-1);
  size_t line_start, line_len;

  if(!str_find_line(buf, target_line, &line_start, &line_len)) return false;
  
  size_t space_pos;
  bool found_space = false;

  for(int i=line_start; i<line_start+line_len; i++){
    if(buf->data[i] == ' '){
      space_pos = i;
      found_space = true;
      break;
    }
  }
  if(!found_space) return false;

  int num_extra = rand_range(5, 30);
  char* white_set = malloc(num_extra);
  if(!white_set) return false;

  for(int i=0; i<num_extra; i++){
    white_set[i] = rand_chance(30)? '\t' : ' ';
  }

  bool result =  buf_insert(buf, space_pos, white_set, num_extra);
  free(white_set);
  return result;
}

bool mut_long_line(Buffer* buf){
   if(!buf || buf->length == 0) return false;

  int line_count = str_count_lines(buf);
  if(line_count == 0) return false;

  int target_line = rand_range(0, line_count-1);
  size_t line_start, line_len;

  if(!str_find_line(buf, target_line, &line_start, &line_len)) return false;
  
  size_t insert_pos = line_start + line_len;
  
  char long_comment[301];
  long_comment[0] = ';';
  long_comment[1] = ' ';
  for(int i=2; i< 300; i++){
    long_comment[i] = 'X';
  }
  long_comment[300] = '\0';

  return buf_insert(buf, insert_pos, long_comment, 300);
}

bool mut_empty_lines(Buffer* buf){
  if(!buf || buf->length == 0) return false;

  int line_count = str_count_lines(buf);
  if(line_count == 0) return false;

  int target_line = rand_range(0, line_count-1);
  size_t line_start, line_len;

  if(!str_find_line(buf, target_line, &line_start, &line_len)) return false;
  
  size_t insert_pos = line_start + line_len;
  
  if(insert_pos < buf->length && buf->data[insert_pos] == '\n'){
    insert_pos++;
  }
  int num_empty_lines = rand_range(3, 8);
  for(int i=0; i<num_empty_lines; i++){
    char empty[] = "\n";
    if(!buf_insert(buf, insert_pos, empty, 1)) return false;
    insert_pos++;
  }
  return true;
}

bool mut_inject_comment(Buffer* buf){
  if(!buf || buf->length == 0) return false;
  int line_count = str_count_lines(buf);
  if(line_count == 0) return false;

  int target_line = -1;

  for(int attempt=0; attempt<5; attempt++){
    int candidate = rand_range(0, line_count-1);
      size_t s, len;

      if(!str_find_line(buf, candidate, &s, &len)) continue;

      for(size_t i=s ; i < s+len  ; i++){
        if(buf->data[i] == ' '){
          target_line = candidate;
          break;
        }
      }
      if(target_line != -1) break;
    }

  if(target_line == -1){
    for(int i=0; i< line_count; i++){
      size_t s, len;
      if(!str_find_line(buf, i, &s, &len)) continue;

      for(size_t j=s ; j < s+len  ; j++){
        if(buf->data[j] == ' '){
          target_line = i;
          break;
        }
      }
      if(target_line != -1) break;
    }
  }

  if(target_line == - 1) return false;
  
  size_t line_start, line_len;
  if(!str_find_line(buf, target_line, &line_start, &line_len)) return false;

  size_t insert_pos = 0;
  for(size_t i=line_start; i<line_start+line_len; i++){
    if(buf->data[i] == ' '){
      insert_pos = i;
      break;
    }
  }
  
  char* comment = "; This is a comment\n";
  size_t comment_len = strlen(comment);

  return buf_insert(buf, insert_pos, comment, comment_len);
}














