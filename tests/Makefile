# Master Test Makefile
# Run all tests for the MiniScript 2.0 project

.PHONY: all cpp cs vm unit integration clean help

# Default: run all tests
all: cpp cs

# Run all C++ tests
cpp:
	@echo "========================================="
	@echo "  Running C++ Tests"
	@echo "========================================="
	$(MAKE) -C cpp

# Run all C# tests
cs:
	@echo "========================================="
	@echo "  Running C# Tests"
	@echo "========================================="
	$(MAKE) -C cs

# Run specific test categories
vm:
	@echo "=== VM Tests Only ==="
	$(MAKE) -C cpp vm

unit:
	@echo "=== Unit Tests Only ==="
	$(MAKE) -C cpp unit

integration:
	@echo "=== Integration Tests Only ==="
	$(MAKE) -C cpp integration

# Clean all test build artifacts
clean:
	@echo "Cleaning all test artifacts..."
	$(MAKE) -C cpp clean
	$(MAKE) -C cs clean
	rm -rf ../build/tests

# Show help
help:
	@echo "MiniScript 2.0 Test Suite"
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Run all tests (C++ and C#)"
	@echo "  cpp         - Run all C++ tests"
	@echo "  cs          - Run all C# tests"
	@echo "  vm          - Run VM tests only"
	@echo "  unit        - Run unit tests only"
	@echo "  integration - Run integration tests only"
	@echo "  clean       - Clean all test build artifacts"
	@echo "  help        - Show this help message"
