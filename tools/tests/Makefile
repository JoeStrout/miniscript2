# Makefile for C# to C++ transpiler tests
# Usage:
#   make test-all-cs              - Run all tests in C#
#   make test-all-expected        - Run all tests in C++ (expected output)
#   make test-all-generated       - Run all tests in C++ (generated output)
#   make test TEST=struct VARIANT=cs           - Run specific test
#   make test TEST=struct VARIANT=exp          - Run specific test (expected C++)
#   make test TEST=struct VARIANT=gen          - Run specific test (generated C++)
#   make clean                    - Clean all build artifacts

TESTS = staticStringConst struct class
CXX = g++
CC = gcc
CXXFLAGS = -std=c++14 -Wall -Wextra -Wno-switch -I../../cpp/core
CFLAGS = -Wall -Wextra -I../../cpp/core
CSC = csc

# C++ support library files
CPP_CORE_DIR = ../../cpp/core
CPP_CORE_SOURCES = $(CPP_CORE_DIR)/CS_String.cpp $(CPP_CORE_DIR)/CS_Math.cpp $(CPP_CORE_DIR)/CS_List.cpp
CPP_CORE_OBJECTS = $(CPP_CORE_SOURCES:.cpp=.o)

# C support library files
C_CORE_SOURCES = $(CPP_CORE_DIR)/StringStorage.c $(CPP_CORE_DIR)/hashing.c $(CPP_CORE_DIR)/unicodeUtil.c
C_CORE_OBJECTS = $(C_CORE_SOURCES:.c=.o)

# All core objects
CORE_OBJECTS = $(CPP_CORE_OBJECTS) $(C_CORE_OBJECTS)

.PHONY: all test-all-cs test-all-expected test-all-generated clean cpp-core

# Run all tests in all variants
all: test-all-cs test-all-expected

# Build C++ core library object files
cpp-core: $(CORE_OBJECTS)

$(CPP_CORE_DIR)/%.o: $(CPP_CORE_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(CPP_CORE_DIR)/%.o: $(CPP_CORE_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run all tests in C#
test-all-cs:
	@echo "========================================="
	@echo "Running all tests in C#"
	@echo "========================================="
	@for test in $(TESTS); do \
		echo ""; \
		echo "--- Testing $$test (C#) ---"; \
		$(MAKE) --no-print-directory test-cs-$$test || exit 1; \
	done
	@echo ""
	@echo "All C# tests passed!"

# Run all tests with expected C++ output
test-all-expected:
	@echo "========================================="
	@echo "Running all tests with expected C++ output"
	@echo "========================================="
	@for test in $(TESTS); do \
		echo ""; \
		echo "--- Testing $$test (C++ expected) ---"; \
		$(MAKE) --no-print-directory test-cpp-expected-$$test || exit 1; \
	done
	@echo ""
	@echo "All expected C++ tests passed!"

# Run all tests with generated C++ output
test-all-generated:
	@echo "========================================="
	@echo "Running all tests with generated C++ output"
	@echo "========================================="
	@for test in $(TESTS); do \
		echo ""; \
		echo "--- Testing $$test (C++ generated) ---"; \
		$(MAKE) --no-print-directory test-cpp-generated-$$test || exit 1; \
	done
	@echo ""
	@echo "All generated C++ tests passed!"

# Individual test targets - C#
test-cs-staticStringConst:
	@cd staticStringConst && $(CSC) -out:test.exe main.cs Op.cs && mono test.exe

test-cs-struct:
	@cd struct && $(CSC) -out:test.exe main.cs Token.cs && mono test.exe

test-cs-class:
	@cd class && $(CSC) -out:test.exe main.cs AST.cs && mono test.exe

# Individual test targets - C++ expected
test-cpp-expected-staticStringConst: cpp-core
	$(CXX) $(CXXFLAGS) -o staticStringConst/test_expected \
		staticStringConst/main.cpp staticStringConst/expected/Op.g.cpp $(CORE_OBJECTS)
	@cd staticStringConst && ./test_expected

test-cpp-expected-struct: cpp-core
	$(CXX) $(CXXFLAGS) -o struct/test_expected \
		struct/main.cpp struct/expected/Token.g.cpp $(CORE_OBJECTS)
	@cd struct && ./test_expected

test-cpp-expected-class: cpp-core
	$(CXX) $(CXXFLAGS) -o class/test_expected \
		class/main.cpp class/expected/AST.g.cpp $(CORE_OBJECTS)
	@cd class && ./test_expected

# Individual test targets - C++ generated
test-cpp-generated-staticStringConst: cpp-core
	$(CXX) $(CXXFLAGS) -o staticStringConst/test_generated \
		staticStringConst/main.cpp staticStringConst/generated/Op.g.cpp $(CORE_OBJECTS)
	@cd staticStringConst && ./test_generated

test-cpp-generated-struct: cpp-core
	$(CXX) $(CXXFLAGS) -o struct/test_generated \
		struct/main.cpp struct/generated/Token.g.cpp $(CORE_OBJECTS)
	@cd struct && ./test_generated

test-cpp-generated-class: cpp-core
	$(CXX) $(CXXFLAGS) -o class/test_generated \
		class/main.cpp class/generated/AST.g.cpp $(CORE_OBJECTS)
	@cd class && ./test_generated

# Convenience target for running a specific test
# Supports synonyms: exp=cpp-expected, gen=cpp-generated
test:
ifeq ($(VARIANT),exp)
	@$(MAKE) --no-print-directory test-cpp-expected-$(TEST)
else ifeq ($(VARIANT),gen)
	@$(MAKE) --no-print-directory test-cpp-generated-$(TEST)
else
	@$(MAKE) --no-print-directory test-$(VARIANT)-$(TEST)
endif

# Clean all build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@for test in $(TESTS); do \
		cd $$test && rm -f test.exe test_expected test_generated && cd ..; \
	done
	@rm -f $(CORE_OBJECTS)
	@echo "Clean complete."
