// This module gathers together all the unit tests for this prototype.
// Each test returns true on success, false on failure.

using System;
using System.Collections.Generic;
// CPP: #include "CS_value_util.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "Disassembler.g.h"
// CPP: #include "Assembler.g.h"  // We really should automate this.
// CPP: #include "Parser.g.h"
// CPP: #include "Lexer.g.h"
// CPP: #include "AST.g.h"
// CPP: #include "CodeEmitter.g.h"
// CPP: #include "CodeGenerator.g.h"
// CPP: #include "Interpreter.g.h"
// CPP: #include "Intrinsic.g.h"
// CPP: #include "CoreIntrinsics.g.h"

namespace MiniScript {

public static class UnitTests {

	public static Boolean Assert(bool condition, String message) {
		if (condition) return true;
		IOHelper.Print(new String("Unit test failure: ") + message);
		return false;
	}
	
	public static Boolean AssertEqual(String actual, String expected) {
		if (actual == expected) return true;
		Assert(false, new String("Unit test failure: expected \"")
		  + expected + "\" but got \"" + actual + "\"");
		return false;
	}
		
	public static Boolean AssertEqual(Double actual, Double expected) {
		if (actual == expected) return true;
		Assert(false, StringUtils.Format("Unit test failure: expected {0} but got {1}",
				expected, actual));
		return false;
	}

	// Compare two UInt32 values, reporting any mismatch in hex (useful for
	// bytecode instructions).  Named distinctly from AssertEqual to avoid an
	// ambiguous overload with the Double version when given integer literals.
	public static Boolean AssertEqualU(UInt32 actual, UInt32 expected) {
		if (actual == expected) return true;
		Assert(false, new String("Unit test failure: expected 0x")
		  + StringUtils.ToHex(expected) + "\" but got 0x" + StringUtils.ToHex(actual));
		return false;
	}

	public static Boolean AssertEqual(List<String> actual, List<String> expected) {
		Boolean ok = true;
		if ((actual == null) != (expected == null)) ok = false; // CPP: // (no nulls)
		if (ok && actual.Count != expected.Count) ok = false;
		for (Int32 i = 0; ok && i < actual.Count; i++) {
			if (actual[i] != expected[i]) ok = false;
		}
		if (ok) return true;
		Assert(false, new String("Unit test failure: expected ")
		  + StringUtils.Str(expected) + " but got " + StringUtils.Str(actual));
		return false;
	}
		
	public static Boolean TestStringUtils() {
		return 
			AssertEqual(StringUtils.ToHex((UInt32)123456789), "075BCD15")
		&&  AssertEqual(new String("abcdef").Left(3), "abc")
		&&	AssertEqual(new String("abcdef").Right(3), "def");
	}
	
	public static Boolean TestDisassembler() {
		return
			AssertEqual(Disassembler.ToString(0x01050A00), "LOAD    r5, r10")
		// Global references print as gN, not kN: the operand indexes the
		// function's global-reference table, not its constant pool.
		&&	AssertEqual(Disassembler.ToString(BytecodeUtil.INS_AB(Opcode.GLOADC_rA_iBC, 3, 7)),
				"GLOADC  r3, g7")
		&&	AssertEqual(Disassembler.ToString(BytecodeUtil.INS_AB(Opcode.GLOADV_rA_iBC, 1, 0)),
				"GLOADV  r1, g0")
		&&	AssertEqual(Disassembler.ToString(BytecodeUtil.INS_AB(Opcode.GSTORE_rA_iBC, 2, 5)),
				"GSTORE  r2, g5");
	}
	
	public static Boolean TestAssembler() {
		// Test tokenization
		Boolean tokensOk = 
			AssertEqual(Assembler.GetTokens("   LOAD r5, r6 # comment"),
			  new List<String> { "LOAD", "r5", "r6" })
		&&  AssertEqual(Assembler.GetTokens("  NOOP  "),
			  new List<String> { "NOOP" })
		&&  AssertEqual(Assembler.GetTokens(" # comment only"),
			  new List<String>())
		&&  AssertEqual(Assembler.GetTokens("LOAD r1, \"Hello world\""),
			  new List<String> { "LOAD", "r1", "\"Hello world\"" })
		&&  AssertEqual(Assembler.GetTokens("LOAD r2, \"test\" # comment after string"),
			  new List<String> { "LOAD", "r2", "\"test\"" });
		
		if (!tokensOk) return false;
		
		// Test instruction assembly
		Assembler assem = new Assembler();
		
		// Test NOOP
		Boolean asmOk = AssertEqual(assem.AddLine("NOOP"), 
			BytecodeUtil.INS(Opcode.NOOP));
		
		// Test LOAD variants
		asmOk = asmOk && AssertEqualU(assem.AddLine("LOAD r5, r3"),
			BytecodeUtil.INS_ABC(Opcode.LOAD_rA_rB, 5, 3, 0));

		asmOk = asmOk && AssertEqualU(assem.AddLine("LOAD r2, 42"),
			BytecodeUtil.INS_AB(Opcode.LOAD_rA_iBC, 2, 42));

		asmOk = asmOk && AssertEqualU(assem.AddLine("LOAD r7, k15"),
			BytecodeUtil.INS_AB(Opcode.LOAD_rA_kBC, 7, 15));

		// Test arithmetic
		asmOk = asmOk && AssertEqualU(assem.AddLine("ADD r1, r2, r3"),
			BytecodeUtil.INS_ABC(Opcode.ADD_rA_rB_rC, 1, 2, 3));

		asmOk = asmOk && AssertEqualU(assem.AddLine("SUB r4, r5, r6"),
			BytecodeUtil.INS_ABC(Opcode.SUB_rA_rB_rC, 4, 5, 6));

		// Test control flow
		asmOk = asmOk && AssertEqualU(assem.AddLine("JUMP 10"),
			BytecodeUtil.INS(Opcode.JUMP_iABC) | (UInt32)(10 & 0xFFFFFF));

		asmOk = asmOk && AssertEqualU(assem.AddLine("IFLT r8, r9"),
			BytecodeUtil.INS_ABC(Opcode.IFLT_rA_rB, 8, 9, 0));

		asmOk = asmOk && AssertEqualU(assem.AddLine("RETURN"),
			BytecodeUtil.INS(Opcode.RETURN));

		// Global access.  The name is interned into the function's
		// global-reference table, so the first mention of "gx" is g0 and the
		// second mention of the same name reuses it.
		asmOk = asmOk && AssertEqualU(assem.AddLine("GSTORE r1, \"gx\""),
			BytecodeUtil.INS_AB(Opcode.GSTORE_rA_iBC, 1, 0));

		asmOk = asmOk && AssertEqualU(assem.AddLine("GLOADC r2, \"gy\""),
			BytecodeUtil.INS_AB(Opcode.GLOADC_rA_iBC, 2, 1));

		asmOk = asmOk && AssertEqualU(assem.AddLine("GLOADV r3, \"gx\""),
			BytecodeUtil.INS_AB(Opcode.GLOADV_rA_iBC, 3, 0));
		
		// Test label assembly with two-pass approach
		List<String> labelTest = new List<String> {
			"NOOP",
			"loop:",
			"LOAD r1, 42",
			"SUB r1, r1, r0", 
			"IFLT r1, r0",
			"JUMP loop",
			"RETURN"
		}; // CPP: });
		
		Assembler labelAssem = new Assembler();
		labelAssem.Assemble(labelTest);
		
		// Find the @main function
		FuncDef mainFunc = labelAssem.FindFunction("@main");
		asmOk = asmOk && Assert(mainFunc, "@main function not found");
		
		// Verify the assembled instructions
		asmOk = asmOk && AssertEqual(mainFunc.Code.Count, 6); // 6 instructions (label doesn't count)
		
		// Check that JUMP loop resolves to correct relative offset
		// loop is at instruction 1, JUMP is at instruction 5, so offset should be 1-5 = -4
		UInt32 jumpInstruction = mainFunc.Code[4]; // 5th instruction (0-indexed)
		UInt32 expectedJump = BytecodeUtil.INS(Opcode.JUMP_iABC) | (UInt32)((-4) & 0xFFFFFF);
		asmOk = asmOk && AssertEqualU(jumpInstruction, expectedJump);
		
		// Test constant support
		List<String> constantTest = new List<String> {
			"LOAD r0, \"hello\"",    // Should use constant index 0
			"LOAD r1, 3.14",        // Should use constant index 1  
			"LOAD r2, 100000"       // Should use constant index 2
		}; // CPP: });
		
		Assembler constAssem = new Assembler();
		constAssem.Assemble(constantTest);
		
		FuncDef constFunc = constAssem.FindFunction("@main");
		asmOk = asmOk && Assert(constFunc, "@main function not found in constant test");
		
		// Verify the assembled instructions use correct constant indices
		asmOk = asmOk && AssertEqualU(constFunc.Code[0],
			BytecodeUtil.INS_AB(Opcode.LOAD_rA_kBC, 0, 0)); // Should use constant index 0
		asmOk = asmOk && AssertEqualU(constFunc.Code[1],
			BytecodeUtil.INS_AB(Opcode.LOAD_rA_kBC, 1, 1)); // Should use constant index 1
		asmOk = asmOk && AssertEqualU(constFunc.Code[2],
			BytecodeUtil.INS_AB(Opcode.LOAD_rA_kBC, 2, 2)); // Should use constant index 2
		
		// Verify we have 3 constants
		asmOk = asmOk && AssertEqual(constFunc.Constants.Count, 3);
		
		// Test small integer (should use immediate form, not constant)
		List<String> immediateTest = new List<String> { "LOAD r3, 42" };
		
		Assembler immediateAssem = new Assembler();
		immediateAssem.Assemble(immediateTest);
		
		FuncDef immediateFunc = immediateAssem.FindFunction("@main");
		asmOk = asmOk && Assert(immediateFunc, "@main function not found in immediate test");
		
		asmOk = asmOk && AssertEqualU(immediateFunc.Code[0],
			BytecodeUtil.INS_AB(Opcode.LOAD_rA_iBC, 3, 42)); // Should use immediate
		asmOk = asmOk && AssertEqual(immediateFunc.Constants.Count, 0); // No constants added
		
		// Test two-pass assembly with multiple constants and instructions
		List<String> multiTest = new List<String> {
			"LOAD r1, \"Hello\"",
			"LOAD r2, \"World\"", 
			"ADD r0, r1, r2",
			"RETURN"
		}; // CPP: });
		
		Assembler multiAssem = new Assembler();
		multiAssem.Assemble(multiTest);
		
		FuncDef multiFunc = multiAssem.FindFunction("@main");
		asmOk = asmOk && Assert(multiFunc, "@main function not found in multi test");
		
		// Check that we have 2 constants
		asmOk = asmOk && AssertEqual(multiFunc.Constants.Count, 2);
		
		// Check that we have 4 instructions
		asmOk = asmOk && AssertEqual(multiFunc.Code.Count, 4);
		
		// Check specific instructions
		if (multiFunc.Code.Count >= 4) {
			// First instruction: LOAD r1, k0 (where k0 = "Hello")
			asmOk = asmOk && AssertEqualU(multiFunc.Code[0],
				BytecodeUtil.INS_AB(Opcode.LOAD_rA_kBC, 1, 0));

			// Second instruction: LOAD r2, k1 (where k1 = "World")
			asmOk = asmOk && AssertEqualU(multiFunc.Code[1],
				BytecodeUtil.INS_AB(Opcode.LOAD_rA_kBC, 2, 1));

			// Third instruction: ADD r0, r1, r2
			asmOk = asmOk && AssertEqualU(multiFunc.Code[2],
				BytecodeUtil.INS_ABC(Opcode.ADD_rA_rB_rC, 0, 1, 2));

			// Fourth instruction: RETURN
			asmOk = asmOk && AssertEqualU(multiFunc.Code[3],
				BytecodeUtil.INS(Opcode.RETURN));
		}
		
		return asmOk;
	}

