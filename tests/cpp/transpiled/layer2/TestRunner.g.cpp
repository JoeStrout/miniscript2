// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: TestRunner.cs

#include "TestRunner.g.h"
#include "IOHelper.g.h"
#include "FuncDefTest.g.h"

namespace MiniScript {



void TestRunner::Main() {
	IOHelper::Print("==========================================");
	IOHelper::Print("  Layer 2 Tests (Core Data Structures)");

	Boolean allPassed = true;

	// Run FuncDef tests
	allPassed = FuncDefTest::RunAll() && allPassed;

	// Only print failure notice if tests failed
	if (!allPassed) {
		IOHelper::Print("  Layer 2: SOME TESTS FAILED");
	}
}


} // end of namespace MiniScript
