// TestRunner.cs - Layer 4
// Main entry point for running all Layer 4 tests

using System;
// CPP: #include "IOHelper.g.h"
// CPP: #include "VMTest.g.h"

namespace MiniScript {

public static class TestRunner {
	public static void Main() {
		IOHelper.Print("==========================================");
		IOHelper.Print("  Layer 4 Tests (VM Layer)");

		Boolean allPassed = true;

		// Run VM tests
		allPassed = VMTest.RunAll() && allPassed;

		// Only print failure notice if tests failed
		if (!allPassed) {
			IOHelper.Print("  Layer 4: SOME TESTS FAILED");
		}
	}
}

}
