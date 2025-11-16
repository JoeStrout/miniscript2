# Parser Tests Makefile
CXX = g++
CXXFLAGS = -std=gnu++11 -Wall -I../../.. -I../../../cpp/compiler -I../../../generated
LDFLAGS =

# Paths
PROJECT_ROOT = ../../..
COMPILER_DIR = $(PROJECT_ROOT)/cpp/compiler
GENERATED_DIR = $(PROJECT_ROOT)/generated
BUILD_DIR = $(PROJECT_ROOT)/build
CORE_OBJS_DIR = $(BUILD_DIR)/cpp/obj
TEST_BUILD_DIR = $(BUILD_DIR)/tests/cpp/parser

# Compiler library objects (built separately)
COMPILER_OBJS = $(COMPILER_DIR)/ShiftReduceParserCode.o \
                $(COMPILER_DIR)/MiniScriptParser.o \
                $(COMPILER_DIR)/MiniScriptLexer.o

# Core library objects (built separately)
CORE_OBJS = $(CORE_OBJS_DIR)/core_*.o

# Test executables
TEST_PARSER = $(TEST_BUILD_DIR)/test_parser

all: $(TEST_PARSER)

$(TEST_BUILD_DIR):
	mkdir -p $(TEST_BUILD_DIR)

$(TEST_PARSER): test_parser.cpp $(COMPILER_OBJS) | $(TEST_BUILD_DIR)
	@if [ ! -f $(COMPILER_DIR)/MiniScriptParser.o ]; then \
		echo "Error: Compiler objects not found. Run 'make' from cpp/compiler/ first."; \
		exit 1; \
	fi
	@if [ ! -f $(CORE_OBJS_DIR)/core_MemPool.o ]; then \
		echo "Error: Core objects not found. Run 'make' from cpp/ first."; \
		exit 1; \
	fi
	$(CXX) $(CXXFLAGS) test_parser.cpp $(COMPILER_OBJS) $(CORE_OBJS) -o $(TEST_PARSER)

test: $(TEST_PARSER)
	@echo "=== Running Parser Tests ==="
	$(TEST_PARSER)

clean:
	rm -rf $(TEST_BUILD_DIR)

.PHONY: all test clean