	public static Boolean TestValueMap() {
		// Test map creation
		Value map = Value.make_empty_map();
		Boolean basicOk = Assert(map.IsMap(), "Map should be identified as map")
			&& AssertEqual(map.MapCount(), 0);

		if (!basicOk) return false;

		// Test insertion and lookup
		Value key1 = Value.make_string("name");
		Value value1 = Value.make_string("John");
		Value key2 = Value.make_string("age");
		Value value2 = new Value(30.0);

		Boolean insertOk = map.MapSet(key1, value1)
			&& map.MapSet(key2, value2)
			&& AssertEqual(map.MapCount(), 2);

		if (!insertOk) return false;

		// Test lookup
		Value retrieved1 = map.MapGet(key1);
		Value retrieved2 = map.MapGet(key2);
		Boolean lookupOk = Assert(retrieved1.IsString(), "Retrieved value should be string")
			&& Assert(retrieved2.IsNumber(), "Retrieved value should be number")
			&& AssertEqual((int)retrieved2.DoubleValue(), 30);

		if (!lookupOk) return false;

		// Test key existence
		Boolean hasKeyOk = Assert(map.HasKey(key1), "Should have key1")
			&& Assert(map.HasKey(key2), "Should have key2")
			&& Assert(!map.HasKey(Value.make_string("nonexistent")), "Should not have nonexistent key");

		if (!hasKeyOk) return false;

		// Test lookup of nonexistent key
		// (For now; later: this should invoke error-handling pipeline)
		Value nonexistent = map.MapGet(Value.make_string("missing"));
		Boolean nonexistentOk = Assert(nonexistent.IsNull(), "Nonexistent key should return null");

		if (!nonexistentOk) return false;

		// Test removal
		Boolean removeOk = Assert(map.MapRemove(key1), "Should successfully remove existing key")
			&& AssertEqual(map.MapCount(), 1)
			&& Assert(!map.HasKey(key1), "Should no longer have removed key")
			&& Assert(map.HasKey(key2), "Should still have other key")
			&& Assert(!map.MapRemove(key1), "Should return false when removing nonexistent key");

		if (!removeOk) return false;

		// Test string conversion (runtime C functions)
		Value singleMap = Value.make_empty_map();
		singleMap.MapSet("test", new Value(42));
		Value singleStr = singleMap.ToStringValue(null);
		Boolean singleStrOk = Assert(singleStr.IsString(), "Map toString should return string")
			&& AssertEqual(singleStr.AsCString(), "{\"test\": 42}");
		if (!singleStrOk) return false;
		String result = singleMap.ToStringValue(null).AsCString();
		if (!AssertEqual(result, "{\"test\": 42}")) return false;

		// Note: We have successfully implemented and tested both conversion approaches:
		// 1. Runtime C functions (list_to_string, map_to_string) → GC Value strings
		// 2. Host-level C++ functions (StringUtils::makeString) → StringPool String
		// Both are working correctly in their respective contexts.

		// Test clearing
		map.Clear();
		Boolean clearOk = AssertEqual(map.MapCount(), 0);

		return clearOk;
	}

	// Helper for parser tests: parse, simplify, and check result
	private static Boolean CheckParse(Parser parser, String input, String expected) {
		ASTNode ast = parser.Parse(input);
		if (parser.HadError()) {
			IOHelper.Print($"Parse error for input: {input}");
			return false;
		}
		ASTNode simplified = ast.Simplify();
		String result = simplified.ToStr();
		if (result != expected) {
			IOHelper.Print($"Parser test failed for: {input}");
			IOHelper.Print($"  Expected: {expected}");
			IOHelper.Print($"  Got:      {result}");
			return false;
		}
		return true;
	}

	public static Boolean TestParser() {
		//IOHelper.Print("  Testing parser...");
		Parser parser = new Parser();
		Boolean ok = true;

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

		return ok;
	}

