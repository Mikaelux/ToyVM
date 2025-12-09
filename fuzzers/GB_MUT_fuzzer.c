// fuzzer_main.c
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include "../coverage.h"
#include "../header.h"
#include "../error.h"
#include "fuzzer_util.h"

// Configuration
#define MAX_ITERATIONS 100000
#define TIMEOUT_SECONDS 5
#define TEMP_INPUT_FILE "fuzz_input.txt"
#define CRASHES_DIR "crashes"
#define CORPUS_DIR "corpus"
#define STATS_FILE "fuzz_stats.txt"
#define COVERAGE_DIR "coverage"
#define VM_COVERAGE_FILE "coverage/vm_coverage.txt"
#define ASM_COVERAGE_FILE "coverage/asm_coverage.txt"

#define MAX_CORPUS 128
#define MAX_CORPUS_PATH 512

char corpus[MAX_CORPUS][MAX_CORPUS_PATH];
int corps_count = 0;

typedef struct {
    int total_runs;
    int crashes;
    int hangs;
    uint32_t vm_new_cov;
    uint32_t asm_new_cov;
    int asm_errors;
    int vm_errors;
    int successful_runs;
    int empty_programs;
    
    int syntax_errors;
    int stack_overflows;
    int stack_underflows;
    int divide_by_zero;
    int infinite_loops;
    int label_errors;
    int register_errors;
    
    
    uint64_t start_time;
    uint64_t total_exec_time_ms;
} FuzzStats;


int plant_seeds(const char* path){
  DIR* d = opendir(path);
  if(!d) return -1;
  struct dirent *entry;

  while((entry = readdir(d)) != NULL){
    if(entry->d_type == DT_REG && corps_count < MAX_CORPUS){
      snprintf(corpus[corps_count], MAX_CORPUS_PATH, "%s/%s", path, entry->d_name);
      corps_count++;
    }
  }
  closedir(d);
  return 0;
}

uint8_t vm_virgin_map[VM_COVERAGE_MAP_SIZE];
void init_vm_virgin(){ memset(vm_virgin_map, 0xFF, VM_COVERAGE_MAP_SIZE);}

uint8_t asm_virgin_map[ASM_COVERAGE_MAP_SIZE];
void init_asm_virgin(){ memset(asm_virgin_map, 0xFF, ASM_COVERAGE_MAP_SIZE);}


static inline uint32_t total_new_cov(FuzzStats* stats) {
    return stats->vm_new_cov + stats->asm_new_cov;
}

// Write buffer content to temp file
static bool write_test_case(Buffer* buf) {
    return file_write(TEMP_INPUT_FILE, buf->data, buf->length);
}

// Save interesting test case
static void save_crash(Buffer* buf, const char* reason, int signal, FuzzStats* stats) {
    char filename[512];
    snprintf(filename, sizeof(filename), 
             "%s/crash_%d_sig%d_%ld.txt", 
             CRASHES_DIR, stats->crashes, signal, time(NULL));
    
    FILE* f = fopen(filename, "w");
    if (f) {
        fprintf(f, "; Crash reason: %s (signal %d)\n", reason, signal);
        fprintf(f, "; Total runs: %d\n\n", stats->total_runs);
        fwrite(buf->data, 1, buf->length, f);
        fclose(f);
        printf("💥 Saved crash to: %s\n", filename);
    }
}

static void save_hang(Buffer* buf, FuzzStats* stats) {
    char filename[512];
    snprintf(filename, sizeof(filename), 
             "%s/hang_%d_%ld.txt", 
             CRASHES_DIR, stats->hangs, time(NULL));
    
    FILE* f = fopen(filename, "w");
    if (f) {
        fprintf(f, "; Program timeout after %d seconds\n", TIMEOUT_SECONDS);
        fprintf(f, "; Total runs: %d\n\n", stats->total_runs);
        fwrite(buf->data, 1, buf->length, f);
        fclose(f);
        printf("⏱️  Saved hang to: %s\n", filename);
    }
}

static void save_corpus(Buffer* buf, FuzzStats* stats, int run){
  char filename[512];
  snprintf(filename, sizeof(filename), 
           "%s/corpus_%d_%ld.txt", 
           CORPUS_DIR, total_new_cov(stats), time(NULL));

  FILE* f = fopen(filename, "w");
  if(f){
    fprintf(f, "; Responsible run was %d\n", run);
    fprintf(f, "; Program found %d new bits in VM, %d in ASM || Total: %d\n"
            , stats->vm_new_cov, stats->asm_new_cov, total_new_cov(stats));

    fprintf(f, "; Total runs: %d\n\n", stats->total_runs);
    fwrite(buf->data, 1, buf->length, f);
    fclose(f);
    printf("✍️ Saved new corpus to %s\n", filename);
  }
}

