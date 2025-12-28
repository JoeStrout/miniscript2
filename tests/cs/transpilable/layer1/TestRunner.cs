// TestRunner.cs - Layer 1
// Main entry point for running all Layer 1 tests

using System;
// CPP: #include "IOHelper.g.h"
// CPP: #include "ValueTest.g.h"

namespace MiniScript {

public static class TestRunner {
	public static void Main() {
		IOHelper.Print("==========================================");
		IOHelper.Print("  Layer 1 Tests (Basic Infrastructure)");

		Boolean allPassed = true;

		// Run Value tests FIRST - everything depends on this
		allPassed = ValueTest.RunAll() && allPassed;

		// TODO: Add StringUtilsTest, MemPoolShimTest, ValueFuncRefTest

		// Only print failure notice if tests failed
		if (!allPassed) {
			IOHelper.Print("  Layer 1: SOME TESTS FAILED");
		}
	}
}

}