	// Helper for code generator tests: parse, generate, and check assembly output
	private static Boolean CheckCodeGen(Parser parser, String input, List<String> expectedLines) {
		ASTNode ast = parser.Parse(input);
		if (parser.HadError()) {
			IOHelper.Print($"Parse error for input: {input}");
			return false;
		}

		AssemblyEmitter emitter = new AssemblyEmitter();
		CodeGenerator gen = new CodeGenerator(emitter);
		gen.CompileFunction(ast, "@main");

		List<String> actualLines = emitter.GetLines();

		// Compare line by line (ignoring comments)
		if (actualLines.Count != expectedLines.Count) {
			IOHelper.Print($"CodeGen test failed for: {input}");
			IOHelper.Print($"  Expected {expectedLines.Count} lines, got {actualLines.Count}");
			IOHelper.Print("  Actual output:");
			for (Int32 i = 0; i < actualLines.Count; i++) {
				IOHelper.Print($"    {actualLines[i]}");
			}
			return false;
		}

		for (Int32 i = 0; i < expectedLines.Count; i++) {
			// Strip comments from actual line for comparison
			String actual = actualLines[i];
			Int32 commentPos = actual.IndexOf(';');
			if (commentPos >= 0) actual = actual.Substring(0, commentPos).TrimEnd();

			String expected = expectedLines[i];
			if (actual != expected) {
				IOHelper.Print($"CodeGen test failed for: {input}");
				IOHelper.Print($"  Line {i}: expected \"{expected}\" but got \"{actual}\"");
				return false;
			}
		}

		return true;
	}

	// Helper to check bytecode generation produces valid FuncDef
	private static Boolean CheckBytecodeGen(Parser parser, String input, Int32 expectedInstructions, Int32 expectedConstants) {
		ASTNode ast = parser.Parse(input);
		if (parser.HadError()) {
			IOHelper.Print($"Parse error for input: {input}");
			return false;
		}

		BytecodeEmitter emitter = new BytecodeEmitter();
		CodeGenerator gen = new CodeGenerator(emitter);
		FuncDef func = gen.CompileFunction(ast, "@main");

		if (func.Code.Count != expectedInstructions) {
			IOHelper.Print($"BytecodeGen test failed for: {input}");
			IOHelper.Print($"  Expected {expectedInstructions} instructions, got {func.Code.Count}");
			return false;
		}

		if (func.Constants.Count != expectedConstants) {
			IOHelper.Print($"BytecodeGen test failed for: {input}");
			IOHelper.Print($"  Expected {expectedConstants} constants, got {func.Constants.Count}");
			return false;
		}

		return true;
	}

	public static Boolean TestCodeGenerator() {
		//IOHelper.Print("  Testing code generator...");
		Parser parser = new Parser();
		Boolean ok = true;

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
		ok = ok && CheckCodeGen(parser, "42", new List<String> {
			"  LOAD_rA_iBC r0, 42",
			"  RETURN"
		}); // CPP: }));

		// Test assembly output for addition (resultReg r0 allocated first)
		ok = ok && CheckCodeGen(parser, "2 + 3", new List<String> {
			"  LOAD_rA_iBC r1, 2",
			"  LOAD_rA_iBC r2, 3",
			"  ADD_rA_rB_rC r0, r1, r2",
			"  RETURN"
		}); // CPP: }));

		// Test subtraction
		ok = ok && CheckCodeGen(parser, "10 - 4", new List<String> {
			"  LOAD_rA_iBC r1, 10",
			"  LOAD_rA_iBC r2, 4",
			"  SUB_rA_rB_rC r0, r1, r2",
			"  RETURN"
		}); // CPP: }));

		// Test multiplication
		ok = ok && CheckCodeGen(parser, "6 * 7", new List<String> {
			"  LOAD_rA_iBC r1, 6",
			"  LOAD_rA_iBC r2, 7",
			"  MUL_rA_rB_rC r0, r1, r2",
			"  RETURN"
		}); // CPP: }));

		// Test division
		ok = ok && CheckCodeGen(parser, "20 / 4", new List<String> {
			"  LOAD_rA_iBC r1, 20",
			"  LOAD_rA_iBC r2, 4",
			"  DIV_rA_rB_rC r0, r1, r2",
			"  RETURN"
		}); // CPP: }));

		// Test comparison (less than)
		ok = ok && CheckCodeGen(parser, "3 < 5", new List<String> {
			"  LOAD_rA_iBC r1, 3",
			"  LOAD_rA_iBC r2, 5",
			"  LT_rA_rB_rC r0, r1, r2",
			"  RETURN"
		}); // CPP: }));

		// Test comparison (greater than - uses swapped LT)
		ok = ok && CheckCodeGen(parser, "5 > 3", new List<String> {
			"  LOAD_rA_iBC r1, 5",
			"  LOAD_rA_iBC r2, 3",
			"  LT_rA_rB_rC r0, r2, r1",  // swapped: r2 < r1
			"  RETURN"
		}); // CPP: }));

		// Test unary minus
		// r0 = result, r1 = 5, r2 = 0, SUB r0, r2, r1 (result = 0 - 5)
		ok = ok && CheckCodeGen(parser, "-5", new List<String> {
			"  LOAD_rA_iBC r1, 5",
			"  LOAD_rA_iBC r2, 0",
			"  SUB_rA_rB_rC r0, r2, r1",
			"  RETURN"
		}); // CPP: }));

		// Test grouping (parentheses) - should compile inner expression directly
		ok = ok && CheckCodeGen(parser, "(42)", new List<String> {
			"  LOAD_rA_iBC r0, 42",
			"  RETURN"
		}); // CPP: }));

		// Test list literal
		ok = ok && CheckCodeGen(parser, "[1, 2, 3]", new List<String> {
			"  LIST_rA_iBC r0, 3",
			"  LOAD_rA_iBC r1, 1",
			"  PUSH_rA_rB r0, r1, r0",
			"  LOAD_rA_iBC r1, 2",
			"  PUSH_rA_rB r0, r1, r0",
			"  LOAD_rA_iBC r1, 3",
			"  PUSH_rA_rB r0, r1, r0",
			"  RETURN"
		}); // CPP: }));

		// Test empty list
		ok = ok && CheckCodeGen(parser, "[]", new List<String> {
			"  LIST_rA_iBC r0, 0",
			"  RETURN"
		}); // CPP: }));

		// Test map literal
		ok = ok && CheckBytecodeGen(parser, "{}", 2, 0);  // MAP + RETURN

		// Test index access (resultReg r0 allocated first)
		ok = ok && CheckCodeGen(parser, "x[0]", new List<String> {
			"  GLOADC_rA_iBC r1, 0",   // x: free name, so a global reference
			"  LOAD_rA_iBC r2, 0",   // index 0
			"  IDXGET_rA_rB_rC r0, r1, r2",
			"  RETURN"
		}); // CPP: }));

		// Test nested expression (precedence)
		// 2 + 3 * 4: outer result r0, load 2 into r1, inner mult result r2, load 3,4 into r3,r4
		// LOAD r1,2; LOAD r3,3; LOAD r4,4; MUL r2,r3,r4; ADD r0,r1,r2; RETURN
		ok = ok && CheckBytecodeGen(parser, "2 + 3 * 4", 6, 0);

		// Test register reuse with nested expressions
		// (1 + 2) + (3 + 4): outer r0, first group r1 (with r2,r3 for operands),
		// second group r2 (reused after freeing r2,r3), with r3,r4 for operands
		// LOAD r2,1; LOAD r3,2; ADD r1,r2,r3; LOAD r3,3; LOAD r4,4; ADD r2,r3,r4; ADD r0,r1,r2; RETURN
		ok = ok && CheckBytecodeGen(parser, "(1 + 2) + (3 + 4)", 8, 0);

		return ok;
	}

