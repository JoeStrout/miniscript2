// TestRunner.cs - Layer 0
// Main entry point for running all Layer 0 tests

using System;
// CPP: #include "IOHelper.g.h"
// CPP: #include "IOHelperTest.g.h"
// CPP: #include "BytecodeTest.g.h"

namespace MiniScript {

public static class TestRunner {
	public static void Main() {
		IOHelper.Print("==========================================");
		IOHelper.Print("  Layer 0 Tests (Foundation)");

		Boolean allPassed = true;

		// Run each test module
		allPassed = IOHelperTest.RunAll() && allPassed;
		allPassed = BytecodeTest.RunAll() && allPassed;

		// Only print failure notice if tests failed
		if (!allPassed) {
			IOHelper.Print("  Layer 0: SOME TESTS FAILED");
		}
	}
}

}
