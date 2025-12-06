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

//utility functions

static uint64_t state;
static uint64_t inc;

void init_rg(){
  getrandom(&state, sizeof(state), 0);
  getrandom(&inc, sizeof(inc), 0);
  inc |= 1;
}

uint32_t pcg32() {
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
  return min + (rand_int() % (max - min + 1));
}

size_t rand_index(size_t size){
  if(size == 0) return 0;
  return pcg32() % size;
}

double rand_dbl(){
  return (double)(pgc32());
}

bool rand_bool(){
  return pgc32() & 1,
}

int rand_chance(int percent){
  return rand_range(0, 99) <= percent;
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
  if(buf->capacity >= min_capacity) return true;

  char *new_data = realloc(buf->data, min_capacity);
  if(!new_data) return false;
  buf->data = new_data;
  buf->capacity = min_capacity;
  return true;

}
bool buf_insert(Buffer* buf, size_t pos, char* data, size_t len){
  if (!buf) return false;
  if(pos > buf->length) return false;

  if(!buf_grow(buf, buf->length + len + 1)) return 0;
  memmove( buf->data + pos + len,
          buf->data + pos,
          buf->length - pos + 1);

  memcpy(buf->data + pos, data, len);
  return true;
}

bool buf_delete(Buffer *buf, size_t pos, size_t len){
  if(pos > buf->length) return false;
  if(pos + len > buf->length) {
    len = buf->size - pos;
  }

  memmove(buf->data+pos,
          buf->data + pos + len,
          buf->length - pos - len + 1);
  buf->length -= len;
  return true;

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
  if(!f){
    return false;
  }
  size_t written = fwrite(data, sizeof(char), len, f);
  fclose(f);
  return written == len;
}
char* file_read(const char* path, size_t *out_len){
  FILE *f = fopen(path, "r");
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
  mode_t mode = S_IRWXU | S_IRWXG | S_IRWX0;
  if(mkdir(path, mode) == -1){
    return false;
  }
  return true;
}
bool file_exists(const char* path){
if (access(fname, F_OK) == 0) {
    return true;
} else {
    return false;
}
}

bool str_find_line(Buffer *buf, int line_num, size_t *out_line_start, size_t *out_line_length){
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

  out_line_start = index;
  while(index < buf->length && buf->data[index] != '\n'){
    index++;
  }
  out_line_length = index - out_line_start;
  return true;
}

int str_count_lines(Buffer* buf){
  int index = 0;
  int line_count = 0;
  while(index < buf->length){
    if(buf->data[index] == '\n') line_count++;
    index++;
  }
  if(buf->length > 0 && buf->data[buff->length - 1] != '\n') line_count++;

  return line_count;
}

