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
#include <sys/mman.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <math.h>
#include "../coverage.h"
#include "../header.h"
#include "../error.h"
#include "rl_bridge/state.h"
#include "rl_bridge/rl_comm.h"
#include "fuzzer_util.h"

// ================= CONFIGURATION =================

#define MAX_ITERATIONS 30000
#define TIMEOUT_SECONDS 5
#define TEMP_INPUT_FILE "fuzz_input.txt"
#define CRASHES_DIR "crashes"
#define CORPUS_DIR "corpus"
#define STATS_FILE "fuzz_stats.txt"
#define COVERAGE_DIR "coverage"
#define VM_COVERAGE_FILE "coverage/vm_coverage.bin"
#define ASM_COVERAGE_FILE "coverage/asm_coverage.bin"
#define RL_CSV_LOG_FILE "rl_fuzzer_log.csv"
#define MAX_CORPUS 512
#define MAX_CORPUS_PATH 512
#define NUM_MUTATION_PER_RUN 3

// ================= DATA STRUCTURES =================

typedef struct {
    uint8_t vm_coverage[VM_COVERAGE_MAP_SIZE];
    uint8_t asm_coverage[ASM_COVERAGE_MAP_SIZE];
    uint32_t prev_vm_loc;
    uint32_t prev_asm_loc;
    Errors result_code;
    uint32_t step_count;
} SharedCoverageData;

static SharedCoverageData* shared_cov = NULL;

typedef enum {
    TIER_SAFE = 0,
    TIER_STRUCTURAL = 1,
    TIER_CHAOS = 2,
    NUM_TIERS = 3
} MutationTier;

typedef struct {
    char path[MAX_CORPUS_PATH];
    uint32_t total_cov;
    uint32_t last_new_cov;
    uint32_t last_used;
    uint32_t exec_count;
    bool is_seed;
    uint32_t vm_execs;
    uint32_t successful;
} Corpus_entry;

static Corpus_entry corpus[MAX_CORPUS];
static int corps_count = 0;

// ================= COVERAGE TRACKING =================

static uint8_t vm_virgin_map[VM_COVERAGE_MAP_SIZE];
static uint8_t asm_virgin_map[ASM_COVERAGE_MAP_SIZE];

static void init_vm_virgin(void) {
    memset(vm_virgin_map, 0xFF, VM_COVERAGE_MAP_SIZE);
}

static void init_asm_virgin(void) {
    memset(asm_virgin_map, 0xFF, ASM_COVERAGE_MAP_SIZE);
}

static void record_vm_edge(uint32_t loc) {
    uint32_t edge = hash_edge(shared_cov->prev_vm_loc, loc) % VM_COVERAGE_MAP_SIZE;
    uint8_t *slot = &shared_cov->vm_coverage[edge];
    if (*slot < 255) (*slot)++;
    shared_cov->prev_vm_loc = loc >> 1;
}

static void record_asm_edge(uint32_t opcode, uint32_t line) {
    uint32_t loc = (opcode << 16) | (line & 0xFFFF);
    uint32_t edge = hash_edge(shared_cov->prev_asm_loc, loc) % ASM_COVERAGE_MAP_SIZE;
    uint8_t *slot = &shared_cov->asm_coverage[edge];
    if (*slot < 255) (*slot)++;
    shared_cov->prev_asm_loc = loc >> 1;
}

typedef struct {
    uint32_t vm_new;
    uint32_t asm_new;
} CoverageResult;

static CoverageResult process_coverage(void) {
    CoverageResult result = {0, 0};
    
    for (size_t i = 0; i < VM_COVERAGE_MAP_SIZE; i++) {
        if (shared_cov->vm_coverage[i] > 0 && vm_virgin_map[i] == 0xFF) {
            vm_virgin_map[i] = 0;
            result.vm_new++;
        }
    }

    for (size_t i = 0; i < ASM_COVERAGE_MAP_SIZE; i++) {
        if (shared_cov->asm_coverage[i] > 0 && asm_virgin_map[i] == 0xFF) {
            asm_virgin_map[i] = 0;
            result.asm_new++;
        }
    }
    
    return result;
}

