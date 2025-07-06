# Benchmark suite for RistrettoDB vs SQLite
CC = clang
CFLAGS = -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS = -lsqlite3 -lm

# Directories
SRC_DIR = .
RISTRETTO_SRC_DIR = ../src
RISTRETTO_INC_DIR = ../include
BIN_DIR = ../bin

# Source files
RISTRETTO_SOURCES = $(wildcard $(RISTRETTO_SRC_DIR)/*.c)
RISTRETTO_OBJECTS = $(RISTRETTO_SOURCES:$(RISTRETTO_SRC_DIR)/%.c=%.o)

# Filter out main.c and ristretto_cli.c from RistrettoDB sources
RISTRETTO_LIB_SOURCES = $(filter-out $(RISTRETTO_SRC_DIR)/main.c $(RISTRETTO_SRC_DIR)/ristretto_cli.c, $(RISTRETTO_SOURCES))
RISTRETTO_LIB_OBJECTS = $(RISTRETTO_LIB_SOURCES:$(RISTRETTO_SRC_DIR)/%.c=%.o)

# Benchmark executables
BENCHMARKS = benchmark microbench speedtest_subset ultra_fast_benchmark

# Include paths
INCLUDES = -I$(RISTRETTO_INC_DIR) -I../embed

.PHONY: all clean benchmarks run-all

all: benchmarks

benchmarks: $(addprefix $(BIN_DIR)/, $(BENCHMARKS))

# Create bin directory if it doesn't exist
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Compile RistrettoDB object files
%.o: $(RISTRETTO_SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build benchmark executables
$(BIN_DIR)/benchmark: $(SRC_DIR)/benchmark.c $(RISTRETTO_LIB_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(RISTRETTO_LIB_OBJECTS) $(LDFLAGS)

$(BIN_DIR)/microbench: $(SRC_DIR)/microbench.c $(RISTRETTO_LIB_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(RISTRETTO_LIB_OBJECTS) $(LDFLAGS)

$(BIN_DIR)/speedtest_subset: $(SRC_DIR)/speedtest_subset.c $(RISTRETTO_LIB_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(RISTRETTO_LIB_OBJECTS) $(LDFLAGS)

$(BIN_DIR)/ultra_fast_benchmark: $(SRC_DIR)/ultra_fast_benchmark.c table_v2.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< table_v2.o $(LDFLAGS)

# Run all benchmarks
run-all: benchmarks
	@echo "Running comprehensive benchmark suite..."
	@echo "========================================"
	@echo
	@echo "1. SQLite vs RistrettoDB Comparison:"
	@$(BIN_DIR)/benchmark
	@echo
	@echo "2. RistrettoDB Microbenchmarks:"
	@$(BIN_DIR)/microbench
	@echo
	@echo "3. SpeedTest1 Subset:"
	@$(BIN_DIR)/speedtest_subset

# Individual benchmark runs
run-benchmark: $(BIN_DIR)/benchmark
	$(BIN_DIR)/benchmark

run-microbench: $(BIN_DIR)/microbench
	$(BIN_DIR)/microbench

run-speedtest: $(BIN_DIR)/speedtest_subset
	$(BIN_DIR)/speedtest_subset

# Ultra-fast benchmark
run-ultra-fast: $(BIN_DIR)/ultra_fast_benchmark
	$(BIN_DIR)/ultra_fast_benchmark

# Performance analysis targets
profile-benchmark: $(BIN_DIR)/benchmark
	@echo "Running benchmark with Instruments (macOS)..."
	xcrun xctrace record --template "Time Profiler" --launch $(BIN_DIR)/benchmark

cachegrind-benchmark: $(BIN_DIR)/benchmark
	@echo "Running benchmark with Cachegrind..."
	valgrind --tool=cachegrind --cachegrind-out-file=benchmark.cachegrind $(BIN_DIR)/benchmark
	@echo "View results with: cg_annotate benchmark.cachegrind"

# Memory analysis
memory-benchmark: $(BIN_DIR)/benchmark
	@echo "Running benchmark with Valgrind memory check..."
	valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all $(BIN_DIR)/benchmark

# Clean targets
clean:
	rm -f *.o
	rm -f *.cachegrind
	rm -f $(addprefix $(BIN_DIR)/, $(BENCHMARKS))
	rm -f *.db

# Help target
help:
	@echo "Available targets:"
	@echo "  all              - Build all benchmarks"
	@echo "  benchmarks       - Build all benchmarks (same as all)"
	@echo "  run-all          - Run all benchmark suites"
	@echo "  run-benchmark    - Run SQLite vs RistrettoDB comparison"
	@echo "  run-microbench   - Run RistrettoDB microbenchmarks"
	@echo "  run-speedtest    - Run SpeedTest1 subset"
	@echo "  profile-benchmark - Profile with Instruments (macOS)"
	@echo "  cachegrind-benchmark - Profile with Cachegrind"
	@echo "  memory-benchmark - Check memory usage with Valgrind"
	@echo "  clean            - Remove all build artifacts"
	@echo "  help             - Show this help message"