// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VMTest.cs

#pragma once
#include "core_includes.h"
// VMTest.cs
// Tests for VM module (Layer 4)


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















class VMTest {

	// Helper to assemble source lines and return functions list
	private: static List<FuncDef> AssembleProgram(List<String> sourceLines);

	// Test VM creation and initial state
	public: static Boolean TestCreation();

	// Test simple LOAD and RETURN
	public: static Boolean TestLoadAndReturn();

	// Test basic arithmetic operations
	public: static Boolean TestArithmetic();

	// Test subtraction
	public: static Boolean TestSubtraction();

	// Test multiplication
	public: static Boolean TestMultiplication();

	// Test simple conditional jump
	public: static Boolean TestConditionalJump();

	// Test simple loop (sum 1 to 5)
	public: static Boolean TestSimpleLoop();

	// Main test runner for this module
	public: static Boolean RunAll();
}; // end of struct VMTest



// INLINE METHODS

} // end of namespace MiniScript