// ================= STATISTICS =================

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

static inline uint32_t total_new_cov(FuzzStats* stats) {
    return stats->vm_new_cov + stats->asm_new_cov;
}

static bool is_asm_error(Errors err) {
    switch(err) {
        case ERR_SYNTAX:
        case ERR_INVALID_TOKEN:
        case ERR_TOO_FEW_OPERANDS:
        case ERR_TOO_MANY_OPERANDS:
        case ERR_LINE_TOO_LONG:
        case ERR_TOKEN_TOO_LONG:
        case ERR_INVALID_REGISTER:
        case ERR_INVALID_LITERAL:
        case ERR_OPERAND_OUT_OF_RANGE:
        case ERR_TOO_MANY_LINES:
        case ERR_DUPLICATE_LABEL:
        case ERR_UNRESOLVED_LABEL:
        case ERR_TOO_MANY_LABELS:
        case ERR_LABEL_TOO_LONG:
            return true;
        default:
            return false;
    }
}

static bool is_vm_error(Errors err) {
    switch(err) {
        case ERR_CALLSTACK_OVERFLOW:
        case ERR_CALLSTACK_UNDERFLOW:
        case ERR_UNKNOWN_OPCODE:
        case ERR_PC_OUT_OF_BOUNDS:
        case ERR_STACK_OVERFLOW:
        case ERR_STACK_UNDERFLOW:
        case ERR_DIVIDE_BY_ZERO:
        case ERR_REGISTER_OUT_OF_BOUNDS:
        case ERR_MISSING_HALT:
        case ERR_MAX_INSTRUCTIONS:
            return true;
        default:
            return false;
    }
}

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
            break;
    }
}

// ================= FILE I/O =================
static inline float coverage_per_success(uint32_t cov, uint32_t success) {
    return success > 0 ? (float)cov / (float)success : 0.0f;
}


static bool write_test_case(Buffer* buf) {
    return file_write(TEMP_INPUT_FILE, buf->data, buf->length);
}

static void save_crash(Buffer* buf, const char* reason, int signal, FuzzStats* stats) {
    char filename[512];
    snprintf(filename, sizeof(filename), 
             "%s/crash_%d_sig%d_%ld.txt", 
             CRASHES_DIR, stats->crashes, signal, (long)time(NULL));
    
    FILE* f = fopen(filename, "w");
    if (f) {
        fprintf(f, "; Crash reason: %s (signal %d)\n", reason, signal);
        fprintf(f, "; Total runs: %d\n\n", stats->total_runs);
        fwrite(buf->data, 1, buf->length, f);
        fclose(f);
        printf("Saved crash to: %s\n", filename);
    }
}

static void save_hang(Buffer* buf, FuzzStats* stats) {
    char filename[512];
    snprintf(filename, sizeof(filename), 
             "%s/hang_%d_%ld.txt", 
             CRASHES_DIR, stats->hangs, (long)time(NULL));
    
    FILE* f = fopen(filename, "w");
    if (f) {
        fprintf(f, "; Program timeout after %d seconds\n", TIMEOUT_SECONDS);
        fprintf(f, "; Total runs: %d\n\n", stats->total_runs);
        fwrite(buf->data, 1, buf->length, f);
        fclose(f);
        printf("Saved hang to: %s\n", filename);
    }
}

