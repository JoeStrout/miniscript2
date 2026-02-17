// CodeGenerator.cs - Compiles AST nodes to bytecode using the visitor pattern
// Uses CodeEmitterBase to support both direct bytecode and assembly text output.

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// H: #include "AST.g.h"
// H: #include "CodeEmitter.g.h"
// H: #include "ErrorPool.g.h"
// CPP: #include "CS_Math.h"
// CPP: #include "gc.h"

namespace MiniScript {


// Compiles AST nodes to bytecode
public class CodeGenerator : IASTVisitor {
	private CodeEmitterBase _emitter;
	private List<Boolean> _regInUse;    // Which registers are currently in use
	private Int32 _firstAvailable;      // Lowest index that might be free
	private Int32 _maxRegUsed;          // High water mark for register usage
	private Dictionary<String, Int32> _variableRegs;  // variable name -> register
	private Int32 _targetReg;           // Target register for next expression (-1 = allocate)
	private List<Int32> _loopExitLabels;      // Stack of loop exit labels for break
	private List<Int32> _loopContinueLabels;  // Stack of loop continue labels for continue
	private List<FuncDef> _functions;          // All compiled functions (shared across inner generators)
	public ErrorPool Errors;

	public CodeGenerator(CodeEmitterBase emitter) {
		_emitter = emitter;
		_regInUse = new List<Boolean>();
		_firstAvailable = 0;
		_maxRegUsed = -1;
		_variableRegs = new Dictionary<String, Int32>();
		_targetReg = -1;
		_loopExitLabels = new List<Int32>();
		_loopContinueLabels = new List<Int32>();
		_functions = new List<FuncDef>();
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
			body[i].Accept(this);
		}
	}

	// Compile a complete function from a single expression/statement
	public FuncDef CompileFunction(ASTNode ast, String funcName) {
		_regInUse.Clear();
		_firstAvailable = 0;
		_maxRegUsed = -1;
		_variableRegs.Clear();

		Int32 resultReg = ast.Accept(this);

		// Move result to r0 if not already there (and if there is a result)
		if (resultReg > 0) {
			_emitter.EmitABC(Opcode.LOAD_rA_rB, 0, resultReg, 0, "move Function result to r0");
		}
		_emitter.Emit(Opcode.RETURN, null);

		return _emitter.Finalize(funcName);
	}

