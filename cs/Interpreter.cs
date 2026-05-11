// Interpreter.cs
//
// The Interpreter class is the main interface to the MiniScript system.
// You give it some MiniScript source code, and tell it where to send its
// output (via delegate functions called TextOutputMethod).  Then you typically
// call RunUntilDone, which returns when either the script has stopped or the
// given timeout has passed.

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// H: #include "IntrinsicAPI.g.h"
// H: #include "VM.g.h"
// H: #include "Parser.g.h"
// H: #include "AST.g.h"
// H: #include "IOHelper.g.h"
// H: #include "Bytecode.g.h"
// H: #include "CodeGenerator.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "Intrinsic.g.h" // ToDo: remove this once we've refactored set_FunctionIndexOffset away
// CPP: #include "CoreIntrinsics.g.h"

namespace MiniScript {

// H: typedef void* object;

/// <summary>
/// Interpreter: an object that contains and runs one MiniScript script.
/// </summary>
public class Interpreter {

	/// <summary>
	/// standardOutput: receives the output of the "print" intrinsic.
	/// </summary>
	public TextOutputMethod standardOutput;
	
	/// <summary>
	/// implicitOutput: receives the value of expressions entered when
	/// in REPL mode.  If you're not using the REPL() method, you can
	/// safely ignore this.
	/// </summary>
	public TextOutputMethod implicitOutput;

	/// <summary>
	/// errorOutput: receives error messages from the compiler or runtime.
	/// (This happens via the ReportError method, which is virtual; so if you
	/// want to catch the actual errors differently, you can subclass
	/// Interpreter and override that method.)
	/// </summary>
	public TextOutputMethod errorOutput;

	/// <summary>
	/// hostData is just a convenient place for you to attach some arbitrary
	/// data to the interpreter.  It gets passed through to the context object,
	/// so you can access it inside your custom intrinsic functions.  Use it
	/// for whatever you like (or don't, if you don't feel the need).
	/// </summary>
	public object hostData;

	/// <summary>
	/// done: returns true when we don't have a virtual machine, or we do have
	/// one and it is done (has reached the end of its code).
	/// </summary>
	public bool done {
		get { return vm == null || !vm.IsRunning; }
	}

	/// <summary>
	/// vm: the virtual machine this interpreter is running.  Most applications
	/// will not need to use this, but it's provided for advanced users.
	/// </summary>
	public VM vm;

	/// <summary>
	/// SourceFile: the name of the file this interpreter loaded (e.g. "myScript.ms"),
	/// or empty string for source provided directly as a string.
	/// Used to populate FuncDef.FileName for stack traces.
	/// </summary>
	public String SourceFile = "";

	protected String source;
	protected Parser parser;
	protected List<FuncDef> compiledFunctions;

	/// <summary>
	/// The most recent compiler or runtime error, as an error Value, or val_null
	/// if there is no error.  Host code can inspect this (and its __isa chain)
	/// to distinguish error types.
	/// </summary>
	public Value Error;

	/// <summary>
	/// The Value produced by the last complete REPL interaction that had implicit
	/// output (a bare expression as the last statement), or val_null otherwise.
	/// Updated at the end of each complete REPL() call.  Host code (e.g. the
	/// REPL loop in App.cs) reads this to push it into the _out history list.
	/// </summary>
	public Value lastImplicitResult = val_null;

	// REPL state
	private String _pendingSource;       // accumulated REPL lines so far
	private Value _replGlobals = val_null; // persistent globals VarMap

	// H_WRAPPER: public: Interpreter(InterpreterStorage* p) : storage(p ? p->shared_from_this() : nullptr) {}  
  
	/// <summary>
	/// Constructor taking some MiniScript source code, and the output delegates.
	/// </summary>
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
		Error = val_null;
	}

	/// <summary>
	/// Constructor taking source code in the form of a list of strings.
	/// </summary>
	public Interpreter(List<String> sourceList, TextOutputMethod standardOutput=null, TextOutputMethod errorOutput=null) {
		String source = String.Join("\n", sourceList);
		Init(source, standardOutput, errorOutput);
	}
	
	//*** BEGIN CS_ONLY ***
	/// <summary>
	/// Constructor taking source code in the form of a string array.
	/// </summary>
	public Interpreter(String[] sourceArray, TextOutputMethod standardOutput=null, TextOutputMethod errorOutput=null) {
		String source = String.Join("\n", sourceArray);
		Init(source, standardOutput, errorOutput);
	}
	//*** END CS_ONLY ***
	
	// H: public: virtual ~InterpreterStorage() = default;

	// H_WRAPPER: public: InterpreterStorage* get_storage() const { return storage.get(); }

	/// <summary>
	/// Stop the virtual machine, and jump to the end of the program code.
	/// Also reset the parser, in case it's stuck waiting for a block ender.
	/// </summary>
	public void Stop() {
		if (vm != null) vm.Stop();
		// TODO: if (parser != null) parser.PartialReset();
	}