// Categorize error types for statistics
static void categorize_error(Errors err, FuzzStats* stats) {
    switch(err) {
        case ERR_SYNTAX:
        case ERR_INVALID_TOKEN:
        case ERR_TOO_FEW_OPERANDS:
        case ERR_TOO_MANY_OPERANDS:
        case ERR_LINE_TOO_LONG:
        case ERR_TOKEN_TOO_LONG:
            stats->syntax_errors++;
            stats->asm_errors++;
            break;
            
        case ERR_STACK_OVERFLOW:
            stats->stack_overflows++;
            stats->vm_errors++;
            break;
            
        case ERR_STACK_UNDERFLOW:
            stats->stack_underflows++;
            stats->vm_errors++;
            break;
            
        case ERR_DIVIDE_BY_ZERO:
            stats->divide_by_zero++;
            stats->vm_errors++;
            break;
            
        case ERR_DUPLICATE_LABEL:
        case ERR_UNRESOLVED_LABEL:
        case ERR_TOO_MANY_LABELS:
        case ERR_LABEL_TOO_LONG:
            stats->label_errors++;
            stats->asm_errors++;
            break;
            
        case ERR_INVALID_REGISTER:
        case ERR_REGISTER_OUT_OF_BOUNDS:
            stats->register_errors++;
            stats->vm_errors++;
            break;
            
        case ERR_MAX_INSTRUCTIONS:
            stats->infinite_loops++;
            stats->vm_errors++;
            break;
            
        case ERR_EMPTY_PROGRAM:
            stats->empty_programs++;
            break;
            
        default:
            if (err >= ERR_SYNTAX && err <= ERR_TOO_MANY_LINES) {
                stats->asm_errors++;
            } else if (err >= ERR_CALLSTACK_OVERFLOW) {
                stats->vm_errors++;
            }
            break;
    }
}

// Run single test in isolated child process
static Errors run_single_test(Buffer* test_input, FuzzStats* stats) {
    stats->total_runs++;
    
    if (!write_test_case(test_input)) {
        fprintf(stderr, "Failed to write test case\n");
        return ERR_IO;
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        return ERR_UNKNOWN;
    }
    
    if (pid == 0) {
        // Child process
        // Redirect stderr to file for error capture
        FILE* err_log = fopen("fuzz_stderr.log", "w");
        if (err_log) {
            dup2(fileno(err_log), STDERR_FILENO);
            fclose(err_log);
        }
        
        // Assemble the program
        Instr* program = NULL;
        int program_size = 0;
        Label* labels = NULL;
        int label_count = 0;
        
        // Your assembler populates these via define_program
        // We need to wrap it to catch errors
        define_program(&program, &program_size, &labels, &label_count);
        
        if (!program || program_size == 0) {
            exit(ERR_EMPTY_PROGRAM);
        }
        
        // Initialize VM
        VM vm = {
            .call_sp = -1,
            .stepcount = 0,
            .lb = label_count,
            .sp = -1,
            .ip = 0,
            .running = true,
            .program = program,
        };
        
        memset(vm.registers, 0, sizeof(vm.registers));
        memset(vm.stack, 0, sizeof(vm.stack));
        memset(vm.callstack, 0, sizeof(vm.callstack));
        
        if (label_count > MAXLABELS) {
            report_vm_error(ERR_TOO_MANY_LABELS, 0, NULL, "Too many labels");
        }
        
        if (labels && label_count > 0) {
            memcpy(vm.labels, labels, label_count * sizeof(Label));
        }
                while (vm.running) {
       
            if (vm.ip < 0 || vm.ip >= program_size) {
                report_vm_error(ERR_PC_OUT_OF_BOUNDS, vm.ip, "index", 
                               "Instruction pointer out of bounds");
            }
            const Instr* instr = &vm.program[vm.ip++];
            instr->execute(&vm, instr);

            record_vm(vm.ip);
            record_asm(instr->ID);

            vm.stepcount++;
            
            if (vm.stepcount >= MAXSTEPS) {
                report_vm_error(ERR_MAX_INSTRUCTIONS, vm.ip, NULL, 
                               "Exceeded maximum instruction count");
            }
        }
        
        // Clean exit
        free_program(program, program_size);
        if (labels) free(labels);
        exit(ERR_OK);
    }

    // Parent process - monitor child
  int status;
  time_t start = time(NULL);
    
  while (1) {
  int result = waitpid(pid, &status, WNOHANG);
        
  if (result == -1) {
    perror("waitpid");
    return ERR_UNKNOWN;
  }
        
  if (result > 0) {
    // Child exited
    if (WIFEXITED(status)) {
      int exit_code = WEXITSTATUS(status);
          // Coverage tracking
    bool new_vm = false;
    bool new_asm = false;

    for(size_t i = 0; i < VM_COVERAGE_MAP_SIZE; i++){
      if(vm_coverage_map[i] > 0 && vm_virgin_map[i] == 0xFF){
        vm_virgin_map[i] = 0;
        new_vm = true;
      }
      if(asm_coverage_map[i] > 0 && asm_virgin_map[i] == 0xFF){
          asm_virgin_map[i] = 0;
          new_asm = true;
      }
    }

    if(stats && (new_vm || new_asm)){
      int curr_run = stats->total_runs;
      if(new_vm){
        stats->vm_new_cov += vm_coverage_count_bits();
        vm_coverage_write(VM_COVERAGE_FILE);
      }
      if(new_asm){
        stats->asm_new_cov += asm_coverage_count_bits(); 
        asm_coverage_write(ASM_COVERAGE_FILE);
      }

      save_corpus(test_input, stats, curr_run);
    } 
          
      if (exit_code == ERR_OK) {
        stats->successful_runs++;
      } else {
        categorize_error(exit_code, stats);
      }
                
      return exit_code;
    }
            
    if (WIFSIGNALED(status)) {
      int sig = WTERMSIG(status);
      stats->crashes++;
      save_crash(test_input, "Signal", sig, stats);
      return ERR_UNKNOWN;
    }


  }
        // Check timeout
    if (difftime(time(NULL), start) >= TIMEOUT_SECONDS) {
      kill(pid, SIGKILL);
      waitpid(pid, &status, 0);
      stats->hangs++;
      save_hang(test_input, stats);
      return ERR_TIMEOUT;
    }
        
    struct timespec ts = {0, 10 * 1000 * 1000};
    nanosleep(&ts, NULL);
  }
}

