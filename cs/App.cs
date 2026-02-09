using System;
using System.Collections.Generic;
using System.Linq;	// only for ToList!
using MiniScript;
using static MiniScript.ValueHelpers;

// H: #include "CodeEmitter.g.h"
// H: #include "ErrorPool.g.h"
// CPP: #include "UnitTests.g.h"
// CPP: #include "VM.g.h"
// CPP: #include "gc.h"
// CPP: #include "gc_debug_output.h"
// CPP: #include "value_string.h"
// CPP: #include "dispatch_macros.h"
// CPP: #include "VMVis.g.h"
// CPP: #include "Assembler.g.h"
// CPP: #include "Disassembler.g.h"
// CPP: #include "Parser.g.h"
// CPP: #include "CodeGenerator.g.h"
// CPP: using namespace MiniScript;

namespace MiniScript {

public struct App {
	public static bool debugMode = false;
	public static bool visMode = false;
	
	public static void MainProgram(List<String> args) {
		// CPP: gc_init();
	
		// Parse command-line switches
		Int32 fileArgIndex = -1;
		String inlineCode = null;
		for (Int32 i = 1; i < args.Count; i++) {
			if (args[i] == "-debug" || args[i] == "-d") {
				debugMode = true;
			} else if (args[i] == "-vis") {
				visMode = true;
			} else if (args[i] == "-c" && i + 1 < args.Count) {
				// Inline code follows -c
				i = i + 1;
				inlineCode = args[i];
			} else if (!args[i].StartsWith("-")) {
				// First non-switch argument is the file
				if (fileArgIndex == -1) fileArgIndex = i;
			}
		}
		
		IOHelper.Print("MiniScript 2.0");
		/*** BEGIN CPP_ONLY ***
		#if VM_USE_COMPUTED_GOTO
		#define VARIANT "(goto)"
		#else
		#define VARIANT "(switch)"
		#endif
		*** END CPP_ONLY ***/
		IOHelper.Print(
			"Build: C# version" // CPP: "Build: C++ " VARIANT " version"
		);
		
		if (debugMode) {
			IOHelper.Print("Running unit tests...");
			if (!UnitTests.RunAll()) return;
			IOHelper.Print("Unit tests complete.");

			IOHelper.Print("Running integration tests...");
			if (!RunIntegrationTests("tests/testSuite.txt")) {
				IOHelper.Print("Some integration tests failed.");
//				return;
			}
			IOHelper.Print("Integration tests complete.");
		}
		
		// Handle inline code (-c) or file argument
		List<FuncDef> functions = null;
		ErrorPool errors = ErrorPool.Create();

		if (inlineCode != null) {
			// Compile inline code
			if (debugMode) IOHelper.Print(StringUtils.Format("Compiling: {0}", inlineCode));
			functions = CompileSource(inlineCode, errors, debugMode);
		} else if (fileArgIndex != -1) {
			String filePath = args[fileArgIndex];

			// Determine file type by extension
			if (filePath.EndsWith(".ms")) {
				functions = CompileSourceFile(filePath, errors);
			} else {
				// Default to assembly file (.msa or any other extension)
				functions = AssembleFile(filePath);
			}
		}

		if (functions != null) {
			RunProgram(functions, errors);
		}

		if (errors.HasError()) {
			IOHelper.Print(errors.TopError());
		}
		
		IOHelper.Print("All done!");
	}

	// Compile MiniScript source code to a list of functions.
	// Set verbose=true for extra debug output (assembly listing, etc.)
	private static List<FuncDef> CompileSource(String source, ErrorPool errors, Boolean verbose=false) {
		// Parse the source as a program (multiple statements)
		Parser parser = new Parser();
		parser.Errors = errors;
		parser.Init(source);
		List<ASTNode> statements = parser.ParseProgram();

		if (parser.HadError()) return null;

		if (statements.Count == 0) {
			if (verbose) IOHelper.Print("No statements to compile.");
			return null;
		}

		// Simplify each AST (e.g. constant folding)
		for (Int32 i = 0; i < statements.Count; i++) {
			statements[i] = statements[i].Simplify();
		}

		// Compile to bytecode
		BytecodeEmitter emitter = new BytecodeEmitter();
		CodeGenerator generator = new CodeGenerator(emitter);
		generator.Errors = errors;
		generator.CompileProgram(statements, "@main");

		if (generator.Errors.HasError()) return null;

		if (verbose) IOHelper.Print("Compilation complete.");

		// In verbose mode, also generate and print assembly
		if (verbose) {
			AssemblyEmitter asmEmitter = new AssemblyEmitter();
			CodeGenerator asmGenerator = new CodeGenerator(asmEmitter);
			asmGenerator.CompileProgram(statements, "@main");

			IOHelper.Print("\nGenerated assembly:");
			IOHelper.Print(asmEmitter.GetAssembly());
		}

		return generator.GetFunctions();
	}

