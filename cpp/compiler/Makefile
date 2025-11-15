# Makefile for GPPG runtime library
CXX = g++
CXXFLAGS = -std=gnu++11 -Wall -I..
LDFLAGS =

# Core library dependencies
CORE_DIR = ../core
CORE_OBJS = $(CORE_DIR)/MemPool.o $(CORE_DIR)/StringPool.o $(CORE_DIR)/StringStorage.o

# Compiler library objects
OBJS = ShiftReduceParserCode.o

all: libgppg.a

libgppg.a: $(OBJS)
	ar rcs $@ $(OBJS)

ShiftReduceParserCode.o: ShiftReduceParserCode.cpp ShiftReduceParserCode.h
	$(CXX) $(CXXFLAGS) -c ShiftReduceParserCode.cpp -o $@

test: test_parser.cpp libgppg.a
	$(CXX) $(CXXFLAGS) test_parser.cpp libgppg.a $(CORE_OBJS) -o test_parser
	./test_parser

clean:
	rm -f $(OBJS) libgppg.a test_parser

.PHONY: all test clean
