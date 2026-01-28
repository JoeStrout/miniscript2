// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Parselet.cs

#pragma once
#include "core_includes.h"
// Parselet.cs - Mini-parsers for the Pratt parser
// Each parselet is responsible for parsing one type of expression construct.


namespace MiniScript {

// FORWARD DECLARATIONS

struct VM;
class VMStorage;
struct Assembler;
class AssemblerStorage;
struct Parselet;
class ParseletStorage;
struct Parser;
class ParserStorage;
struct FuncDef;
class FuncDefStorage;
struct ASTNode;
class ASTNodeStorage;
struct NumberNode;
class NumberNodeStorage;
struct StringNode;
class StringNodeStorage;
struct IdentifierNode;
class IdentifierNodeStorage;
struct AssignmentNode;
class AssignmentNodeStorage;
struct UnaryOpNode;
class UnaryOpNodeStorage;
struct BinaryOpNode;
class BinaryOpNodeStorage;
struct CallNode;
class CallNodeStorage;
struct GroupNode;
class GroupNodeStorage;
struct ListNode;
class ListNodeStorage;
struct MapNode;
class MapNodeStorage;
struct IndexNode;
class IndexNodeStorage;
struct MemberNode;
class MemberNodeStorage;
struct MethodCallNode;
class MethodCallNodeStorage;

// DECLARATIONS









// Precedence levels (higher precedence binds more strongly)
enum class Precedence : Int32 {
	NONE = 0,
	ASSIGNMENT = 1,
	OR = 2,
	AND = 3,
	EQUALITY = 4,        // == !=
	COMPARISON = 5,      // < > <= >=
	SUM = 6,             // + -
	PRODUCT = 7,         // * / %
	POWER = 8,           // ^
	UNARY = 9,           // - not
	CALL = 10,           // () []
	PRIMARY = 11
}; // end of enum Precedence


// Base class for all parselets

























class ParseletStorage : public std::enable_shared_from_this<ParseletStorage> {
	public: virtual ~ParseletStorage() {}
	public: Precedence Prec;
}; // end of class ParseletStorage

struct Parselet {
	protected: std::shared_ptr<ParseletStorage> storage;
  public:
	Parselet(std::shared_ptr<ParseletStorage> stor) : storage(stor) {}
	Parselet() : storage(nullptr) {}
	friend bool IsNull(Parselet inst) { return inst.storage == nullptr; }
	template<typename WrapperType, typename StorageType>
	friend WrapperType As(ASTNode node) {
		StorageType* stor = dynamic_cast<StorageType*>(node.storage.get());
		if (stor == nullptr) return WrapperType(nullptr);
		return WrapperType(node.storage);
	}

	public: Precedence Prec() { return get()->Prec; }
	public: void set_Prec(Precedence _v) { get()->Prec = _v; }
}; // end of struct Parselet


// INLINE METHODS

} // end of namespace MiniScript

// InfixParselet: abstract base for parselets that handle infix operators.
public abstract class InfixParselet : Parselet {
	public abstract ASTNode Parse(Parser parser, ASTNode left, Token token);
}

// NumberParselet: handles number literals.
public class NumberParselet : PrefixParselet {
	public override ASTNode Parse(Parser parser, Token token) {
		return new NumberNode(token.DoubleValue);
	}
}

// StringParselet: handles string literals.
public class StringParselet : PrefixParselet {
	public override ASTNode Parse(Parser parser, Token token) {
		return new StringNode(token.Text);
	}
}

// IdentifierParselet: handles identifiers, which can be:
// - Variable lookups
// - Variable assignments (when followed by '=')
// - Function calls (when followed by '(')
public class IdentifierParselet : PrefixParselet {
	public override ASTNode Parse(Parser parser, Token token) {
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

	public override ASTNode Parse(Parser parser, Token token) {
		ASTNode operand = parser.ParseExpression(Prec);
		return new UnaryOpNode(_op, operand);
	}
}

// GroupParselet: handles parenthesized expressions like '(2 + 3)'.
public class GroupParselet : PrefixParselet {
	public override ASTNode Parse(Parser parser, Token token) {
		ASTNode expr = parser.ParseExpression(Precedence.NONE);
		parser.Expect(TokenType.RPAREN, "Expected ')' after expression");
		return new GroupNode(expr);
	}
}

// ListParselet: handles list literals like '[1, 2, 3]'.
public class ListParselet : PrefixParselet {
	public override ASTNode Parse(Parser parser, Token token) {
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
	public override ASTNode Parse(Parser parser, Token token) {
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

	public override ASTNode Parse(Parser parser, ASTNode left, Token token) {
		// For right-associative operators, use lower precedence for RHS
		Precedence rhsPrec = _rightAssoc ? (Precedence)((Int32)Prec - 1) : Prec;
		ASTNode right = parser.ParseExpression(rhsPrec);
		return new BinaryOpNode(_op, left, right);
	}
}

// CallParselet: handles function calls like 'foo(x, y)' and method calls like 'obj.method(x)'.
public class CallParselet : InfixParselet {
	public CallParselet() {
		Prec = Precedence.CALL;
	}

	public override ASTNode Parse(Parser parser, ASTNode left, Token token) {
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

		// Other cases (e.g., result of function call being called)
		// For now, report an error - could be extended later
		parser.ReportError("Expected function name or method access before '('");
		return new NumberNode(0);
	}
}

// IndexParselet: handles index access like 'list[0]' or 'map["key"]'.
public class IndexParselet : InfixParselet {
	public IndexParselet() {
		Prec = Precedence.CALL;
	}

	public override ASTNode Parse(Parser parser, ASTNode left, Token token) {
		ASTNode index = parser.ParseExpression(Precedence.NONE);
		parser.Expect(TokenType.RBRACKET, "Expected ']' after index");
		return new IndexNode(left, index);
	}
}

// MemberParselet: handles member access like 'obj.field'.
public class MemberParselet : InfixParselet {
	public MemberParselet() {
		Prec = Precedence.CALL;
	}

	public override ASTNode Parse(Parser parser, ASTNode left, Token token) {
		Token memberToken = parser.Expect(TokenType.IDENTIFIER, "Expected member name after '.'");
		return new MemberNode(left, memberToken.Text);
	}
}

}