static void save_corpus(Buffer* buf, FuzzStats* stats, int run, uint32_t vm_new, uint32_t asm_new) {
    char filename[512];
    snprintf(filename, sizeof(filename), 
             "%s/corpus_%u_%ld.txt", 
             CORPUS_DIR, vm_new + asm_new, (long)time(NULL));

    FILE* f = fopen(filename, "w");
    if (f) {
        fprintf(f, "; Responsible run was %d\n", run);
        fprintf(f, "; Program found %u new edges in VM, %u in ASM || Total: %u\n",
                vm_new, asm_new, vm_new + asm_new);
        fprintf(f, "; Total runs: %d\n\n", stats->total_runs);
        fwrite(buf->data, 1, buf->length, f);
        fclose(f);
        printf("Saved new corpus to %s (vm:%u asm:%u)\n", filename, vm_new, asm_new);
    }
}

static void save_coverage_map(const char* path, uint8_t* map, size_t size) {
    FILE* f = fopen(path, "wb");
    if (f) {
        fwrite(map, 1, size, f);
        fclose(f);
    }
}

static void init_csv_log(void) {
    FILE* f = fopen(RL_CSV_LOG_FILE, "w");
    if (f) {
        fprintf(f, 
                "iteration,crashes,hangs,asm_errors,vm_errors,successful,reward,"
                "vm_cov,asm_cov,"
                "asm_cov_per_success,vm_cov_per_success\n");
        fclose(f);
    }
}

static void log_to_csv(int iteration, FuzzStats* stats, float reward) {
    FILE* f = fopen(RL_CSV_LOG_FILE, "a");
  float asm_per_success =
        coverage_per_success(stats->asm_new_cov, stats->successful_runs);

    float vm_per_success =
        coverage_per_success(stats->vm_new_cov, stats->successful_runs);

    if (f) {
        fprintf(f, "%d,%d,%d,%d,%d,%d,%.6f,%u,%u,%.6f,%.6f\n",
                iteration,
                stats->crashes,
                stats->hangs,
                stats->asm_errors,
                stats->vm_errors,
                stats->successful_runs,
                reward,
                stats->vm_new_cov,
                stats->asm_new_cov,
                asm_per_success,
                vm_per_success);
        fclose(f);
    }
}

// ================= CORPUS MANAGEMENT =================

static int plant_seeds(const char* path) {
    DIR* d = opendir(path);
    if (!d) return -1;
    
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_type != DT_REG) continue;
        if (corps_count >= MAX_CORPUS) break;

        Corpus_entry* e = &corpus[corps_count];
        memset(e, 0, sizeof(*e));
        snprintf(e->path, MAX_CORPUS_PATH, "%s/%s", path, entry->d_name);
        e->is_seed = (strncmp(entry->d_name, "seed", 4) == 0);
        corps_count++;
    }
    closedir(d);
    return 0;
}

static void promote_corpus_entry(int idx) {
    if (idx < 0 || idx >= corps_count) return;

    Corpus_entry *e = &corpus[idx];
    if (e->is_seed) return;

    if (e->last_new_cov > 1 || e->successful >= 2) {
        e->is_seed = true;
        printf("🌱 Promoted to seed: %s\n", e->path);
    }
}

static void reload_corpus(void) {
    corps_count = 0;
    plant_seeds(CORPUS_DIR);
}

static int pick_corpus_entry(int iteration) {
    if (corps_count == 0) return -1;

    double weights[MAX_CORPUS];
    double sum = 0.0;

    for (int i = 0; i < corps_count; i++) {
        int age = iteration - (int)corpus[i].last_used;
        double freshness = (age < 1000) ? 2.0 : 0.0;
        double w = 1.0 
                  + 5.0 * corpus[i].last_new_cov
                  + 0.1 * corpus[i].total_cov
                  + freshness
                  + (corpus[i].is_seed ? 5.0 : 0.0);
        weights[i] = w;
        sum += w;
    }

    double r = (rand() / (double)RAND_MAX) * sum;
    for (int i = 0; i < corps_count; i++) {
        r -= weights[i];
        if (r <= 0) return i;
    }
    return corps_count - 1;
}

