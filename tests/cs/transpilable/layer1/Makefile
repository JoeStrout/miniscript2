# Layer 1 Test Makefile
# Tests for basic infrastructure (depends on layer 0)

# Paths
CS_DIR = ../../../../cs
BUILD_DIR = ../../../../build/tests/cs/layer1
TEST_FRAMEWORK = ../TestFramework.cs

# Source files
SOURCES = $(TEST_FRAMEWORK) \
          $(CS_DIR)/IOHelper.cs \
          $(CS_DIR)/Bytecode.cs \
          $(CS_DIR)/Value.cs \
          $(CS_DIR)/VarMap.cs \
          $(CS_DIR)/StringUtils.cs \
          $(CS_DIR)/MemPoolShim.cs \
          $(CS_DIR)/ValueFuncRef.cs \
          ValueTest.cs \
          TestRunner.cs

# Output
OUTPUT = $(BUILD_DIR)/Layer1Tests.exe

.PHONY: all test clean

all: $(OUTPUT)

$(OUTPUT): $(SOURCES)
	@mkdir -p $(BUILD_DIR)
	@echo "Building Layer 1 tests..."
	csc -out:$(OUTPUT) $(SOURCES)

test: $(OUTPUT)
	@echo ""
	@echo "Running Layer 1 tests..."
	mono $(OUTPUT)

clean:
	@echo "Cleaning Layer 1 tests..."
	rm -rf $(BUILD_DIR)
