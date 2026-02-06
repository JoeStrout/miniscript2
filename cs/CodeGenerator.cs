// CodeGenerator.cs - Compiles AST nodes to bytecode using the visitor pattern
// Uses CodeEmitterBase to support both direct bytecode and assembly text output.

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// H: #include "AST.g.h"
// H: #include "CodeEmitter.g.h"
// CPP: #include "CS_Math.h"

namespace MiniScript {


// Compiles AST nodes to bytecode
public class CodeGenerator : IASTVisitor {
	private CodeEmitterBase _emitter;
	private List<Boolean> _regInUse;    // Which registers are currently in use
	private Int32 _firstAvailable;      // Lowest index that might be free
	private Int32 _maxRegUsed;          // High water mark for register usage
	private Dictionary<String, Int32> _variableRegs;  // variable name -> register

	public CodeGenerator(CodeEmitterBase emitter) {
		_emitter = emitter;
		_regInUse = new List<Boolean>();
		_firstAvailable = 0;
		_maxRegUsed = -1;
		_variableRegs = new Dictionary<String, Int32>();
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

	// Compile an expression, placing result in a newly allocated register
	// Returns the register number holding the result
	public Int32 Compile(ASTNode ast) {
		return ast.Accept(this);
	}

	// Compile a complete function from a single expression/statement
	public FuncDef CompileFunction(ASTNode ast, String funcName) {
		_regInUse.Clear();
		_firstAvailable = 0;
		_maxRegUsed = -1;
		_variableRegs.Clear();

		Int32 resultReg = ast.Accept(this);

		// Move result to r0 if not already there
		if (resultReg != 0) {
			_emitter.EmitABC(Opcode.LOAD_rA_rB, 0, resultReg, 0, "move result to r0");
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

		Int32 resultReg = 0;

		// Compile each statement sequentially
		for (Int32 i = 0; i < statements.Count; i++) {
			// Free temporary registers before each statement, but keep variable registers
			_regInUse.Clear();
			_firstAvailable = 0;

			// Mark variable registers as still in use
			foreach (Int32 reg in _variableRegs.Values) { // CPP: for (Int32 reg : _variableRegs.GetValues()) {
				while (_regInUse.Count <= reg) {
					_regInUse.Add(false);
				}
				_regInUse[reg] = true;
				if (reg >= _firstAvailable) _firstAvailable = reg + 1;
			}

			resultReg = statements[i].Accept(this);

			// Move result to r0 after each statement
			// (the last one's result will be the function's return value)
			if (resultReg != 0) {
				_emitter.EmitABC(Opcode.LOAD_rA_rB, 0, resultReg, 0, "move result to r0");
			}
		}

		_emitter.Emit(Opcode.RETURN, null);

		return _emitter.Finalize(funcName);
	}

	// --- Visit methods for each AST node type ---

	public Int32 Visit(NumberNode node) {
		Int32 reg = AllocReg();
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
		Int32 reg = AllocReg();
		Int32 constIdx = _emitter.AddConstant(make_string(node.Value));
		_emitter.EmitAB(Opcode.LOAD_rA_kBC, reg, constIdx, $"r{reg} = \"{node.Value}\"");
		return reg;
	}

	public Int32 Visit(IdentifierNode node) {
		Int32 resultReg = AllocReg();

		Int32 varReg;
		if (_variableRegs.TryGetValue(node.Name, out varReg)) {
			// Variable found - emit LOADC (load-and-call for implicit function invocation)
			Int32 nameIdx = _emitter.AddConstant(make_string(node.Name));
			_emitter.EmitABC(Opcode.LOADC_rA_rB_kC, resultReg, varReg, nameIdx,
				$"r{resultReg} = {node.Name}");
		} else {
			// Undefined variable - for now just load 0
			// TODO: handle outer/globals lookup or report error
			_emitter.EmitAB(Opcode.LOAD_rA_iBC, resultReg, 0, $"undefined: {node.Name}");
		}

		return resultReg;
	}

	public Int32 Visit(AssignmentNode node) {
		Int32 valueReg = node.Value.Accept(this);

		// Get or allocate register for this variable
		Int32 varReg;
		if (_variableRegs.TryGetValue(node.Variable, out varReg)) {
			// Variable already has a register - reuse it
		} else {
			// Allocate new register for variable
			varReg = AllocReg();
			_variableRegs[node.Variable] = varReg;
		}

		// Emit ASSIGN: copy value and set name
		Int32 nameIdx = _emitter.AddConstant(make_string(node.Variable));
		_emitter.EmitABC(Opcode.ASSIGN_rA_rB_kC, varReg, valueReg, nameIdx,
			$"{node.Variable} = {node.Value.ToStr()}");

		// Note that we don't FreeReg(varReg) here, as we need this register to
		// continue to serve as the storage for this variable for the life of
		// the function.  (TODO: or until some lifetime analysis determines that
		// the variable will never be read again.)
		return varReg;
	}

	public Int32 Visit(UnaryOpNode node) {
		Int32 operandReg = node.Operand.Accept(this);

		if (node.Op == Op.MINUS) {
			// Negate: result = 0 - operand
			Int32 resultReg = AllocReg();
			Int32 zeroReg = AllocReg();
			_emitter.EmitAB(Opcode.LOAD_rA_iBC, zeroReg, 0, "r{zeroReg} = 0 (for negation)");
			_emitter.EmitABC(Opcode.SUB_rA_rB_rC, resultReg, zeroReg, operandReg, $"r{resultReg} = -{node.Operand.ToStr()}");
			FreeReg(zeroReg);
			FreeReg(operandReg);
			return resultReg;
		} else if (node.Op == Op.NOT) {
			// Fuzzy logic NOT: 1 - AbsClamp01(operand)
			Int32 resultReg = AllocReg();
			_emitter.EmitABC(Opcode.NOT_rA_rB, resultReg, operandReg, 0, $"not {node.Operand.ToStr()}");
			FreeReg(operandReg);
			return resultReg;
		}

		// Unknown unary operator - return operand unchanged
		// ToDo: report internal error
		return operandReg;
	}

	public Int32 Visit(BinaryOpNode node) {
		Int32 resultReg = AllocReg();
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
		// Generate code for function calls
		// For intrinsic functions, we use CALLFN which expects:
		// - Arguments loaded into consecutive registers starting at baseReg
		// - Return value placed in baseReg after the call

		// First, compile all arguments into consecutive registers
		Int32 baseReg = AllocReg();  // This will hold first arg and return value
		Int32 argCount = node.Arguments.Count;

		// If we have arguments, compile them
		if (argCount > 0) {
			// Compile first argument into baseReg
			Int32 firstArgReg = node.Arguments[0].Accept(this);
			if (firstArgReg != baseReg) {
				_emitter.EmitABC(Opcode.LOAD_rA_rB, baseReg, firstArgReg, 0, $"move arg 0 to r{baseReg}");
				FreeReg(firstArgReg);
			}

			// Compile remaining arguments into consecutive registers
			for (Int32 i = 1; i < argCount; i++) {
				Int32 argReg = AllocReg();
				Int32 compiledReg = node.Arguments[i].Accept(this);
				if (compiledReg != argReg) {
					_emitter.EmitABC(Opcode.LOAD_rA_rB, argReg, compiledReg, 0, $"move arg {i} to r{argReg}");
					FreeReg(compiledReg);
				}
			}
		}

		// Emit the function call
		// CALLFN_iA_kBC: A = base register, BC = constant index for function name
		Int32 funcNameIdx = _emitter.AddConstant(make_string(node.Function));
		_emitter.EmitAB(Opcode.CALLFN_iA_kBC, baseReg, funcNameIdx, $"call {node.Function}");

		// Free any extra argument registers we allocated (keeping baseReg for return value)
		// Note: The argument registers after baseReg are now free since the call has completed
		for (Int32 i = 1; i < argCount; i++) {
			FreeReg(baseReg + i);
		}

		// Return value is in baseReg
		return baseReg;
	}

	public Int32 Visit(GroupNode node) {
		// Groups just delegate to their inner expression
		return node.Expression.Accept(this);
	}

	public Int32 Visit(ListNode node) {
		// Create a list with the given number of elements
		Int32 listReg = AllocReg();
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
		Int32 mapReg = AllocReg();
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
		Int32 resultReg = AllocReg();
		Int32 targetReg = node.Target.Accept(this);
		Int32 indexReg = node.Index.Accept(this);

		_emitter.EmitABC(Opcode.INDEX_rA_rB_rC, resultReg, targetReg, indexReg,
			$"{node.Target.ToStr()}[{node.Index.ToStr()}]");

		FreeReg(indexReg);
		FreeReg(targetReg);
		return resultReg;
	}

	public Int32 Visit(MemberNode node) {
		// Member access not yet fully implemented
		Int32 resultReg = AllocReg();
		Int32 targetReg = node.Target.Accept(this);
		// TODO: Implement member access
		_emitter.EmitABC(Opcode.LOAD_rA_rB, resultReg, targetReg, 0, $"TODO: {node.Target.ToStr()}.{node.Member}");
		FreeReg(targetReg);
		return resultReg;
	}

	public Int32 Visit(MethodCallNode node) {
		// Method calls not yet implemented
		Int32 reg = AllocReg();
		_emitter.EmitAB(Opcode.LOAD_rA_iBC, reg, 0, $"TODO: {node.Target.ToStr()}.{node.Method}()");
		return reg;
	}
}

}
