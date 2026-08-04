// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CodeGenerator.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// CodeGenerator.cs - Compiles AST nodes to bytecode using the visitor pattern
// Uses CodeEmitterBase to support both direct bytecode and assembly text output.

#include "AST.g.h"
#include "CodeEmitter.g.h"
#include "ErrorTypes.g.h"

namespace MiniScript {

// DECLARATIONS

class CodeGeneratorStorage : public std::enable_shared_from_this<CodeGeneratorStorage>, public IASTVisitor {
	friend struct CodeGenerator;
	private: CodeEmitterBase _emitter;
	private: List<Boolean> _regInUse; // Which registers are currently in use
	private: Int32 _firstAvailable; // Lowest index that might be free
	private: Int32 _maxRegUsed; // High water mark for register usage
	private: Dictionary<String, Int32> _variableRegs; // variable name -> register
	private: List<String> _namedStack; // variables definitely assigned at the current point (stack-disciplined by conditional nesting)
	private: List<Boolean> _namedIsReg; // parallel to _namedStack: true if bound to a register by a NAME op, false if declared only via `locals.x =`
	private: String _localOnlyName; // while compiling the RHS of a first assignment, the variable being created ("" when inactive)
	private: List<String> _breakNames;
	private: List<Boolean> _breakIsReg;
	private: List<Int32> _breakStarts; // per open loop: where its accumulator starts
	private: List<Boolean> _breakSeen; // per open loop: has any break contributed yet?
	private: List<Int32> _loopNameMarks; // per open loop: _namedStack depth on entry
	private: Int32 _targetReg; // Target register for next expression (-1 = allocate)
	private: List<Int32> _loopExitLabels; // Stack of loop exit labels for break
	private: List<Int32> _loopContinueLabels; // Stack of loop continue labels for continue
	private: List<FuncDef> _functions; // Compile-time registry of all functions (for naming + disassembly)
	private: Boolean _globalScope;
	public: String FileName = ""; // Source file name, copied to each compiled FuncDef
	public: Value Error;
	// Definite assignment at the `break`s of each open loop -- what survives past a
	// loop that only ends by breaking out of it.  See NoteBreak/EndLoopNames.  The
	// per-loop accumulators are stacked end to end in _breakNames/_breakIsReg, with
	// _breakStarts giving each one's first index; the innermost loop's is always the
	// tail, so a loop's entries can be dropped by truncating.

	// True while compiling code whose named variables are GLOBALS rather than
	// registers -- that is, @main and only @main.  A module compiled for `import`
	// is a function returning its own locals, so its top-level names are locals
	// and this stays false there.  See notes/GLOBALS.md.
	// At global scope a named variable is a slot in the Globals table, so it gets
	// no register and no NAME op.  Assignments compile to GSTORE and reads to
	// GLOADC/GLOADV, all of which name the variable through this function's
	// global-reference table rather than through a register or the constant pool.

	public: CodeGeneratorStorage(CodeEmitterBase emitter);

	// Get all compiled functions (index 0 = @main, 1+ = inner functions)
	public: List<FuncDef> GetFunctions();

	// Allocate a register
	private: Int32 AllocReg();

	// Free a register so it can be reused
	private: void FreeReg(Int32 reg);

	// Allocate a block of consecutive registers
	// Returns the first register of the block
	private: Int32 AllocConsecutiveRegs(Int32 count);

	// Is this register currently bound to a variable?
	// LIST and MAP have to create their container before filling it, so unlike every
	// other Visit method they write their destination before reading their operands.
	// That is only safe if nothing the operands read lives in that register.  The
	// registers a nested expression can read are exactly those bound to variables:
	// directly (LOADC/LOADV rX, rVar), or by name at runtime (locals.x / globals.x,
	// which resolve through the frame's name table to the same register).  Anything
	// else it touches is a temp it allocated itself.
	// This is deliberately register-based rather than name-based: it does not care
	// *why* the register might be read, so it covers the locals/globals route that no
	// name-matching walk can see.  MayReadVar answers a different question -- whether
	// the RHS names the variable, which is what decides NAME ordering on a first
	// assignment -- and neither subsumes the other.
	private: Boolean IsLiveVariableReg(Int32 reg);

	// Compile an expression into a specific target register
	// The target register should already be allocated by the caller
	private: Int32 CompileInto(ASTNode node, Int32 targetReg);

	// Get target register if set, otherwise allocate a new one
	// IMPORTANT: Call this at the START of each Visit method, before any recursive calls
	private: Int32 GetTargetOrAlloc();

	// Compile an expression, placing result in a newly allocated register
	// Returns the register number holding the result
	public: Int32 Compile(ASTNode ast);

	// Reset temporary registers before compiling a new statement.
	// Keeps r0 and all variable registers; frees everything else.
	private: void ResetTempRegisters();

	// Compile a list of statements (a block body).
	// Resets temporary registers before each statement.
	private: void CompileBody(List<ASTNode> body);

	// After compiling a statement, guard against silently dropping an error.
	// A bare expression used as a statement throws its value away; if that value
	// is an error, nobody can ever catch it, so ERRCHK halts the program there
	// (with the discarding line in the stack trace).  Statements proper have no
	// meaningful result register, and are skipped.
	private: void EmitDiscardCheck(ASTNode stmt, Int32 resultReg);

	// Compile a body of statements that may not execute (a loop body, or an if
	// with no else).  Nothing it assigns is definitely assigned afterward, so any
	// names recorded while compiling it are forgotten on exit (the body's names
	// sit at the tail of _namedStack, since any nested conditional bodies have
	// already been entered and exited).
	// Visit(IfNode) does not use this: with both arms present it has to compare
	// them rather than discard both, so it does its own marking.
	private: void CompileConditionalBody(List<ASTNode> body);

	// Combine the two arms of an if/else into what is definitely assigned after it.
	// The "then" arm's names come in as thenNames/thenIsReg (already popped off the
	// stack, since the else arm must not see them -- it is an alternative path, not
	// a continuation).  The "else" arm's names are what sits above `mark` now.
	// Normally that is the intersection: `if c then n = 1 else n = 0` leaves n
	// assigned, which matters because MiniScript has no ternary operator and this
	// is how you write one.  Both arms give the variable the same register --
	// _variableRegs is not unwound between them, so the else arm reuses what the
	// then arm allocated -- so an entry surviving the merge describes one register,
	// as EnsureNamed requires.  It survives as register-bound only if both arms
	// bound it; a NAME in one arm and a `locals.x =` in the other leaves the name
	// assigned but its register not established on every path.
	// An arm that cannot complete normally (it returns, breaks, or continues) is
	// the exception: if control reaches the code after the if, that arm did not
	// fall through to it, so only the other arm's assignments matter and they hold
	// unconditionally.
	private: void MergeBranchNames(Int32 mark, List<String> thenNames, List<Boolean> thenIsReg, Boolean thenAbrupt, Boolean elseAbrupt);

	// Can this body only be left by jumping somewhere else -- a return, or a break
	// or continue out of the enclosing loop?  Used by MergeBranchNames; a body that
	// ends this way never falls through to whatever follows it.
	private: Boolean EndsAbruptly(List<ASTNode> body);

	// ── Loop-body variable registers ─────────────────────────────────────────
	// Register allocation assumes control flows straight through: a temp freed at
	// the end of an expression is free for whatever comes next.  A loop breaks
	// that assumption, because its back edge re-runs the condition after the body
	// has been compiled -- so a variable first assigned in the body must not sit
	// anywhere the condition writes.  Two things the condition writes are easy to
	// miss: temps it frees before the body is compiled, and, for any call it makes,
	// the entire callee frame from `calleeBase` up (see EmitCallSequence).
	// Rather than try to enumerate those after the fact, we give the body's new
	// variables their registers *before* compiling the condition.  Then they are
	// simply part of the live set the condition allocates around, and the ordinary
	// rules do the rest.
	// The registers are parked under an internal key, not the variable's own name,
	// so that the variable is still "new" at its first assignment -- which is what
	// decides NAME ordering, and what makes `c = c + 1` in the body the error it
	// should be rather than a read of an unwritten register.
	// A read that comes *before* the assignment does consult the reservation, since
	// it is the only way to compile one instruction that is right on every
	// iteration: see VisitIdentifier.  The reservation is what lets it name the
	// register the local is going to occupy.
	private: static String PendingVarRegKey(String varName);

