// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: ErrorTypes.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// ErrorTypes.cs - Static error type values and factory methods for MiniScript errors.
// ErrorType.compiler and ErrorType.runtime are prototype error values.
// Errors created via the factory methods have their __isa set to one of these,
// allowing host code to categorize errors by type.

#include "value.h"
#include "GCManager.g.h"

namespace MiniScript {

// DECLARATIONS

class ErrorType {
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

	// Create a runtime error value with the given message (no stack trace).
	public: static Value RuntimeError(String msg);

	// GC mark callback to protect our static error prototypes from collection.
	public: static void MarkRoots(object user_data);

}; // end of struct ErrorType

// INLINE METHODS

} // end of namespace MiniScript