	// Compile a complete function from a list of statements (program)
	public FuncDef CompileProgram(List<ASTNode> statements, String funcName) {
		_regInUse.Clear();
		_firstAvailable = 0;
		_maxRegUsed = -1;
		_variableRegs.Clear();

		// Reserve index 0 for @main
		_functions.Clear();
		_functions.Add(null);

		// Compile each statement, putting result into r0
		for (Int32 i = 0; i < statements.Count; i++) {
			ResetTempRegisters();
			CompileInto(statements[i], 0);
		}

		_emitter.Emit(Opcode.RETURN, null);

		FuncDef mainFunc = _emitter.Finalize(funcName);
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
			Int32 constIdx = _emitter.AddConstant(make_double(value));
			_emitter.EmitAB(Opcode.LOAD_rA_kBC, reg, constIdx, $"r{reg} = {value}");
		}
		return reg;
	}

	public Int32 Visit(StringNode node) {
		Int32 reg = GetTargetOrAlloc();
		Int32 constIdx = _emitter.AddConstant(make_string(node.Value));
		_emitter.EmitAB(Opcode.LOAD_rA_kBC, reg, constIdx, $"r{reg} = \"{node.Value}\"");
		return reg;
	}

	private Int32 VisitIdentifier(IdentifierNode node, bool addressOf) {
		Int32 resultReg = GetTargetOrAlloc();

		// Handle built-in constants
		if (node.Name == "null") {
			_emitter.EmitAB(Opcode.LOAD_rA_iBC, resultReg, 0, $"r{resultReg} = null");
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

		Int32 varReg;
		if (_variableRegs.TryGetValue(node.Name, out varReg)) {
			// Variable found - emit LOADC (load-and-call for implicit function invocation)
			Int32 nameIdx = _emitter.AddConstant(make_string(node.Name));
			Opcode op = addressOf ? Opcode.LOADV_rA_rB_kC : Opcode.LOADC_rA_rB_kC;
			String a = addressOf ? "@" : "";
			_emitter.EmitABC(op, resultReg, varReg, nameIdx,
				$"r{resultReg} = {a}{node.Name}");
		} else {
			// Variable not in local scope — emit LOADC/LOADV referencing r0 with
			// the variable name. At runtime, the name check on r0 will fail,
			// triggering LookupVariable to search outer/global scope.
			Int32 nameIdx = _emitter.AddConstant(make_string(node.Name));
			Opcode op = addressOf ? Opcode.LOADV_rA_rB_kC : Opcode.LOADC_rA_rB_kC;
			String a = addressOf ? "@" : "";
			_emitter.EmitABC(op, resultReg, 0, nameIdx,
				$"r{resultReg} = {a}{node.Name} (outer)");
		}

		return resultReg;
	}

	public Int32 Visit(IdentifierNode node) {
		return VisitIdentifier(node, false);
	}

	public Int32 Visit(AssignmentNode node) {
		if (_targetReg > 0) {
			Errors.Add(StringUtils.Format("Compiler Error: unexpected target register {0} in assignment", _targetReg));
		}
		
		// Get or allocate register for this variable
		Int32 varReg;
		if (_variableRegs.TryGetValue(node.Variable, out varReg)) {
			// Variable already has a register - reuse it (and don't bother with naming)
		} else {
			// Hmm.  Should we allocate a new register for this variable, or
			// just claim the target register as our storage?  I'm going to alloc
			// a new one for now, because I can't be sure the caller won't free
			// the target register when done.  But we should probably return to
			// this later and see if we can optimize it more.
			varReg = AllocReg();
			_variableRegs[node.Variable] = varReg;
			Int32 nameIdx = _emitter.AddConstant(make_string(node.Variable));
			_emitter.EmitAB(Opcode.NAME_rA_kBC, varReg, nameIdx, $"use r{varReg} for {node.Variable}");
		}
		CompileInto(node.Value, varReg);  // get RHS directly into the variable's register

		// Note that we don't FreeReg(varReg) here, as we need this register to
		// continue to serve as the storage for this variable for the life of
		// the function.  (TODO: or until some lifetime analysis determines that
		// the variable will never be read again.)

		// BUT, if varReg is not the explicit target register, then we need to copy
		// the value there as well.  Not sure why that would ever be the case (since
		// assignment can't be used in an expression in MiniScript).  So:
		return varReg;
	}

	public Int32 Visit(IndexedAssignmentNode node) {
		Int32 containerReg = node.Target.Accept(this);
		Int32 indexReg = node.Index.Accept(this);
		Int32 valueReg = node.Value.Accept(this);

		_emitter.EmitABC(Opcode.IDXSET_rA_rB_rC, containerReg, indexReg, valueReg,
			$"{node.Target.ToStr()}[{node.Index.ToStr()}] = {node.Value.ToStr()}");

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
		Errors.Add("Compiler Error: unknown unary operator");
		if (operandReg != resultReg) {
			_emitter.EmitABC(Opcode.LOAD_rA_rB, resultReg, operandReg, 0, "move to target");
			FreeReg(operandReg);
		}
		return resultReg;
	}

	public Int32 Visit(BinaryOpNode node) {
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
			op = Opcode.MULT_rA_rB_rC;
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
			// Power requires a function call - not yet implemented
			// For now, emit a placeholder
			op = Opcode.NOOP;
			opSymbol = "^";
		} else if (node.Op == Op.AND) {
			// Fuzzy logic AND: AbsClamp01(a * b)
			op = Opcode.AND_rA_rB_rC;
			opSymbol = "and";
		} else if (node.Op == Op.OR) {
			// Fuzzy logic OR: AbsClamp01(a + b - a*b)
			op = Opcode.OR_rA_rB_rC;
			opSymbol = "or";
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

	public Int32 Visit(CallNode node) {
		// Capture target register if one was specified (don't allocate yet)
		Int32 explicitTarget = _targetReg;
		_targetReg = -1;

		Int32 argCount = node.Arguments.Count;

		// Check if the function is a known variable (user-defined function)
		Int32 funcVarReg;
		if (_variableRegs.TryGetValue(node.Function, out funcVarReg)) {
			// User-defined function call: ARGBLK + ARGs + CALL_rA_rB_rC
			return CompileUserCall(node, funcVarReg, explicitTarget);
		}

		// Intrinsic function call: CALLFN
		// Arguments loaded into consecutive registers starting at baseReg
		// Return value placed in baseReg after the call

		// Allocate consecutive registers for arguments
		// The first register (baseReg) will also hold the return value
		Int32 baseReg = (argCount > 0) ? AllocConsecutiveRegs(argCount) : AllocReg();

		// Compile each argument directly into its target register
		for (Int32 i = 0; i < argCount; i++) {
			CompileInto(node.Arguments[i], baseReg + i);
		}

		// Emit the function call
		// CALLFN_iA_kBC: A = base register, BC = constant index for function name
		Int32 funcNameIdx = _emitter.AddConstant(make_string(node.Function));
		_emitter.EmitAB(Opcode.CALLFN_iA_kBC, baseReg, funcNameIdx, $"call {node.Function}");

		// Free any extra argument registers (keeping baseReg for return value)
		for (Int32 i = 1; i < argCount; i++) {
			FreeReg(baseReg + i);
		}

		// If an explicit target was requested and it's different, move result there
		if (explicitTarget >= 0 && explicitTarget != baseReg) {
			_emitter.EmitABC(Opcode.LOAD_rA_rB, explicitTarget, baseReg, 0, $"move Call result to r{explicitTarget}");
			FreeReg(baseReg);
			return explicitTarget;
		}

		return baseReg;
	}

	// Compile a call to a user-defined function (funcref in a register)
	private Int32 CompileUserCall(CallNode node, Int32 funcVarReg, Int32 explicitTarget) {
		Int32 argCount = node.Arguments.Count;

		// Compile arguments into temporary registers
		List<Int32> argRegs = new List<Int32>();
		for (Int32 i = 0; i < argCount; i++) {
			argRegs.Add(node.Arguments[i].Accept(this));
		}

		// Emit ARGBLK (24-bit arg count)
		_emitter.EmitABC(Opcode.ARGBLK_iABC, 0, 0, argCount, $"argblock {argCount}");

		// Emit ARG for each argument
		for (Int32 i = 0; i < argCount; i++) {
			_emitter.EmitA(Opcode.ARG_rA, argRegs[i], $"arg {i}");
		}

		// Determine base register for callee frame (past all our used registers)
		Int32 calleeBase = _maxRegUsed + 1;
		_emitter.ReserveRegister(calleeBase);

		// Determine result register
		Int32 resultReg = (explicitTarget >= 0) ? explicitTarget : AllocReg();

		// Emit CALL: result in rA, callee frame at rB, funcref in rC
		_emitter.EmitABC(Opcode.CALL_rA_rB_rC, resultReg, calleeBase, funcVarReg,
			$"call {node.Function}, result to r{resultReg}");

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
		// Create a list with the given number of elements
		Int32 listReg = GetTargetOrAlloc();
		Int32 count = node.Elements.Count;
		_emitter.EmitAB(Opcode.LIST_rA_iBC, listReg, count, $"r{listReg} = new list[{count}]");

		// Push each element onto the list
		for (Int32 i = 0; i < count; i++) {
			Int32 elemReg = node.Elements[i].Accept(this);
			_emitter.EmitABC(Opcode.PUSH_rA_rB, listReg, elemReg, 0, $"push element {i} onto r{listReg}");
			FreeReg(elemReg);
		}

		return listReg;
	}

	public Int32 Visit(MapNode node) {
		// Create a map
		Int32 mapReg = GetTargetOrAlloc();
		Int32 count = node.Keys.Count;
		_emitter.EmitAB(Opcode.MAP_rA_iBC, mapReg, count, $"new map[{count}]");

		// Set each key-value pair
		for (Int32 i = 0; i < count; i++) {
			Int32 keyReg = node.Keys[i].Accept(this);
			Int32 valueReg = node.Values[i].Accept(this);
			_emitter.EmitABC(Opcode.IDXSET_rA_rB_rC, mapReg, keyReg, valueReg, $"map[{node.Keys[i].ToStr()}] = {node.Values[i].ToStr()}");
			FreeReg(valueReg);
			FreeReg(keyReg);
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

		if (addressOf) {
			_emitter.EmitABC(Opcode.INDEX_rA_rB_rC, resultReg, targetReg, indexReg,
				$"@{node.Target.ToStr()}[{node.Index.ToStr()}]");
		} else {
			_emitter.EmitABC(Opcode.METHFIND_rA_rB_rC, resultReg, targetReg, indexReg,
				$"{node.Target.ToStr()}[{node.Index.ToStr()}]");
			SuperNode superTarget = node.Target as SuperNode;
			if (superTarget != null) {
				Int32 selfReg = GetSelfReg();
				_emitter.EmitA(Opcode.SETSELF_rA, selfReg, $"preserve self for super access");
			}
			_emitter.EmitA(Opcode.CALLIFREF_rA, resultReg, $"auto-invoke if funcref");
		}

		FreeReg(indexReg);
		FreeReg(targetReg);
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
		Int32 constIdx = _emitter.AddConstant(make_string(node.Member));
		_emitter.EmitAB(Opcode.LOAD_rA_kBC, indexReg, constIdx, $"r{indexReg} = \"{node.Member}\"");

		if (addressOf) {
			// @ prefix: plain INDEX, no auto-invoke
			_emitter.EmitABC(Opcode.INDEX_rA_rB_rC, resultReg, targetReg, indexReg,
				$"@{node.Target.ToStr()}.{node.Member}");
		} else {
			// Normal access: METHFIND + optional SETSELF + CALLIFREF
			_emitter.EmitABC(Opcode.METHFIND_rA_rB_rC, resultReg, targetReg, indexReg,
				$"{node.Target.ToStr()}.{node.Member}");
			// If target is 'super', preserve current self
			SuperNode superTarget = node.Target as SuperNode;
			if (superTarget != null) {
				Int32 selfReg = GetSelfReg();
				_emitter.EmitA(Opcode.SETSELF_rA, selfReg, $"preserve self for super access");
			}
			_emitter.EmitA(Opcode.CALLIFREF_rA, resultReg, $"auto-invoke if funcref");
		}

		FreeReg(indexReg);
		FreeReg(targetReg);
		return resultReg;
	}

	public Int32 Visit(ExprCallNode node) {
		// Check if the function expression is a member access or index operation
		// on a map — if so, this is a method call and we need to set self/super.
		MemberNode memberTarget = node.Function as MemberNode;
		if (memberTarget != null) {
			// obj.method() via ExprCallNode — treat as method call
			SuperNode superTarget = memberTarget.Target as SuperNode;
			bool preserveSelf = (superTarget != null);
			Int32 receiverReg = memberTarget.Target.Accept(this);
			Int32 resultReg = EmitMethodCall(receiverReg, memberTarget.Member, node.Arguments, preserveSelf);
			FreeReg(receiverReg);
			return resultReg;
		}

		IndexNode indexTarget = node.Function as IndexNode;
		if (indexTarget != null) {
			// obj[key]() — treat as method call if key is a string
			SuperNode superTarget = indexTarget.Target as SuperNode;
			bool preserveSelf = (superTarget != null);
			Int32 explicitTarget = _targetReg;
			_targetReg = -1;

			// Evaluate receiver and key
			Int32 receiverReg = indexTarget.Target.Accept(this);
			Int32 keyReg = indexTarget.Index.Accept(this);

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

			// Compile arguments
			Int32 argCount = node.Arguments.Count;
			List<Int32> argRegs = new List<Int32>();
			for (Int32 i = 0; i < argCount; i++) {
				argRegs.Add(node.Arguments[i].Accept(this));
			}

			_emitter.EmitABC(Opcode.ARGBLK_iABC, 0, 0, argCount, $"argblock {argCount}");
			for (Int32 i = 0; i < argCount; i++) {
				_emitter.EmitA(Opcode.ARG_rA, argRegs[i], $"arg {i}");
			}

			Int32 calleeBase = _maxRegUsed + 1;
			_emitter.ReserveRegister(calleeBase);
			Int32 resultReg = (explicitTarget >= 0) ? explicitTarget : AllocReg();

			_emitter.EmitABC(Opcode.CALL_rA_rB_rC, resultReg, calleeBase, funcReg,
				$"method call via index, result to r{resultReg}");

			for (Int32 i = 0; i < argCount; i++) {
				FreeReg(argRegs[i]);
			}
			FreeReg(funcReg);
			FreeReg(receiverReg);
			return resultReg;
		}

		// Regular function call (not a method call)
		Int32 explicitTarget2 = _targetReg;
		_targetReg = -1;

		Int32 argCount2 = node.Arguments.Count;

		// Evaluate the function expression to get the funcref
		Int32 funcReg2 = node.Function.Accept(this);

		// Compile arguments into temporary registers
		List<Int32> argRegs2 = new List<Int32>();
		for (Int32 i = 0; i < argCount2; i++) {
			argRegs2.Add(node.Arguments[i].Accept(this));
		}

		// Emit ARGBLK (24-bit arg count)
		_emitter.EmitABC(Opcode.ARGBLK_iABC, 0, 0, argCount2, $"argblock {argCount2}");

		// Emit ARG for each argument
		for (Int32 i = 0; i < argCount2; i++) {
			_emitter.EmitA(Opcode.ARG_rA, argRegs2[i], $"arg {i}");
		}

		// Determine base register for callee frame (past all our used registers)
		Int32 calleeBase2 = _maxRegUsed + 1;
		_emitter.ReserveRegister(calleeBase2);

		// Determine result register
		Int32 resultReg2 = (explicitTarget2 >= 0) ? explicitTarget2 : AllocReg();

		// Emit CALL: result in rA, callee frame at rB, funcref in rC
		_emitter.EmitABC(Opcode.CALL_rA_rB_rC, resultReg2, calleeBase2, funcReg2,
			$"call expr, result to r{resultReg2}");

		// Free argument registers and funcref register
		for (Int32 i = 0; i < argCount2; i++) {
			FreeReg(argRegs2[i]);
		}
		FreeReg(funcReg2);

		return resultReg2;
	}

	public Int32 Visit(MethodCallNode node) {
		// Check if the target is 'super' — if so, preserve current self
		SuperNode superTarget = node.Target as SuperNode;
		bool preserveSelf = (superTarget != null);

		// Evaluate receiver
		Int32 receiverReg = node.Target.Accept(this);

		// Emit method call
		Int32 resultReg = EmitMethodCall(receiverReg, node.Method, node.Arguments, preserveSelf);

		FreeReg(receiverReg);
		return resultReg;
	}

	public Int32 Visit(WhileNode node) {
		// While loop generates:
		//   loopStart:
		//       [evaluate condition]
		//       BRFALSE condReg, afterLoop
		//       [body statements]
		//       JUMP loopStart
		//   afterLoop:

		Int32 loopStart = _emitter.CreateLabel();
		Int32 afterLoop = _emitter.CreateLabel();

		// Push labels for break and continue statements
		_loopExitLabels.Add(afterLoop);
		_loopContinueLabels.Add(loopStart);

		// Place loopStart label
		_emitter.PlaceLabel(loopStart);

		// Evaluate condition
		Int32 condReg = node.Condition.Accept(this);

		// Branch to afterLoop if condition is false
		_emitter.EmitBranch(Opcode.BRFALSE_rA_iBC, condReg, afterLoop, "exit loop if false");
		FreeReg(condReg);

		// Compile body statements
		CompileBody(node.Body);

		// Jump back to loopStart
		_emitter.EmitJump(Opcode.JUMP_iABC, loopStart, "loop back");

		// Place afterLoop label
		_emitter.PlaceLabel(afterLoop);

		// Pop labels
		_loopExitLabels.RemoveAt(_loopExitLabels.Count - 1);
		_loopContinueLabels.RemoveAt(_loopContinueLabels.Count - 1);

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

		// Compile "then" body
		CompileBody(node.ThenBody);

		// Jump over else body (if there is one)
		if (node.ElseBody.Count > 0) {
			_emitter.EmitJump(Opcode.JUMP_iABC, afterIf, "jump past else");

			// Place else label
			_emitter.PlaceLabel(elseLabel);

			// Compile "else" body
			CompileBody(node.ElseBody);
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

		Int32 loopStart = _emitter.CreateLabel();
		Int32 afterLoop = _emitter.CreateLabel();

		// Push labels for break and continue statements
		_loopExitLabels.Add(afterLoop);
		_loopContinueLabels.Add(loopStart);

		// Evaluate iterable expression
		Int32 listReg = node.Iterable.Accept(this);

		// Allocate hidden index register (starts at -1; NEXT will increment to 0)
		Int32 indexReg = AllocReg();
		_emitter.EmitAB(Opcode.LOAD_rA_iBC, indexReg, -1, "for loop index = -1");

		// Get or create register for loop variable
		Int32 varReg;
		if (_variableRegs.TryGetValue(node.Variable, out varReg)) {
			// Variable already exists
		} else {
			varReg = AllocReg();
			_variableRegs[node.Variable] = varReg;
			Int32 nameIdx = _emitter.AddConstant(make_string(node.Variable));
			_emitter.EmitAB(Opcode.NAME_rA_kBC, varReg, nameIdx, $"use r{varReg} for {node.Variable}");
		}

		// Place loopStart label
		_emitter.PlaceLabel(loopStart);

		// NEXT: increment index, skip next if done
		_emitter.EmitABC(Opcode.NEXT_rA_rB, indexReg, listReg, 0, "index++; skip next if done");
		_emitter.EmitJump(Opcode.JUMP_iABC, afterLoop, "exit loop");

		// Get current element: varReg = listReg[indexReg]
		_emitter.EmitABC(Opcode.INDEX_rA_rB_rC, varReg, listReg, indexReg, $"{node.Variable} = list[index]");

		// Compile body statements
		CompileBody(node.Body);

		// Jump back to loopStart
		_emitter.EmitJump(Opcode.JUMP_iABC, loopStart, "loop back");

		// Place afterLoop label
		_emitter.PlaceLabel(afterLoop);

		// Pop labels
		_loopExitLabels.RemoveAt(_loopExitLabels.Count - 1);
		_loopContinueLabels.RemoveAt(_loopContinueLabels.Count - 1);

		// Free temporary registers (but keep variable register)
		FreeReg(indexReg);
		FreeReg(listReg);

		// For loops don't produce a value
		return -1;
	}

	public Int32 Visit(BreakNode node) {
		// Break jumps to the innermost loop's exit label
		if (_loopExitLabels.Count == 0) {
			Errors.Add("Compiler Error: 'break' without open loop block");
			_emitter.Emit(Opcode.NOOP, "break outside loop (error)");
		} else {
			Int32 exitLabel = _loopExitLabels[_loopExitLabels.Count - 1];
			_emitter.EmitJump(Opcode.JUMP_iABC, exitLabel, "break");
		}
		return -1;
	}

	public Int32 Visit(ContinueNode node) {
		// Continue jumps to the innermost loop's continue label (loop start)
		if (_loopContinueLabels.Count == 0) {
			Errors.Add("Compiler Error: 'continue' without open loop block");
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
		result = make_null();
		NumberNode numNode = node as NumberNode;
		if (numNode != null) {
			result = make_double(numNode.Value);
			return true;
		}
		StringNode strNode = node as StringNode;
		if (strNode != null) {
			result = make_string(strNode.Value);
			return true;
		}
		IdentifierNode idNode = node as IdentifierNode;
		if (idNode != null) {
			if (idNode.Name == "null") { result = make_null(); return true; }
			if (idNode.Name == "true") { result = make_double(1); return true; }
			if (idNode.Name == "false") { result = make_double(0); return true; }
			return false;
		}
		UnaryOpNode unaryNode = node as UnaryOpNode;
		if (unaryNode != null && unaryNode.Op == Op.MINUS) {
			NumberNode innerNum = unaryNode.Operand as NumberNode;
			if (innerNum != null) {
				result = make_double(-innerNum.Value);
				return true;
			}
			return false;
		}
		Value list;
		Value elemVal;
		ListNode listNode = node as ListNode;
		if (listNode != null) {
			list = make_list(listNode.Elements.Count);
			for (Int32 i = 0; i < listNode.Elements.Count; i++) {
				if (!TryEvaluateConstant(listNode.Elements[i], out elemVal)) return false;
				list_push(list, elemVal);
			}
			freeze_value(list);
			result = list;
			return true;
		}
		Value map;
		Value keyVal;
		Value valVal;
		MapNode mapNode = node as MapNode;
		if (mapNode != null) {
			map = make_map(mapNode.Keys.Count);
			for (Int32 i = 0; i < mapNode.Keys.Count; i++) {
				if (!TryEvaluateConstant(mapNode.Keys[i], out keyVal)) return false;
				if (!TryEvaluateConstant(mapNode.Values[i], out valVal)) return false;
				map_set(map, keyVal, valVal);
			}
			freeze_value(map);
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
		innerGen._functions = _functions;  // share the function list
		innerGen.Errors = Errors;

		// Reserve r0 for return value, then set up param registers (r1, r2, ...)
		innerGen.AllocReg();  // r0 reserved for return value
		for (Int32 i = 0; i < node.ParamNames.Count; i++) {
			Int32 paramReg = innerGen.AllocReg();  // r1, r2, ...
			String name = node.ParamNames[i];
			innerGen._variableRegs[name] = paramReg;
			Int32 nameIdx = innerEmitter.AddConstant(make_string(name));
			innerEmitter.EmitAB(Opcode.NAME_rA_kBC, paramReg, nameIdx, $"param {name}");
		}

		// Compile the function body
		innerGen.CompileBody(node.Body);

		// Emit implicit RETURN at end of body
		innerEmitter.Emit(Opcode.RETURN, null);

		// Finalize the inner function
		String funcName = StringUtils.Format("@f{0}", funcIndex);
		FuncDef funcDef = innerEmitter.Finalize(funcName);

		// Set parameter info on the FuncDef
		Value defaultVal;
		for (Int32 i = 0; i < node.ParamNames.Count; i++) {
			funcDef.ParamNames.Add(make_string(node.ParamNames[i]));
			ASTNode defaultNode = node.ParamDefaults[i];
			if (defaultNode != null) {
				if (TryEvaluateConstant(defaultNode, out defaultVal)) {
					funcDef.ParamDefaults.Add(defaultVal);
				} else {
					Errors.Add(StringUtils.Format("Default value for parameter '{0}' must be a constant", node.ParamNames[i]));
					funcDef.ParamDefaults.Add(make_null());
				}
			} else {
				funcDef.ParamDefaults.Add(make_null());
			}
		}

		// If the inner function uses self/super, ensure the outer function also
		// allocates those registers so ApplyPendingContext can populate them
		// and FUNCREF can capture them for the closure.
		if (funcDef.SelfReg >= 0) GetSelfReg();
		if (funcDef.SuperReg >= 0) GetSuperReg();

		// Store in the shared functions list
		_functions[funcIndex] = funcDef;

		// In the outer function, emit FUNCREF to create a reference
		_emitter.EmitAB(Opcode.FUNCREF_iA_iBC, resultReg, funcIndex,
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

	public Int32 Visit(SelfNode node) {
		Int32 resultReg = GetTargetOrAlloc();
		Int32 selfReg = GetSelfReg();
		Int32 nameIdx = _emitter.AddConstant(val_self);
		_emitter.EmitABC(Opcode.LOADV_rA_rB_kC, resultReg, selfReg, nameIdx,
			$"r{resultReg} = self");
		return resultReg;
	}

	public Int32 Visit(SuperNode node) {
		Int32 resultReg = GetTargetOrAlloc();
		Int32 superReg = GetSuperReg();
		Int32 nameIdx = _emitter.AddConstant(val_super);
		_emitter.EmitABC(Opcode.LOADV_rA_rB_kC, resultReg, superReg, nameIdx,
			$"r{resultReg} = super");
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

		// Look up the method using METHFIND (walks __isa chain, sets pending self/super)
		Int32 keyReg = AllocReg();
		Int32 constIdx = _emitter.AddConstant(make_string(methodKey));
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

		// Compile arguments
		Int32 argCount = arguments.Count;
		List<Int32> argRegs = new List<Int32>();
		for (Int32 i = 0; i < argCount; i++) {
			argRegs.Add(arguments[i].Accept(this));
		}

		// Emit ARGBLK + ARG instructions
		_emitter.EmitABC(Opcode.ARGBLK_iABC, 0, 0, argCount, $"argblock {argCount}");
		for (Int32 i = 0; i < argCount; i++) {
			_emitter.EmitA(Opcode.ARG_rA, argRegs[i], $"arg {i}");
		}

		// Determine callee frame base
		Int32 calleeBase = _maxRegUsed + 1;
		_emitter.ReserveRegister(calleeBase);

		// Result register
		Int32 resultReg = (explicitTarget >= 0) ? explicitTarget : AllocReg();

		// Emit CALL
		_emitter.EmitABC(Opcode.CALL_rA_rB_rC, resultReg, calleeBase, funcReg,
			$"method call {methodKey}, result to r{resultReg}");

		// Free temporaries
		for (Int32 i = 0; i < argCount; i++) {
			FreeReg(argRegs[i]);
		}
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