	// Reserve a register for each variable the given loop body creates.  Returns
	// the names reserved, for ReleaseBodyVarRegs.  Does nothing at global scope,
	// where top-level names are slots rather than registers.
	private: List<String> ReserveBodyVarRegs(List<ASTNode> body);

	// Drop any reservation the body did not end up claiming.  A claimed one has
	// already been renamed to the variable itself by TakeVarReg.
	private: void ReleaseBodyVarRegs(List<String> reserved);

	// Allocate the register for a variable being created.  If an enclosing loop
	// reserved one for it, take that; the caller records it under the variable's
	// own name, which is what retires the reservation.
	private: Int32 TakeVarReg(String varName);

	// Collect the names of variables that assignments in this statement list
	// create.  Descends into nested loop and if bodies, since those assignments
	// happen in this scope too, but not into nested function bodies, whose
	// variables are locals of that function.
	private: void CollectAssignedVars(List<ASTNode> body, List<String> result);

	// ── Hoisting a loop body's NAME ops ──────────────────────────────────────
	// A variable first assigned inside a loop body has its NAME op emitted inside
	// the loop, where it re-runs every iteration even though the binding it
	// establishes never changes after the first.  The rotated loop (see
	// Visit(WhileNode)) has a preheader -- code that runs once, and only when the
	// body is going to run at least once -- which is the right home for it.
	// Hoisting is sound only for a name the body assigns before anything could
	// observe it, because NAME is not bookkeeping: it sets names[base+reg], the
	// guard LOADC checks before trusting the register, so running it early turns a
	// read that should have raised Undefined Identifier (or reached an enclosing
	// scope) into a read of an unwritten register.  A candidate must therefore be
	//   * assigned by a plain `x = ...` at the body's top level -- not inside a
	//     nested if or loop, where the assignment might not run at all;
	//   * unread by every statement up to and including that one;
	//   * out of reach of any `break` or `continue` ahead of it, which would carry
	//     control to code that can see the variable before it was assigned;
	//   * new here: no register yet, and not already definitely assigned.
	// MayReadVar answers the read test, and the dynamic-scope case with it, since
	// ScopeNode always answers true.  It is the same test Visit(AssignmentNode) uses
	// to decide whether a NAME may precede its own RHS, and it draws the boundary in
	// the same place: a closure over this frame, called in between, could still
	// observe the variable, because defining a function reads nothing.  Hoisting
	// therefore inherits exactly the imprecision straight-line assignment already
	// has, and adds none of its own.
	private: List<String> HoistableBodyNames(List<ASTNode> body);

	// Can this statement transfer control out of the enclosing loop body -- to the
	// code after the loop, or to the next iteration -- without running what follows
	// it?  A nested loop's own break and continue do not count: they bind to that
	// loop, so control still resumes here.  A `return` does not count either; it
	// leaves the function, so no later read can observe anything.
	private: Boolean MayLeaveLoop(ASTNode node);

	private: Boolean AnyLeavesLoop(List<ASTNode> nodes);

	private: static Boolean ContainsName(List<String> names, String name);

	// Emit the hoisted NAME ops into a loop preheader, and record the names as
	// definitely assigned so EnsureNamed skips them inside the body.  The caller
	// must pop back to the returned mark once the loop is closed: the preheader
	// runs only when the body does, so a zero-iteration loop leaves these names
	// undefined for the code that follows.
	private: Int32 EmitHoistedNames(List<String> names);

	// ── Definite assignment ──────────────────────────────────────────────────
	// _namedStack holds the variables that are definitely assigned at the current
	// point -- assigned on every path that reaches here.  Two things read it:
	//   * EnsureNamed, to skip a NAME op that a dominating one already covers.
	//     That only counts entries flagged in _namedIsReg, since only a NAME
	//     actually binds the name to a register.
	//   * The first-assignment check in Visit(AssignmentNode), which counts every
	//     entry: `locals.x = 1` creates x just as surely as an assignment does,
	//     it simply routes through the frame's variable map instead of a register.
	// The stack discipline is what makes "definitely" true rather than "somewhere
	// earlier": a conditional body's entries are popped on exit (see
	// CompileConditionalBody), and an if/else keeps only what both arms assigned
	// (see MergeBranchNames).

	// Record that varName is definitely assigned from here on.  isReg says whether
	// a NAME op bound it to its register, or it was only declared through `locals`.
	private: void PushName(String varName, Boolean isReg);

	// Drop every entry above mark, undoing the assignments a non-dominating body made.
	private: void PopNamesTo(Int32 mark);

	// Does a NAME op binding varName to its register already dominate this point?
	// Only such an entry counts: `locals.x = 1` makes x assigned without binding a
	// register, so it cannot stand in for the NAME.
	private: Boolean IsRegisterNamed(String varName);

	// Is varName definitely assigned at this point, by any means?
	private: Boolean IsDefinitelyAssigned(String varName);

	// Ensure a NAME op has been emitted for the given variable on a path that
	// dominates the current point.  If not, emit one now and record it.  This is
	// what makes a variable "exist" at runtime; it must run on every path that
	// assigns the variable (e.g. both branches of a single-line if).
	private: void EnsureNamed(String varName, Int32 varReg);

	// Record that `locals.x = ...` created x.  No register is bound, so this
	// counts for definite assignment but not for EnsureNamed.
	private: void NoteLocalsDeclared(String varName);

	// ── Definite assignment across a loop ────────────────────────────────────
	// A loop body normally contributes nothing, because it may run zero times.
	// `while true` (or any constant-true condition) is the exception worth
	// handling: the body always runs, and the only way to reach the code after the
	// loop is a `break`, so whatever is assigned at every break is assigned after
	// the loop.  This is not a nicety -- `while true` around an input prompt, with
	// a `break` once the input validates, is how the idiom is written:
	//     while true
	//         power = input("how much? ").val
	//         if power <= limit then break
	//     end while
	//     power = power * factor      // power is assigned here
	// Each open loop accumulates the intersection over the breaks seen so far.

	private: void ClearLoopNames();

	private: void BeginLoopNames();

	// Fold the current definite-assignment state into the innermost loop's
	// accumulator.  Only entries above the loop's own mark count: anything below it
	// was already assigned before the loop and needs no help from us.
	private: void NoteBreak();

	// Close the innermost loop, applying what its breaks established.  alwaysRuns
	// says the body is guaranteed to execute (a constant-true condition); without
	// that we cannot conclude anything, since the loop may run zero times.  A
	// constant-true loop with no break never falls through to the code after it, so
	// there is nothing to add there either.
	private: void EndLoopNames(Boolean alwaysRuns);

	// Does this loop condition always hold, so that the body is certain to run?
	// Only literals, judged by the same rule Value.BoolValue applies at run time:
	// a nonzero number, a nonempty string, a nonempty list or map, or the keyword
	// `true`.  `while true` is the form that matters; the rest come along because
	// the rule is "a literal we can evaluate here", not a special case for one
	// spelling.  Anything with a variable in it we decline to reason about, even
	// when it is obviously constant.
	private: Boolean IsAlwaysTrue(ASTNode condition);

	// ── Global scope ─────────────────────────────────────────────────────────

	// Key under which an internal (non-user) register is parked in _variableRegs.
	// That dictionary doubles as the set of registers ResetTempRegisters must
	// preserve, and '@' cannot appear in an identifier, so no user variable can
	// ever collide with one of these.
	private: static String LoopVarRegKey(String varName);

	// Intern a global name in this function's global-reference table.  The BC
	// operand of GLOADC/GLOADV/GSTORE is an index into that table, resolved to a
	// slot number the first time the function runs against a given namespace.
	private: Int32 AddGlobalRef(String varName);

	// Begin compiling top-level code.  Nothing to set up: at global scope a named
	// variable is a slot, reached by name through the reference table, so there is
	// no globals map to cache in a register and no register allocation to do.
	private: void BeginGlobalScope();

