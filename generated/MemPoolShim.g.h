// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: MemPoolShim.cs

#pragma once
#include "core_includes.h"
// This file is obsolete; we should no longer need MemPool or its shim.
// ToDo: remove this.


namespace MiniScript {

// FORWARD DECLARATIONS

struct VM;
class VMStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct App;
class AppStorage;

// DECLARATIONS







// Shim class to provide cross-platform pool management.
// In C#, these are no-ops since we don't use pools.
// In C++, these map to actual MemPoolManager and StringPool calls.
class MemPoolShim {

	// Get an unused pool number (C++), or 0 (C#)
	public: static Byte GetUnusedPool();

	// Get the current default string pool
	public: static Byte GetDefaultStringPool();

	// Set the default string pool
	public: static void SetDefaultStringPool(Byte poolNum);

	// Clear a string pool
	public: static void ClearStringPool(Byte poolNum);

	// Intern a string in the current default pool (C++), or just return it (C#)
	public: static String InternString(String str);
}; // end of struct MemPoolShim









// INLINE METHODS

} // end of namespace MiniScript
