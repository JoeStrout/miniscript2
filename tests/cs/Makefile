# C# Tests Makefile
# Orchestrates all C# test categories

.PHONY: all parser clean help

# Default: run all C# tests
all: parser

# Run parser tests
parser:
	@echo "=== C# Parser Tests ==="
	$(MAKE) -C parser test

# Clean all C# test artifacts
clean:
	@echo "Cleaning C# test artifacts..."
	$(MAKE) -C parser clean

# Show help
help:
	@echo "C# Test Suite"
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all    - Run all C# tests"
	@echo "  parser - Run parser tests only"
	@echo "  clean  - Clean all test build artifacts"
	@echo "  help   - Show this help message"