	// Store a register into the named global.  Creates the global if it is new;
	// this is the only way a name comes into existence at top level.
	private: void EmitGlobalStore(String varName, Int32 valueReg);

	// Read a free variable -- one with no register in the function being compiled
	// -- into resultReg.  At global scope that is certainly a global.  Inside a
	// function it is usually a global too (an intrinsic, or a top-level name), but
	// it may be an enclosing local, so GLOADC/GLOADV check the frame before
	// reaching for the slot and fall back to the full run-time search if anything
	// could shadow it.  That check is at run time rather than here on purpose:
	// names can enter a scope dynamically -- `locals["x"] = 1`, a map handed to
	// another function, `import` -- so no amount of looking at the lexical chain
	// would be sound.  See notes/GLOBALS.md section 8, stage 5.
	private: void EmitFreeLoad(Boolean addressOf, Int32 resultReg, String varName, String comment);

	// Compile a complete function from a single expression/statement
	public: FuncDef CompileFunction(ASTNode ast, String funcName);

	// Compile a module for import: like CompileProgram but appends LOCALS + RETURN
	// so the module returns its own top-level locals map as its result.
	// Returns all compiled functions (index 0 = @main, 1+ = inner functions).
	public: List<FuncDef> CompileImport(List<ASTNode> statements, String funcName);

	// Compile a complete function from a list of statements (program)
	public: FuncDef CompileProgram(List<ASTNode> statements, String funcName);

	// --- Visit methods for each AST node type ---

	public: Int32 Visit(NumberNode node);

	public: Int32 Visit(StringNode node);

	private: Int32 VisitIdentifier(IdentifierNode node, bool addressOf);

	// Emit a LOADV (addressOf) or LOADC (auto-invoking) that copies R[srcReg]
	// into R[resultReg] after checking that srcReg is still named nameVal,
	// falling back to a runtime lookup by that name if it is not.
	// The usual kC forms carry the name's constant-pool index in the 8-bit C
	// field, so they can only reach the first 256 constants.  Past that we emit
	// the rC forms instead: a LOAD_rA_kBC (whose constant index is 16-bit) puts
	// the name in a scratch register, and the opcode reads it from there.  The
	// two instructions are adjacent and the scratch register is freed right
	// after, so it costs nothing in the common case.
	private: void EmitNamedLoad(Boolean addressOf, Int32 resultReg, Int32 srcReg, Value nameVal, String comment);

	public: Int32 Visit(IdentifierNode node);

	public: Int32 Visit(AssignmentNode node);

	// Assignment at top level: evaluate the right-hand side into a temp, then
	// store it into the named global.
	// None of the register bookkeeping in the local case applies here.  There is
	// no NAME op, because the variable is not a register.  There is no
	// first-assignment special case either: the slot is not written until the RHS
	// has been evaluated, so `n = n + 1` creating a global reads the enclosing
	// scope (or fails as undefined) exactly the way it should, with no temp needed
	// to order things.
	private: Int32 VisitGlobalAssignment(AssignmentNode node);

	public: Int32 Visit(IndexedAssignmentNode node);

	public: Int32 Visit(UnaryOpNode node);

	public: Int32 Visit(BinaryOpNode node);

	// Compile 'and'/'or' with short-circuit evaluation.  The right operand is
	// only evaluated when the left operand does not already determine the
	// result.  An error operand never short-circuits the truthiness test
	// (which would throw); BRERR peels it off first so the surviving
	// BRTRUE/BRFALSE only ever sees a non-error value.
	private: Int32 CompileShortCircuit(BinaryOpNode node);

	public: Int32 Visit(ComparisonChainNode node);

	// Emit a single comparison opcode into destReg
	private: void EmitComparison(String op, Int32 destReg, Int32 leftReg, Int32 rightReg);

	public: Int32 Visit(CallNode node);

	// Compile a call to a user-defined function (funcref in a register)
	private: Int32 CompileUserCall(CallNode node, Int32 funcVarReg, Int32 explicitTarget);

	// Compile argument expressions into temporary registers.
	private: List<Int32> CompileArguments(List<ASTNode> arguments);

	// Emit ARGBLK + ARG instructions, compute callee frame, emit CALL, and free
	// the argument registers.  Returns the result register.
	private: Int32 EmitCallSequence(Int32 funcReg, List<Int32> argRegs, Int32 explicitTarget, String comment);

	public: Int32 Visit(GroupNode node);

	public: Int32 Visit(ListNode node);

	public: Int32 Visit(MapNode node);

	public: Int32 Visit(IndexNode node);

	// Compile index access, optionally as address-of (no auto-invoke)
	private: Int32 VisitIndex(IndexNode node, bool addressOf);

	public: Int32 Visit(SliceNode node);

	public: Int32 Visit(MemberNode node);

	// Compile member access, optionally as address-of (no auto-invoke)
	private: Int32 VisitMember(MemberNode node, bool addressOf);

	// Shared tail for VisitIndex/VisitMember: emit INDEX (address-of),
	// IDXGET (bracket access, no auto-invoke), or
	// METHFIND + optional SETSELF + CALLIFREF (dot access with auto-invoke).
	private: void EmitAccessOrInvoke(Int32 resultReg, Int32 targetReg, Int32 indexReg, bool addressOf, bool isDotAccess, ASTNode targetNode, String comment);

	public: Int32 Visit(ExprCallNode node);

	public: Int32 Visit(MethodCallNode node);

	public: Int32 Visit(WhileNode node);

	public: Int32 Visit(IfNode node);

	public: Int32 Visit(ForNode node);

	public: Int32 Visit(BreakNode node);

	public: Int32 Visit(ContinueNode node);

	// Try to evaluate an AST node as a compile-time constant value.
	// Returns true if successful, with the result in 'result'.
	// Handles: numbers, strings, null/true/false, unary minus, list/map literals.
	// Lists and maps are automatically frozen (immutable).
	public: static Boolean TryEvaluateConstant(ASTNode node, Value* result);

	public: Int32 Visit(FunctionNode node);

	// Allocate (or retrieve) the register for 'self'
	private: Int32 GetSelfReg();

	// Allocate (or retrieve) the register for 'super'
	private: Int32 GetSuperReg();
	private: Boolean _scanUsesSelf;
	private: Boolean _scanUsesSuper;

	// Pre-scan a function body to reserve the self/super registers up front,
	// before any temporary registers are allocated.  The VM populates these
	// registers with method-call context at function entry, so if they were
	// allocated lazily (on first reference) they could land on a slot already
	// used and freed as a temp — and a later temp would clobber the context.
	// Does NOT descend into nested function bodies: a self/super reference
	// inside an inner function needs a register in that function, not this one.

	private: void ReserveSelfSuperRegs(List<ASTNode> body);

	private: void ScanNodeList(List<ASTNode> nodes);

	private: void ScanNode(ASTNode node);

	public: Int32 Visit(SelfNode node);

	public: Int32 Visit(SuperNode node);

	public: Int32 Visit(ScopeNode node);

	// Emit a method call: METHFIND + optional SETSELF + ARGBLK + ARGs + CALL
	// receiverReg: register holding the receiver object
	// methodKey: string name of the method
	// arguments: list of argument AST nodes
	// preserveSelf: if true, emit SETSELF to keep current self (for super.method() calls)
	private: Int32 EmitMethodCall(Int32 receiverReg, String methodKey, List<ASTNode> arguments, bool preserveSelf);

	public: Int32 Visit(ReturnNode node);
}; // end of class CodeGeneratorStorage

// Compiles AST nodes to bytecode
struct CodeGenerator : public IASTVisitor {
	friend class CodeGeneratorStorage;
	protected: std::shared_ptr<CodeGeneratorStorage> storage;
  public:
	CodeGenerator(std::shared_ptr<CodeGeneratorStorage> stor) : storage(stor) {}
	CodeGenerator() : storage(nullptr) {}
	CodeGenerator(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const CodeGenerator& inst) { return inst.storage == nullptr; }
	private: CodeGeneratorStorage* get() const;

