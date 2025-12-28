// FuncDefTest.cs
// Tests for FuncDef module (Layer 2)

using System;
using static MiniScript.ValueHelpers;
// CPP: #include "FuncDef.g.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "TestFramework.g.h"

namespace MiniScript {

public static class FuncDefTest {

	public static Boolean TestCreation() {
		FuncDef func = new FuncDef();
		Boolean ok = true;

		ok = TestFramework.Assert(func != null, "FuncDef created") && ok;
		ok = TestFramework.AssertEqual(func.Name, "", "default name is empty") && ok;
		ok = TestFramework.AssertEqual(func.Code.Count, 0, "code starts empty") && ok;
		ok = TestFramework.AssertEqual(func.Constants.Count, 0, "constants starts empty") && ok;

		return ok;
	}

	public static Boolean TestAddCode() {
		FuncDef func = new FuncDef();
		func.Code.Add(0x12345678);
		func.Code.Add(0xABCDEF00);

		Boolean ok = true;
		ok = TestFramework.AssertEqual(func.Code.Count, 2, "code has 2 instructions") && ok;
		ok = TestFramework.AssertEqual(func.Code[0], (UInt32)0x12345678, "first instruction") && ok;
		ok = TestFramework.AssertEqual(func.Code[1], (UInt32)0xABCDEF00, "second instruction") && ok;

		return ok;
	}

	public static Boolean TestAddConstants() {
		FuncDef func = new FuncDef();
		func.Constants.Add(make_int(42));
		func.Constants.Add(make_string("hello"));

		Boolean ok = true;
		ok = TestFramework.AssertEqual(func.Constants.Count, 2, "constants has 2 values") && ok;
		ok = TestFramework.AssertEqual(as_int(func.Constants[0]), 42, "first constant") && ok;
		ok = TestFramework.AssertEqual(as_cstring(func.Constants[1]), "hello", "second constant") && ok;

		return ok;
	}

	public static Boolean TestReserveRegister() {
		FuncDef func = new FuncDef();
		func.ReserveRegister(5);

		Boolean ok = true;
		ok = TestFramework.Assert(func.RegisterCount >= 6, "reserved at least 6 registers") && ok;

		return ok;
	}

	// Main test runner for this module
	public static Boolean RunAll() {
		IOHelper.Print("");
		IOHelper.Print("=== FuncDef Tests ===");
		TestFramework.Reset();

		Boolean allPassed = true;
		allPassed = TestCreation() && allPassed;
		allPassed = TestAddCode() && allPassed;
		allPassed = TestAddConstants() && allPassed;
		allPassed = TestReserveRegister() && allPassed;

		TestFramework.PrintSummary("FuncDef");
		return TestFramework.AllPassed();
	}
}

}
