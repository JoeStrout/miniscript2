# MS2Proto3 C++ Makefile
# Builds the transpiled C++ version

CXX = g++
CC = gcc

# Handle computed-goto mode override
ifeq ($(GOTO_MODE),on)
    GOTO_FLAG = -DVM_USE_COMPUTED_GOTO=1
else ifeq ($(GOTO_MODE),off)
    GOTO_FLAG = -DVM_USE_COMPUTED_GOTO=0
else
    GOTO_FLAG = 
endif

CXXFLAGS = -std=gnu++11 -Wall -Wextra -O3 -DNDEBUG -Icore $(GOTO_FLAG)
CFLAGS = -std=c99 -Wall -Wextra -O3 -DNDEBUG -Icore $(GOTO_FLAG)
SRCDIR = core
GENDIR = ../generated
BUILDDIR = ../build/cpp
OBJDIR = $(BUILDDIR)/obj

# Core C++ and C files (excluding test programs)
CORE_CPP_SOURCES = $(filter-out $(SRCDIR)/test_%.cpp $(SRCDIR)/debug_%.cpp, $(wildcard $(SRCDIR)/*.cpp))
CORE_C_SOURCES = $(wildcard $(SRCDIR)/*.c)
CORE_CPP_OBJECTS = $(CORE_CPP_SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/core_%.o)
CORE_C_OBJECTS = $(CORE_C_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/core_%.o)
CORE_OBJECTS = $(CORE_CPP_OBJECTS) $(CORE_C_OBJECTS)

# Generated C++ files (will be populated by transpiler)
GEN_SOURCES = $(wildcard $(GENDIR)/*.g.cpp)
GEN_OBJECTS = $(GEN_SOURCES:$(GENDIR)/%.g.cpp=$(OBJDIR)/gen_%.o)

# All objects
OBJECTS = $(CORE_OBJECTS) $(GEN_OBJECTS)

# Target executable
TARGET = $(BUILDDIR)/MS2Proto3

# Test programs
TEST_STRING_POOL = $(BUILDDIR)/test_string_pool_debug
TEST_SIMPLE = $(BUILDDIR)/test_simple
TEST_DEBUG_BASIC = $(BUILDDIR)/test_debug_basic
TEST_TRIVIAL = $(BUILDDIR)/test_trivial
DEBUG_TRIVIAL = $(BUILDDIR)/debug_trivial

.PHONY: all clean test_string_pool test_simple test_debug_basic test_trivial debug_trivial

all: $(TARGET)

test_string_pool: $(TEST_STRING_POOL)
test_simple: $(TEST_SIMPLE)
test_debug_basic: $(TEST_DEBUG_BASIC)
test_trivial: $(TEST_TRIVIAL)
debug_trivial: $(DEBUG_TRIVIAL)

$(TARGET): $(OBJECTS) | $(BUILDDIR)
	$(CXX) $(OBJECTS) -o $@

# Test program - only needs core objects, not generated ones
$(TEST_STRING_POOL): $(CORE_OBJECTS) $(OBJDIR)/core_test_string_pool_debug.o | $(BUILDDIR)
	$(CXX) $(CORE_OBJECTS) $(OBJDIR)/core_test_string_pool_debug.o -o $@

# Simple test program
$(TEST_SIMPLE): $(CORE_OBJECTS) $(OBJDIR)/core_test_simple.o | $(BUILDDIR)
	$(CXX) $(CORE_OBJECTS) $(OBJDIR)/core_test_simple.o -o $@

# Basic debug test program
$(TEST_DEBUG_BASIC): $(CORE_OBJECTS) $(OBJDIR)/core_test_debug_basic.o | $(BUILDDIR)
	$(CXX) $(CORE_OBJECTS) $(OBJDIR)/core_test_debug_basic.o -o $@

# Trivial test program
$(TEST_TRIVIAL): $(CORE_OBJECTS) $(OBJDIR)/core_test_trivial.o | $(BUILDDIR)
	$(CXX) $(CORE_OBJECTS) $(OBJDIR)/core_test_trivial.o -o $@

# Debug trivial test program
$(DEBUG_TRIVIAL): $(CORE_OBJECTS) $(OBJDIR)/core_debug_trivial.o | $(BUILDDIR)
	$(CXX) $(CORE_OBJECTS) $(OBJDIR)/core_debug_trivial.o -o $@

# Core C++ object files
$(OBJDIR)/core_%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Core C object files
$(OBJDIR)/core_%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Generated C++ object files  
$(OBJDIR)/gen_%.o: $(GENDIR)/%.g.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create directories
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(BUILDDIR)

# Show build info
info:
	@echo "Core sources: $(CORE_SOURCES)"
	@echo "Generated sources: $(GEN_SOURCES)"
	@echo "Objects: $(OBJECTS)"
	@echo "Target: $(TARGET)"