	public static Boolean TestEmitPatternValidation() {
		//IOHelper.Print("  Testing emit pattern validation...");
		Boolean ok = true;

		// Test that GetEmitPattern correctly identifies patterns
		ok = ok && Assert(BytecodeUtil.GetEmitPattern(Opcode.RETURN) == EmitPattern.None,
			"RETURN should be EmitPattern.None");
		ok = ok && Assert(BytecodeUtil.GetEmitPattern(Opcode.NOOP) == EmitPattern.None,
			"NOOP should be EmitPattern.None");
		ok = ok && Assert(BytecodeUtil.GetEmitPattern(Opcode.LOCALS_rA) == EmitPattern.A,
			"LOCALS_rA should be EmitPattern.A");
		ok = ok && Assert(BytecodeUtil.GetEmitPattern(Opcode.ARG_rA) == EmitPattern.A,
			"ARG_rA should be EmitPattern.A");
		ok = ok && Assert(BytecodeUtil.GetEmitPattern(Opcode.LOAD_rA_iBC) == EmitPattern.AB,
			"LOAD_rA_iBC should be EmitPattern.AB");
		ok = ok && Assert(BytecodeUtil.GetEmitPattern(Opcode.LOAD_rA_rB) == EmitPattern.ABC,
			"LOAD_rA_rB should be EmitPattern.ABC");
		ok = ok && Assert(BytecodeUtil.GetEmitPattern(Opcode.IFLT_iAB_rC) == EmitPattern.BC,
			"IFLT_iAB_rC should be EmitPattern.BC");
		ok = ok && Assert(BytecodeUtil.GetEmitPattern(Opcode.ADD_rA_rB_rC) == EmitPattern.ABC,
			"ADD_rA_rB_rC should be EmitPattern.ABC");
		ok = ok && Assert(BytecodeUtil.GetEmitPattern(Opcode.LT_rA_rB_iC) == EmitPattern.ABC,
			"LT_rA_rB_iC should be EmitPattern.ABC");

		return ok;
	}

	public static Boolean TestLexer() {
		//IOHelper.Print("  Testing lexer...");
		Boolean ok = true;

		// Helper to check a single token
		Lexer lexer;
		Token tok;

		// Test simple number
		lexer = new Lexer("42");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.NUMBER, "Expected NUMBER token");
		ok = ok && AssertEqual(tok.Text, "42");
		ok = ok && AssertEqual(tok.DoubleValue, 42);

		// Test float
		lexer = new Lexer("3.14");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.NUMBER, "Expected NUMBER token for float");
		ok = ok && AssertEqual(tok.Text, "3.14");

