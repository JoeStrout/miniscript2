using System;
using System.Collections.Generic;
using System.Linq;	// only for ToList!
using MiniScript;
using static MiniScript.ValueHelpers;

// H: #include "CodeEmitter.g.h"
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
		}
		
		// Handle inline code (-c) or file argument
		List<FuncDef> functions = null;

		if (inlineCode != null) {
			// Compile inline code
			if (debugMode) IOHelper.Print(StringUtils.Format("Compiling: {0}", inlineCode));
			functions = CompileSource(inlineCode);
		} else if (fileArgIndex != -1) {
			String filePath = args[fileArgIndex];

			// Determine file type by extension
			if (filePath.EndsWith(".ms")) {
				functions = CompileSourceFile(filePath);
			} else {
				// Default to assembly file (.msa or any other extension)
				functions = AssembleFile(filePath);
			}
		}

		if (functions != null) {
			RunProgram(functions);
		}
		
		IOHelper.Print("All done!");
	}

	// Compile MiniScript source code to a list of functions
	private static List<FuncDef> CompileSource(String source) {
		// Parse the source
		Parser parser = new Parser();
		ASTNode ast = parser.Parse(source);

		if (parser.HadError()) {
			IOHelper.Print("Compilation failed with parse errors.");
			return null;
		}

		if (debugMode) IOHelper.Print(StringUtils.Format("AST: {0}", ast.ToStr()));

		// Simplify the AST (constant folding)
		ast = ast.Simplify();

		// Compile to bytecode
		BytecodeEmitter emitter = new BytecodeEmitter();
		CodeGenerator generator = new CodeGenerator(emitter);
		FuncDef mainFunc = generator.CompileFunction(ast, "@main");

		if (debugMode) IOHelper.Print("Compilation complete.");

		// In debug mode, also generate and print assembly
		if (debugMode) {
			AssemblyEmitter asmEmitter = new AssemblyEmitter();
			CodeGenerator asmGenerator = new CodeGenerator(asmEmitter);
			asmGenerator.CompileFunction(ast, "@main");

			IOHelper.Print("\nGenerated assembly:");
			IOHelper.Print(asmEmitter.GetAssembly());
		}

		List<FuncDef> functions = new List<FuncDef>();
		functions.Add(mainFunc);
		return functions;
	}

	// Compile a MiniScript source file (.ms) to a list of functions
	private static List<FuncDef> CompileSourceFile(String filePath) {
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

		return CompileSource(source);
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

	// Run a program given its list of functions
	private static void RunProgram(List<FuncDef> functions) {
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

		if (!vm.ReportRuntimeError()) {
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
