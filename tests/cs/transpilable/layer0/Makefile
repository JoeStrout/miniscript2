# Layer 0 Test Makefile
# Tests for foundation modules (no dependencies)

# Paths
CS_DIR = ../../../../cs
BUILD_DIR = ../../../../build/tests/cs/layer0
TEST_FRAMEWORK = ../TestFramework.cs

# Source files
SOURCES = $(TEST_FRAMEWORK) \
          $(CS_DIR)/IOHelper.cs \
          $(CS_DIR)/Bytecode.cs \
          IOHelperTest.cs \
          BytecodeTest.cs \
          TestRunner.cs

# Output
OUTPUT = $(BUILD_DIR)/Layer0Tests.exe

.PHONY: all test clean

all: $(OUTPUT)

$(OUTPUT): $(SOURCES)
	@mkdir -p $(BUILD_DIR)
	@echo "Building Layer 0 tests..."
	csc -out:$(OUTPUT) $(SOURCES)

test: $(OUTPUT)
	@echo ""
	@echo "Running Layer 0 tests..."
	mono $(OUTPUT)

clean:
	@echo "Cleaning Layer 0 tests..."
	rm -rf $(BUILD_DIR)
