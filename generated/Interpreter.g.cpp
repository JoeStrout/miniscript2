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
}
void InterpreterStorage::Reset(List<FuncDef> functions) {
	Interpreter _this(std::static_pointer_cast<InterpreterStorage>(shared_from_this()));
	source = nullptr;
	parser = nullptr;
	compiledFunctions = functions;
	Error = Value::Null;

	// Create and configure VM
	vm =  VM::New();
	vm.SetInterpreter(_this);
	vm.Reset(functions);
}
void InterpreterStorage::Compile() {
	Interpreter _this(std::static_pointer_cast<InterpreterStorage>(shared_from_this()));
	if (!IsNull(vm)) return;		// already compiled

	Error = Value::Null;

	if (IsNull(parser)) parser =  Parser::New();
	parser.Init(source);
	List<ASTNode> statements = parser.ParseProgram();

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

	// Create and configure VM
	vm =  VM::New();
	vm.SetInterpreter(_this);
	vm.Reset(compiledFunctions);
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
	if (IsNull(sourceLine)) return;

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

	// Nothing to do if no statements
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
	ASTNode lastStmt = statements[statements.Count() - 1];
	AssignmentNode asAssign = As<AssignmentNode, AssignmentNodeStorage>(lastStmt);
	IndexedAssignmentNode asIdxAssign = As<IndexedAssignmentNode, IndexedAssignmentNodeStorage>(lastStmt);
	WhileNode asWhile = As<WhileNode, WhileNodeStorage>(lastStmt);
	IfNode asIf = As<IfNode, IfNodeStorage>(lastStmt);
	ForNode asFor = As<ForNode, ForNodeStorage>(lastStmt);
	BreakNode asBreak = As<BreakNode, BreakNodeStorage>(lastStmt);
	ContinueNode asContinue = As<ContinueNode, ContinueNodeStorage>(lastStmt);
	ReturnNode asReturn = As<ReturnNode, ReturnNodeStorage>(lastStmt);
	if (IsNull(asAssign) && IsNull(asIdxAssign)
		&& IsNull(asWhile) && IsNull(asIf) && IsNull(asFor)
		&& IsNull(asBreak) && IsNull(asContinue) && IsNull(asReturn)) {
		hasImplicitOutput = Boolean(true);
	}

	// Compile to bytecode.  Each REPL line is its own @main; previously
	// defined functions are reached as funcref values in the globals VarMap.
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

	// Create/reset VM
	if (IsNull(vm)) vm =  VM::New();
	vm.SetInterpreter(_this);
	vm.Reset(functions, _replGlobals);

	// If this is the first REPL entry, create the initial globals VarMap
	if (_replGlobals.IsNull()) {
		_replGlobals = Value::make_varmap(vm.GetStack(), vm.GetNames(), 0, 
			functions[0].MaxRegs());
		// ToDo: make the transpiler smart enough to do this ---^ on its own
		vm.set_ReplGlobals(_replGlobals);
	}

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
bool InterpreterStorage::NeedMoreInput() {
	return !IsNull(_pendingSource) && !IsNull(parser) && parser.NeedMoreInput();
}
Value InterpreterStorage::GetGlobalValue(String varName) {
	if (IsNull(vm)) return Value::Null;
	// Search the @main frame (base 0) for a register with this name
	Value nameVal = Value::make_string(varName);
	Int32 regCount = !IsNull(vm.CurrentFunction()) ? vm.StackSize() : 0;
	// Look through all named registers at base 0 (the global frame)
	Value name;
	for (Int32 i = 0; i < regCount; i++) {
		name = vm.GetStackName(i);
		if (!name.IsNull() && name == nameVal) {
			return vm.GetStackValue(i);
		}
	}
	return Value::Null;
}
void InterpreterStorage::SetGlobalValue(String varName,Value value) {
	// In REPL mode the persistent globals live in _replGlobals, a VarMap.
	// Setting a key here makes it visible to subsequent user code as a
	// global variable.  If a global VarMap doesn't exist yet (e.g. no REPL
	// entry has run), there is nothing to set.
	if (_replGlobals.IsNull()) return;
	_replGlobals.MapSet(varName, value);
}
void InterpreterStorage::ResetReplGlobals() {
	_replGlobals = Value::Null;
	if (!IsNull(vm)) vm.set_ReplGlobals(Value::Null);
}
void InterpreterStorage::ReportError(Value error) {
	String msg = StringUtils::Format("{0}", Value::error_message(error));
	String prefix = Value::error_isa_contains(error, ErrorTypes::compiler) ? "Compiler Error: " : "Runtime Error: ";
	ReportError(prefix + msg);
}
void InterpreterStorage::ReportError(String message) {
	if (!IsNull(errorOutput)) errorOutput(message, Boolean(true));
}

} // end of namespace MiniScript
