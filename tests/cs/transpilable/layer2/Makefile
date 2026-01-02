# Layer 2 Test Makefile
# Tests for core data structures (depends on layers 0-1)

# Paths
CS_DIR = ../../../../cs
BUILD_DIR = ../../../../build/tests/cs/layer2
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
          FuncDefTest.cs \
          TestRunner.cs

# Output
OUTPUT = $(BUILD_DIR)/Layer2Tests.exe

.PHONY: all test clean

all: $(OUTPUT)

$(OUTPUT): $(SOURCES)
	@mkdir -p $(BUILD_DIR)
	@echo "Building Layer 2 tests..."
	csc -out:$(OUTPUT) $(SOURCES)

test: $(OUTPUT)
	@echo ""
	@echo "Running Layer 2 tests..."
	mono $(OUTPUT) || exit 1

clean:
	@echo "Cleaning Layer 2 tests..."
	rm -rf $(BUILD_DIR)
