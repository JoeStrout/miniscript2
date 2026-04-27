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

	// Return just the filename portion of a path (e.g. "/foo/bar.ms" -> "bar.ms").
	private: static String GetPathFilename(String filePath);

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

	// Get one line of REPL input from the user.  Return it as a String,
	// or return null if we reach end-of-input (e.g. control-D).
	private: static String GetREPLInput(Interpreter interp);

	private: static void RunREPL();

}; // end of struct App

// INLINE METHODS

} // end of namespace MiniScript