static void update_corpus(int idx, uint32_t new_vm_cov, uint32_t new_asm_cov, int iteration) {
    if (idx < 0 || idx >= corps_count) return;
    
    uint32_t new_cov = new_asm_cov + new_vm_cov;
    corpus[idx].last_new_cov = new_cov;
    corpus[idx].total_cov += new_cov;
    corpus[idx].last_used = (uint32_t)iteration;
    corpus[idx].exec_count++;

    for (int k = 0; k < corps_count; k++) {
        corpus[k].last_new_cov = (uint32_t)(corpus[k].last_new_cov * 0.9);
    }
}

// ================= MUTATIONS =================

typedef bool (*MutationFunc)(Buffer*);

static MutationFunc tier_safe[] = {
    mut_boundary_value,
    mut_swap_operands,
    mut_insert_instruction,
    mut_duplicate_instruction,
    mut_shuffle_instructions,
    mut_swap_opcode,
    mut_excess_whitespace,
    mut_long_line,
    mut_empty_lines,
    mut_inject_comment,
};
#define TIER_SAFE_COUNT ((int)(sizeof(tier_safe) / sizeof(tier_safe[0])))

static MutationFunc tier_structural[] = {
    mut_corrupt_opcode,
    mut_delete_instruction,
    mut_break_label,
    mut_lonely_return,
    mut_invalid_register,
};
#define TIER_STRUCTURAL_COUNT ((int)(sizeof(tier_structural) / sizeof(tier_structural[0])))

static MutationFunc tier_chaos[] = {
    mut_flip_bit,
    mut_flip_byte,
    mut_insert_byte,
    mut_delete_byte,
    mut_duplicate_chunk,
    mut_stack_overflow,
    mut_stack_underflow,
    mut_divide_by_zero,
    mut_uninitialized_register,
    mut_infinite_loop,
    mut_missing_halt, 
    mut_callstack_overflow,
    mut_integer_overflow,
    mut_invalid_jump,
    mut_duplicate_label,
};
#define TIER_CHAOS_COUNT ((int)(sizeof(tier_chaos) / sizeof(tier_chaos[0])))

static void apply_mutation_tier(Buffer *buf, int tier, int mutation_idx) {
    MutationFunc *list = NULL;
    int count = 0;

    switch (tier) {
        case TIER_SAFE:
            list = tier_safe;
            count = TIER_SAFE_COUNT;
            break;
        case TIER_STRUCTURAL:
            list = tier_structural;
            count = TIER_STRUCTURAL_COUNT;
            break;
        case TIER_CHAOS:
            list = tier_chaos;
            count = TIER_CHAOS_COUNT;
            break;
        default:
            return;
    }

    if (mutation_idx < 0 || mutation_idx >= count) return;
    list[mutation_idx](buf);
}

