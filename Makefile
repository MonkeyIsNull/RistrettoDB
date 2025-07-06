CC = clang
CFLAGS = -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic -Iinclude -Iembed -I.
LDFLAGS = 
DEBUGFLAGS = -g -O0 -DDEBUG
TARGET = ristretto
TEST_TARGET = test_basic
TEST_V2_TARGET = test_table_v2
TEST_COMPREHENSIVE_TARGET = test_comprehensive
TEST_ORIGINAL_TARGET = test_original_api
TEST_STRESS_TARGET = test_stress

# Library targets
STATIC_LIB = libristretto.a
DYNAMIC_LIB = libristretto.so
LIBRARY_VERSION = 2.0.0

SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
BUILD_DIR = build
BIN_DIR = bin
LIB_DIR = lib

# Library source files (exclude CLI)
LIB_SOURCES = $(filter-out $(SRC_DIR)/main.c $(SRC_DIR)/ristretto_cli.c, $(wildcard $(SRC_DIR)/*.c))
LIB_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LIB_SOURCES))

# CLI source files
CLI_SOURCES = $(SRC_DIR)/ristretto_cli.c
CLI_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(CLI_SOURCES))

# All source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%.o,$(TEST_SOURCES))

.PHONY: all clean debug test test-v2 test-comprehensive test-original test-stress test-all run benchmark
.PHONY: libraries static dynamic install uninstall example

# Default target builds both CLI and libraries
all: $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR) $(BIN_DIR)/$(TARGET) libraries

# Library targets  
libraries: $(LIB_DIR)/$(STATIC_LIB) $(LIB_DIR)/$(DYNAMIC_LIB)

static: $(LIB_DIR)/$(STATIC_LIB)

dynamic: $(LIB_DIR)/$(DYNAMIC_LIB)

debug: CFLAGS += $(DEBUGFLAGS)
debug: clean all

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(LIB_DIR):
	mkdir -p $(LIB_DIR)

# Build CLI executable (links against static library)
$(BIN_DIR)/$(TARGET): $(LIB_DIR)/$(STATIC_LIB) $(CLI_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(CLI_OBJECTS) -L$(LIB_DIR) -lristretto $(LDFLAGS)

# Build static library
$(LIB_DIR)/$(STATIC_LIB): $(LIB_OBJECTS) | $(LIB_DIR)
	ar rcs $@ $^
	ranlib $@

# Build dynamic library
$(LIB_DIR)/$(DYNAMIC_LIB): $(LIB_OBJECTS) | $(LIB_DIR)
	$(CC) -shared -fPIC -o $@ $^ $(LDFLAGS)

# Compile library object files with position-independent code
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

$(BUILD_DIR)/test_%.o: $(TEST_DIR)/test_%.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Test targets
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
	@echo "ALL TEST SUITES COMPLETED!"
	@echo "Original API tests"
	@echo "Table V2 basic tests" 
	@echo "Comprehensive functionality tests"
	@echo "Original SQL API tests"
	@echo "Stress and performance tests"

# Test executables (link against static library)
$(BIN_DIR)/$(TEST_TARGET): $(LIB_DIR)/$(STATIC_LIB) $(BUILD_DIR)/test_basic.o
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/test_basic.o -L$(LIB_DIR) -lristretto $(LDFLAGS)

$(BIN_DIR)/$(TEST_V2_TARGET): $(LIB_DIR)/$(STATIC_LIB) $(BUILD_DIR)/test_table_v2.o
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/test_table_v2.o -L$(LIB_DIR) -lristretto $(LDFLAGS)

$(BIN_DIR)/$(TEST_COMPREHENSIVE_TARGET): $(LIB_DIR)/$(STATIC_LIB) $(BUILD_DIR)/test_comprehensive.o
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/test_comprehensive.o -L$(LIB_DIR) -lristretto $(LDFLAGS)

$(BIN_DIR)/$(TEST_ORIGINAL_TARGET): $(LIB_DIR)/$(STATIC_LIB) $(BUILD_DIR)/test_original_api.o
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/test_original_api.o -L$(LIB_DIR) -lristretto $(LDFLAGS)

$(BIN_DIR)/$(TEST_STRESS_TARGET): $(LIB_DIR)/$(STATIC_LIB) $(BUILD_DIR)/test_stress.o
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/test_stress.o -L$(LIB_DIR) -lristretto $(LDFLAGS)

run: $(BIN_DIR)/$(TARGET)
	$(BIN_DIR)/$(TARGET)

# Example program showing how to embed RistrettoDB
example: $(LIB_DIR)/$(STATIC_LIB)
	@if [ ! -f example.c ]; then \
		echo "Creating example.c..."; \
		echo '#include "ristretto.h"' > example.c; \
		echo '#include <stdio.h>' >> example.c; \
		echo '' >> example.c; \
		echo 'int main() {' >> example.c; \
		echo '    printf("RistrettoDB Version: %s\\n", ristretto_version());' >> example.c; \
		echo '    RistrettoDB* db = ristretto_open("example.db");' >> example.c; \
		echo '    if (db) {' >> example.c; \
		echo '        printf("Database opened successfully!\\n");' >> example.c; \
		echo '        ristretto_close(db);' >> example.c; \
		echo '    }' >> example.c; \
		echo '    return 0;' >> example.c; \
		echo '}' >> example.c; \
	fi
	$(CC) $(CFLAGS) -o example example.c -L$(LIB_DIR) -lristretto
	./example

# Installation targets
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include

install: all
	mkdir -p $(BINDIR) $(LIBDIR) $(INCLUDEDIR)
	cp $(BIN_DIR)/$(TARGET) $(BINDIR)/
	cp $(LIB_DIR)/$(STATIC_LIB) $(LIBDIR)/
	cp $(LIB_DIR)/$(DYNAMIC_LIB) $(LIBDIR)/
	cp embed/ristretto.h $(INCLUDEDIR)/
	ldconfig || true

uninstall:
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(LIBDIR)/$(STATIC_LIB)
	rm -f $(LIBDIR)/$(DYNAMIC_LIB)*
	rm -f $(INCLUDEDIR)/ristretto.h

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)
	rm -f example example.c

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
	@echo "RistrettoDB Build System"
	@echo "========================"
	@echo ""
	@echo "Main targets:"
	@echo "  make              - Build CLI and libraries"
	@echo "  make libraries    - Build both static and dynamic libraries"
	@echo "  make static       - Build static library (libristretto.a)"
	@echo "  make dynamic      - Build dynamic library (libristretto.so)"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make example      - Build and run embedding example"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make format       - Format source code"
	@echo ""
	@echo "Testing targets:"
	@echo "  make test         - Build and run basic tests"
	@echo "  make test-v2      - Build and run table v2 tests"
	@echo "  make test-comprehensive - Run comprehensive functionality tests"
	@echo "  make test-original - Run original SQL API tests"
	@echo "  make test-stress   - Run stress and performance tests"
	@echo "  make test-all      - Run ALL test suites"
	@echo ""
	@echo "Installation:"
	@echo "  make install      - Install to $(PREFIX) (default: /usr/local)"
	@echo "  make uninstall    - Remove installation"
	@echo ""
	@echo "Benchmark targets:"
	@echo "  make benchmark         - Build and run all benchmarks"
	@echo "  make benchmark-build   - Build benchmark executables"
	@echo "  make benchmark-vs-sqlite - Run SQLite comparison"
	@echo "  make benchmark-micro   - Run microbenchmarks"
	@echo "  make benchmark-speedtest - Run speedtest subset"
	@echo "  make benchmark-ultra-fast - Run ultra-fast write benchmark"
	@echo "  make benchmark-clean   - Clean benchmark artifacts"
	@echo ""
	@echo "Files created:"
	@echo "  bin/ristretto     - CLI executable"
	@echo "  lib/libristretto.a - Static library"
	@echo "  lib/libristretto.so - Dynamic library"
	@echo "  ristretto.h       - Public header for embedding"