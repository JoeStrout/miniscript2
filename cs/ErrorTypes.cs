// ErrorTypes.cs - Static error type values and factory methods for MiniScript errors.
//
// ErrorType.compiler and ErrorType.runtime are prototype error values.
// Errors created via the factory methods have their __isa set to one of these,
// allowing host code to categorize errors by type.

using System;
using static MiniScript.ValueHelpers;
// H: #include "value.h"
// CPP: #include "gc.h"

namespace MiniScript {

public static class ErrorType {
	public static Value compiler = val_null;
	public static Value runtime = val_null;

	// H: static void MarkRoots(void* user_data);

	// Initialize the compiler and runtime prototype error values.
	// Safe to call multiple times (no-op if already initialized).
	// Must be called after gc_init() in C++; in C# this is called lazily.
	public static void Init() {
		// CPP: static bool registered = false; if (!registered) { gc_register_mark_callback(ErrorType::MarkRoots, nullptr); registered = true; }
		if (is_null(compiler)) {
			compiler = make_error(make_string("Compiler Error"), val_null, val_null, val_null);
			freeze_value(compiler);
		}
		if (is_null(runtime)) {
			runtime = make_error(make_string("Runtime Error"), val_null, val_null, val_null);
			freeze_value(runtime);
		}
	}

	// Create a compiler error value with the given message.
	public static Value CompilerError(String msg) {
		if (is_null(compiler)) Init();
		return make_error(make_string(msg), val_null, val_null, compiler);
	}

	// Create a runtime error value with the given message and stack trace.
	public static Value RuntimeError(String msg, Value stack) {
		if (is_null(runtime)) Init();
		return make_error(make_string(msg), val_null, stack, runtime);
	}

	// Create a runtime error value with the given message (no stack trace).
	public static Value RuntimeError(String msg) {
		if (is_null(runtime)) Init();
		return make_error(make_string(msg), val_null, val_null, runtime);
	}

	/*** BEGIN CPP_ONLY ***
	// GC mark callback to protect our static error prototypes from collection.
	void ErrorType::MarkRoots(void* user_data) {
		(void)user_data;
		gc_mark_value(compiler);
		gc_mark_value(runtime);
	}
	*** END CPP_ONLY ***/

}

}
