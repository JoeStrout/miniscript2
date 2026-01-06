// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: TestRunner.cs

#include "TestRunner.g.h"
#include "IOHelper.g.h"
#include "ValueTest.g.h"
#include "StringUtilsTest.g.h"

namespace MiniScript {


void TestRunner::Main() {
	IOHelper::Print("==========================================");
	IOHelper::Print("  Layer 1 Tests (Basic Infrastructure)");

	Boolean allPassed = true;

	// Run Value tests FIRST - everything depends on TestRunner(shared_from_this())
	allPassed = ValueTest::RunAll() && allPassed;

	// Run StringUtils tests
	allPassed = StringUtilsTest::RunAll() && allPassed;

	// TODO: Add MemPoolShimTest, ValueFuncRefTest

	// Only print failure notice if tests failed
	if (!allPassed) {
		IOHelper::Print("  Layer 1: SOME TESTS FAILED");
	}
}


} // end of namespace MiniScript
