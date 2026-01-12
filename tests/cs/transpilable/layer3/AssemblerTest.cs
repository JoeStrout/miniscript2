// AssemblerTest.cs
// Tests for Assembler module (Layer 3)

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// CPP: #include "Assembler.g.h"
// CPP: #include "Bytecode.g.h"
// CPP: #include "FuncDef.g.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "TestFramework.g.h"

namespace MiniScript {

public static class AssemblerTest {

	// Test Assembler creation and initial state
	public static Boolean TestCreation() {
		Boolean ok = true;

		Assembler assem = new Assembler();
		ok = TestFramework.Assert(assem != null, "Assembler created") && ok;
		ok = TestFramework.AssertEqual(assem.Functions.Count, 0, "no functions initially") && ok;
		ok = TestFramework.Assert(assem.Current != null, "Current function exists") && ok;
		ok = TestFramework.AssertEqual(assem.HasError, false, "no error initially") && ok;

		return ok;
	}

	// Test GetTokens static method
	public static Boolean TestGetTokens() {
		Boolean ok = true;

		// Simple case with spaces
		List<String> tokens = Assembler.GetTokens("LOAD r1, r2");
		ok = TestFramework.AssertEqual(tokens.Count, 3, "token count for LOAD r1, r2") && ok;
		ok = TestFramework.AssertEqual(tokens[0], "LOAD", "first token") && ok;
		ok = TestFramework.AssertEqual(tokens[1], "r1", "second token") && ok;
		ok = TestFramework.AssertEqual(tokens[2], "r2", "third token") && ok;

		// With comment
		tokens = Assembler.GetTokens("ADD r0, r1, r2  # add them");
		ok = TestFramework.AssertEqual(tokens.Count, 4, "token count with comment") && ok;
		ok = TestFramework.AssertEqual(tokens[0], "ADD", "ADD mnemonic") && ok;

		// Empty line
		tokens = Assembler.GetTokens("   # just a comment");
		ok = TestFramework.AssertEqual(tokens.Count, 0, "empty line with comment") && ok;

		// Quoted string
		tokens = Assembler.GetTokens("LOAD r1, \"hello world\"");
		ok = TestFramework.AssertEqual(tokens.Count, 3, "token count with string") && ok;
		ok = TestFramework.AssertEqual(tokens[2], "\"hello world\"", "quoted string token") && ok;

		return ok;
	}

	// Test assembling simple instructions
	public static Boolean TestAddLine() {
		Boolean ok = true;

		Assembler assem = new Assembler();

		// Test NOOP
		UInt32 instr = assem.AddLine("NOOP");
		ok = TestFramework.AssertEqual(BytecodeUtil.OP(instr), (Byte)Opcode.NOOP, "NOOP opcode") && ok;

		// Test LOAD with immediate
		instr = assem.AddLine("LOAD r0, 42");
		ok = TestFramework.AssertEqual(BytecodeUtil.OP(instr), (Byte)Opcode.LOAD_rA_iBC, "LOAD immediate opcode") && ok;
		ok = TestFramework.AssertEqual((Int32)BytecodeUtil.Au(instr), 0, "LOAD dest register") && ok;
		ok = TestFramework.AssertEqual((Int32)BytecodeUtil.BCs(instr), 42, "LOAD immediate value") && ok;

		// Test LOAD register to register
		instr = assem.AddLine("LOAD r1, r0");
		ok = TestFramework.AssertEqual(BytecodeUtil.OP(instr), (Byte)Opcode.LOAD_rA_rB, "LOAD reg opcode") && ok;
		ok = TestFramework.AssertEqual((Int32)BytecodeUtil.Au(instr), 1, "LOAD dest r1") && ok;
		ok = TestFramework.AssertEqual((Int32)BytecodeUtil.Bu(instr), 0, "LOAD src r0") && ok;

		// Test ADD
		instr = assem.AddLine("ADD r2, r0, r1");
		ok = TestFramework.AssertEqual(BytecodeUtil.OP(instr), (Byte)Opcode.ADD_rA_rB_rC, "ADD opcode") && ok;
		ok = TestFramework.AssertEqual((Int32)BytecodeUtil.Au(instr), 2, "ADD dest") && ok;
		ok = TestFramework.AssertEqual((Int32)BytecodeUtil.Bu(instr), 0, "ADD src1") && ok;
		ok = TestFramework.AssertEqual((Int32)BytecodeUtil.Cu(instr), 1, "ADD src2") && ok;

		// Test RETURN
		instr = assem.AddLine("RETURN");
		ok = TestFramework.AssertEqual(BytecodeUtil.OP(instr), (Byte)Opcode.RETURN, "RETURN opcode") && ok;

		// Verify instructions were added to current function
		ok = TestFramework.AssertEqual(assem.Current.Code.Count, 5, "5 instructions added") && ok;

		return ok;
	}

	// Test assembling with constants (strings, floats)
	public static Boolean TestConstants() {
		Boolean ok = true;

		Assembler assem = new Assembler();

		// Load a string constant
		UInt32 instr = assem.AddLine("LOAD r0, \"hello\"");
		ok = TestFramework.AssertEqual(BytecodeUtil.OP(instr), (Byte)Opcode.LOAD_rA_kBC, "LOAD string uses constant") && ok;
		ok = TestFramework.AssertEqual(assem.Current.Constants.Count, 1, "one constant added") && ok;

		// Load another string - should add another constant
		instr = assem.AddLine("LOAD r1, \"world\"");
		ok = TestFramework.AssertEqual(assem.Current.Constants.Count, 2, "two constants") && ok;

		// Load same string - should reuse constant
		instr = assem.AddLine("LOAD r2, \"hello\"");
		ok = TestFramework.AssertEqual(assem.Current.Constants.Count, 2, "still two constants (reused)") && ok;

		return ok;
	}

	// Test error handling
	public static Boolean TestErrorHandling() {
		Boolean ok = true;

		Assembler assem = new Assembler();

		// Valid instruction first
		assem.AddLine("NOOP");
		ok = TestFramework.AssertEqual(assem.HasError, false, "no error after NOOP") && ok;

		// Invalid instruction
		assem.AddLine("LOAD r0");  // Missing second operand
		ok = TestFramework.AssertEqual(assem.HasError, true, "error after bad LOAD") && ok;

		// ClearError should reset
		assem.ClearError();
		ok = TestFramework.AssertEqual(assem.HasError, false, "error cleared") && ok;

		return ok;
	}

	// Main test runner for this module
	public static Boolean RunAll() {
		IOHelper.Print("");
		IOHelper.Print("=== Assembler Tests ===");
		TestFramework.Reset();

		Boolean allPassed = true;
		allPassed = TestCreation() && allPassed;
		allPassed = TestGetTokens() && allPassed;
		allPassed = TestAddLine() && allPassed;
		allPassed = TestConstants() && allPassed;
		allPassed = TestErrorHandling() && allPassed;

		TestFramework.PrintSummary("Assembler");
		return TestFramework.AllPassed();
	}
}

}
