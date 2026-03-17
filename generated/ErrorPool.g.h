// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: ErrorPool.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// ErrorPool.cs - Simple error collection for MiniScript compilation and execution.
// Shared across lexer, parser, code generator, and VM stages.

namespace MiniScript {

// DECLARATIONS

struct ErrorPool {
	private: List<String> _errors;

	// Use Create() to get an initialized ErrorPool before copying it.
	// This ensures all copies share the same underlying list.
	public: static ErrorPool Create();

	private: void EnsureList();

	public: void Add(String message);

	public: Boolean HasError();

	public: String TopError();

	public: List<String> GetErrors();

	public: void Clear();
}; // end of struct ErrorPool

// INLINE METHODS

} // end of namespace MiniScript
