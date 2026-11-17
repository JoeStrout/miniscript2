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

	// ── Per-program type maps ────────────────────────────────────────────────────

	// Methods a script adds to `list` or `string` belong to that program: another
	// interpreter must not see them, a chained program (same namespace) must, and
	// a plain Reset (new namespace) must start from the pristine templates.
	public: static Boolean TestTypeMapIsolation();

	// Builder for TestPerProgram: a fresh map each call.
	private: static Value BuildPerProgramTestMap();

	// A host-defined PerProgram: one value per program, the same value on every
	// request, kept alive by the program's namespace, and rebuilt after `reset`
	// (Interpreter.ClearGlobals).
	public: static Boolean TestPerProgram();

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

	// ── GC slot trimming and allocation trigger ──────────────────────────────────

	public: static Boolean TestGCSlotTrim();

	public: static Boolean TestGCAllocTrigger();

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

	// ── Calling a function value from native code (RunFunction) ──────────────────

	// A host that accepts a callback -- a fill or map function, a sort key, a
	// raylib hook -- cannot tell whether the script handed it `@myFunc` or
	// `@abs`; both are funcrefs.  Intrinsic ones used to come back null, because
	// RunFunction only knew how to push a frame and drive RunInner, and an
	// intrinsic has no bytecode for RunInner to run.
	public: static Boolean TestRunFunction();

	// RunFunction is synchronous: the host is blocked waiting for the result,
	// so there is no later in which to resume a pending intrinsic or come back
	// after a yield.  A callee that tries either must stop with a clear runtime
	// error -- not hang (yield used to loop forever), and not return early with
	// a stale continuation left behind (wait and import used to be mistaken for
	// RunFunction's own manual call).  Issue #44.
	// Run the fixture (source) up to its top-level yield, call funcName via
	// RunFunction, and check that the call failed with the synchronous-callback
	// error.
	private: static Boolean CheckRunFunctionFails(String source, String funcName);

	public: static Boolean TestRunFunctionCantWait();
	private: static Boolean _pushCallIntrinsicsMade;

	// ── Calling a function value from an intrinsic, asynchronously (PushCall) ──
	// Issue #34.  These test-only intrinsics are registered once, globally (as
	// all intrinsics are), under names no script would use.

	private: static void MakePushCallTestIntrinsics();

	private: static Boolean CheckGlobal(Interpreter interp, String name, String expected);

	public: static Boolean TestPushCall();
	private: static Boolean _getVarIntrinsicMade;

	// Context.GetVar: an intrinsic can read its arguments by parameter name
	// (the MiniScript 1.x style) as well as by position.  This must hold on a
	// first call, on a re-invocation after a not-done result, from inside a
	// script function whose own locals share those names, with a default
	// argument, and via RunFunction.  (The VM clears the register names of an
	// intrinsic's frame so they don't show up in the caller's `locals`; GetVar
	// must not depend on those names.)
	public: static Boolean TestGetVar();

	private: static Boolean CheckGlobalNamed(Interpreter interp, String testName, String name, String expected);

	// A function with more named variables than the register file can hold: past
	// MaxVarRegIndex the code generator spills them to the frame's variable map
	// (bugs.md entry 18).  Checks that the early ones -- the spilled ones -- still
	// read back, that a spilled variable can be updated and used as a loop
	// variable, and that they appear in `locals`.
	public: static Boolean TestSpilledLocals();
	private: static Boolean _stackGrowthIntrinsicsMade;

	// ── Growing the call stack and register stack (issue #27) ─────────────────
	// Both stacks start small and grow on demand.  Growing the register stack
	// reallocates it, so the VM must re-derive its cached pointer (Span, in C#)
	// into the current frame afterward.  The tricky case is a native callback
	// that runs script synchronously (RunFunction) deep enough to grow the
	// stack: when it returns, the calling frame must read and write its
	// registers in the new storage, not the old.

	private: static void MakeStackGrowthTestIntrinsics();

	public: static Boolean TestStackGrowth();

	// A single unlimited vm.Run() must carry an ordinary program (one that does
	// nothing to yield) all the way to the end.  If it returns while the VM is
	// still running, some opcode handler left the dispatch loop instead of
	// continuing to the next instruction -- in the C++ computed-goto build, this
	// is what a plain `break` in a VM_CASE (where VM_NEXT() was meant) does.
	// Such a bug gives correct results, but the host sleeps between slices, so
	// it shows up only as a drastic slowdown.  Keep this program touching a
	// broad range of opcodes (calls/returns, map and list stores, loops, isa,
	// comparisons) so that a stray exit in any of them is caught here.
	public: static Boolean TestRunWithoutYield();

	// A property setter that arrives in a host dictionary has to be noticed as
	// the map is born (GCMap.SeedOrder): the host's stores never pass the VM's
	// assignment path, so a setter nobody noticed would be silently dead -- it
	// would simply never fire, with no error to say why.
	public: static Boolean TestHostMapSetters();

	// Value.IsaContains has a hand-written twin in cpp/core/value.cpp rather
	// than a generated one, so the two can drift apart without anything
	// noticing -- and once did, over exactly the reflexive case below.  These
	// assertions run on both sides and hold them together.
	public: static Boolean TestIsaContains();

	public: static Boolean RunAll();
}; // end of struct UnitTests

// INLINE METHODS

} // end of namespace MiniScript
