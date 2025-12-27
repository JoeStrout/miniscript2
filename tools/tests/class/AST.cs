using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;

namespace MiniScript {

// Base class for all AST nodes.
public abstract class ASTNode {

	// Each node type should override this to provide a string representation
	public abstract String Str();

	// Simplify this node (constant folding and other optimizations)
	// Returns a simplified version of this node (may be a new node, or this node unchanged)
	public abstract ASTNode Simplify();
}

// Number literal node (e.g., 42, 3.14)
public class NumberNode : ASTNode {
	public Double value;

	[MethodImpl(AggressiveInlining)]
	public NumberNode(Double value) {
		this.value = value;
	}

	[MethodImpl(AggressiveInlining)]
	public override String Str() {
		return $"{value}";
	}

	[MethodImpl(AggressiveInlining)]
	public override ASTNode Simplify() {
		return this;
	}
}

// Binary operator node (e.g., x + y, a * b)
public class BinaryOpNode : ASTNode {
	public String op;       // Op.PLUS, etc.
	public ASTNode left;    // left operand
	public ASTNode right;   // right operand

	[MethodImpl(AggressiveInlining)]
	public BinaryOpNode(String op, ASTNode left, ASTNode right) {
		this.op = op;
		this.left = left;
		this.right = right;
	}

	[MethodImpl(AggressiveInlining)]
	public override String Str() {
		return op + "(" + left.Str() + ", " + right.Str() + ")";
	}

	public override ASTNode Simplify() {
		ASTNode simplifiedLeft = left.Simplify();
		ASTNode simplifiedRight = right.Simplify();

		// If both operands are constants, compute the result
		var leftNum = simplifiedLeft as NumberNode;
		var rightNum = simplifiedRight as NumberNode;
		if (leftNum != null && rightNum != null) {
			if (op == "PLUS") {
				return new NumberNode(leftNum.value + rightNum.value);
			} else if (op == "MINUS") {
				return new NumberNode(leftNum.value - rightNum.value);
			} else if (op == "TIMES") {
				return new NumberNode(leftNum.value * rightNum.value);
			}
		}

		// Otherwise return binary op with simplified operands
		return new BinaryOpNode(op, simplifiedLeft, simplifiedRight);
	}
}

}
