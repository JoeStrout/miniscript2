// CodeGenerator.cs - Compiles AST nodes to bytecode using the visitor pattern
// Uses CodeEmitterBase to support both direct bytecode and assembly text output.

using System;
using System.Collections.Generic;
// H: #include "AST.g.h"
// H: #include "CodeEmitter.g.h"
// H: #include "ErrorTypes.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "CS_Math.h"

namespace MiniScript {


// Compiles AST nodes to bytecode
public class CodeGenerator : IASTVisitor {
	private CodeEmitterBase _emitter;
	private List<Boolean> _regInUse;    // Which registers are currently in use
	private Int32 _firstAvailable;      // Lowest index that might be free
	private Int32 _maxRegUsed;          // High water mark for register usage
	private Dictionary<String, Int32> _variableRegs;  // variable name -> register
	private List<String> _namedStack;   // variables definitely assigned at the current point (stack-disciplined by conditional nesting)
	private List<Boolean> _namedIsReg;  // parallel to _namedStack: true if bound to a register by a NAME op, false if declared only via `locals.x =`
	private String _localOnlyName;      // while compiling the RHS of a first assignment, the variable being created ("" when inactive)
	// Definite assignment at the `break`s of each open loop -- what survives past a
	// loop that only ends by breaking out of it.  See NoteBreak/EndLoopNames.  The
	// per-loop accumulators are stacked end to end in _breakNames/_breakIsReg, with
	// _breakStarts giving each one's first index; the innermost loop's is always the
	// tail, so a loop's entries can be dropped by truncating.
	private List<String> _breakNames;
	private List<Boolean> _breakIsReg;
	private List<Int32> _breakStarts;      // per open loop: where its accumulator starts
	private List<Boolean> _breakSeen;      // per open loop: has any break contributed yet?
	private List<Int32> _loopNameMarks;    // per open loop: _namedStack depth on entry
	private Int32 _targetReg;           // Target register for next expression (-1 = allocate)
	private List<Int32> _loopExitLabels;      // Stack of loop exit labels for break
	private List<Int32> _loopContinueLabels;  // Stack of loop continue labels for continue
	private List<FuncDef> _functions;          // Compile-time registry of all functions (for naming + disassembly)

	// True while compiling code whose named variables are GLOBALS rather than
	// registers -- that is, @main and only @main.  A module compiled for `import`
	// is a function returning its own locals, so its top-level names are locals
	// and this stays false there.  See notes/GLOBALS.md.
	//
	// At global scope a named variable is a slot in the Globals table, so it gets
	// no register and no NAME op.  Assignments compile to GSTORE and reads to
	// GLOADC/GLOADV, all of which name the variable through this function's
	// global-reference table rather than through a register or the constant pool.
	private Boolean _globalScope;

	public String FileName = "";               // Source file name, copied to each compiled FuncDef
	public Value Error;

	public CodeGenerator(CodeEmitterBase emitter) {
		_emitter = emitter;
		_regInUse = new List<Boolean>();
		_firstAvailable = 0;
		_maxRegUsed = -1;
		_variableRegs = new Dictionary<String, Int32>();
		_namedStack = new List<String>();
		_namedIsReg = new List<Boolean>();
		_localOnlyName = "";
		_breakNames = new List<String>();
		_breakIsReg = new List<Boolean>();
		_breakStarts = new List<Int32>();
		_breakSeen = new List<Boolean>();
		_loopNameMarks = new List<Int32>();
		_targetReg = -1;
		_loopExitLabels = new List<Int32>();
		_loopContinueLabels = new List<Int32>();
		_functions = new List<FuncDef>();
		_globalScope = false;
		Error = Value.Null;
	}

	// Get all compiled functions (index 0 = @main, 1+ = inner functions)
	public List<FuncDef> GetFunctions() {
		return _functions;
	}

	// Allocate a register
	private Int32 AllocReg() {
		// Scan from _firstAvailable to find first free register
		Int32 reg = _firstAvailable;
		while (reg < _regInUse.Count && _regInUse[reg]) {
			reg = reg + 1;
		}

		// Expand the list if needed
		while (_regInUse.Count <= reg) {
			_regInUse.Add(false);
		}

		// Mark register as in use
		_regInUse[reg] = true;

		// Update _firstAvailable to search from next position
		_firstAvailable = reg + 1;

		// Update high water mark
		if (reg > _maxRegUsed) _maxRegUsed = reg;

		_emitter.ReserveRegister(reg);
		return reg;
	}

	// Free a register so it can be reused
	private void FreeReg(Int32 reg) {
		if (reg < 0 || reg >= _regInUse.Count) return;

		_regInUse[reg] = false;

		// Update _firstAvailable if this register is lower
		if (reg < _firstAvailable) _firstAvailable = reg;

		// Update _maxRegUsed if we freed the highest register
		if (reg == _maxRegUsed) {
			// Search downward for the new maximum register in use
			_maxRegUsed = reg - 1;
			while (_maxRegUsed >= 0 && !_regInUse[_maxRegUsed]) {
				_maxRegUsed = _maxRegUsed - 1;
			}
		}
	}

	// Allocate a block of consecutive registers
	// Returns the first register of the block
	private Int32 AllocConsecutiveRegs(Int32 count) {
		if (count <= 0) return -1;
		if (count == 1) return AllocReg();

		// Find first position where 'count' consecutive registers are free
		Int32 startReg = _firstAvailable;
		while (true) {
			// Check if registers startReg through startReg+count-1 are all free
			Boolean allFree = true;
			for (Int32 i = 0; i < count; i++) {
				Int32 reg = startReg + i;
				if (reg < _regInUse.Count && _regInUse[reg]) {
					allFree = false;
					startReg = reg + 1;  // Skip past this in-use register
					break;
				}
			}
			if (allFree) break;
		}

		// Allocate all registers in the block
		for (Int32 i = 0; i < count; i++) {
			Int32 reg = startReg + i;
			// Expand the list if needed
			while (_regInUse.Count <= reg) {
				_regInUse.Add(false);
			}
			_regInUse[reg] = true;
			_emitter.ReserveRegister(reg);
			if (reg > _maxRegUsed) _maxRegUsed = reg;
		}

		// Update _firstAvailable
		_firstAvailable = startReg + count;

		return startReg;
	}

	// Is this register currently bound to a variable?
	//
	// LIST and MAP have to create their container before filling it, so unlike every
	// other Visit method they write their destination before reading their operands.
	// That is only safe if nothing the operands read lives in that register.  The
	// registers a nested expression can read are exactly those bound to variables:
	// directly (LOADC/LOADV rX, rVar), or by name at runtime (locals.x / globals.x,
	// which resolve through the frame's name table to the same register).  Anything
	// else it touches is a temp it allocated itself.
	//
	// This is deliberately register-based rather than name-based: it does not care
	// *why* the register might be read, so it covers the locals/globals route that no
	// name-matching walk can see.  MayReadVar answers a different question -- whether
	// the RHS names the variable, which is what decides NAME ordering on a first
	// assignment -- and neither subsumes the other.
	private Boolean IsLiveVariableReg(Int32 reg) {
		foreach (Int32 r in _variableRegs.Values) { // CPP: for (Int32 r : _variableRegs.GetValues()) {
			if (r == reg) return true;
		}
		return false;
	}

	// Compile an expression into a specific target register
	// The target register should already be allocated by the caller
	private Int32 CompileInto(ASTNode node, Int32 targetReg) {
		_targetReg = targetReg;
		Int32 result = node.Accept(this);
		_targetReg = -1;
		return result;
	}

	// Get target register if set, otherwise allocate a new one
	// IMPORTANT: Call this at the START of each Visit method, before any recursive calls
	private Int32 GetTargetOrAlloc() {
		Int32 target = _targetReg;
		_targetReg = -1;  // Clear immediately to avoid affecting nested calls
		if (target >= 0) return target;
		return AllocReg();
	}

	// Compile an expression, placing result in a newly allocated register
	// Returns the register number holding the result
	public Int32 Compile(ASTNode ast) {
		return ast.Accept(this);
	}

	// Reset temporary registers before compiling a new statement.
	// Keeps r0 and all variable registers; frees everything else.
	private void ResetTempRegisters() {
		_regInUse.Clear();
		_regInUse.Add(true);  // r0
		_firstAvailable = 1;
		foreach (Int32 reg in _variableRegs.Values) { // CPP: for (Int32 reg : _variableRegs.GetValues()) {
			while (_regInUse.Count <= reg) {
				_regInUse.Add(false);
			}
			_regInUse[reg] = true;
			if (reg >= _firstAvailable) _firstAvailable = reg + 1;
		}
	}

	// Compile a list of statements (a block body).
	// Resets temporary registers before each statement.
	private void CompileBody(List<ASTNode> body) {
		for (Int32 i = 0; i < body.Count; i++) {
			ResetTempRegisters();
			if (body[i].Line != 0) _emitter.CurrentLine = body[i].Line;
			Int32 resultReg = body[i].Accept(this);
			EmitDiscardCheck(body[i], resultReg);
		}
	}

	// After compiling a statement, guard against silently dropping an error.
	// A bare expression used as a statement throws its value away; if that value
	// is an error, nobody can ever catch it, so ERRCHK halts the program there
	// (with the discarding line in the stack trace).  Statements proper have no
	// meaningful result register, and are skipped.
	private void EmitDiscardCheck(ASTNode stmt, Int32 resultReg) {
		if (stmt.IsStatement() || resultReg < 0) return;
		_emitter.EmitA(Opcode.ERRCHK_rA, resultReg, "halt if result is an uncaught error");
	}

	// Compile a body of statements that may not execute (a loop body, or an if
	// with no else).  Nothing it assigns is definitely assigned afterward, so any
	// names recorded while compiling it are forgotten on exit (the body's names
	// sit at the tail of _namedStack, since any nested conditional bodies have
	// already been entered and exited).
	//
	// Visit(IfNode) does not use this: with both arms present it has to compare
	// them rather than discard both, so it does its own marking.
	private void CompileConditionalBody(List<ASTNode> body) {
		Int32 mark = _namedStack.Count;
		CompileBody(body);
		PopNamesTo(mark);
	}

	// Combine the two arms of an if/else into what is definitely assigned after it.
	// The "then" arm's names come in as thenNames/thenIsReg (already popped off the
	// stack, since the else arm must not see them -- it is an alternative path, not
	// a continuation).  The "else" arm's names are what sits above `mark` now.
	//
	// Normally that is the intersection: `if c then n = 1 else n = 0` leaves n
	// assigned, which matters because MiniScript has no ternary operator and this
	// is how you write one.  Both arms give the variable the same register --
	// _variableRegs is not unwound between them, so the else arm reuses what the
	// then arm allocated -- so an entry surviving the merge describes one register,
	// as EnsureNamed requires.  It survives as register-bound only if both arms
	// bound it; a NAME in one arm and a `locals.x =` in the other leaves the name
	// assigned but its register not established on every path.
	//
	// An arm that cannot complete normally (it returns, breaks, or continues) is
	// the exception: if control reaches the code after the if, that arm did not
	// fall through to it, so only the other arm's assignments matter and they hold
	// unconditionally.
	private void MergeBranchNames(Int32 mark, List<String> thenNames, List<Boolean> thenIsReg, Boolean thenAbrupt, Boolean elseAbrupt) {
		// Only one arm can reach past the if: keep exactly that arm's names.
		if (thenAbrupt && !elseAbrupt) return;  // the else arm's names are already in place
		if (elseAbrupt && !thenAbrupt) {
			PopNamesTo(mark);
			for (Int32 i = 0; i < thenNames.Count; i++) PushName(thenNames[i], thenIsReg[i]);
			return;
		}

		// Otherwise keep what both arms assigned.  (If both are abrupt nothing
		// after the if is reachable, so the intersection is as good as anything.)
		List<String> keep = new List<String>();
		List<Boolean> keepIsReg = new List<Boolean>();
		for (Int32 i = 0; i < thenNames.Count; i++) {
			for (Int32 j = mark; j < _namedStack.Count; j++) {
				if (_namedStack[j] != thenNames[i]) continue;
				keep.Add(thenNames[i]);
				keepIsReg.Add(thenIsReg[i] && _namedIsReg[j]);
				break;
			}
		}
		PopNamesTo(mark);
		for (Int32 i = 0; i < keep.Count; i++) PushName(keep[i], keepIsReg[i]);
	}