	private: CodeEmitterBase _emitter();
	private: void set__emitter(CodeEmitterBase _v);
	private: List<Boolean> _regInUse(); // Which registers are currently in use
	private: void set__regInUse(List<Boolean> _v); // Which registers are currently in use
	private: Int32 _firstAvailable(); // Lowest index that might be free
	private: void set__firstAvailable(Int32 _v); // Lowest index that might be free
	private: Int32 _maxRegUsed(); // High water mark for register usage
	private: void set__maxRegUsed(Int32 _v); // High water mark for register usage
	private: Dictionary<String, Int32> _variableRegs(); // variable name -> register
	private: void set__variableRegs(Dictionary<String, Int32> _v); // variable name -> register
	private: List<String> _namedStack(); // variables definitely assigned at the current point (stack-disciplined by conditional nesting)
	private: void set__namedStack(List<String> _v); // variables definitely assigned at the current point (stack-disciplined by conditional nesting)
	private: List<Boolean> _namedIsReg(); // parallel to _namedStack: true if bound to a register by a NAME op, false if declared only via `locals.x =`
	private: void set__namedIsReg(List<Boolean> _v); // parallel to _namedStack: true if bound to a register by a NAME op, false if declared only via `locals.x =`
	private: String _localOnlyName(); // while compiling the RHS of a first assignment, the variable being created ("" when inactive)
	private: void set__localOnlyName(String _v); // while compiling the RHS of a first assignment, the variable being created ("" when inactive)
	private: List<String> _breakNames();
	private: void set__breakNames(List<String> _v);
	private: List<Boolean> _breakIsReg();
	private: void set__breakIsReg(List<Boolean> _v);
	private: List<Int32> _breakStarts(); // per open loop: where its accumulator starts
	private: void set__breakStarts(List<Int32> _v); // per open loop: where its accumulator starts
	private: List<Boolean> _breakSeen(); // per open loop: has any break contributed yet?
	private: void set__breakSeen(List<Boolean> _v); // per open loop: has any break contributed yet?
	private: List<Int32> _loopNameMarks(); // per open loop: _namedStack depth on entry
	private: void set__loopNameMarks(List<Int32> _v); // per open loop: _namedStack depth on entry
	private: Int32 _targetReg(); // Target register for next expression (-1 = allocate)
	private: void set__targetReg(Int32 _v); // Target register for next expression (-1 = allocate)
	private: List<Int32> _loopExitLabels(); // Stack of loop exit labels for break
	private: void set__loopExitLabels(List<Int32> _v); // Stack of loop exit labels for break
	private: List<Int32> _loopContinueLabels(); // Stack of loop continue labels for continue
	private: void set__loopContinueLabels(List<Int32> _v); // Stack of loop continue labels for continue
	private: List<FuncDef> _functions(); // Compile-time registry of all functions (for naming + disassembly)
	private: void set__functions(List<FuncDef> _v); // Compile-time registry of all functions (for naming + disassembly)
	private: Boolean _globalScope();
	private: void set__globalScope(Boolean _v);
	public: String FileName(); // Source file name, copied to each compiled FuncDef
	public: void set_FileName(String _v); // Source file name, copied to each compiled FuncDef
	public: Value Error();
	public: void set_Error(Value _v);
	// Definite assignment at the `break`s of each open loop -- what survives past a
	// loop that only ends by breaking out of it.  See NoteBreak/EndLoopNames.  The
	// per-loop accumulators are stacked end to end in _breakNames/_breakIsReg, with
	// _breakStarts giving each one's first index; the innermost loop's is always the
	// tail, so a loop's entries can be dropped by truncating.

	// True while compiling code whose named variables are GLOBALS rather than
	// registers -- that is, @main and only @main.  A module compiled for `import`
	// is a function returning its own locals, so its top-level names are locals
	// and this stays false there.  See notes/GLOBALS.md.
	// At global scope a named variable is a slot in the Globals table, so it gets
	// no register and no NAME op.  Assignments compile to GSTORE and reads to
	// GLOADC/GLOADV, all of which name the variable through this function's
	// global-reference table rather than through a register or the constant pool.

	public: static CodeGenerator New(CodeEmitterBase emitter) {
		return CodeGenerator(std::make_shared<CodeGeneratorStorage>(emitter));
	}

	// Get all compiled functions (index 0 = @main, 1+ = inner functions)
	public: inline List<FuncDef> GetFunctions();

	// Allocate a register
	private: inline Int32 AllocReg();

	// Free a register so it can be reused
	private: inline void FreeReg(Int32 reg);

	// Allocate a block of consecutive registers
	// Returns the first register of the block
	private: inline Int32 AllocConsecutiveRegs(Int32 count);

	// Is this register currently bound to a variable?
	// LIST and MAP have to create their container before filling it, so unlike every
	// other Visit method they write their destination before reading their operands.
	// That is only safe if nothing the operands read lives in that register.  The
	// registers a nested expression can read are exactly those bound to variables:
	// directly (LOADC/LOADV rX, rVar), or by name at runtime (locals.x / globals.x,
	// which resolve through the frame's name table to the same register).  Anything
	// else it touches is a temp it allocated itself.
	// This is deliberately register-based rather than name-based: it does not care
	// *why* the register might be read, so it covers the locals/globals route that no
	// name-matching walk can see.  MayReadVar answers a different question -- whether
	// the RHS names the variable, which is what decides NAME ordering on a first
	// assignment -- and neither subsumes the other.
	private: inline Boolean IsLiveVariableReg(Int32 reg);

	// Compile an expression into a specific target register
	// The target register should already be allocated by the caller
	private: inline Int32 CompileInto(ASTNode node, Int32 targetReg);

	// Get target register if set, otherwise allocate a new one
	// IMPORTANT: Call this at the START of each Visit method, before any recursive calls
	private: inline Int32 GetTargetOrAlloc();

	// Compile an expression, placing result in a newly allocated register
	// Returns the register number holding the result
	public: inline Int32 Compile(ASTNode ast);

	// Reset temporary registers before compiling a new statement.
	// Keeps r0 and all variable registers; frees everything else.
	private: inline void ResetTempRegisters();

	// Compile a list of statements (a block body).
	// Resets temporary registers before each statement.
	private: inline void CompileBody(List<ASTNode> body);

	// After compiling a statement, guard against silently dropping an error.
	// A bare expression used as a statement throws its value away; if that value
	// is an error, nobody can ever catch it, so ERRCHK halts the program there
	// (with the discarding line in the stack trace).  Statements proper have no
	// meaningful result register, and are skipped.
	private: inline void EmitDiscardCheck(ASTNode stmt, Int32 resultReg);

	// Compile a body of statements that may not execute (a loop body, or an if
	// with no else).  Nothing it assigns is definitely assigned afterward, so any
	// names recorded while compiling it are forgotten on exit (the body's names
	// sit at the tail of _namedStack, since any nested conditional bodies have
	// already been entered and exited).
	// Visit(IfNode) does not use this: with both arms present it has to compare
	// them rather than discard both, so it does its own marking.
	private: inline void CompileConditionalBody(List<ASTNode> body);

	// Combine the two arms of an if/else into what is definitely assigned after it.
	// The "then" arm's names come in as thenNames/thenIsReg (already popped off the
	// stack, since the else arm must not see them -- it is an alternative path, not
	// a continuation).  The "else" arm's names are what sits above `mark` now.
	// Normally that is the intersection: `if c then n = 1 else n = 0` leaves n
	// assigned, which matters because MiniScript has no ternary operator and this
	// is how you write one.  Both arms give the variable the same register --
	// _variableRegs is not unwound between them, so the else arm reuses what the
	// then arm allocated -- so an entry surviving the merge describes one register,
	// as EnsureNamed requires.  It survives as register-bound only if both arms
	// bound it; a NAME in one arm and a `locals.x =` in the other leaves the name
	// assigned but its register not established on every path.
	// An arm that cannot complete normally (it returns, breaks, or continues) is
	// the exception: if control reaches the code after the if, that arm did not
	// fall through to it, so only the other arm's assignments matter and they hold
	// unconditionally.
	private: inline void MergeBranchNames(Int32 mark, List<String> thenNames, List<Boolean> thenIsReg, Boolean thenAbrupt, Boolean elseAbrupt);

