// TestRunner.cs - Layer 3
// Main entry point for running all Layer 3 tests

using System;
// CPP: #include "IOHelper.g.h"
// CPP: #include "DisassemblerTest.g.h"

namespace MiniScript {

public static class TestRunner {
	public static void Main() {
		IOHelper.Print("==========================================");
		IOHelper.Print("  Layer 3 Tests (Processing Layer)");

		Boolean allPassed = true;

		// Run Disassembler tests
		allPassed = DisassemblerTest.RunAll() && allPassed;

		// Only print failure notice if tests failed
		if (!allPassed) {
			IOHelper.Print("  Layer 3: SOME TESTS FAILED");
		}
	}
}

}
