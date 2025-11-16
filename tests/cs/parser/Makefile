# C# Parser Tests Makefile

# Paths
PROJECT_ROOT = ../../..
BUILD_DIR = $(PROJECT_ROOT)/build/tests/cs/parser

# Test executable
TEST_EXE = $(BUILD_DIR)/ParserTest.dll

all: $(TEST_EXE)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_EXE): ParserTest.cs ParserTest.csproj | $(BUILD_DIR)
	@echo "Building C# parser test..."
	dotnet build -o $(BUILD_DIR)

test: $(TEST_EXE)
	@echo "=== Running C# Parser Tests ==="
	dotnet $(TEST_EXE)

clean:
	rm -rf $(BUILD_DIR)
	rm -rf obj bin

.PHONY: all test clean
