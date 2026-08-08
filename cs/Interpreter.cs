// Interpreter.cs
//
// The Interpreter class is the main interface to the MiniScript system.
// You give it some MiniScript source code, and tell it where to send its
// output (via delegate functions called TextOutputMethod).  Then you typically
// call RunUntilDone, which returns when either the script has stopped or the
// given timeout has passed.

using System;
using System.Collections.Generic;
// H: #include "IntrinsicAPI.g.h"
// H: #include "VM.g.h"
// H: #include "Parser.g.h"
// H: #include "AST.g.h"
// H: #include "IOHelper.g.h"
// H: #include "Bytecode.g.h"
// H: #include "CodeGenerator.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "CS_value_util.h"
// CPP: #include "CoreIntrinsics.g.h"

namespace MiniScript {

// H: typedef void* object;

// 
// Interpreter: an object that contains and runs one MiniScript script.
// 
public class Interpreter {

	// 
	// standardOutput: receives the output of the "print" intrinsic.
	// 
	public TextOutputMethod standardOutput;
	
	// 
	// implicitOutput: receives the value of expressions entered when
	// in REPL mode.  If you're not using the REPL() method, you can
	// safely ignore this.
	// 
	public TextOutputMethod implicitOutput = null;

	// 
	// errorOutput: receives error messages from the compiler or runtime.
	// (This happens via the ReportError method, which is virtual; so if you
	// want to catch the actual errors differently, you can subclass
	// Interpreter and override that method.)
	// 
	public TextOutputMethod errorOutput;

	// 
	// hostData is just a convenient place for you to attach some arbitrary
	// data to the interpreter.  It gets passed through to the context object,
	// so you can access it inside your custom intrinsic functions.  Use it
	// for whatever you like (or don't, if you don't feel the need).
	// 
	public object hostData = null;

	// 
	// done: returns true when we don't have a virtual machine, or we do have
	// one and it is done (has reached the end of its code).
	// 
	public bool done {
		get { return vm == null || !vm.IsRunning; }
	}

	// 
	// vm: the virtual machine this interpreter is running.  Most applications
	// will not need to use this, but it's provided for advanced users.
	// 
	public VM vm;

	// 
	// SourceFile: the name of the file this interpreter loaded (e.g. "myScript.ms"),
	// or empty string for source provided directly as a string.
	// Used to populate FuncDef.FileName for stack traces.
	// 
	public String SourceFile = "";

	protected String source;
	protected Parser parser;
	protected List<FuncDef> compiledFunctions;

	// 
	// The most recent compiler or runtime error, as an error Value, or Value.Null
	// if there is no error.  Host code can inspect this (and its __isa chain)
	// to distinguish error types.
	// 
	public Value Error;

	// 
	// The Value produced by the last complete REPL interaction that had implicit
	// output (a bare expression as the last statement), or Value.Null otherwise.
	// Updated at the end of each complete REPL() call.  Host code (e.g. the
	// REPL loop in App.cs) reads this to push it into the _out history list.
	// 
	public Value lastImplicitResult = Value.Null;

	// REPL state
	private String _pendingSource;       // accumulated REPL lines so far

	// This interpreter's global namespace.  Created on demand and then kept for
	// the life of the interpreter unless Reset replaces it, so it is stable across
	// REPL lines, across a program ending, and across chaining to a new program.
	// Handed to each VM at Reset.  See notes/GLOBALS.md.
	private Globals _globals;

	// H_WRAPPER: public: Interpreter(InterpreterStorage* p) : storage(p ? p->shared_from_this() : nullptr) {}  
  
	// 
	// Constructor taking some MiniScript source code, and the output delegates.
	// 
	public Interpreter(String source=null, TextOutputMethod standardOutput=null, TextOutputMethod errorOutput=null) {
		Init(source, standardOutput, errorOutput);
	}
	
	private void Init(String _source, TextOutputMethod _standardOutput, TextOutputMethod _errorOutput) {
		source = _source;
		if (_standardOutput == null) {
			_standardOutput = (s, eol) => IOHelper.Print(s); // CPP: _standardOutput = [](String s, Boolean) { IOHelper::Print(s); };
		}
		if (errorOutput == null) errorOutput = _standardOutput;
		standardOutput = _standardOutput;
		errorOutput = _errorOutput;
		Error = Value.Null;
		_globals = null;
	}

