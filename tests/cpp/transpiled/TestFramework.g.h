// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: TestFramework.cs

#pragma once
#include "core_includes.h"
// TestFramework.cs
// Simple, transpilable test assertion framework
// Designed to work identically in both C# and transpiled C++


namespace MiniScript {

// FORWARD DECLARATIONS

struct String;
class StringStorage;
struct List;
class ListStorage;

// DECLARATIONS




class TestFramework {
	public: static Int32 FailCount;
	public: static Int32 PassCount;

	// Reset counters (useful for running multiple test suites)
	public: static void Reset();

	// Basic boolean assertion
	public: static Boolean Assert(Boolean condition, String message);

	// Assert two integers are equal
	public: static Boolean AssertEqual(Int32 actual, Int32 expected, String context);

	// Assert two unsigned integers are equal
	public: static Boolean AssertEqual(UInt32 actual, UInt32 expected, String context);

	// Assert two strings are equal
	public: static Boolean AssertEqual(String actual, String expected, String context);

	// Assert two booleans are equal
	public: static Boolean AssertEqual(Boolean actual, Boolean expected, String context);

	// Assert C string actual vs String expected (C++ only - prevents ambiguity with Boolean overload)
	public: static Boolean AssertEqual(const char* actual, String expected, String context) {
		return AssertEqual(String(actual), expected, context);
	}

	// Print summary of test results (only if there were failures)
	public: static void PrintSummary(String suiteName);

	// Return true if all tests passed
	public: static Boolean AllPassed();
}; // end of struct TestFramework


// INLINE METHODS

} // end of namespace MiniScript