	// Can this body only be left by jumping somewhere else -- a return, or a break
	// or continue out of the enclosing loop?  Used by MergeBranchNames; a body that
	// ends this way never falls through to whatever follows it.
	private Boolean EndsAbruptly(List<ASTNode> body) {
		if (body.Count == 0) return false;
		ASTNode last = body[body.Count - 1];

		ReturnNode returnN = last as ReturnNode;
		if (returnN != null) return true;
		BreakNode breakN = last as BreakNode;
		if (breakN != null) return true;
		ContinueNode continueN = last as ContinueNode;
		if (continueN != null) return true;

		// A nested if leaves abruptly only if every way through it does.
		IfNode ifN = last as IfNode;
		if (ifN != null && ifN.ElseBody.Count > 0) {
			return EndsAbruptly(ifN.ThenBody) && EndsAbruptly(ifN.ElseBody);
		}
		return false;
	}

	// ── Loop-body variable registers ─────────────────────────────────────────
	//
	// Register allocation assumes control flows straight through: a temp freed at
	// the end of an expression is free for whatever comes next.  A loop breaks
	// that assumption, because its back edge re-runs the condition after the body
	// has been compiled -- so a variable first assigned in the body must not sit
	// anywhere the condition writes.  Two things the condition writes are easy to
	// miss: temps it frees before the body is compiled, and, for any call it makes,
	// the entire callee frame from `calleeBase` up (see EmitCallSequence).
	//
	// Rather than try to enumerate those after the fact, we give the body's new
	// variables their registers *before* compiling the condition.  Then they are
	// simply part of the live set the condition allocates around, and the ordinary
	// rules do the rest.
	//
	// The registers are parked under an internal key, not the variable's own name,
	// so that the variable is still "new" at its first assignment -- which is what
	// decides NAME ordering, and what makes `c = c + 1` in the body the error it
	// should be rather than a read of an unwritten register.
	//
	// A read that comes *before* the assignment does consult the reservation, since
	// it is the only way to compile one instruction that is right on every
	// iteration: see VisitIdentifier.  The reservation is what lets it name the
	// register the local is going to occupy.
	private static String PendingVarRegKey(String varName) {
		return "@pending " + varName;
	}

	// Reserve a register for each variable the given loop body creates.  Returns
	// the names reserved, for ReleaseBodyVarRegs.  Does nothing at global scope,
	// where top-level names are slots rather than registers.
	private List<String> ReserveBodyVarRegs(List<ASTNode> body) {
		List<String> reserved = new List<String>();
		if (_globalScope) return reserved;

		List<String> assigned = new List<String>();
		CollectAssignedVars(body, assigned);
		for (Int32 i = 0; i < assigned.Count; i++) {
			String name = assigned[i];
			Int32 existing;
			// Already has a register (a parameter, an earlier statement, or an
			// enclosing loop that reserved it), or we just reserved it ourselves.
			if (_variableRegs.TryGetValue(name, out existing)) continue;
			String key = PendingVarRegKey(name);
			if (_variableRegs.TryGetValue(key, out existing)) continue;
			_variableRegs[key] = AllocReg();
			reserved.Add(name);
		}
		return reserved;
	}

	// Drop any reservation the body did not end up claiming.  A claimed one has
	// already been renamed to the variable itself by TakeVarReg.
	private void ReleaseBodyVarRegs(List<String> reserved) {
		for (Int32 i = 0; i < reserved.Count; i++) {
			String key = PendingVarRegKey(reserved[i]);
			Int32 reg;
			if (!_variableRegs.TryGetValue(key, out reg)) continue;
			_variableRegs.Remove(key);
			FreeReg(reg);
		}
	}

	// Allocate the register for a variable being created.  If an enclosing loop
	// reserved one for it, take that; the caller records it under the variable's
	// own name, which is what retires the reservation.
	private Int32 TakeVarReg(String varName) {
		Int32 reg;
		String key = PendingVarRegKey(varName);
		if (_variableRegs.TryGetValue(key, out reg)) {
			_variableRegs.Remove(key);
			return reg;
		}
		return AllocReg();
	}

	// Collect the names of variables that assignments in this statement list
	// create.  Descends into nested loop and if bodies, since those assignments
	// happen in this scope too, but not into nested function bodies, whose
	// variables are locals of that function.
	private void CollectAssignedVars(List<ASTNode> body, List<String> result) {
		for (Int32 i = 0; i < body.Count; i++) {
			ASTNode node = body[i];

			AssignmentNode assignN = node as AssignmentNode;
			if (assignN != null) { result.Add(assignN.Variable); continue; }

			WhileNode whileN = node as WhileNode;
			if (whileN != null) { CollectAssignedVars(whileN.Body, result); continue; }

			IfNode ifN = node as IfNode;
			if (ifN != null) {
				CollectAssignedVars(ifN.ThenBody, result);
				CollectAssignedVars(ifN.ElseBody, result);
				continue;
			}

			ForNode forN = node as ForNode;
			if (forN != null) {
				result.Add(forN.Variable);
				CollectAssignedVars(forN.Body, result);
				continue;
			}
		}
	}

	// ── Hoisting a loop body's NAME ops ──────────────────────────────────────
	//
	// A variable first assigned inside a loop body has its NAME op emitted inside
	// the loop, where it re-runs every iteration even though the binding it
	// establishes never changes after the first.  The rotated loop (see
	// Visit(WhileNode)) has a preheader -- code that runs once, and only when the
	// body is going to run at least once -- which is the right home for it.
	//
	// Hoisting is sound only for a name the body assigns before anything could
	// observe it, because NAME is not bookkeeping: it sets names[base+reg], the
	// guard LOADC checks before trusting the register, so running it early turns a
	// read that should have raised Undefined Identifier (or reached an enclosing
	// scope) into a read of an unwritten register.  A candidate must therefore be
	//
	//   * assigned by a plain `x = ...` at the body's top level -- not inside a
	//     nested if or loop, where the assignment might not run at all;
	//   * unread by every statement up to and including that one;
	//   * out of reach of any `break` or `continue` ahead of it, which would carry
	//     control to code that can see the variable before it was assigned;
	//   * new here: no register yet, and not already definitely assigned.
	//
	// MayReadVar answers the read test, and the dynamic-scope case with it, since
	// ScopeNode always answers true.  It is the same test Visit(AssignmentNode) uses
	// to decide whether a NAME may precede its own RHS, and it draws the boundary in
	// the same place: a closure over this frame, called in between, could still
	// observe the variable, because defining a function reads nothing.  Hoisting
	// therefore inherits exactly the imprecision straight-line assignment already
	// has, and adds none of its own.
	private List<String> HoistableBodyNames(List<ASTNode> body) {
		List<String> result = new List<String>();
		// Top-level names are slots in the Globals table, not registers, and no NAME
		// is emitted for them.  See notes/GLOBALS.md.
		if (_globalScope) return result;

		for (Int32 i = 0; i < body.Count; i++) {
			// Past a break or continue nothing later qualifies: the code it jumps to
			// can read a variable whose assignment it skipped.
			if (i > 0 && MayLeaveLoop(body[i - 1])) return result;

			AssignmentNode assignN = body[i] as AssignmentNode;
			if (assignN == null) continue;
			String name = assignN.Variable;

			if (IsDefinitelyAssigned(name)) continue;
			Int32 existing;
			if (_variableRegs.TryGetValue(name, out existing)) continue;
			// Needs a register to name, which ReserveBodyVarRegs parked for it.
			if (!_variableRegs.TryGetValue(PendingVarRegKey(name), out existing)) continue;
			if (ContainsName(result, name)) continue;

			// Any read at or before the assignment disqualifies it.  Including the
			// statement itself covers a RHS that reads the name; that is a compile
			// error in its own right (see VisitIdentifier), but the test here does
			// not depend on that check catching it first.
			Boolean readEarlier = false;
			for (Int32 j = 0; j <= i; j++) {
				if (!body[j].MayReadVar(name)) continue;
				readEarlier = true;
				break;
			}
			if (readEarlier) continue;

			result.Add(name);
		}
		return result;
	}

	// Can this statement transfer control out of the enclosing loop body -- to the
	// code after the loop, or to the next iteration -- without running what follows
	// it?  A nested loop's own break and continue do not count: they bind to that
	// loop, so control still resumes here.  A `return` does not count either; it
	// leaves the function, so no later read can observe anything.
	private Boolean MayLeaveLoop(ASTNode node) {
		BreakNode breakN = node as BreakNode;
		if (breakN != null) return true;
		ContinueNode continueN = node as ContinueNode;
		if (continueN != null) return true;

		IfNode ifN = node as IfNode;
		if (ifN != null) return AnyLeavesLoop(ifN.ThenBody) || AnyLeavesLoop(ifN.ElseBody);

		return false;
	}

	private Boolean AnyLeavesLoop(List<ASTNode> nodes) {
		if (nodes == null) return false;
		for (Int32 i = 0; i < nodes.Count; i++) {
			if (MayLeaveLoop(nodes[i])) return true;
		}
		return false;
	}

	private static Boolean ContainsName(List<String> names, String name) {
		for (Int32 i = 0; i < names.Count; i++) {
			if (names[i] == name) return true;
		}
		return false;
	}

	// Emit the hoisted NAME ops into a loop preheader, and record the names as
	// definitely assigned so EnsureNamed skips them inside the body.  The caller
	// must pop back to the returned mark once the loop is closed: the preheader
	// runs only when the body does, so a zero-iteration loop leaves these names
	// undefined for the code that follows.
	private Int32 EmitHoistedNames(List<String> names) {
		Int32 mark = _namedStack.Count;
		for (Int32 i = 0; i < names.Count; i++) {
			Int32 reg;
			if (!_variableRegs.TryGetValue(PendingVarRegKey(names[i]), out reg)) continue;
			Int32 nameIdx = _emitter.AddConstant(Value.make_string(names[i]));
			_emitter.EmitAB(Opcode.NAME_rA_kBC, reg, nameIdx,
				$"use r{reg} for {names[i]} (hoisted)");
			PushName(names[i], true);
		}
		return mark;
	}

	// ── Definite assignment ──────────────────────────────────────────────────
	//
	// _namedStack holds the variables that are definitely assigned at the current
	// point -- assigned on every path that reaches here.  Two things read it:
	//
	//   * EnsureNamed, to skip a NAME op that a dominating one already covers.
	//     That only counts entries flagged in _namedIsReg, since only a NAME
	//     actually binds the name to a register.
	//   * The first-assignment check in Visit(AssignmentNode), which counts every
	//     entry: `locals.x = 1` creates x just as surely as an assignment does,
	//     it simply routes through the frame's variable map instead of a register.
	//
	// The stack discipline is what makes "definitely" true rather than "somewhere
	// earlier": a conditional body's entries are popped on exit (see
	// CompileConditionalBody), and an if/else keeps only what both arms assigned
	// (see MergeBranchNames).

	// Record that varName is definitely assigned from here on.  isReg says whether
	// a NAME op bound it to its register, or it was only declared through `locals`.
	private void PushName(String varName, Boolean isReg) {
		_namedStack.Add(varName);
		_namedIsReg.Add(isReg);
	}

	// Drop every entry above mark, undoing the assignments a non-dominating body made.
	private void PopNamesTo(Int32 mark) {
		while (_namedStack.Count > mark) {
			_namedStack.RemoveAt(_namedStack.Count - 1);
			_namedIsReg.RemoveAt(_namedIsReg.Count - 1);
		}
	}

	// Does a NAME op binding varName to its register already dominate this point?
	// Only such an entry counts: `locals.x = 1` makes x assigned without binding a
	// register, so it cannot stand in for the NAME.
	private Boolean IsRegisterNamed(String varName) {
		for (Int32 i = 0; i < _namedStack.Count; i++) {
			if (_namedStack[i] == varName && _namedIsReg[i]) return true;
		}
		return false;
	}

	// Is varName definitely assigned at this point, by any means?
	private Boolean IsDefinitelyAssigned(String varName) {
		for (Int32 i = 0; i < _namedStack.Count; i++) {
			if (_namedStack[i] == varName) return true;
		}
		return false;
	}

	// Ensure a NAME op has been emitted for the given variable on a path that
	// dominates the current point.  If not, emit one now and record it.  This is
	// what makes a variable "exist" at runtime; it must run on every path that
	// assigns the variable (e.g. both branches of a single-line if).
	private void EnsureNamed(String varName, Int32 varReg) {
		if (IsRegisterNamed(varName)) return;
		Int32 nameIdx = _emitter.AddConstant(Value.make_string(varName));
		_emitter.EmitAB(Opcode.NAME_rA_kBC, varReg, nameIdx, $"use r{varReg} for {varName}");
		PushName(varName, true);
	}

