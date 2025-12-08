# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -g
LDFLAGS = -lm

# Directories
SRC_DIR = .
FUZZ_DIR = fuzzers

# VM sources (has main.c)
VM_SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/n_assembler.c $(SRC_DIR)/error.c $(SRC_DIR)/vm_core.c
VM_OBJS = $(VM_SRCS:.c=.o)

# Fuzzer sources (no main from VM)
FUZZ_SRCS = $(FUZZ_DIR)/RL_fuzzer.c $(FUZZ_DIR)/fuzzer_util.c $(SRC_DIR)/n_assembler.c $(SRC_DIR)/error.c $(SRC_DIR)/vm_core.c
FUZZ_OBJS = $(FUZZ_SRCS:.c=.o)

# Binaries
VM_BIN = vm
FUZZ_BIN = fuzzer

# Default build
all: $(VM_BIN) $(FUZZ_BIN)

# Build VM
$(VM_BIN): $(VM_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build fuzzer
$(FUZZ_BIN): $(FUZZ_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Generic compile rule
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(VM_OBJS) $(FUZZ_OBJS) $(VM_BIN) $(FUZZ_BIN)

.PHONY: all clean
