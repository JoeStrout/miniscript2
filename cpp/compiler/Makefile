# Makefile for GPPG runtime library and MiniScript parser
CXX = g++
CXXFLAGS = -std=gnu++11 -Wall -I..
LDFLAGS =

# Compiler library objects
RUNTIME_OBJS = ShiftReduceParserCode.o
PARSER_OBJS = MiniScriptParser.o MiniScriptLexer.o
OBJS = $(RUNTIME_OBJS) $(PARSER_OBJS)

all: $(OBJS)
	@echo "Compiler library objects built."

ShiftReduceParserCode.o: ShiftReduceParserCode.cpp ShiftReduceParserCode.h
	$(CXX) $(CXXFLAGS) -c ShiftReduceParserCode.cpp -o $@

MiniScriptParser.o: MiniScriptParser.cpp MiniScriptParser.h ShiftReduceParserCode.h
	$(CXX) $(CXXFLAGS) -c MiniScriptParser.cpp -o $@

MiniScriptLexer.o: MiniScriptLexer.cpp MiniScriptLexer.h MiniScriptParser.h ShiftReduceParserCode.h
	$(CXX) $(CXXFLAGS) -c MiniScriptLexer.cpp -o $@

clean:
	rm -f $(OBJS)

.PHONY: all clean
