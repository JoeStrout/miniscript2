// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: App.cs

#include "App.g.h"
#include "CodeEmitter.g.h"
#include "ErrorPool.g.h"
#include "UnitTests.g.h"
#include "VM.g.h"
#include "gc.h"
#include "gc_debug_output.h"
#include "value_string.h"
#include "dispatch_macros.h"
#include "VMVis.g.h"
#include "Assembler.g.h"
#include "Disassembler.g.h"
#include "Parser.g.h"
#include "CodeGenerator.g.h"
#include "StringUtils.g.h"
#include "IOHelper.g.h"
#include "Interpreter.g.h"
#include "Intrinsic.g.h" // ToDo: remove this once we've refactored set_FunctionIndexOffset away
#include <thread>
#include <chrono>
using namespace MiniScript;

int main(int argc, const char* argv[]) {
List<String> args;
for (int i=0; i<argc; i++) args.Add(String(argv[i]));
MiniScript::App::MainProgram(args);
}

namespace MiniScript {

bool App::debugMode = false;
bool App::visMode = false;
void App::MainProgram(List<String> args) {
	gc_init();
	value_init_constants();

	// Parse command-line switches
	Int32 fileArgIndex = -1;
	String inlineCode = nullptr;
	for (Int32 i = 1; i < args.Count(); i++) {
		if (args[i] == "-debug" || args[i] == "-d") {
			debugMode = Boolean(true);
		} else if (args[i] == "-vis") {
			visMode = Boolean(true);
		} else if (args[i] == "-c" && i + 1 < args.Count()) {
			// Inline code follows -c
			i = i + 1;
			inlineCode = args[i];
		} else if (!args[i].StartsWith("-")) {
			// First non-switch argument is the file
			if (fileArgIndex == -1) fileArgIndex = i;
		}
	}
	
	IOHelper::Print("MiniScript 2.0");
	#if VM_USE_COMPUTED_GOTO
	#define VARIANT "(goto)"
	#else
	#define VARIANT "(switch)"
	#endif
	IOHelper::Print(
		"Build: C++ " VARIANT " version"
	);
	
	if (debugMode) {
		IOHelper::Print("Running unit tests...");
		if (!UnitTests::RunAll()) return;
		IOHelper::Print("Unit tests complete.");

		IOHelper::Print("Running integration tests...");
		if (!RunIntegrationTests("tests/testSuite.txt")) {
			IOHelper::Print("Some integration tests failed.");
//				return;
		}
		IOHelper::Print("Integration tests complete.");
	}
	
	// Handle inline code (-c), file argument, or REPL
	if (!IsNull(inlineCode)) {
		if (debugMode) IOHelper::Print(StringUtils::Format("Compiling: {0}", inlineCode));
		Interpreter interp = CreateInterpreter();
		interp.Reset(inlineCode);
		RunInterpreter(interp);
	} else if (fileArgIndex != -1) {
		String filePath = args[fileArgIndex];
		Interpreter interp = CreateInterpreter();
		if (filePath.EndsWith(".ms")) {
			// Source file: read, join, and compile via Interpreter
			if (debugMode) IOHelper::Print(StringUtils::Format("Reading source file: {0}", filePath));
			List<String> lines = IOHelper::ReadFile(filePath);
			if (lines.Count() == 0) {
				IOHelper::Print("No lines read from file.");
			} else {
				String source = "";
				for (Int32 i = 0; i < lines.Count(); i++) {
					if (i > 0) source += "\n";
					source += lines[i];
				}
				if (debugMode) IOHelper::Print(StringUtils::Format("Parsing {0} lines...", lines.Count()));
				interp.Reset(source);
				RunInterpreter(interp);
			}
		} else {
			// Assembly file (.msa or any other extension)
			List<FuncDef> functions = AssembleFile(filePath);
			if (!IsNull(functions)) {
				interp.Reset(functions);
				RunInterpreter(interp);
			}
		}
	} else if (!debugMode) {
		// No file or inline code: enter REPL mode
		RunREPL();
	}

	IOHelper::Print("All done!");
}
Interpreter App::CreateInterpreter() {
	Interpreter interp =  Interpreter::New();
	interp.set_standardOutput([](String s, Boolean) { IOHelper::Print(s); });
	interp.set_errorOutput([](String s, Boolean) { IOHelper::Print(s); });
	return interp;
}
List<FuncDef> App::AssembleFile(String filePath) {
	if (debugMode) IOHelper::Print(StringUtils::Format("Reading assembly file: {0}", filePath));

	List<String> lines = IOHelper::ReadFile(filePath);
	if (lines.Count() == 0) {
		IOHelper::Print("No lines read from file.");
		return nullptr;
	}

	if (debugMode) IOHelper::Print(StringUtils::Format("Assembling {0} lines...", lines.Count()));
	Assembler assembler =  Assembler::New();
	assembler.SetFunctionIndexOffset(Intrinsic::Count());

	// Assemble the code
	assembler.Assemble(lines);

	// Check for assembly errors
	if (assembler.HasError()) {
		IOHelper::Print("Assembly failed with errors.");
		return nullptr;
	}

	if (debugMode) IOHelper::Print("Assembly complete.");

	return assembler.Functions();
}
bool App::RunIntegrationTests(String filePath) {
	List<String> lines = IOHelper::ReadFile(filePath);
	if (lines.Count() == 0) {
		IOHelper::Print(StringUtils::Format("Could not read test file: {0}", filePath));
		return Boolean(false);
	}

	Int32 testCount = 0;
	Int32 passCount = 0;
	Int32 failCount = 0;

	// Parse and run tests
	List<String> inputLines =  List<String>::New();
	List<String> expectedLines =  List<String>::New();
	bool inExpected = Boolean(false);
	Int32 testStartLine = 0;

	for (Int32 i = 0; i < lines.Count(); i++) {
		String line = lines[i];

		// Lines starting with ==== are comments/separators
		if (line.StartsWith("====")) {
			// If we have a pending test, run it
			if (inputLines.Count() > 0) {
				testCount++;
				bool passed = RunSingleTest(inputLines, expectedLines, testStartLine);
				if (passed) {
					passCount++;
				} else {
					failCount++;
				}
			}
			// Reset for next test
			inputLines =  List<String>::New();
			expectedLines =  List<String>::New();
			inExpected = Boolean(false);
			testStartLine = i + 2;  // Next line after this comment
			continue;
		}

		// Lines starting with ---- separate input from expected output
		if (line.StartsWith("----")) {
			inExpected = Boolean(true);
			continue;
		}

		// Accumulate lines
		if (inExpected) {
			expectedLines.Add(line);
		} else {
			inputLines.Add(line);
		}
	}

	// Handle final test if file doesn't end with ====
	if (inputLines.Count() > 0) {
		testCount++;
		bool passed = RunSingleTest(inputLines, expectedLines, testStartLine);
		if (passed) {
			passCount++;
		} else {
			failCount++;
		}
	}

	// Report results
	IOHelper::Print(StringUtils::Format("Integration tests: {0} passed, {1} failed, {2} total",
		passCount, failCount, testCount));

	return failCount == 0;
}
bool App::RunSingleTest(List<String> inputLines,List<String> expectedLines,Int32 lineNum) {
	// Join input lines into source code
	String source = "";
	for (Int32 i = 0; i < inputLines.Count(); i++) {
		if (i > 0) source += "\n";
		source += inputLines[i];
	}

	// Skip empty tests
	if (String::IsNullOrEmpty(source.Trim())) return Boolean(true);

	// Set up print output capture
	List<String> printOutput =  List<String>::New();
	static List<String> gPrintOutput;
	gPrintOutput = printOutput;  // Use global reference

	// Compile and run via Interpreter
	Interpreter interp =  Interpreter::New();
	interp.set_standardOutput([](String s, Boolean) { gPrintOutput.Add(s); });
	interp.set_errorOutput([](String s, Boolean) { gPrintOutput.Add(s); });
	interp.Reset(source);
	interp.RunUntilDone();

	// Get expected output (join lines, trim trailing empty lines)
	String expected = "";
	for (Int32 i = 0; i < expectedLines.Count(); i++) {
		if (i > 0) expected += "\n";
		expected += expectedLines[i];
	}
	expected = expected.Trim();

	// Get actual output from print statements
	String actual = "";
	for (Int32 i = 0; i < printOutput.Count(); i++) {
		if (i > 0) actual += "\n";
		actual += printOutput[i];
	}
	actual = actual.Trim();

	if (actual != expected) {
		IOHelper::Print(StringUtils::Format("FAIL (line {0}): {1}", lineNum, source));
		IOHelper::Print(StringUtils::Format("Expected:\n{0}", expected));
		IOHelper::Print(StringUtils::Format("Actual:  \n{0}", actual));
		return Boolean(false);
	}

	return Boolean(true);
}
void App::RunInterpreter(Interpreter interp) {
	interp.Compile();
	VM vm = interp.vm();
	if (IsNull(vm)) return;		// compilation error (already reported)

	// Debug: disassemble and print
	if (debugMode) {
		List<FuncDef> functions = vm.GetFunctions();
		IOHelper::Print("Disassembly:\n");
		List<String> disassembly = Disassembler::Disassemble(functions, Boolean(true));
		for (Int32 i = 0; i < disassembly.Count(); i++) {
			IOHelper::Print(disassembly[i]);
		}

		IOHelper::Print(StringUtils::Format("Found {0} functions:", functions.Count()));
		for (Int32 i = 0; i < functions.Count(); i++) {
			FuncDef func = functions[i];
			IOHelper::Print(StringUtils::Format("  {0}: {1} instructions, {2} constants, MaxRegs={3}",
				func.Name(), func.Code().Count(), func.Constants().Count(), func.MaxRegs()));
		}

		IOHelper::Print("");
		IOHelper::Print("Executing @main with VM...");
	}

	GC_PUSH_SCOPE();
	Value result = val_null; GC_PROTECT(&result);

	if (visMode) {
		VMVis vis = VMVis(vm);
		vis.ClearScreen();
		while (vm.IsRunning()) {
			vis.UpdateDisplay();
			String cmd = IOHelper::Input("Command: ");
			if (String::IsNullOrEmpty(cmd)) cmd = "step";
			if (cmd[0] == 'q')  {
				GC_POP_SCOPE();
				return;
			}
			if (cmd[0] == 's') {
				result = vm.Run(1);
				continue;
			} else if (cmd == "gcmark") {
				vis.ClearScreen();
				gc_mark_and_report();
			} else if (cmd == "interndump") {
				vis.ClearScreen();
				dump_intern_table();
			} else {
				IOHelper::Print("Available commands:");
				IOHelper::Print("q[uit] -- Quit to shell");
				IOHelper::Print("s[tep] -- single-step VM");
				IOHelper::Print("gcmark -- run GC mark and show reachable objects (C++ only)");
				IOHelper::Print("interndump -- dump interned strings table (C++ only)");
			}
			IOHelper::Input("\n(Press Return.)");
			vis.ClearScreen();
		}
	} else {
		vm.set_DebugMode(debugMode);
		while (vm.IsRunning()) {
			result = vm.Run();
			if (vm.IsRunning()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
	}

	if (!vm.Errors().HasError()) {
		IOHelper::Print("\nVM execution complete. Result in r0:");
		IOHelper::Print(StringUtils::Format("\u001b[1;93m{0}\u001b[0m", result)); // (bold bright yellow)
	}
	GC_POP_SCOPE();
}
void App::RunREPL() {
	Interpreter interp =  Interpreter::New();
	interp.set_standardOutput([](String s, Boolean) { IOHelper::Print(s); });
	interp.set_errorOutput([](String s, Boolean) { IOHelper::Print(s); });
	interp.set_implicitOutput([](String s, Boolean) { IOHelper::Print(s); });
	while (Boolean(true)) {
		String prompt = interp.NeedMoreInput() ? ">>> " : "> ";
		String line = IOHelper::Input(prompt);
		if (IsNull(line)) break;
		interp.REPL(line, 60);
	}
}

} // end of namespace MiniScript