	// Can this body only be left by jumping somewhere else -- a return, or a break
	// or continue out of the enclosing loop?  Used by MergeBranchNames; a body that
	// ends this way never falls through to whatever follows it.
	private: inline Boolean EndsAbruptly(List<ASTNode> body);

	// ── Loop-body variable registers ─────────────────────────────────────────
	// Register allocation assumes control flows straight through: a temp freed at
	// the end of an expression is free for whatever comes next.  A loop breaks
	// that assumption, because its back edge re-runs the condition after the body
	// has been compiled -- so a variable first assigned in the body must not sit
	// anywhere the condition writes.  Two things the condition writes are easy to
	// miss: temps it frees before the body is compiled, and, for any call it makes,
	// the entire callee frame from `calleeBase` up (see EmitCallSequence).
	// Rather than try to enumerate those after the fact, we give the body's new
	// variables their registers *before* compiling the condition.  Then they are
	// simply part of the live set the condition allocates around, and the ordinary
	// rules do the rest.
	// The registers are parked under an internal key, not the variable's own name,
	// so that the variable is still "new" at its first assignment -- which is what
	// decides NAME ordering, and what makes `c = c + 1` in the body the error it
	// should be rather than a read of an unwritten register.
	// A read that comes *before* the assignment does consult the reservation, since
	// it is the only way to compile one instruction that is right on every
	// iteration: see VisitIdentifier.  The reservation is what lets it name the
	// register the local is going to occupy.
	private: static String PendingVarRegKey(String varName) { return CodeGeneratorStorage::PendingVarRegKey(varName); }

	// Reserve a register for each variable the given loop body creates.  Returns
	// the names reserved, for ReleaseBodyVarRegs.  Does nothing at global scope,
	// where top-level names are slots rather than registers.
	private: inline List<String> ReserveBodyVarRegs(List<ASTNode> body);

	// Drop any reservation the body did not end up claiming.  A claimed one has
	// already been renamed to the variable itself by TakeVarReg.
	private: inline void ReleaseBodyVarRegs(List<String> reserved);

	// Allocate the register for a variable being created.  If an enclosing loop
	// reserved one for it, take that; the caller records it under the variable's
	// own name, which is what retires the reservation.
	private: inline Int32 TakeVarReg(String varName);

	// Collect the names of variables that assignments in this statement list
	// create.  Descends into nested loop and if bodies, since those assignments
	// happen in this scope too, but not into nested function bodies, whose
	// variables are locals of that function.
	private: inline void CollectAssignedVars(List<ASTNode> body, List<String> result);

	// ── Hoisting a loop body's NAME ops ──────────────────────────────────────
	// A variable first assigned inside a loop body has its NAME op emitted inside
	// the loop, where it re-runs every iteration even though the binding it
	// establishes never changes after the first.  The rotated loop (see
	// Visit(WhileNode)) has a preheader -- code that runs once, and only when the
	// body is going to run at least once -- which is the right home for it.
	// Hoisting is sound only for a name the body assigns before anything could
	// observe it, because NAME is not bookkeeping: it sets names[base+reg], the
	// guard LOADC checks before trusting the register, so running it early turns a
	// read that should have raised Undefined Identifier (or reached an enclosing
	// scope) into a read of an unwritten register.  A candidate must therefore be
	//   * assigned by a plain `x = ...` at the body's top level -- not inside a
	//     nested if or loop, where the assignment might not run at all;
	//   * unread by every statement up to and including that one;
	//   * out of reach of any `break` or `continue` ahead of it, which would carry
	//     control to code that can see the variable before it was assigned;
	//   * new here: no register yet, and not already definitely assigned.
	// MayReadVar answers the read test, and the dynamic-scope case with it, since
	// ScopeNode always answers true.  It is the same test Visit(AssignmentNode) uses
	// to decide whether a NAME may precede its own RHS, and it draws the boundary in
	// the same place: a closure over this frame, called in between, could still
	// observe the variable, because defining a function reads nothing.  Hoisting
	// therefore inherits exactly the imprecision straight-line assignment already
	// has, and adds none of its own.
	private: inline List<String> HoistableBodyNames(List<ASTNode> body);

	// Can this statement transfer control out of the enclosing loop body -- to the
	// code after the loop, or to the next iteration -- without running what follows
	// it?  A nested loop's own break and continue do not count: they bind to that
	// loop, so control still resumes here.  A `return` does not count either; it
	// leaves the function, so no later read can observe anything.
	private: inline Boolean MayLeaveLoop(ASTNode node);

	private: inline Boolean AnyLeavesLoop(List<ASTNode> nodes);

	private: static Boolean ContainsName(List<String> names, String name) { return CodeGeneratorStorage::ContainsName(names, name); }

	// Emit the hoisted NAME ops into a loop preheader, and record the names as
	// definitely assigned so EnsureNamed skips them inside the body.  The caller
	// must pop back to the returned mark once the loop is closed: the preheader
	// runs only when the body does, so a zero-iteration loop leaves these names
	// undefined for the code that follows.
	private: inline Int32 EmitHoistedNames(List<String> names);

	// ── Definite assignment ──────────────────────────────────────────────────
	// _namedStack holds the variables that are definitely assigned at the current
	// point -- assigned on every path that reaches here.  Two things read it:
	//   * EnsureNamed, to skip a NAME op that a dominating one already covers.
	//     That only counts entries flagged in _namedIsReg, since only a NAME
	//     actually binds the name to a register.
	//   * The first-assignment check in Visit(AssignmentNode), which counts every
	//     entry: `locals.x = 1` creates x just as surely as an assignment does,
	//     it simply routes through the frame's variable map instead of a register.
	// The stack discipline is what makes "definitely" true rather than "somewhere
	// earlier": a conditional body's entries are popped on exit (see
	// CompileConditionalBody), and an if/else keeps only what both arms assigned
	// (see MergeBranchNames).

	// Record that varName is definitely assigned from here on.  isReg says whether
	// a NAME op bound it to its register, or it was only declared through `locals`.
	private: inline void PushName(String varName, Boolean isReg);

	// Drop every entry above mark, undoing the assignments a non-dominating body made.
	private: inline void PopNamesTo(Int32 mark);

	// Does a NAME op binding varName to its register already dominate this point?
	// Only such an entry counts: `locals.x = 1` makes x assigned without binding a
	// register, so it cannot stand in for the NAME.
	private: inline Boolean IsRegisterNamed(String varName);

	// Is varName definitely assigned at this point, by any means?
	private: inline Boolean IsDefinitelyAssigned(String varName);

	// Ensure a NAME op has been emitted for the given variable on a path that
	// dominates the current point.  If not, emit one now and record it.  This is
	// what makes a variable "exist" at runtime; it must run on every path that
	// assigns the variable (e.g. both branches of a single-line if).
	private: inline void EnsureNamed(String varName, Int32 varReg);

	// Record that `locals.x = ...` created x.  No register is bound, so this
	// counts for definite assignment but not for EnsureNamed.
	private: inline void NoteLocalsDeclared(String varName);

	// ── Definite assignment across a loop ────────────────────────────────────
	// A loop body normally contributes nothing, because it may run zero times.
	// `while true` (or any constant-true condition) is the exception worth
	// handling: the body always runs, and the only way to reach the code after the
	// loop is a `break`, so whatever is assigned at every break is assigned after
	// the loop.  This is not a nicety -- `while true` around an input prompt, with
	// a `break` once the input validates, is how the idiom is written:
	//     while true
	//         power = input("how much? ").val
	//         if power <= limit then break
	//     end while
	//     power = power * factor      // power is assigned here
	// Each open loop accumulates the intersection over the breaks seen so far.

	private: inline void ClearLoopNames();

	private: inline void BeginLoopNames();

	// Fold the current definite-assignment state into the innermost loop's
	// accumulator.  Only entries above the loop's own mark count: anything below it
	// was already assigned before the loop and needs no help from us.
	private: inline void NoteBreak();