char* str_find_substr(Buffer* buf, char*needle){
  if(!buf || !needle) return NULL;
  int needle_len = strlen(needle);
  int buf_len = buf->length;

  for(size_t i = 0 ; i < buff_len - needle_len; i++){
    if(memcmp(buf->data + i, needle, needle_len) == 0){
      char * result = malloc(needle_len + 1);
      if(!result) return NULL;
      memcpy(result, buf->data+i, needle_len);
      result[needle_len] = '\0';
      return result;
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
  uint64_t telap = curtime - telap;
  return (double)telap;
}



//MUTATION functions
//

bool mut_flip_bit(Buffer* buf){
  if(buf->length == 0) return false;
  size_t index = rand_index(buf->length);
  int bit_to_flip = rand_range(0, 7);

  buf->data[index] ^= (1U << bit_to_flip);
  return true;
}

bool mut_flip_byte(Buffer* buf){
  if(buf->length == 0) return false;
  size_t index = rand_index(buf->length);
  buf->data[index] = rand_byte();
  return true;

}

bool mut_insert_byte(Buffer* buf){
  if(buf->length > buf->capacity) return false;
  size_t index = rand_range(0, buf->length);
  uint8_t byte = rand_byte();

  buf_insert(buf, index, (char*)&byte, 1);
  return true;
}

bool mut_delete_byte(Buffer* buf){
  if(buf->length > buf->capacity) return false;
  size_t index = rand_index(buf->length);
  buf_delete(buf, index, 1);
return true;
}

bool mut_duplicate_chunk(Buffer* buf){
  if(buf->length < 2 )return false;
  size_t chunk_start = rand_index(buf->length);
  size_t max_length = buf->length - start_index;
  size_t chunk_length = rand_range(1, max_length > 32 ? 32 : max_length);

  size_t insert_pos = rand_range(0, buf->length);
  char*chunk_copy = malloc(chunk_length);
  if(!chunk_copy) return false;

  memcpy(chunk_copy, buf->data + chunk_start, chunk_length);

  bool result = buf_insert(buf, insert_pos, chunk_copy, chunk_length);
}

const char* opcodes[] = {
    "psh", "add", "sub", "mul", "div", "pop", 
    "set", "load", "hlt", "label", "jmp", 
    "je", "jne", "jg", "jge", "jl", "jle",
    "cmp", "call", "return", "inc", "dec"
};
int num_opcodes = sizeof(opcodes) / sizeof(opcodes[0]);



bool get_opcode_on_line(Buffer* buf, size_t line_start, size_t* out_start, size_t* out_length){
  size_t pos = line_start;
  while (pos < buf->length && (buf->data[pos] == ' ' || buf->data[pos] == '\t')){
    pos++;
  }
  if(pos >= buf->length || buf->data[pos] == '\n'){
    return false;
  }

  *out_start = pos;

  while(pos < buf->length && buf->data[pos] != ' '
                          && buf->data[poq] != '\t'
                          && buf->data[pos] != '\n'){
    pos++;
  }

  *out_length = pos - *out_start;
  return (*out_length > 0);
}

bool mut_swap_opcode(Buffer* buf){
  int num_lines = str_count_lines(buf);
  if(num_lines == 0) return false;
  int line = rand_range(0, num_lines-1);

  size_t l_start; size_t l_length;
  if(!str_find_line(buf, line, &l_start, &l_length)) return false;

  if(l_start < 0 || l_length < 0) return false;
  size_t op_start, op_len;
  if(!get_opcode_on_line(buf, l_start, &op_start, &op_len)){
    return false;
  }
  const char* new_opcode = opcodes[rand_index(num_opcodes)];

  return buf_replace(buf, op_start, op_len, new_opcode, strlen(new_opcode));

  
}
bool mut_corrupt_opcode(Buffer* buf){
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
        size_t cut = rand_range(1, op_len - 1);
        return buf_delete(buf, op_start + op_len - cut, cut);
      }
      break;
    case 1:
      {
        size_t char_pos = op_start + rand_index(rng, op_len);
        buf->data[char_pos] ^= 32;
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
        size_t char_pos = op_start + rand_index(rng, op_length);
        buf->data[char_pos] += rand_range(rng, -2, 2);
        return true;
      }
  }
return false;
}


int *find_next_num(Buffer* buf, size_t from, NumberCor* numz){
  size_t i = from;

  while(i < buf->length){
    bool is_start = false;
    bool is_negative = false;

    char c = buf-> data[i];

    if(c > '0' && c < '9'){
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


const int boundary_vals[] = {
  "0", "1", "-1",
  "127", "-128", "128", "-129",
  "255", "256",
  "32767", "-32768", "32768", "-32769", "65535", "655356",
  "2147483647", "-2147483648","2147483648", "-2147483649",
  "2147483647", "2147483648", "-2147483648", "-2147483649"
  "99999999999999", "0x10", "1.5"
}

int boundary_size = sizeof(boundary_vals) / sizeof(boundary_vals[0]);

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
  const char* boundary_choice = boundary_vals[rand_index(boundary_size)];
  return buf_replace(buf, target->start, target->len, boundary_choice, strlen(boundary_choice));

}
bool mut_invalid_register(Buffer* buf){

}
bool mut_swap_operands(Buffer* buf);

//instruction-level

bool mut_insert_instruction(Buffer* buf);
bool mut_delete_instruction(Buffer* buf);
bool mut_duplicate_instruction(Buffer* buf);
bool mut_shuffle_instructions(Buffer* buf);

//runtime-level

bool mut_stack_overflow(Buffer* buf);
bool mut_stack_underflow(Buffer* buf);
bool mut_callstack_overflow(Buffer* buf);
bool mut_divide_by_zero(Buffer* buf);
bool mut_break_label(Buffer* buf);
bool mut_duplicate_label(Buffer* buf);
bool mut_infinite_loop(Buffer* buf);
bool mut_missing_halt(Buffer* buf);

//formatting-level

bool mut_excess_whitespace(Buffer* buf);
bool mut_long_line(Buffer* buf);
bool mut_empty_lines(Buffer* buf);
bool mut_inject_comment(Buffer* buf);














