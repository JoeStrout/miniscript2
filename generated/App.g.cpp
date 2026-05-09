// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: App.cs

#include "App.g.h"
#include "CodeEmitter.g.h"
#include "ErrorTypes.g.h"
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
#include "CoreIntrinsics.g.h"
#include <thread>
#include <chrono>
#if USE_EDITLINE
#include "editline/editline.h"
#endif
using namespace MiniScript;

int main(int argc, const char* argv[]) {
List<String> args;
for (int i=0; i<argc; i++) args.Add(String(argv[i]));
MiniScript::App::MainProgram(args);
}

namespace MiniScript {

bool App::debugMode = Boolean(false);
bool App::visMode = Boolean(false);
void App::MainProgram(List<String> args) {
	gc_init();
	value_init_constants();
	ErrorType::Init();

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
	
	IOHelper::Print("MiniScript 2.0", TextStyle::Strong);
	#if VM_USE_COMPUTED_GOTO
	#define VARIANT "(goto)"
	#else
	#define VARIANT "(switch)"
	#endif
	IOHelper::Print(
		"Build: C++ " VARIANT " version",
		TextStyle::Subdued
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
				interp.set_SourceFile(GetPathFilename(filePath));
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
String App::GetPathFilename(String filePath) {
	int pos = filePath.LastIndexOf('/');
	int pos2 = filePath.LastIndexOf('\\');
	if (pos2 >= 0 && (pos < 0 || pos2 > pos)) pos = pos2;
	return (pos >= 0) ? filePath.Substring(pos + 1) : filePath;
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

	if (is_null(vm.Error())) {
		IOHelper::Print("\nVM execution complete. Result in r0:");
		IOHelper::Print(StringUtils::Format("\x1b[1;93m{0}\x1b[0m", result)); // (bold bright yellow)
	} else {
		vm.ReportRuntimeError();
	}
	GC_POP_SCOPE();
}
String App::GetREPLInput(Interpreter interp) {
	while (Boolean(true)) {
		// Build prompt: " _in[N]: " for a fresh line, or a matching-width
		// continuation prompt whose spaces align with the _in prompt.
		Int32 idx = list_count(CoreIntrinsics::replInList);
		String prompt;
		if (interp.NeedMoreInput()) {
			// Width of " _in[N]: " = 8 + digits(N).  Use (3+digits(N)) spaces before "...:".
			String idxStr = StringUtils::Format("{0}", idx);
			String padding = StringUtils::Spaces(idxStr.Length() + 3);
			prompt = padding + "...:   ";
		} else {
			prompt = StringUtils::Format(" _in[{0}]: ", idx);
			IOHelper::Print("");  // blank line before the input prompt
		}

		// Read one raw line.
		String line;
		#if USE_EDITLINE
		String styledPrompt = IOHelper::GetStyleTermCode(TextStyle::Subdued) + prompt +
		  IOHelper::GetStyleTermCode(TextStyle::Normal);
		char* rawLine = readline(styledPrompt.c_str());
		IOHelper::NoteStyleSet(TextStyle::Normal);
		if (!rawLine) return String(nullptr);
		line = rawLine;
		if (rawLine[0] != '\0') add_history(rawLine);
		free(rawLine);
		#else
		line = IOHelper::Input(prompt, TextStyle::Subdued, TextStyle::Normal);
		#endif

		if (IsNull(line)) return String(nullptr);

		// Handle ! metacommands (only valid on the first line of an interaction).
		if (!interp.NeedMoreInput() && line.Length() > 0 && line[0] == '!') {
			String meta = line.Substring(1).Trim();
			// !? [count] [search] — show history
			if (meta.StartsWith("?")) {
				HandleHistorySearch(meta.Substring(1).Trim());
				continue; // prompt again
			}
			// !N or !-N — replay a prior input
			String recalled = RecallInput(meta);
			if (IsNull(recalled)) {
				IOHelper::Print(StringUtils::Format("No such history entry: {0}", meta), TextStyle::Error);
				continue;
			}
			IOHelper::Print(recalled, TextStyle::Subdued); // show what we're replaying
			return recalled;
		}

		return line;
	}
}
Int32 App::ParseInt(String s) {
	if (s.Length() == 0) return -1;
	Int32 result = 0;
	for (Int32 ci = 0; ci < s.Length(); ci++) {
		Int32 d = (Int32)(unsigned char)s[ci] - (Int32)'0';
		if (d < 0 || d > 9) return -1;
		result = result * 10 + d;
	}
	return result;
}
void App::HandleHistorySearch(String metaRest) {
	Int32 count = 15;
	String search = nullptr;

	// Parse optional leading integer count, then optional search term.
	Int32 spacePos = metaRest.IndexOf(' ');
	String countPart = spacePos >= 0 ? metaRest.Substring(0, spacePos) : metaRest;
	String rest = spacePos >= 0 ? metaRest.Substring(spacePos + 1).Trim() : "";
	Int32 parsed = ParseInt(countPart);
	if (parsed > 0) {
		count = parsed;
		if (rest.Length() > 0) search = rest;
	} else {
		// No leading number: treat whole metaRest as search term.
		if (metaRest.Length() > 0) search = metaRest;
	}

	Int32 total = list_count(CoreIntrinsics::replInList);
	// First pass (backward): find the oldest index among the last `count` matches.
	Int32 remaining = count;
	Int32 firstIdx = total;
	for (Int32 i = total - 1; i >= 0 && remaining > 0; i--) {
		String entry = as_cstring(list_get(CoreIntrinsics::replInList, i));
		if (!IsNull(search) && entry.IndexOf(search) < 0) continue;
		remaining--;
		firstIdx = i;
	}
	// Second pass (forward): display in ascending order.
	Int32 shown = 0;
	for (Int32 i = firstIdx; i < total && shown < count; i++) {
		String entry = as_cstring(list_get(CoreIntrinsics::replInList, i));
		if (!IsNull(search) && entry.IndexOf(search) < 0) continue;
		IOHelper::Print(StringUtils::Format(" _in[{0}]: {1}", i, entry), TextStyle::Subdued);
		shown++;
	}
	if (shown == 0) IOHelper::Print("(no matching history)", TextStyle::Subdued);
}
String App::RecallInput(String indexStr) {
	Int32 total = list_count(CoreIntrinsics::replInList);
	if (total == 0) return nullptr;
	bool negative = indexStr.Length() > 0 && indexStr[0] == '-';
	Int32 idx = ParseInt(negative ? indexStr.Substring(1) : indexStr);
	if (idx < 0) return nullptr;
	if (negative) idx = total - idx;
	if (idx < 0 || idx >= total) return nullptr;
	return as_cstring(list_get(CoreIntrinsics::replInList, idx));
}
void App::RunREPL() {
	CoreIntrinsics::replInList = make_list(0);
	CoreIntrinsics::replOutList = make_list(0);

	Interpreter interp =  Interpreter::New();
	interp.set_standardOutput([](String s, Boolean) { IOHelper::Print(s, TextStyle::Strong); });
	interp.set_errorOutput([](String s, Boolean) { IOHelper::Print(s, TextStyle::Error); });

	String currentInput = nullptr;
	GC_PUSH_SCOPE();
	Value inListBefore; GC_PROTECT(&inListBefore);
	Value implVal; GC_PROTECT(&implVal);
	while (Boolean(true)) {
		bool needingMoreBefore = interp.NeedMoreInput();
		String line = GetREPLInput(interp);
		if (IsNull(line)) break;

		// Accumulate multi-line input.
		if (!needingMoreBefore) {
			currentInput = line;
		} else {
			currentInput = currentInput + "\n" + line;
		}

		inListBefore = CoreIntrinsics::replInList;
		interp.REPL(line, 60);

		// When the interaction completes, record it and display implicit output.
		// Skip recording if reset was called (it replaces the lists with fresh ones).
		if (!interp.NeedMoreInput()) {
			bool wasReset = !value_identical(CoreIntrinsics::replInList, inListBefore);
			if (!wasReset) {
				Int32 idx = list_count(CoreIntrinsics::replInList);
				implVal = interp.lastImplicitResult();
				list_push(CoreIntrinsics::replInList, make_string(currentInput));
				list_push(CoreIntrinsics::replOutList, implVal);
				if (!is_null(implVal)) {
					IOHelper::PrintNoCR(StringUtils::Format("_out[{0}]: ", idx), TextStyle::Subdued);
					IOHelper::Print(StringUtils::Format("{0}", implVal), TextStyle::Strong);
				}
			}
			currentInput = nullptr;
		}
	}
	GC_POP_SCOPE();
}

} // end of namespace MiniScript