	// Record that `locals.x = ...` created x.  No register is bound, so this
	// counts for definite assignment but not for EnsureNamed.
	private void NoteLocalsDeclared(String varName) {
		if (IsDefinitelyAssigned(varName)) return;
		PushName(varName, false);
	}

	// ── Definite assignment across a loop ────────────────────────────────────
	//
	// A loop body normally contributes nothing, because it may run zero times.
	// `while true` (or any constant-true condition) is the exception worth
	// handling: the body always runs, and the only way to reach the code after the
	// loop is a `break`, so whatever is assigned at every break is assigned after
	// the loop.  This is not a nicety -- `while true` around an input prompt, with
	// a `break` once the input validates, is how the idiom is written:
	//
	//     while true
	//         power = input("how much? ").val
	//         if power <= limit then break
	//     end while
	//     power = power * factor      // power is assigned here
	//
	// Each open loop accumulates the intersection over the breaks seen so far.

	private void ClearLoopNames() {
		_breakNames.Clear();
		_breakIsReg.Clear();
		_breakStarts.Clear();
		_breakSeen.Clear();
		_loopNameMarks.Clear();
	}

	private void BeginLoopNames() {
		_breakStarts.Add(_breakNames.Count);
		_breakSeen.Add(false);
		_loopNameMarks.Add(_namedStack.Count);
	}

	// Fold the current definite-assignment state into the innermost loop's
	// accumulator.  Only entries above the loop's own mark count: anything below it
	// was already assigned before the loop and needs no help from us.
	private void NoteBreak() {
		if (_breakStarts.Count == 0) return;
		Int32 loop = _breakStarts.Count - 1;
		Int32 start = _breakStarts[loop];
		Int32 mark = _loopNameMarks[loop];

		if (!_breakSeen[loop]) {
			for (Int32 i = mark; i < _namedStack.Count; i++) {
				_breakNames.Add(_namedStack[i]);
				_breakIsReg.Add(_namedIsReg[i]);
			}
			_breakSeen[loop] = true;
			return;
		}

		// Later breaks narrow it: keep only what this path assigns too.
		List<String> keep = new List<String>();
		List<Boolean> keepIsReg = new List<Boolean>();
		for (Int32 i = start; i < _breakNames.Count; i++) {
			for (Int32 j = mark; j < _namedStack.Count; j++) {
				if (_namedStack[j] != _breakNames[i]) continue;
				keep.Add(_breakNames[i]);
				keepIsReg.Add(_breakIsReg[i] && _namedIsReg[j]);
				break;
			}
		}
		while (_breakNames.Count > start) {
			_breakNames.RemoveAt(_breakNames.Count - 1);
			_breakIsReg.RemoveAt(_breakIsReg.Count - 1);
		}
		for (Int32 i = 0; i < keep.Count; i++) {
			_breakNames.Add(keep[i]);
			_breakIsReg.Add(keepIsReg[i]);
		}
	}

	// Close the innermost loop, applying what its breaks established.  alwaysRuns
	// says the body is guaranteed to execute (a constant-true condition); without
	// that we cannot conclude anything, since the loop may run zero times.  A
	// constant-true loop with no break never falls through to the code after it, so
	// there is nothing to add there either.
	private void EndLoopNames(Boolean alwaysRuns) {
		Int32 loop = _breakStarts.Count - 1;
		Int32 start = _breakStarts[loop];

		if (alwaysRuns && _breakSeen[loop]) {
			for (Int32 i = start; i < _breakNames.Count; i++) {
				if (IsDefinitelyAssigned(_breakNames[i])) continue;
				PushName(_breakNames[i], _breakIsReg[i]);
			}
		}

		while (_breakNames.Count > start) {
			_breakNames.RemoveAt(_breakNames.Count - 1);
			_breakIsReg.RemoveAt(_breakIsReg.Count - 1);
		}
		_breakStarts.RemoveAt(loop);
		_breakSeen.RemoveAt(loop);
		_loopNameMarks.RemoveAt(loop);
	}

	// Does this loop condition always hold, so that the body is certain to run?
	// Only literals, judged by the same rule Value.BoolValue applies at run time:
	// a nonzero number, a nonempty string, a nonempty list or map, or the keyword
	// `true`.  `while true` is the form that matters; the rest come along because
	// the rule is "a literal we can evaluate here", not a special case for one
	// spelling.  Anything with a variable in it we decline to reason about, even
	// when it is obviously constant.
	private Boolean IsAlwaysTrue(ASTNode condition) {
		NumberNode num = condition as NumberNode;
		if (num != null) return num.Value != 0;
		StringNode str = condition as StringNode;
		if (str != null) return str.Value != "";
		ListNode list = condition as ListNode;
		if (list != null) return list.Elements.Count != 0;
		MapNode map = condition as MapNode;
		if (map != null) return map.Keys.Count != 0;
		IdentifierNode ident = condition as IdentifierNode;
		if (ident != null) return ident.Name == "true";
		return false;
	}

	// ── Global scope ─────────────────────────────────────────────────────────

	// Key under which an internal (non-user) register is parked in _variableRegs.
	// That dictionary doubles as the set of registers ResetTempRegisters must
	// preserve, and '@' cannot appear in an identifier, so no user variable can
	// ever collide with one of these.
	private static String LoopVarRegKey(String varName) {
		return "@loopvar " + varName;
	}

	// Intern a global name in this function's global-reference table.  The BC
	// operand of GLOADC/GLOADV/GSTORE is an index into that table, resolved to a
	// slot number the first time the function runs against a given namespace.
	private Int32 AddGlobalRef(String varName) {
		Int32 refIdx = _emitter.AddGlobalRef(Value.make_string(varName));
		if (refIdx > 65535 && Error.IsNull()) {
			Error = ErrorTypes.CompilerError("too many distinct global variables in one function", FileName, _emitter.CurrentLine);
		}
		return refIdx;
	}

	// Begin compiling top-level code.  Nothing to set up: at global scope a named
	// variable is a slot, reached by name through the reference table, so there is
	// no globals map to cache in a register and no register allocation to do.
	private void BeginGlobalScope() {
		_globalScope = true;
	}

	// Store a register into the named global.  Creates the global if it is new;
	// this is the only way a name comes into existence at top level.
	private void EmitGlobalStore(String varName, Int32 valueReg) {
		_emitter.EmitAB(Opcode.GSTORE_rA_iBC, valueReg, AddGlobalRef(varName),
			$"{varName} = r{valueReg}");
	}

	// Read a free variable -- one with no register in the function being compiled
	// -- into resultReg.  At global scope that is certainly a global.  Inside a
	// function it is usually a global too (an intrinsic, or a top-level name), but
	// it may be an enclosing local, so GLOADC/GLOADV check the frame before
	// reaching for the slot and fall back to the full run-time search if anything
	// could shadow it.  That check is at run time rather than here on purpose:
	// names can enter a scope dynamically -- `locals["x"] = 1`, a map handed to
	// another function, `import` -- so no amount of looking at the lexical chain
	// would be sound.  See notes/GLOBALS.md section 8, stage 5.
	private void EmitFreeLoad(Boolean addressOf, Int32 resultReg, String varName, String comment) {
		Opcode op = addressOf ? Opcode.GLOADV_rA_iBC : Opcode.GLOADC_rA_iBC;
		_emitter.EmitAB(op, resultReg, AddGlobalRef(varName), comment);
	}

	// Compile a complete function from a single expression/statement
	public FuncDef CompileFunction(ASTNode ast, String funcName) {
		_regInUse.Clear();
		_firstAvailable = 0;
		_maxRegUsed = -1;
		_variableRegs.Clear();
		_namedStack.Clear();
		_namedIsReg.Clear();
		ClearLoopNames();
		_globalScope = false;

		Int32 resultReg = ast.Accept(this);

		// Move result to r0 if not already there (and if there is a result)
		if (resultReg > 0) {
			_emitter.EmitABC(Opcode.LOAD_rA_rB, 0, resultReg, 0, "move Function result to r0");
		}
		_emitter.Emit(Opcode.RETURN, null);

		return _emitter.Finalize(funcName);
	}

	// Compile a module for import: like CompileProgram but appends LOCALS + RETURN
	// so the module returns its own top-level locals map as its result.
	// Returns all compiled functions (index 0 = @main, 1+ = inner functions).
	public List<FuncDef> CompileImport(List<ASTNode> statements, String funcName) {
		_regInUse.Clear();
		_firstAvailable = 0;
		_maxRegUsed = -1;
		_variableRegs.Clear();
		_namedStack.Clear();
		_namedIsReg.Clear();
		ClearLoopNames();
		// A module's top-level names are its LOCALS, not globals -- that is the
		// whole point of returning them as a map -- so this is not global scope.
		_globalScope = false;

		_functions.Clear();
		_functions.Add(null);

		for (Int32 i = 0; i < statements.Count; i++) {
			ResetTempRegisters();
			if (statements[i].Line != 0) _emitter.CurrentLine = statements[i].Line;
			Int32 resultReg = CompileInto(statements[i], 0);
			EmitDiscardCheck(statements[i], resultReg);
		}

		_emitter.EmitA(Opcode.LOCALS_rA, 0, "return locals");
		_emitter.Emit(Opcode.RETURN, null);

		FuncDef mainFunc = _emitter.Finalize(funcName);
		mainFunc.FileName = funcName;
		_functions[0] = mainFunc;
		return _functions;
	}

	// Compile a complete function from a list of statements (program)
	public FuncDef CompileProgram(List<ASTNode> statements, String funcName) {
		_regInUse.Clear();
		_firstAvailable = 0;
		_maxRegUsed = -1;
		_variableRegs.Clear();
		_namedStack.Clear();
		_namedIsReg.Clear();
		ClearLoopNames();

		// Reserve index 0 for @main
		_functions.Clear();
		_functions.Add(null);

		BeginGlobalScope();

		// Compile each statement, putting result into r0
		for (Int32 i = 0; i < statements.Count; i++) {
			ResetTempRegisters();
			if (statements[i].Line != 0) _emitter.CurrentLine = statements[i].Line;
			Int32 resultReg = CompileInto(statements[i], 0);
			EmitDiscardCheck(statements[i], resultReg);
		}

		_emitter.Emit(Opcode.RETURN, null);

		FuncDef mainFunc = _emitter.Finalize(funcName);
		mainFunc.FileName = FileName;
		_functions[0] = mainFunc;
		return mainFunc;
	}

	// --- Visit methods for each AST node type ---

	public Int32 Visit(NumberNode node) {
		Int32 reg = GetTargetOrAlloc();
		Double value = node.Value;

		// Check if value fits in signed 16-bit immediate
		if (value == Math.Floor(value) && value >= -32768 && value <= 32767) {
			_emitter.EmitAB(Opcode.LOAD_rA_iBC, reg, (Int32)value, $"r{reg} = {value}");
		} else {
			// Store in constants and load from there
			Int32 constIdx = _emitter.AddConstant(new Value(value));
			_emitter.EmitAB(Opcode.LOAD_rA_kBC, reg, constIdx, $"r{reg} = {value}");
		}
		return reg;
	}

	public Int32 Visit(StringNode node) {
		Int32 reg = GetTargetOrAlloc();
		Int32 constIdx = _emitter.AddConstant(Value.make_string(node.Value));
		_emitter.EmitAB(Opcode.LOAD_rA_kBC, reg, constIdx, $"r{reg} = \"{node.Value}\"");
		return reg;
	}

