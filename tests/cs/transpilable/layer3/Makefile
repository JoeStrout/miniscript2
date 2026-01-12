# Layer 3 Test Makefile
# Tests for processing layer (depends on layers 0-2)

# Paths
CS_DIR = ../../../../cs
BUILD_DIR = ../../../../build/tests/cs/layer3
TEST_FRAMEWORK = ../TestFramework.cs

# Source files
SOURCES = $(TEST_FRAMEWORK) \
          $(CS_DIR)/IOHelper.cs \
          $(CS_DIR)/Bytecode.cs \
          $(CS_DIR)/Value.cs \
          $(CS_DIR)/StringUtils.cs \
          $(CS_DIR)/MemPoolShim.cs \
          $(CS_DIR)/ValueFuncRef.cs \
          $(CS_DIR)/VarMap.cs \
          $(CS_DIR)/FuncDef.cs \
          $(CS_DIR)/Disassembler.cs \
          $(CS_DIR)/Assembler.cs \
          DisassemblerTest.cs \
          AssemblerTest.cs \
          TestRunner.cs

# Output
OUTPUT = $(BUILD_DIR)/Layer3Tests.exe

.PHONY: all test clean

all: $(OUTPUT)

$(OUTPUT): $(SOURCES)
	@mkdir -p $(BUILD_DIR)
	@echo "Building Layer 3 tests..."
	csc -out:$(OUTPUT) $(SOURCES)

test: $(OUTPUT)
	@echo ""
	@echo "Running Layer 3 tests..."
	mono $(OUTPUT) || exit 1

clean:
	@echo "Cleaning Layer 3 tests..."
	rm -rf $(BUILD_DIR)
