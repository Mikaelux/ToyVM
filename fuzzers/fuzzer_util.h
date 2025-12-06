#IFNDEF FUZZER_UTIL_H
#define FUZZER_UTIL_H

//UTILITY
//random

void init_rg();
uint32_t pgc32();
int rand_int();
int rand_range(int min, int max);
size_t rand_index(size_t size);
double rand_dbl();
bool rand_bool();
int rand_chance(int percent);
char rand_ascii();
uint8_t rand_byte();

//buffer

typedef struct Buffer{
    char* data;
    size_t length;
    size_t capacity;
} Buffer;

Buffer* buf_new(size_t initial_capacity);
void buf_free(Buffer *buf);

bool buf_grow(Buffer *buf, size_t min_capacity);
bool buf_insert(Buffer* buf, size_t pos, char* data, size_t len);
bool buf_delete(Buffer *buf, size_t pos, size_t len);
bool buf_replace(Buffer* buf, size_t pos, size_t old_len, char* new_data, size_t new_len);

//FILE IO 

bool file_write(const char* path, char* data, size_t len);
char* file_read(const char* path, size_t *out_len);
bool dir_create(const char* path);
bool file_exists(const char* path);

bool str_find_line(Buffer* buf, int line_num, size_t *out_line_start, size_t *out_line_length);
int str_count_lines(Buffer* buf);
char* str_find_substr(Buffer* buf, char*needle);
bool get_opcode_on_line(Buffer* buf, size_t line_start, size_t* out_start, size_t* out_length);
//time 
uint64_t time_now_ms();
double time_elapsed(uint64_t start_ms);

typedef struct NumberCor{
  size_t start;
  size_t len;
} NumberCor;
int *find_next_num(Buffer* buf, size_t from, size_t *num_start, size_t num_len);
//MUTATION
//byte-level
bool mut_flip_bit(Buffer* buf);
bool mut_flip_byte(Buffer* buf);
bool mut_insert_byte(Buffer* buf);
bool mut_delete_byte(Buffer* buf);
bool mut_duplicate_chunk(Buffer* buf);

bool mut_swap_opcode(Buffer* buf);
bool mut_corrupt_opcode(Buffer* buf);
bool mut_boundary_value(Buffer* buf);
bool mut_invalid_register(Buffer* buf);
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


extern const char* opcodes[];
extern const int boundary_vals[];



#endif
