// CodeGenerator.cs - Compiles AST nodes to bytecode using the visitor pattern
// Uses ICodeEmitter to support both direct bytecode and assembly text output.

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;

namespace MiniScript {

// Compiles AST nodes to bytecode
public class CodeGenerator : IASTVisitor {
	private ICodeEmitter _emitter;
	private Int32 _nextReg;      // Next available register
	private Int32 _maxRegUsed;   // High water mark for register usage

	public CodeGenerator(ICodeEmitter emitter) {
		_emitter = emitter;
		_nextReg = 0;
		_maxRegUsed = -1;
	}

	// Allocate a register
	private Int32 AllocReg() {
		Int32 reg = _nextReg;
		_nextReg = _nextReg + 1;
		if (reg > _maxRegUsed) _maxRegUsed = reg;
		_emitter.ReserveRegister(reg);
		return reg;
	}

	// Free a register (only if it's the most recently allocated)
	// This enables simple stack-based register reuse
	// ToDo: consider more sophisticated register-in-use tracking,
	// so we don't have to free registers in reverse order.
	// Or possibly something that frees all registers allocated
	// after a certain point.
	private void FreeReg(Int32 reg) {
		if (reg == _nextReg - 1) {
			_nextReg = _nextReg - 1;
		}
	}

	// Compile an expression, placing result in a newly allocated register
	// Returns the register number holding the result
	public Int32 Compile(ASTNode ast) {
		return ast.Accept(this);
	}

	// Compile a complete function from an expression
	public FuncDef CompileFunction(ASTNode ast, String funcName) {
		_nextReg = 0;
		_maxRegUsed = -1;

		Int32 resultReg = ast.Accept(this);

		// Move result to r0 if not already there
		if (resultReg != 0) {
			_emitter.EmitAB(Opcode.LOAD_rA_rB, 0, resultReg, "move result to r0");
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
			_emitter.EmitAB(Opcode.LOAD_rA_iBC, reg, (Int32)value, $"{value}");
		} else {
			// Store in constants and load from there
			Int32 constIdx = _emitter.AddConstant(make_double(value));
			_emitter.EmitAB(Opcode.LOAD_rA_kBC, reg, constIdx, $"{value}");
		}
		return reg;
	}

	public Int32 Visit(StringNode node) {
		Int32 reg = AllocReg();
		Int32 constIdx = _emitter.AddConstant(make_string(node.Value));
		_emitter.EmitAB(Opcode.LOAD_rA_kBC, reg, constIdx, $"\"{node.Value}\"");
		return reg;
	}

	public Int32 Visit(IdentifierNode node) {
		// For now, identifiers are not supported (requires variable scope)
		// This is a placeholder that will be expanded later
		Int32 reg = AllocReg();
		// TODO: Look up variable in scope and load its value
		_emitter.EmitAB(Opcode.LOAD_rA_iBC, reg, 0, $"TODO: load {node.Name}");
		return reg;
	}

	public Int32 Visit(AssignmentNode node) {
		// For now, assignment is not fully supported (requires variable scope)
		// Compile the value expression and return its register
		Int32 valueReg = node.Value.Accept(this);
		// TODO: Store value in variable
		return valueReg;
	}

	public Int32 Visit(UnaryOpNode node) {
		Int32 operandReg = node.Operand.Accept(this);

		if (node.Op == Op.MINUS) {
			// Negate: result = 0 - operand
			Int32 resultReg = AllocReg();
			Int32 zeroReg = AllocReg();
			_emitter.EmitAB(Opcode.LOAD_rA_iBC, zeroReg, 0, "0 for negation");
			_emitter.EmitABC(Opcode.SUB_rA_rB_rC, resultReg, zeroReg, operandReg, $"-{node.Operand.ToStr()}");
			FreeReg(zeroReg);
			FreeReg(operandReg);
			return resultReg;
		} else if (node.Op == Op.NOT) {
			// Logical not: compare with 0, result is 1 if equal, 0 otherwise
			// (ToDo: proper MiniScript fuzzy-logic NOT)
			Int32 resultReg = AllocReg();
			_emitter.EmitABC(Opcode.EQ_rA_rB_iC, resultReg, operandReg, 0, $"not {node.Operand.ToStr()}");
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
			// Logical AND - not yet implemented (requires short-circuit evaluation)
			op = Opcode.NOOP;
			opSymbol = "and";
		} else if (node.Op == Op.OR) {
			// Logical OR - not yet implemented (requires short-circuit evaluation)
			op = Opcode.NOOP;
			opSymbol = "or";
		}

		if (op != Opcode.NOOP) {
			_emitter.EmitABC(op, resultReg, leftReg, rightReg,
				$"{node.Left.ToStr()} {opSymbol} {node.Right.ToStr()}");
		}

		FreeReg(rightReg);
		FreeReg(leftReg);
		return resultReg;
	}

	public Int32 Visit(CallNode node) {
		// Function calls not yet implemented
		Int32 reg = AllocReg();
		_emitter.EmitAB(Opcode.LOAD_rA_iBC, reg, 0, $"TODO: call {node.Function}");
		return reg;
	}

	public Int32 Visit(GroupNode node) {
		// Groups just delegate to their inner expression
		return node.Expression.Accept(this);
	}

	public Int32 Visit(ListNode node) {
		// Create a list with the given number of elements
		Int32 listReg = AllocReg();
		Int32 count = node.Elements.Count;
		_emitter.EmitAB(Opcode.LIST_rA_iBC, listReg, count, $"new list[{count}]");

		// Push each element onto the list
		for (Int32 i = 0; i < count; i++) {
			Int32 elemReg = node.Elements[i].Accept(this);
			_emitter.EmitAB(Opcode.PUSH_rA_rB, listReg, elemReg, $"push element {i}");
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
		_emitter.EmitAB(Opcode.LOAD_rA_rB, resultReg, targetReg, $"TODO: {node.Target.ToStr()}.{node.Member}");
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
