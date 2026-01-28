// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Parselet.cs

#include "Parselet.g.h"

namespace MiniScript {










ASTNode NumberParseletStorage::Parse(IParser& parser, Token token) {
	return  NumberNode::New(token.DoubleValue);
}


ASTNode StringParseletStorage::Parse(IParser& parser, Token token) {
	return  StringNode::New(token.Text);
}


ASTNode IdentifierParseletStorage::Parse(IParser& parser, Token token) {
	String name = token.Text;

	// Check what comes next
	if (parser.Check(TokenType::ASSIGN)) {
		parser.Consume();  // consume '='
		ASTNode value = parser.ParseExpression(Precedence::ASSIGNMENT);
		return  AssignmentNode::New(name, value);
	}

	// Just a variable reference
	return  IdentifierNode::New(name);
}


UnaryOpParseletStorage::UnaryOpParseletStorage(String op, Precedence prec) {
	_op = op;
	Prec = prec;
}
ASTNode UnaryOpParseletStorage::Parse(IParser& parser, Token token) {
	ASTNode operand = parser.ParseExpression(Prec);
	return  UnaryOpNode::New(_op, operand);
}


ASTNode GroupParseletStorage::Parse(IParser& parser, Token token) {
	ASTNode expr = parser.ParseExpression(Precedence::NONE);
	parser.Expect(TokenType::RPAREN, "Expected ')' after expression");
	return  GroupNode::New(expr);
}


ASTNode ListParseletStorage::Parse(IParser& parser, Token token) {
	List<ASTNode> elements =  List<ASTNode>::New();

	if (!parser.Check(TokenType::RBRACKET)) {
		do {
			elements.Add(parser.ParseExpression(Precedence::NONE));
		} while (parser.Match(TokenType::COMMA));
	}

	parser.Expect(TokenType::RBRACKET, "Expected ']' after list elements");
	return  ListNode::New(elements);
}


ASTNode MapParseletStorage::Parse(IParser& parser, Token token) {
	List<ASTNode> keys =  List<ASTNode>::New();
	List<ASTNode> values =  List<ASTNode>::New();

	if (!parser.Check(TokenType::RBRACE)) {
		do {
			ASTNode key = parser.ParseExpression(Precedence::NONE);
			parser.Expect(TokenType::COLON, "Expected ':' after map key");
			ASTNode value = parser.ParseExpression(Precedence::NONE);
			keys.Add(key);
			values.Add(value);
		} while (parser.Match(TokenType::COMMA));
	}

	parser.Expect(TokenType::RBRACE, "Expected '}' after map entries");
	return  MapNode::New(keys, values);
}


BinaryOpParseletStorage::BinaryOpParseletStorage(String op, Precedence prec, Boolean rightAssoc) {
	_op = op;
	Prec = prec;
	_rightAssoc = rightAssoc;
}
BinaryOpParseletStorage::BinaryOpParseletStorage(String op, Precedence prec) {
	_op = op;
	Prec = prec;
	_rightAssoc = Boolean(false);
}
ASTNode BinaryOpParseletStorage::Parse(IParser& parser, ASTNode left, Token token) {
	// For right-associative operators, use lower precedence for RHS
	Precedence rhsPrec = _rightAssoc ? (Precedence)((Int32)Prec - 1) : Prec;
	ASTNode right = parser.ParseExpression(rhsPrec);
	return  BinaryOpNode::New(_op, left, right);
}


CallParseletStorage::CallParseletStorage() {
	Prec = Precedence::CALL;
}
ASTNode CallParseletStorage::Parse(IParser& parser, ASTNode left, Token token) {
	List<ASTNode> args =  List<ASTNode>::New();

	if (!parser.Check(TokenType::RPAREN)) {
		do {
			args.Add(parser.ParseExpression(Precedence::NONE));
		} while (parser.Match(TokenType::COMMA));
	}

	parser.Expect(TokenType::RPAREN, "Expected ')' after arguments");

	// Simple function call: foo(x, y)
	IdentifierNode funcName = As<IdentifierNode, IdentifierNodeStorage>(left);
	if (!IsNull(funcName)) {
		return  CallNode::New(funcName.Name(), args);
	}

	// Method call: obj::method(x, y)
	MemberNode memberAccess = As<MemberNode, MemberNodeStorage>(left);
	if (!IsNull(memberAccess)) {
		return  MethodCallNode::New(memberAccess.Target(), memberAccess.Member(), args);
	}

	// Other cases (e::g::, result of function call being called)
	// For now, report an error - could be extended later
	parser.ReportError("Expected function name or method access before '('");
	return  NumberNode::New(0);
}


IndexParseletStorage::IndexParseletStorage() {
	Prec = Precedence::CALL;
}
ASTNode IndexParseletStorage::Parse(IParser& parser, ASTNode left, Token token) {
	ASTNode index = parser.ParseExpression(Precedence::NONE);
	parser.Expect(TokenType::RBRACKET, "Expected ']' after index");
	return  IndexNode::New(left, index);
}


MemberParseletStorage::MemberParseletStorage() {
	Prec = Precedence::CALL;
}
ASTNode MemberParseletStorage::Parse(IParser& parser, ASTNode left, Token token) {
	Token memberToken = parser.Expect(TokenType::IDENTIFIER, "Expected member name after '.'");
	return  MemberNode::New(left, memberToken.Text);
}


} // end of namespace MiniScript