	/// <summary>
	/// Reset the interpreter with the given source code.
	/// </summary>
	public void Reset(String _source="") {
		source = _source;
		parser = null;
		vm = null;
		compiledFunctions = null;
		Error = val_null;
	}

	/// <summary>
	/// Reset the interpreter with pre-compiled functions (e.g. from an assembler).
	/// The list must contain a FuncDef named "@main".
	/// </summary>
	public void Reset(List<FuncDef> functions) {
		source = null;
		parser = null;
		compiledFunctions = functions;
		Error = val_null;

		// Create and configure VM
		vm = new VM();
		vm.SetInterpreter(this);
		vm.Reset(functions);
	}

	/// <summary>
	/// Compile our source code, if we haven't already done so, so that we are
	/// either ready to run, or generate compiler errors (reported via errorOutput).
	/// </summary>
	public void Compile() {
		if (vm != null) return;		// already compiled

		Error = val_null;

		if (parser == null) parser = new Parser();
		parser.Init(source);
		List<ASTNode> statements = parser.ParseProgram();

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
		generator.FunctionIndexOffset = Intrinsic.Count();
		generator.FileName = SourceFile;
		generator.CompileProgram(statements, "@main");

		if (!is_null(generator.Error)) {
			Error = generator.Error;
			ReportError(Error);
			return;
		}

		compiledFunctions = generator.GetFunctions();

		// Create and configure VM
		vm = new VM();
		vm.SetInterpreter(this);
		vm.Reset(compiledFunctions);
	}

	/// <summary>
	/// Reset the virtual machine to the beginning of the code.  Note that this
	/// does *not* recompile; it simply resets the VM with the same functions.
	/// Useful in cases where you have a short script you want to run over and
	/// over, without recompiling every time.
	/// </summary>
	public void Restart() {
		if (vm != null && compiledFunctions != null) {
			Error = val_null;
			vm.Reset(compiledFunctions);
		}
	}