static Errors run_single_test(Buffer* test_input, FuzzStats* stats, CoverageResult* cov_result) {
    stats->total_runs++;
    
    cov_result->vm_new = 0;
    cov_result->asm_new = 0;
    
    if (!write_test_case(test_input)) {
        fprintf(stderr, "Failed to write test case\n");
        return ERR_IO;
    }
    
    memset(shared_cov->vm_coverage, 0, VM_COVERAGE_MAP_SIZE);
    memset(shared_cov->asm_coverage, 0, ASM_COVERAGE_MAP_SIZE);
    shared_cov->prev_vm_loc = 0;
    shared_cov->prev_asm_loc = 0;
    shared_cov->step_count = 0;
    shared_cov->result_code = ERR_OK;
    
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork");
        return ERR_UNKNOWN;
    }
    
    if (pid == 0) {
        // ==================== CHILD PROCESS ====================
        
        FILE* err_log = fopen("fuzz_stderr.log", "w");
        if (err_log) {
            dup2(fileno(err_log), STDERR_FILENO);
            fclose(err_log);
        }
        
        Instr* program = NULL;
        int program_size = 0;
        Label* labels = NULL;
        int label_count = 0;
        
        define_program(&program, &program_size, &labels, &label_count);
        
        if (!program || program_size == 0) {
            exit(ERR_EMPTY_PROGRAM);
        }
        
        for (int i = 0; i < program_size; i++) {
            record_asm_edge((uint32_t)program[i].ID, (uint32_t)i);
        }
        
        VM vm;
        memset(&vm, 0, sizeof(VM));
        vm.call_sp = -1;
        vm.stepcount = 0;
        vm.lb = label_count;
        vm.sp = -1;
        vm.ip = 0;
        vm.running = true;
        vm.program = program;
        
        if (label_count > MAXLABELS) {
            report_vm_error(ERR_TOO_MANY_LABELS, 0, NULL, "Too many labels");
        }
        
        if (labels && label_count > 0) {
            memcpy(vm.labels, labels, (size_t)label_count * sizeof(Label));
        }
        
        while (vm.running) {
            if (vm.ip < 0 || vm.ip >= program_size) {
                report_vm_error(ERR_PC_OUT_OF_BOUNDS, vm.ip, "index", 
                               "Instruction pointer out of bounds");
            }
            
            const Instr* instr = &vm.program[vm.ip];
            
            record_vm_edge((uint32_t)vm.ip);
            record_asm_edge((uint32_t)instr->ID, (uint32_t)vm.ip);
            
            vm.ip++;
            instr->execute(&vm, instr);

            vm.stepcount++;
            shared_cov->step_count = (uint32_t)vm.stepcount;
            
            if (vm.stepcount >= MAXSTEPS) {
                report_vm_error(ERR_MAX_INSTRUCTIONS, vm.ip, NULL, 
                               "Exceeded maximum instruction count");
            }
            
            if (current_state) {
                current_state->numeric_features[6] = (float)vm.stepcount;
            }
        }
        
        free_program(program, program_size);
        if (labels) free(labels);
        exit(ERR_OK);
    }

    // ==================== PARENT PROCESS ====================
    int status;
    time_t start = time(NULL);
    
    while (1) {
        int result = waitpid(pid, &status, WNOHANG);
        
        if (result == -1) {
            perror("waitpid");
            return ERR_UNKNOWN;
        }
        
        if (result > 0) {
            if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);

                if (exit_code != ERR_OK && exit_code < ERR_COUNT) {
                    if (is_asm_error((Errors)exit_code)) {
                        state_update_asm_error(current_state, (Errors)exit_code);
                    } else if (is_vm_error((Errors)exit_code)) {
                        state_update_vm_error(current_state, (Errors)exit_code);
                    }
                }
                
                *cov_result = process_coverage();
                
                stats->vm_new_cov += cov_result->vm_new;
                stats->asm_new_cov += cov_result->asm_new;

                if (cov_result->vm_new > 0 || cov_result->asm_new > 0) {
                    save_corpus(test_input, stats, stats->total_runs, 
                               cov_result->vm_new, cov_result->asm_new);
                    
                    if (cov_result->vm_new > 0) {
                        save_coverage_map(VM_COVERAGE_FILE, 
                                         shared_cov->vm_coverage, 
                                         VM_COVERAGE_MAP_SIZE);
                    }
                    if (cov_result->asm_new > 0) {
                        save_coverage_map(ASM_COVERAGE_FILE, 
                                         shared_cov->asm_coverage, 
                                         ASM_COVERAGE_MAP_SIZE);
                    }
                }

                if (exit_code == ERR_OK) {
                    stats->successful_runs++;
                } else {
                    categorize_error((Errors)exit_code, stats);
                }
                
                return (Errors)exit_code;
            }
            
            if (WIFSIGNALED(status)) {
                int sig = WTERMSIG(status);
                
                *cov_result = process_coverage();
                stats->vm_new_cov += cov_result->vm_new;
                stats->asm_new_cov += cov_result->asm_new;
                
                stats->crashes++;
                save_crash(test_input, "Signal", sig, stats);
                
                if (cov_result->vm_new > 0 || cov_result->asm_new > 0) {
                    save_corpus(test_input, stats, stats->total_runs,
                               cov_result->vm_new, cov_result->asm_new);
                }
                
                return ERR_UNKNOWN;
            }
        }

        if (difftime(time(NULL), start) >= TIMEOUT_SECONDS) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            
            *cov_result = process_coverage();
            stats->vm_new_cov += cov_result->vm_new;
            stats->asm_new_cov += cov_result->asm_new;
            
            stats->hangs++;
            save_hang(test_input, stats);
            return ERR_TIMEOUT;
        }
        
        struct timespec ts = {0, 10 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
}

