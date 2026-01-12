// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: AssemblerTest.cs

#include "AssemblerTest.g.h"
#include "Assembler.g.h"
#include "Bytecode.g.h"
#include "FuncDef.g.h"
#include "IOHelper.g.h"
#include "TestFramework.g.h"

namespace MiniScript {


Boolean AssemblerTest::TestCreation() {
	Boolean ok = Boolean(true);

	Assembler assem =  Assembler::New();
	ok = TestFramework::Assert(!IsNull(assem), "Assembler created") && ok;
	ok = TestFramework::AssertEqual(assem.Functions().Count(), 0, "no functions initially") && ok;
	ok = TestFramework::Assert(!IsNull(assem.Current()), "Current function exists") && ok;
	ok = TestFramework::AssertEqual(assem.HasError(), Boolean(false), "no error initially") && ok;

	return ok;
}
Boolean AssemblerTest::TestGetTokens() {
	Boolean ok = Boolean(true);

	// Simple case with spaces
	List<String> tokens = Assembler::GetTokens("LOAD r1, r2");
	ok = TestFramework::AssertEqual(tokens.Count(), 3, "token count for LOAD r1, r2") && ok;
	ok = TestFramework::AssertEqual(tokens[0], "LOAD", "first token") && ok;
	ok = TestFramework::AssertEqual(tokens[1], "r1", "second token") && ok;
	ok = TestFramework::AssertEqual(tokens[2], "r2", "third token") && ok;

	// With comment
	tokens = Assembler::GetTokens("ADD r0, r1, r2  # add them");
	ok = TestFramework::AssertEqual(tokens.Count(), 4, "token count with comment") && ok;
	ok = TestFramework::AssertEqual(tokens[0], "ADD", "ADD mnemonic") && ok;

	// Empty line
	tokens = Assembler::GetTokens("   # just a comment");
	ok = TestFramework::AssertEqual(tokens.Count(), 0, "empty line with comment") && ok;

	// Quoted string
	tokens = Assembler::GetTokens("LOAD r1, \"hello world\"");
	ok = TestFramework::AssertEqual(tokens.Count(), 3, "token count with string") && ok;
	ok = TestFramework::AssertEqual(tokens[2], "\"hello world\"", "quoted string token") && ok;

	return ok;
}
Boolean AssemblerTest::TestAddLine() {
	Boolean ok = Boolean(true);

	Assembler assem =  Assembler::New();

	// Test NOOP
	UInt32 instr = assem.AddLine("NOOP");
	ok = TestFramework::AssertEqual(BytecodeUtil::OP(instr), (Byte)Opcode::NOOP, "NOOP opcode") && ok;

	// Test LOAD with immediate
	instr = assem.AddLine("LOAD r0, 42");
	ok = TestFramework::AssertEqual(BytecodeUtil::OP(instr), (Byte)Opcode::LOAD_rA_iBC, "LOAD immediate opcode") && ok;
	ok = TestFramework::AssertEqual((Int32)BytecodeUtil::Au(instr), 0, "LOAD dest register") && ok;
	ok = TestFramework::AssertEqual((Int32)BytecodeUtil::BCs(instr), 42, "LOAD immediate value") && ok;

	// Test LOAD register to register
	instr = assem.AddLine("LOAD r1, r0");
	ok = TestFramework::AssertEqual(BytecodeUtil::OP(instr), (Byte)Opcode::LOAD_rA_rB, "LOAD reg opcode") && ok;
	ok = TestFramework::AssertEqual((Int32)BytecodeUtil::Au(instr), 1, "LOAD dest r1") && ok;
	ok = TestFramework::AssertEqual((Int32)BytecodeUtil::Bu(instr), 0, "LOAD src r0") && ok;

	// Test ADD
	instr = assem.AddLine("ADD r2, r0, r1");
	ok = TestFramework::AssertEqual(BytecodeUtil::OP(instr), (Byte)Opcode::ADD_rA_rB_rC, "ADD opcode") && ok;
	ok = TestFramework::AssertEqual((Int32)BytecodeUtil::Au(instr), 2, "ADD dest") && ok;
	ok = TestFramework::AssertEqual((Int32)BytecodeUtil::Bu(instr), 0, "ADD src1") && ok;
	ok = TestFramework::AssertEqual((Int32)BytecodeUtil::Cu(instr), 1, "ADD src2") && ok;

	// Test RETURN
	instr = assem.AddLine("RETURN");
	ok = TestFramework::AssertEqual(BytecodeUtil::OP(instr), (Byte)Opcode::RETURN, "RETURN opcode") && ok;

	// Verify instructions were added to current function
	ok = TestFramework::AssertEqual(assem.Current().Code().Count(), 5, "5 instructions added") && ok;

	return ok;
}
Boolean AssemblerTest::TestConstants() {
	Boolean ok = Boolean(true);

	Assembler assem =  Assembler::New();

	// Load a string constant
	UInt32 instr = assem.AddLine("LOAD r0, \"hello\"");
	ok = TestFramework::AssertEqual(BytecodeUtil::OP(instr), (Byte)Opcode::LOAD_rA_kBC, "LOAD string uses constant") && ok;
	ok = TestFramework::AssertEqual(assem.Current().Constants().Count(), 1, "one constant added") && ok;

	// Load another string - should add another constant
	instr = assem.AddLine("LOAD r1, \"world\"");
	ok = TestFramework::AssertEqual(assem.Current().Constants().Count(), 2, "two constants") && ok;

	// Load same string - should reuse constant
	instr = assem.AddLine("LOAD r2, \"hello\"");
	ok = TestFramework::AssertEqual(assem.Current().Constants().Count(), 2, "still two constants (reused)") && ok;

	return ok;
}
Boolean AssemblerTest::TestErrorHandling() {
	Boolean ok = Boolean(true);

	Assembler assem =  Assembler::New();

	// Valid instruction first
	assem.AddLine("NOOP");
	ok = TestFramework::AssertEqual(assem.HasError(), Boolean(false), "no error after NOOP") && ok;

	// Invalid instruction
	assem.AddLine("LOAD r0");  // Missing second operand
	ok = TestFramework::AssertEqual(assem.HasError(), Boolean(true), "error after bad LOAD") && ok;

	// ClearError should reset
	assem.ClearError();
	ok = TestFramework::AssertEqual(assem.HasError(), Boolean(false), "error cleared") && ok;

	return ok;
}
Boolean AssemblerTest::RunAll() {
	IOHelper::Print("");
	IOHelper::Print("=== Assembler Tests ===");
	TestFramework::Reset();

	Boolean allPassed = Boolean(true);
	allPassed = TestCreation() && allPassed;
	allPassed = TestGetTokens() && allPassed;
	allPassed = TestAddLine() && allPassed;
	allPassed = TestConstants() && allPassed;
	allPassed = TestErrorHandling() && allPassed;

	TestFramework::PrintSummary("Assembler");
	return TestFramework::AllPassed();
}


} // end of namespace MiniScript
