// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: ErrorTypes.cs

#include "ErrorTypes.g.h"

namespace MiniScript {

Value ErrorType::compiler = val_null;
Value ErrorType::runtime = val_null;
void ErrorType::Init() {
	static bool registered = false; if (!registered) { GCManager::Instance().RegisterMarkCallback(ErrorType::MarkRoots, nullptr); registered = true; }
	if (is_null(compiler)) {
		compiler = make_error(make_string("Compiler Error"), val_null, val_null, val_null);
		freeze_value(compiler);
	}
	if (is_null(runtime)) {
		runtime = make_error(make_string("Runtime Error"), val_null, val_null, val_null);
		freeze_value(runtime);
	}
}
Value ErrorType::CompilerError(String msg) {
	if (is_null(compiler)) Init();
	return make_error(make_string(msg), val_null, val_null, compiler);
}
Value ErrorType::RuntimeError(String msg,Value stack) {
	if (is_null(runtime)) Init();
	return make_error(make_string(msg), val_null, stack, runtime);
}
Value ErrorType::RuntimeError(String msg) {
	if (is_null(runtime)) Init();
	return make_error(make_string(msg), val_null, val_null, runtime);
}
// GC mark callback to protect our static error prototypes from collection.
void ErrorType::MarkRoots(void* user_data, GCManager& gc) {
	(void)user_data;
	gc.Mark(compiler);
	gc.Mark(runtime);
}

} // end of namespace MiniScript
