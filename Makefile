CC = clang
CFLAGS = -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS = 
DEBUGFLAGS = -g -O0 -DDEBUG
TARGET = ristretto
TEST_TARGET = test_ristretto
TEST_V2_TARGET = test_table_v2
TEST_COMPREHENSIVE_TARGET = test_comprehensive
TEST_ORIGINAL_TARGET = test_original_api
TEST_STRESS_TARGET = test_stress

SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
BUILD_DIR = build
BIN_DIR = bin

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%.o,$(TEST_SOURCES))

.PHONY: all clean debug test test-v2 test-comprehensive test-original test-stress test-all run benchmark

all: $(BUILD_DIR) $(BIN_DIR) $(BIN_DIR)/$(TARGET)

debug: CFLAGS += $(DEBUGFLAGS)
debug: clean $(BIN_DIR)/$(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

test: $(BIN_DIR)/$(TEST_TARGET)
	$(BIN_DIR)/$(TEST_TARGET)

test-v2: $(BIN_DIR)/$(TEST_V2_TARGET)
	$(BIN_DIR)/$(TEST_V2_TARGET)

test-comprehensive: $(BIN_DIR)/$(TEST_COMPREHENSIVE_TARGET)
	$(BIN_DIR)/$(TEST_COMPREHENSIVE_TARGET)

test-original: $(BIN_DIR)/$(TEST_ORIGINAL_TARGET)
	$(BIN_DIR)/$(TEST_ORIGINAL_TARGET)

test-stress: $(BIN_DIR)/$(TEST_STRESS_TARGET)
	$(BIN_DIR)/$(TEST_STRESS_TARGET)

test-all: test test-v2 test-comprehensive test-original test-stress
	@echo ""
	@echo "🎉 ALL TEST SUITES COMPLETED!"
	@echo "✅ Original API tests"
	@echo "✅ Table V2 basic tests" 
	@echo "✅ Comprehensive functionality tests"
	@echo "✅ Original SQL API tests"
	@echo "✅ Stress and performance tests"

$(BIN_DIR)/$(TEST_TARGET): $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS)) $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/$(TEST_V2_TARGET): $(BUILD_DIR)/table_v2.o $(BUILD_DIR)/test_table_v2.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/$(TEST_COMPREHENSIVE_TARGET): $(BUILD_DIR)/table_v2.o $(BUILD_DIR)/test_comprehensive.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/$(TEST_ORIGINAL_TARGET): $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS)) $(BUILD_DIR)/test_original_api.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_DIR)/$(TEST_STRESS_TARGET): $(BUILD_DIR)/table_v2.o $(BUILD_DIR)/test_stress.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(BIN_DIR)/$(TARGET)
	$(BIN_DIR)/$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

format:
	clang-format -i $(SRC_DIR)/*.c $(INCLUDE_DIR)/*.h $(TEST_DIR)/*.c

# Benchmark targets
benchmark:
	@echo "Building and running benchmarks..."
	$(MAKE) -C benchmark benchmarks
	$(MAKE) -C benchmark run-all

benchmark-build:
	$(MAKE) -C benchmark benchmarks

benchmark-vs-sqlite:
	$(MAKE) -C benchmark run-benchmark

benchmark-micro:
	$(MAKE) -C benchmark run-microbench

benchmark-speedtest:
	$(MAKE) -C benchmark run-speedtest

benchmark-ultra-fast:
	$(MAKE) -C benchmark run-ultra-fast

benchmark-clean:
	$(MAKE) -C benchmark clean

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make              - Build the ristretto executable"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make test         - Build and run basic tests"
	@echo "  make test-v2      - Build and run table v2 tests"
	@echo "  make test-comprehensive - Run comprehensive functionality tests"
	@echo "  make test-original - Run original SQL API tests"
	@echo "  make test-stress   - Run stress and performance tests"
	@echo "  make test-all      - Run ALL test suites"
	@echo "  make run          - Build and run ristretto"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make format       - Format source code"
	@echo ""
	@echo "Benchmark targets:"
	@echo "  make benchmark         - Build and run all benchmarks"
	@echo "  make benchmark-build   - Build benchmark executables"
	@echo "  make benchmark-vs-sqlite - Run SQLite comparison"
	@echo "  make benchmark-micro   - Run microbenchmarks"
	@echo "  make benchmark-speedtest - Run speedtest subset"
	@echo "  make benchmark-ultra-fast - Run ultra-fast write benchmark"
	@echo "  make benchmark-clean   - Clean benchmark artifacts"