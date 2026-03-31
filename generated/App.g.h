// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: App.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"

namespace MiniScript {

// DECLARATIONS

struct App {
	public: static bool debugMode;
	public: static bool visMode;
	
	public: static void MainProgram(List<String> args);

	// Create an Interpreter with standard output wiring
	private: static Interpreter CreateInterpreter();

	// Assemble an assembly file (.msa) to a list of functions
	private: static List<FuncDef> AssembleFile(String filePath);

	// Run integration tests from a test suite file
	public: static bool RunIntegrationTests(String filePath);

	// Run a single integration test
	private: static bool RunSingleTest(List<String> inputLines, List<String> expectedLines, Int32 lineNum);

	// Run an Interpreter that has already been compiled or loaded with functions.
	private: static void RunInterpreter(Interpreter interp);

	private: static void RunREPL();

}; // end of struct App

// INLINE METHODS

} // end of namespace MiniScript
