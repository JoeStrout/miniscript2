// Parselet.cs - Mini-parsers for the Pratt parser
// Each parselet is responsible for parsing one type of expression construct.

using System;
using System.Collections.Generic;
// H: #include "AST.g.h"
// H: #include "Lexer.g.h"
// H: #include "LangConstants.g.h"

namespace MiniScript {

// IParser interface: defines the methods that parselets need from the parser.
// This breaks the circular dependency between Parser and Parselet.
public interface IParser {
	Boolean Check(TokenType type);
	Boolean Match(TokenType type);
	Token Consume();
	Token Expect(TokenType type, String errorMessage);
	ASTNode ParseExpression(Precedence minPrecedence);
	void ReportError(String message);
	Boolean CanStartExpression(TokenType type);
}

// Base class for all parselets
public class Parselet {
	public Precedence Prec;
}

// PrefixParselet: abstract base for parselets that handle tokens
// starting an expression (numbers, identifiers, unary operators).
public abstract class PrefixParselet : Parselet {
	public abstract ASTNode Parse(IParser parser, Token token);
}

// InfixParselet: abstract base for parselets that handle infix operators.
public abstract class InfixParselet : Parselet {
	public abstract ASTNode Parse(IParser parser, ASTNode left, Token token);
}

// NumberParselet: handles number literals.
public class NumberParselet : PrefixParselet {
	public NumberParselet() {}
	public override ASTNode Parse(IParser parser, Token token) {
		return new NumberNode(token.DoubleValue);
	}
}

// SelfParselet: handles the 'self' keyword.
public class SelfParselet : PrefixParselet {
	public SelfParselet() {}
	public override ASTNode Parse(IParser parser, Token token) {
		return new SelfNode();
	}
}

// SuperParselet: handles the 'super' keyword.
public class SuperParselet : PrefixParselet {
	public SuperParselet() {}
	public override ASTNode Parse(IParser parser, Token token) {
		return new SuperNode();
	}
}

// StringParselet: handles string literals.
public class StringParselet : PrefixParselet {
	public StringParselet() {}
	public override ASTNode Parse(IParser parser, Token token) {
		return new StringNode(token.Text);
	}
}

// IdentifierParselet: handles identifiers, which can be:
// - Variable lookups
// - Variable assignments (when followed by '=')
// - Function calls (when followed by '(')
public class IdentifierParselet : PrefixParselet {
	public IdentifierParselet() {}
	public override ASTNode Parse(IParser parser, Token token) {
		String name = token.Text;

		// Check what comes next
		if (parser.Check(TokenType.ASSIGN)) {
			parser.Consume();  // consume '='
			ASTNode value = parser.ParseExpression(Precedence.ASSIGNMENT);
			return new AssignmentNode(name, value);
		}

		// Just a variable reference
		return new IdentifierNode(name);
	}
}

// UnaryOpParselet: handles prefix unary operators like '-' and 'not'.
public class UnaryOpParselet : PrefixParselet {
	private String _op;

	public UnaryOpParselet(String op, Precedence prec) {
		_op = op;
		Prec = prec;
	}

	public override ASTNode Parse(IParser parser, Token token) {
		ASTNode operand = parser.ParseExpression(Prec);
		return new UnaryOpNode(_op, operand);
	}
}

// GroupParselet: handles parenthesized expressions like '(2 + 3)'.
public class GroupParselet : PrefixParselet {
	public GroupParselet() {}
	public override ASTNode Parse(IParser parser, Token token) {
		ASTNode expr = parser.ParseExpression(Precedence.NONE);
		parser.Expect(TokenType.RPAREN, "Expected ')' after expression");
		return new GroupNode(expr);
	}
}

// ListParselet: handles list literals like '[1, 2, 3]'.
public class ListParselet : PrefixParselet {
	public ListParselet() {}
	public override ASTNode Parse(IParser parser, Token token) {
		List<ASTNode> elements = new List<ASTNode>();

		if (!parser.Check(TokenType.RBRACKET)) {
			do {
				elements.Add(parser.ParseExpression(Precedence.NONE));
			} while (parser.Match(TokenType.COMMA));
		}

		parser.Expect(TokenType.RBRACKET, "Expected ']' after list elements");
		return new ListNode(elements);
	}
}

// MapParselet: handles map literals like '{"a": 1}'.
public class MapParselet : PrefixParselet {
	public MapParselet() {}
	public override ASTNode Parse(IParser parser, Token token) {
		List<ASTNode> keys = new List<ASTNode>();
		List<ASTNode> values = new List<ASTNode>();

		if (!parser.Check(TokenType.RBRACE)) {
			do {
				ASTNode key = parser.ParseExpression(Precedence.NONE);
				parser.Expect(TokenType.COLON, "Expected ':' after map key");
				ASTNode value = parser.ParseExpression(Precedence.NONE);
				keys.Add(key);
				values.Add(value);
			} while (parser.Match(TokenType.COMMA));
		}

		parser.Expect(TokenType.RBRACE, "Expected '}' after map entries");
		return new MapNode(keys, values);
	}
}

// BinaryOpParselet: handles binary operators like '+', '-', '*', etc.
public class BinaryOpParselet : InfixParselet {
	private String _op;
	private Boolean _rightAssoc;

	public BinaryOpParselet(String op, Precedence prec, Boolean rightAssoc) {
		_op = op;
		Prec = prec;
		_rightAssoc = rightAssoc;
	}

	public BinaryOpParselet(String op, Precedence prec) {
		_op = op;
		Prec = prec;
		_rightAssoc = false;
	}