	//
	// This interpreter's global namespace, created if it does not exist yet.
	// Available before the first compile, which is what lets a host seed globals
	// (and read them back) without any program having run -- the job the REPL("")
	// bootstrap used to do.
	//
	public Globals GetGlobals() {
		if (_globals == null) _globals = Globals.Create();
		return _globals;
	}

	//
	// Discard every global, in place.  The namespace object and its slot numbering
	// survive, so this takes effect immediately for code that is already running --
	// including the rest of the statement that asked for it, which is what the
	// `reset` intrinsic needs.
	//
	public void ClearGlobals() {
		if (_globals != null) _globals.Clear();
	}

	// 
	// Constructor taking source code in the form of a list of strings.
	// 
	public Interpreter(List<String> sourceList, TextOutputMethod standardOutput=null, TextOutputMethod errorOutput=null) {
		String source = String.Join("\n", sourceList);
		Init(source, standardOutput, errorOutput);
	}
	
	//*** BEGIN CS_ONLY ***
	// 
	// Constructor taking source code in the form of a string array.
	// 
	public Interpreter(String[] sourceArray, TextOutputMethod standardOutput=null, TextOutputMethod errorOutput=null) {
		String source = String.Join("\n", sourceArray);
		Init(source, standardOutput, errorOutput);
	}
	//*** END CS_ONLY ***
	
	// H: public: virtual ~InterpreterStorage() = default;

	// H_WRAPPER: public: InterpreterStorage* get_storage() const { return storage.get(); }

	// 
	// Stop the virtual machine, and jump to the end of the program code.
	// Also reset the parser, in case it's stuck waiting for a block ender.
	// 
	public void Stop() {
		if (vm != null) vm.Stop();
		// TODO: if (parser != null) parser.PartialReset();
	}

	// 
	// Reset the interpreter with the given source code.
	// 
	public void Reset(String _source="") {
		source = _source;
		parser = null;
		vm = null;
		compiledFunctions = null;
		Error = Value.Null;
		// A new program gets a new namespace.  Releasing the old one drops the GC
		// root its map holds; anything still referring to it (a function compiled
		// by the old program and handed to a host, say) keeps it alive normally.
		if (_globals != null) {
			_globals.Release();
			_globals = null;
		}
	}

	//
	// Reset the interpreter with the given source code and compile it, but keep
	// the current program's global variables, so the new program starts with
	// those globals already defined.  This is what a "chain to another script"
	// intrinsic (`run`) needs: the outgoing script's state stays available, and
	// any global the new script assigns simply overwrites the inherited value.
	// Functions carried over keep working too, since a top-level function's
	// closure captures this very namespace.
	//
	// This is now just Reset without the part that drops the namespace: the
	// globals were never bound to the outgoing program's registers, so there is
	// nothing to gather off them and nothing to rebind.
	//
	// The outgoing VM is stopped, which matters when (as with `run`) this is
	// called from an intrinsic: we are then inside that VM's own Run loop, and
	// replacing this.vm does not stop it -- without the Stop it would carry on
	// executing the rest of the abandoned program after the intrinsic returns.
	//
	public void ResetPreservingGlobals(String _source="") {
		if (vm != null) vm.Stop();
		Globals keptGlobals = GetGlobals();
		_globals = null;         // hide it from Reset, which Releases what it finds
		Reset(_source);
		_globals = keptGlobals;  // still rooted, since Reset never saw it
		Compile();
	}

	// 
	// Reset the interpreter with pre-compiled functions (e.g. from an assembler).
	// The list must contain a FuncDef named "@main".
	// 
	public void Reset(List<FuncDef> functions) {
		source = null;
		parser = null;
		compiledFunctions = functions;
		Error = Value.Null;

		// A new program gets a new namespace, as in Reset(String).
		if (_globals != null) {
			_globals.Release();
			_globals = null;
		}

		// Create and configure VM
		vm = new VM();
		vm.SetInterpreter(this);
		vm.Reset(functions, GetGlobals());
	}

