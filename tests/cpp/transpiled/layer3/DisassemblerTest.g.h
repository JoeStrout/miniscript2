// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: DisassemblerTest.cs

#pragma once
#include "core_includes.h"
// DisassemblerTest.cs
// Tests for Disassembler module (Layer 3)


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
















class DisassemblerTest {

	// Test AssemOp returns correct short mnemonics
	public: static Boolean TestAssemOp();

	// Test ToString formats instructions correctly
	public: static Boolean TestToString();

	// Test Disassemble with a simple function
	public: static Boolean TestDisassembleFunction();

	// Main test runner for this module
	public: static Boolean RunAll();
}; // end of struct DisassemblerTest


// INLINE METHODS

} // end of namespace MiniScript
