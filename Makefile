CC = clang
# Portable baseline by default (no -march=native, which produces binaries that
# SIGILL on a different microarchitecture). The SIMD paths in src/simd.c use
# fixed 128-bit clang vector types (NEON on arm64, SSE2 on x86-64) that compile
# on the baseline without -march=native.
#
# Opt in to host-tuned codegen with:  make SIMD=native
SIMD ?= portable
# _DEFAULT_SOURCE exposes the POSIX/BSD functions the storage engine uses
# (ftruncate, fsync, flock, mmap/msync). Under a strict -std=c11 on glibc
# (Linux) these are hidden without a feature-test macro, which breaks the
# build; the macro is a harmless no-op on macOS/BSD.
CFLAGS = -O3 -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Wpedantic -Iinclude -Iembed -I.
ifeq ($(SIMD),native)
CFLAGS += -march=native
endif
LDFLAGS =
DEBUGFLAGS = -g -O0 -DDEBUG
TEST_V2_TARGET = test_table_v2
TEST_COMPREHENSIVE_TARGET = test_comprehensive
TEST_STRESS_TARGET = test_stress
TEST_GOLDEN_TARGET = test_golden_format

# Library targets
STATIC_LIB = libristretto.a
DYNAMIC_LIB = libristretto.so
LIBRARY_VERSION = 0.3.0

SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
BUILD_DIR = build
BIN_DIR = bin
LIB_DIR = lib

# Library source files. After the V2-only pivot src/ holds exactly the engine
# (table_v2.c), the public wrappers (ristretto_api.c) and version.c.
LIB_SOURCES = $(wildcard $(SRC_DIR)/*.c)
LIB_OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LIB_SOURCES))

# All source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
HEADERS = $(wildcard $(INCLUDE_DIR)/*.h)

TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%.o,$(TEST_SOURCES))

.PHONY: all clean debug test-v2 test-comprehensive test-stress test-golden test-all benchmark
.PHONY: libraries static dynamic install uninstall example

# Default target builds the libraries (static + dynamic). There is no CLI in
# the V2-only build.
all: $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR) libraries

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
test-v2: $(BIN_DIR)/$(TEST_V2_TARGET)
	$(BIN_DIR)/$(TEST_V2_TARGET)

test-comprehensive: $(BIN_DIR)/$(TEST_COMPREHENSIVE_TARGET)
	$(BIN_DIR)/$(TEST_COMPREHENSIVE_TARGET)

test-stress: $(BIN_DIR)/$(TEST_STRESS_TARGET)
	$(BIN_DIR)/$(TEST_STRESS_TARGET)

test-golden: $(BIN_DIR)/$(TEST_GOLDEN_TARGET)
	$(BIN_DIR)/$(TEST_GOLDEN_TARGET)

test-all: test-v2 test-comprehensive test-stress test-golden
	@echo ""
	@echo "ALL TEST SUITES COMPLETED!"
	@echo "Table V2 basic tests"
	@echo "Comprehensive functionality tests"
	@echo "Stress and performance tests"
	@echo "Golden on-disk format round-trip test"

# Test executables (link against static library)
$(BIN_DIR)/$(TEST_V2_TARGET): $(LIB_DIR)/$(STATIC_LIB) $(BUILD_DIR)/test_table_v2.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/test_table_v2.o $(LIB_DIR)/$(STATIC_LIB) $(LDFLAGS)

$(BIN_DIR)/$(TEST_COMPREHENSIVE_TARGET): $(LIB_DIR)/$(STATIC_LIB) $(BUILD_DIR)/test_comprehensive.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/test_comprehensive.o $(LIB_DIR)/$(STATIC_LIB) $(LDFLAGS)

$(BIN_DIR)/$(TEST_STRESS_TARGET): $(LIB_DIR)/$(STATIC_LIB) $(BUILD_DIR)/test_stress.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/test_stress.o $(LIB_DIR)/$(STATIC_LIB) $(LDFLAGS)

$(BIN_DIR)/$(TEST_GOLDEN_TARGET): $(LIB_DIR)/$(STATIC_LIB) $(BUILD_DIR)/test_golden_format.o | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/test_golden_format.o $(LIB_DIR)/$(STATIC_LIB) $(LDFLAGS)

# Example program showing how to embed RistrettoDB (Table V2 API)
example: $(LIB_DIR)/$(STATIC_LIB)
	@if [ ! -f example.c ]; then \
		echo "Creating example.c..."; \
		echo '#include "ristretto.h"' > example.c; \
		echo '#include <stdio.h>' >> example.c; \
		echo '' >> example.c; \
		echo 'int main() {' >> example.c; \
		echo '    printf("RistrettoDB Version: %s\\n", ristretto_version());' >> example.c; \
		echo '    RistrettoTable* t = ristretto_table_create("example",' >> example.c; \
		echo '        "CREATE TABLE example (id INTEGER, name TEXT(32))");' >> example.c; \
		echo '    if (t) {' >> example.c; \
		echo '        printf("Table created successfully!\\n");' >> example.c; \
		echo '        ristretto_table_close(t);' >> example.c; \
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
	mkdir -p $(LIBDIR) $(INCLUDEDIR)
	cp $(LIB_DIR)/$(STATIC_LIB) $(LIBDIR)/
	cp $(LIB_DIR)/$(DYNAMIC_LIB) $(LIBDIR)/
	cp embed/ristretto.h $(INCLUDEDIR)/
	ldconfig || true

uninstall:
	rm -f $(LIBDIR)/$(STATIC_LIB)
	rm -f $(LIBDIR)/$(DYNAMIC_LIB)*
	rm -f $(INCLUDEDIR)/ristretto.h

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR)
	rm -f example example.c

format:
	clang-format -i $(SRC_DIR)/*.c $(INCLUDE_DIR)/*.h $(TEST_DIR)/*.c

# Benchmark targets. The V2-only tree ships a single write-throughput
# benchmark (benchmark/ultra_fast_benchmark.c). See benchmark/README.md for the
# pinned, reproducible configuration.
benchmark:
	@echo "Building and running the V2 write-throughput benchmark..."
	$(MAKE) -C benchmark run-ultra-fast

benchmark-build:
	$(MAKE) -C benchmark benchmarks

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
	@echo "  make              - Build static + dynamic libraries"
	@echo "  make libraries    - Build both static and dynamic libraries"
	@echo "  make static       - Build static library (libristretto.a)"
	@echo "  make dynamic      - Build dynamic library (libristretto.so)"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make example      - Build and run embedding example"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make format       - Format source code"
	@echo ""
	@echo "Testing targets:"
	@echo "  make test-v2      - Build and run Table V2 tests"
	@echo "  make test-comprehensive - Run comprehensive functionality tests"
	@echo "  make test-stress   - Run stress and performance tests"
	@echo "  make test-golden   - Run golden on-disk format round-trip test"
	@echo "  make test-all      - Run ALL test suites"
	@echo ""
	@echo "Installation:"
	@echo "  make install      - Install to $(PREFIX) (default: /usr/local)"
	@echo "  make uninstall    - Remove installation"
	@echo ""
	@echo "Benchmark targets:"
	@echo "  make benchmark         - Build and run the V2 write-throughput benchmark"
	@echo "  make benchmark-build   - Build benchmark executables"
	@echo "  make benchmark-ultra-fast - Run ultra-fast write benchmark"
	@echo "  make benchmark-clean   - Clean benchmark artifacts"
	@echo ""
	@echo "Files created:"
	@echo "  lib/libristretto.a - Static library"
	@echo "  lib/libristretto.so - Dynamic library"
	@echo "  ristretto.h       - Public header for embedding"