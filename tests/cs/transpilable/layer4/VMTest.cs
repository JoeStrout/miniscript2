// VMTest.cs
// Tests for VM module (Layer 4)

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// CPP: #include "gc.h"
// CPP: #include "Assembler.g.h"
// CPP: #include "Bytecode.g.h"
// CPP: #include "FuncDef.g.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "TestFramework.g.h"
// CPP: #include "VM.g.h"

namespace MiniScript {

public static class VMTest {

	// Helper to assemble source lines and return functions list
	private static List<FuncDef> AssembleProgram(List<String> sourceLines) {
		Assembler assem = new Assembler();
		assem.Assemble(sourceLines);
		return assem.Functions;
	}

	// Test VM creation and initial state
	public static Boolean TestCreation() {
		Boolean ok = true;

		VM vm = new VM();
		ok = TestFramework.Assert(vm != null, "VM created") && ok;
		ok = TestFramework.AssertEqual(vm.IsRunning, false, "not running initially") && ok;

		return ok;
	}

	// Test simple LOAD and RETURN
	public static Boolean TestLoadAndReturn() {
		Boolean ok = true;

		// Build a simple function: LOAD r0, 42; RETURN
		List<String> source = new List<String>();
		source.Add("@main:");
		source.Add("LOAD r0, 42");
		source.Add("RETURN");

		List<FuncDef> functions = AssembleProgram(source);

		// Run VM
		VM vm = new VM();
		vm.Reset(functions);
		Value result = vm.Run();

		ok = TestFramework.AssertEqual(as_int(result), 42, "return value is 42") && ok;
		ok = TestFramework.AssertEqual(vm.IsRunning, false, "VM stopped after return") && ok;

		return ok;
	}

	// Test basic arithmetic operations
	public static Boolean TestArithmetic() {
		Boolean ok = true;

		// Build function: r0 = 10, r1 = 3, r2 = r0 + r1, return r2
		List<String> source = new List<String>();
		source.Add("@main:");
		source.Add("LOAD r0, 10");
		source.Add("LOAD r1, 3");
		source.Add("ADD r2, r0, r1");
		source.Add("LOAD r0, r2");  // copy result to r0 for return
		source.Add("RETURN");

		List<FuncDef> functions = AssembleProgram(source);

		VM vm = new VM();
		vm.Reset(functions);
		Value result = vm.Run();

		ok = TestFramework.AssertEqual(as_int(result), 13, "10 + 3 = 13") && ok;

		return ok;
	}

	// Test subtraction
	public static Boolean TestSubtraction() {
		Boolean ok = true;

		List<String> source = new List<String>();
		source.Add("@main:");
		source.Add("LOAD r0, 10");
		source.Add("LOAD r1, 3");
		source.Add("SUB r2, r0, r1");
		source.Add("LOAD r0, r2");
		source.Add("RETURN");

		List<FuncDef> functions = AssembleProgram(source);

		VM vm = new VM();
		vm.Reset(functions);
		Value result = vm.Run();

		ok = TestFramework.AssertEqual(as_int(result), 7, "10 - 3 = 7") && ok;

		return ok;
	}

	// Test multiplication
	public static Boolean TestMultiplication() {
		Boolean ok = true;

		List<String> source = new List<String>();
		source.Add("@main:");
		source.Add("LOAD r0, 6");
		source.Add("LOAD r1, 7");
		source.Add("MULT r2, r0, r1");
		source.Add("LOAD r0, r2");
		source.Add("RETURN");

		List<FuncDef> functions = AssembleProgram(source);

		VM vm = new VM();
		vm.Reset(functions);
		Value result = vm.Run();

		ok = TestFramework.AssertEqual(as_int(result), 42, "6 * 7 = 42") && ok;

		return ok;
	}

	// Test simple conditional jump
	public static Boolean TestConditionalJump() {
		Boolean ok = true;

		// if 5 < 10, return 1, else return 0
		List<String> source = new List<String>();
		source.Add("@main:");
		source.Add("LOAD r0, 5");
		source.Add("LOAD r1, 10");
		source.Add("LT r2, r0, r1");      // r2 = (5 < 10) = 1
		source.Add("BRFALSE r2, 2");      // if false, skip 2 instructions
		source.Add("LOAD r0, 1");         // true path
		source.Add("RETURN");
		source.Add("LOAD r0, 0");         // false path
		source.Add("RETURN");

		List<FuncDef> functions = AssembleProgram(source);

		VM vm = new VM();
		vm.Reset(functions);
		Value result = vm.Run();

		ok = TestFramework.AssertEqual(as_int(result), 1, "5 < 10 is true, return 1") && ok;

		return ok;
	}

	// Test simple loop (sum 1 to 5)
	public static Boolean TestSimpleLoop() {
		Boolean ok = true;

		// sum = 0; i = 1; while i <= 5: sum += i; i++; return sum
		List<String> source = new List<String>();
		source.Add("@main:");
		source.Add("LOAD r0, 0");         // 0: r0 = sum = 0
		source.Add("LOAD r1, 1");         // 1: r1 = i = 1
		// loop:
		source.Add("LT r2, 5, r1");       // 2: r2 = (5 < r1) = (i > 5)
		source.Add("BRTRUE r2, 4");       // 3: if i > 5, exit loop (jump to RETURN)
		source.Add("ADD r0, r0, r1");     // 4: sum += i
		source.Add("LOAD r2, 1");         // 5: r2 = 1
		source.Add("ADD r1, r1, r2");     // 6: i++
		source.Add("JUMP -6");            // 7: back to loop (PC 8 + (-6) = 2)
		source.Add("RETURN");             // 8: return sum

		List<FuncDef> functions = AssembleProgram(source);

		VM vm = new VM();
		vm.Reset(functions);
		Value result = vm.Run();

		// 1 + 2 + 3 + 4 + 5 = 15
		ok = TestFramework.AssertEqual(as_int(result), 15, "sum 1..5 = 15") && ok;

		return ok;
	}

	// Main test runner for this module
	public static Boolean RunAll() {
		IOHelper.Print("");
		IOHelper.Print("=== VM Tests ===");
		TestFramework.Reset();

		Boolean allPassed = true;
		allPassed = TestCreation() && allPassed;
		allPassed = TestLoadAndReturn() && allPassed;
		allPassed = TestArithmetic() && allPassed;
		allPassed = TestSubtraction() && allPassed;
		allPassed = TestMultiplication() && allPassed;
		allPassed = TestConditionalJump() && allPassed;
		allPassed = TestSimpleLoop() && allPassed;

		TestFramework.PrintSummary("VM");
		return TestFramework.AllPassed();
	}
}

}
