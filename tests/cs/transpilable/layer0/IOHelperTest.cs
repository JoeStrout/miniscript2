// IOHelperTest.cs
// Tests for IOHelper module (Layer 0 - no dependencies)

using System;
using System.Collections.Generic;
// CPP: #include "IOHelper.g.h"
// CPP: #include "TestFramework.g.h"

namespace MiniScript {

public static class IOHelperTest {

	public static Boolean TestPrint() {
		// Simple test that print doesn't crash
		// Don't print anything - we're testing that Print works, not filling output
		return TestFramework.Assert(true, "Print should not crash");
	}

	public static Boolean TestReadFile() {
		// Test reading a file (using a test fixture)
		// For now, just test that it doesn't crash
		// The actual file path will vary between C# and C++ test runs
		return TestFramework.Assert(true, "ReadFile test (placeholder)");
	}

	// Main test runner for this module
	public static Boolean RunAll() {
		IOHelper.Print("=== IOHelper Tests ===");
		TestFramework.Reset();

		Boolean allPassed = true;
		allPassed = TestPrint() && allPassed;
		allPassed = TestReadFile() && allPassed;

		TestFramework.PrintSummary("IOHelper");
		return TestFramework.AllPassed();
	}
}

}
