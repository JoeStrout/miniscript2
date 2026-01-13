# Layer 4 Test Makefile
# Tests for VM layer (depends on layers 0-3)
# Note: Uses dotnet (not Mono) due to .NET 5+ API requirements in VM.cs

# Paths
BUILD_DIR = ../../../../build/tests/cs/layer4

# Output
OUTPUT = $(BUILD_DIR)/Layer4Tests.dll

.PHONY: all build test clean

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@echo "Building Layer 4 tests..."
	dotnet build -o $(BUILD_DIR)

test: build
	@echo ""
	@echo "Running Layer 4 tests..."
	dotnet run

clean:
	@echo "Cleaning Layer 4 tests..."
	dotnet clean
	rm -rf $(BUILD_DIR)
	rm -rf bin obj