	private Int32 VisitIdentifier(IdentifierNode node, bool addressOf) {
		Int32 resultReg = GetTargetOrAlloc();

		// Handle built-in constants
		if (node.Name == "null") {
			_emitter.EmitA(Opcode.LOADNULL_rA, resultReg, $"r{resultReg} = null");
			return resultReg;
		}
		if (node.Name == "true") {
			_emitter.EmitAB(Opcode.LOAD_rA_iBC, resultReg, 1, $"r{resultReg} = true");
			return resultReg;
		}
		if (node.Name == "false") {
			_emitter.EmitAB(Opcode.LOAD_rA_iBC, resultReg, 0, $"r{resultReg} = false");
			return resultReg;
		}

		// Reading the variable that the assignment we are inside is creating.  The
		// local does not exist yet, so this can only mean the enclosing scope's
		// variable of the same name -- and on the next time through a loop it would
		// mean the local instead, which is nobody's intent.  MiniScript 1 warned
		// here and read the outer one; MS2 requires the intent to be written down.
		if (_localOnlyName != "" && node.Name == _localOnlyName) {
			if (Error.IsNull()) {
				Error = ErrorTypes.CompilerError(StringUtils.Format(
					"illegal assignment to unqualified local '{0}' based on nonlocal", node.Name),
					FileName, _emitter.CurrentLine);
			}
			return resultReg;
		}

		Int32 varReg;
		String at = addressOf ? "@" : "";
		if (_variableRegs.TryGetValue(node.Name, out varReg)) {
			// Variable found - emit LOADC (load-and-call for implicit function invocation)
			EmitNamedLoad(addressOf, resultReg, varReg, Value.make_string(node.Name),
				$"r{resultReg} = {at}{node.Name}");
		} else if (_variableRegs.TryGetValue(PendingVarRegKey(node.Name), out varReg)) {
			// The local does not exist yet, but an enclosing loop has reserved the
			// register it will live in (see ReserveBodyVarRegs), and this read sits
			// ahead of the assignment that creates it.  One instruction has to serve
			// both the iteration where the local does not exist and the ones after it
			// does, so read the reserved register the guarded way: the NAME op is what
			// makes the guard match, so until the assignment has run once this falls
			// back to the run-time search and finds the enclosing scope, and from then
			// on it finds the local that now shadows it.
			EmitNamedLoad(addressOf, resultReg, varReg, Value.make_string(node.Name),
				$"r{resultReg} = {at}{node.Name} (outer until assigned)");
		} else {
			// Variable has no register here: at global scope it is a slot, and
			// otherwise it may be an enclosing local, so the search happens at
			// runtime.  EmitFreeLoad picks between the two.
			EmitFreeLoad(addressOf, resultReg, node.Name,
				$"r{resultReg} = {at}{node.Name} (outer)");
		}

		return resultReg;
	}

	// Emit a LOADV (addressOf) or LOADC (auto-invoking) that copies R[srcReg]
	// into R[resultReg] after checking that srcReg is still named nameVal,
	// falling back to a runtime lookup by that name if it is not.
	//
	// The usual kC forms carry the name's constant-pool index in the 8-bit C
	// field, so they can only reach the first 256 constants.  Past that we emit
	// the rC forms instead: a LOAD_rA_kBC (whose constant index is 16-bit) puts
	// the name in a scratch register, and the opcode reads it from there.  The
	// two instructions are adjacent and the scratch register is freed right
	// after, so it costs nothing in the common case.
	private void EmitNamedLoad(Boolean addressOf, Int32 resultReg, Int32 srcReg, Value nameVal, String comment) {
		Int32 nameIdx = _emitter.AddConstant(nameVal);
		if (nameIdx <= 255) {
			Opcode op = addressOf ? Opcode.LOADV_rA_rB_kC : Opcode.LOADC_rA_rB_kC;
			_emitter.EmitABC(op, resultReg, srcReg, nameIdx, comment);
			return;
		}
		Opcode regOp = addressOf ? Opcode.LOADV_rA_rB_rC : Opcode.LOADC_rA_rB_rC;
		Int32 nameReg = AllocReg();
		_emitter.EmitAB(Opcode.LOAD_rA_kBC, nameReg, nameIdx,
			$"r{nameReg} = name for {comment}");
		_emitter.EmitABC(regOp, resultReg, srcReg, nameReg, comment);
		FreeReg(nameReg);
	}

	public Int32 Visit(IdentifierNode node) {
		return VisitIdentifier(node, false);
	}

	public Int32 Visit(AssignmentNode node) {
		if (_targetReg > 0) {
			if (Error.IsNull()) Error = ErrorTypes.CompilerError(StringUtils.Format("unexpected target register {0} in assignment", _targetReg), FileName, _emitter.CurrentLine);
		}

		if (_globalScope) return VisitGlobalAssignment(node);

		// Get or allocate register for this variable.
		Int32 varReg;
		Boolean isNew = !_variableRegs.TryGetValue(node.Variable, out varReg);
		if (isNew) {
			// Hmm.  Should we allocate a new register for this variable, or
			// just claim the target register as our storage?  I'm going to alloc
			// a new one for now, because I can't be sure the caller won't free
			// the target register when done.  But we should probably return to
			// this later and see if we can optimize it more.
			varReg = TakeVarReg(node.Variable);
		}

		// A first assignment whose RHS names the same variable is an error: see
		// VisitIdentifier, which reports it.  The test is definite assignment, not
		// isNew -- isNew asks whether the name has a register anywhere in this
		// function, which stays true after a conditional body even though the
		// assignment in it may never have run.  Arm _localOnlyName before compiling
		// the RHS and disarm it after; nested function bodies compile with their own
		// CodeGenerator, so it cannot leak into one.
		Boolean localOnly = !IsDefinitelyAssigned(node.Variable);

		// Creating a variable whose name the RHS may read through `locals`/`outer`/
		// `globals` is the one case where the NAME op has to wait: NAME is not a
		// passive binding, and letting it run first would show the RHS a half-created
		// variable.  The RHS then has to land in a temp so the copy can follow the
		// NAME (see below).  Bare reads of the variable do not get here -- they are
		// the error above -- so what remains is the dynamic-scope route, which
		// MayReadVar reports conservatively (ScopeNode always answers true).
		//
		// This is only about *creating* a variable.  Once it exists, the RHS should
		// read its register, so NAME stays ahead of the RHS and no temp is needed --
		// "x = x + 1" compiles straight into x's register.  Guarding the destination
		// against a RHS that overwrites it before reading its operands is a separate
		// concern, handled by IsLiveVariableReg in Visit(ListNode)/Visit(MapNode).
		Boolean useTemp = isNew && node.Value.MayReadVar(node.Variable);

		// Emit a NAME op unless one already dominates this point.  This must run
		// on every path that assigns the variable, including conditional branches
		// (e.g. the else clause of a single-line if), or the variable would be
		// undefined at runtime when only that path executes.
		if (!useTemp) EnsureNamed(node.Variable, varReg);
		// If the RHS is a function expression, note the current function count so we
		// can assign the variable name to the resulting FuncDef afterward.
		FunctionNode rhsFunc = node.Value as FunctionNode;
		Int32 funcIndexBeforeRHS = _functions.Count;

		String savedLocalOnly = _localOnlyName;
		if (localOnly) _localOnlyName = node.Variable;

		if (useTemp) {
			Int32 tempReg = AllocReg();
			Int32 rhsReg = CompileInto(node.Value, tempReg);
			_localOnlyName = savedLocalOnly;
			// NAME is not a passive binding: via MapToRegister it imports any existing
			// value for this name out of the frame's live LocalVarMap into the
			// register.  Copying the temp in afterwards overwrites that stale import
			// with the value we just computed, so the copy must follow the NAME.
			EnsureNamed(node.Variable, varReg);
			_emitter.EmitABC(Opcode.LOAD_rA_rB, varReg, rhsReg, 0, $"r{varReg} = r{rhsReg}");
			FreeReg(tempReg);
		} else {
			CompileInto(node.Value, varReg);  // get RHS directly into the variable's register
			_localOnlyName = savedLocalOnly;
		}

		// The variable exists from here on, so nested expressions in later statements
		// may read its register directly.
		if (isNew) _variableRegs[node.Variable] = varReg;

		// If the RHS was a function expression, give that FuncDef the variable name.
		if (rhsFunc != null && funcIndexBeforeRHS < _functions.Count) {
			FuncDef rhsFuncDef = _functions[funcIndexBeforeRHS];
			if (rhsFuncDef != null) rhsFuncDef.Name = node.Variable;
		}

		// Note that we don't FreeReg(varReg) here, as we need this register to
		// continue to serve as the storage for this variable for the life of
		// the function.  (TODO: or until some lifetime analysis determines that
		// the variable will never be read again.)

		// BUT, if varReg is not the explicit target register, then we need to copy
		// the value there as well.  Not sure why that would ever be the case (since
		// assignment can't be used in an expression in MiniScript).  So:
		return varReg;
	}

	//
	// Assignment at top level: evaluate the right-hand side into a temp, then
	// store it into the named global.
	//
	// None of the register bookkeeping in the local case applies here.  There is
	// no NAME op, because the variable is not a register.  There is no
	// first-assignment special case either: the slot is not written until the RHS
	// has been evaluated, so `n = n + 1` creating a global reads the enclosing
	// scope (or fails as undefined) exactly the way it should, with no temp needed
	// to order things.
	//
	private Int32 VisitGlobalAssignment(AssignmentNode node) {
		// If the RHS is a function expression, note the current function count so
		// we can name the resulting FuncDef afterward.
		FunctionNode rhsFunc = node.Value as FunctionNode;
		Int32 funcIndexBeforeRHS = _functions.Count;

		Int32 valueReg = AllocReg();
		CompileInto(node.Value, valueReg);

		if (rhsFunc != null && funcIndexBeforeRHS < _functions.Count) {
			FuncDef rhsFuncDef = _functions[funcIndexBeforeRHS];
			if (rhsFuncDef != null) rhsFuncDef.Name = node.Variable;
		}

		EmitGlobalStore(node.Variable, valueReg);
		return valueReg;
	}

	public Int32 Visit(IndexedAssignmentNode node) {
		Int32 containerReg = node.Target.Accept(this);
		Int32 indexReg = node.Index.Accept(this);

		// If the RHS is a function expression, note the current function count so we
		// can assign a name to the resulting FuncDef afterward.
		FunctionNode rhsFunc = node.Value as FunctionNode;
		Int32 funcIndexBeforeRHS = _functions.Count;

		Int32 valueReg = node.Value.Accept(this);

		// If the RHS was a function expression, give it the LHS name.
		if (rhsFunc != null && funcIndexBeforeRHS < _functions.Count) {
			FuncDef rhsFuncDef = _functions[funcIndexBeforeRHS];
			if (rhsFuncDef != null && node.LHSName != null) {
				rhsFuncDef.Name = node.LHSName;
			}
		}

		_emitter.EmitABC(Opcode.IDXSET_rA_rB_rC, containerReg, indexReg, valueReg,
			$"{node.Target.ToStr()}[{node.Index.ToStr()}] = {node.Value.ToStr()}");

		// `locals.x = ...` creates local x as surely as `x = ...` does, so a later
		// `x = x + 1` is reading a variable that exists.  Record it (no register is
		// bound, hence the false flag; the read still goes through the frame's
		// variable map).  Only for a constant name: `locals[expr]` names nothing we
		// can know here.  At global scope `locals` is the globals table, which has
		// no register-based naming at all.
		if (!_globalScope) {
			ScopeNode scopeTarget = node.Target as ScopeNode;
			StringNode nameIndex = node.Index as StringNode;
			if (scopeTarget != null && nameIndex != null && scopeTarget.Scope == ScopeType.Locals) {
				NoteLocalsDeclared(nameIndex.Value);
			}
		}

		FreeReg(valueReg);
		FreeReg(indexReg);
		return containerReg;
	}

	public Int32 Visit(UnaryOpNode node) {
		if (node.Op == Op.ADDRESS_OF) {
			// Special case: lookup without function call (address-of)
			var ident = node.Operand as IdentifierNode;
			if (ident != null) return VisitIdentifier(ident, true);
			var member = node.Operand as MemberNode;
			if (member != null) return VisitMember(member, true);
			var index = node.Operand as IndexNode;
			if (index != null) return VisitIndex(index, true);
			// On anything else, @ has no effect.
			return node.Operand.Accept(this);
		}
			
		Int32 resultReg = GetTargetOrAlloc();  // Capture target before any recursive calls

		Int32 operandReg = node.Operand.Accept(this);

		if (node.Op == Op.MINUS) {
			// Negate: result = 0 - operand
			Int32 zeroReg = AllocReg();
			_emitter.EmitAB(Opcode.LOAD_rA_iBC, zeroReg, 0, "r{zeroReg} = 0 (for negation)");
			_emitter.EmitABC(Opcode.SUB_rA_rB_rC, resultReg, zeroReg, operandReg, $"r{resultReg} = -{node.Operand.ToStr()}");
			FreeReg(zeroReg);
			FreeReg(operandReg);
			return resultReg;
		} else if (node.Op == Op.NOT) {
			// Fuzzy logic NOT: 1 - AbsClamp01(operand)
			_emitter.EmitABC(Opcode.NOT_rA_rB, resultReg, operandReg, 0, $"not {node.Operand.ToStr()}");
			FreeReg(operandReg);
			return resultReg;
		} else if (node.Op == Op.NEW) {
			// new: create a map with __isa set to the operand
			_emitter.EmitABC(Opcode.NEW_rA_rB, resultReg, operandReg, 0, $"new {node.Operand.ToStr()}");
			FreeReg(operandReg);
			return resultReg;
		}

		// Unknown unary operator - move operand to result if needed
		if (Error.IsNull()) Error = ErrorTypes.CompilerError("unknown unary operator", FileName, _emitter.CurrentLine);
		if (operandReg != resultReg) {
			_emitter.EmitABC(Opcode.LOAD_rA_rB, resultReg, operandReg, 0, "move to target");
			FreeReg(operandReg);
		}
		return resultReg;
	}

