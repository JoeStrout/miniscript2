# Master Test Makefile
# Run all tests for the MiniScript 2.0 project
#
# Note: the bulk of the real coverage is NOT here -- it is in the interpreter
# binary itself, as `miniscript2 --test`: UnitTests.RunAll plus the ~800-case
# integration suite in testSuite.txt.  What lives under tests/ is the layered
# C# unit tests, which only layer 0 still has.

.PHONY: all cs scripts clean help

# Default: run all tests
all: cs scripts

# Self-checking MiniScript scripts.  Each exits nonzero if any check fails, so
# make stops on the first one that does.  Prefers the C++ build, falling back
# to the C# one; skips with a note if neither has been built.
MS2 := $(firstword $(wildcard ../build/cpp/miniscript2 ../build/cs/miniscript2))

scripts:
	@echo "========================================="
	@echo "  Running MiniScript test scripts"
	@echo "========================================="
ifeq ($(MS2),)
	@echo "No interpreter built; run tools/build.sh first.  Skipping."
else
	$(MS2) setters.ms
endif

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
	@echo "  scripts     - Run the self-checking MiniScript test scripts"
	@echo "  clean       - Clean all test build artifacts"
	@echo "  help        - Show this help message"
