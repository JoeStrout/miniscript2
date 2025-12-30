// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: BytecodeTest.cs

#include "BytecodeTest.g.h"
#include "Bytecode.g.h"
#include "IOHelper.g.h"
#include "TestFramework.g.h"

namespace MiniScript {




Boolean BytecodeTest::TestOpcodeEnumValues() {
	// Test that opcode enum values are sequential starting at 0
	Boolean ok = true;
	ok = TestFramework::AssertEqual((Int32)Opcode::NOOP, 0, "NOOP opcode") && ok;
	ok = TestFramework::AssertEqual((Int32)Opcode::LOAD_rA_rB, 1, "LOAD_rA_rB opcode") && ok;
	ok = TestFramework::AssertEqual((Int32)Opcode::RETURN, 61, "RETURN opcode") && ok;
	return ok;
}
Boolean BytecodeTest::TestInstructionEncoding() {
	// Test INS_ABC encoding
	UInt32 instruction = BytecodeUtil::INS_ABC(Opcode::ADD_rA_rB_rC, 1, 2, 3);
	Boolean ok = true;
	ok = TestFramework::AssertEqual(BytecodeUtil::OP(instruction), (Byte)Opcode::ADD_rA_rB_rC, "Opcode extraction") && ok;
	ok = TestFramework::AssertEqual(BytecodeUtil::Au(instruction), (Byte)1, "A field extraction") && ok;
	ok = TestFramework::AssertEqual(BytecodeUtil::Bu(instruction), (Byte)2, "B field extraction") && ok;
	ok = TestFramework::AssertEqual(BytecodeUtil::Cu(instruction), (Byte)3, "C field extraction") && ok;
	return ok;
}
Boolean BytecodeTest::TestInstructionEncodingAB() {
	// Test INS_AB encoding
	UInt32 instruction = BytecodeUtil::INS_AB(Opcode::LOAD_rA_iBC, 5, 42);
	Boolean ok = true;
	ok = TestFramework::AssertEqual(BytecodeUtil::OP(instruction), (Byte)Opcode::LOAD_rA_iBC, "Opcode extraction") && ok;
	ok = TestFramework::AssertEqual(BytecodeUtil::Au(instruction), (Byte)5, "A field extraction") && ok;
	ok = TestFramework::AssertEqual(BytecodeUtil::BCu(instruction), (UInt16)42, "BC field extraction") && ok;
	ok = TestFramework::AssertEqual(BytecodeUtil::BCs(instruction), (Int16)42, "BC signed extraction") && ok;
	return ok;
}
Boolean BytecodeTest::TestNegativeImmediate() {
	// Test negative immediate values
	UInt32 instruction = BytecodeUtil::INS_AB(Opcode::LOAD_rA_iBC, 3, -100);
	Boolean ok = true;
	ok = TestFramework::AssertEqual(BytecodeUtil::BCs(instruction), (Int16)(-100), "Negative BC extraction") && ok;
	return ok;
}
Boolean BytecodeTest::TestToMnemonic() {
	// Test opcode to mnemonic conversion
	Boolean ok = true;
	String noop = BytecodeUtil::ToMnemonic(Opcode::NOOP);
	String expected1 = "NOOP";
	ok = TestFramework::AssertEqual(noop, expected1, "NOOP mnemonic") && ok;
	String add = BytecodeUtil::ToMnemonic(Opcode::ADD_rA_rB_rC);
	String expected2 = "ADD_rA_rB_rC";
	ok = TestFramework::AssertEqual(add, expected2, "ADD mnemonic") && ok;
	return ok;
}
Boolean BytecodeTest::TestFromMnemonic() {
	// Test mnemonic to opcode conversion
	Boolean ok = true;
	ok = TestFramework::AssertEqual((Int32)BytecodeUtil::FromMnemonic("NOOP"), (Int32)Opcode::NOOP, "NOOP from mnemonic") && ok;
	ok = TestFramework::AssertEqual((Int32)BytecodeUtil::FromMnemonic("ADD_rA_rB_rC"), (Int32)Opcode::ADD_rA_rB_rC, "ADD from mnemonic") && ok;
	return ok;
}
Boolean BytecodeTest::RunAll() {
	IOHelper::Print("=== Bytecode Tests ===");
	TestFramework::Reset();

	Boolean allPassed = true;
	allPassed = TestOpcodeEnumValues() && allPassed;
	allPassed = TestInstructionEncoding() && allPassed;
	allPassed = TestInstructionEncodingAB() && allPassed;
	allPassed = TestNegativeImmediate() && allPassed;
	allPassed = TestToMnemonic() && allPassed;
	allPassed = TestFromMnemonic() && allPassed;

	TestFramework::PrintSummary("Bytecode");
	return TestFramework::AllPassed();
}


} // end of namespace MiniScript
