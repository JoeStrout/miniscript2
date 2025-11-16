# Makefile for GPPG runtime library and MiniScript parser
CXX = g++
CXXFLAGS = -std=gnu++11 -Wall -I. -I.. -I../core -I../../generated
LDFLAGS =

# Compiler library objects
RUNTIME_OBJS = ShiftReduceParserCode.o
PARSER_OBJS = MiniScriptParser.o MiniScriptLexer.o
OBJS = $(RUNTIME_OBJS) $(PARSER_OBJS)

all: $(OBJS)
	@echo "Compiler library objects built."

ShiftReduceParserCode.o: ShiftReduceParserCode.cpp ShiftReduceParserCode.h
	$(CXX) $(CXXFLAGS) -c ShiftReduceParserCode.cpp -o $@

MiniScriptParser.o: ../../generated/MiniScriptParser.g.cpp ../../generated/MiniScriptParser.g.h ShiftReduceParserCode.h
	$(CXX) $(CXXFLAGS) -c ../../generated/MiniScriptParser.g.cpp -o $@

MiniScriptLexer.o: MiniScriptLexer.cpp MiniScriptLexer.h ../../generated/MiniScriptParser.g.h ShiftReduceParserCode.h
	$(CXX) $(CXXFLAGS) -c MiniScriptLexer.cpp -o $@

clean:
	rm -f $(OBJS)

.PHONY: all clean
