// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Interpreter.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// Interpreter.cs
// The Interpreter class is the main interface to the MiniScript system.
// You give it some MiniScript source code, and tell it where to send its
// output (via delegate functions called TextOutputMethod).  Then you typically
// call RunUntilDone, which returns when either the script has stopped or the
// given timeout has passed.

#include "IntrinsicAPI.g.h"
#include "VM.g.h"
#include "Parser.g.h"
#include "AST.g.h"
#include "IOHelper.g.h"
#include "Bytecode.g.h"
#include "CodeGenerator.g.h"

namespace MiniScript {
typedef void* object;

// DECLARATIONS

class InterpreterStorage : public std::enable_shared_from_this<InterpreterStorage> {
	friend struct Interpreter;
	public: TextOutputMethod standardOutput;
	public: TextOutputMethod implicitOutput;
	public: TextOutputMethod errorOutput;
	public: object hostData;
	public: VM vm;
	public: String SourceFile = "";
	protected: String source;
	protected: Parser parser;
	protected: List<FuncDef> compiledFunctions;
	public: Value Error;
	public: Value lastImplicitResult = val_null;
	private: String _pendingSource; // accumulated REPL lines so far
	private: Value _replGlobals = val_null; // persistent globals VarMap

	/// <summary>
	/// standardOutput: receives the output of the "print" intrinsic.
	/// </summary>
	
	/// <summary>
	/// implicitOutput: receives the value of expressions entered when
	/// in REPL mode.  If you're not using the REPL() method, you can
	/// safely ignore this.
	/// </summary>

	/// <summary>
	/// errorOutput: receives error messages from the compiler or runtime.
	/// (This happens via the ReportError method, which is virtual; so if you
	/// want to catch the actual errors differently, you can subclass
	/// Interpreter and override that method.)
	/// </summary>

	/// <summary>
	/// hostData is just a convenient place for you to attach some arbitrary
	/// data to the interpreter.  It gets passed through to the context object,
	/// so you can access it inside your custom intrinsic functions.  Use it
	/// for whatever you like (or don't, if you don't feel the need).
	/// </summary>

	/// <summary>
	/// done: returns true when we don't have a virtual machine, or we do have
	/// one and it is done (has reached the end of its code).
	/// </summary>

	/// <summary>
	/// vm: the virtual machine this interpreter is running.  Most applications
	/// will not need to use this, but it's provided for advanced users.
	/// </summary>

	/// <summary>
	/// SourceFile: the name of the file this interpreter loaded (e.g. "myScript.ms"),
	/// or empty string for source provided directly as a string.
	/// Used to populate FuncDef.FileName for stack traces.
	/// </summary>

	/// <summary>
	/// The most recent compiler or runtime error, as an error Value, or val_null
	/// if there is no error.  Host code can inspect this (and its __isa chain)
	/// to distinguish error types.
	/// </summary>

	/// <summary>
	/// The Value produced by the last complete REPL interaction that had implicit
	/// output (a bare expression as the last statement), or val_null otherwise.
	/// Updated at the end of each complete REPL() call.  Host code (e.g. the
	/// REPL loop in App.cs) reads this to push it into the _out history list.
	/// </summary>

	// REPL state

  
	/// <summary>
	/// Constructor taking some MiniScript source code, and the output delegates.
	/// </summary>
	public: InterpreterStorage(String source=nullptr, TextOutputMethod standardOutput=nullptr, TextOutputMethod errorOutput=nullptr);
	
	private: void Init(String _source, TextOutputMethod _standardOutput, TextOutputMethod _errorOutput);

	/// <summary>
	/// Constructor taking source code in the form of a list of strings.
	/// </summary>
	public: InterpreterStorage(List<String> sourceList, TextOutputMethod standardOutput=nullptr, TextOutputMethod errorOutput=nullptr);
	public: virtual ~InterpreterStorage() = default;
	
	

	/// <summary>
	/// Stop the virtual machine, and jump to the end of the program code.
	/// Also reset the parser, in case it's stuck waiting for a block ender.
	/// </summary>
	public: void Stop();

