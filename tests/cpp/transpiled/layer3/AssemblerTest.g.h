// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: AssemblerTest.cs

#pragma once
#include "core_includes.h"
// AssemblerTest.cs
// Tests for Assembler module (Layer 3)


namespace MiniScript {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
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















class AssemblerTest {

	// Test Assembler creation and initial state
	public: static Boolean TestCreation();

	// Test GetTokens static method
	public: static Boolean TestGetTokens();

	// Test assembling simple instructions
	public: static Boolean TestAddLine();

	// Test assembling with constants (strings, floats)
	public: static Boolean TestConstants();

	// Test error handling
	public: static Boolean TestErrorHandling();

	// Main test runner for this module
	public: static Boolean RunAll();
}; // end of struct AssemblerTest




// INLINE METHODS

} // end of namespace MiniScript