	public Int32 Visit(BinaryOpNode node) {
		// 'and'/'or' use short-circuit evaluation: the right operand is not
		// evaluated if the left operand alone determines the result.
		if (node.Op == Op.AND || node.Op == Op.OR) {
			return CompileShortCircuit(node);
		}

		Int32 resultReg = GetTargetOrAlloc();  // Capture target before any recursive calls
		Int32 leftReg = node.Left.Accept(this);
		Int32 rightReg = node.Right.Accept(this);

		Opcode op = Opcode.NOOP;
		String opSymbol = "?";

		if (node.Op == Op.PLUS) {
			op = Opcode.ADD_rA_rB_rC;
			opSymbol = "+";
		} else if (node.Op == Op.MINUS) {
			op = Opcode.SUB_rA_rB_rC;
			opSymbol = "-";
		} else if (node.Op == Op.TIMES) {
			op = Opcode.MUL_rA_rB_rC;
			opSymbol = "*";
		} else if (node.Op == Op.DIVIDE) {
			op = Opcode.DIV_rA_rB_rC;
			opSymbol = "/";
		} else if (node.Op == Op.MOD) {
			op = Opcode.MOD_rA_rB_rC;
			opSymbol = "%";
		} else if (node.Op == Op.LESS_THAN) {
			op = Opcode.LT_rA_rB_rC;
			opSymbol = "<";
		} else if (node.Op == Op.LESS_EQUAL) {
			op = Opcode.LE_rA_rB_rC;
			opSymbol = "<=";
		} else if (node.Op == Op.EQUALS) {
			op = Opcode.EQ_rA_rB_rC;
			opSymbol = "==";
		} else if (node.Op == Op.NOT_EQUAL) {
			op = Opcode.NE_rA_rB_rC;
			opSymbol = "!=";
		} else if (node.Op == Op.GREATER_THAN) {
			// a > b is same as b < a
			op = Opcode.LT_rA_rB_rC;
			opSymbol = ">";
			// Swap operands
			Int32 temp = leftReg;
			leftReg = rightReg;
			rightReg = temp;
		} else if (node.Op == Op.GREATER_EQUAL) {
			// a >= b is same as b <= a
			op = Opcode.LE_rA_rB_rC;
			opSymbol = ">=";
			// Swap operands
			Int32 temp = leftReg;
			leftReg = rightReg;
			rightReg = temp;
		} else if (node.Op == Op.POWER) {
			op = Opcode.POW_rA_rB_rC;
			opSymbol = "^";
		} else if (node.Op == Op.ISA) {
			op = Opcode.ISA_rA_rB_rC;
			opSymbol = "isa";
		}

		if (op != Opcode.NOOP) {
			_emitter.EmitABC(op, resultReg, leftReg, rightReg,
				$"r{resultReg} = {node.Left.ToStr()} {opSymbol} {node.Right.ToStr()}");
		}

		FreeReg(rightReg);
		FreeReg(leftReg);
		return resultReg;
	}

	// Compile 'and'/'or' with short-circuit evaluation.  The right operand is
	// only evaluated when the left operand does not already determine the
	// result.  An error operand never short-circuits the truthiness test
	// (which would throw); BRERR peels it off first so the surviving
	// BRTRUE/BRFALSE only ever sees a non-error value.
	private Int32 CompileShortCircuit(BinaryOpNode node) {
		Boolean isAnd = (node.Op == Op.AND);
		Int32 resultReg = GetTargetOrAlloc();
		Int32 doneLabel = _emitter.CreateLabel();

		Int32 leftReg = node.Left.Accept(this);

		if (isAnd) {
			// 'and': error left -> result is the error; false left -> result 0;
			// true left -> evaluate right and combine with the fuzzy AND op.
			Int32 errLabel = _emitter.CreateLabel();
			Int32 zeroLabel = _emitter.CreateLabel();
			_emitter.EmitBranch(Opcode.BRERR_rA_iBC, leftReg, errLabel, "short-circuit 'and': left is error");
			_emitter.EmitBranch(Opcode.BRFALSE_rA_iBC, leftReg, zeroLabel, "short-circuit 'and': left is false");

			Int32 rightReg = node.Right.Accept(this);
			_emitter.EmitABC(Opcode.AND_rA_rB_rC, resultReg, leftReg, rightReg,
				$"r{resultReg} = {node.Left.ToStr()} and {node.Right.ToStr()}");
			FreeReg(rightReg);
			_emitter.EmitJump(Opcode.JUMP_iABC, doneLabel, "skip short-circuit value");

			_emitter.PlaceLabel(errLabel);
			_emitter.EmitABC(Opcode.LOAD_rA_rB, resultReg, leftReg, 0,
				$"r{resultReg} = {node.Left.ToStr()} (error short-circuit)");
			_emitter.EmitJump(Opcode.JUMP_iABC, doneLabel, "skip short-circuit value");

			_emitter.PlaceLabel(zeroLabel);
			_emitter.EmitAB(Opcode.LOAD_rA_iBC, resultReg, 0, $"r{resultReg} = 0 (short-circuit)");
		} else {
			// 'or': error left -> evaluate right (the fuzzy OR op returns the
			// right operand); a fully-true left -> result 1; otherwise evaluate
			// right and combine with the fuzzy OR op.  The result is 1 only when
			// the left operand's fuzzy value is >= 1 (a partial value like 0.5
			// must NOT short-circuit, since 0.5 or x is genuinely fuzzy).  We
			// test that by negating: not(left) is false exactly when left is
			// fully true, which reduces the question to a plain BRFALSE.
			Int32 oneLabel = _emitter.CreateLabel();
			Int32 evalLabel = _emitter.CreateLabel();
			_emitter.EmitBranch(Opcode.BRERR_rA_iBC, leftReg, evalLabel, "short-circuit 'or': left is error");
			Int32 notReg = AllocReg();
			_emitter.EmitABC(Opcode.NOT_rA_rB, notReg, leftReg, 0, $"r{notReg} = not {node.Left.ToStr()}");
			_emitter.EmitBranch(Opcode.BRFALSE_rA_iBC, notReg, oneLabel, "short-circuit 'or': left is fully true");
			FreeReg(notReg);

			_emitter.PlaceLabel(evalLabel);
			Int32 rightReg = node.Right.Accept(this);
			_emitter.EmitABC(Opcode.OR_rA_rB_rC, resultReg, leftReg, rightReg,
				$"r{resultReg} = {node.Left.ToStr()} or {node.Right.ToStr()}");
			FreeReg(rightReg);
			_emitter.EmitJump(Opcode.JUMP_iABC, doneLabel, "skip short-circuit value");

			_emitter.PlaceLabel(oneLabel);
			_emitter.EmitAB(Opcode.LOAD_rA_iBC, resultReg, 1, $"r{resultReg} = 1 (short-circuit)");
		}

		FreeReg(leftReg);
		_emitter.PlaceLabel(doneLabel);
		return resultReg;
	}

	public Int32 Visit(ComparisonChainNode node) {
		Int32 resultReg = GetTargetOrAlloc();

		// Evaluate ALL operands first (each exactly once)
		List<Int32> valueRegs = new List<Int32>();
		for (Int32 i = 0; i < node.Operands.Count; i++) {
			valueRegs.Add(node.Operands[i].Accept(this));
		}

		// First comparison → resultReg
		EmitComparison(node.Operators[0], resultReg, valueRegs[0], valueRegs[1]);

		// Each subsequent comparison: compute into tempReg, multiply with resultReg
		for (Int32 i = 1; i < node.Operators.Count; i++) {
			Int32 tempReg = AllocReg();
			EmitComparison(node.Operators[i], tempReg, valueRegs[i], valueRegs[i + 1]);
			_emitter.EmitABC(Opcode.MUL_rA_rB_rC, resultReg, resultReg, tempReg, "chain AND");
			FreeReg(tempReg);
		}

		// Free operand registers
		for (Int32 i = 0; i < valueRegs.Count; i++) {
			FreeReg(valueRegs[i]);
		}

		return resultReg;
	}

	// Emit a single comparison opcode into destReg
	private void EmitComparison(String op, Int32 destReg, Int32 leftReg, Int32 rightReg) {
		if (op == Op.LESS_THAN) {
			_emitter.EmitABC(Opcode.LT_rA_rB_rC, destReg, leftReg, rightReg, "chain <");
		} else if (op == Op.LESS_EQUAL) {
			_emitter.EmitABC(Opcode.LE_rA_rB_rC, destReg, leftReg, rightReg, "chain <=");
		} else if (op == Op.GREATER_THAN) {
			_emitter.EmitABC(Opcode.LT_rA_rB_rC, destReg, rightReg, leftReg, "chain >");
		} else if (op == Op.GREATER_EQUAL) {
			_emitter.EmitABC(Opcode.LE_rA_rB_rC, destReg, rightReg, leftReg, "chain >=");
		} else if (op == Op.EQUALS) {
			_emitter.EmitABC(Opcode.EQ_rA_rB_rC, destReg, leftReg, rightReg, "chain ==");
		} else if (op == Op.NOT_EQUAL) {
			_emitter.EmitABC(Opcode.NE_rA_rB_rC, destReg, leftReg, rightReg, "chain !=");
		}
	}

	public Int32 Visit(CallNode node) {
		// Capture target register if one was specified (don't allocate yet)
		Int32 explicitTarget = _targetReg;
		_targetReg = -1;

		// Check if the function is a known local variable
		Int32 funcVarReg;
		if (_variableRegs.TryGetValue(node.Function, out funcVarReg)) {
			// Known local: ARGBLK + ARGs + CALL_rA_rB_rC
			return CompileUserCall(node, funcVarReg, explicitTarget);
		}

		// Not a known local — fetch the funcref by name, without auto-invoking it
		// (so, the LOADV/GLOADV side of the pair rather than LOADC/GLOADC).
		Int32 funcReg = AllocReg();
		EmitFreeLoad(true, funcReg, node.Function,
			$"r{funcReg} = @{node.Function} (runtime lookup)");

		Int32 result = CompileUserCall(node, funcReg, explicitTarget);
		FreeReg(funcReg);
		return result;
	}

	// Compile a call to a user-defined function (funcref in a register)
	private Int32 CompileUserCall(CallNode node, Int32 funcVarReg, Int32 explicitTarget) {
		List<Int32> argRegs = CompileArguments(node.Arguments);
		return EmitCallSequence(funcVarReg, argRegs, explicitTarget, $"call {node.Function}");
	}

	// Compile argument expressions into temporary registers.
	private List<Int32> CompileArguments(List<ASTNode> arguments) {
		List<Int32> argRegs = new List<Int32>();
		for (Int32 i = 0; i < arguments.Count; i++) {
			argRegs.Add(arguments[i].Accept(this));
		}
		return argRegs;
	}

	// Emit ARGBLK + ARG instructions, compute callee frame, emit CALL, and free
	// the argument registers.  Returns the result register.
	private Int32 EmitCallSequence(Int32 funcReg, List<Int32> argRegs, Int32 explicitTarget, String comment) {
		Int32 argCount = argRegs.Count;

		// Emit ARGBLK + ARG instructions
		_emitter.EmitABC(Opcode.ARGBLK_iABC, 0, 0, argCount, $"argblock {argCount}");
		for (Int32 i = 0; i < argCount; i++) {
			_emitter.EmitA(Opcode.ARG_rA, argRegs[i], $"arg {i}");
		}

		// Determine callee frame base (past all our used registers)
		Int32 calleeBase = _maxRegUsed + 1;
		_emitter.ReserveRegister(calleeBase);

		// Determine result register
		Int32 resultReg = (explicitTarget >= 0) ? explicitTarget : AllocReg();

		// Emit CALL: result in rA, callee frame at rB, funcref in rC
		_emitter.EmitABC(Opcode.CALL_rA_rB_rC, resultReg, calleeBase, funcReg,
			$"{comment}, result to r{resultReg}");

		// Free argument registers
		for (Int32 i = 0; i < argCount; i++) {
			FreeReg(argRegs[i]);
		}

		return resultReg;
	}

