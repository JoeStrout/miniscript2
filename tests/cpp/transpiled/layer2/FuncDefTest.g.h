// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: FuncDefTest.cs

#pragma once
#include "core_includes.h"
// FuncDefTest.cs
// Tests for FuncDef module (Layer 2)


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















class FuncDefTest {

	public: static Boolean TestCreation();

	public: static Boolean TestAddCode();

	public: static Boolean TestAddConstants();

	public: static Boolean TestReserveRegister();

	// Main test runner for this module
	public: static Boolean RunAll();
}; // end of struct FuncDefTest


// INLINE METHODS

} // end of namespace MiniScript