	// 
	// Compile our source code, if we haven't already done so, so that we are
	// either ready to run, or generate compiler errors (reported via errorOutput).
	// 
	public void Compile() {
		if (vm != null) return;		// already compiled

		Error = Value.Null;

		if (parser == null) parser = new Parser();
		parser.Init(source, SourceFile);
		List<ASTNode> statements = parser.ParseProgram();
		parser.RequireComplete();   // no more input is coming; this is not a REPL

		if (parser.HadError()) {
			Error = parser.Error;
			ReportError(Error);
			return;
		}

		if (statements.Count == 0) return;

		// Simplify AST (constant folding, etc.)
		for (Int32 i = 0; i < statements.Count; i++) {
			statements[i] = statements[i].Simplify();
		}

		// Compile to bytecode (offset past intrinsics so indices don't collide)
		BytecodeEmitter emitter = new BytecodeEmitter();
		CodeGenerator generator = new CodeGenerator(emitter);
		generator.FileName = SourceFile;
		generator.CompileProgram(statements, "@main");

		if (!generator.Error.IsNull()) {
			Error = generator.Error;
			ReportError(Error);
			return;
		}

		compiledFunctions = generator.GetFunctions();

		// Create and configure VM, running in this interpreter's namespace --
		// which already holds anything a host seeded, or (via
		// ResetPreservingGlobals) the outgoing program's globals.
		vm = new VM();
		vm.SetInterpreter(this);
		vm.Reset(compiledFunctions, GetGlobals());
	}

	//
	// Compile a standalone chunk of source (e.g. an imported module) to a
	// runnable FuncDef, without touching this interpreter's own VM.  This wraps
	// the parse -> simplify -> code-generate pipeline that `import`-style
	// intrinsics need, so hosts (and our own ShellIntrinsics) don't have to
	// open-code it.  The returned FuncDef is the module's "@main"; nested
	// functions are embedded in it, so it can be handed straight to
	// VM.ManuallyPushCall.  On a parse or compile error, returns a null FuncDef
	// and sets `error` to the error Value (Value.Null on success).
	//
	public static FuncDef CompileToFunc(String source, String fileName, out Value error) {
		error = Value.Null;
		Parser parser = new Parser();
		parser.Init(source, fileName);
		List<ASTNode> statements = parser.ParseProgram();
		parser.RequireComplete();   // a module file ends where it ends
		if (parser.HadError()) {
			error = parser.Error;
			return null;
		}
		// Simplify AST (constant folding, etc.)
		for (Int32 i = 0; i < statements.Count; i++) {
			statements[i] = statements[i].Simplify();
		}
		BytecodeEmitter emitter = new BytecodeEmitter();
		CodeGenerator generator = new CodeGenerator(emitter);
		generator.FileName = fileName;
		List<FuncDef> functions = generator.CompileImport(statements, fileName);
		if (!generator.Error.IsNull()) {
			error = generator.Error;
			return null;
		}
		if (functions.Count == 0) return null;
		return functions[0];   // the module's @main
	}

	//
	// Synchronously call a MiniScript function value with the given arguments,
	// running the VM re-entrantly to completion, and return its result.  This is
	// the host-facing entry point for calling back into MiniScript from native
	// code (e.g. raylib's file-I/O and trace-log callback hooks): it is safe to
	// call from inside an intrinsic or C callback, where the host needs a result
	// immediately and cannot unwind the VM.  Returns Value.Null if there is no VM.
	//
	public Value RunFunction(Value funcRef, List<Value> args) {
		if (vm == null) return Value.Null;
		return vm.RunFunction(funcRef, args);
	}

	//
	// Reset the virtual machine to the beginning of the code.  Note that this
	// does *not* recompile; it simply resets the VM with the same functions.
	// Useful in cases where you have a short script you want to run over and
	// over, without recompiling every time.
	// 
	public void Restart() {
		if (vm != null && compiledFunctions != null) {
			Error = Value.Null;
			vm.Reset(compiledFunctions);
		}
	}