	public Int32 Visit(GroupNode node) {
		// Groups just delegate to their inner expression
		return node.Expression.Accept(this);
	}

	public Int32 Visit(ListNode node) {
		Int32 listReg = GetTargetOrAlloc();

		// LIST writes its destination before the elements are evaluated, so if the
		// destination holds a live variable, an element that reads that variable would
		// see the new (empty) list instead -- e.g. "x = [x]" would build a list
		// containing itself.  Build in a temp and copy at the end.
		Int32 buildReg = listReg;
		if (IsLiveVariableReg(listReg)) buildReg = AllocReg();

		// Create a list with the given number of elements
		Int32 count = node.Elements.Count;
		_emitter.EmitAB(Opcode.LIST_rA_iBC, buildReg, count, $"r{buildReg} = new list[{count}]");

		// Push each element onto the list
		for (Int32 i = 0; i < count; i++) {
			Int32 elemReg = node.Elements[i].Accept(this);
			_emitter.EmitABC(Opcode.PUSH_rA_rB, buildReg, elemReg, 0, $"push element {i} onto r{buildReg}");
			FreeReg(elemReg);
		}

		if (buildReg != listReg) {
			_emitter.EmitABC(Opcode.LOAD_rA_rB, listReg, buildReg, 0, $"r{listReg} = r{buildReg}");
			FreeReg(buildReg);
		}

		return listReg;
	}

	public Int32 Visit(MapNode node) {
		Int32 mapReg = GetTargetOrAlloc();

		// As in Visit(ListNode): MAP writes its destination before the keys and values
		// are evaluated, so build in a temp when the destination is live.
		Int32 buildReg = mapReg;
		if (IsLiveVariableReg(mapReg)) buildReg = AllocReg();

		// Create a map
		Int32 count = node.Keys.Count;
		_emitter.EmitAB(Opcode.MAP_rA_iBC, buildReg, count, $"new map[{count}]");

		// Set each key-value pair
		for (Int32 i = 0; i < count; i++) {
			Int32 keyReg = node.Keys[i].Accept(this);
			Int32 valueReg = node.Values[i].Accept(this);
			_emitter.EmitABC(Opcode.IDXSET_rA_rB_rC, buildReg, keyReg, valueReg, $"map[{node.Keys[i].ToStr()}] = {node.Values[i].ToStr()}");
			FreeReg(valueReg);
			FreeReg(keyReg);
		}

		if (buildReg != mapReg) {
			_emitter.EmitABC(Opcode.LOAD_rA_rB, mapReg, buildReg, 0, $"r{mapReg} = r{buildReg}");
			FreeReg(buildReg);
		}

		return mapReg;
	}

	public Int32 Visit(IndexNode node) {
		return VisitIndex(node, false);
	}

	// Compile index access, optionally as address-of (no auto-invoke)
	private Int32 VisitIndex(IndexNode node, bool addressOf) {
		Int32 resultReg = GetTargetOrAlloc();  // Capture target before any recursive calls
		Int32 targetReg = node.Target.Accept(this);
		Int32 indexReg = node.Index.Accept(this);
		String comment = $"{node.Target.ToStr()}[{node.Index.ToStr()}]";

		EmitAccessOrInvoke(resultReg, targetReg, indexReg, addressOf, false, node.Target, comment);

		FreeReg(indexReg);
		FreeReg(targetReg);
		return resultReg;
	}

	public Int32 Visit(SliceNode node) {
		Int32 resultReg = GetTargetOrAlloc();
		Int32 containerReg = node.Target.Accept(this);

		// Allocate two consecutive registers for start and end indices
		Int32 startReg = AllocConsecutiveRegs(2);
		Int32 endReg = startReg + 1;

		if (node.StartIndex != null) {
			CompileInto(node.StartIndex, startReg);
		} else {
			_emitter.EmitA(Opcode.LOADNULL_rA, startReg, $"r{startReg} = null (slice start)");
		}

		if (node.EndIndex != null) {
			CompileInto(node.EndIndex, endReg);
		} else {
			_emitter.EmitA(Opcode.LOADNULL_rA, endReg, $"r{endReg} = null (slice end)");
		}

		_emitter.EmitABC(Opcode.SLICE_rA_rB_rC, resultReg, containerReg, startReg,
			$"r{resultReg} = {node.Target.ToStr()}[{node.ToStr()}]");

		FreeReg(endReg);
		FreeReg(startReg);
		FreeReg(containerReg);
		return resultReg;
	}

	public Int32 Visit(MemberNode node) {
		return VisitMember(node, false);
	}

	// Compile member access, optionally as address-of (no auto-invoke)
	private Int32 VisitMember(MemberNode node, bool addressOf) {
		Int32 resultReg = GetTargetOrAlloc();
		Int32 targetReg = node.Target.Accept(this);
		Int32 indexReg = AllocReg();
		Int32 constIdx = _emitter.AddConstant(Value.make_string(node.Member));
		_emitter.EmitAB(Opcode.LOAD_rA_kBC, indexReg, constIdx, $"r{indexReg} = \"{node.Member}\"");
		String comment = $"{node.Target.ToStr()}.{node.Member}";

		EmitAccessOrInvoke(resultReg, targetReg, indexReg, addressOf, true, node.Target, comment);

		FreeReg(indexReg);
		FreeReg(targetReg);
		return resultReg;
	}

	// Shared tail for VisitIndex/VisitMember: emit INDEX (address-of),
	// IDXGET (bracket access, no auto-invoke), or
	// METHFIND + optional SETSELF + CALLIFREF (dot access with auto-invoke).
	private void EmitAccessOrInvoke(Int32 resultReg, Int32 targetReg, Int32 indexReg, bool addressOf, bool isDotAccess, ASTNode targetNode, String comment) {
		if (addressOf) {
			_emitter.EmitABC(Opcode.INDEX_rA_rB_rC, resultReg, targetReg, indexReg,
				$"@{comment}");
		} else if (isDotAccess) {
			_emitter.EmitABC(Opcode.METHFIND_rA_rB_rC, resultReg, targetReg, indexReg, comment);
			SuperNode superTarget = targetNode as SuperNode;
			if (superTarget != null) {
				Int32 selfReg = GetSelfReg();
				_emitter.EmitA(Opcode.SETSELF_rA, selfReg, $"preserve self for super access");
			}
			_emitter.EmitA(Opcode.CALLIFREF_rA, resultReg, $"auto-invoke if funcref");
		} else {
			// Bracket access: look up value (with type-map fallback) but never auto-invoke a funcRef.
			_emitter.EmitABC(Opcode.IDXGET_rA_rB_rC, resultReg, targetReg, indexReg, comment);
		}
	}

	public Int32 Visit(ExprCallNode node) {
		// Capture and clear _targetReg up front, so that evaluating the
		// receiver/function expression doesn't accidentally consume it.
		Int32 explicitTarget = _targetReg;
		_targetReg = -1;

		// Check if the function expression is a member access or index operation
		// on a map — if so, this is a method call and we need to set self/super.
		MemberNode memberTarget = node.Function as MemberNode;
		if (memberTarget != null) {
			// obj.method() via ExprCallNode — treat as method call
			SuperNode superTarget = memberTarget.Target as SuperNode;
			bool preserveSelf = (superTarget != null);
			Int32 receiverReg = memberTarget.Target.Accept(this);
			_targetReg = explicitTarget;  // restore for EmitMethodCall
			Int32 resultReg = EmitMethodCall(receiverReg, memberTarget.Member, node.Arguments, preserveSelf);
			FreeReg(receiverReg);
			return resultReg;
		}

		IndexNode indexTarget = node.Function as IndexNode;
		if (indexTarget != null) {
			// obj[key]() — treat as method call if key is a string
			SuperNode superTarget = indexTarget.Target as SuperNode;
			bool preserveSelf = (superTarget != null);

			// Evaluate receiver and key
			Int32 receiverReg = indexTarget.Target.Accept(this);
			Int32 keyReg = indexTarget.Index.Accept(this);

			// Compile arguments before the method lookup, so that a METHFIND
			// emitted while evaluating an argument can't clobber the pending
			// self/super context for this call.
			List<Int32> argRegs = CompileArguments(node.Arguments);

			// Use METHFIND instead of INDEX
			Int32 funcReg = AllocReg();
			_emitter.EmitABC(Opcode.METHFIND_rA_rB_rC, funcReg, receiverReg, keyReg,
				$"r{funcReg} = method lookup");

			// For super[key]() calls, preserve self
			if (preserveSelf) {
				Int32 selfReg = GetSelfReg();
				_emitter.EmitA(Opcode.SETSELF_rA, selfReg, $"preserve self for super call");
			}

			FreeReg(keyReg);
			Int32 resultReg = EmitCallSequence(funcReg, argRegs, explicitTarget, "method call via index");
			FreeReg(funcReg);
			FreeReg(receiverReg);
			return resultReg;
		}

		// Regular function call (not a method call)
		// Evaluate the function expression to get the funcref
		Int32 funcReg2 = node.Function.Accept(this);

		List<Int32> argRegs2 = CompileArguments(node.Arguments);
		Int32 resultReg2 = EmitCallSequence(funcReg2, argRegs2, explicitTarget, "call expr");
		FreeReg(funcReg2);

		return resultReg2;
	}

	public Int32 Visit(MethodCallNode node) {
		// Capture and clear _targetReg up front, so that evaluating the
		// receiver doesn't accidentally consume it.
		Int32 explicitTarget = _targetReg;
		_targetReg = -1;

		// Check if the target is 'super' — if so, preserve current self
		SuperNode superTarget = node.Target as SuperNode;
		bool preserveSelf = (superTarget != null);

		// Evaluate receiver
		Int32 receiverReg = node.Target.Accept(this);

		// Restore _targetReg so EmitMethodCall places the result there
		_targetReg = explicitTarget;

		// Emit method call
		Int32 resultReg = EmitMethodCall(receiverReg, node.Method, node.Arguments, preserveSelf);

		FreeReg(receiverReg);
		return resultReg;
	}

	public Int32 Visit(WhileNode node) {
		// The loop is rotated: the condition is tested once on the way in, and again
		// at the bottom of the body, so the steady state costs one branch per
		// iteration instead of a branch plus an unconditional jump.
		//
		//       [evaluate condition]     <-- preheader copy
		//       BRFALSE condReg, afterLoop
		//       [hoisted NAME ops]
		//   top:
		//       [body statements]
		//   continueTarget:
		//       [evaluate condition]     <-- back-edge copy
		//       BRTRUE condReg, top
		//   afterLoop:
		//
		// The condition runs the same number of times as in the straight-line
		// layout, just relocated, so side effects in it are unaffected.  What the
		// rotation buys beyond the branch is the preheader: code that runs once, and
		// only when the body is going to run at least once.  That is where a NAME op
		// for a variable the body creates belongs -- see HoistableBodyNames.
		//
		// Compiling the condition twice is also more accurate than compiling it
		// once.  The back-edge copy sees the body's variables, so a condition
		// mentioning one reads the local it now is, while the preheader copy still
		// reaches the enclosing scope -- which is what each point actually means.
		Boolean alwaysRuns = IsAlwaysTrue(node.Condition);

		Int32 top = _emitter.CreateLabel();
		Int32 continueTarget = _emitter.CreateLabel();
		Int32 afterLoop = _emitter.CreateLabel();

		// `continue` goes to the back-edge test, not to the top of the body: it means
		// "next iteration", and the test decides whether there is one.
		_loopExitLabels.Add(afterLoop);
		_loopContinueLabels.Add(continueTarget);
		BeginLoopNames();

		// Give the body's new variables their registers before compiling the
		// condition, so the condition allocates around them rather than on top of
		// them; the back edge below re-runs the condition after the body.  See
		// ReserveBodyVarRegs.
		List<String> reserved = ReserveBodyVarRegs(node.Body);

		// Preheader: the entry test.  A constant-true condition needs none -- the
		// body always runs -- and emitting one would only add dead code.
		if (!alwaysRuns) {
			Int32 condReg = node.Condition.Accept(this);
			_emitter.EmitBranch(Opcode.BRFALSE_rA_iBC, condReg, afterLoop, "skip loop if false");
			FreeReg(condReg);
		}

		// Still in the preheader, and past the entry test: the body runs from here.
		Int32 nameMark = EmitHoistedNames(HoistableBodyNames(node.Body));

		_emitter.PlaceLabel(top);
		CompileConditionalBody(node.Body);

		// Back edge.  The body's registers are still reserved, so the test allocates
		// around them the way the preheader copy did.
		_emitter.PlaceLabel(continueTarget);
		ResetTempRegisters();
		if (node.Condition.Line != 0) _emitter.CurrentLine = node.Condition.Line;
		if (alwaysRuns) {
			_emitter.EmitJump(Opcode.JUMP_iABC, top, "loop back");
		} else {
			Int32 backReg = node.Condition.Accept(this);
			_emitter.EmitBranch(Opcode.BRTRUE_rA_iBC, backReg, top, "loop back if true");
			FreeReg(backReg);
		}

		ReleaseBodyVarRegs(reserved);
		_emitter.PlaceLabel(afterLoop);

		// The hoisted names live only for the duration of the loop: reaching here by
		// way of the entry test means the preheader's NAME ops never ran.  Dropping
		// them before EndLoopNames leaves it free to restore any that every `break`
		// agreed on, which is sound because a break implies the body ran.
		PopNamesTo(nameMark);

		_loopExitLabels.RemoveAt(_loopExitLabels.Count - 1);
		_loopContinueLabels.RemoveAt(_loopContinueLabels.Count - 1);
		EndLoopNames(alwaysRuns);

		// While loops don't produce a value
		return -1;
	}

