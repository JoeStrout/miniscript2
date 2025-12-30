// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: TestRunner.cs

#include "TestRunner.g.h"
#include "IOHelper.g.h"
#include "IOHelperTest.g.h"
#include "BytecodeTest.g.h"

namespace MiniScript {



void TestRunner::Main() {
	IOHelper::Print("==========================================");
	IOHelper::Print("  Layer 0 Tests (Foundation)");

	Boolean allPassed = true;

	// Run each test module
	allPassed = IOHelperTest::RunAll() && allPassed;
	allPassed = BytecodeTest::RunAll() && allPassed;

	// Only print failure notice if tests failed
	if (!allPassed) {
		IOHelper::Print("  Layer 0: SOME TESTS FAILED");
	}
}



} // end of namespace MiniScript
