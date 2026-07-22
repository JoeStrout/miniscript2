// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: ErrorTypes.cs

#include "ErrorTypes.g.h"
#include "CS_value_util.h"
#include "StringUtils.g.h"

namespace MiniScript {

Value ErrorTypes::compiler = Value::Null;
Value ErrorTypes::runtime = Value::Null;
bool ErrorTypes::_markRootsRegistered = Boolean(false);
void ErrorTypes::Init() {
	if (!_markRootsRegistered) {
		GCManager::RegisterMarkCallback(ErrorTypes::MarkRoots, nullptr);
		_markRootsRegistered = Boolean(true);
	}
	if (compiler.IsNull()) {
		compiler = Value::make_error(Value::make_string("Compiler Error"), Value::Null, Value::Null, Value::Null);
		compiler.Freeze();
	}
	if (runtime.IsNull()) {
		runtime = Value::make_error(Value::make_string("Runtime Error"), Value::Null, Value::Null, Value::Null);
		runtime.Freeze();
	}
}
Value ErrorTypes::CompilerError(String msg) {
	if (compiler.IsNull()) Init();
	return Value::make_error(Value::make_string(msg), Value::Null, Value::Null, compiler);
}
Value ErrorTypes::RuntimeError(String msg,Value stack) {
	if (runtime.IsNull()) Init();
	return Value::make_error(Value::make_string(msg), Value::Null, stack, runtime);
}
Value ErrorTypes::RuntimeError(String msg) {
	if (runtime.IsNull()) Init();
	return Value::make_error(Value::make_string(msg), Value::Null, Value::value_current_stack_trace(), runtime);
}
Value ErrorTypes::FileError(String msg) {
	return RuntimeError("File error: " + msg);
}
Value ErrorTypes::FormatError(String msg) {
	return RuntimeError("Format error: " + msg);
}
Value ErrorTypes::TypeError(String expectedType,Value actualValue) {
	return RuntimeError("Type error: " + expectedType + " required, but got "
		+ actualValue.TypeName());
}
String ErrorTypes::ErrorLocation(Value error) {
	Value stack = error.Stack();
	if (!stack.IsList() || stack.ListCount() == 0) return "";
	String loc = StringUtils::Format("{0}", stack.ListGet(0));
	// Drop the "(current program) " prefix used for the top-level
	// script, leaving just "line N" for the common case.
	String prefix = "(current program) ";
	if (loc.Length() >= prefix.Length() && loc.Left(prefix.Length()) == prefix) {
		loc = loc.Substring(prefix.Length());
	}
	return loc;
}
String ErrorTypes::DescribeError(Value error) {
	String prefix = error.IsaContains(compiler) ? "Compiler Error: " : "Runtime Error: ";
	String msg = StringUtils::Format("{0}", error.Message());
	String loc = ErrorLocation(error);
	if (loc == "") return prefix + msg;
	return StringUtils::Format("{0}{1} [{2}]", prefix, msg, loc);
}
void ErrorTypes::MarkRoots(object user_data) {
	GCManager::Mark(compiler);
	GCManager::Mark(runtime);
}

} // end of namespace MiniScript
