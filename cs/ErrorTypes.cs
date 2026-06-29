// ErrorTypes.cs - Static error type values and factory methods for MiniScript errors.
//
// ErrorTypes.compiler and ErrorTypes.runtime are prototype error values.
// Errors created via the factory methods have their __isa set to one of these,
// allowing host code to categorize errors by type.

using System;
using static MiniScript.Value;
// H: #include "value.h"
// H: #include "GCManager.g.h"
// CPP: #include "CS_value_util.h"

namespace MiniScript {

public static class ErrorTypes {
	public static Value compiler = Value.Null;
	public static Value runtime = Value.Null;

	private static bool _markRootsRegistered = false;

	// Initialize the compiler and runtime prototype error values.
	// Safe to call multiple times (no-op if already initialized).
	// Must be called after gc_init() in C++; in C# this is called lazily.
	public static void Init() {
		if (!_markRootsRegistered) {
			GCManager.RegisterMarkCallback(MarkRoots, null); // CPP: GCManager::RegisterMarkCallback(ErrorTypes::MarkRoots, nullptr);
			_markRootsRegistered = true;
		}
		if (is_null(compiler)) {
			compiler = make_error(make_string("Compiler Error"), Value.Null, Value.Null, Value.Null);
			freeze_value(compiler);
		}
		if (is_null(runtime)) {
			runtime = make_error(make_string("Runtime Error"), Value.Null, Value.Null, Value.Null);
			freeze_value(runtime);
		}
	}

	// Create a compiler error value with the given message.
	public static Value CompilerError(String msg) {
		if (is_null(compiler)) Init();
		return make_error(make_string(msg), Value.Null, Value.Null, compiler);
	}

	// Create a runtime error value with the given message and stack trace.
	public static Value RuntimeError(String msg, Value stack) {
		if (is_null(runtime)) Init();
		return make_error(make_string(msg), Value.Null, stack, runtime);
	}

	// Create a runtime error value with the given message.  Attaches the active
	// VM's current stack trace (via value_current_stack_trace), so every runtime
	// error value -- whether returned by an intrinsic, a core value operation, or
	// the VM itself -- carries an accurate trace.  Returns Value.Null stack if no
	// VM is running (e.g. errors built during setup, before execution).
	public static Value RuntimeError(String msg) {
		if (is_null(runtime)) Init();
		return make_error(make_string(msg), Value.Null, value_current_stack_trace(), runtime);
	}
	
	// Create a file error value with the given message.
	// All file-I/O failures funnel through here so the error type can be
	// refined later (e.g., give it a dedicated "file.Error" __isa prototype).
	public static Value FileError(String msg) {
		return RuntimeError("File error: " + msg);
	}

	// Create a format error value with the given message.  Used when input text
	// cannot be parsed as expected (e.g. an unparseable date or number) or when
	// a format specifier is invalid.  Funnels through RuntimeError for now, but
	// can later get a dedicated __isa prototype.
	public static Value FormatError(String msg) {
		return RuntimeError("Format error: " + msg);
	}

	// Create a parameter type error: an argument was of the wrong type.
	// `expectedType` names the required type or types (e.g. "number" or
	// "list or map"); `actualValue` is the offending argument.  Funnels through
	// RuntimeError for now, but can later get a dedicated __isa prototype.
	public static Value TypeError(String expectedType, Value actualValue) {
		return RuntimeError("Type error: " + expectedType + " required, but got "
			+ value_type_name(actualValue));
	}

	// GC mark callback to protect our static error prototypes from collection.
	public static void MarkRoots(object user_data) {
		GCManager.Mark(compiler);
		GCManager.Mark(runtime);
	}

}

}