	// Close the innermost loop, applying what its breaks established.  alwaysRuns
	// says the body is guaranteed to execute (a constant-true condition); without
	// that we cannot conclude anything, since the loop may run zero times.  A
	// constant-true loop with no break never falls through to the code after it, so
	// there is nothing to add there either.
	private: inline void EndLoopNames(Boolean alwaysRuns);

	// Does this loop condition always hold, so that the body is certain to run?
	// Only literals, judged by the same rule Value.BoolValue applies at run time:
	// a nonzero number, a nonempty string, a nonempty list or map, or the keyword
	// `true`.  `while true` is the form that matters; the rest come along because
	// the rule is "a literal we can evaluate here", not a special case for one
	// spelling.  Anything with a variable in it we decline to reason about, even
	// when it is obviously constant.
	private: inline Boolean IsAlwaysTrue(ASTNode condition);

	// ── Global scope ─────────────────────────────────────────────────────────

	// Key under which an internal (non-user) register is parked in _variableRegs.
	// That dictionary doubles as the set of registers ResetTempRegisters must
	// preserve, and '@' cannot appear in an identifier, so no user variable can
	// ever collide with one of these.
	private: static String LoopVarRegKey(String varName) { return CodeGeneratorStorage::LoopVarRegKey(varName); }

	// Intern a global name in this function's global-reference table.  The BC
	// operand of GLOADC/GLOADV/GSTORE is an index into that table, resolved to a
	// slot number the first time the function runs against a given namespace.
	private: inline Int32 AddGlobalRef(String varName);

	// Begin compiling top-level code.  Nothing to set up: at global scope a named
	// variable is a slot, reached by name through the reference table, so there is
	// no globals map to cache in a register and no register allocation to do.
	private: inline void BeginGlobalScope();

	// Store a register into the named global.  Creates the global if it is new;
	// this is the only way a name comes into existence at top level.
	private: inline void EmitGlobalStore(String varName, Int32 valueReg);

	// Read a free variable -- one with no register in the function being compiled
	// -- into resultReg.  At global scope that is certainly a global.  Inside a
	// function it is usually a global too (an intrinsic, or a top-level name), but
	// it may be an enclosing local, so GLOADC/GLOADV check the frame before
	// reaching for the slot and fall back to the full run-time search if anything
	// could shadow it.  That check is at run time rather than here on purpose:
	// names can enter a scope dynamically -- `locals["x"] = 1`, a map handed to
	// another function, `import` -- so no amount of looking at the lexical chain
	// would be sound.  See notes/GLOBALS.md section 8, stage 5.
	private: inline void EmitFreeLoad(Boolean addressOf, Int32 resultReg, String varName, String comment);

	// Compile a complete function from a single expression/statement
	public: inline FuncDef CompileFunction(ASTNode ast, String funcName);

	// Compile a module for import: like CompileProgram but appends LOCALS + RETURN
	// so the module returns its own top-level locals map as its result.
	// Returns all compiled functions (index 0 = @main, 1+ = inner functions).
	public: inline List<FuncDef> CompileImport(List<ASTNode> statements, String funcName);

	// Compile a complete function from a list of statements (program)
	public: inline FuncDef CompileProgram(List<ASTNode> statements, String funcName);

	// --- Visit methods for each AST node type ---

	public: inline Int32 Visit(NumberNode node);

	public: inline Int32 Visit(StringNode node);

	private: inline Int32 VisitIdentifier(IdentifierNode node, bool addressOf);

	// Emit a LOADV (addressOf) or LOADC (auto-invoking) that copies R[srcReg]
	// into R[resultReg] after checking that srcReg is still named nameVal,
	// falling back to a runtime lookup by that name if it is not.
	// The usual kC forms carry the name's constant-pool index in the 8-bit C
	// field, so they can only reach the first 256 constants.  Past that we emit
	// the rC forms instead: a LOAD_rA_kBC (whose constant index is 16-bit) puts
	// the name in a scratch register, and the opcode reads it from there.  The
	// two instructions are adjacent and the scratch register is freed right
	// after, so it costs nothing in the common case.
	private: inline void EmitNamedLoad(Boolean addressOf, Int32 resultReg, Int32 srcReg, Value nameVal, String comment);

	public: inline Int32 Visit(IdentifierNode node);

	public: inline Int32 Visit(AssignmentNode node);

	// Assignment at top level: evaluate the right-hand side into a temp, then
	// store it into the named global.
	// None of the register bookkeeping in the local case applies here.  There is
	// no NAME op, because the variable is not a register.  There is no
	// first-assignment special case either: the slot is not written until the RHS
	// has been evaluated, so `n = n + 1` creating a global reads the enclosing
	// scope (or fails as undefined) exactly the way it should, with no temp needed
	// to order things.
	private: inline Int32 VisitGlobalAssignment(AssignmentNode node);

	public: inline Int32 Visit(IndexedAssignmentNode node);

	public: inline Int32 Visit(UnaryOpNode node);

	public: inline Int32 Visit(BinaryOpNode node);

	// Compile 'and'/'or' with short-circuit evaluation.  The right operand is
	// only evaluated when the left operand does not already determine the
	// result.  An error operand never short-circuits the truthiness test
	// (which would throw); BRERR peels it off first so the surviving
	// BRTRUE/BRFALSE only ever sees a non-error value.
	private: inline Int32 CompileShortCircuit(BinaryOpNode node);

	public: inline Int32 Visit(ComparisonChainNode node);

	// Emit a single comparison opcode into destReg
	private: inline void EmitComparison(String op, Int32 destReg, Int32 leftReg, Int32 rightReg);

	public: inline Int32 Visit(CallNode node);

	// Compile a call to a user-defined function (funcref in a register)
	private: inline Int32 CompileUserCall(CallNode node, Int32 funcVarReg, Int32 explicitTarget);

	// Compile argument expressions into temporary registers.
	private: inline List<Int32> CompileArguments(List<ASTNode> arguments);

	// Emit ARGBLK + ARG instructions, compute callee frame, emit CALL, and free
	// the argument registers.  Returns the result register.
	private: inline Int32 EmitCallSequence(Int32 funcReg, List<Int32> argRegs, Int32 explicitTarget, String comment);

	public: inline Int32 Visit(GroupNode node);

	public: inline Int32 Visit(ListNode node);

	public: inline Int32 Visit(MapNode node);

	public: inline Int32 Visit(IndexNode node);

	// Compile index access, optionally as address-of (no auto-invoke)
	private: inline Int32 VisitIndex(IndexNode node, bool addressOf);

	public: inline Int32 Visit(SliceNode node);

	public: inline Int32 Visit(MemberNode node);

	// Compile member access, optionally as address-of (no auto-invoke)
	private: inline Int32 VisitMember(MemberNode node, bool addressOf);

	// Shared tail for VisitIndex/VisitMember: emit INDEX (address-of),
	// IDXGET (bracket access, no auto-invoke), or
	// METHFIND + optional SETSELF + CALLIFREF (dot access with auto-invoke).
	private: inline void EmitAccessOrInvoke(Int32 resultReg, Int32 targetReg, Int32 indexReg, bool addressOf, bool isDotAccess, ASTNode targetNode, String comment);

	public: inline Int32 Visit(ExprCallNode node);

	public: inline Int32 Visit(MethodCallNode node);

	public: inline Int32 Visit(WhileNode node);

	public: inline Int32 Visit(IfNode node);

	public: inline Int32 Visit(ForNode node);

	public: inline Int32 Visit(BreakNode node);

	public: inline Int32 Visit(ContinueNode node);

	// Try to evaluate an AST node as a compile-time constant value.
	// Returns true if successful, with the result in 'result'.
	// Handles: numbers, strings, null/true/false, unary minus, list/map literals.
	// Lists and maps are automatically frozen (immutable).
	public: static Boolean TryEvaluateConstant(ASTNode node, Value* result) { return CodeGeneratorStorage::TryEvaluateConstant(node, result); }

	public: inline Int32 Visit(FunctionNode node);

	// Allocate (or retrieve) the register for 'self'
	private: inline Int32 GetSelfReg();

