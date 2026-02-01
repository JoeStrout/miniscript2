// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: UnitTests.cs

#include "UnitTests.g.h"
#include "IOHelper.g.h"
#include "StringUtils.g.h"
#include "Disassembler.g.h"
#include "gc.h"
#include "Assembler.g.h"  // We really should automate this.
#include "Parser.g.h"
#include "AST.g.h"
#include "CodeEmitter.g.h"
#include "CodeGenerator.g.h"

namespace MiniScript {


Boolean UnitTests::Assert(bool condition, String message) {
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
	&&  AssertEqual( String::New("abcdef").Left(3), "abc")
	&&	AssertEqual( String::New("abcdef").Right(3), "def");
}
Boolean UnitTests::TestDisassembler() {
	return
		AssertEqual(Disassembler::ToString(0x01050A00), "LOAD    r5, r10");
}
Boolean UnitTests::TestAssembler() {
	// Test tokenization
	Boolean tokensOk = 
		AssertEqual(Assembler::GetTokens("   LOAD r5, r6 # comment"),
		   List<String>::New({ "LOAD", "r5", "r6" }))
	&&  AssertEqual(Assembler::GetTokens("  NOOP  "),
		   List<String>::New({ "NOOP" }))
	&&  AssertEqual(Assembler::GetTokens(" # comment only"),
		   List<String>::New())
	&&  AssertEqual(Assembler::GetTokens("LOAD r1, \"Hello world\""),
		   List<String>::New({ "LOAD", "r1", "\"Hello world\"" }))
	&&  AssertEqual(Assembler::GetTokens("LOAD r2, \"test\" # comment after string"),
		   List<String>::New({ "LOAD", "r2", "\"test\"" }));
	
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
	List<String> labelTest =  List<String>::New({
		"NOOP",
		"loop:",
		"LOAD r1, 42",
		"SUB r1, r1, r0", 
		"IFLT r1, r0",
		"JUMP loop",
		"RETURN"
	});
	
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
	List<String> constantTest =  List<String>::New({
		"LOAD r0, \"hello\"",    // Should use constant index 0
		"LOAD r1, 3.14",        // Should use constant index 1  
		"LOAD r2, 100000"       // Should use constant index 2
	});
	
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
	List<String> immediateTest =  List<String>::New({ "LOAD r3, 42" });
	
	Assembler immediateAssem =  Assembler::New();
	immediateAssem.Assemble(immediateTest);
	
	FuncDef immediateFunc = immediateAssem.FindFunction("@main");
	asmOk = asmOk && Assert(immediateFunc, "@main function not found in immediate test");
	
	asmOk = asmOk && AssertEqual(immediateFunc.Code()[0],
		BytecodeUtil::INS_AB(Opcode::LOAD_rA_iBC, 3, 42)); // Should use immediate
	asmOk = asmOk && AssertEqual(immediateFunc.Constants().Count(), 0); // No constants added
	
	// Test two-pass assembly with multiple constants and instructions
	List<String> multiTest =  List<String>::New({
		"LOAD r1, \"Hello\"",
		"LOAD r2, \"World\"", 
		"ADD r0, r1, r2",
		"RETURN"
	});
	
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
	GC_PUSH_SCOPE();
	Value map = make_empty_map(); GC_PROTECT(&map);
	Boolean basicOk = Assert(is_map(map), "Map should be identified as map")
		&& AssertEqual(map_count(map), 0);

	if (!basicOk)  {
		GC_POP_SCOPE();
		return Boolean(false);
	}

	// Test insertion and lookup
	Value key1 = make_string("name"); GC_PROTECT(&key1);
	Value value1 = make_string("John"); GC_PROTECT(&value1);
	Value key2 = make_string("age"); GC_PROTECT(&key2);
	Value value2 = make_int(30); GC_PROTECT(&value2);

	Boolean insertOk = map_set(map, key1, value1)
		&& map_set(map, key2, value2)
		&& AssertEqual(map_count(map), 2);

	if (!insertOk)  {
		GC_POP_SCOPE();
		return Boolean(false);
	}

	// Test lookup
	Value retrieved1 = map_get(map, key1); GC_PROTECT(&retrieved1);
	Value retrieved2 = map_get(map, key2); GC_PROTECT(&retrieved2);
	Boolean lookupOk = Assert(is_string(retrieved1), "Retrieved value should be string")
		&& Assert(is_int(retrieved2), "Retrieved value should be int")
		&& AssertEqual(as_int(retrieved2), 30);

	if (!lookupOk)  {
		GC_POP_SCOPE();
		return Boolean(false);
	}

	// Test key existence
	Boolean hasKeyOk = Assert(map_has_key(map, key1), "Should have key1")
		&& Assert(map_has_key(map, key2), "Should have key2")
		&& Assert(!map_has_key(map, make_string("nonexistent")), "Should not have nonexistent key");

	if (!hasKeyOk)  {
		GC_POP_SCOPE();
		return Boolean(false);
	}

	// Test lookup of nonexistent key
	// (For now; later: this should invoke error-handling pipeline)
	Value nonexistent = map_get(map, make_string("missing")); GC_PROTECT(&nonexistent);
	Boolean nonexistentOk = Assert(is_null(nonexistent), "Nonexistent key should return null");

	if (!nonexistentOk)  {
		GC_POP_SCOPE();
		return Boolean(false);
	}

	// Test removal
	Boolean removeOk = Assert(map_remove(map, key1), "Should successfully remove existing key")
		&& AssertEqual(map_count(map), 1)
		&& Assert(!map_has_key(map, key1), "Should no longer have removed key")
		&& Assert(map_has_key(map, key2), "Should still have other key")
		&& Assert(!map_remove(map, key1), "Should return false when removing nonexistent key");

	if (!removeOk)  {
		GC_POP_SCOPE();
		return Boolean(false);
	}

	// Test string conversion (runtime C functions)
	Value singleMap = make_empty_map(); GC_PROTECT(&singleMap);
	map_set(singleMap, make_string("test"), make_int(42));
	Value singleStr = to_string(singleMap); GC_PROTECT(&singleStr);
	Boolean singleStrOk = Assert(is_string(singleStr), "Map toString should return string")
		&& AssertEqual(as_cstring(singleStr), "{\"test\": 42}");
	if (!singleStrOk)  {
		GC_POP_SCOPE();
		return Boolean(false);
	}
	String result = StringUtils::Format("{0}", singleMap);
	if (!AssertEqual(result, "{\"test\": 42}"))  {
		GC_POP_SCOPE();
		return Boolean(false);
	}

	// Note: We have successfully implemented and tested both conversion approaches:
	// 1. Runtime C functions (list_to_string, map_to_string) → GC Value strings
	// 2. Host-level C++ functions (StringUtils::makeString) → StringPool String
	// Both are working correctly in their respective contexts.

	// Test clearing
	map_clear(map);
	Boolean clearOk = AssertEqual(map_count(map), 0);

	GC_POP_SCOPE();
	return clearOk;
}
Boolean UnitTests::CheckParse(Parser parser, String input, String expected) {
	ASTNode ast = parser.Parse(input);
	if (parser.HadError()) {
		IOHelper::Print(Interp("Parse error for input: {}", input));
		return Boolean(false);
	}
	ASTNode simplified = ast.Simplify();
	String result = simplified.ToStr();
	if (result != expected) {
		IOHelper::Print(Interp("Parser test failed for: {}", input));
		IOHelper::Print(Interp("  Expected: {}", expected));
		IOHelper::Print(Interp("  Got:      {}", result));
		return Boolean(false);
	}
	return Boolean(true);
}
Boolean UnitTests::TestParser() {
	IOHelper::Print("  Testing parser...");
	Parser parser =  Parser::New();
	Boolean ok = Boolean(true);

	// Test simple numbers
	ok = ok && CheckParse(parser, "42", "42");
	ok = ok && CheckParse(parser, "3.14", "3.14");

	// Test simple arithmetic with constant folding
	ok = ok && CheckParse(parser, "2 + 3", "5");
	ok = ok && CheckParse(parser, "10 - 4", "6");
	ok = ok && CheckParse(parser, "6 * 7", "42");
	ok = ok && CheckParse(parser, "20 / 4", "5");
	ok = ok && CheckParse(parser, "17 % 5", "2");

	// Test precedence (multiplication before addition)
	ok = ok && CheckParse(parser, "2 + 3 * 4", "14");
	ok = ok && CheckParse(parser, "2 * 3 + 4", "10");

	// Test parentheses override precedence
	ok = ok && CheckParse(parser, "(2 + 3) * 4", "20");

	// Test unary minus
	ok = ok && CheckParse(parser, "-5", "-5");
	ok = ok && CheckParse(parser, "10 + -3", "7");

	// Test power operator (right associative)
	ok = ok && CheckParse(parser, "2 ^ 3", "8");
	ok = ok && CheckParse(parser, "2 ^ 3 ^ 2", "512");  // 2^(3^2) = 2^9 = 512

	// Test comparison operators (result is 1 for true, 0 for false)
	ok = ok && CheckParse(parser, "5 == 5", "1");
	ok = ok && CheckParse(parser, "5 == 6", "0");
	ok = ok && CheckParse(parser, "5 != 6", "1");
	ok = ok && CheckParse(parser, "3 < 5", "1");
	ok = ok && CheckParse(parser, "5 < 3", "0");
	ok = ok && CheckParse(parser, "5 <= 5", "1");
	ok = ok && CheckParse(parser, "5 > 3", "1");
	ok = ok && CheckParse(parser, "5 >= 5", "1");

	// Test logical operators
	ok = ok && CheckParse(parser, "1 and 1", "1");
	ok = ok && CheckParse(parser, "1 and 0", "0");
	ok = ok && CheckParse(parser, "0 or 1", "1");
	ok = ok && CheckParse(parser, "0 or 0", "0");
	ok = ok && CheckParse(parser, "not 0", "1");
	ok = ok && CheckParse(parser, "not 1", "0");

	// Test identifiers (these don't simplify, just return as-is)
	ok = ok && CheckParse(parser, "x", "x");
	ok = ok && CheckParse(parser, "foo", "foo");

	// Test expressions with identifiers (partial simplification)
	ok = ok && CheckParse(parser, "x + 0", "PLUS(x, 0)");
	ok = ok && CheckParse(parser, "2 + x", "PLUS(2, x)");

	// Test string literals
	ok = ok && CheckParse(parser, "\"hello\"", "\"hello\"");

	// Test list literals
	ok = ok && CheckParse(parser, "[]", "[]");
	ok = ok && CheckParse(parser, "[1, 2, 3]", "[1, 2, 3]");

	// Test map literals
	ok = ok && CheckParse(parser, "{}", "{}");

	// Test function calls (don't simplify)
	ok = ok && CheckParse(parser, "sqrt(4)", "sqrt(4)");
	ok = ok && CheckParse(parser, "max(1, 2)", "max(1, 2)");

	// Test index access
	ok = ok && CheckParse(parser, "list[0]", "list[0]");
	ok = ok && CheckParse(parser, "map[\"key\"]", "map[\"key\"]");

	// Test member access
	ok = ok && CheckParse(parser, "obj.field", "obj.field");

	// Test chained operations
	ok = ok && CheckParse(parser, "a.b.c", "a.b.c");
	ok = ok && CheckParse(parser, "list[0][1]", "list[0][1]");
	ok = ok && CheckParse(parser, "obj.method(x)", "obj.method(x)");

	// Test complex expressions with mixed operators
	ok = ok && CheckParse(parser, "1 + 2 * 3 - 4", "3");  // 1 + 6 - 4 = 3
	ok = ok && CheckParse(parser, "10 / 2 + 3 * 4", "17");  // 5 + 12 = 17

	// Test nested parentheses
	ok = ok && CheckParse(parser, "((1 + 2))", "3");
	ok = ok && CheckParse(parser, "((2 + 3) * (4 + 5))", "45");

	// Test assignment (returns assignment node, doesn't simplify)
	ok = ok && CheckParse(parser, "x = 5", "x = 5");
	ok = ok && CheckParse(parser, "y = 2 + 3", "y = 5");

	if (ok) {
		IOHelper::Print("  All parser tests passed.");
	}

	return ok;
}
Boolean UnitTests::CheckCodeGen(Parser parser, String input, List<String> expectedLines) {
	ASTNode ast = parser.Parse(input);
	if (parser.HadError()) {
		IOHelper::Print(Interp("Parse error for input: {}", input));
		return Boolean(false);
	}

	AssemblyEmitter emitter =  AssemblyEmitter::New();
	CodeGenerator gen =  CodeGenerator::New(emitter);
	gen.CompileFunction(ast, "@main");

	List<String> actualLines = emitter.GetLines();

	// Compare line by line (ignoring comments)
	if (actualLines.Count() != expectedLines.Count()) {
		IOHelper::Print(Interp("CodeGen test failed for: {}", input));
		IOHelper::Print(Interp("  Expected {} lines, got {}", expectedLines.Count(), actualLines.Count()));
		IOHelper::Print("  Actual output:");
		for (Int32 i = 0; i < actualLines.Count(); i++) {
			IOHelper::Print(Interp("    {}", actualLines[i]));
		}
		return Boolean(false);
	}

	for (Int32 i = 0; i < expectedLines.Count(); i++) {
		// Strip comments from actual line for comparison
		String actual = actualLines[i];
		Int32 commentPos = actual::IndexOf(';');
		if (commentPos >= 0) actual = actual.Substring(0, commentPos).TrimEnd();

		String expected = expectedLines[i];
		if (actual != expected) {
			IOHelper::Print(Interp("CodeGen test failed for: {}", input));
			IOHelper::Print(Interp("  Line {}: expected \"{}\" but got \"{}\"", i, expected, actual));
			return Boolean(false);
		}
	}

	return Boolean(true);
}
Boolean UnitTests::CheckBytecodeGen(Parser parser, String input, Int32 expectedInstructions, Int32 expectedConstants) {
	ASTNode ast = parser.Parse(input);
	if (parser.HadError()) {
		IOHelper::Print(Interp("Parse error for input: {}", input));
		return Boolean(false);
	}

	BytecodeEmitter emitter =  BytecodeEmitter::New();
	CodeGenerator gen =  CodeGenerator::New(emitter);
	FuncDef func = gen.CompileFunction(ast, "@main");

	if (func.Code().Count() != expectedInstructions) {
		IOHelper::Print(Interp("BytecodeGen test failed for: {}", input));
		IOHelper::Print(Interp("  Expected {} instructions, got {}", expectedInstructions, func.Code().Count()));
		return Boolean(false);
	}

	if (func.Constants().Count() != expectedConstants) {
		IOHelper::Print(Interp("BytecodeGen test failed for: {}", input));
		IOHelper::Print(Interp("  Expected {} constants, got {}", expectedConstants, func.Constants().Count()));
		return Boolean(false);
	}

	return Boolean(true);
}
Boolean UnitTests::TestCodeGenerator() {
	IOHelper::Print("  Testing code generator...");
	Parser parser =  Parser::New();
	Boolean ok = Boolean(true);

	// Test simple integer (immediate form, no constants)
	ok = ok && CheckBytecodeGen(parser, "42", 2, 0);  // LOAD + RETURN

	// Test float (requires constant)
	ok = ok && CheckBytecodeGen(parser, "3.14", 2, 1);  // LOAD_kBC + RETURN

	// Test large integer (requires constant)
	ok = ok && CheckBytecodeGen(parser, "100000", 2, 1);  // LOAD_kBC + RETURN

	// Test string (requires constant)
	ok = ok && CheckBytecodeGen(parser, "\"hello\"", 2, 1);  // LOAD_kBC + RETURN

	// Test simple addition
	// With resultReg allocated first: LOAD r1,2; LOAD r2,3; ADD r0,r1,r2; RETURN
	ok = ok && CheckBytecodeGen(parser, "2 + 3", 4, 0);

	// Test assembly output for simple number
	ok = ok && CheckCodeGen(parser, "42",  List<String>::New({
		"  LOAD_rA_iBC r0, 42",
		"  RETURN"
	});

	// Test assembly output for addition (resultReg r0 allocated first)
	ok = ok && CheckCodeGen(parser, "2 + 3",  List<String>::New({
		"  LOAD_rA_iBC r1, 2",
		"  LOAD_rA_iBC r2, 3",
		"  ADD_rA_rB_rC r0, r1, r2",
		"  RETURN"
	});

	// Test subtraction
	ok = ok && CheckCodeGen(parser, "10 - 4",  List<String>::New({
		"  LOAD_rA_iBC r1, 10",
		"  LOAD_rA_iBC r2, 4",
		"  SUB_rA_rB_rC r0, r1, r2",
		"  RETURN"
	});

	// Test multiplication
	ok = ok && CheckCodeGen(parser, "6 * 7",  List<String>::New({
		"  LOAD_rA_iBC r1, 6",
		"  LOAD_rA_iBC r2, 7",
		"  MULT_rA_rB_rC r0, r1, r2",
		"  RETURN"
	});

	// Test division
	ok = ok && CheckCodeGen(parser, "20 / 4",  List<String>::New({
		"  LOAD_rA_iBC r1, 20",
		"  LOAD_rA_iBC r2, 4",
		"  DIV_rA_rB_rC r0, r1, r2",
		"  RETURN"
	});

	// Test comparison (less than)
	ok = ok && CheckCodeGen(parser, "3 < 5",  List<String>::New({
		"  LOAD_rA_iBC r1, 3",
		"  LOAD_rA_iBC r2, 5",
		"  LT_rA_rB_rC r0, r1, r2",
		"  RETURN"
	});

	// Test comparison (greater than - uses swapped LT)
	ok = ok && CheckCodeGen(parser, "5 > 3",  List<String>::New({
		"  LOAD_rA_iBC r1, 5",
		"  LOAD_rA_iBC r2, 3",
		"  LT_rA_rB_rC r0, r2, r1",  // swapped: r2 < r1
		"  RETURN"
	});

	// Test unary minus
	// r0 = 5, r1 = result, r2 = 0, SUB r1, r2, r0 (result = 0 - 5)
	ok = ok && CheckCodeGen(parser, "-5",  List<String>::New({
		"  LOAD_rA_iBC r0, 5",
		"  LOAD_rA_iBC r2, 0",
		"  SUB_rA_rB_rC r1, r2, r0",
		"  LOAD_rA_rB r0, r1",
		"  RETURN"
	});

	// Test grouping (parentheses) - should compile inner expression directly
	ok = ok && CheckCodeGen(parser, "(42)",  List<String>::New({
		"  LOAD_rA_iBC r0, 42",
		"  RETURN"
	});

	// Test list literal
	ok = ok && CheckCodeGen(parser, "[1, 2, 3]",  List<String>::New({
		"  LIST_rA_iBC r0, 3",
		"  LOAD_rA_iBC r1, 1",
		"  PUSH_rA_rB r0, r1",
		"  LOAD_rA_iBC r1, 2",
		"  PUSH_rA_rB r0, r1",
		"  LOAD_rA_iBC r1, 3",
		"  PUSH_rA_rB r0, r1",
		"  RETURN"
	});

	// Test empty list
	ok = ok && CheckCodeGen(parser, "[]",  List<String>::New({
		"  LIST_rA_iBC r0, 0",
		"  RETURN"
	});

	// Test map literal
	ok = ok && CheckBytecodeGen(parser, "{}", 2, 0);  // MAP + RETURN

	// Test index access (resultReg r0 allocated first)
	ok = ok && CheckCodeGen(parser, "x[0]",  List<String>::New({
		"  LOAD_rA_iBC r1, 0",   // TODO: load x
		"  LOAD_rA_iBC r2, 0",   // index 0
		"  INDEX_rA_rB_rC r0, r1, r2",
		"  RETURN"
	});

	// Test nested expression (precedence)
	// 2 + 3 * 4: outer result r0, load 2 into r1, inner mult result r2, load 3,4 into r3,r4
	// LOAD r1,2; LOAD r3,3; LOAD r4,4; MULT r2,r3,r4; ADD r0,r1,r2; RETURN
	ok = ok && CheckBytecodeGen(parser, "2 + 3 * 4", 6, 0);

	// Test register reuse with nested expressions
	// (1 + 2) + (3 + 4): outer r0, first group r1 (with r2,r3 for operands),
	// second group r2 (reused after freeing r2,r3), with r3,r4 for operands
	// LOAD r2,1; LOAD r3,2; ADD r1,r2,r3; LOAD r3,3; LOAD r4,4; ADD r2,r3,r4; ADD r0,r1,r2; RETURN
	ok = ok && CheckBytecodeGen(parser, "(1 + 2) + (3 + 4)", 8, 0);

	if (ok) {
		IOHelper::Print("  All code generator tests passed.");
	}

	return ok;
}
Boolean UnitTests::TestEmitPatternValidation() {
	IOHelper::Print("  Testing emit pattern validation...");
	Boolean ok = Boolean(true);

	// Test that GetEmitPattern correctly identifies patterns
	ok = ok && Assert(BytecodeUtil::GetEmitPattern(Opcode::RETURN) == EmitPattern::None,
		"RETURN should be EmitPattern.None");
	ok = ok && Assert(BytecodeUtil::GetEmitPattern(Opcode::NOOP) == EmitPattern::None,
		"NOOP should be EmitPattern.None");
	ok = ok && Assert(BytecodeUtil::GetEmitPattern(Opcode::LOCALS_rA) == EmitPattern::A,
		"LOCALS_rA should be EmitPattern.A");
	ok = ok && Assert(BytecodeUtil::GetEmitPattern(Opcode::ARG_rA) == EmitPattern::A,
		"ARG_rA should be EmitPattern.A");
	ok = ok && Assert(BytecodeUtil::GetEmitPattern(Opcode::LOAD_rA_iBC) == EmitPattern::AB,
		"LOAD_rA_iBC should be EmitPattern.AB");
	ok = ok && Assert(BytecodeUtil::GetEmitPattern(Opcode::LOAD_rA_rB) == EmitPattern::AB,
		"LOAD_rA_rB should be EmitPattern.AB");
	ok = ok && Assert(BytecodeUtil::GetEmitPattern(Opcode::IFLT_iAB_rC) == EmitPattern::BC,
		"IFLT_iAB_rC should be EmitPattern.BC");
	ok = ok && Assert(BytecodeUtil::GetEmitPattern(Opcode::ADD_rA_rB_rC) == EmitPattern::ABC,
		"ADD_rA_rB_rC should be EmitPattern.ABC");
	ok = ok && Assert(BytecodeUtil::GetEmitPattern(Opcode::LT_rA_rB_iC) == EmitPattern::ABC,
		"LT_rA_rB_iC should be EmitPattern.ABC");

	if (ok) {
		IOHelper::Print("  All emit pattern validation tests passed.");
	}
	return ok;
}
Boolean UnitTests::RunAll() {
	return TestStringUtils()
		&& TestDisassembler()
		&& TestAssembler()
		&& TestValueMap()
		&& TestParser()
		&& TestCodeGenerator()
		&& TestEmitPatternValidation();
}


} // end of namespace MiniScript
