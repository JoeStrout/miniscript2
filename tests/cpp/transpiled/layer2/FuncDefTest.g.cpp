// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: FuncDefTest.cs

#include "FuncDefTest.g.h"
#include "FuncDef.g.h"
#include "IOHelper.g.h"
#include "TestFramework.g.h"

namespace MiniScript {


Boolean FuncDefTest::TestCreation() {
	FuncDef func =  FuncDef();
	Boolean ok = true;

	ok = TestFramework::Assert(!IsNull(func), "FuncDef created") && ok;
	ok = TestFramework::AssertEqual(func::Name, "", "default name is empty") && ok;
	ok = TestFramework::AssertEqual(func::Code::Count, 0, "code starts empty") && ok;
	ok = TestFramework::AssertEqual(func::Constants::Count, 0, "constants starts empty") && ok;

	return ok;
}
Boolean FuncDefTest::TestAddCode() {
	FuncDef func =  FuncDef();
	func::Code::Add(0x12345678);
	func::Code::Add(0xABCDEF00);

	Boolean ok = true;
	ok = TestFramework::AssertEqual(func::Code::Count, 2, "code has 2 instructions") && ok;
	ok = TestFramework::AssertEqual(func::Code[0], (UInt32)0x12345678, "first instruction") && ok;
	ok = TestFramework::AssertEqual(func::Code[1], (UInt32)0xABCDEF00, "second instruction") && ok;

	return ok;
}
Boolean FuncDefTest::TestAddConstants() {
	FuncDef func =  FuncDef();
	func::Constants::Add(make_int(42));
	func::Constants::Add(make_string("hello"));

	Boolean ok = true;
	ok = TestFramework::AssertEqual(func::Constants::Count, 2, "constants has 2 values") && ok;
	ok = TestFramework::AssertEqual(as_int(func::Constants[0]), 42, "first constant") && ok;
	ok = TestFramework::AssertEqual(as_cstring(func::Constants[1]), "hello", "second constant") && ok;

	return ok;
}
Boolean FuncDefTest::TestReserveRegister() {
	FuncDef func =  FuncDef();
	func::ReserveRegister(5);

	Boolean ok = true;
	ok = TestFramework::Assert(func::MaxRegs >= 6, "reserved at least 6 registers") && ok;

	return ok;
}
Boolean FuncDefTest::RunAll() {
	IOHelper::Print("");
	IOHelper::Print("=== FuncDef Tests ===");
	TestFramework::Reset();

	Boolean allPassed = true;
	allPassed = TestCreation() && allPassed;
	allPassed = TestAddCode() && allPassed;
	allPassed = TestAddConstants() && allPassed;
	allPassed = TestReserveRegister() && allPassed;

	TestFramework::PrintSummary("FuncDef");
	return TestFramework::AllPassed();
}


} // end of namespace MiniScript
