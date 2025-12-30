// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: TestFramework.cs

#pragma once
#include "core_includes.h"
// TestFramework.cs
// Simple, transpilable test assertion framework
// Designed to work identically in both C# and transpiled C++


namespace MiniScript {

// FORWARD DECLARATIONS


// DECLARATIONS


class TestFramework {
	public: static Int32 FailCount;
	public: static Int32 PassCount;
	public: static void Reset();
	public: static Boolean Assert(Boolean condition, String message);
	public: static Boolean AssertEqual(Int32 actual, Int32 expected, String context);
	public: static Boolean AssertEqual(UInt32 actual, UInt32 expected, String context);
	public: static Boolean AssertEqual(String actual, String expected, String context);
	public: static Boolean AssertEqual(Boolean actual, Boolean expected, String context);
	public: static void PrintSummary(String suiteName);
	public: static Boolean AllPassed();
}; // end of struct TestFramework


// INLINE METHODS


} // end of namespace MiniScript
