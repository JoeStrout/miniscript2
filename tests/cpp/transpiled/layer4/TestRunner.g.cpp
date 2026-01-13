// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: TestRunner.cs

#include "TestRunner.g.h"
#include "IOHelper.g.h"
#include "VMTest.g.h"

namespace MiniScript {


void TestRunner::Main() {
	IOHelper::Print("==========================================");
	IOHelper::Print("  Layer 4 Tests (VM Layer)");

	Boolean allPassed = Boolean(true);

	// Run VM tests
	allPassed = VMTest::RunAll() && allPassed;

	// Only print failure notice if tests failed
	if (!allPassed) {
		IOHelper::Print("  Layer 4: SOME TESTS FAILED");
	}
}


} // end of namespace MiniScript