	public Int32 Visit(IfNode node) {
		// If statement generates:
		//       [evaluate condition]
		//       BRFALSE condReg, elseLabel (or afterIf if no else)
		//       [then body]
		//       JUMP afterIf
		//   elseLabel:
		//       [else body]
		//   afterIf:

		Int32 afterIf = _emitter.CreateLabel();
		Int32 elseLabel = (node.ElseBody.Count > 0) ? _emitter.CreateLabel() : afterIf;

		// Evaluate condition
		Int32 condReg = node.Condition.Accept(this);

		// Branch to else (or afterIf) if condition is false
		_emitter.EmitBranch(Opcode.BRFALSE_rA_iBC, condReg, elseLabel, "if condition false, jump to else");
		FreeReg(condReg);

		// Compile "then" body.  With no else there is a path around it, so its
		// assignments are forgotten; with an else, MergeBranchNames decides.
		if (node.ElseBody.Count == 0) {
			CompileConditionalBody(node.ThenBody);
		} else {
			Int32 mark = _namedStack.Count;
			CompileBody(node.ThenBody);

			// Take the then arm's names off the stack before compiling the else arm:
			// the else arm is an alternative path, not a continuation, so nothing the
			// then arm assigned is in scope for it.
			List<String> thenNames = new List<String>();
			List<Boolean> thenIsReg = new List<Boolean>();
			for (Int32 i = mark; i < _namedStack.Count; i++) {
				thenNames.Add(_namedStack[i]);
				thenIsReg.Add(_namedIsReg[i]);
			}
			PopNamesTo(mark);

			_emitter.EmitJump(Opcode.JUMP_iABC, afterIf, "jump past else");

			// Place else label
			_emitter.PlaceLabel(elseLabel);

			// Compile "else" body
			CompileBody(node.ElseBody);

			MergeBranchNames(mark, thenNames, thenIsReg,
				EndsAbruptly(node.ThenBody), EndsAbruptly(node.ElseBody));
		}

		// Place afterIf label
		_emitter.PlaceLabel(afterIf);

		// If statements don't produce a value
		return -1;
	}

	public Int32 Visit(ForNode node) {
		// For loop generates (using NEXT opcode for performance):
		//   [evaluate iterable into listReg]
		//   indexReg = -1  (NEXT increments before checking)
		// loopStart:
		//   NEXT indexReg, listReg  (indexReg++; if indexReg >= len(listReg) skip next)
		//   JUMP afterLoop          (skipped unless we've reached the end)
		//   varReg = listReg[indexReg]
		//   [body statements]
		//   JUMP loopStart
		// afterLoop:
		//
		// At function scope the first NEXT/ITERGET is peeled off into a preheader, so
		// that the loop variable's NAME has somewhere to sit that runs once and only
		// when there is a first element:
		//
		//   NEXT indexReg, listReg
		//   JUMP afterLoop          (zero iterations: the variable never comes to be)
		//   varReg = listReg[indexReg]
		//   NAME varReg, "v"
		//   JUMP bodyStart
		// loopStart:
		//   ... as above ...
		// bodyStart:
		//   [body statements]
		//
		// The NAME has to dominate the body, because the body is the only place the
		// loop variable is assigned; but it must not run when the loop iterates zero
		// times, because NAME is what makes a variable exist -- emitting it ahead of
		// the loop outright left `for j in []` with a defined `j` where MiniScript 1
		// raises Undefined Identifier (bugs.md entry 6).  Peeling satisfies both, at
		// four instructions of code size and nothing per iteration.
		//
		// Global scope keeps the plain layout: top-level names are slots in the
		// Globals table, no NAME is emitted for them, and the zero-iteration case is
		// already right.

		Int32 loopStart = _emitter.CreateLabel();
		Int32 bodyStart = _emitter.CreateLabel();
		Int32 afterLoop = _emitter.CreateLabel();

		// Push labels for break and continue statements
		_loopExitLabels.Add(afterLoop);
		_loopContinueLabels.Add(loopStart);
		BeginLoopNames();

		// Evaluate iterable expression
		Int32 listReg = node.Iterable.Accept(this);

		// Allocate hidden index register (starts at -1; NEXT will increment to 0)
		Int32 indexReg = AllocReg();
		_emitter.EmitAB(Opcode.LOAD_rA_iBC, indexReg, -1, "for loop index = -1");

		// Register listReg and indexReg as internal variables so that
		// ResetTempRegisters (called before each body statement) won't free them.
		String idxName = "__" + node.Variable + "_idx";
		String listName = "__" + node.Variable + "_list";
		_variableRegs[idxName] = indexReg;
		_variableRegs[listName] = listReg;

		// Get or create register for loop variable.  At global scope the loop
		// variable is a global like any other top-level name, so the register is
		// only where ITERGET drops each element on its way to the slot; it is
		// parked under an internal key so the body's ResetTempRegisters leaves it
		// alone, and it is never findable by the user's name.
		Int32 varReg;
		if (_globalScope) {
			varReg = AllocReg();
			_variableRegs[LoopVarRegKey(node.Variable)] = varReg;
		} else if (_variableRegs.TryGetValue(node.Variable, out varReg)) {
			// Variable already exists
		} else {
			varReg = TakeVarReg(node.Variable);
			_variableRegs[node.Variable] = varReg;
		}
		// Peel the first iteration's NEXT/ITERGET when the loop variable still needs
		// a NAME, so the NAME lands on a path taken only when the body will run.  If
		// a NAME already dominates -- the variable existed before the loop -- there
		// is nothing to place and the plain layout is smaller.
		Boolean peelFirst = !_globalScope && !IsRegisterNamed(node.Variable);
		if (peelFirst) {
			_emitter.EmitABC(Opcode.NEXT_rA_rB, indexReg, listReg, 0, "index++; skip next if done");
			_emitter.EmitJump(Opcode.JUMP_iABC, afterLoop, "no iterations: leave the loop variable undefined");
			_emitter.EmitABC(Opcode.ITERGET_rA_rB_rC, varReg, listReg, indexReg,
				$"{node.Variable} = iterget(container, index)");
			EnsureNamed(node.Variable, varReg);
			_emitter.EmitJump(Opcode.JUMP_iABC, bodyStart, "enter the body");
		}

		// Place loopStart label
		_emitter.PlaceLabel(loopStart);

		// NEXT: increment index, skip next if done
		_emitter.EmitABC(Opcode.NEXT_rA_rB, indexReg, listReg, 0, "index++; skip next if done");
		_emitter.EmitJump(Opcode.JUMP_iABC, afterLoop, "exit loop");

		// Get current element by position: varReg = iterget(listReg, indexReg)
		// For lists/strings this is the same as INDEX; for maps it returns {"key":k, "value":v}
		_emitter.EmitABC(Opcode.ITERGET_rA_rB_rC, varReg, listReg, indexReg, $"{node.Variable} = iterget(container, index)");

		// At global scope, publish the element as a global before running the body.
		if (_globalScope) EmitGlobalStore(node.Variable, varReg);

		// Compile body statements
		_emitter.PlaceLabel(bodyStart);
		CompileConditionalBody(node.Body);

		// Jump back to loopStart
		_emitter.EmitJump(Opcode.JUMP_iABC, loopStart, "loop back");

		// Place afterLoop label
		_emitter.PlaceLabel(afterLoop);

		// Pop labels.  A `for` may iterate zero times, so its breaks establish
		// nothing about the code after it.
		_loopExitLabels.RemoveAt(_loopExitLabels.Count - 1);
		_loopContinueLabels.RemoveAt(_loopContinueLabels.Count - 1);
		EndLoopNames(false);

		// Remove internal variable names and free the registers
		_variableRegs.Remove(idxName);
		_variableRegs.Remove(listName);
		if (_globalScope) {
			_variableRegs.Remove(LoopVarRegKey(node.Variable));
			FreeReg(varReg);
		}
		FreeReg(indexReg);
		FreeReg(listReg);

		// For loops don't produce a value
		return -1;
	}

	public Int32 Visit(BreakNode node) {
		// Break jumps to the innermost loop's exit label
		if (_loopExitLabels.Count == 0) {
			if (Error.IsNull()) Error = ErrorTypes.CompilerError("'break' without open loop block", FileName, _emitter.CurrentLine);
			_emitter.Emit(Opcode.NOOP, "break outside loop (error)");
		} else {
			Int32 exitLabel = _loopExitLabels[_loopExitLabels.Count - 1];
			_emitter.EmitJump(Opcode.JUMP_iABC, exitLabel, "break");
			NoteBreak();  // this path reaches the code after the loop; record what it assigns
		}
		return -1;
	}

	public Int32 Visit(ContinueNode node) {
		// Continue jumps to the innermost loop's continue label (loop start)
		if (_loopContinueLabels.Count == 0) {
			if (Error.IsNull()) Error = ErrorTypes.CompilerError("'continue' without open loop block", FileName, _emitter.CurrentLine);
			_emitter.Emit(Opcode.NOOP, "continue outside loop (error)");
		} else {
			Int32 continueLabel = _loopContinueLabels[_loopContinueLabels.Count - 1];
			_emitter.EmitJump(Opcode.JUMP_iABC, continueLabel, "continue");
		}
		return -1;
	}

	// Try to evaluate an AST node as a compile-time constant value.
	// Returns true if successful, with the result in 'result'.
	// Handles: numbers, strings, null/true/false, unary minus, list/map literals.
	// Lists and maps are automatically frozen (immutable).
	public static Boolean TryEvaluateConstant(ASTNode node, out Value result) {
		result = Value.Null;
		NumberNode numNode = node as NumberNode;
		if (numNode != null) {
			result = new Value(numNode.Value);
			return true;
		}
		StringNode strNode = node as StringNode;
		if (strNode != null) {
			result = Value.make_string(strNode.Value);
			return true;
		}
		IdentifierNode idNode = node as IdentifierNode;
		if (idNode != null) {
			if (idNode.Name == "null") { result = Value.Null; return true; }
			if (idNode.Name == "true") { result = new Value(1); return true; }
			if (idNode.Name == "false") { result = new Value(0); return true; }
			return false;
		}
		UnaryOpNode unaryNode = node as UnaryOpNode;
		if (unaryNode != null && unaryNode.Op == Op.MINUS) {
			NumberNode innerNum = unaryNode.Operand as NumberNode;
			if (innerNum != null) {
				result = new Value(-innerNum.Value);
				return true;
			}
			return false;
		}
		Value list;
		Value elemVal;
		ListNode listNode = node as ListNode;
		if (listNode != null) {
			list = Value.make_list(listNode.Elements.Count);
			for (Int32 i = 0; i < listNode.Elements.Count; i++) {
				if (!TryEvaluateConstant(listNode.Elements[i], out elemVal)) return false;
				list.Push(elemVal);
			}
			list.Freeze();
			result = list;
			return true;
		}
		Value map;
		Value keyVal;
		Value valVal;
		MapNode mapNode = node as MapNode;
		if (mapNode != null) {
			map = Value.make_map(mapNode.Keys.Count);
			for (Int32 i = 0; i < mapNode.Keys.Count; i++) {
				if (!TryEvaluateConstant(mapNode.Keys[i], out keyVal)) return false;
				if (!TryEvaluateConstant(mapNode.Values[i], out valVal)) return false;
				map.MapSet(keyVal, valVal);
			}
			map.Freeze();
			result = map;
			return true;
		}
		return false;
	}

