// DisassemblerTest.cs
// Tests for Disassembler module (Layer 3)

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// CPP: #include "Disassembler.g.h"
// CPP: #include "FuncDef.g.h"
// CPP: #include "Bytecode.g.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "TestFramework.g.h"

namespace MiniScript {

public static class DisassemblerTest {

	// Test AssemOp returns correct short mnemonics
	public static Boolean TestAssemOp() {
		Boolean ok = true;

		// Test various opcodes map to their short forms
		ok = TestFramework.AssertEqual(Disassembler.AssemOp(Opcode.NOOP), "NOOP", "NOOP mnemonic") && ok;
		ok = TestFramework.AssertEqual(Disassembler.AssemOp(Opcode.LOAD_rA_rB), "LOAD", "LOAD_rA_rB mnemonic") && ok;
		ok = TestFramework.AssertEqual(Disassembler.AssemOp(Opcode.LOAD_rA_iBC), "LOAD", "LOAD_rA_iBC mnemonic") && ok;
		ok = TestFramework.AssertEqual(Disassembler.AssemOp(Opcode.LOAD_rA_kBC), "LOAD", "LOAD_rA_kBC mnemonic") && ok;
		ok = TestFramework.AssertEqual(Disassembler.AssemOp(Opcode.ADD_rA_rB_rC), "ADD", "ADD mnemonic") && ok;
		ok = TestFramework.AssertEqual(Disassembler.AssemOp(Opcode.SUB_rA_rB_rC), "SUB", "SUB mnemonic") && ok;
		ok = TestFramework.AssertEqual(Disassembler.AssemOp(Opcode.MULT_rA_rB_rC), "MULT", "MULT mnemonic") && ok;
		ok = TestFramework.AssertEqual(Disassembler.AssemOp(Opcode.RETURN), "RETURN", "RETURN mnemonic") && ok;
		ok = TestFramework.AssertEqual(Disassembler.AssemOp(Opcode.JUMP_iABC), "JUMP", "JUMP mnemonic") && ok;
		ok = TestFramework.AssertEqual(Disassembler.AssemOp(Opcode.CALLF_iA_iBC), "CALLF", "CALLF mnemonic") && ok;

		return ok;
	}

	// Test ToString formats instructions correctly
	public static Boolean TestToString() {
		Boolean ok = true;

		// Test no-operand instructions
		UInt32 noop = BytecodeUtil.INS(Opcode.NOOP);
		ok = TestFramework.AssertEqual(Disassembler.ToString(noop), "NOOP   ", "NOOP format") && ok;

		UInt32 ret = BytecodeUtil.INS(Opcode.RETURN);
		ok = TestFramework.AssertEqual(Disassembler.ToString(ret), "RETURN ", "RETURN format") && ok;

		// Test rA, rB format (e.g., LOAD r2, r5)
		UInt32 loadReg = BytecodeUtil.INS_ABC(Opcode.LOAD_rA_rB, 2, 5, 0);
		ok = TestFramework.AssertEqual(Disassembler.ToString(loadReg), "LOAD    r2, r5", "LOAD rA, rB format") && ok;

		// Test rA, iBC format (e.g., LOAD r3, 42)
		UInt32 loadImm = BytecodeUtil.INS_AB(Opcode.LOAD_rA_iBC, 3, 42);
		ok = TestFramework.AssertEqual(Disassembler.ToString(loadImm), "LOAD    r3, 42", "LOAD rA, iBC format") && ok;

		// Test rA, kBC format (e.g., LOAD r1, k5)
		UInt32 loadConst = BytecodeUtil.INS_AB(Opcode.LOAD_rA_kBC, 1, 5);
		ok = TestFramework.AssertEqual(Disassembler.ToString(loadConst), "LOAD    r1, k5", "LOAD rA, kBC format") && ok;

		// Test rA, rB, rC format (e.g., ADD r0, r1, r2)
		UInt32 add = BytecodeUtil.INS_ABC(Opcode.ADD_rA_rB_rC, 0, 1, 2);
		ok = TestFramework.AssertEqual(Disassembler.ToString(add), "ADD     r0, r1, r2", "ADD rA, rB, rC format") && ok;

		// Test iABC format (e.g., JUMP 100)
		UInt32 jump = BytecodeUtil.INS(Opcode.JUMP_iABC) | 100;
		ok = TestFramework.AssertEqual(Disassembler.ToString(jump), "JUMP    100", "JUMP iABC format") && ok;

		return ok;
	}

	// Test Disassemble with a simple function
	public static Boolean TestDisassembleFunction() {
		Boolean ok = true;

		// Create a simple function with a few instructions
		FuncDef func = new FuncDef();
		func.Name = "testFunc";
		func.ReserveRegister(2);

		// Add some constants
		func.Constants.Add(make_int(42));
		func.Constants.Add(make_string("hello"));

		// Add some code: LOAD r0, 10; LOAD r1, k0; ADD r2, r0, r1; RETURN
		func.Code.Add(BytecodeUtil.INS_AB(Opcode.LOAD_rA_iBC, 0, 10));
		func.Code.Add(BytecodeUtil.INS_AB(Opcode.LOAD_rA_kBC, 1, 0));
		func.Code.Add(BytecodeUtil.INS_ABC(Opcode.ADD_rA_rB_rC, 2, 0, 1));
		func.Code.Add(BytecodeUtil.INS(Opcode.RETURN));

		// Disassemble
		List<String> output = new List<String>();
		Disassembler.Disassemble(func, output, false);

		// Verify output has expected content
		ok = TestFramework.Assert(output.Count >= 4, "output has at least 4 lines") && ok;

		// Check that instructions are present (non-detailed mode)
		Boolean hasLoad = false;
		Boolean hasAdd = false;
		Boolean hasReturn = false;
		for (Int32 i = 0; i < output.Count; i++) {
			if (output[i].Contains("LOAD")) hasLoad = true;
			if (output[i].Contains("ADD")) hasAdd = true;
			if (output[i].Contains("RETURN")) hasReturn = true;
		}
		ok = TestFramework.Assert(hasLoad, "output contains LOAD") && ok;
		ok = TestFramework.Assert(hasAdd, "output contains ADD") && ok;
		ok = TestFramework.Assert(hasReturn, "output contains RETURN") && ok;

		return ok;
	}

	// Main test runner for this module
	public static Boolean RunAll() {
		IOHelper.Print("");
		IOHelper.Print("=== Disassembler Tests ===");
		TestFramework.Reset();

		Boolean allPassed = true;
		allPassed = TestAssemOp() && allPassed;
		allPassed = TestToString() && allPassed;
		allPassed = TestDisassembleFunction() && allPassed;

		TestFramework.PrintSummary("Disassembler");
		return TestFramework.AllPassed();
	}
}

}
