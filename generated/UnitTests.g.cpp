// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: UnitTests.cs

#include "UnitTests.g.h"
#include "IOHelper.g.h"
#include "StringUtils.g.h"
#include "Disassembler.g.h"
#include "Assembler.g.h"  // We really should automate this.

namespace MiniScript {


Boolean UnitTests::Assert(Boolean condition, String message) {
	if (condition) return Boolean(true);
	IOHelper::Print( String::New("Unit test failure: ") + message);
	return Boolean(false);
}
Boolean UnitTests::AssertEqual(String actual, String expected) {
	if (actual == expected) return Boolean(true);
	Assert(Boolean(false),  String::New("Unit test failure: expected \"")
	  + expected + "\" but got \"" + actual + "\"");
	return Boolean(false);
}
Boolean UnitTests::AssertEqual(UInt32 actual, UInt32 expected) {
	if (actual == expected) return Boolean(true);
	Assert(Boolean(false),  String::New("Unit test failure: expected 0x")
	  + StringUtils::ToHex(expected) + "\" but got 0x" + StringUtils::ToHex(actual));
	return Boolean(false);
}
Boolean UnitTests::AssertEqual(Int32 actual, Int32 expected) {
	if (actual == expected) return Boolean(true);
	Assert(Boolean(false), StringUtils::Format("Unit test failure: expected {0} but got {1}",
			expected, actual));
	return Boolean(false);
}
Boolean UnitTests::AssertEqual(List<String> actual, List<String> expected) {
	Boolean ok = Boolean(true);
	// (no nulls)
	if (ok && actual.Count() != expected.Count()) ok = Boolean(false);
	for (Int32 i = 0; ok && i < actual.Count(); i++) {
		if (actual[i] != expected[i]) ok = Boolean(false);
	}
	if (ok) return Boolean(true);
	Assert(Boolean(false),  String::New("Unit test failure: expected ")
	  + StringUtils::Str(expected) + " but got " + StringUtils::Str(actual));
	return Boolean(false);
}
Boolean UnitTests::TestStringUtils() {
	return 
		AssertEqual(StringUtils::ToHex((UInt32)123456789), "075BCD15")
	&&  AssertEqual( String::New("abcdef")::Left(3), "abc")
	&&	AssertEqual( String::New("abcdef")::Right(3), "def");
}
Boolean UnitTests::TestDisassembler() {
	return
		AssertEqual(Disassembler::ToString(0x01050A00), "LOAD    r5, r10");
}
Boolean UnitTests::TestAssembler() {
	// Test tokenization
	Boolean tokensOk = 
		AssertEqual(Assembler::GetTokens("   LOAD r5, r6 # comment"),
		   List<String>::New { "LOAD", "r5", "r6" })
	&&  AssertEqual(Assembler::GetTokens("  NOOP  "),
		   List<String>::New { "NOOP" })
	&&  AssertEqual(Assembler::GetTokens(" # comment only"),
		   List<String>::New())
	&&  AssertEqual(Assembler::GetTokens("LOAD r1, \"Hello world\""),
		   List<String>::New { "LOAD", "r1", "\"Hello world\"" })
	&&  AssertEqual(Assembler::GetTokens("LOAD r2, \"test\" # comment after string"),
		   List<String>::New { "LOAD", "r2", "\"test\"" });
	
	if (!tokensOk) return Boolean(false);
	
	// Test instruction assembly
	Assembler assem =  Assembler::New();
	
	// Test NOOP
	Boolean asmOk = AssertEqual(assem.AddLine("NOOP"), 
		BytecodeUtil::INS(Opcode::NOOP));
	
	// Test LOAD variants
	asmOk = asmOk && AssertEqual(assem.AddLine("LOAD r5, r3"), 
		BytecodeUtil::INS_ABC(Opcode::LOAD_rA_rB, 5, 3, 0));
	
	asmOk = asmOk && AssertEqual(assem.AddLine("LOAD r2, 42"), 
		BytecodeUtil::INS_AB(Opcode::LOAD_rA_iBC, 2, 42));
	
	asmOk = asmOk && AssertEqual(assem.AddLine("LOAD r7, k15"), 
		BytecodeUtil::INS_AB(Opcode::LOAD_rA_kBC, 7, 15));
	
	// Test arithmetic
	asmOk = asmOk && AssertEqual(assem.AddLine("ADD r1, r2, r3"), 
		BytecodeUtil::INS_ABC(Opcode::ADD_rA_rB_rC, 1, 2, 3));
	
	asmOk = asmOk && AssertEqual(assem.AddLine("SUB r4, r5, r6"), 
		BytecodeUtil::INS_ABC(Opcode::SUB_rA_rB_rC, 4, 5, 6));
	
	// Test control flow
	asmOk = asmOk && AssertEqual(assem.AddLine("JUMP 10"), 
		BytecodeUtil::INS(Opcode::JUMP_iABC) | (UInt32)(10 & 0xFFFFFF));
	
	asmOk = asmOk && AssertEqual(assem.AddLine("IFLT r8, r9"), 
		BytecodeUtil::INS_ABC(Opcode::IFLT_rA_rB, 8, 9, 0));
	
	asmOk = asmOk && AssertEqual(assem.AddLine("RETURN"), 
		BytecodeUtil::INS(Opcode::RETURN));
	
	// Test label assembly with two-pass approach
	List<String> labelTest =  List<String>::New {
		"NOOP",
		"loop:",
		"LOAD r1, 42",
		"SUB r1, r1, r0", 
		"IFLT r1, r0",
		"JUMP loop",
		"RETURN"
	};
	
	Assembler labelAssem =  Assembler::New();
	labelAssem.Assemble(labelTest);
	
	// Find the @main function
	FuncDef mainFunc = labelAssem.FindFunction("@main");
	asmOk = asmOk && Assert(mainFunc, "@main function not found");
	
	// Verify the assembled instructions
	asmOk = asmOk && AssertEqual(mainFunc.Code().Count(), 6); // 6 instructions (label doesn't count)
	
	// Check that JUMP loop resolves to correct relative offset
	// loop is at instruction 1, JUMP is at instruction 5, so offset should be 1-5 = -4
	UInt32 jumpInstruction = mainFunc.Code()[4]; // 5th instruction (0-indexed)
	UInt32 expectedJump = BytecodeUtil::INS(Opcode::JUMP_iABC) | (UInt32)((-4) & 0xFFFFFF);
	asmOk = asmOk && AssertEqual(jumpInstruction, expectedJump);
	
	// Test constant support
	List<String> constantTest =  List<String>::New {
		"LOAD r0, \"hello\"",    // Should use constant index 0
		"LOAD r1, 3.14",        // Should use constant index 1  
		"LOAD r2, 100000"       // Should use constant index 2
	};
	
	Assembler constAssem =  Assembler::New();
	constAssem.Assemble(constantTest);
	
	FuncDef constFunc = constAssem.FindFunction("@main");
	asmOk = asmOk && Assert(constFunc, "@main function not found in constant test");
	
	// Verify the assembled instructions use correct constant indices
	asmOk = asmOk && AssertEqual(constFunc.Code()[0], 
		BytecodeUtil::INS_AB(Opcode::LOAD_rA_kBC, 0, 0)); // Should use constant index 0
	asmOk = asmOk && AssertEqual(constFunc.Code()[1],
		BytecodeUtil::INS_AB(Opcode::LOAD_rA_kBC, 1, 1)); // Should use constant index 1
	asmOk = asmOk && AssertEqual(constFunc.Code()[2],
		BytecodeUtil::INS_AB(Opcode::LOAD_rA_kBC, 2, 2)); // Should use constant index 2
	
	// Verify we have 3 constants
	asmOk = asmOk && AssertEqual(constFunc.Constants().Count(), 3);
	
	// Test small integer (should use immediate form, not constant)
	List<String> immediateTest =  List<String>::New {
		"LOAD r3, 42"  // Should use immediate, not constant
	};
	
	Assembler immediateAssem =  Assembler::New();
	immediateAssem.Assemble(immediateTest);
	
	FuncDef immediateFunc = immediateAssem.FindFunction("@main");
	asmOk = asmOk && Assert(immediateFunc, "@main function not found in immediate test");
	
	asmOk = asmOk && AssertEqual(immediateFunc.Code()[0],
		BytecodeUtil::INS_AB(Opcode::LOAD_rA_iBC, 3, 42)); // Should use immediate
	asmOk = asmOk && AssertEqual(immediateFunc.Constants().Count(), 0); // No constants added
	
	// Test two-pass assembly with multiple constants and instructions
	List<String> multiTest =  List<String>::New {
		"LOAD r1, \"Hello\"",
		"LOAD r2, \"World\"", 
		"ADD r0, r1, r2",
		"RETURN"
	};
	
	Assembler multiAssem =  Assembler::New();
	multiAssem.Assemble(multiTest);
	
	FuncDef multiFunc = multiAssem.FindFunction("@main");
	asmOk = asmOk && Assert(multiFunc, "@main function not found in multi test");
	
	// Check that we have 2 constants
	asmOk = asmOk && AssertEqual(multiFunc.Constants().Count(), 2);
	
	// Check that we have 4 instructions
	asmOk = asmOk && AssertEqual(multiFunc.Code().Count(), 4);
	
	// Check specific instructions
	if (multiFunc.Code().Count() >= 4) {
		// First instruction: LOAD r1, k0 (where k0 = "Hello")
		asmOk = asmOk && AssertEqual(multiFunc.Code()[0],
			BytecodeUtil::INS_AB(Opcode::LOAD_rA_kBC, 1, 0));
		
		// Second instruction: LOAD r2, k1 (where k1 = "World")
		asmOk = asmOk && AssertEqual(multiFunc.Code()[1],
			BytecodeUtil::INS_AB(Opcode::LOAD_rA_kBC, 2, 1));
		
		// Third instruction: ADD r0, r1, r2
		asmOk = asmOk && AssertEqual(multiFunc.Code()[2],
			BytecodeUtil::INS_ABC(Opcode::ADD_rA_rB_rC, 0, 1, 2));
		
		// Fourth instruction: RETURN
		asmOk = asmOk && AssertEqual(multiFunc.Code()[3],
			BytecodeUtil::INS(Opcode::RETURN));
	}
	
	return asmOk;
}
Boolean UnitTests::TestValueMap() {
	// Test map creation
	Value map = make_empty_map(); GC_PROTECT(&map);
	Boolean basicOk = Assert(is_map(map), "Map should be identified as map")
		&& AssertEqual(map_count(map), 0);

	if (!basicOk) return Boolean(false);

	// Test insertion and lookup
	Value key1 = make_string("name"); GC_PROTECT(&key1);
	Value value1 = make_string("John"); GC_PROTECT(&value1);
	Value key2 = make_string("age"); GC_PROTECT(&key2);
	Value value2 = make_int(30); GC_PROTECT(&value2);

	Boolean insertOk = map_set(map, key1, value1)
		&& map_set(map, key2, value2)
		&& AssertEqual(map_count(map), 2);

	if (!insertOk) return Boolean(false);

	// Test lookup
	Value retrieved1 = map_get(map, key1); GC_PROTECT(&retrieved1);
	Value retrieved2 = map_get(map, key2); GC_PROTECT(&retrieved2);
	Boolean lookupOk = Assert(is_string(retrieved1), "Retrieved value should be string")
		&& Assert(is_int(retrieved2), "Retrieved value should be int")
		&& AssertEqual(as_int(retrieved2), 30);

	if (!lookupOk) return Boolean(false);

	// Test key existence
	Boolean hasKeyOk = Assert(map_has_key(map, key1), "Should have key1")
		&& Assert(map_has_key(map, key2), "Should have key2")
		&& Assert(!map_has_key(map, make_string("nonexistent")), "Should not have nonexistent key");

	if (!hasKeyOk) return Boolean(false);

	// Test lookup of nonexistent key
	// (For now; later: UnitTests(shared_from_this()) should invoke error-handling pipeline)
	Value nonexistent = map_get(map, make_string("missing")); GC_PROTECT(&nonexistent);
	Boolean nonexistentOk = Assert(is_null(nonexistent), "Nonexistent key should return null");

	if (!nonexistentOk) return Boolean(false);

	// Test removal
	Boolean removeOk = Assert(map_remove(map, key1), "Should successfully remove existing key")
		&& AssertEqual(map_count(map), 1)
		&& Assert(!map_has_key(map, key1), "Should no longer have removed key")
		&& Assert(map_has_key(map, key2), "Should still have other key")
		&& Assert(!map_remove(map, key1), "Should return false when removing nonexistent key");

	if (!removeOk) return Boolean(false);

	// Test string conversion (runtime C functions)
	Value singleMap = make_empty_map(); GC_PROTECT(&singleMap);
	map_set(singleMap, make_string("test"), make_int(42));
	Value singleStr = to_string(singleMap); GC_PROTECT(&singleStr);
	Boolean singleStrOk = Assert(is_string(singleStr), "Map toString should return string")
		&& AssertEqual(as_cstring(singleStr), "{\"test\": 42}");
	if (!singleStrOk) return Boolean(false);
	String result = StringUtils::Format("{0}", singleMap);
	if (!AssertEqual(result, "{\"test\": 42}")) return Boolean(false);

	// Note: We have successfully implemented and tested both conversion approaches:
	// 1. Runtime C functions (list_to_string, map_to_string) → GC Value strings
	// 2. Host-level C++ functions (StringUtils::makeString) → StringPool String
	// Both are working correctly in their respective contexts::

	// Test clearing
	map_clear(map);
	Boolean clearOk = AssertEqual(map_count(map), 0);

	return clearOk;
}
Boolean UnitTests::RunAll() {
	return TestStringUtils()
		&& TestDisassembler()
		&& TestAssembler()
		&& TestValueMap();
}


} // end of namespace MiniScript