	// Allocate (or retrieve) the register for 'super'
	private: inline Int32 GetSuperReg();
	private: Boolean _scanUsesSelf();
	private: void set__scanUsesSelf(Boolean _v);
	private: Boolean _scanUsesSuper();
	private: void set__scanUsesSuper(Boolean _v);

	// Pre-scan a function body to reserve the self/super registers up front,
	// before any temporary registers are allocated.  The VM populates these
	// registers with method-call context at function entry, so if they were
	// allocated lazily (on first reference) they could land on a slot already
	// used and freed as a temp — and a later temp would clobber the context.
	// Does NOT descend into nested function bodies: a self/super reference
	// inside an inner function needs a register in that function, not this one.

	private: inline void ReserveSelfSuperRegs(List<ASTNode> body);

	private: inline void ScanNodeList(List<ASTNode> nodes);

	private: inline void ScanNode(ASTNode node);

	public: inline Int32 Visit(SelfNode node);

	public: inline Int32 Visit(SuperNode node);

	public: inline Int32 Visit(ScopeNode node);

	// Emit a method call: METHFIND + optional SETSELF + ARGBLK + ARGs + CALL
	// receiverReg: register holding the receiver object
	// methodKey: string name of the method
	// arguments: list of argument AST nodes
	// preserveSelf: if true, emit SETSELF to keep current self (for super.method() calls)
	private: inline Int32 EmitMethodCall(Int32 receiverReg, String methodKey, List<ASTNode> arguments, bool preserveSelf);