	/// <summary>
	/// Reset the interpreter with the given source code.
	/// </summary>
	public: void Reset(String _source="");

	/// <summary>
	/// Reset the interpreter with pre-compiled functions (e.g. from an assembler).
	/// The list must contain a FuncDef named "@main".
	/// </summary>
	public: void Reset(List<FuncDef> functions);

	/// <summary>
	/// Compile our source code, if we haven't already done so, so that we are
	/// either ready to run, or generate compiler errors (reported via errorOutput).
	/// </summary>
	public: void Compile();

	/// <summary>
	/// Reset the virtual machine to the beginning of the code.  Note that this
	/// does *not* recompile; it simply resets the VM with the same functions.
	/// Useful in cases where you have a short script you want to run over and
	/// over, without recompiling every time.
	/// </summary>
	public: void Restart();

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
	public: void RunUntilDone(double timeLimit=60, bool returnEarly=Boolean(true));

	/// <summary>
	/// Run one step (small batch) of the virtual machine.  This method is not
	/// very useful except in special cases; usually you will use RunUntilDone instead.
	/// </summary>
	public: void Step();

	/// <summary>
	/// Read Eval Print Loop.  Run the given source until it either terminates,
	/// or hits the given time limit.  When it terminates, if we have new
	/// implicit output, print that to the implicitOutput stream.
	/// </summary>
	/// <param name="sourceLine">line of source code to parse and run</param>
	/// <param name="timeLimit">time limit in seconds</param>
	public: void REPL(String sourceLine, double timeLimit=60);

	/// <summary>
	/// Report whether the virtual machine is still running, that is,
	/// whether it has not yet reached the end of the program code.
	/// </summary>
	public: bool Running();

	/// <summary>
	/// Return whether the parser needs more input, for example because we have
	/// run out of source code in the middle of an "if" block.  This is typically
	/// used with REPL for making an interactive console, so you can change the
	/// prompt when more input is expected.
	/// </summary>
	public: bool NeedMoreInput();

	/// <summary>
	/// Get a value from the global namespace of this interpreter.
	/// Searches the @main frame's named registers for the given variable name.
	/// </summary>
	/// <param name="varName">name of global variable to get</param>
	/// <returns>Value of the named variable, or val_null if not found</returns>
	public: Value GetGlobalValue(String varName);

	/// <summary>
	/// Set a value in the global namespace of this interpreter.
	/// Searches the @main frame's named registers and updates the first match.
	/// </summary>
	/// <param name="varName">name of global variable to set</param>
	/// <param name="value">value to set</param>
	public: void SetGlobalValue(String varName, Value value);

	/// <summary>
	/// Discard the persistent REPL globals VarMap.  The next REPL() call will
	/// rebuild it from scratch, effectively clearing all user-defined globals.
	/// Called by the `reset` intrinsic to take effect immediately during execution.
	/// </summary>
	public: void ResetReplGlobals();

	/// <summary>
	/// Report an error value to the user via errorOutput.  The default
	/// implementation formats the error message as a string and calls
	/// ReportError(String).  Subclass and override to do something different
	/// (e.g. inspect the error type or store it for later retrieval).
	/// </summary>
	/// <param name="error">error Value to report</param>
	protected: virtual void ReportError(Value error);

	/// <summary>
	/// Report a single error string to the user via errorOutput.  The default
	/// implementation simply invokes errorOutput.  If you want to do something
	/// different, subclass Interpreter and override this method.
	/// </summary>
	/// <param name="message">error message</param>
	protected: virtual void ReportError(String message);
}; // end of class InterpreterStorage

/// <summary>
/// Interpreter: an object that contains and runs one MiniScript script.
/// </summary>
struct Interpreter {
	friend class InterpreterStorage;
	protected: std::shared_ptr<InterpreterStorage> storage;
  public:
	Interpreter(std::shared_ptr<InterpreterStorage> stor) : storage(stor) {}
	Interpreter() : storage(nullptr) {}
	Interpreter(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const Interpreter& inst) { return inst.storage == nullptr; }
	private: InterpreterStorage* get() const;

