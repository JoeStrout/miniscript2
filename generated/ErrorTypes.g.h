// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: ErrorTypes.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// ErrorTypes.cs - Static error type values and factory methods for MiniScript errors.
// ErrorTypes.compiler and ErrorTypes.runtime are prototype error values.
// Errors created via the factory methods have their __isa set to one of these,
// allowing host code to categorize errors by type.

#include "value.h"
#include "GCManager.g.h"

namespace MiniScript {

// DECLARATIONS

class ErrorTypes {
	public: static Value compiler;
	public: static Value runtime;
	private: static bool _markRootsRegistered;

	// Initialize the compiler and runtime prototype error values.
	// Safe to call multiple times (no-op if already initialized).
	// Must be called after gc_init() in C++; in C# this is called lazily.
	public: static void Init();

	// Create a compiler error value with the given message.
	public: static Value CompilerError(String msg);

	// Create a runtime error value with the given message and stack trace.
	public: static Value RuntimeError(String msg, Value stack);

	// Create a runtime error value with the given message.  Attaches the active
	// VM's current stack trace (via value_current_stack_trace), so every runtime
	// error value -- whether returned by an intrinsic, a core value operation, or
	// the VM itself -- carries an accurate trace.  Returns val_null stack if no
	// VM is running (e.g. errors built during setup, before execution).
	public: static Value RuntimeError(String msg);
	
	// Create a file error value with the given message.
	// All file-I/O failures funnel through here so the error type can be
	// refined later (e.g., give it a dedicated "file.Error" __isa prototype).
	public: static Value FileError(String msg);

	// Create a format error value with the given message.  Used when input text
	// cannot be parsed as expected (e.g. an unparseable date or number) or when
	// a format specifier is invalid.  Funnels through RuntimeError for now, but
	// can later get a dedicated __isa prototype.
	public: static Value FormatError(String msg);

	// Create a parameter type error: an argument was of the wrong type.
	// `expectedType` names the required type or types (e.g. "number" or
	// "list or map"); `actualValue` is the offending argument.  Funnels through
	// RuntimeError for now, but can later get a dedicated __isa prototype.
	public: static Value TypeError(String expectedType, Value actualValue);

	// GC mark callback to protect our static error prototypes from collection.
	public: static void MarkRoots(object user_data);

}; // end of struct ErrorTypes

// INLINE METHODS

} // end of namespace MiniScript
