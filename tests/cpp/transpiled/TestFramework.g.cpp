// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: TestFramework.cs

#include "TestFramework.g.h"
#include "IOHelper.g.h"
#include "value.h"

namespace MiniScript {


Int32 TestFramework::FailCount = 0;
Int32 TestFramework::PassCount = 0;
void TestFramework::Reset() {
	FailCount = 0;
	PassCount = 0;
}
Boolean TestFramework::Assert(Boolean condition, String message) {
	if (condition) {
		PassCount++;
		return Boolean(true);
	}
	IOHelper::Print(Interp("FAIL: {}", message));
	FailCount++;
	return Boolean(false);
}
Boolean TestFramework::AssertEqual(Int32 actual, Int32 expected, String context) {
	if (actual == expected) {
		PassCount++;
		return Boolean(true);
	}
	IOHelper::Print(Interp("FAIL: {} - expected {}, got {}", context, expected, actual));
	FailCount++;
	return Boolean(false);
}
Boolean TestFramework::AssertEqual(UInt32 actual, UInt32 expected, String context) {
	if (actual == expected) {
		PassCount++;
		return Boolean(true);
	}
	IOHelper::Print(Interp("FAIL: {} - expected {}, got {}", context, expected, actual));
	FailCount++;
	return Boolean(false);
}
Boolean TestFramework::AssertEqual(String actual, String expected, String context) {
	if (actual == expected) {
		PassCount++;
		return Boolean(true);
	}
	IOHelper::Print(Interp("FAIL: {} - expected \"{}\", got \"{}\"", context, expected, actual));
	FailCount++;
	return Boolean(false);
}
Boolean TestFramework::AssertEqual(Boolean actual, Boolean expected, String context) {
	if (actual == expected) {
		PassCount++;
		return Boolean(true);
	}
	String expStr = expected ? "true" : "false";
	String actStr = actual ? "true" : "false";
	IOHelper::Print(Interp("FAIL: {} - expected {}, got {}", context, expStr, actStr));
	FailCount++;
	return Boolean(false);
}
void TestFramework::PrintSummary(String suiteName) {
	// Silent - failures are already printed
}
Boolean TestFramework::AllPassed() {
	return FailCount == 0;
}


} // end of namespace MiniScript
