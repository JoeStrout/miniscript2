# Master Makefile for Transpilable C# Tests
# Runs tests in dependency order, stopping on first failure

.PHONY: all layer0 clean help

# Default: run all layers in sequence, stop on first failure
all: layer0
	@echo ""
	@echo "=========================================="
	@echo "  ALL TRANSPILABLE TESTS PASSED"
	@echo "=========================================="
	@echo ""

# Layer 0 - Foundation (no dependencies)
layer0:
	@echo ""
	@echo "=========================================="
	@echo "  LAYER 0: Foundation"
	@echo "=========================================="
	$(MAKE) -C layer0 test || (echo "Layer 0 FAILED - stopping" && exit 1)

# Clean all test artifacts
clean:
	@echo "Cleaning all transpilable test artifacts..."
	$(MAKE) -C layer0 clean
	rm -rf ../../../build/tests/cs

# Show help
help:
	@echo "Transpilable C# Test Suite"
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all    - Run all tests in dependency order (stops on first failure)"
	@echo "  layer0 - Run Layer 0 tests (Foundation)"
	@echo "  clean  - Clean all test build artifacts"
	@echo "  help   - Show this help message"
