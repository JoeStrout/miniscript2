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
	public: static bool quietMode;
	public: static bool testMode;
	public: static bool interactiveMode;

	public: static void MainProgram(List<String> args);

	// Print the startup banner shown on entering the REPL.
	private: static void PrintBanner();

	// Print usage/help text to standard output.
	private: static void PrintUsage(String progName);

	// Print version information to standard output.
	private: static void PrintVersion();

	// Report a command-line usage error on stderr and exit with status 2.
	private: static void UsageError(String progName, String message);

	// Exit the process with the code the `exit` intrinsic recorded on the VM.
	private: static void DoExit(Int32 exitCode);

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

	// Get one line of REPL input.  Builds the history-aware prompt, handles !
	// metacommands, and sets `line` to the line to hand to the interpreter.
	// Returns false on EOF.  (We can't signal EOF with a null line, because on
	// the C++ side an empty line -- which is perfectly valid -- is also null.)
	private: static Boolean GetREPLInput(Interpreter interp, String* line);

	// Parse a non-negative integer from a string.  Returns -1 on failure.
	private: static Int32 ParseInt(String s);

	// Display REPL input history entries matching an optional count and search term.
	// metaRest is everything after "!?" with leading whitespace stripped.
	private: static void HandleHistorySearch(String metaRest);

	// Recall a history entry by index string ("5", "-2", etc.).
	// Returns the source string, or null if the index is out of range.
	private: static String RecallInput(String indexStr);

	// Run the interactive REPL.  Pass an Interpreter that has already run a
	// script (for -i) to continue in that script's namespace, or null to start
	// fresh.
	private: static void RunREPL(Interpreter interp);

}; // end of struct App

// INLINE METHODS

} // end of namespace MiniScript