	private static List<FuncDef> CompileSourceFile(String filePath, ErrorPool errors) {
		if (debugMode) IOHelper.Print(StringUtils.Format("Reading source file: {0}", filePath));

		List<String> lines = IOHelper.ReadFile(filePath);
		if (lines.Count == 0) {
			IOHelper.Print("No lines read from file.");
			return null;
		}

		// Join lines into a single source string
		String source = "";
		for (Int32 i = 0; i < lines.Count; i++) {
			if (i > 0) source += "\n";
			source += lines[i];
		}

		if (debugMode) IOHelper.Print(StringUtils.Format("Parsing {0} lines...", lines.Count));

		return CompileSource(source, errors, debugMode);
	}

	// Assemble an assembly file (.msa) to a list of functions
	private static List<FuncDef> AssembleFile(String filePath) {
		if (debugMode) IOHelper.Print(StringUtils.Format("Reading assembly file: {0}", filePath));

		List<String> lines = IOHelper.ReadFile(filePath);
		if (lines.Count == 0) {
			IOHelper.Print("No lines read from file.");
			return null;
		}

		if (debugMode) IOHelper.Print(StringUtils.Format("Assembling {0} lines...", lines.Count));
		Assembler assembler = new Assembler();

		// Assemble the code
		assembler.Assemble(lines);

		// Check for assembly errors
		if (assembler.HasError) {
			IOHelper.Print("Assembly failed with errors.");
			return null;
		}

		if (debugMode) IOHelper.Print("Assembly complete.");

		return assembler.Functions;
	}

