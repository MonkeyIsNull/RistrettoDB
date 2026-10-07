# Benchmark suite for RistrettoDB (Table V2).
#
# V2-only: the single benchmark measures Table V2 write throughput. The SQLite
# contrast is OPTIONAL and OFF by default (RistrettoDB has no SQL engine to
# compare against) — enable it with:  make run-ultra-fast WITH_SQLITE=1
CC = clang
# Portable baseline by default (-march=native produces binaries that SIGILL on
# a different CPU). Opt into host tuning with:  make SIMD=native
SIMD ?= portable
CFLAGS = -O3 -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Wpedantic
ifeq ($(SIMD),native)
CFLAGS += -march=native
endif

# The SQLite contrast is opt-in and CI-independent.
LDFLAGS = -lm
ifdef WITH_SQLITE
CFLAGS  += -DRISTRETTO_BENCH_SQLITE
LDFLAGS += -lsqlite3
endif

# Directories
SRC_DIR = .
RISTRETTO_SRC_DIR = ../src
RISTRETTO_INC_DIR = ../include
BIN_DIR = ../bin

# Benchmark executables
BENCHMARKS = ultra_fast_benchmark

# Include paths
INCLUDES = -I$(RISTRETTO_INC_DIR) -I../embed

.PHONY: all clean benchmarks run-all run-ultra-fast help

all: benchmarks

benchmarks: $(addprefix $(BIN_DIR)/, $(BENCHMARKS))

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compile the V2 engine object the benchmark links against.
table_v2.o: $(RISTRETTO_SRC_DIR)/table_v2.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BIN_DIR)/ultra_fast_benchmark: $(SRC_DIR)/ultra_fast_benchmark.c table_v2.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< table_v2.o $(LDFLAGS)

run-all: run-ultra-fast

run-ultra-fast: $(BIN_DIR)/ultra_fast_benchmark
	$(BIN_DIR)/ultra_fast_benchmark

clean:
	rm -f *.o
	rm -f $(addprefix $(BIN_DIR)/, $(BENCHMARKS))
	rm -f *.db
	rm -rf data/

help:
	@echo "Available targets:"
	@echo "  all / benchmarks  - Build the V2 write-throughput benchmark"
	@echo "  run-ultra-fast    - Build and run it (add WITH_SQLITE=1 for the optional contrast)"
	@echo "  clean             - Remove build artifacts"
