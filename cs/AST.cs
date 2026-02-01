// AST.cs - Abstract Syntax Tree nodes for MiniScript
// These classes use the smart-pointer-wrapper pattern when transpiled to C++.

using System;
using System.Collections.Generic;
// CPP: #include "StringUtils.g.h"
// CPP: #include "CS_Math.h"
// CPP: #include <cmath>

namespace MiniScript {

// Operator constants (stored as strings to ease debugging)
public static class Op {
	public const String PLUS = "PLUS";
	public const String MINUS = "MINUS";
	public const String TIMES = "TIMES";
	public const String DIVIDE = "DIVIDE";
	public const String MOD = "MOD";
	public const String POWER = "POWER";
	public const String EQUALS = "EQUALS";
	public const String NOT_EQUAL = "NOT_EQUAL";
	public const String LESS_THAN = "LESS_THAN";
	public const String GREATER_THAN = "GREATER_THAN";
	public const String LESS_EQUAL = "LESS_EQUAL";
	public const String GREATER_EQUAL = "GREATER_EQUAL";
	public const String AND = "AND";
	public const String OR = "OR";
	public const String NOT = "NOT";
}

// Visitor interface for AST traversal (e.g., code generation)
public interface IASTVisitor {
	Int32 Visit(NumberNode node);
	Int32 Visit(StringNode node);
	Int32 Visit(IdentifierNode node);
	Int32 Visit(AssignmentNode node);
	Int32 Visit(UnaryOpNode node);
	Int32 Visit(BinaryOpNode node);
	Int32 Visit(CallNode node);
	Int32 Visit(GroupNode node);
	Int32 Visit(ListNode node);
	Int32 Visit(MapNode node);
	Int32 Visit(IndexNode node);
	Int32 Visit(MemberNode node);
	Int32 Visit(MethodCallNode node);
}

// Base class for all AST nodes.
// When transpiled to C++, these become shared_ptr-wrapped classes.
public abstract class ASTNode {
	// Each node type should override this to provide a string representation
	public abstract String ToStr();

	// Simplify this node (constant folding and other optimizations)
	// Returns a simplified version of this node (may be a new node, or this node unchanged)
	public abstract ASTNode Simplify();

	// Visitor pattern: accept a visitor and return the result (e.g., register number)
	public abstract Int32 Accept(IASTVisitor visitor);
}

// Number literal node (e.g., 42, 3.14)
public class NumberNode : ASTNode {
	public Double Value;

	public NumberNode(Double value) {
		Value = value;
	}

	public override String ToStr() {
		return $"{Value}";
	}