	/// <summary>
	/// Run the compiled code until we either reach the end, or we reach the
	/// specified time limit.  In the latter case, you can then call RunUntilDone
	/// again to continue execution right from where it left off.
	///
	/// Or, if returnEarly is true, we will also return if the VM is yielding
	/// (i.e., an intrinsic needs to wait for something).  Again, call
	/// RunUntilDone again later to continue.
	///
	/// Note that this method first compiles the source code if it wasn't compiled
	/// already, and in that case, may generate compiler errors.  And of course
	/// it may generate runtime errors while running.  In either case, these are
	/// reported via errorOutput.
	/// </summary>
	/// <param name="timeLimit">maximum amount of time to run before returning, in seconds</param>
	/// <param name="returnEarly">if true, return as soon as the VM yields</param>
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
			if (!is_null(vm.Error)) {
				Error = vm.Error;
				ReportError(Error);
				Stop();
				return;
			}
			if (returnEarly && vm.yielding) return;		// waiting for something
		}
	}

	/// <summary>
	/// Run one step (small batch) of the virtual machine.  This method is not
	/// very useful except in special cases; usually you will use RunUntilDone instead.
	/// </summary>
	public void Step() {
		Compile();
		if (vm == null) return;
		vm.Run(1);
		if (!is_null(vm.Error)) {
			Error = vm.Error;
			ReportError(Error);
			Stop();
		}
	}

	/// <summary>
	/// Read Eval Print Loop.  Run the given source until it either terminates,
	/// or hits the given time limit.  When it terminates, if we have new
	/// implicit output, print that to the implicitOutput stream.
	/// </summary>
	/// <param name="sourceLine">line of source code to parse and run</param>
	/// <param name="timeLimit">time limit in seconds</param>
	public void REPL(String sourceLine, double timeLimit=60) {
		if (sourceLine == null) return;

		// Accumulate source lines
		if (_pendingSource == null) {
			_pendingSource = sourceLine;
		} else {
			_pendingSource = _pendingSource + "\n" + sourceLine;
		}

		// Try to parse
		Error = val_null;
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

		// Nothing to do if no statements
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
		ASTNode lastStmt = statements[statements.Count - 1];
		AssignmentNode asAssign = lastStmt as AssignmentNode;
		IndexedAssignmentNode asIdxAssign = lastStmt as IndexedAssignmentNode;
		WhileNode asWhile = lastStmt as WhileNode;
		IfNode asIf = lastStmt as IfNode;
		ForNode asFor = lastStmt as ForNode;
		BreakNode asBreak = lastStmt as BreakNode;
		ContinueNode asContinue = lastStmt as ContinueNode;
		ReturnNode asReturn = lastStmt as ReturnNode;
		if (asAssign == null && asIdxAssign == null
			&& asWhile == null && asIf == null && asFor == null
			&& asBreak == null && asContinue == null && asReturn == null) {
			hasImplicitOutput = true;
		}

		// Compile to bytecode (offset past intrinsics + previous user functions)
		Int32 funcOffset = (vm != null) ? vm.FunctionCount() : Intrinsic.Count();
		BytecodeEmitter emitter = new BytecodeEmitter();
		CodeGenerator generator = new CodeGenerator(emitter);
		generator.FunctionIndexOffset = funcOffset;
		generator.CompileProgram(statements, "@main");

		if (!is_null(generator.Error)) {
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

		// Create/reset VM
		if (vm == null) vm = new VM();
		vm.SetInterpreter(this);
		vm.Reset(functions, _replGlobals);

		// If this is the first REPL entry, create the initial globals VarMap
		if (is_null(_replGlobals)) {
			_replGlobals = make_varmap(vm.GetStack(), vm.GetNames(), 0, functions[0].MaxRegs); // CPP: _replGlobals = make_varmap(&vm.GetStack()[0], &vm.GetNames()[0], 0, functions[0].MaxRegs());
			vm.ReplGlobals = _replGlobals;
		}

		// Run
		double startTime = vm.ElapsedTime();
		vm.yielding = false;
		bool hadRuntimeError = false;
		while (vm.IsRunning && !vm.yielding) {
			if (vm.ElapsedTime() - startTime > timeLimit) break;
			vm.Run(1000);
			if (!is_null(vm.Error)) {
				Error = vm.Error;
				ReportError(Error);
				hadRuntimeError = true;
				break;
			}
		}

		// Implicit output: if last statement was a bare expression, capture r0.
		// Always update lastImplicitResult (null on error or no implicit output).
		lastImplicitResult = val_null;
		Value result;
		if (hasImplicitOutput && !hadRuntimeError) {
			result = vm.GetStackValue(vm.BaseIndex);
			if (!is_null(result)) {
				lastImplicitResult = result;
				if (implicitOutput != null) {
					implicitOutput.Invoke(StringUtils.Format("{0}", result), true);
				}
			}
		}

		_pendingSource = null;
	}

	/// <summary>
	/// Report whether the virtual machine is still running, that is,
	/// whether it has not yet reached the end of the program code.
	/// </summary>
	public bool Running() {
		return vm != null && vm.IsRunning;
	}

	/// <summary>
	/// Return whether the parser needs more input, for example because we have
	/// run out of source code in the middle of an "if" block.  This is typically
	/// used with REPL for making an interactive console, so you can change the
	/// prompt when more input is expected.
	/// </summary>
	public bool NeedMoreInput() {
		return _pendingSource != null && parser != null && parser.NeedMoreInput();
	}

	/// <summary>
	/// Get a value from the global namespace of this interpreter.
	/// Searches the @main frame's named registers for the given variable name.
	/// </summary>
	/// <param name="varName">name of global variable to get</param>
	/// <returns>Value of the named variable, or val_null if not found</returns>
	public Value GetGlobalValue(String varName) {
		if (vm == null) return val_null;
		// Search the @main frame (base 0) for a register with this name
		Value nameVal = make_string(varName);
		Int32 regCount = vm.CurrentFunction != null ? vm.StackSize() : 0;
		// Look through all named registers at base 0 (the global frame)
		Value name;
		for (Int32 i = 0; i < regCount; i++) {
			name = vm.GetStackName(i);
			if (!is_null(name) && value_equal(name, nameVal)) {
				return vm.GetStackValue(i);
			}
		}
		return val_null;
	}

	/// <summary>
	/// Set a value in the global namespace of this interpreter.
	/// Searches the @main frame's named registers and updates the first match.
	/// </summary>
	/// <param name="varName">name of global variable to set</param>
	/// <param name="value">value to set</param>
	public void SetGlobalValue(String varName, Value value) {
		// TODO: Implement when VM supports setting stack values by index.
		// The current VM API only provides read access to the stack.
	}

	/// <summary>
	/// Discard the persistent REPL globals VarMap.  The next REPL() call will
	/// rebuild it from scratch, effectively clearing all user-defined globals.
	/// Called by the `reset` intrinsic to take effect immediately during execution.
	/// </summary>
	public void ResetReplGlobals() {
		_replGlobals = val_null;
		if (vm != null) vm.ReplGlobals = val_null;
	}

	/// <summary>
	/// Report an error value to the user via errorOutput.  The default
	/// implementation formats the error message as a string and calls
	/// ReportError(String).  Subclass and override to do something different
	/// (e.g. inspect the error type or store it for later retrieval).
	/// </summary>
	/// <param name="error">error Value to report</param>
	protected virtual void ReportError(Value error) {
		String msg = StringUtils.Format("{0}", error_message(error));
		String prefix = error_isa_contains(error, ErrorType.compiler) ? "Compiler Error: " : "Runtime Error: ";
		ReportError(prefix + msg);
	}

	/// <summary>
	/// Report a single error string to the user via errorOutput.  The default
	/// implementation simply invokes errorOutput.  If you want to do something
	/// different, subclass Interpreter and override this method.
	/// </summary>
	/// <param name="message">error message</param>
	protected virtual void ReportError(String message) {
		if (errorOutput != null) errorOutput.Invoke(message, true);
	}
}

}
