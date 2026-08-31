# Master Test Makefile
# Run all tests for the MiniScript 2.0 project
#
# Note: the bulk of the real coverage is NOT here -- it is in the interpreter
# binary itself, as `miniscript2 --test`: UnitTests.RunAll plus the ~800-case
# integration suite in testSuite.txt.  What lives under tests/ is the layered
# C# unit tests, which only layer 0 still has.

.PHONY: all cs clean help

# Default: run all tests
all: cs

# Run all C# tests
cs:
	@echo "========================================="
	@echo "  Running C# Tests"
	@echo "========================================="
	$(MAKE) -C cs

# Clean all test build artifacts
clean:
	@echo "Cleaning all test artifacts..."
	$(MAKE) -C cs clean
	rm -rf ../build/tests

# Show help
help:
	@echo "MiniScript 2.0 Test Suite"
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Run all tests"
	@echo "  cs          - Run all C# tests"
	@echo "  clean       - Clean all test build artifacts"
	@echo "  help        - Show this help message"