	public Int32 Visit(FunctionNode node) {
		Int32 resultReg = GetTargetOrAlloc();

		// Reserve an index for this function in the shared list
		Int32 funcIndex = _functions.Count;
		_functions.Add(null);  // placeholder

		// Create a new CodeGenerator for the inner function
		BytecodeEmitter innerEmitter = new BytecodeEmitter();
		CodeGenerator innerGen = new CodeGenerator(innerEmitter);
		innerGen._functions = _functions;  // share the function registry
		innerGen.FileName = FileName;      // share the source file name

		// Reserve r0 for return value, then set up param registers (r1, r2, ...)
		innerGen.AllocReg();  // r0 reserved for return value
		for (Int32 i = 0; i < node.ParamNames.Count; i++) {
			Int32 paramReg = innerGen.AllocReg();  // r1, r2, ...
			String name = node.ParamNames[i];
			innerGen._variableRegs[name] = paramReg;
			Int32 nameIdx = innerEmitter.AddConstant(Value.make_string(name));
			innerEmitter.EmitAB(Opcode.NAME_rA_kBC, paramReg, nameIdx, $"param {name}");
			// Params are named unconditionally at function entry, so reassigning
			// one in the body needn't re-emit NAME.
			innerGen.PushName(name, true);
		}

		// Check for a docstring: if the first body statement is a string literal,
		// store it as the Note and skip compiling it (it's a no-op at runtime).
		String noteText = "";
		List<ASTNode> bodyToCompile = node.Body;
		if (node.Body.Count > 0) {
			StringNode firstStmt = node.Body[0].Simplify() as StringNode;
			if (firstStmt != null) {
				noteText = firstStmt.Value;
				bodyToCompile.RemoveAt(0);
			}
		}

		// Reserve self/super registers before compiling the body, so they
		// can't collide with recycled temporary registers.
		innerGen.ReserveSelfSuperRegs(bodyToCompile);

		// Compile the function body
		innerGen.CompileBody(bodyToCompile);
		if (Error.IsNull() && !innerGen.Error.IsNull()) Error = innerGen.Error;

		// Emit implicit RETURN at end of body
		innerEmitter.Emit(Opcode.RETURN, null);

		// Finalize the inner function
		String funcName = StringUtils.Format("@f{0}", funcIndex);
		FuncDef funcDef = innerEmitter.Finalize(funcName);

		// Set the note (docstring) and file name
		funcDef.Note = noteText;
		funcDef.FileName = FileName;

		// Set parameter info on the FuncDef
		Value defaultVal;
		for (Int32 i = 0; i < node.ParamNames.Count; i++) {
			funcDef.ParamNames.Add(Value.make_string(node.ParamNames[i]));
			ASTNode defaultNode = node.ParamDefaults[i];
			if (defaultNode != null) {
				if (TryEvaluateConstant(defaultNode, out defaultVal)) {
					funcDef.ParamDefaults.Add(defaultVal);
				} else {
					if (Error.IsNull()) Error = ErrorTypes.CompilerError(StringUtils.Format("Default value for parameter '{0}' must be a constant", node.ParamNames[i]), FileName, _emitter.CurrentLine);
					funcDef.ParamDefaults.Add(Value.Null);
				}
			} else {
				funcDef.ParamDefaults.Add(Value.Null);
			}
		}

		// If the inner function uses self/super, ensure the outer function also
		// allocates those registers so ApplyPendingContext can populate them
		// and FUNCREF can capture them for the closure.
		if (funcDef.SelfReg >= 0) GetSelfReg();
		if (funcDef.SuperReg >= 0) GetSuperReg();

		// Store in the compile-time function registry
		_functions[funcIndex] = funcDef;

		// Store a template funcref (no captured outer vars) in this function's
		// constant pool, and emit FUNCREF to bind it into a closure at runtime.
		Value funcTemplate = Value.make_funcref(funcDef, Value.Null);
		Int32 templateConst = _emitter.AddConstant(funcTemplate);
		_emitter.EmitAB(Opcode.FUNCREF_iA_iBC, resultReg, templateConst,
			$"r{resultReg} = funcref {funcName}");

		return resultReg;
	}

	// Allocate (or retrieve) the register for 'self'
	private Int32 GetSelfReg() {
		FuncDef fd = _emitter.PendingFunc;
		if (fd.SelfReg >= 0) return fd.SelfReg;
		Int32 reg = AllocReg();
		_variableRegs["self"] = reg;
		// Don't emit NAME here — ApplyPendingContext sets the name at runtime
		// when called as a method. If not called as a method, the name stays
		// empty so LOADV/LOADC will fall through to outer scope lookup (closures).
		fd.SelfReg = (Int16)reg;
		return reg;
	}

	// Allocate (or retrieve) the register for 'super'
	private Int32 GetSuperReg() {
		FuncDef fd = _emitter.PendingFunc;
		if (fd.SuperReg >= 0) return fd.SuperReg;
		Int32 reg = AllocReg();
		_variableRegs["super"] = reg;
		// Don't emit NAME here — see GetSelfReg comment.
		fd.SuperReg = (Int16)reg;
		return reg;
	}

	// Pre-scan a function body to reserve the self/super registers up front,
	// before any temporary registers are allocated.  The VM populates these
	// registers with method-call context at function entry, so if they were
	// allocated lazily (on first reference) they could land on a slot already
	// used and freed as a temp — and a later temp would clobber the context.
	// Does NOT descend into nested function bodies: a self/super reference
	// inside an inner function needs a register in that function, not this one.
	private Boolean _scanUsesSelf;
	private Boolean _scanUsesSuper;

	private void ReserveSelfSuperRegs(List<ASTNode> body) {
		_scanUsesSelf = false;
		_scanUsesSuper = false;
		ScanNodeList(body);
		if (_scanUsesSelf) GetSelfReg();
		if (_scanUsesSuper) GetSuperReg();
	}

	private void ScanNodeList(List<ASTNode> nodes) {
		for (Int32 i = 0; i < nodes.Count; i++) {
			ScanNode(nodes[i]);
		}
	}

	private void ScanNode(ASTNode node) {
		if (node == null) return;
		if (_scanUsesSuper) return;  // already found everything worth finding

		SuperNode superN = node as SuperNode;
		if (superN != null) {
			// A super reference also needs the self register (SETSELF preserves
			// the current self across the super call).
			_scanUsesSelf = true;
			_scanUsesSuper = true;
			return;
		}
		SelfNode selfN = node as SelfNode;
		if (selfN != null) { _scanUsesSelf = true; return; }

		// Do not descend into nested function definitions.
		FunctionNode funcN = node as FunctionNode;
		if (funcN != null) return;

		AssignmentNode assignN = node as AssignmentNode;
		if (assignN != null) { ScanNode(assignN.Value); return; }

		IndexedAssignmentNode idxAssignN = node as IndexedAssignmentNode;
		if (idxAssignN != null) {
			ScanNode(idxAssignN.Target);
			ScanNode(idxAssignN.Index);
			ScanNode(idxAssignN.Value);
			return;
		}

		UnaryOpNode unaryN = node as UnaryOpNode;
		if (unaryN != null) { ScanNode(unaryN.Operand); return; }

		BinaryOpNode binN = node as BinaryOpNode;
		if (binN != null) { ScanNode(binN.Left); ScanNode(binN.Right); return; }

		ComparisonChainNode cmpN = node as ComparisonChainNode;
		if (cmpN != null) { ScanNodeList(cmpN.Operands); return; }

		CallNode callN = node as CallNode;
		if (callN != null) { ScanNodeList(callN.Arguments); return; }

		GroupNode groupN = node as GroupNode;
		if (groupN != null) { ScanNode(groupN.Expression); return; }

		ListNode listN = node as ListNode;
		if (listN != null) { ScanNodeList(listN.Elements); return; }

		MapNode mapN = node as MapNode;
		if (mapN != null) { ScanNodeList(mapN.Keys); ScanNodeList(mapN.Values); return; }

		IndexNode indexN = node as IndexNode;
		if (indexN != null) { ScanNode(indexN.Target); ScanNode(indexN.Index); return; }

		SliceNode sliceN = node as SliceNode;
		if (sliceN != null) {
			ScanNode(sliceN.Target);
			ScanNode(sliceN.StartIndex);
			ScanNode(sliceN.EndIndex);
			return;
		}

		MemberNode memberN = node as MemberNode;
		if (memberN != null) { ScanNode(memberN.Target); return; }

		MethodCallNode methN = node as MethodCallNode;
		if (methN != null) { ScanNode(methN.Target); ScanNodeList(methN.Arguments); return; }

		ExprCallNode exprCallN = node as ExprCallNode;
		if (exprCallN != null) { ScanNode(exprCallN.Function); ScanNodeList(exprCallN.Arguments); return; }

		WhileNode whileN = node as WhileNode;
		if (whileN != null) { ScanNode(whileN.Condition); ScanNodeList(whileN.Body); return; }

		IfNode ifN = node as IfNode;
		if (ifN != null) {
			ScanNode(ifN.Condition);
			ScanNodeList(ifN.ThenBody);
			ScanNodeList(ifN.ElseBody);
			return;
		}

		ForNode forN = node as ForNode;
		if (forN != null) { ScanNode(forN.Iterable); ScanNodeList(forN.Body); return; }

		ReturnNode returnN = node as ReturnNode;
		if (returnN != null) { ScanNode(returnN.Value); return; }
	}

	public Int32 Visit(SelfNode node) {
		Int32 resultReg = GetTargetOrAlloc();
		Int32 selfReg = GetSelfReg();
		EmitNamedLoad(true, resultReg, selfReg, Value.selfString,
			$"r{resultReg} = self");
		return resultReg;
	}

	public Int32 Visit(SuperNode node) {
		Int32 resultReg = GetTargetOrAlloc();
		Int32 superReg = GetSuperReg();
		EmitNamedLoad(true, resultReg, superReg, Value.superString,
			$"r{resultReg} = super");
		return resultReg;
	}

	public Int32 Visit(ScopeNode node) {
		Int32 resultReg = GetTargetOrAlloc();
		if (node.Scope == ScopeType.Outer) {
			_emitter.EmitA(Opcode.OUTER_rA, resultReg, $"r{resultReg} = outer");
		} else if (node.Scope == ScopeType.Globals) {
			_emitter.EmitA(Opcode.GLOBALS_rA, resultReg, $"r{resultReg} = globals");
		} else {
			_emitter.EmitA(Opcode.LOCALS_rA, resultReg, $"r{resultReg} = locals");
		}
		return resultReg;
	}

	// Emit a method call: METHFIND + optional SETSELF + ARGBLK + ARGs + CALL
	// receiverReg: register holding the receiver object
	// methodKey: string name of the method
	// arguments: list of argument AST nodes
	// preserveSelf: if true, emit SETSELF to keep current self (for super.method() calls)
	private Int32 EmitMethodCall(Int32 receiverReg, String methodKey, List<ASTNode> arguments, bool preserveSelf) {
		Int32 explicitTarget = _targetReg;
		_targetReg = -1;

		// Compile arguments first.  Evaluating an argument expression may emit
		// its own METHFIND (e.g. a member access like `self.name`), which would
		// clobber the pending self/super context.  By compiling args before the
		// method lookup, nothing comes between METHFIND and CALL to disturb it.
		List<Int32> argRegs = CompileArguments(arguments);

		// Look up the method using METHFIND (walks __isa chain, sets pending self/super)
		Int32 keyReg = AllocReg();
		Int32 constIdx = _emitter.AddConstant(Value.make_string(methodKey));
		_emitter.EmitAB(Opcode.LOAD_rA_kBC, keyReg, constIdx, $"r{keyReg} = \"{methodKey}\"");
		Int32 funcReg = AllocReg();
		_emitter.EmitABC(Opcode.METHFIND_rA_rB_rC, funcReg, receiverReg, keyReg,
			$"r{funcReg} = {methodKey} (method lookup)");
		FreeReg(keyReg);

		// For super.method() calls, override pendingSelf with the current self
		if (preserveSelf) {
			Int32 selfReg = GetSelfReg();
			_emitter.EmitA(Opcode.SETSELF_rA, selfReg, $"preserve self for super call");
		}
		Int32 resultReg = EmitCallSequence(funcReg, argRegs, explicitTarget, $"method call {methodKey}");
		FreeReg(funcReg);

		return resultReg;
	}

	public Int32 Visit(ReturnNode node) {
		// Compile return value into r0, then emit RETURN
		if (node.Value != null) {
			CompileInto(node.Value, 0);
		}
		_emitter.Emit(Opcode.RETURN, null);
		return -1;
	}
}

}
