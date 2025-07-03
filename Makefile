CC = clang
CFLAGS = -O3 -march=native -std=c11 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS = 
DEBUGFLAGS = -g -O0 -DDEBUG
TARGET = ristretto
TEST_TARGET = test_ristretto

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

.PHONY: all clean debug test run

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

$(BIN_DIR)/$(TEST_TARGET): $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS)) $(TEST_OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

run: $(BIN_DIR)/$(TARGET)
	$(BIN_DIR)/$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

format:
	clang-format -i $(SRC_DIR)/*.c $(INCLUDE_DIR)/*.h $(TEST_DIR)/*.c

.PHONY: help
help:
	@echo "Available targets:"
	@echo "  make         - Build the ristretto executable"
	@echo "  make debug   - Build with debug symbols"
	@echo "  make test    - Build and run tests"
	@echo "  make run     - Build and run ristretto"
	@echo "  make clean   - Remove build artifacts"
	@echo "  make format  - Format source code"