// ================= REWARD COMPUTATION =================

static float compute_reward(Errors result, uint32_t vm_cov, uint32_t asm_cov, int prog_size) {
    float reward = 0.0f;
    uint32_t cov = vm_cov + asm_cov;
    
    if (cov > 0) {
        reward += 5.0f * logf(1.0f + (float)cov);
    } else {
        reward -= 0.5f;
    }

    if (is_vm_error(result)) {
        if (cov > 0) {
            reward += 0.5f;
        } else {
            reward -= 0.3f;
        }
    }

    if (is_asm_error(result)) {
        reward -= (prog_size < 20 ? 0.1f : 0.5f);
    }

    if (result == ERR_OK) {
        reward += 0.5f;
    }

    if (result == ERR_TIMEOUT) {
        reward -= 1.0f;
    }
    
    reward += 0.001f * fminf(200.0f, (float)prog_size);

    return reward;
}

// ================= STATISTICS PRINTING =================

static void print_stats(FuzzStats* stats) {
    double elapsed = (double)(time_now_ms() - stats->start_time) / 1000.0;
    double execs_per_sec = elapsed > 0 ? stats->total_runs / elapsed : 0;
    
    printf("\n========== FUZZING STATISTICS ==========\n");
    printf("Total runs:        %d\n", stats->total_runs);
    printf("Time elapsed:      %.2f seconds\n", elapsed);
    printf("Execs/sec:         %.2f\n", execs_per_sec);
    printf("\n");
    printf("Successful:        %d (%.1f%%)\n", 
           stats->successful_runs, 
           100.0 * stats->successful_runs / (stats->total_runs + 1));
    printf("Crashes:           %d (%.1f%%)\n", 
           stats->crashes, 
           100.0 * stats->crashes / (stats->total_runs + 1));
    printf("Hangs:             %d (%.1f%%)\n", 
           stats->hangs, 
           100.0 * stats->hangs / (stats->total_runs + 1));
    printf("\n");
    printf("VM Edge Coverage:  %u\n", stats->vm_new_cov);
    printf("ASM Edge Coverage: %u\n", stats->asm_new_cov);
    printf("\n");
    printf("Assembler errors:  %d (%.1f%%)\n", 
           stats->asm_errors, 
           100.0 * stats->asm_errors / (stats->total_runs + 1));
    printf("  - Syntax:        %d\n", stats->syntax_errors);
    printf("  - Labels:        %d\n", stats->label_errors);
    printf("\n");
    printf("VM errors:         %d (%.1f%%)\n", 
           stats->vm_errors, 
           100.0 * stats->vm_errors / (stats->total_runs + 1));
    printf("  - Stack OF:      %d\n", stats->stack_overflows);
    printf("  - Stack UF:      %d\n", stats->stack_underflows);
    printf("  - Div by zero:   %d\n", stats->divide_by_zero);
    printf("  - Inf loops:     %d\n", stats->infinite_loops);
    printf("  - Registers:     %d\n", stats->register_errors);
    printf("\n");
    printf("Empty programs:    %d\n", stats->empty_programs);
    printf("========================================\n");
}

