// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: UnitTests.cs

#pragma once
#include "core_includes.h"
// This module gathers together all the unit tests for this prototype.
// Each test returns true on success, false on failure.


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








class UnitTests {

	public: static Boolean Assert(Boolean condition, String message);
	
	public: static Boolean AssertEqual(String actual, String expected);
		
	public: static Boolean AssertEqual(UInt32 actual, UInt32 expected);
		
	public: static Boolean AssertEqual(Int32 actual, Int32 expected);
		
	public: static Boolean AssertEqual(List<String> actual, List<String> expected);
		
	public: static Boolean TestStringUtils();
	
	public: static Boolean TestDisassembler();
	
	public: static Boolean TestAssembler();

	public: static Boolean TestValueMap();

	public: static Boolean RunAll();
}; // end of struct UnitTests








// INLINE METHODS

} // end of namespace MiniScript
