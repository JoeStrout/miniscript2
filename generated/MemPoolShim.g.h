// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: MemPoolShim.cs

#pragma once
#include "core_includes.h"

namespace MiniScript {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct App;
class AppStorage;
struct Lexer;
class LexerStorage;

// DECLARATIONS






	// Shim class to provide cross-platform pool management.
	// In C#, these are no-ops since we don't use pools.
	// In C++, these map to actual MemPoolManager and StringPool calls.
class MemPoolShim {

		// Get an unused pool number (C++), or 0 (C#)
	public: static Byte GetUnusedPool();
}; // end of struct MemPoolShim










// INLINE METHODS