	// 
	// Run the compiled code until we either reach the end, or we reach the
	// specified time limit.  In the latter case, you can then call RunUntilDone
	// again to continue execution right from where it left off.
	///
	// Or, if returnEarly is true, we will also return if the VM is yielding
	// (i.e., an intrinsic needs to wait for something).  Again, call
	// RunUntilDone again later to continue.
	///
	// Note that this method first compiles the source code if it wasn't compiled
	// already, and in that case, may generate compiler errors.  And of course
	// it may generate runtime errors while running.  In either case, these are
	// reported via errorOutput.
	// 
	// <param name="timeLimit">maximum amount of time to run before returning, in seconds</param>
	// <param name="returnEarly">if true, return as soon as the VM yields</param>
	public void RunUntilDone(double timeLimit=60, bool returnEarly=true) {
		if (vm == null) {
			Compile();
			if (vm == null) return;		// (must have been some error)
		}
		double startTime = vm.ElapsedTime();
		vm.yielding = false;
		while (vm.IsRunning && !vm.yielding) {
			if (vm.ElapsedTime() - startTime > timeLimit) return;	// time's up for now
			vm.Run(1000);	// run in small batches so we can check the time
			if (!vm.Error.IsNull()) {
				Error = vm.Error;
				ReportError(Error);
				Stop();
				return;
			}
			if (returnEarly && vm.yielding) return;		// waiting for something
		}
	}

	// 
	// Run one step (small batch) of the virtual machine.  This method is not
	// very useful except in special cases; usually you will use RunUntilDone instead.
	// 
	public void Step() {
		Compile();
		if (vm == null) return;
		vm.Run(1);
		if (!vm.Error.IsNull()) {
			Error = vm.Error;
			ReportError(Error);
			Stop();
		}
	}

	// 
	// Read Eval Print Loop.  Run the given source until it either terminates,
	// or hits the given time limit.  When it terminates, if we have new
	// implicit output, print that to the implicitOutput stream.
	// 
	// <param name="sourceLine">line of source code to parse and run</param>
	// <param name="timeLimit">time limit in seconds</param>
	public void REPL(String sourceLine, double timeLimit=60) {
		if (sourceLine == null) sourceLine = "";

		// Accumulate source lines
		if (_pendingSource == null) {
			_pendingSource = sourceLine;
		} else {
			_pendingSource = _pendingSource + "\n" + sourceLine;
		}

		// Try to parse
		Error = Value.Null;
		if (parser == null) parser = new Parser();
		parser.Init(_pendingSource);
		List<ASTNode> statements = parser.ParseProgram();

		// If parser needs more input, return and wait for next line
		if (parser.NeedMoreInput()) return;

		// If there were parse errors, report and reset
		if (parser.HadError()) {
			Error = parser.Error;
			ReportError(Error);
			_pendingSource = null;
			return;
		}

		// Nothing to do if there are no statements.  (This used to make an
		// exception for REPL("") with no VM yet, because seeding globals needed a
		// compile to have created somewhere to put them.  SetGlobalValue now works
		// before the first compile, so that bootstrap is gone.)
		if (statements.Count == 0) {
			_pendingSource = null;
			return;
		}

		// Simplify AST
		for (Int32 i = 0; i < statements.Count; i++) {
			statements[i] = statements[i].Simplify();
		}

		// Detect implicit output: last statement is a bare expression
		// (not an assignment, block statement, break, continue, or return)
		Boolean hasImplicitOutput = false;
		if (statements.Count > 0) {
			ASTNode lastStmt = statements[statements.Count - 1];
			hasImplicitOutput = !lastStmt.IsStatement();
		}

		// Compile to bytecode.  Each REPL line is its own @main; previously
		// defined functions are reached as funcref values in the globals table.
		BytecodeEmitter emitter = new BytecodeEmitter();
		CodeGenerator generator = new CodeGenerator(emitter);
		generator.CompileProgram(statements, "@main");

		if (!generator.Error.IsNull()) {
			Error = generator.Error;
			ReportError(Error);
			_pendingSource = null;
			return;
		}

		List<FuncDef> functions = generator.GetFunctions();

		// Debug: output the disassembly
		//foreach (String line in Disassembler.Disassemble(functions)) {
		//	IOHelper.Print(line);
		//}

		// Create/reset VM.  The namespace is the interpreter's, so it is the same
		// one every line -- there is no first-line special case, and nothing is
		// rebound onto the new @main's registers.
		if (vm == null) vm = new VM();
		vm.SetInterpreter(this);
		vm.Reset(functions, GetGlobals());

		// Run
		double startTime = vm.ElapsedTime();
		vm.yielding = false;
		bool hadRuntimeError = false;
		while (vm.IsRunning && !vm.yielding) {
			if (vm.ElapsedTime() - startTime > timeLimit) break;
			vm.Run(1000);
			if (!vm.Error.IsNull()) {
				Error = vm.Error;
				ReportError(Error);
				hadRuntimeError = true;
				break;
			}
		}

		// Implicit output: if last statement was a bare expression, capture r0.
		// Always update lastImplicitResult (null on error or no implicit output).
		lastImplicitResult = Value.Null;
		Value result;
		if (hasImplicitOutput && !hadRuntimeError) {
			result = vm.GetStackValue(vm.BaseIndex);
			if (!result.IsNull()) {
				lastImplicitResult = result;
				if (implicitOutput != null) {
					implicitOutput.Invoke(StringUtils.Format("{0}", result), true);
				}
			}
		}

		_pendingSource = null;
	}

