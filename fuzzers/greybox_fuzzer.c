#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdint.h>
#include<unistd.h>
#include<sys/random.h>
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

char* str_find_line(Buffer *buf, int line_num){
  int index = 0;
  int current_line = 0;

  while(current_line < line_num && index < buf->length){
    if(buf->data[index] == '\n'){
      current_line++;
    }
    index++;
  }

  if(current_line != line_num){
    return NULL;
  }

  int start = index;
  while(index < buf->length && buf->data[index] != '\n'){
    index++;
  }
  int end = index;
  int length = end - start;
  char *result = malloc(length+1);
  if(!result) return NULL;
  memcpy(result, buf->data + start, length);
  result[length] = '\0';
  return result;
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









