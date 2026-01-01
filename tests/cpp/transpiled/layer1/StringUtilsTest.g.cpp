// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: StringUtilsTest.cs

#include "StringUtilsTest.g.h"
#include "StringUtils.g.h"
#include "IOHelper.g.h"
#include "TestFramework.g.h"

namespace MiniScript {


Boolean StringUtilsTest::TestToHexUInt32() {
	Boolean ok = true;

	ok = TestFramework::AssertEqual(StringUtils::ToHex((UInt32)0x00000000), "00000000", "ToHex(0)") && ok;
	ok = TestFramework::AssertEqual(StringUtils::ToHex((UInt32)0x12345678), "12345678", "ToHex(0x12345678)") && ok;
	ok = TestFramework::AssertEqual(StringUtils::ToHex((UInt32)0xABCDEF00), "ABCDEF00", "ToHex(0xABCDEF00)") && ok;
	ok = TestFramework::AssertEqual(StringUtils::ToHex((UInt32)0xFFFFFFFF), "FFFFFFFF", "ToHex(0xFFFFFFFF)") && ok;

	return ok;
}
Boolean StringUtilsTest::TestToHexByte() {
	Boolean ok = true;

	ok = TestFramework::AssertEqual(StringUtils::ToHex((Byte)0x00), "00", "ToHex(byte 0)") && ok;
	ok = TestFramework::AssertEqual(StringUtils::ToHex((Byte)0x42), "42", "ToHex(byte 0x42)") && ok;
	ok = TestFramework::AssertEqual(StringUtils::ToHex((Byte)0xAB), "AB", "ToHex(byte 0xAB)") && ok;
	ok = TestFramework::AssertEqual(StringUtils::ToHex((Byte)0xFF), "FF", "ToHex(byte 0xFF)") && ok;

	return ok;
}
Boolean StringUtilsTest::TestSpaces() {
	Boolean ok = true;

	ok = TestFramework::AssertEqual(StringUtils::Spaces(0), "", "Spaces(0)") && ok;
	ok = TestFramework::AssertEqual(StringUtils::Spaces(1), " ", "Spaces(1)") && ok;
	ok = TestFramework::AssertEqual(StringUtils::Spaces(5), "     ", "Spaces(5)") && ok;

	return ok;
}
Boolean StringUtilsTest::TestSpacePad() {
	Boolean ok = true;

	ok = TestFramework::AssertEqual(StringUtils::SpacePad("Hi", 5), "Hi   ", "SpacePad('Hi', 5)") && ok;
	ok = TestFramework::AssertEqual(StringUtils::SpacePad("Hello", 5), "Hello", "SpacePad('Hello', 5)") && ok;
	ok = TestFramework::AssertEqual(StringUtils::SpacePad("Hello World", 5), "Hello", "SpacePad('Hello World', 5)") && ok;
	ok = TestFramework::AssertEqual(StringUtils::SpacePad("", 3), "   ", "SpacePad('', 3)") && ok;

	return ok;
}
Boolean StringUtilsTest::RunAll() {
	IOHelper::Print("");
	IOHelper::Print("=== StringUtils Tests ===");
	TestFramework::Reset();

	Boolean allPassed = true;
	allPassed = TestToHexUInt32() && allPassed;
	allPassed = TestToHexByte() && allPassed;
	allPassed = TestSpaces() && allPassed;
	allPassed = TestSpacePad() && allPassed;

	TestFramework::PrintSummary("StringUtils");
	return TestFramework::AllPassed();
}




} // end of namespace MiniScript