	// 
	// Report whether the virtual machine is still running, that is,
	// whether it has not yet reached the end of the program code.
	// 
	public bool Running() {
		return vm != null && vm.IsRunning;
	}

	//
	// Report whether the virtual machine is done, that is, whether we have no
	// virtual machine, or we have one and it has reached the end of its code.
	// This is the logical inverse of Running(), provided as a convenience (and
	// for parity with MiniScript 1.x, where host main loops commonly test Done).
	//
	public bool Done() {
		return !Running();
	}

	//
	// Report whether the program in this interpreter called `exit`, and with
	// what result code.  A host main loop polls this after running a slice to
	// decide whether to shut down; a host running a *child* interpreter (an
	// embedded REPL, say) can use it to tell a program that exited from one
	// that simply reached its end.  The state lives on the VM and is cleared
	// whenever a new program is compiled, so this describes the current run.
	//
	public bool ExitRequested() {
		return vm != null && vm.ExitRequested;
	}

	//
	// The result code passed to `exit`, or 0 if the program did not call it.
	//
	public Int32 ExitCode() {
		if (vm == null) return 0;
		return vm.ExitCode;
	}

	//
	// Return whether the parser needs more input, for example because we have
	// run out of source code in the middle of an "if" block.  This is typically
	// used with REPL for making an interactive console, so you can change the
	// prompt when more input is expected.
	// 
	public bool NeedMoreInput() {
		return _pendingSource != null && parser != null && parser.NeedMoreInput();
	}

	// 
	// Get a value from the global namespace of this interpreter.
	//
	// This and SetGlobalValue read and write the same slot, so they agree in every
	// mode.  Both work before the first compile, after the program has ended, and
	// at any call depth -- the namespace does not belong to a running program.
	//
	// <param name="varName">name of global variable to get</param>
	// <returns>Value of the named variable, or Value.Null if not found</returns>
	public Value GetGlobalValue(String varName) {
		Globals globals = GetGlobals();
		Int32 slot = globals.Find(Value.make_string(varName));
		if (slot < 0 || !globals.SlotIsAssigned(slot)) return Value.Null;
		return globals.ValueAtSlot(slot);
	}

	//
	// Set a value in the global namespace of this interpreter, creating the
	// global if it does not exist yet.  See GetGlobalValue.
	//
	// <param name="varName">name of global variable to set</param>
	// <param name="value">value to set</param>
	public void SetGlobalValue(String varName, Value value) {
		Globals globals = GetGlobals();
		globals.SetSlot(globals.Resolve(Value.make_string(varName)), value);
	}

	// 
	// Report an error value to the user via errorOutput.  The default
	// implementation formats the error message as a string and calls
	// ReportError(String).  Subclass and override to do something different
	// (e.g. inspect the error type or store it for later retrieval).
	// 
	// <param name="error">error Value to report</param>
	protected virtual void ReportError(Value error) {
		ReportError(ErrorTypes.DescribeError(error));
	}

	// 
	// Report a single error string to the user via errorOutput.  The default
	// implementation simply invokes errorOutput.  If you want to do something
	// different, subclass Interpreter and override this method.
	// 
	// <param name="message">error message</param>
	protected virtual void ReportError(String message) {
		if (errorOutput != null) errorOutput.Invoke(message, true);
	}
}

}