	public: TextOutputMethod standardOutput();
	public: void set_standardOutput(TextOutputMethod _v);
	public: TextOutputMethod implicitOutput();
	public: void set_implicitOutput(TextOutputMethod _v);
	public: TextOutputMethod errorOutput();
	public: void set_errorOutput(TextOutputMethod _v);
	public: object hostData();
	public: void set_hostData(object _v);
	public: VM vm();
	public: void set_vm(VM _v);
	public: String SourceFile();
	public: void set_SourceFile(String _v);
	protected: String source();
	protected: void set_source(String _v);
	protected: Parser parser();
	protected: void set_parser(Parser _v);
	protected: List<FuncDef> compiledFunctions();
	protected: void set_compiledFunctions(List<FuncDef> _v);
	public: Value Error();
	public: void set_Error(Value _v);
	public: Value lastImplicitResult();
	public: void set_lastImplicitResult(Value _v);
	private: String _pendingSource(); // accumulated REPL lines so far
	private: void set__pendingSource(String _v); // accumulated REPL lines so far
	private: Value _replGlobals(); // persistent globals VarMap
	private: void set__replGlobals(Value _v); // persistent globals VarMap
	public: Interpreter(InterpreterStorage* p) : storage(p ? p->shared_from_this() : nullptr) {}  

	/// <summary>
	/// standardOutput: receives the output of the "print" intrinsic.
	/// </summary>
	
	/// <summary>
	/// implicitOutput: receives the value of expressions entered when
	/// in REPL mode.  If you're not using the REPL() method, you can
	/// safely ignore this.
	/// </summary>

	/// <summary>
	/// errorOutput: receives error messages from the compiler or runtime.
	/// (This happens via the ReportError method, which is virtual; so if you
	/// want to catch the actual errors differently, you can subclass
	/// Interpreter and override that method.)
	/// </summary>

	/// <summary>
	/// hostData is just a convenient place for you to attach some arbitrary
	/// data to the interpreter.  It gets passed through to the context object,
	/// so you can access it inside your custom intrinsic functions.  Use it
	/// for whatever you like (or don't, if you don't feel the need).
	/// </summary>

	/// <summary>
	/// done: returns true when we don't have a virtual machine, or we do have
	/// one and it is done (has reached the end of its code).
	/// </summary>

	/// <summary>
	/// vm: the virtual machine this interpreter is running.  Most applications
	/// will not need to use this, but it's provided for advanced users.
	/// </summary>

	/// <summary>
	/// SourceFile: the name of the file this interpreter loaded (e.g. "myScript.ms"),
	/// or empty string for source provided directly as a string.
	/// Used to populate FuncDef.FileName for stack traces.
	/// </summary>

	/// <summary>
	/// The most recent compiler or runtime error, as an error Value, or val_null
	/// if there is no error.  Host code can inspect this (and its __isa chain)
	/// to distinguish error types.
	/// </summary>

	/// <summary>
	/// The Value produced by the last complete REPL interaction that had implicit
	/// output (a bare expression as the last statement), or val_null otherwise.
	/// Updated at the end of each complete REPL() call.  Host code (e.g. the
	/// REPL loop in App.cs) reads this to push it into the _out history list.
	/// </summary>

	// REPL state

  
	/// <summary>
	/// Constructor taking some MiniScript source code, and the output delegates.
	/// </summary>
	public: static Interpreter New(String source=nullptr, TextOutputMethod standardOutput=nullptr, TextOutputMethod errorOutput=nullptr) {
		return Interpreter(std::make_shared<InterpreterStorage>(source, standardOutput, errorOutput));
	}
	
	private: inline void Init(String _source, TextOutputMethod _standardOutput, TextOutputMethod _errorOutput);

