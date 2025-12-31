# Layer 1 Transpiled Test Makefile
# Builds and runs C++ tests transpiled from C#

# Paths
CPP_CORE = ../../../../cpp/core
GENERATED = ../../../../generated
BUILD_DIR = ../../../../build/tests/cpp/layer1
CS_SOURCE_DIR = ../../../cs/transpilable/layer1
CS_PARENT_DIR = ../../../cs/transpilable
TRANSPILER = ../../../../tools/transpile.ms

# Transpiled test sources (generated .g.cpp files)
TRANSPILED_TEST_SOURCES = ../TestFramework.g.cpp \
                          ValueTest.g.cpp \
                          TestRunner.g.cpp

# Transpiled production sources (from generated/)
TRANSPILED_PROD_SOURCES = $(GENERATED)/IOHelper.g.cpp \
                          $(GENERATED)/Bytecode.g.cpp

# C++ core runtime sources
CPP_CORE_SOURCES = $(CPP_CORE)/value.c \
                   $(CPP_CORE)/value_string.c \
                   $(CPP_CORE)/value_list.c \
                   $(CPP_CORE)/value_map.c \
                   $(CPP_CORE)/StringStorage.c \
                   $(CPP_CORE)/CS_String.cpp \
                   $(CPP_CORE)/CS_List.cpp \
                   $(CPP_CORE)/CS_Math.cpp \
                   $(CPP_CORE)/CS_value_util.cpp

# All sources
SOURCES = $(TRANSPILED_TEST_SOURCES) $(TRANSPILED_PROD_SOURCES) $(CPP_CORE_SOURCES)

# Output executable
OUTPUT = $(BUILD_DIR)/Layer1Tests

# Compiler settings
CXX = g++
CC = gcc
CXXFLAGS = -std=gnu++11 -I$(CPP_CORE) -I$(GENERATED) -I. -I.. -Wall -DGC_AGGRESSIVE
CFLAGS = -I$(CPP_CORE) -I$(GENERATED) -I. -I.. -Wall -DGC_AGGRESSIVE
LDFLAGS =

# Object files
CPP_OBJECTS = $(BUILD_DIR)/main.o \
              $(BUILD_DIR)/TestFramework.o \
              $(BUILD_DIR)/ValueTest.o \
              $(BUILD_DIR)/TestRunner.o \
              $(BUILD_DIR)/IOHelper.o \
              $(BUILD_DIR)/Bytecode.o \
              $(BUILD_DIR)/CS_String.o \
              $(BUILD_DIR)/CS_List.o \
              $(BUILD_DIR)/CS_Math.o \
              $(BUILD_DIR)/CS_value_util.o

C_OBJECTS = $(BUILD_DIR)/value.o \
            $(BUILD_DIR)/value_string.o \
            $(BUILD_DIR)/value_list.o \
            $(BUILD_DIR)/value_map.o \
            $(BUILD_DIR)/StringStorage.o \
            $(BUILD_DIR)/unicodeUtil.o \
            $(BUILD_DIR)/gc.o \
            $(BUILD_DIR)/hashing.o

ALL_OBJECTS = $(CPP_OBJECTS) $(C_OBJECTS)

.PHONY: all transpile build test clean

# Default: transpile, build, and test
all: test

# Transpile C# test files to C++
transpile:
	@echo "Transpiling layer1 tests..."
	@echo "  Transpiling TestFramework (shared across all layers)..."
	cd $(CS_PARENT_DIR) && miniscript ../../../tools/transpile.ms . -o ../../cpp/transpiled
	@echo "  Transpiling layer1 tests..."
	cd $(CS_PARENT_DIR) && miniscript ../../../tools/transpile.ms layer1 -o ../../cpp/transpiled/layer1

# Build the C++ executable
build: $(OUTPUT)

$(OUTPUT): $(ALL_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	@echo "Linking Layer 1 C++ tests..."
	$(CXX) -o $(OUTPUT) $(ALL_OBJECTS) $(LDFLAGS)

# Compile C++ files
$(BUILD_DIR)/main.o: main.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/TestFramework.o: ../TestFramework.g.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/ValueTest.o: ValueTest.g.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/TestRunner.o: TestRunner.g.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/IOHelper.o: $(GENERATED)/IOHelper.g.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/Bytecode.o: $(GENERATED)/Bytecode.g.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/CS_String.o: $(CPP_CORE)/CS_String.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/CS_List.o: $(CPP_CORE)/CS_List.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/CS_Math.o: $(CPP_CORE)/CS_Math.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR)/CS_value_util.o: $(CPP_CORE)/CS_value_util.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Compile C files
$(BUILD_DIR)/value.o: $(CPP_CORE)/value.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/value_string.o: $(CPP_CORE)/value_string.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/value_list.o: $(CPP_CORE)/value_list.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/value_map.o: $(CPP_CORE)/value_map.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/StringStorage.o: $(CPP_CORE)/StringStorage.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/unicodeUtil.o: $(CPP_CORE)/unicodeUtil.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/gc.o: $(CPP_CORE)/gc.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/hashing.o: $(CPP_CORE)/hashing.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# Run the tests
test:
	@$(MAKE) build
	@echo ""
	@echo "Running Layer 1 C++ tests..."
	$(OUTPUT)

# Clean build artifacts
clean:
	@echo "Cleaning Layer 1 C++ tests..."
	rm -rf $(BUILD_DIR)
	rm -f *.g.cpp *.g.h