		// Test string
		lexer = new Lexer("\"hello\"");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.STRING, "Expected STRING token");
		ok = ok && AssertEqual(tok.Text, "hello");

		// Test identifier
		lexer = new Lexer("myVar");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.IDENTIFIER, "Expected IDENTIFIER token");
		ok = ok && AssertEqual(tok.Text, "myVar");

		// Test operators
		lexer = new Lexer("+ - * / %");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.PLUS, "Expected PLUS");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.MINUS, "Expected MINUS");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.TIMES, "Expected TIMES");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.DIVIDE, "Expected DIVIDE");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.MOD, "Expected MOD");

		// Test comparison operators
		lexer = new Lexer("== != < > <= >=");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.EQUALS, "Expected EQUALS");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.NOT_EQUAL, "Expected NOT_EQUAL");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.LESS_THAN, "Expected LESS_THAN");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.GREATER_THAN, "Expected GREATER_THAN");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.LESS_EQUAL, "Expected LESS_EQUAL");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.GREATER_EQUAL, "Expected GREATER_EQUAL");

		// Test keywords
		lexer = new Lexer("and or not");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.AND, "Expected AND");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.OR, "Expected OR");
		ok = ok && Assert(lexer.NextToken().Type == TokenType.NOT, "Expected NOT");

		// Test comment at end of line
		lexer = new Lexer("42 // this is a comment");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.NUMBER, "Expected NUMBER before comment");
		ok = ok && AssertEqual(tok.Text, "42");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.COMMENT, "Expected COMMENT token");
		ok = ok && AssertEqual(tok.Text, "// this is a comment");

		// Test comment-only line
		lexer = new Lexer("// just a comment");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.COMMENT, "Expected COMMENT token for comment-only");
		ok = ok && AssertEqual(tok.Text, "// just a comment");

		// Test comment followed by newline and more code
		lexer = new Lexer("x // comment\ny");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.IDENTIFIER, "Expected IDENTIFIER x");
		ok = ok && AssertEqual(tok.Text, "x");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.COMMENT, "Expected COMMENT");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.EOL, "Expected EOL after comment");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.IDENTIFIER, "Expected IDENTIFIER y");
		ok = ok && AssertEqual(tok.Text, "y");

		// Test division vs comment
		lexer = new Lexer("10 / 2");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.NUMBER, "Expected NUMBER 10");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.DIVIDE, "Expected DIVIDE, not COMMENT");
		tok = lexer.NextToken();
		ok = ok && Assert(tok.Type == TokenType.NUMBER, "Expected NUMBER 2");

		return ok;
	}

	// CPP: static List<String> gTestOutput;

	// Helper: run a sequence of REPL inputs and collect all printed output.
	private static List<String> RunREPLSequence(List<String> inputs) {
		List<String> output = new List<String>();
		// CPP: gTestOutput = output;
		Interpreter interp = new Interpreter();
		interp.standardOutput = (String s, bool eol) => { output.Add(s); }; // CPP:
		// CPP: interp.set_standardOutput([](String s, Boolean) { gTestOutput.Add(s); });
		interp.implicitOutput = (String s, bool eol) => { output.Add(s); }; // CPP:
		// CPP: interp.set_implicitOutput([](String s, Boolean) { gTestOutput.Add(s); });
		interp.errorOutput = (String s, bool eol) => { output.Add(s); }; // CPP:
		// CPP: interp.set_errorOutput([](String s, Boolean) { gTestOutput.Add(s); });
		for (Int32 i = 0; i < inputs.Count; i++) {
			interp.REPL(inputs[i]);
		}
		return output;
	}

	public static Boolean TestParserNeedMoreInput() {
		Boolean ok = true;

		// Incomplete while block should need more input
		Parser parser = new Parser();
		parser.Init("while true");
		parser.ParseProgram();
		ok = ok && Assert(parser.NeedMoreInput(), "while without end should need more input");

		// Incomplete if block
		parser = new Parser();
		parser.Init("if true then");
		parser.ParseProgram();
		ok = ok && Assert(parser.NeedMoreInput(), "if-then without end should need more input");

		// Incomplete for block
		parser = new Parser();
		parser.Init("for i in range(10)");
		parser.ParseProgram();
		ok = ok && Assert(parser.NeedMoreInput(), "for without end should need more input");

		// Incomplete function block
		parser = new Parser();
		parser.Init("f = function(x)");
		parser.ParseProgram();
		ok = ok && Assert(parser.NeedMoreInput(), "function without end should need more input");

		// Complete statement should NOT need more input
		parser = new Parser();
		parser.Init("x = 42");
		parser.ParseProgram();
		ok = ok && Assert(!parser.NeedMoreInput(), "complete statement should not need more input");

		// Syntax error should NOT be treated as need-more-input
		parser = new Parser();
		parser.Init("if + then");
		parser.ParseProgram();
		ok = ok && Assert(!parser.NeedMoreInput(), "syntax error should not be need-more-input");

		if (!ok) IOHelper.Print("TestParserNeedMoreInput FAILED");
		return ok;
	}

	public static Boolean TestREPL() {
		Boolean ok = true;

		// Test 1: Simple global persistence
		{
			List<String> inputs = new List<String>();
			inputs.Add("x = 42");
			inputs.Add("print x");
			List<String> output = RunREPLSequence(inputs);
			ok = ok && Assert(output.Count >= 1 && output[0] == "42",
				StringUtils.Format("Global persistence: expected '42' but got {0}",
					output.Count > 0 ? output[0] : "(empty)"));
		}

		// Test 2: Global update (x = x + 1)
		{
			List<String> inputs = new List<String>();
			inputs.Add("x = 10");
			inputs.Add("x = x + 1");
			inputs.Add("print x");
			List<String> output = RunREPLSequence(inputs);
			ok = ok && Assert(output.Count >= 1 && output[0] == "11",
				StringUtils.Format("Global update: expected '11' but got {0}",
					output.Count > 0 ? output[0] : "(empty)"));
		}

		// Test 3: Function reading globals implicitly
		{
			List<String> inputs = new List<String>();
			inputs.Add("f = function; print x; end function");
			inputs.Add("x = 42; f");
			List<String> output = RunREPLSequence(inputs);
			ok = ok && Assert(output.Count >= 1 && output[0] == "42",
				StringUtils.Format("Function accessing globals (implicitly): expected '42' but got {0}",
					output.Count > 0 ? output[0] : "(empty)"));
		}

		// Test 3b: Function reading globals explicitly
		{
			List<String> inputs = new List<String>();
			inputs.Add("f = function; print globals.x; end function");
			inputs.Add("x = 42; f");
			List<String> output = RunREPLSequence(inputs);
			ok = ok && Assert(output.Count >= 1 && output[0] == "42",
				StringUtils.Format("Function accessing globals (explicitly): expected '42' but got {0}",
					output.Count > 0 ? output[0] : "(empty)"));
		}

		// Test 3b: Function updating globals, then read in main
		{
			List<String> inputs = new List<String>();
			inputs.Add("incX = function; globals.x += 1; end function");
			inputs.Add("x = 10; incX; print x");
			List<String> output = RunREPLSequence(inputs);
			ok = ok && Assert(output.Count >= 1 && output[0] == "11",
				StringUtils.Format("Function updating global: expected '11' but got {0}",
					output.Count > 0 ? output[0] : "(empty)"));
		}

		// Test 4: Implicit output for bare expression
		{
			List<String> inputs = new List<String>();
			inputs.Add("2 + 3");
			List<String> output = RunREPLSequence(inputs);
			ok = ok && Assert(output.Count >= 1 && output[0] == "5",
				StringUtils.Format("Implicit output: expected '5' but got {0}",
					output.Count > 0 ? output[0] : "(empty)"));
		}

		// Test 5: No implicit output for assignment
		{
			List<String> inputs = new List<String>();
			inputs.Add("x = 42");
			List<String> output = RunREPLSequence(inputs);
			ok = ok && Assert(output.Count == 0,
				StringUtils.Format("Assignment should not produce output, got {0} items",
					output.Count));
		}

		// Test 6: Multi-line block via NeedMoreInput
		{
			Interpreter interp = new Interpreter();
			List<String> output = new List<String>();
			// CPP: gTestOutput = output;
			interp.standardOutput = (String s, bool eol) => { output.Add(s); }; // CPP:
			// CPP: interp.set_standardOutput([](String s, Boolean) { gTestOutput.Add(s); });
			interp.REPL("if true then");
			ok = ok && Assert(interp.NeedMoreInput(), "After 'if true then', should need more input");
			interp.REPL("print 99");
			ok = ok && Assert(interp.NeedMoreInput(), "After body line, should still need more input");
			interp.REPL("end if");
			ok = ok && Assert(!interp.NeedMoreInput(), "After 'end if', should not need more input");
			ok = ok && Assert(output.Count >= 1 && output[0] == "99",
				StringUtils.Format("Multi-line if: expected '99' but got {0}",
					output.Count > 0 ? output[0] : "(empty)"));
		}

		// Test 7: Function call with argument across REPL entries
		{
			List<String> inputs = new List<String>();
			inputs.Add("f = function(s)");
			inputs.Add("return s * 4");
			inputs.Add("end function");
			inputs.Add("print f(\"spam\")");
			List<String> output = RunREPLSequence(inputs);
			ok = ok && Assert(output.Count >= 1 && output[0] == "spamspamspamspam",
				StringUtils.Format("Function call with arg: expected 'spamspamspamspam' but got {0}",
					output.Count > 0 ? output[0] : "(empty)"));
		}

		// Test 8: Function call as expression (implicit output) — not via print
		{
			List<String> inputs = new List<String>();
			inputs.Add("f = function(s)");
			inputs.Add("return s * 4");
			inputs.Add("end function");
			inputs.Add("f(\"spam\")");
			List<String> output = RunREPLSequence(inputs);
			ok = ok && Assert(output.Count >= 1 && output[0] == "spamspamspamspam",
				StringUtils.Format("Function call implicit output: expected 'spamspamspamspam' but got {0}",
					output.Count > 0 ? output[0] : "(empty)"));
		}

		// Test 9: Function call result used in expression
		{
			List<String> inputs = new List<String>();
			inputs.Add("f = function(s)");
			inputs.Add("return s * 4");
			inputs.Add("end function");
			inputs.Add("print f(\"spam \") + \"and spam!\"");
			List<String> output = RunREPLSequence(inputs);
			ok = ok && Assert(output.Count >= 1 && output[0] == "spam spam spam spam and spam!",
				StringUtils.Format("Function call in expression: expected 'spam spam spam spam and spam!' but got {0}",
					output.Count > 0 ? output[0] : "(empty)"));
		}

		// Test 10: Multiple functions
		{
			List<String> inputs = new List<String>();
			inputs.Add("f = function; return 101; end function");
			inputs.Add("g = function; return 202; end function");
			inputs.Add("print f");
			inputs.Add("print g");
			inputs.Add("print f(\"spam \") + \" and spam!\"");			
			List<String> output = RunREPLSequence(inputs);
			ok = ok && Assert(output.Count >= 2 && output[0] == "101" && output[1] == "202",
				StringUtils.Format("Multi-function test: expected 101 and 102 but got {0} and {1}",
					output[0], output[1]));
		}

		if (!ok) IOHelper.Print("TestREPL FAILED");
		return ok;
	}

	// ── Chaining to a new program, preserving globals ────────────────────────────

	public static Boolean TestResetPreservingGlobals() {
		Boolean ok = true;

		List<String> output = new List<String>();
		// CPP: gTestOutput = output;
		Interpreter interp;
		interp = new Interpreter("a = 42\ns = \"kept\"\nf = function; return a * 2; end function");
		interp.standardOutput = (String s, bool eol) => { output.Add(s); }; // CPP:
		// CPP: interp.set_standardOutput([](String s, Boolean) { gTestOutput.Add(s); });
		interp.errorOutput = (String s, bool eol) => { output.Add(s); }; // CPP:
		// CPP: interp.set_errorOutput([](String s, Boolean) { gTestOutput.Add(s); });
		interp.RunUntilDone(10, false);
		ok = ok && Assert(output.Count == 0,
			StringUtils.Format("first program should print nothing, got {0} lines", output.Count));

		// Chain to a second program.  It reads a global it never assigns (s),
		// reads then reassigns another (a), and calls a function the first
		// program defined -- which must still see the globals through its
		// closure, and must still find the `print` intrinsic on the new VM.
		interp.ResetPreservingGlobals("print s\nprint a\nprint f\na = 7\nprint a");
		// Collect before running: the preserved globals are reachable only as
		// the new VM's persistent globals map, so this fails if that map (and
		// the gathered entries in it) is not treated as a GC root.
		GCManager.CollectGarbage();
		interp.RunUntilDone(10, false);

		ok = ok && Assert(output.Count == 4,
			StringUtils.Format("chained program should print 4 lines, got {0}", output.Count));
		if (output.Count == 4) {
			ok = ok && Assert(output[0] == "kept",
				StringUtils.Format("global untouched by new program: expected 'kept', got '{0}'", output[0]));
			ok = ok && Assert(output[1] == "42",
				StringUtils.Format("global read before reassignment: expected '42', got '{0}'", output[1]));
			ok = ok && Assert(output[2] == "84",
				StringUtils.Format("function from old program: expected '84', got '{0}'", output[2]));
			ok = ok && Assert(output[3] == "7",
				StringUtils.Format("global reassigned by new program: expected '7', got '{0}'", output[3]));
		}

		// A plain Reset must still start clean.
		interp.Reset("if globals.hasIndex(\"a\") then print \"leaked\" else print \"clean\"");
		interp.RunUntilDone(10, false);
		ok = ok && Assert(output.Count == 5 && output[4] == "clean",
			StringUtils.Format("plain Reset should not preserve globals, got '{0}'",
				output.Count > 4 ? output[4] : "(no output)"));

		if (!ok) IOHelper.Print("TestResetPreservingGlobals FAILED");
		return ok;
	}

	// ── The host global API ──────────────────────────────────────────────────────

	// Get/SetGlobalValue used to disagree: Set wrote the REPL globals map and so
	// did nothing at all outside REPL mode, while Get read the VM's globals and
	// answered in both.  They now reach the same slot, which this pins down --
	// along with the two timing cases that used to need the REPL("") bootstrap:
	// seeding before any compile, and reading after the program has ended.
	public static Boolean TestHostGlobals() {
		Boolean ok = true;

		List<String> output = new List<String>();
		// CPP: gTestOutput = output;
		Interpreter interp;
		interp = new Interpreter("print seeded\nresult = seeded * 2\nmine = \"here\"");
		interp.standardOutput = (String s, bool eol) => { output.Add(s); }; // CPP:
		// CPP: interp.set_standardOutput([](String s, Boolean) { gTestOutput.Add(s); });
		interp.errorOutput = (String s, bool eol) => { output.Add(s); }; // CPP:
		// CPP: interp.set_errorOutput([](String s, Boolean) { gTestOutput.Add(s); });

		// Seed before anything is compiled -- there is no VM at this point.
		interp.SetGlobalValue("seeded", new Value(21));
		ok = ok && Assert(interp.GetGlobalValue("seeded") == new Value(21),
			"host Set then Get, before the first compile");

		interp.RunUntilDone(10, false);
		ok = ok && Assert(output.Count == 1 && output[0] == "21",
			StringUtils.Format("program should see the seeded global, got '{0}'",
				output.Count > 0 ? output[0] : "(no output)"));

		// Read after the program has ended.  Both a global the program computed
		// from the seed and one it invented from nothing must still be there;
		// @main's registers are long gone, and that used to take half of them
		// with it.
		ok = ok && Assert(interp.GetGlobalValue("result") == new Value(42),
			"global computed by the program, read after it ended");
		ok = ok && Assert(interp.GetGlobalValue("mine") == Value.make_string("here"),
			"global created by the program, read after it ended");

		// An unset name is null, not an error and not the Unassigned sentinel.
		ok = ok && Assert(interp.GetGlobalValue("neverSet").IsNull(),
			"unset global should read as null");

		// Reset drops the namespace.
		interp.Reset("");
		ok = ok && Assert(interp.GetGlobalValue("mine").IsNull(),
			"Reset should discard globals");

		if (!ok) IOHelper.Print("TestHostGlobals FAILED");
		return ok;
	}

	// ── Running one program against two global namespaces ────────────────────────

	// Compiled code caches its (name -> slot) resolutions in the FuncDef, guarded
	// by the namespace's Id (notes/GLOBALS.md section 4.3).  The guard exists for
	// hosting: a function compiled alongside one Globals may be run against
	// another, and must resolve in whichever one the VM is pointed at.
	//
	// Nothing else in this tree ever points a VM at a second namespace -- VM.SetGlobals
	// has no other caller -- so without this test the re-resolution branch never
	// executes at all, and a stale cache would go unnoticed until an embedding host
	// hit it.
	//
	// The two namespaces are deliberately built so that every shared name lands on
	// a *different* slot in each.  That is what makes this a test: if the cache
	// were not invalidated, the second run would index the second table with the
	// first table's slot numbers and read some other variable's value, rather than
	// happening to come out right.
	public static Boolean TestGlobalsSwitch() {
		Boolean ok = true;

		List<String> output = new List<String>();
		// CPP: gTestOutput = output;
		Interpreter interp;
		// Reads three globals it never assigns; gamma is read from inside a
		// function, so it exercises an inner FuncDef's reference table too (each
		// FuncDef carries its own cache and its own guard).
		interp = new Interpreter("print alpha\nprint beta\nshow = function; return gamma; end function\nprint show");
		interp.standardOutput = (String s, bool eol) => { output.Add(s); }; // CPP:
		// CPP: interp.set_standardOutput([](String s, Boolean) { gTestOutput.Add(s); });
		interp.errorOutput = (String s, bool eol) => { output.Add(s); }; // CPP:
		// CPP: interp.set_errorOutput([](String s, Boolean) { gTestOutput.Add(s); });

		// Namespace A: the interpreter's own.  Seeding order fixes the slots.
		interp.SetGlobalValue("alpha", Value.make_string("A-alpha"));
		interp.SetGlobalValue("beta",  Value.make_string("A-beta"));
		interp.SetGlobalValue("gamma", Value.make_string("A-gamma"));
		Globals gA = interp.GetGlobals();

		// Namespace B: same three names, seeded in a rotated order so no name
		// keeps its slot number.  Globals.Create roots the map, so B survives
		// collection while we hold it.
		Globals gB = Globals.Create();
		gB.SetSlot(gB.Resolve(Value.make_string("gamma")), Value.make_string("B-gamma"));
		gB.SetSlot(gB.Resolve(Value.make_string("alpha")), Value.make_string("B-alpha"));
		gB.SetSlot(gB.Resolve(Value.make_string("beta")),  Value.make_string("B-beta"));

		ok = ok && Assert(gA.Id() != gB.Id(), "two namespaces must have distinct Ids");
		// If these ever coincide the test still passes, but stops proving anything.
		ok = ok && Assert(gA.Find(Value.make_string("alpha")) != gB.Find(Value.make_string("alpha"))
		              && gA.Find(Value.make_string("beta"))  != gB.Find(Value.make_string("beta"))
		              && gA.Find(Value.make_string("gamma")) != gB.Find(Value.make_string("gamma")),
			"test setup: every shared name must sit at a different slot in A and B");

		// Run 1: namespace A.
		interp.RunUntilDone(10, false);
		ok = ok && CheckGlobalsSwitchRun(output, 0, "A", "first run, in namespace A");

		// Run 2: point the VM at B and run the very same FuncDefs.  Their caches
		// now name slots in A, and every one of them is wrong for B.
		VM theVM = interp.vm;
		theVM.SetGlobals(gB);
		interp.Restart();
		interp.RunUntilDone(10, false);
		ok = ok && CheckGlobalsSwitchRun(output, 3, "B", "second run, after switching to namespace B");

		// Run 3: back to A.  The caches now name B's slots, so this catches a
		// guard that only invalidates once.
		theVM.SetGlobals(gA);
		interp.Restart();
		interp.RunUntilDone(10, false);
		ok = ok && CheckGlobalsSwitchRun(output, 6, "A", "third run, after switching back to namespace A");

		// Neither namespace should have been disturbed by running against the other.
		ok = ok && Assert(gB.ValueAtSlot(gB.Find(Value.make_string("alpha"))) == Value.make_string("B-alpha"),
			"namespace B unchanged after running in A again");
		ok = ok && Assert(interp.GetGlobalValue("alpha") == Value.make_string("A-alpha"),
			"namespace A unchanged after running in B");

		// Point the VM back at something the interpreter owns before dropping our
		// root on B, so nothing is left holding a released namespace.
		gB.Release();

		if (!ok) IOHelper.Print("TestGlobalsSwitch FAILED");
		return ok;
	}

	// Check the three lines one run of the TestGlobalsSwitch program should emit,
	// starting at output[first].  `tag` is "A" or "B" -- the namespace whose values
	// we expect to see.
	private static Boolean CheckGlobalsSwitchRun(List<String> output, Int32 first, String tag, String what) {
		if (output.Count < first + 3) {
			return Assert(false, StringUtils.Format("{0}: expected 3 more lines, have {1} in total",
				what, output.Count));
		}
		Boolean ok = true;
		ok = ok && Assert(output[first] == tag + "-alpha",
			StringUtils.Format("{0}: expected '{1}-alpha', got '{2}'", what, tag, output[first]));
		ok = ok && Assert(output[first + 1] == tag + "-beta",
			StringUtils.Format("{0}: expected '{1}-beta', got '{2}'", what, tag, output[first + 1]));
		// Read from inside a function: the stage-5 path, with its own FuncDef cache.
		ok = ok && Assert(output[first + 2] == tag + "-gamma",
			StringUtils.Format("{0}: expected '{1}-gamma' (read inside a function), got '{2}'",
				what, tag, output[first + 2]));
		return ok;
	}

	// ── GCHandle test ────────────────────────────────────────────────────────────

	private static Int32 _handleFinalizerCallCount = 0;
	private static void TestHandleFinalizer(object userData) {
		_handleFinalizerCallCount++;
	}

	public static Boolean TestGCHandle() {
		Boolean ok = true;
		_handleFinalizerCallCount = 0;

		// Allocate a handle and verify the predicate.
		Value h = GCManager.NewHandle(null, TestHandleFinalizer);
		ok = ok && Assert(h.IsHandle(), "NewHandle should produce a handle value");
		ok = ok && Assert(!h.IsMap(), "handle should not test as map");
		ok = ok && Assert(!h.IsNull(), "handle should not test as null");

		// Keep the handle alive across a GC cycle via retain count; callback must not fire yet.
		GCManager.Handles.Retain(h.ItemIndex());
		GCManager.CollectGarbage();
		ok = ok && Assert(_handleFinalizerCallCount == 0,
			"callback should not fire while handle is still reachable");

		// Release the retain — handle is now unreachable.  Next GC must sweep it.
		GCManager.Handles.Release(h.ItemIndex());
		GCManager.CollectGarbage();
		ok = ok && Assert(_handleFinalizerCallCount == 1,
			"callback should fire exactly once when handle is collected");

		if (!ok) IOHelper.Print("TestGCHandle FAILED");
		return ok;
	}

	// Helper for MayReadVar tests: parse an assignment, then ask its RHS.
	private static Boolean CheckMayReadVar(Parser parser, String input, String varName, Boolean expected) {
		ASTNode ast = parser.Parse(input);
		if (parser.HadError()) {
			IOHelper.Print($"Parse error for input: {input}");
			return false;
		}
		AssignmentNode assign = ast.Simplify() as AssignmentNode;
		if (assign == null) {
			IOHelper.Print($"MayReadVar test input is not an assignment: {input}");
			return false;
		}
		Boolean result = assign.Value.MayReadVar(varName);
		if (result != expected) {
			IOHelper.Print($"MayReadVar('{varName}') on RHS of `{input}`: expected {expected}, got {result}");
			return false;
		}
		return true;
	}

	public static Boolean TestMayReadVar() {
		Parser parser = new Parser();
		Boolean ok = true;

		// Structural recursion should find the name wherever it appears.
		ok = ok && CheckMayReadVar(parser, "x = x", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = @x", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = [x]", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = [[1, x]]", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = {x: 1}", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = {1: x}", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = 1 + x * 2", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = -x", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = f(x)", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = x(1)", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = x.foo", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = x[1]", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = a[1:x]", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = 0 <= x <= 9", "x", true);

		// ...and answer false when it genuinely does not appear.
		ok = ok && CheckMayReadVar(parser, "x = 42", "x", false);
		ok = ok && CheckMayReadVar(parser, "x = \"s\"", "x", false);
		ok = ok && CheckMayReadVar(parser, "x = []", "x", false);
		ok = ok && CheckMayReadVar(parser, "x = [y, z]", "x", false);
		ok = ok && CheckMayReadVar(parser, "x = f(y)", "x", false);
		// A member name is not a variable read: y.x reads y, not x.
		ok = ok && CheckMayReadVar(parser, "x = y.x", "x", false);

		// ScopeNode resolves by name at runtime, reaching registers the AST never
		// names, so it must answer true for any variable.  This is what keeps
		// "x = [globals.x]" from building a list containing itself.
		ok = ok && CheckMayReadVar(parser, "x = [globals.x]", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = [locals.x]", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = globals[\"x\"]", "x", true);
		ok = ok && CheckMayReadVar(parser, "x = [outer.somethingElse]", "x", true);

		if (!ok) IOHelper.Print("TestMayReadVar FAILED");
		return ok;
	}

	//
	// Intrinsic parameter defaults must survive a full collection that runs
	// before any VM exists.  They are created when an intrinsic is *defined*,
	// and only become reachable through a funcref much later; nothing but
	// Intrinsic.MarkRoots keeps them alive in between.  The defaults are short
	// strings, hence interned, so only a FULL pass can sweep them -- which is
	// why this went unnoticed for so long.
	//
	public static Boolean TestIntrinsicDefaults() {
		List<String> before = CollectIntrinsicStringDefaults();
		GCManager.FullCollectGarbage();
		List<String> after = CollectIntrinsicStringDefaults();

		Boolean ok = Assert(before.Count > 0, "expected some string-valued defaults")
			&& AssertEqual(after.Count, before.Count);
		if (!ok) return false;
		for (Int32 i = 0; i < before.Count; i++) {
			if (!AssertEqual(after[i], before[i])) return false;
		}

		// The short-name registry holds Values the same way (cs/Intrinsic.cs).
		Value listType = CoreIntrinsics.ListType();
		GCManager.FullCollectGarbage();
		return AssertEqual(Intrinsic.GetShortName(listType), "list");
	}

	// Every string-valued parameter default of every intrinsic, in a fixed
	// order.  BuildFuncDef makes a throwaway FuncDef -- it is not a GC object
	// and roots nothing -- so reading the defaults this way does not itself
	// keep them alive.
	private static List<String> CollectIntrinsicStringDefaults() {
		List<String> result = new List<String>();
		Int32 count = Intrinsic.Count();
		for (Int32 i = 0; i < count; i++) {
			FuncDef def = Intrinsic.GetByIndex(i).BuildFuncDef();
			for (Int32 j = 0; j < def.ParamDefaults.Count; j++) {
				Value d = def.ParamDefaults[j];
				if (d.IsString()) result.Add(d.AsCString());
			}
		}
		return result;
	}

	//
	// The global slot table (cs/Globals.cs), exercised on its own -- no VM, no
	// compiled code.  See notes/GLOBALS.md; this is stage 1 of that plan, so
	// nothing here goes through the VM yet.
	//
	public static Boolean TestGlobals() {
		Globals g = Globals.Create();
		Value map = g.AsMap();

		Value nameX = Value.make_string("x");
		Value nameY = Value.make_string("y");

		// ── Create ────────────────────────────────────────────────────────────
		Boolean createOk = Assert(map.IsMap(), "globals view should be a map")
			&& AssertEqual(g.Count(), 0)
			&& AssertEqual(g.SlotCount(), 0)
			&& AssertEqual(map.MapCount(), 0)
			&& Assert(g.Id() > 0, "Globals should have a positive Id");
		if (!createOk) return false;

		// ── Resolve creates a slot but not a global ───────────────────────────
		// A resolved-but-unset name must NOT read as a global; that is what lets
		// compiled code resolve every name a function mentions up front.
		Int32 slotX = g.Resolve(nameX);
		Boolean resolveOk = AssertEqual(slotX, 0)
			&& AssertEqual(g.SlotCount(), 1)
			&& AssertEqual(g.Count(), 0)
			&& Assert(!g.SlotIsAssigned(slotX), "a resolved slot starts unassigned")
			&& Assert(!map.HasKey(nameX), "resolving must not create a global")
			&& Assert(g.ValueAtSlot(slotX).IsUnassigned(), "empty slot holds the sentinel")
			&& Assert(g.NameAtSlot(slotX) == nameX, "slot remembers its name")
			&& AssertEqual(g.Resolve(nameX), 0)
			&& Assert(g.Find(nameY) == -1, "Find must not create a slot");
		if (!resolveOk) return false;

		// ── Assign, through both doors ────────────────────────────────────────
		// Writing a slot directly and writing through the map must be the same
		// operation -- that identity is the whole point of the design.
		g.SetSlot(slotX, new Value(42.0));
		map.MapSet(nameY, Value.make_string("hello"));
		Int32 slotY = g.Find(nameY);
		Value readX;
		Boolean assignOk = AssertEqual(g.Count(), 2)
			&& AssertEqual(map.MapCount(), 2)
			&& AssertEqual(slotY, 1)
			&& AssertEqual(map.MapGet(nameX).DoubleValue(), 42.0)
			&& Assert(g.TryGet(nameY, out readX), "TryGet should find y")
			&& AssertEqual(readX.ToString(null), "hello")
			&& AssertEqual(g.ValueAtSlot(slotY).ToString(null), "hello");
		if (!assignOk) return false;

		// ── null is a value; Unassigned is not ────────────────────────────────
		// Storing null must leave the global bound, and must not read back as
		// the sentinel.  Conflating these is the bug the sentinel exists to
		// prevent.
		map.MapSet(nameX, Value.Null);
		Value nullRead;
		Boolean nullOk = Assert(map.HasKey(nameX), "a global set to null is still bound")
			&& AssertEqual(g.Count(), 2)
			&& Assert(g.SlotIsAssigned(slotX), "null-valued slot is assigned")
			&& Assert(!g.ValueAtSlot(slotX).IsUnassigned(), "null is not Unassigned")
			&& Assert(g.TryGet(nameX, out nullRead), "TryGet finds a null-valued global")
			&& Assert(nullRead.IsNull(), "and reads back as null");
		if (!nullOk) return false;

		// The sentinel must not equal anything user code can make, including
		// itself-by-content: it is a funcref compared by identity.
		Boolean sentinelOk = Assert(!Value.Null.IsUnassigned(), "null is not the sentinel")
			&& Assert(!Value.make_string("x").IsUnassigned(), "a string is not the sentinel")
			&& Assert(Value.Unassigned.IsUnassigned(), "the sentinel is itself")
			&& Assert(!Value.Unassigned.IsNull(), "the sentinel is not null");
		if (!sentinelOk) return false;

		// ── Remove, and re-add into the SAME slot ─────────────────────────────
		// Slot stability is what lets compiled code cache a slot index forever.
		Boolean removeOk = Assert(map.MapRemove(nameX), "removing a bound global reports true")
			&& AssertEqual(g.Count(), 1)
			&& AssertEqual(g.SlotCount(), 2)
			&& Assert(!map.HasKey(nameX), "removed global is gone")
			&& Assert(g.ValueAtSlot(slotX).IsUnassigned(), "its slot is unassigned again")
			&& Assert(!g.Remove(nameX), "removing it twice reports false")
			&& Assert(g.Find(nameX) == slotX, "but the slot is still reserved for the name");
		if (!removeOk) return false;

		map.MapSet(nameX, new Value(7.0));
		Boolean readdOk = AssertEqual(g.Find(nameX), slotX)
			&& AssertEqual(g.SlotCount(), 2)
			&& AssertEqual(g.Count(), 2)
			&& AssertEqual(map.MapGet(nameX).DoubleValue(), 7.0);
		if (!readdOk) return false;

		// ── Iteration skips unassigned slots ──────────────────────────────────
		map.MapSet(Value.make_string("z"), new Value(3.0));
		map.MapRemove(nameY);            // leaves a hole in the middle
		Int32 seen = 0;
		Boolean sawX = false;
		Boolean sawY = false;
		Boolean sawZ = false;
		MapIterator it = map.Iterator();
		Value iterKey, iterVal;
		while (Value.map_iterator_next(ref it)) { // CPP: while (map_iterator_next(&it, &iterKey, &iterVal)) {
			iterKey = it.Key; iterVal = it.Val; // CPP: 
			seen++;			
			if (iterKey == nameX) sawX = true;
			if (iterKey == nameY) sawY = true;
			if (iterKey == Value.make_string("z")) sawZ = true;
			if (iterVal.IsUnassigned()) { // CPP: if (iterVal.IsUnassigned()) {
				Assert(false, "iteration must never yield the sentinel");
				return false;
			}
		}
		Boolean iterOk = AssertEqual(seen, 2)
			&& Assert(sawX, "iteration should see x")
			&& Assert(!sawY, "iteration should skip the removed y")
			&& Assert(sawZ, "iteration should see z");
		if (!iterOk) return false;

		// ── Non-string keys work, as they do on any map ───────────────────────
		Value numKey = new Value(42.0);
		map.MapSet(numKey, Value.make_string("answer"));
		Boolean numKeyOk = Assert(map.HasKey(numKey), "globals should accept a non-string key")
			&& AssertEqual(map.MapGet(numKey).ToString(null), "answer");
		if (!numKeyOk) return false;

		// ── A name built at run time must match the same name as a literal ────
		// The name->slot index is a hash, so this is the property it depends on:
		// `globals["dyn" + i]` has to find the same slot as a compiled reference
		// to `dyn0`.  It was broken in the C++ port for keys of 5 bytes or less
		// (equal strings hashed differently depending on representation); the
		// integration suite covers it more thoroughly under SECTION: STRING
		// REPRESENTATION AND MAP KEYS.
		Value builtName = Value.make_string("d").Add(Value.make_string("yn0"), null);
		map.MapSet(Value.make_string("dyn0"), new Value(11.0));
		Boolean keyReprOk = Assert(map.HasKey(builtName),
			"a computed key must find the same global as the literal key");
		if (!keyReprOk) return false;

		// ── Clear keeps the slots (and therefore cached slot indices) ─────────
		Int32 slotsBeforeClear = g.SlotCount();
		map.Clear();
		Boolean clearOk = AssertEqual(g.Count(), 0)
			&& AssertEqual(map.MapCount(), 0)
			&& AssertEqual(g.SlotCount(), slotsBeforeClear)
			&& AssertEqual(g.Find(nameX), slotX)
			&& Assert(!map.HasKey(nameX), "cleared global is unbound")
			&& AssertEqual(map.Iterator().Iter, -1) // CPP: 
			;
		if (!clearOk) return false;

		// ── Survives collection ───────────────────────────────────────────────
		// The map is rooted by Globals.Create; the names and values hanging off
		// the slot table are reachable only through GCMap.MarkChildren -> here.
		// This uses a FULL collection on purpose: global names are short strings
		// and therefore interned, and only a full pass sweeps the interned set.
		Value survivor = Value.make_string("a string long enough to be heap-allocated, not tiny");
		map.MapSet(nameX, survivor);
		GCManager.FullCollectGarbage();
		Boolean gcOk = Assert(map.HasKey(nameX), "global should survive collection")
			&& AssertEqual(map.MapGet(nameX).ToString(null),
				"a string long enough to be heap-allocated, not tiny")
			&& AssertEqual(g.Count(), 1);
		if (!gcOk) return false;

		// ── Distinct namespaces are independent, with distinct Ids ────────────
		Globals g2 = Globals.Create();
		g2.AsMap().MapSet(nameX, new Value(99.0));
		Boolean twoOk = Assert(g2.Id() != g.Id(), "each Globals gets its own Id")
			&& AssertEqual(g2.AsMap().MapGet(nameX).DoubleValue(), 99.0)
			&& AssertEqual(map.MapGet(nameX).ToString(null),
				"a string long enough to be heap-allocated, not tiny");
		if (!twoOk) return false;

		// ── Freeze ────────────────────────────────────────────────────────────
		// Done last, on its own table: Freeze is one-way, and the write that a
		// frozen map rejects reports through VM.ActiveVM(), which is null here.
		// Enforcement lives in Value.MapSet, above the backing, so there is
		// nothing globals-specific to exercise beyond the flag round-tripping.
		Globals g3 = Globals.Create();
		g3.AsMap().MapSet(nameX, new Value(1.0));
		Boolean freezeOk = Assert(!g3.AsMap().IsFrozen(), "a new globals map is not frozen");
		g3.AsMap().Freeze();
		freezeOk = freezeOk && Assert(g3.AsMap().IsFrozen(), "globals map should report frozen");
		if (!freezeOk) return false;

		g.Release();
		g2.Release();
		g3.Release();
		return true;
	}

	public static Boolean RunAll() {
		return TestIntrinsicDefaults()   // first: wants to run before any VM builds the funcrefs
			&& TestStringUtils()
			&& TestDisassembler()
			&& TestAssembler()
			&& TestValueMap()
			&& TestGlobals()
			&& TestLexer()
			&& TestParser()
			&& TestMayReadVar()
			&& TestCodeGenerator()
			&& TestEmitPatternValidation()
			&& TestParserNeedMoreInput()
			&& TestREPL()
			&& TestResetPreservingGlobals()
		&& TestHostGlobals()
		&& TestGlobalsSwitch()
			&& TestGCHandle();
	}
}

}
