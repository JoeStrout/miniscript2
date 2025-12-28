# Master Makefile for Transpilable C# Tests
# Runs tests in dependency order, stopping on first failure

.PHONY: all layer0 layer1 layer2 layer3 layer4 layer5 clean help

# Default: run all layers in sequence, stop on first failure
all: layer0 layer1 layer2
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

# Layer 1 - Basic Infrastructure (depends on layer 0)
layer1:
	@echo ""
	@echo "=========================================="
	@echo "  LAYER 1: Basic Infrastructure"
	@echo "=========================================="
	$(MAKE) -C layer1 test || (echo "Layer 1 FAILED - stopping" && exit 1)

# Layer 2 - Core Data Structures (depends on layers 0-1)
layer2:
	@echo ""
	@echo "=========================================="
	@echo "  LAYER 2: Core Data Structures"
	@echo "=========================================="
	$(MAKE) -C layer2 test || (echo "Layer 2 FAILED - stopping" && exit 1)

# Layer 3 - Processing Layer (depends on layers 0-2)
layer3:
	@echo ""
	@echo "=========================================="
	@echo "  LAYER 3: Processing Layer"
	@echo "=========================================="
	@echo "Layer 3 tests not yet implemented"
	# $(MAKE) -C layer3 test || (echo "Layer 3 FAILED - stopping" && exit 1)

# Layer 4 - Execution Engine (depends on layers 0-3)
layer4:
	@echo ""
	@echo "=========================================="
	@echo "  LAYER 4: Execution Engine"
	@echo "=========================================="
	@echo "Layer 4 tests not yet implemented"
	# $(MAKE) -C layer4 test || (echo "Layer 4 FAILED - stopping" && exit 1)

# Layer 5 - Top-level Components (depends on layers 0-4)
layer5:
	@echo ""
	@echo "=========================================="
	@echo "  LAYER 5: Top-level Components"
	@echo "=========================================="
	@echo "Layer 5 tests not yet implemented"
	# $(MAKE) -C layer5 test || (echo "Layer 5 FAILED - stopping" && exit 1)

# Clean all test artifacts
clean:
	@echo "Cleaning all transpilable test artifacts..."
	$(MAKE) -C layer0 clean
	$(MAKE) -C layer1 clean
	$(MAKE) -C layer2 clean
	# $(MAKE) -C layer3 clean
	# $(MAKE) -C layer4 clean
	# $(MAKE) -C layer5 clean
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
	@echo "  layer1 - Run Layer 1 tests (Basic Infrastructure)"
	@echo "  layer2 - Run Layer 2 tests (Core Data Structures)"
	@echo "  layer3 - Run Layer 3 tests (Processing Layer) [not yet implemented]"
	@echo "  layer4 - Run Layer 4 tests (Execution Engine) [not yet implemented]"
	@echo "  layer5 - Run Layer 5 tests (Top-level Components) [not yet implemented]"
	@echo "  clean  - Clean all test build artifacts"
	@echo "  help   - Show this help message"
	@echo ""
	@echo "Tests run in dependency order and stop on first failure."
