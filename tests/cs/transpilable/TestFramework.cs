// TestFramework.cs
// Simple, transpilable test assertion framework
// Designed to work identically in both C# and transpiled C++

using System;
// CPP: #include "IOHelper.g.h"
// CPP: #include "value.h"

namespace MiniScript {

public static class TestFramework {
	public static Int32 FailCount = 0;
	public static Int32 PassCount = 0;

	// Reset counters (useful for running multiple test suites)
	public static void Reset() {
		FailCount = 0;
		PassCount = 0;
	}

	// Basic boolean assertion
	public static Boolean Assert(Boolean condition, String message) {
		if (condition) {
			PassCount++;
			return true;
		}
		IOHelper.Print($"FAIL: {message}");
		FailCount++;
		return false;
	}

	// Assert two integers are equal
	public static Boolean AssertEqual(Int32 actual, Int32 expected, String context) {
		if (actual == expected) {
			PassCount++;
			return true;
		}
		IOHelper.Print($"FAIL: {context} - expected {expected}, got {actual}");
		FailCount++;
		return false;
	}

	// Assert two unsigned integers are equal
	public static Boolean AssertEqual(UInt32 actual, UInt32 expected, String context) {
		if (actual == expected) {
			PassCount++;
			return true;
		}
		IOHelper.Print($"FAIL: {context} - expected {expected}, got {actual}");
		FailCount++;
		return false;
	}

	// Assert two strings are equal
	public static Boolean AssertEqual(String actual, String expected, String context) {
		if (actual == expected) {
			PassCount++;
			return true;
		}
		IOHelper.Print($"FAIL: {context} - expected \"{expected}\", got \"{actual}\"");
		FailCount++;
		return false;
	}

	// Assert two booleans are equal
	public static Boolean AssertEqual(Boolean actual, Boolean expected, String context) {
		if (actual == expected) {
			PassCount++;
			return true;
		}
		String expStr = expected ? "true" : "false";
		String actStr = actual ? "true" : "false";
		IOHelper.Print($"FAIL: {context} - expected {expStr}, got {actStr}");
		FailCount++;
		return false;
	}

	// Print summary of test results (only if there were failures)
	public static void PrintSummary(String suiteName) {
		// Silent - failures are already printed
	}

	// Return true if all tests passed
	public static Boolean AllPassed() {
		return FailCount == 0;
	}
}

}
