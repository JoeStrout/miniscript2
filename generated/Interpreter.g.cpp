// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Interpreter.cs

#include "Interpreter.g.h"

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
	errors = ErrorPool::Create();
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
	errors.Clear();
}
void InterpreterStorage::Compile() {
	Interpreter _this(std::static_pointer_cast<InterpreterStorage>(shared_from_this()));
	if (!IsNull(vm)) return;		// already compiled

	errors.Clear();

	if (IsNull(parser)) parser =  Parser::New();
	parser.set_Errors(errors);
	parser.Init(source);
	List<ASTNode> statements = parser.ParseProgram();

	if (parser.HadError()) {
		ReportErrors();
		return;
	}

	if (statements.Count() == 0) return;

	// Simplify AST (constant folding, etc.)
	for (Int32 i = 0; i < statements.Count(); i++) {
		statements[i] = statements[i].Simplify();
	}

	// Compile to bytecode
	BytecodeEmitter emitter =  BytecodeEmitter::New();
	CodeGenerator generator =  CodeGenerator::New(emitter);
	generator.set_Errors(errors);
	generator.CompileProgram(statements, "@main");

	if (errors.HasError()) {
		ReportErrors();
		return;
	}

	compiledFunctions = generator.GetFunctions();

	// Create and configure VM
	vm =  VM::New();
	vm.set_Errors(errors);
	vm.SetInterpreter(_this);
	vm.Reset(compiledFunctions);

	if (errors.HasError()) {
		ReportErrors();
		vm = nullptr;
	}
}
void InterpreterStorage::Restart() {
	if (!IsNull(vm) && !IsNull(compiledFunctions)) {
		errors.Clear();
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
		if (errors.HasError()) {
			ReportErrors();
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
	if (errors.HasError()) {
		ReportErrors();
		Stop();
	}
}
void InterpreterStorage::REPL(String sourceLine,double timeLimit) {
	// TODO: Implement incremental parsing and REPL support.
	// For now, this is a stub that compiles and runs a complete program
	// each time.  A proper implementation will accumulate lines,
	// support NeedMoreInput, and track implicit results.
	if (IsNull(sourceLine)) return;

	Reset(sourceLine);
	RunUntilDone(timeLimit, Boolean(true));
}
bool InterpreterStorage::Running() {
	return !IsNull(vm) && vm.IsRunning();
}
bool InterpreterStorage::NeedMoreInput() {
	// TODO: Implement when incremental parsing is added.
	return Boolean(false);
}
Value InterpreterStorage::GetGlobalValue(String varName) {
	if (IsNull(vm)) return val_null;
	// Search the @main frame (base 0) for a register with this name
	GC_PUSH_SCOPE();
	Value nameVal = make_string(varName); GC_PROTECT(&nameVal);
	Int32 regCount = !IsNull(vm.CurrentFunction()) ? vm.StackSize() : 0;
	// Look through all named registers at base 0 (the global frame)
	Value name; GC_PROTECT(&name);
	for (Int32 i = 0; i < regCount; i++) {
		name = vm.GetStackName(i);
		if (!is_null(name) && value_equal(name, nameVal)) {
			GC_POP_SCOPE();
			return vm.GetStackValue(i);
		}
	}
	GC_POP_SCOPE();
	return val_null;
}
void InterpreterStorage::SetGlobalValue(String varName,Value value) {
	// TODO: Implement when VM supports setting stack values by index.
	// The current VM API only provides read access to the stack.
}
void InterpreterStorage::ReportErrors() {
	if (IsNull(errorOutput)) return;
	List<String> errorList = errors.GetErrors();
	for (Int32 i = 0; i < errorList.Count(); i++) {
		ReportError(errorList[i]);
	}
	errors.Clear();
}
void InterpreterStorage::ReportError(String message) {
	if (!IsNull(errorOutput)) errorOutput(message, Boolean(true));
}

} // end of namespace MiniScript
