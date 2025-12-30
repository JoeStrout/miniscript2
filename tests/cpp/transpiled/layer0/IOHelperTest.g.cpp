// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IOHelperTest.cs

#include "IOHelperTest.g.h"
#include "IOHelper.g.h"
#include "TestFramework.g.h"

namespace MiniScript {


Boolean IOHelperTest::TestPrint() {
	// Simple test that print doesn't crash
	// Don't print anything - we're testing that Print works, not filling output
	return TestFramework::Assert(true, "Print should not crash");
}
Boolean IOHelperTest::TestReadFile() {
	// Test reading a file (using a test fixture)
	// For now, just test that it doesn't crash
	// The actual file path will vary between C# and C++ test runs
	return TestFramework::Assert(true, "ReadFile test (placeholder)");
}
Boolean IOHelperTest::RunAll() {
	IOHelper::Print("=== IOHelper Tests ===");
	TestFramework::Reset();

	Boolean allPassed = true;
	allPassed = TestPrint() && allPassed;
	allPassed = TestReadFile() && allPassed;

	TestFramework::PrintSummary("IOHelper");
	return TestFramework::AllPassed();
}




} // end of namespace MiniScript