	public override ASTNode Parse(IParser parser, ASTNode left, Token token) {
		// For right-associative operators, use lower precedence for RHS
		Precedence rhsPrec = _rightAssoc ? (Precedence)((Int32)Prec - 1) : Prec;
		ASTNode right = parser.ParseExpression(rhsPrec);
		return new BinaryOpNode(_op, left, right);
	}
}

// ComparisonParselet: handles comparison operators with Python-style chaining.
// If only one comparison, returns BinaryOpNode; if chained, returns ComparisonChainNode.
public class ComparisonParselet : InfixParselet {
	private String _op;

	public ComparisonParselet(String op, Precedence prec) {
		_op = op;
		Prec = prec;
	}

	public override ASTNode Parse(IParser parser, ASTNode left, Token token) {
		// Parse RHS at our precedence (same-precedence ops won't be consumed)
		ASTNode right = parser.ParseExpression(Prec);

		// Check if next token is another comparison operator
		if (!IsComparisonToken(parser)) {
			return new BinaryOpNode(_op, left, right);
		}

		// Chain detected - collect all operands and operators
		List<ASTNode> operands = new List<ASTNode>();
		List<String> operators = new List<String>();
		operands.Add(left);
		operands.Add(right);
		operators.Add(_op);

		while (IsComparisonToken(parser)) {
			Token nextToken = parser.Consume();
			String nextOp = TokenToComparisonOp(nextToken.Type);
			ASTNode nextRhs = parser.ParseExpression(Prec);
			operands.Add(nextRhs);
			operators.Add(nextOp);
		}

		return new ComparisonChainNode(operands, operators);
	}

	private static Boolean IsComparisonToken(IParser parser) {
		return parser.Check(TokenType.LESS_THAN)
			|| parser.Check(TokenType.GREATER_THAN)
			|| parser.Check(TokenType.LESS_EQUAL)
			|| parser.Check(TokenType.GREATER_EQUAL)
			|| parser.Check(TokenType.EQUALS)
			|| parser.Check(TokenType.NOT_EQUAL);
	}

	private static String TokenToComparisonOp(TokenType type) {
		if (type == TokenType.LESS_THAN) return Op.LESS_THAN;
		if (type == TokenType.GREATER_THAN) return Op.GREATER_THAN;
		if (type == TokenType.LESS_EQUAL) return Op.LESS_EQUAL;
		if (type == TokenType.GREATER_EQUAL) return Op.GREATER_EQUAL;
		if (type == TokenType.EQUALS) return Op.EQUALS;
		if (type == TokenType.NOT_EQUAL) return Op.NOT_EQUAL;
		return Op.EQUALS;
	}
}

// CallParselet: handles function calls like 'foo(x, y)' and method calls like 'obj.method(x)'.
public class CallParselet : InfixParselet {
	public CallParselet() {
		Prec = Precedence.CALL;
	}

	public override ASTNode Parse(IParser parser, ASTNode left, Token token) {
		List<ASTNode> args = new List<ASTNode>();

		if (!parser.Check(TokenType.RPAREN)) {
			do {
				args.Add(parser.ParseExpression(Precedence.NONE));
			} while (parser.Match(TokenType.COMMA));
		}

		parser.Expect(TokenType.RPAREN, "Expected ')' after arguments");

		// Simple function call: foo(x, y)
		IdentifierNode funcName = left as IdentifierNode;
		if (funcName != null) {
			return new CallNode(funcName.Name, args);
		}

		// Method call: obj.method(x, y)
		MemberNode memberAccess = left as MemberNode;
		if (memberAccess != null) {
			return new MethodCallNode(memberAccess.Target, memberAccess.Member, args);
		}

		// General expression call: expr(args), e.g. funcs[0](10)
		return new ExprCallNode(left, args);
	}
}

// IndexParselet: handles index access like 'list[0]' or 'map["key"]'.
public class IndexParselet : InfixParselet {
	public IndexParselet() {
		Prec = Precedence.CALL;
	}

	public override ASTNode Parse(IParser parser, ASTNode left, Token token) {
		// Check for [:endExpr] (omitted start)
		if (parser.Check(TokenType.COLON)) {
			parser.Consume();  // consume ':'
			ASTNode endExpr = null;
			if (!parser.Check(TokenType.RBRACKET)) {
				endExpr = parser.ParseExpression(Precedence.NONE);
			}
			parser.Expect(TokenType.RBRACKET, "Expected ']' after slice");
			return new SliceNode(left, null, endExpr);
		}

		ASTNode startExpr = parser.ParseExpression(Precedence.NONE);

		// Check for slice syntax: [startExpr:endExpr]
		if (parser.Check(TokenType.COLON)) {
			parser.Consume();  // consume ':'
			ASTNode endExpr = null;
			if (!parser.Check(TokenType.RBRACKET)) {
				endExpr = parser.ParseExpression(Precedence.NONE);
			}
			parser.Expect(TokenType.RBRACKET, "Expected ']' after slice");
			return new SliceNode(left, startExpr, endExpr);
		}

		// Plain index access
		parser.Expect(TokenType.RBRACKET, "Expected ']' after index");
		return new IndexNode(left, startExpr);
	}
}

// MemberParselet: handles member access like 'obj.field'.
public class MemberParselet : InfixParselet {
	public MemberParselet() {
		Prec = Precedence.CALL;
	}

	public override ASTNode Parse(IParser parser, ASTNode left, Token token) {
		Token memberToken = parser.Expect(TokenType.IDENTIFIER, "Expected member name after '.'");
		return new MemberNode(left, memberToken.Text);
	}
}

}