	public override ASTNode Simplify() {
		return this;
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

// String literal node (e.g., "hello")
public class StringNode : ASTNode {
	public String Value;

	public StringNode(String value) {
		Value = value;
	}

	public override String ToStr() {
		return "\"" + Value + "\"";
	}

	public override ASTNode Simplify() {
		return this;
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

// Identifier node (e.g., variable name like "x" or "foo")
public class IdentifierNode : ASTNode {
	public String Name;

	public IdentifierNode(String name) {
		Name = name;
	}

	public override String ToStr() {
		return Name;
	}

	public override ASTNode Simplify() {
		return this;
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

// Assignment node (e.g., x = 42, foo = bar + 1)
public class AssignmentNode : ASTNode {
	public String Variable;     // variable name being assigned to
	public ASTNode Value;       // expression being assigned

	public AssignmentNode(String variable, ASTNode value) {
		Variable = variable;
		Value = value;
	}

	public override String ToStr() {
		return Variable + " = " + Value.ToStr();
	}

	public override ASTNode Simplify() {
		ASTNode simplifiedValue = Value.Simplify();
		return new AssignmentNode(Variable, simplifiedValue);
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

// Unary operator node (e.g., -x, not flag)
public class UnaryOpNode : ASTNode {
	public String Op;           // Op.MINUS, Op.NOT, etc.
	public ASTNode Operand;     // the expression being operated on

	public UnaryOpNode(String op, ASTNode operand) {
		Op = op;
		Operand = operand;
	}

	public override String ToStr() {
		return Op + "(" + Operand.ToStr() + ")";
	}

	public override ASTNode Simplify() {
		ASTNode simplifiedOperand = Operand.Simplify();

		// If operand is a constant, compute the result
		NumberNode num = simplifiedOperand as NumberNode;
		if (num != null) {
			if (Op == MiniScript.Op.MINUS) {
				return new NumberNode(-num.Value);
			} else if (Op == MiniScript.Op.NOT) {
				return new NumberNode(num.Value == 0 ? 1 : 0);
			}
		}

		// Otherwise return unary op with simplified operand
		return new UnaryOpNode(Op, simplifiedOperand);
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

// Binary operator node (e.g., x + y, a * b)
public class BinaryOpNode : ASTNode {
	public String Op;           // Op.PLUS, etc.
	public ASTNode Left;        // left operand
	public ASTNode Right;       // right operand

	public BinaryOpNode(String op, ASTNode left, ASTNode right) {
		Op = op;
		Left = left;
		Right = right;
	}

	public override String ToStr() {
		return Op + "(" + Left.ToStr() + ", " + Right.ToStr() + ")";
	}

	public override ASTNode Simplify() {
		ASTNode simplifiedLeft = Left.Simplify();
		ASTNode simplifiedRight = Right.Simplify();

		// If both operands are numeric constants, compute the result
		NumberNode leftNum = simplifiedLeft as NumberNode;
		NumberNode rightNum = simplifiedRight as NumberNode;
		if (leftNum != null && rightNum != null) {
			if (Op == MiniScript.Op.PLUS) {
				return new NumberNode(leftNum.Value + rightNum.Value);
			} else if (Op == MiniScript.Op.MINUS) {
				return new NumberNode(leftNum.Value - rightNum.Value);
			} else if (Op == MiniScript.Op.TIMES) {
				return new NumberNode(leftNum.Value * rightNum.Value);
			} else if (Op == MiniScript.Op.DIVIDE) {
				return new NumberNode(leftNum.Value / rightNum.Value);
			} else if (Op == MiniScript.Op.MOD) {
				Double result = leftNum.Value % rightNum.Value; // CPP: Double result = fmod(leftNum.Value(), rightNum.Value());
				return new NumberNode(result);
			} else if (Op == MiniScript.Op.POWER) {
				return new NumberNode(Math.Pow(leftNum.Value, rightNum.Value));
			} else if (Op == MiniScript.Op.EQUALS) {
				return new NumberNode(leftNum.Value == rightNum.Value ? 1 : 0);
			} else if (Op == MiniScript.Op.NOT_EQUAL) {
				return new NumberNode(leftNum.Value != rightNum.Value ? 1 : 0);
			} else if (Op == MiniScript.Op.LESS_THAN) {
				return new NumberNode(leftNum.Value < rightNum.Value ? 1 : 0);
			} else if (Op == MiniScript.Op.GREATER_THAN) {
				return new NumberNode(leftNum.Value > rightNum.Value ? 1 : 0);
			} else if (Op == MiniScript.Op.LESS_EQUAL) {
				return new NumberNode(leftNum.Value <= rightNum.Value ? 1 : 0);
			} else if (Op == MiniScript.Op.GREATER_EQUAL) {
				return new NumberNode(leftNum.Value >= rightNum.Value ? 1 : 0);
			} else if (Op == MiniScript.Op.AND) {
				return new NumberNode((leftNum.Value != 0 && rightNum.Value != 0) ? 1 : 0);
			} else if (Op == MiniScript.Op.OR) {
				return new NumberNode((leftNum.Value != 0 || rightNum.Value != 0) ? 1 : 0);
			}
		}

		// Otherwise return binary op with simplified operands
		return new BinaryOpNode(Op, simplifiedLeft, simplifiedRight);
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

// Function call node (e.g., sqrt(x), max(a, b))
public class CallNode : ASTNode {
	public String Function;             // function name
	public List<ASTNode> Arguments;     // list of argument expressions

	public CallNode(String function, List<ASTNode> arguments) {
		Function = function;
		Arguments = arguments;
	}

	public CallNode(String function) {
		Function = function;
		Arguments = new List<ASTNode>();
	}

	public override String ToStr() {
		String result = Function + "(";
		for (Int32 i = 0; i < Arguments.Count; i++) {
			if (i > 0) result = result + ", ";
			result = result + Arguments[i].ToStr();
		}
		result = result + ")";
		return result;
	}

	public override ASTNode Simplify() {
		List<ASTNode> simplifiedArgs = new List<ASTNode>();
		for (Int32 i = 0; i < Arguments.Count; i++) {
			simplifiedArgs.Add(Arguments[i].Simplify());
		}
		return new CallNode(Function, simplifiedArgs);
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

// Grouping node (e.g., parenthesized expression like "(x + y)")
// Useful for preserving structure for pretty-printing or code generation.
public class GroupNode : ASTNode {
	public ASTNode Expression;

	public GroupNode(ASTNode expression) {
		Expression = expression;
	}

	public override String ToStr() {
		return "(" + Expression.ToStr() + ")";
	}

	public override ASTNode Simplify() {
		// Groups don't affect value, just return simplified child
		return Expression.Simplify();
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

// List literal node (e.g., [1, 2, 3])
public class ListNode : ASTNode {
	public List<ASTNode> Elements;

	public ListNode(List<ASTNode> elements) {
		Elements = elements;
	}

	public ListNode() {
		Elements = new List<ASTNode>();
	}

	public override String ToStr() {
		String result = "[";
		for (Int32 i = 0; i < Elements.Count; i++) {
			if (i > 0) result = result + ", ";
			result = result + Elements[i].ToStr();
		}
		result = result + "]";
		return result;
	}

	public override ASTNode Simplify() {
		List<ASTNode> simplifiedElements = new List<ASTNode>();
		for (Int32 i = 0; i < Elements.Count; i++) {
			simplifiedElements.Add(Elements[i].Simplify());
		}
		return new ListNode(simplifiedElements);
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

// Map literal node (e.g., {"a": 1, "b": 2})
public class MapNode : ASTNode {
	public List<ASTNode> Keys;
	public List<ASTNode> Values;

	public MapNode(List<ASTNode> keys, List<ASTNode> values) {
		Keys = keys;
		Values = values;
	}

	public MapNode() {
		Keys = new List<ASTNode>();
		Values = new List<ASTNode>();
	}

	public override String ToStr() {
		String result = "{";
		for (Int32 i = 0; i < Keys.Count; i++) {
			if (i > 0) result = result + ", ";
			result = result + Keys[i].ToStr() + ": " + Values[i].ToStr();
		}
		result = result + "}";
		return result;
	}

	public override ASTNode Simplify() {
		List<ASTNode> simplifiedKeys = new List<ASTNode>();
		List<ASTNode> simplifiedValues = new List<ASTNode>();
		for (Int32 i = 0; i < Keys.Count; i++) {
			simplifiedKeys.Add(Keys[i].Simplify());
			simplifiedValues.Add(Values[i].Simplify());
		}
		return new MapNode(simplifiedKeys, simplifiedValues);
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

// Index access node (e.g., list[0], map["key"])
public class IndexNode : ASTNode {
	public ASTNode Target;      // the list/map/string being indexed
	public ASTNode Index;       // the index expression

	public IndexNode(ASTNode target, ASTNode index) {
		Target = target;
		Index = index;
	}

	public override String ToStr() {
		return Target.ToStr() + "[" + Index.ToStr() + "]";
	}

	public override ASTNode Simplify() {
		return new IndexNode(Target.Simplify(), Index.Simplify());
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

// Member access node (e.g., obj.field)
public class MemberNode : ASTNode {
	public ASTNode Target;      // the object being accessed
	public String Member;       // the member name

	public MemberNode(ASTNode target, String member) {
		Target = target;
		Member = member;
	}

	public override String ToStr() {
		return Target.ToStr() + "." + Member;
	}

	public override ASTNode Simplify() {
		return new MemberNode(Target.Simplify(), Member);
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

// Method call node (e.g., obj.method(x, y))
// This is distinct from CallNode which handles simple function calls.
public class MethodCallNode : ASTNode {
	public ASTNode Target;              // the object whose method is being called
	public String Method;               // the method name
	public List<ASTNode> Arguments;     // list of argument expressions

	public MethodCallNode(ASTNode target, String method, List<ASTNode> arguments) {
		Target = target;
		Method = method;
		Arguments = arguments;
	}

	public override String ToStr() {
		String result = Target.ToStr() + "." + Method + "(";
		for (Int32 i = 0; i < Arguments.Count; i++) {
			if (i > 0) result = result + ", ";
			result = result + Arguments[i].ToStr();
		}
		result = result + ")";
		return result;
	}

	public override ASTNode Simplify() {
		List<ASTNode> simplifiedArgs = new List<ASTNode>();
		for (Int32 i = 0; i < Arguments.Count; i++) {
			simplifiedArgs.Add(Arguments[i].Simplify());
		}
		return new MethodCallNode(Target.Simplify(), Method, simplifiedArgs);
	}

	public override Int32 Accept(IASTVisitor visitor) {
		return visitor.Visit(this);
	}
}

}