	// Run integration tests from a test suite file
	public static bool RunIntegrationTests(String filePath) {
		List<String> lines = IOHelper.ReadFile(filePath);
		if (lines.Count == 0) {
			IOHelper.Print(StringUtils.Format("Could not read test file: {0}", filePath));
			return false;
		}

		Int32 testCount = 0;
		Int32 passCount = 0;
		Int32 failCount = 0;

		// Parse and run tests
		List<String> inputLines = new List<String>();
		List<String> expectedLines = new List<String>();
		bool inExpected = false;
		Int32 testStartLine = 0;

		for (Int32 i = 0; i < lines.Count; i++) {
			String line = lines[i];

			// Lines starting with ==== are comments/separators
			if (line.StartsWith("====")) {
				// If we have a pending test, run it
				if (inputLines.Count > 0) {
					testCount++;
					bool passed = RunSingleTest(inputLines, expectedLines, testStartLine);
					if (passed) {
						passCount++;
					} else {
						failCount++;
					}
				}
				// Reset for next test
				inputLines = new List<String>();
				expectedLines = new List<String>();
				inExpected = false;
				testStartLine = i + 2;  // Next line after this comment
				continue;
			}

			// Lines starting with ---- separate input from expected output
			if (line.StartsWith("----")) {
				inExpected = true;
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
		if (inputLines.Count > 0) {
			testCount++;
			bool passed = RunSingleTest(inputLines, expectedLines, testStartLine);
			if (passed) {
				passCount++;
			} else {
				failCount++;
			}
		}

		// Report results
		IOHelper.Print(StringUtils.Format("Integration tests: {0} passed, {1} failed, {2} total",
			passCount, failCount, testCount));

		return failCount == 0;
	}

	// Run a single integration test
	private static bool RunSingleTest(List<String> inputLines, List<String> expectedLines, Int32 lineNum) {
		// Join input lines into source code
		String source = "";
		for (Int32 i = 0; i < inputLines.Count; i++) {
			if (i > 0) source += "\n";
			source += inputLines[i];
		}

		// Skip empty tests
		if (String.IsNullOrEmpty(source.Trim())) return true;

		// Set up a shared error pool for all stages
		ErrorPool errors = ErrorPool.Create();

		// Compile the source (parser + code generator share the error pool)
		List<FuncDef> functions = CompileSource(source, errors);

		// Set up print output capture
		List<String> printOutput = new List<String>();
		// CPP: static List<String> gPrintOutput;
		// CPP: gPrintOutput = printOutput;  // Use global reference

		if (functions != null) {
			// Run the program with print callback
			VM vm = new VM();
			vm.Errors = errors;
			//*** BEGIN CS_ONLY ***
			vm.SetPrintCallback((String s) => { printOutput.Add(s); });
			//*** END CS_ONLY ***
			// CPP: VMStorage::sPrintCallback = [](const String& s) { gPrintOutput.Add(s); };
			vm.Reset(functions);
			vm.Run();

			// Reset the callback before returning
			//*** BEGIN CS_ONLY ***
			vm.SetPrintCallback(null);
			//*** END CS_ONLY ***
			// CPP: VMStorage::sPrintCallback = nullptr;
		}

		// If there were errors, add the first error to the output
		// (matching MiniScript 1.x behavior of reporting only the first error)
		if (errors.HasError()) {
			printOutput.Add(errors.TopError());
		}

		// Get expected output (join lines, trim trailing empty lines)
		String expected = "";
		for (Int32 i = 0; i < expectedLines.Count; i++) {
			if (i > 0) expected += "\n";
			expected += expectedLines[i];
		}
		expected = expected.Trim();

		// Get actual output from print statements
		String actual = "";
		for (Int32 i = 0; i < printOutput.Count; i++) {
			if (i > 0) actual += "\n";
			actual += printOutput[i];
		}
		actual = actual.Trim();

		if (actual != expected) {
			IOHelper.Print(StringUtils.Format("FAIL (line {0}): {1}", lineNum, source));
			IOHelper.Print(StringUtils.Format("  Expected: {0}", expected));
			IOHelper.Print(StringUtils.Format("  Actual:   {0}", actual));
			return false;
		}

		return true;
	}

	// Run a program given its list of functions
	private static void RunProgram(List<FuncDef> functions, ErrorPool errors) {
		// Disassemble and print program (debug only)
		if (debugMode) {
			IOHelper.Print("Disassembly:\n");
			List<String> disassembly = Disassembler.Disassemble(functions, true);
			for (Int32 i = 0; i < disassembly.Count; i++) {
				IOHelper.Print(disassembly[i]);
			}

			// Print all functions found
			IOHelper.Print(StringUtils.Format("Found {0} functions:", functions.Count));
			for (Int32 i = 0; i < functions.Count; i++) {
				FuncDef func = functions[i];
				IOHelper.Print(StringUtils.Format("  {0}: {1} instructions, {2} constants, MaxRegs={3}",
					func.Name, func.Code.Count, func.Constants.Count, func.MaxRegs));
			}

			IOHelper.Print("");
			IOHelper.Print("Executing @main with VM...");
		}

		// Run the program
		VM vm = new VM();
		vm.Errors = errors;
		vm.Reset(functions);
		Value result = make_null();

		if (visMode) {
			VMVis vis = new VMVis(vm);
			vis.ClearScreen();
			while (vm.IsRunning) {
				vis.UpdateDisplay();
				String cmd = IOHelper.Input("Command: ");
				if (String.IsNullOrEmpty(cmd)) cmd = "step";
				if (cmd[0] == 'q') return;
				if (cmd[0] == 's') {
					result = vm.Run(1);
					continue;
				} else if (cmd == "gcmark") {
					vis.ClearScreen();
					IOHelper.Print("GC mark only applies to the C++ version.");  // CPP: gc_mark_and_report();
				} else if (cmd == "interndump") {
					vis.ClearScreen();
					IOHelper.Print("Intern table dump only applies to the C++ version.");  // CPP: dump_intern_table();
				} else {
					IOHelper.Print("Available commands:");
					IOHelper.Print("q[uit] -- Quit to shell");
					IOHelper.Print("s[tep] -- single-step VM");
					IOHelper.Print("pooldump -- dump all string pool state (C++ only)");
					IOHelper.Print("gcdump -- dump all GC objects with hex view (C++ only)");
					IOHelper.Print("gcmark -- run GC mark and show reachable objects (C++ only)");
					IOHelper.Print("interndump -- dump interned strings table (C++ only)");
				}
				IOHelper.Input("\n(Press Return.)");
				vis.ClearScreen();
			}
		} else {
			vm.DebugMode = debugMode;
			result = vm.Run();
		}

		if (!errors.HasError()) {
			IOHelper.Print("\nVM execution complete. Result in r0:");
			IOHelper.Print(StringUtils.Format("\u001b[1;93m{0}\u001b[0m", result)); // (bold bright yellow)
		}
	}

	//*** BEGIN CS_ONLY ***
	public static void Main(String[] args) {
		// Note: C# args does not include the program name (unlike C++ argv),
		// so we prepend a placeholder to match C++ behavior.
		List<String> argList = new List<String> { "miniscript2" };
		argList.AddRange(args);
		MainProgram(argList);
	}
	//*** END CS_ONLY ***
}

/*** BEGIN CPP_ONLY ***

int main(int argc, const char* argv[]) {
	List<String> args;
	for (int i=0; i<argc; i++) args.Add(String(argv[i]));
	MiniScript::App::MainProgram(args);
}

*** END CPP_ONLY ***/

}
