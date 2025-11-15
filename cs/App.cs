using System;
using System.Collections.Generic;
using System.Linq;	// only for ToList!
using MiniScript;
using static MiniScript.ValueHelpers;

// CPP: #include "UnitTests.g.h"
// CPP: #include "VM.g.h"
// CPP: #include "gc.h"
// CPP: #include "gc_debug_output.h"
// CPP: #include "value_string.h"
// CPP: #include "dispatch_macros.h"
// CPP: #include "VMVis.g.h"
// CPP: #include "StringPool.h"
// CPP: #include "MemPoolShim.g.h"
// CPP: using namespace MiniScript;

public class App {
	public static bool debugMode = false;
	public static bool visMode = false;
	
	public static void Main(string[] args) {
		// CPP: gc_init();

		// Use a temp pool for reading the file, processing command-line arguments, and other setup
		Byte tempPool = MemPoolShim.GetUnusedPool();
		MemPoolShim.SetDefaultStringPool(tempPool);

		//*** BEGIN CS_ONLY ***
		// The args passed to the C# main program do not include the program path.
		// To get an arg list like what C++ gets, we must do:
		args = Environment.GetCommandLineArgs();
		Int32 argCount = args.Length;
		//*** END CS_ONLY ***
		
		// Parse command-line switches
		Int32 fileArgIndex = -1;
		for (Int32 i = 1; i < argCount; i++) {
			if (args[i] == "-debug") {
				debugMode = true;
			} else if (args[i] == "-vis") {
				visMode = true;
			} else if (!args[i].StartsWith("-")) {
				// First non-switch argument is the assembly file
				if (fileArgIndex == -1) fileArgIndex = i;
			}
		}
		
		IOHelper.Print("MiniScript 2.0 Prototype 3");
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
		IOHelper.Print("Milestone 3: complete!");
		IOHelper.Print("Milestone 4: in progress");
		
		if (debugMode) {
			IOHelper.Print("Running unit tests...");
			if (!UnitTests.RunAll()) return;
			IOHelper.Print("Unit tests complete.");
		}
		
		// Check for assembly file argument
		if (fileArgIndex != -1) {
			String filePath = args[fileArgIndex];
			if (debugMode) IOHelper.Print(StringUtils.Format("Reading assembly file: {0}", filePath));
			
			List<String> lines = IOHelper.ReadFile(filePath);
			if (lines.Count == 0) {
				IOHelper.Print("No lines read from file.");
				return;
			}
			
			if (debugMode) IOHelper.Print(StringUtils.Format("Assembling {0} lines...", lines.Count));
			Assembler assembler = new Assembler();
			
			// Assemble the code, with permanent strings stored in pool 0
			MemPoolShim.SetDefaultStringPool(0);
			assembler.Assemble(lines);
			MemPoolShim.SetDefaultStringPool(tempPool);
			
			// Check for assembly errors
			if (assembler.HasError) {
				IOHelper.Print("Assembly failed with errors.");
				return; // Bail out rather than trying to run a half-assembled program
			}
			
			if (debugMode) IOHelper.Print("Assembly complete.");
			
			// Disassemble and print program (debug only)
			if (debugMode) {
				IOHelper.Print("Disassembly:\n");
				List<String> disassembly = Disassembler.Disassemble(assembler.Functions, true);
				for (Int32 i = 0; i < disassembly.Count; i++) {
					IOHelper.Print(disassembly[i]);
				}
				
				// Print all functions found
				IOHelper.Print(StringUtils.Format("Found {0} functions:", assembler.Functions.Count));
				for (Int32 i = 0; i < assembler.Functions.Count; i++) {
					FuncDef func = assembler.Functions[i];
					IOHelper.Print(StringUtils.Format("  {0}: {1} instructions, {2} constants, MaxRegs={3}", 
						func.Name, func.Code.Count, func.Constants.Count, func.MaxRegs));
				}
				
				IOHelper.Print("");
				IOHelper.Print("Executing @main with VM...");
			}
			
			// Release our temp pool, and switch back to pool 0 to run the program.
			MemPoolShim.ClearStringPool(MemPoolShim.GetDefaultStringPool());
			MemPoolShim.SetDefaultStringPool(0);

			// Run the program
			VM vm = new VM();
			vm.Reset(assembler.Functions);
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
					} else if (cmd == "pooldump") {
						vis.ClearScreen();
						IOHelper.Print("String pools only apply to the C++ version.");  // CPP: StringPool::dumpAllPoolState();
					} else if (cmd == "gcdump") {
						vis.ClearScreen();
						IOHelper.Print("GC dump only applies to the C++ version.");  // CPP: gc_dump_objects();
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
		
		IOHelper.Print("All done!");
	}
}

/*** BEGIN CPP_ONLY ***

int main(int argc, const char* argv[]) {
	String* args = new String[argc];
	for (int i=0; i<argc; i++) args[i] = String(argv[i]);
	App::Main(args, argc);
}

*** END CPP_ONLY ***/