	public: inline Int32 Visit(ReturnNode node);
}; // end of struct CodeGenerator

// INLINE METHODS

inline CodeGeneratorStorage* CodeGenerator::get() const { return static_cast<CodeGeneratorStorage*>(storage.get()); }
inline CodeEmitterBase CodeGenerator::_emitter() { return get()->_emitter; }
inline void CodeGenerator::set__emitter(CodeEmitterBase _v) { get()->_emitter = _v; }
inline List<Boolean> CodeGenerator::_regInUse() { return get()->_regInUse; } // Which registers are currently in use
inline void CodeGenerator::set__regInUse(List<Boolean> _v) { get()->_regInUse = _v; } // Which registers are currently in use
inline Int32 CodeGenerator::_firstAvailable() { return get()->_firstAvailable; } // Lowest index that might be free
inline void CodeGenerator::set__firstAvailable(Int32 _v) { get()->_firstAvailable = _v; } // Lowest index that might be free
inline Int32 CodeGenerator::_maxRegUsed() { return get()->_maxRegUsed; } // High water mark for register usage
inline void CodeGenerator::set__maxRegUsed(Int32 _v) { get()->_maxRegUsed = _v; } // High water mark for register usage
inline Dictionary<String, Int32> CodeGenerator::_variableRegs() { return get()->_variableRegs; } // variable name -> register
inline void CodeGenerator::set__variableRegs(Dictionary<String, Int32> _v) { get()->_variableRegs = _v; } // variable name -> register
inline List<String> CodeGenerator::_namedStack() { return get()->_namedStack; } // variables definitely assigned at the current point (stack-disciplined by conditional nesting)
inline void CodeGenerator::set__namedStack(List<String> _v) { get()->_namedStack = _v; } // variables definitely assigned at the current point (stack-disciplined by conditional nesting)
inline List<Boolean> CodeGenerator::_namedIsReg() { return get()->_namedIsReg; } // parallel to _namedStack: true if bound to a register by a NAME op, false if declared only via `locals.x =`
inline void CodeGenerator::set__namedIsReg(List<Boolean> _v) { get()->_namedIsReg = _v; } // parallel to _namedStack: true if bound to a register by a NAME op, false if declared only via `locals.x =`
inline String CodeGenerator::_localOnlyName() { return get()->_localOnlyName; } // while compiling the RHS of a first assignment, the variable being created ("" when inactive)
inline void CodeGenerator::set__localOnlyName(String _v) { get()->_localOnlyName = _v; } // while compiling the RHS of a first assignment, the variable being created ("" when inactive)
inline List<String> CodeGenerator::_breakNames() { return get()->_breakNames; }
inline void CodeGenerator::set__breakNames(List<String> _v) { get()->_breakNames = _v; }
inline List<Boolean> CodeGenerator::_breakIsReg() { return get()->_breakIsReg; }
inline void CodeGenerator::set__breakIsReg(List<Boolean> _v) { get()->_breakIsReg = _v; }
inline List<Int32> CodeGenerator::_breakStarts() { return get()->_breakStarts; } // per open loop: where its accumulator starts
inline void CodeGenerator::set__breakStarts(List<Int32> _v) { get()->_breakStarts = _v; } // per open loop: where its accumulator starts
inline List<Boolean> CodeGenerator::_breakSeen() { return get()->_breakSeen; } // per open loop: has any break contributed yet?
inline void CodeGenerator::set__breakSeen(List<Boolean> _v) { get()->_breakSeen = _v; } // per open loop: has any break contributed yet?
inline List<Int32> CodeGenerator::_loopNameMarks() { return get()->_loopNameMarks; } // per open loop: _namedStack depth on entry
inline void CodeGenerator::set__loopNameMarks(List<Int32> _v) { get()->_loopNameMarks = _v; } // per open loop: _namedStack depth on entry
inline Int32 CodeGenerator::_targetReg() { return get()->_targetReg; } // Target register for next expression (-1 = allocate)
inline void CodeGenerator::set__targetReg(Int32 _v) { get()->_targetReg = _v; } // Target register for next expression (-1 = allocate)
inline List<Int32> CodeGenerator::_loopExitLabels() { return get()->_loopExitLabels; } // Stack of loop exit labels for break
inline void CodeGenerator::set__loopExitLabels(List<Int32> _v) { get()->_loopExitLabels = _v; } // Stack of loop exit labels for break
inline List<Int32> CodeGenerator::_loopContinueLabels() { return get()->_loopContinueLabels; } // Stack of loop continue labels for continue
inline void CodeGenerator::set__loopContinueLabels(List<Int32> _v) { get()->_loopContinueLabels = _v; } // Stack of loop continue labels for continue
inline List<FuncDef> CodeGenerator::_functions() { return get()->_functions; } // Compile-time registry of all functions (for naming + disassembly)
inline void CodeGenerator::set__functions(List<FuncDef> _v) { get()->_functions = _v; } // Compile-time registry of all functions (for naming + disassembly)
inline Boolean CodeGenerator::_globalScope() { return get()->_globalScope; }
inline void CodeGenerator::set__globalScope(Boolean _v) { get()->_globalScope = _v; }
inline String CodeGenerator::FileName() { return get()->FileName; } // Source file name, copied to each compiled FuncDef
inline void CodeGenerator::set_FileName(String _v) { get()->FileName = _v; } // Source file name, copied to each compiled FuncDef
inline Value CodeGenerator::Error() { return get()->Error; }
inline void CodeGenerator::set_Error(Value _v) { get()->Error = _v; }
inline List<FuncDef> CodeGenerator::GetFunctions() { return get()->GetFunctions(); }
inline Int32 CodeGenerator::AllocReg() { return get()->AllocReg(); }
inline void CodeGenerator::FreeReg(Int32 reg) { return get()->FreeReg(reg); }
inline Int32 CodeGenerator::AllocConsecutiveRegs(Int32 count) { return get()->AllocConsecutiveRegs(count); }
inline Boolean CodeGenerator::IsLiveVariableReg(Int32 reg) { return get()->IsLiveVariableReg(reg); }
inline Int32 CodeGenerator::CompileInto(ASTNode node,Int32 targetReg) { return get()->CompileInto(node, targetReg); }
inline Int32 CodeGenerator::GetTargetOrAlloc() { return get()->GetTargetOrAlloc(); }
inline Int32 CodeGenerator::Compile(ASTNode ast) { return get()->Compile(ast); }
inline void CodeGenerator::ResetTempRegisters() { return get()->ResetTempRegisters(); }
inline void CodeGenerator::CompileBody(List<ASTNode> body) { return get()->CompileBody(body); }
inline void CodeGenerator::EmitDiscardCheck(ASTNode stmt,Int32 resultReg) { return get()->EmitDiscardCheck(stmt, resultReg); }
inline void CodeGenerator::CompileConditionalBody(List<ASTNode> body) { return get()->CompileConditionalBody(body); }
inline void CodeGenerator::MergeBranchNames(Int32 mark,List<String> thenNames,List<Boolean> thenIsReg,Boolean thenAbrupt,Boolean elseAbrupt) { return get()->MergeBranchNames(mark, thenNames, thenIsReg, thenAbrupt, elseAbrupt); }
inline Boolean CodeGenerator::EndsAbruptly(List<ASTNode> body) { return get()->EndsAbruptly(body); }
inline List<String> CodeGenerator::ReserveBodyVarRegs(List<ASTNode> body) { return get()->ReserveBodyVarRegs(body); }
inline void CodeGenerator::ReleaseBodyVarRegs(List<String> reserved) { return get()->ReleaseBodyVarRegs(reserved); }
inline Int32 CodeGenerator::TakeVarReg(String varName) { return get()->TakeVarReg(varName); }
inline void CodeGenerator::CollectAssignedVars(List<ASTNode> body,List<String> result) { return get()->CollectAssignedVars(body, result); }
inline List<String> CodeGenerator::HoistableBodyNames(List<ASTNode> body) { return get()->HoistableBodyNames(body); }
inline Boolean CodeGenerator::MayLeaveLoop(ASTNode node) { return get()->MayLeaveLoop(node); }
inline Boolean CodeGenerator::AnyLeavesLoop(List<ASTNode> nodes) { return get()->AnyLeavesLoop(nodes); }
inline Int32 CodeGenerator::EmitHoistedNames(List<String> names) { return get()->EmitHoistedNames(names); }
inline void CodeGenerator::PushName(String varName,Boolean isReg) { return get()->PushName(varName, isReg); }
inline void CodeGenerator::PopNamesTo(Int32 mark) { return get()->PopNamesTo(mark); }
inline Boolean CodeGenerator::IsRegisterNamed(String varName) { return get()->IsRegisterNamed(varName); }
inline Boolean CodeGenerator::IsDefinitelyAssigned(String varName) { return get()->IsDefinitelyAssigned(varName); }
inline void CodeGenerator::EnsureNamed(String varName,Int32 varReg) { return get()->EnsureNamed(varName, varReg); }
inline void CodeGenerator::NoteLocalsDeclared(String varName) { return get()->NoteLocalsDeclared(varName); }
inline void CodeGenerator::ClearLoopNames() { return get()->ClearLoopNames(); }
inline void CodeGenerator::BeginLoopNames() { return get()->BeginLoopNames(); }
inline void CodeGenerator::NoteBreak() { return get()->NoteBreak(); }
inline void CodeGenerator::EndLoopNames(Boolean alwaysRuns) { return get()->EndLoopNames(alwaysRuns); }
inline Boolean CodeGenerator::IsAlwaysTrue(ASTNode condition) { return get()->IsAlwaysTrue(condition); }
inline Int32 CodeGenerator::AddGlobalRef(String varName) { return get()->AddGlobalRef(varName); }
inline void CodeGenerator::BeginGlobalScope() { return get()->BeginGlobalScope(); }
inline void CodeGenerator::EmitGlobalStore(String varName,Int32 valueReg) { return get()->EmitGlobalStore(varName, valueReg); }
inline void CodeGenerator::EmitFreeLoad(Boolean addressOf,Int32 resultReg,String varName,String comment) { return get()->EmitFreeLoad(addressOf, resultReg, varName, comment); }
inline FuncDef CodeGenerator::CompileFunction(ASTNode ast,String funcName) { return get()->CompileFunction(ast, funcName); }
inline List<FuncDef> CodeGenerator::CompileImport(List<ASTNode> statements,String funcName) { return get()->CompileImport(statements, funcName); }
inline FuncDef CodeGenerator::CompileProgram(List<ASTNode> statements,String funcName) { return get()->CompileProgram(statements, funcName); }
inline Int32 CodeGenerator::Visit(NumberNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(StringNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::VisitIdentifier(IdentifierNode node,bool addressOf) { return get()->VisitIdentifier(node, addressOf); }
inline void CodeGenerator::EmitNamedLoad(Boolean addressOf,Int32 resultReg,Int32 srcReg,Value nameVal,String comment) { return get()->EmitNamedLoad(addressOf, resultReg, srcReg, nameVal, comment); }
inline Int32 CodeGenerator::Visit(IdentifierNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(AssignmentNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::VisitGlobalAssignment(AssignmentNode node) { return get()->VisitGlobalAssignment(node); }
inline Int32 CodeGenerator::Visit(IndexedAssignmentNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(UnaryOpNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(BinaryOpNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::CompileShortCircuit(BinaryOpNode node) { return get()->CompileShortCircuit(node); }
inline Int32 CodeGenerator::Visit(ComparisonChainNode node) { return get()->Visit(node); }
inline void CodeGenerator::EmitComparison(String op,Int32 destReg,Int32 leftReg,Int32 rightReg) { return get()->EmitComparison(op, destReg, leftReg, rightReg); }
inline Int32 CodeGenerator::Visit(CallNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::CompileUserCall(CallNode node,Int32 funcVarReg,Int32 explicitTarget) { return get()->CompileUserCall(node, funcVarReg, explicitTarget); }
inline List<Int32> CodeGenerator::CompileArguments(List<ASTNode> arguments) { return get()->CompileArguments(arguments); }
inline Int32 CodeGenerator::EmitCallSequence(Int32 funcReg,List<Int32> argRegs,Int32 explicitTarget,String comment) { return get()->EmitCallSequence(funcReg, argRegs, explicitTarget, comment); }
inline Int32 CodeGenerator::Visit(GroupNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(ListNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(MapNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(IndexNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::VisitIndex(IndexNode node,bool addressOf) { return get()->VisitIndex(node, addressOf); }
inline Int32 CodeGenerator::Visit(SliceNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(MemberNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::VisitMember(MemberNode node,bool addressOf) { return get()->VisitMember(node, addressOf); }
inline void CodeGenerator::EmitAccessOrInvoke(Int32 resultReg,Int32 targetReg,Int32 indexReg,bool addressOf,bool isDotAccess,ASTNode targetNode,String comment) { return get()->EmitAccessOrInvoke(resultReg, targetReg, indexReg, addressOf, isDotAccess, targetNode, comment); }
inline Int32 CodeGenerator::Visit(ExprCallNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(MethodCallNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(WhileNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(IfNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(ForNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(BreakNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(ContinueNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(FunctionNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::GetSelfReg() { return get()->GetSelfReg(); }
inline Int32 CodeGenerator::GetSuperReg() { return get()->GetSuperReg(); }
inline Boolean CodeGenerator::_scanUsesSelf() { return get()->_scanUsesSelf; }
inline void CodeGenerator::set__scanUsesSelf(Boolean _v) { get()->_scanUsesSelf = _v; }
inline Boolean CodeGenerator::_scanUsesSuper() { return get()->_scanUsesSuper; }
inline void CodeGenerator::set__scanUsesSuper(Boolean _v) { get()->_scanUsesSuper = _v; }
inline void CodeGenerator::ReserveSelfSuperRegs(List<ASTNode> body) { return get()->ReserveSelfSuperRegs(body); }
inline void CodeGenerator::ScanNodeList(List<ASTNode> nodes) { return get()->ScanNodeList(nodes); }
inline void CodeGenerator::ScanNode(ASTNode node) { return get()->ScanNode(node); }
inline Int32 CodeGenerator::Visit(SelfNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(SuperNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::Visit(ScopeNode node) { return get()->Visit(node); }
inline Int32 CodeGenerator::EmitMethodCall(Int32 receiverReg,String methodKey,List<ASTNode> arguments,bool preserveSelf) { return get()->EmitMethodCall(receiverReg, methodKey, arguments, preserveSelf); }
inline Int32 CodeGenerator::Visit(ReturnNode node) { return get()->Visit(node); }

} // end of namespace MiniScript