// Apply random mutations to buffer
static void apply_mutations(Buffer* buf, int num_mutations) {
    typedef bool (*MutationFunc)(Buffer*);
    
    MutationFunc mutations[] = {
        // Byte-level
        mut_flip_bit,
        mut_flip_byte,
        mut_insert_byte,
        mut_delete_byte,
        mut_duplicate_chunk,
        
        // Instruction-level
        mut_swap_opcode,
        mut_corrupt_opcode,
        mut_boundary_value,
        mut_invalid_register,
        mut_swap_operands,
        mut_insert_instruction,
        mut_delete_instruction,
        mut_duplicate_instruction,
        mut_shuffle_instructions,
        
        // Runtime-level
        mut_stack_overflow,
        mut_stack_underflow,
        mut_callstack_overflow,
        mut_divide_by_zero,
        mut_integer_overflow,
        mut_uninitialized_register,
        mut_invalid_jump,
        mut_lonely_return,
        mut_break_label,
        mut_duplicate_label,
        mut_infinite_loop,
        mut_missing_halt,
        
        // Formatting-level
        mut_excess_whitespace,
        mut_long_line,
        mut_empty_lines,
        mut_inject_comment,
    };
    
    int num_mutators = sizeof(mutations) / sizeof(mutations[0]);
    
    for (int i = 0; i < num_mutations; i++) {
        int idx = rand_range(0, num_mutators - 1);
        mutations[idx](buf);
    }
}


