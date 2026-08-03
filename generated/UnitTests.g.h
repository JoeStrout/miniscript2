// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: UnitTests.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// This module gathers together all the unit tests for this prototype.
// Each test returns true on success, false on failure.

namespace MiniScript {

// DECLARATIONS

class UnitTests {

	public: static Boolean Assert(bool condition, String message);
	
	public: static Boolean AssertEqual(String actual, String expected);
		
	public: static Boolean AssertEqual(Double actual, Double expected);

	// Compare two UInt32 values, reporting any mismatch in hex (useful for
	// bytecode instructions).  Named distinctly from AssertEqual to avoid an
	// ambiguous overload with the Double version when given integer literals.
	public: static Boolean AssertEqualU(UInt32 actual, UInt32 expected);

	public: static Boolean AssertEqual(List<String> actual, List<String> expected);
		
	public: static Boolean TestStringUtils();
	
	public: static Boolean TestDisassembler();
	
	public: static Boolean TestAssembler();

	public: static Boolean TestValueMap();

	// Helper for parser tests: parse, simplify, and check result
	private: static Boolean CheckParse(Parser parser, String input, String expected);

	public: static Boolean TestParser();

	// Helper for code generator tests: parse, generate, and check assembly output
	private: static Boolean CheckCodeGen(Parser parser, String input, List<String> expectedLines);

	// Helper to check bytecode generation produces valid FuncDef
	private: static Boolean CheckBytecodeGen(Parser parser, String input, Int32 expectedInstructions, Int32 expectedConstants);

	public: static Boolean TestCodeGenerator();

	public: static Boolean TestEmitPatternValidation();

	public: static Boolean TestLexer();

	// Helper: run a sequence of REPL inputs and collect all printed output.
	private: static List<String> RunREPLSequence(List<String> inputs);

	public: static Boolean TestParserNeedMoreInput();

	public: static Boolean TestREPL();

	// ── Chaining to a new program, preserving globals ────────────────────────────

	public: static Boolean TestResetPreservingGlobals();

	// ── The host global API ──────────────────────────────────────────────────────

	// Get/SetGlobalValue used to disagree: Set wrote the REPL globals map and so
	// did nothing at all outside REPL mode, while Get read the VM's globals and
	// answered in both.  They now reach the same slot, which this pins down --
	// along with the two timing cases that used to need the REPL("") bootstrap:
	// seeding before any compile, and reading after the program has ended.
	public: static Boolean TestHostGlobals();

	// ── Running one program against two global namespaces ────────────────────────

	// Compiled code caches its (name -> slot) resolutions in the FuncDef, guarded
	// by the namespace's Id (notes/GLOBALS.md section 4.3).  The guard exists for
	// hosting: a function compiled alongside one Globals may be run against
	// another, and must resolve in whichever one the VM is pointed at.
	// Nothing else in this tree ever points a VM at a second namespace -- VM.SetGlobals
	// has no other caller -- so without this test the re-resolution branch never
	// executes at all, and a stale cache would go unnoticed until an embedding host
	// hit it.
	// The two namespaces are deliberately built so that every shared name lands on
	// a *different* slot in each.  That is what makes this a test: if the cache
	// were not invalidated, the second run would index the second table with the
	// first table's slot numbers and read some other variable's value, rather than
	// happening to come out right.
	public: static Boolean TestGlobalsSwitch();

	// Check the three lines one run of the TestGlobalsSwitch program should emit,
	// starting at output[first].  `tag` is "A" or "B" -- the namespace whose values
	// we expect to see.
	private: static Boolean CheckGlobalsSwitchRun(List<String> output, Int32 first, String tag, String what);
	private: static Int32 _handleFinalizerCallCount;

	// ── GCHandle test ────────────────────────────────────────────────────────────

	private: static void TestHandleFinalizer(object userData);

	public: static Boolean TestGCHandle();

	// Helper for MayReadVar tests: parse an assignment, then ask its RHS.
	private: static Boolean CheckMayReadVar(Parser parser, String input, String varName, Boolean expected);

	public: static Boolean TestMayReadVar();

	// Intrinsic parameter defaults must survive a full collection that runs
	// before any VM exists.  They are created when an intrinsic is *defined*,
	// and only become reachable through a funcref much later; nothing but
	// Intrinsic.MarkRoots keeps them alive in between.  The defaults are short
	// strings, hence interned, so only a FULL pass can sweep them -- which is
	// why this went unnoticed for so long.
	public: static Boolean TestIntrinsicDefaults();

	// Every string-valued parameter default of every intrinsic, in a fixed
	// order.  BuildFuncDef makes a throwaway FuncDef -- it is not a GC object
	// and roots nothing -- so reading the defaults this way does not itself
	// keep them alive.
	private: static List<String> CollectIntrinsicStringDefaults();

	// The global slot table (cs/Globals.cs), exercised on its own -- no VM, no
	// compiled code.  See notes/GLOBALS.md; this is stage 1 of that plan, so
	// nothing here goes through the VM yet.
	public: static Boolean TestGlobals();

	public: static Boolean RunAll();
}; // end of struct UnitTests

// INLINE METHODS

} // end of namespace MiniScript
