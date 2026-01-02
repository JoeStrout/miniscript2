// TestRunner.cs - Layer 2
// Main entry point for running all Layer 2 tests

using System;
// CPP: #include "IOHelper.g.h"
// CPP: #include "FuncDefTest.g.h"

namespace MiniScript {

public static class TestRunner {
	public static void Main() {
		IOHelper.Print("==========================================");
		IOHelper.Print("  Layer 2 Tests (Core Data Structures)");

		Boolean allPassed = true;

		// Run FuncDef tests
		allPassed = FuncDefTest.RunAll() && allPassed;

		// Only print failure notice if tests failed
		if (!allPassed) {
			IOHelper.Print("  Layer 2: SOME TESTS FAILED");
		}
	}
}

}