	/// <summary>
	/// Constructor taking source code in the form of a list of strings.
	/// </summary>
	public: static Interpreter New(List<String> sourceList, TextOutputMethod standardOutput=nullptr, TextOutputMethod errorOutput=nullptr) {
		return Interpreter(std::make_shared<InterpreterStorage>(sourceList, standardOutput, errorOutput));
	}
	public: InterpreterStorage* get_storage() const { return storage.get(); }
	
	

	/// <summary>
	/// Stop the virtual machine, and jump to the end of the program code.
	/// Also reset the parser, in case it's stuck waiting for a block ender.
	/// </summary>
	public: inline void Stop();

	/// <summary>
	/// Reset the interpreter with the given source code.
	/// </summary>
	public: inline void Reset(String _source="");

	/// <summary>
	/// Reset the interpreter with pre-compiled functions (e.g. from an assembler).
	/// The list must contain a FuncDef named "@main".
	/// </summary>
	public: inline void Reset(List<FuncDef> functions);

	/// <summary>
	/// Compile our source code, if we haven't already done so, so that we are
	/// either ready to run, or generate compiler errors (reported via errorOutput).
	/// </summary>
	public: inline void Compile();

	/// <summary>
	/// Reset the virtual machine to the beginning of the code.  Note that this
	/// does *not* recompile; it simply resets the VM with the same functions.
	/// Useful in cases where you have a short script you want to run over and
	/// over, without recompiling every time.
	/// </summary>
	public: inline void Restart();

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
	public: inline void RunUntilDone(double timeLimit=60, bool returnEarly=Boolean(true));

	/// <summary>
	/// Run one step (small batch) of the virtual machine.  This method is not
	/// very useful except in special cases; usually you will use RunUntilDone instead.
	/// </summary>
	public: inline void Step();

	/// <summary>
	/// Read Eval Print Loop.  Run the given source until it either terminates,
	/// or hits the given time limit.  When it terminates, if we have new
	/// implicit output, print that to the implicitOutput stream.
	/// </summary>
	/// <param name="sourceLine">line of source code to parse and run</param>
	/// <param name="timeLimit">time limit in seconds</param>
	public: inline void REPL(String sourceLine, double timeLimit=60);

	/// <summary>
	/// Report whether the virtual machine is still running, that is,
	/// whether it has not yet reached the end of the program code.
	/// </summary>
	public: inline bool Running();

	/// <summary>
	/// Return whether the parser needs more input, for example because we have
	/// run out of source code in the middle of an "if" block.  This is typically
	/// used with REPL for making an interactive console, so you can change the
	/// prompt when more input is expected.
	/// </summary>
	public: inline bool NeedMoreInput();

	/// <summary>
	/// Get a value from the global namespace of this interpreter.
	/// Searches the @main frame's named registers for the given variable name.
	/// </summary>
	/// <param name="varName">name of global variable to get</param>
	/// <returns>Value of the named variable, or val_null if not found</returns>
	public: inline Value GetGlobalValue(String varName);

	/// <summary>
	/// Set a value in the global namespace of this interpreter.
	/// Searches the @main frame's named registers and updates the first match.
	/// </summary>
	/// <param name="varName">name of global variable to set</param>
	/// <param name="value">value to set</param>
	public: inline void SetGlobalValue(String varName, Value value);

	/// <summary>
	/// Discard the persistent REPL globals VarMap.  The next REPL() call will
	/// rebuild it from scratch, effectively clearing all user-defined globals.
	/// Called by the `reset` intrinsic to take effect immediately during execution.
	/// </summary>
	public: inline void ResetReplGlobals();

	/// <summary>
	/// Report an error value to the user via errorOutput.  The default
	/// implementation formats the error message as a string and calls
	/// ReportError(String).  Subclass and override to do something different
	/// (e.g. inspect the error type or store it for later retrieval).
	/// </summary>
	/// <param name="error">error Value to report</param>
	protected: inline virtual void ReportError(Value error);