// Print statistics
static void print_stats(FuzzStats* stats) {
    double elapsed = (time_now_ms() - stats->start_time) / 1000.0;
    double execs_per_sec = elapsed > 0 ? stats->total_runs / elapsed : 0;
    
    printf("\n========== FUZZING STATISTICS ==========\n");
    printf("Total runs:        %d\n", stats->total_runs);
    printf("Time elapsed:      %.2f seconds\n", elapsed);
    printf("Execs/sec:         %.2f\n", execs_per_sec);
    printf("\n");
    printf("Successful:        %d (%.1f%%)\n", 
           stats->successful_runs, 
           100.0 * stats->successful_runs / stats->total_runs);
    printf("Crashes:           %d (%.1f%%)\n", 
           stats->crashes, 
           100.0 * stats->crashes / stats->total_runs);
    printf("Hangs:             %d (%.1f%%)\n", 
           stats->hangs, 
           100.0 * stats->hangs / stats->total_runs);
    printf("\n");
    printf("VM Coverage:             %d (%.1f%%)\n", 
           stats->vm_new_cov, 
           100.0 * stats->vm_new_cov / stats->total_runs);
    printf("\n");
    printf("ASM Coverage:             %d (%.1f%%)\n", 
           stats->asm_new_cov, 
           100.0 * stats->asm_new_cov / stats->total_runs);
    printf("\n");

    printf("Assembler errors:  %d (%.1f%%)\n", 
           stats->asm_errors, 
           100.0 * stats->asm_errors / stats->total_runs);
    printf("  - Syntax:        %d\n", stats->syntax_errors);
    printf("  - Labels:        %d\n", stats->label_errors);
    printf("\n");
    printf("VM errors:         %d (%.1f%%)\n", 
           stats->vm_errors, 
           100.0 * stats->vm_errors / stats->total_runs);
    printf("  - Stack OF:      %d\n", stats->stack_overflows);
    printf("  - Stack UF:      %d\n", stats->stack_underflows);
    printf("  - Div by zero:   %d\n", stats->divide_by_zero);
    printf("  - Inf loops:     %d\n", stats->infinite_loops);
    printf("  - Registers:     %d\n", stats->register_errors);
    printf("\n");
    printf("Empty programs:    %d\n", stats->empty_programs);
    printf("========================================\n");
}

// Save statistics to file
static void save_stats(FuzzStats* stats) {
    FILE* f = fopen(STATS_FILE, "w");
    if (!f) return;
    
    double elapsed = (time_now_ms() - stats->start_time) / 1000.0;
    
    fprintf(f, "total_runs: %d\n", stats->total_runs);
    fprintf(f, "elapsed_sec: %.2f\n", elapsed);
    fprintf(f, "crashes: %d\n", stats->crashes);
    fprintf(f, "hangs: %d\n", stats->hangs);
    fprintf(f, "new_discoveries: %d\n", total_new_cov(stats));
    fprintf(f, "asm_errors: %d\n", stats->asm_errors);
    fprintf(f, "vm_errors: %d\n", stats->vm_errors);
    fprintf(f, "successful: %d\n", stats->successful_runs);
    
    fclose(f);
}

int main(int argc, char** argv) {
    // Initialize
    coverage_init_shared();
    init_rg();
    dir_create(CRASHES_DIR);
    dir_create(CORPUS_DIR);
    dir_create(COVERAGE_DIR);
    FuzzStats stats = {0};
    stats.start_time = time_now_ms();
    init_vm_virgin();
    init_asm_virgin();

    
    int max_iterations = MAX_ITERATIONS;
    if (argc > 1) {
        max_iterations = atoi(argv[1]);
    }
    
    printf("🐛 Stack VM Fuzzer\n");
    printf("==================\n");
    printf("Max iterations: %d\n", max_iterations);
    printf("Timeout: %d seconds\n", TIMEOUT_SECONDS);
    printf("Crashes dir: %s/\n", CRASHES_DIR);
    printf("\n");
    
    // Count seed programs
    plant_seeds(CORPUS_DIR);
    
    // Main fuzzing loop
    for (int i = 0; i < max_iterations; i++) {
        vm_coverage_reset();
        asm_coverage_reset();

        // Select random seed
        int corpus_idx = rand_range(0, corps_count - 1);
        const char* body = corpus[corpus_idx];
        
        // Create buffer from seed
        Buffer* test_buf = buf_new(2048);
        FILE *f_corpus = fopen(body, "rb");
        if(!f_corpus){
          buf_free(test_buf);
          continue;
        }
        fseek(f_corpus, 0, SEEK_END);
        long f_cor_size = ftell(f_corpus);
        rewind(f_corpus);
        
        char* data = malloc(f_cor_size);
        fread(data, 1, f_cor_size, f_corpus);
        fclose(f_corpus);
        
        buf_append(test_buf, data, f_cor_size);
        free(data);
        // Apply mutations
        /*int num_mutations = rand_range(1, 3);*/
        apply_mutations(test_buf, 2);
        
        // Run test
        Errors result = run_single_test(test_buf, &stats);
        (void)result;
        // Cleanup
        buf_free(test_buf);
        

        // Progress report every 100 runs
         if ((i + 1) % 100 == 0) {
            printf("[%d/%d] Crashes: %d | Hangs: %d | ASM: %d | VM: %d | OK: %d\n",
                   i + 1, max_iterations,
                   stats.crashes, stats.hangs,
                   stats.asm_errors, stats.vm_errors,
                   stats.successful_runs);
        } 
    }
    
    // Final report
    print_stats(&stats); 
    save_stats(&stats);
    
    return 0;
}
