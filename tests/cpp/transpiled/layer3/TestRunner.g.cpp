// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: TestRunner.cs

#include "TestRunner.g.h"
#include "IOHelper.g.h"
#include "DisassemblerTest.g.h"
#include "AssemblerTest.g.h"

namespace MiniScript {


void TestRunner::Main() {
	IOHelper::Print("==========================================");
	IOHelper::Print("  Layer 3 Tests (Processing Layer)");

	Boolean allPassed = Boolean(true);

	// Run Disassembler tests
	allPassed = DisassemblerTest::RunAll() && allPassed;

	// Run Assembler tests
	allPassed = AssemblerTest::RunAll() && allPassed;

	// Only print failure notice if tests failed
	if (!allPassed) {
		IOHelper::Print("  Layer 3: SOME TESTS FAILED");
	}
}


} // end of namespace MiniScript
