# C# Tests Makefile
# Orchestrates all C# test categories

.PHONY: all parser transpilable clean help

# Default: run all C# tests
all: transpilable parser

# Run transpilable tests (layer-based, stops on first failure)
transpilable:
	@echo ""
	@echo "========================================="
	@echo "  Transpilable C# Tests"
	@echo "========================================="
	$(MAKE) -C transpilable all

# Run parser tests
parser:
	@echo ""
	@echo "=== C# Parser Tests ==="
	$(MAKE) -C parser test

# Clean all C# test artifacts
clean:
	@echo "Cleaning C# test artifacts..."
	$(MAKE) -C transpilable clean
	$(MAKE) -C parser clean

# Show help
help:
	@echo "C# Test Suite"
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Run all C# tests (transpilable + parser)"
	@echo "  transpilable - Run transpilable tests only (layer-based)"
	@echo "  parser       - Run parser tests only"
	@echo "  clean        - Clean all test build artifacts"
	@echo "  help         - Show this help message"
