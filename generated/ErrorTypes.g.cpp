// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: ErrorTypes.cs

#include "ErrorTypes.g.h"
#include "CS_value_util.h"

namespace MiniScript {

Value ErrorTypes::compiler = Value::Null;
Value ErrorTypes::runtime = Value::Null;
bool ErrorTypes::_markRootsRegistered = Boolean(false);
void ErrorTypes::Init() {
	if (!_markRootsRegistered) {
		GCManager::RegisterMarkCallback(ErrorTypes::MarkRoots, nullptr);
		_markRootsRegistered = Boolean(true);
	}
	if (is_null(compiler)) {
		compiler = make_error(make_string("Compiler Error"), Value::Null, Value::Null, Value::Null);
		freeze_value(compiler);
	}
	if (is_null(runtime)) {
		runtime = make_error(make_string("Runtime Error"), Value::Null, Value::Null, Value::Null);
		freeze_value(runtime);
	}
}
Value ErrorTypes::CompilerError(String msg) {
	if (is_null(compiler)) Init();
	return make_error(make_string(msg), Value::Null, Value::Null, compiler);
}
Value ErrorTypes::RuntimeError(String msg,Value stack) {
	if (is_null(runtime)) Init();
	return make_error(make_string(msg), Value::Null, stack, runtime);
}
Value ErrorTypes::RuntimeError(String msg) {
	if (is_null(runtime)) Init();
	return make_error(make_string(msg), Value::Null, value_current_stack_trace(), runtime);
}
Value ErrorTypes::FileError(String msg) {
	return RuntimeError("File error: " + msg);
}
Value ErrorTypes::FormatError(String msg) {
	return RuntimeError("Format error: " + msg);
}
Value ErrorTypes::TypeError(String expectedType,Value actualValue) {
	return RuntimeError("Type error: " + expectedType + " required, but got "
		+ value_type_name(actualValue));
}
void ErrorTypes::MarkRoots(object user_data) {
	GCManager::Mark(compiler);
	GCManager::Mark(runtime);
}

} // end of namespace MiniScript
