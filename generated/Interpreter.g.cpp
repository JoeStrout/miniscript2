// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Interpreter.cs

#include "Interpreter.g.h"
#include "StringUtils.g.h"
#include "CS_value_util.h"
#include "CoreIntrinsics.g.h"

namespace MiniScript {

InterpreterStorage::InterpreterStorage(String source,TextOutputMethod standardOutput,TextOutputMethod errorOutput) {
	Init(source, standardOutput, errorOutput);
}
void InterpreterStorage::Init(String _source,TextOutputMethod _standardOutput,TextOutputMethod _errorOutput) {
	source = _source;
	if (IsNull(_standardOutput)) {
		_standardOutput = [](String s, Boolean) { IOHelper::Print(s); };
	}
	if (IsNull(errorOutput)) errorOutput = _standardOutput;
	standardOutput = _standardOutput;
	errorOutput = _errorOutput;
	Error = Value::Null;
	_globals = nullptr;
}
Globals InterpreterStorage::GetGlobals() {
	if (IsNull(_globals)) _globals = Globals::Create();
	return _globals;
}
void InterpreterStorage::ClearGlobals() {
	if (!IsNull(_globals)) _globals.Clear();
}
InterpreterStorage::InterpreterStorage(List<String> sourceList,TextOutputMethod standardOutput,TextOutputMethod errorOutput) {
	String source = String::Join("\n", sourceList);
	Init(source, standardOutput, errorOutput);
}
void InterpreterStorage::Stop() {
	if (!IsNull(vm)) vm.Stop();
	// TODO: if (parser != null) parser.PartialReset();
}
void InterpreterStorage::Reset(String _source) {
	source = _source;
	parser = nullptr;
	vm = nullptr;
	compiledFunctions = nullptr;
	Error = Value::Null;
	// A new program gets a new namespace.  Releasing the old one drops the GC
	// root its map holds; anything still referring to it (a function compiled
	// by the old program and handed to a host, say) keeps it alive normally.
	if (!IsNull(_globals)) {
		_globals.Release();
		_globals = nullptr;
	}
}
void InterpreterStorage::ResetPreservingGlobals(String _source) {
	if (!IsNull(vm)) vm.Stop();
	Globals keptGlobals = GetGlobals();
	_globals = nullptr;         // hide it from Reset, which Releases what it finds
	Reset(_source);
	_globals = keptGlobals;  // still rooted, since Reset never saw it
	Compile();
}
void InterpreterStorage::Reset(List<FuncDef> functions) {
	Interpreter _this(std::static_pointer_cast<InterpreterStorage>(shared_from_this()));
	source = nullptr;
	parser = nullptr;
	compiledFunctions = functions;
	Error = Value::Null;

	// A new program gets a new namespace, as in Reset(String).
	if (!IsNull(_globals)) {
		_globals.Release();
		_globals = nullptr;
	}

	// Create and configure VM
	vm =  VM::New();
	vm.SetInterpreter(_this);
	vm.Reset(functions, GetGlobals());
}
void InterpreterStorage::Compile() {
	Interpreter _this(std::static_pointer_cast<InterpreterStorage>(shared_from_this()));
	if (!IsNull(vm)) return;		// already compiled

	Error = Value::Null;

	if (IsNull(parser)) parser =  Parser::New();
	parser.Init(source, SourceFile);
	List<ASTNode> statements = parser.ParseProgram();
	parser.RequireComplete();   // no more input is coming; this is not a REPL

	if (parser.HadError()) {
		Error = parser.Error();
		ReportError(Error);
		return;
	}

	if (statements.Count() == 0) return;

	// Simplify AST (constant folding, etc.)
	for (Int32 i = 0; i < statements.Count(); i++) {
		statements[i] = statements[i].Simplify();
	}

	// Compile to bytecode (offset past intrinsics so indices don't collide)
	BytecodeEmitter emitter =  BytecodeEmitter::New();
	CodeGenerator generator =  CodeGenerator::New(emitter);
	generator.set_FileName(SourceFile);
	generator.CompileProgram(statements, "@main");

	if (!generator.Error().IsNull()) {
		Error = generator.Error();
		ReportError(Error);
		return;
	}

	compiledFunctions = generator.GetFunctions();

	// Create and configure VM, running in this interpreter's namespace --
	// which already holds anything a host seeded, or (via
	// ResetPreservingGlobals) the outgoing program's globals.
	vm =  VM::New();
	vm.SetInterpreter(_this);
	vm.Reset(compiledFunctions, GetGlobals());
}
FuncDef InterpreterStorage::CompileToFunc(String source,String fileName,Value* error) {
	*error = Value::Null;
	Parser parser =  Parser::New();
	parser.Init(source, fileName);
	List<ASTNode> statements = parser.ParseProgram();
	parser.RequireComplete();   // a module file ends where it ends
	if (parser.HadError()) {
		*error = parser.Error();
		return nullptr;
	}
	// Simplify AST (constant folding, etc.)
	for (Int32 i = 0; i < statements.Count(); i++) {
		statements[i] = statements[i].Simplify();
	}
	BytecodeEmitter emitter =  BytecodeEmitter::New();
	CodeGenerator generator =  CodeGenerator::New(emitter);
	generator.set_FileName(fileName);
	List<FuncDef> functions = generator.CompileImport(statements, fileName);
	if (!generator.Error().IsNull()) {
		*error = generator.Error();
		return nullptr;
	}
	if (functions.Count() == 0) return nullptr;
	return functions[0];   // the module's @main
}
Value InterpreterStorage::RunFunction(Value funcRef,List<Value> args) {
	if (IsNull(vm)) return Value::Null;
	return vm.RunFunction(funcRef, args);
}
void InterpreterStorage::Restart() {
	if (!IsNull(vm) && !IsNull(compiledFunctions)) {
		Error = Value::Null;
		vm.Reset(compiledFunctions);
	}
}
void InterpreterStorage::RunUntilDone(double timeLimit,bool returnEarly) {
	if (IsNull(vm)) {
		Compile();
		if (IsNull(vm)) return;		// (must have been some error)
	}
	double startTime = vm.ElapsedTime();
	vm.set_yielding(Boolean(false));
	while (vm.IsRunning() && !vm.yielding()) {
		if (vm.ElapsedTime() - startTime > timeLimit) return;	// time's up for now
		vm.Run(1000);	// run in small batches so we can check the time
		if (!vm.Error().IsNull()) {
			Error = vm.Error();
			ReportError(Error);
			Stop();
			return;
		}
		if (returnEarly && vm.yielding()) return;		// waiting for something
	}
}
void InterpreterStorage::Step() {
	Compile();
	if (IsNull(vm)) return;
	vm.Run(1);
	if (!vm.Error().IsNull()) {
		Error = vm.Error();
		ReportError(Error);
		Stop();
	}
}
void InterpreterStorage::REPL(String sourceLine,double timeLimit) {
	Interpreter _this(std::static_pointer_cast<InterpreterStorage>(shared_from_this()));
	// An empty line is not nothing: with no VM yet it is how a host asks for
	// one, so that globals can be seeded before any user code runs (see the
	// empty-statements case below).  Null is treated the same, and must be:
	// the C++ port represents an empty string AS null (CS_String.cpp), so
	// there a caller passing "" arrives here indistinguishable from null,
	// and bailing out would make that bootstrap impossible on that side.
	// Parsing empty source is a path Compile() already relies on.
	if (IsNull(sourceLine)) sourceLine = "";

	// Accumulate source lines
	if (IsNull(_pendingSource)) {
		_pendingSource = sourceLine;
	} else {
		_pendingSource = _pendingSource + "\n" + sourceLine;
	}

	// Try to parse
	Error = Value::Null;
	if (IsNull(parser)) parser =  Parser::New();
	parser.Init(_pendingSource);
	List<ASTNode> statements = parser.ParseProgram();

	// If parser needs more input, return and wait for next line
	if (parser.NeedMoreInput()) return;

	// If there were parse errors, report and reset
	if (parser.HadError()) {
		Error = parser.Error();
		ReportError(Error);
		_pendingSource = nullptr;
		return;
	}

	// Nothing to do if there are no statements.  (This used to make an
	// exception for REPL("") with no VM yet, because seeding globals needed a
	// compile to have created somewhere to put them.  SetGlobalValue now works
	// before the first compile, so that bootstrap is gone.)
	if (statements.Count() == 0) {
		_pendingSource = nullptr;
		return;
	}

	// Simplify AST
	for (Int32 i = 0; i < statements.Count(); i++) {
		statements[i] = statements[i].Simplify();
	}

	// Detect implicit output: last statement is a bare expression
	// (not an assignment, block statement, break, continue, or return)
	Boolean hasImplicitOutput = Boolean(false);
	if (statements.Count() > 0) {
		ASTNode lastStmt = statements[statements.Count() - 1];
		hasImplicitOutput = !lastStmt.IsStatement();
	}

	// Compile to bytecode.  Each REPL line is its own @main; previously
	// defined functions are reached as funcref values in the globals table.
	BytecodeEmitter emitter =  BytecodeEmitter::New();
	CodeGenerator generator =  CodeGenerator::New(emitter);
	generator.CompileProgram(statements, "@main");

	if (!generator.Error().IsNull()) {
		Error = generator.Error();
		ReportError(Error);
		_pendingSource = nullptr;
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
	if (IsNull(vm)) vm =  VM::New();
	vm.SetInterpreter(_this);
	vm.Reset(functions, GetGlobals());

	// Run
	double startTime = vm.ElapsedTime();
	vm.set_yielding(Boolean(false));
	bool hadRuntimeError = Boolean(false);
	while (vm.IsRunning() && !vm.yielding()) {
		if (vm.ElapsedTime() - startTime > timeLimit) break;
		vm.Run(1000);
		if (!vm.Error().IsNull()) {
			Error = vm.Error();
			ReportError(Error);
			hadRuntimeError = Boolean(true);
			break;
		}
	}

	// Implicit output: if last statement was a bare expression, capture r0.
	// Always update lastImplicitResult (null on error or no implicit output).
	lastImplicitResult = Value::Null;
	Value result;
	if (hasImplicitOutput && !hadRuntimeError) {
		result = vm.GetStackValue(vm.BaseIndex());
		if (!result.IsNull()) {
			lastImplicitResult = result;
			if (!IsNull(implicitOutput)) {
				implicitOutput(StringUtils::Format("{0}", result), Boolean(true));
			}
		}
	}

	_pendingSource = nullptr;
}
bool InterpreterStorage::Running() {
	return !IsNull(vm) && vm.IsRunning();
}
bool InterpreterStorage::Done() {
	return !Running();
}
bool InterpreterStorage::ExitRequested() {
	return !IsNull(vm) && vm.ExitRequested();
}
Int32 InterpreterStorage::ExitCode() {
	if (IsNull(vm)) return 0;
	return vm.ExitCode();
}
bool InterpreterStorage::NeedMoreInput() {
	return !IsNull(_pendingSource) && !IsNull(parser) && parser.NeedMoreInput();
}
Value InterpreterStorage::GetGlobalValue(String varName) {
	Globals globals = GetGlobals();
	Int32 slot = globals.Find(Value::make_string(varName));
	if (slot < 0 || !globals.SlotIsAssigned(slot)) return Value::Null;
	return globals.ValueAtSlot(slot);
}
void InterpreterStorage::SetGlobalValue(String varName,Value value) {
	Globals globals = GetGlobals();
	globals.SetSlot(globals.Resolve(Value::make_string(varName)), value);
}
void InterpreterStorage::ReportError(Value error) {
	ReportError(ErrorTypes::DescribeError(error));
}
void InterpreterStorage::ReportError(String message) {
	if (!IsNull(errorOutput)) errorOutput(message, Boolean(true));
}

} // end of namespace MiniScript