	/// <summary>
	/// Report a single error string to the user via errorOutput.  The default
	/// implementation simply invokes errorOutput.  If you want to do something
	/// different, subclass Interpreter and override this method.
	/// </summary>
	/// <param name="message">error message</param>
	protected: inline virtual void ReportError(String message);
}; // end of struct Interpreter

// INLINE METHODS

inline InterpreterStorage* Interpreter::get() const { return static_cast<InterpreterStorage*>(storage.get()); }
inline TextOutputMethod Interpreter::standardOutput() { return get()->standardOutput; }
inline void Interpreter::set_standardOutput(TextOutputMethod _v) { get()->standardOutput = _v; }
inline TextOutputMethod Interpreter::implicitOutput() { return get()->implicitOutput; }
inline void Interpreter::set_implicitOutput(TextOutputMethod _v) { get()->implicitOutput = _v; }
inline TextOutputMethod Interpreter::errorOutput() { return get()->errorOutput; }
inline void Interpreter::set_errorOutput(TextOutputMethod _v) { get()->errorOutput = _v; }
inline object Interpreter::hostData() { return get()->hostData; }
inline void Interpreter::set_hostData(object _v) { get()->hostData = _v; }
inline VM Interpreter::vm() { return get()->vm; }
inline void Interpreter::set_vm(VM _v) { get()->vm = _v; }
inline String Interpreter::SourceFile() { return get()->SourceFile; }
inline void Interpreter::set_SourceFile(String _v) { get()->SourceFile = _v; }
inline String Interpreter::source() { return get()->source; }
inline void Interpreter::set_source(String _v) { get()->source = _v; }
inline Parser Interpreter::parser() { return get()->parser; }
inline void Interpreter::set_parser(Parser _v) { get()->parser = _v; }
inline List<FuncDef> Interpreter::compiledFunctions() { return get()->compiledFunctions; }
inline void Interpreter::set_compiledFunctions(List<FuncDef> _v) { get()->compiledFunctions = _v; }
inline Value Interpreter::Error() { return get()->Error; }
inline void Interpreter::set_Error(Value _v) { get()->Error = _v; }
inline Value Interpreter::lastImplicitResult() { return get()->lastImplicitResult; }
inline void Interpreter::set_lastImplicitResult(Value _v) { get()->lastImplicitResult = _v; }
inline String Interpreter::_pendingSource() { return get()->_pendingSource; } // accumulated REPL lines so far
inline void Interpreter::set__pendingSource(String _v) { get()->_pendingSource = _v; } // accumulated REPL lines so far
inline Value Interpreter::_replGlobals() { return get()->_replGlobals; } // persistent globals VarMap
inline void Interpreter::set__replGlobals(Value _v) { get()->_replGlobals = _v; } // persistent globals VarMap
inline void Interpreter::Init(String _source,TextOutputMethod _standardOutput,TextOutputMethod _errorOutput) { return get()->Init(_source, _standardOutput, _errorOutput); }
inline void Interpreter::Stop() { return get()->Stop(); }
inline void Interpreter::Reset(String _source) { return get()->Reset(_source); }
inline void Interpreter::Reset(List<FuncDef> functions) { return get()->Reset(functions); }
inline void Interpreter::Compile() { return get()->Compile(); }
inline void Interpreter::Restart() { return get()->Restart(); }
inline void Interpreter::RunUntilDone(double timeLimit,bool returnEarly) { return get()->RunUntilDone(timeLimit, returnEarly); }
inline void Interpreter::Step() { return get()->Step(); }
inline void Interpreter::REPL(String sourceLine,double timeLimit) { return get()->REPL(sourceLine, timeLimit); }
inline bool Interpreter::Running() { return get()->Running(); }
inline bool Interpreter::NeedMoreInput() { return get()->NeedMoreInput(); }
inline Value Interpreter::GetGlobalValue(String varName) { return get()->GetGlobalValue(varName); }
inline void Interpreter::SetGlobalValue(String varName,Value value) { return get()->SetGlobalValue(varName, value); }
inline void Interpreter::ResetReplGlobals() { return get()->ResetReplGlobals(); }
inline void Interpreter::ReportError(Value error) { return get()->ReportError(error); }
inline void Interpreter::ReportError(String message) { return get()->ReportError(message); }

} // end of namespace MiniScript