static void save_stats(FuzzStats* stats) {
    FILE* f = fopen(STATS_FILE, "w");
    if (!f) return;
    
    double elapsed = (double)(time_now_ms() - stats->start_time) / 1000.0;
    
    fprintf(f, "total_runs: %d\n", stats->total_runs);
    fprintf(f, "elapsed_sec: %.2f\n", elapsed);
    fprintf(f, "crashes: %d\n", stats->crashes);
    fprintf(f, "hangs: %d\n", stats->hangs);
    fprintf(f, "vm_edge_coverage: %u\n", stats->vm_new_cov);
    fprintf(f, "asm_edge_coverage: %u\n", stats->asm_new_cov);
    fprintf(f, "asm_errors: %d\n", stats->asm_errors);
    fprintf(f, "vm_errors: %d\n", stats->vm_errors);
    fprintf(f, "successful: %d\n", stats->successful_runs);
    
    fclose(f);
}

// ================= MAIN =================

int main(int argc, char** argv) {
    init_rg();
    dir_create(CRASHES_DIR);
    dir_create(CORPUS_DIR);
    dir_create(COVERAGE_DIR);
    
    FuzzStats stats;
    memset(&stats, 0, sizeof(FuzzStats));
    stats.start_time = time_now_ms();
    
    init_vm_virgin();
    init_asm_virgin();
    
    shared_cov = mmap(NULL, sizeof(SharedCoverageData),
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared_cov == MAP_FAILED) {
        perror("mmap shared_cov");
        return 1;
    }
    memset(shared_cov, 0, sizeof(SharedCoverageData));
    
    current_state = mmap(NULL, sizeof(State), PROT_READ | PROT_WRITE, 
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (current_state == MAP_FAILED) {
        perror("mmap current_state");
        munmap(shared_cov, sizeof(SharedCoverageData));
        return 1;
    }
    state_init(current_state);
    
    if (rl_comm_init("/home/shu/testing.sock") < 0) {
        fprintf(stderr, "Warning: Could not connect to RL agent, running without RL\n");
    }
    
    int max_iterations = MAX_ITERATIONS;
    if (argc > 1) {
        max_iterations = atoi(argv[1]);
    }

    printf("🐛 Stack VM Fuzzer (Edge Coverage)\n");
    printf("===================================\n");
    printf("Max iterations: %d\n", max_iterations);
    printf("Timeout: %d seconds\n", TIMEOUT_SECONDS);
    printf("Crashes dir: %s/\n", CRASHES_DIR);
    printf("\n");
    
    plant_seeds(CORPUS_DIR);
    init_csv_log();
    
    for (int i = 0; i < max_iterations; i++) {
        float reward = 0.0f;
        state_reset(current_state);

        int corpus_idx = pick_corpus_entry(i);
        if (corpus_idx < 0) {
            Buffer* test_buf = buf_new(128);
            buf_append(test_buf, "hlt\n", 4);
            CoverageResult cov_result;
            run_single_test(test_buf, &stats, &cov_result);
            buf_free(test_buf);
            continue;
        }

        const char* body = corpus[corpus_idx].path;
        Buffer* test_buf = buf_new(2048);
        
        FILE *f_corpus = fopen(body, "rb");
        if (!f_corpus) {
            buf_free(test_buf);
            continue;
        }

        fseek(f_corpus, 0, SEEK_END);
        long f_cor_size = ftell(f_corpus);
        rewind(f_corpus);
        
        if ((i + 1) % 1000 == 0) {
            reload_corpus();
            printf("Reloaded corpus: %d files\n", corps_count);
        }
        
        char* data = malloc((size_t)f_cor_size + 1);
        if (!data) {
            fclose(f_corpus);
            buf_free(test_buf);
            continue;
        }
        
        size_t bytes_read = fread(data, 1, (size_t)f_cor_size, f_corpus);
        fclose(f_corpus);
        
        if (bytes_read != (size_t)f_cor_size) {
            free(data);
            buf_free(test_buf);
            continue;
        }
        
        buf_append(test_buf, data, (size_t)f_cor_size);
        free(data);

        float* send_vector = malloc(sizeof(float) * STATE_VECTOR_SIZE(current_state));
        if (!send_vector) {
            buf_free(test_buf);
            continue;
        }
        
        state_serialize(current_state, send_vector);
        rl_send_state(send_vector, STATE_VECTOR_SIZE(current_state));
        free(send_vector);

        int actions[NUM_MUTATION_PER_RUN][2];
        if (rl_recv_action((int*)actions, NUM_MUTATION_PER_RUN * 2) < 0) {
            fprintf(stderr, "Failed to receive action\n");
            buf_free(test_buf);
            continue;
        }
        
        int tier_used[NUM_TIERS] = {0, 0, 0};
        
        for (int l = 0; l < NUM_MUTATION_PER_RUN; l++) {
            int tier = actions[l][0] % NUM_TIERS;
            int mut = actions[l][1];
            
            switch (tier) {
                case TIER_SAFE:
                    mut = mut % TIER_SAFE_COUNT;
                    break;
                case TIER_STRUCTURAL:
                    mut = mut % TIER_STRUCTURAL_COUNT;
                    break;
                case TIER_CHAOS:
                    mut = mut % TIER_CHAOS_COUNT;
                    break;
                default:
                    mut = 0;
                    break;
            }
            
            tier_used[tier] = 1;
            apply_mutation_tier(test_buf, tier, mut);
        }

        CoverageResult cov_result;
        Errors result = run_single_test(test_buf, &stats, &cov_result);
        
        Corpus_entry *e = &corpus[corpus_idx];
        e->exec_count++;
        if (!is_asm_error(result)) {
            e->vm_execs++;
        }
        if (result == ERR_OK) {
            e->successful++;
        }
        
        state_update_run_stats(current_state, cov_result.vm_new, cov_result.asm_new, 
                              (result == ERR_UNKNOWN) ? 1 : 0);

        if (tier_used[TIER_SAFE]) {
            if (cov_result.vm_new > 0 || cov_result.asm_new > 0) {
                current_state->numeric_features[7] += 1.0f;
            } else {
                current_state->numeric_features[7] -= 0.1f;
            }
        }

        if (tier_used[TIER_STRUCTURAL]) {
            if (is_vm_error(result)) {
                current_state->numeric_features[8] += 1.0f;
            } else {
                current_state->numeric_features[8] -= 0.2f;
            }
        }

        if (tier_used[TIER_CHAOS]) {
            if (cov_result.vm_new > 0 || cov_result.asm_new > 0) {
                current_state->numeric_features[9] += 1.0f;
            } else {
                current_state->numeric_features[9] -= 0.2f;
            }
        }

        update_corpus(corpus_idx, cov_result.vm_new, cov_result.asm_new, i);
        promote_corpus_entry(corpus_idx);
        
        reward = compute_reward(result, cov_result.vm_new, cov_result.asm_new, 
                               (int)current_state->numeric_features[5]);
        rl_send_reward(reward);

        buf_free(test_buf);

        if ((i + 1) % 100 == 0) {
            log_to_csv(i + 1, &stats, reward);
            printf("[%d/%d] Crashes: %d | Hangs: %d | ASM: %d | VM: %d | OK: %d | "
                   "VM_Cov: %u | ASM_Cov: %u\n",
                   i + 1, max_iterations,
                   stats.crashes, stats.hangs,
                   stats.asm_errors, stats.vm_errors,
                   stats.successful_runs,
                   stats.vm_new_cov, stats.asm_new_cov);
        }
    }
    
    print_stats(&stats); 
    save_stats(&stats);
    rl_comm_close();
    munmap(current_state, sizeof(State));
    munmap(shared_cov, sizeof(SharedCoverageData));
    
    return 0;
}
