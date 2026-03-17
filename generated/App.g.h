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

	// Compile MiniScript source code to a list of functions.
	// Set verbose=true for extra debug output (assembly listing, etc.)
	private: static List<FuncDef> CompileSource(String source, ErrorPool errors, Boolean verbose=false);

	private: static List<FuncDef> CompileSourceFile(String filePath, ErrorPool errors);

	// Assemble an assembly file (.msa) to a list of functions
	private: static List<FuncDef> AssembleFile(String filePath);

	// Run integration tests from a test suite file
	public: static bool RunIntegrationTests(String filePath);

	// Run a single integration test
	private: static bool RunSingleTest(List<String> inputLines, List<String> expectedLines, Int32 lineNum);

	// Run a program given its list of functions
	private: static void RunProgram(List<FuncDef> functions, ErrorPool errors);

}; // end of struct App

// INLINE METHODS

} // end of namespace MiniScript
