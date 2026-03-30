// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: UnitTests.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// This module gathers together all the unit tests for this prototype.
// Each test returns true on success, false on failure.

namespace MiniScript {

// DECLARATIONS

class UnitTests {

	public: static Boolean Assert(bool condition, String message);
	
	public: static Boolean AssertEqual(String actual, String expected);
		
	public: static Boolean AssertEqual(UInt32 actual, UInt32 expected);
		
	public: static Boolean AssertEqual(Int32 actual, Int32 expected);
		
	public: static Boolean AssertEqual(List<String> actual, List<String> expected);
		
	public: static Boolean TestStringUtils();
	
	public: static Boolean TestDisassembler();
	
	public: static Boolean TestAssembler();

	public: static Boolean TestValueMap();

	// Helper for parser tests: parse, simplify, and check result
	private: static Boolean CheckParse(Parser parser, String input, String expected);

	public: static Boolean TestParser();

	// Helper for code generator tests: parse, generate, and check assembly output
	private: static Boolean CheckCodeGen(Parser parser, String input, List<String> expectedLines);

	// Helper to check bytecode generation produces valid FuncDef
	private: static Boolean CheckBytecodeGen(Parser parser, String input, Int32 expectedInstructions, Int32 expectedConstants);

	public: static Boolean TestCodeGenerator();

	public: static Boolean TestEmitPatternValidation();

	public: static Boolean TestLexer();

	// Helper: run a sequence of REPL inputs and collect all printed output.
	private: static List<String> RunREPLSequence(List<String> inputs);

	public: static Boolean TestParserNeedMoreInput();

	public: static Boolean TestREPL();

	public: static Boolean RunAll();
}; // end of struct UnitTests

// INLINE METHODS

} // end of namespace MiniScript
