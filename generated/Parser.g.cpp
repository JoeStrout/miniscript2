// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Parser.cs

#include "Parser.g.h"
#include "AST.g.h"
#include "IOHelper.g.h"

namespace MiniScript {


ParserStorage::ParserStorage() {
	_errors =  List<String>::New();
	_prefixParselets =  Dictionary<TokenType, PrefixParselet>::New();
	_infixParselets =  Dictionary<TokenType, InfixParselet>::New();

	RegisterParselets();
}
void ParserStorage::RegisterParselets() {
	// Prefix parselets (tokens that can start an expression)
	RegisterPrefix(TokenType::NUMBER,  NumberParselet::New());
	RegisterPrefix(TokenType::STRING,  StringParselet::New());
	RegisterPrefix(TokenType::IDENTIFIER,  IdentifierParselet::New());
	RegisterPrefix(TokenType::LPAREN,  GroupParselet::New());
	RegisterPrefix(TokenType::LBRACKET,  ListParselet::New());
	RegisterPrefix(TokenType::LBRACE,  MapParselet::New());

	// Unary operators
	RegisterPrefix(TokenType::MINUS,  UnaryOpParselet::New(Op::MINUS, Precedence::UNARY));
	RegisterPrefix(TokenType::NOT,  UnaryOpParselet::New(Op::NOT, Precedence::UNARY));

	// Binary operators
	RegisterInfix(TokenType::PLUS,  BinaryOpParselet::New(Op::PLUS, Precedence::SUM));
	RegisterInfix(TokenType::MINUS,  BinaryOpParselet::New(Op::MINUS, Precedence::SUM));
	RegisterInfix(TokenType::TIMES,  BinaryOpParselet::New(Op::TIMES, Precedence::PRODUCT));
	RegisterInfix(TokenType::DIVIDE,  BinaryOpParselet::New(Op::DIVIDE, Precedence::PRODUCT));
	RegisterInfix(TokenType::MOD,  BinaryOpParselet::New(Op::MOD, Precedence::PRODUCT));
	RegisterInfix(TokenType::CARET,  BinaryOpParselet::New(Op::POWER, Precedence::POWER, Boolean(true))); // right-assoc

	// Comparison operators
	RegisterInfix(TokenType::EQUALS,  BinaryOpParselet::New(Op::EQUALS, Precedence::EQUALITY));
	RegisterInfix(TokenType::NOT_EQUAL,  BinaryOpParselet::New(Op::NOT_EQUAL, Precedence::EQUALITY));
	RegisterInfix(TokenType::LESS_THAN,  BinaryOpParselet::New(Op::LESS_THAN, Precedence::COMPARISON));
	RegisterInfix(TokenType::GREATER_THAN,  BinaryOpParselet::New(Op::GREATER_THAN, Precedence::COMPARISON));
	RegisterInfix(TokenType::LESS_EQUAL,  BinaryOpParselet::New(Op::LESS_EQUAL, Precedence::COMPARISON));
	RegisterInfix(TokenType::GREATER_EQUAL,  BinaryOpParselet::New(Op::GREATER_EQUAL, Precedence::COMPARISON));

	// Logical operators
	RegisterInfix(TokenType::AND,  BinaryOpParselet::New(Op::AND, Precedence::AND));
	RegisterInfix(TokenType::OR,  BinaryOpParselet::New(Op::OR, Precedence::OR));

	// Call and index operators
	RegisterInfix(TokenType::LPAREN,  CallParselet::New());
	RegisterInfix(TokenType::LBRACKET,  IndexParselet::New());
	RegisterInfix(TokenType::DOT,  MemberParselet::New());
}
void ParserStorage::RegisterPrefix(TokenType type, PrefixParselet parselet) {
	_prefixParselets[type] = parselet;
}
void ParserStorage::RegisterInfix(TokenType type, InfixParselet parselet) {
	_infixParselets[type] = parselet;
}
void ParserStorage::Init(String source) {
	_lexer = Lexer(source);
	_hadError = Boolean(false);
	_errors.Clear();
	Advance();  // Prime the pump with the first token
}
void ParserStorage::Advance() {
	do {
		_current = _lexer.NextToken();
	} while (_current.Type == TokenType::COMMENT);
}
Boolean ParserStorage::Check(TokenType type) {
	return _current.Type == type;
}
Boolean ParserStorage::Match(TokenType type) {
	if (_current.Type == type) {
		Advance();
		return Boolean(true);
	}
	return Boolean(false);
}
Token ParserStorage::Consume() {
	Token tok = _current;
	Advance();
	return tok;
}
Token ParserStorage::Expect(TokenType type, String errorMessage) {
	if (_current.Type == type) {
		Token tok = _current;
		Advance();
		return tok;
	}
	ReportError(errorMessage);
	return Token(TokenType::ERROR, "", _current.Line, _current.Column);
}
Precedence ParserStorage::GetPrecedence() {
	InfixParselet parselet = nullptr;
	if (_infixParselets.TryGetValue(_current.Type, &parselet)) {
		return parselet.Prec();
	}
	return Precedence::NONE;
}
Boolean ParserStorage::CanStartExpression(TokenType type) {
	return type == TokenType::NUMBER
		|| type == TokenType::STRING
		|| type == TokenType::IDENTIFIER
		|| type == TokenType::LPAREN
		|| type == TokenType::LBRACKET
		|| type == TokenType::LBRACE
		|| type == TokenType::MINUS
		|| type == TokenType::NOT;
}
Boolean ParserStorage::CanStartCallArgument(TokenType type) {
	return type == TokenType::NUMBER
		|| type == TokenType::STRING
		|| type == TokenType::IDENTIFIER
		|| type == TokenType::LPAREN
		|| type == TokenType::LBRACKET
		|| type == TokenType::LBRACE
		|| type == TokenType::MINUS
		|| type == TokenType::NOT;
}
ASTNode ParserStorage::ParseExpression(Precedence minPrecedence) {
	Parser _this(std::static_pointer_cast<ParserStorage>(shared_from_this()));
	Token token = _current;
	Advance();

	// Look up the prefix parselet for this token
	PrefixParselet prefix = nullptr;
	if (!_prefixParselets.TryGetValue(token.Type, &prefix)) {
		ReportError(Interp("Unexpected token: {}", token.Text));
		return  NumberNode::New(0);
	}

	ASTNode left = prefix.Parse(_this, token);

	// Continue parsing infix expressions while precedence allows
	while ((Int32)minPrecedence < (Int32)GetPrecedence()) {
		token = _current;
		Advance();

		InfixParselet infix = nullptr;
		if (_infixParselets.TryGetValue(token.Type, &infix)) {
			left = infix.Parse(_this, left, token);
		}
	}

	return left;
}
ASTNode ParserStorage::ParseExpression() {
	return ParseExpression(Precedence::NONE);
}
ASTNode ParserStorage::ParseExpressionFrom(ASTNode left) {
	Parser _this(std::static_pointer_cast<ParserStorage>(shared_from_this()));
	while ((Int32)Precedence::NONE < (Int32)GetPrecedence()) {
		Token token = _current;
		Advance();
		InfixParselet infix = nullptr;
		if (_infixParselets.TryGetValue(token.Type, &infix)) {
			left = infix.Parse(_this, left, token);
		}
	}
	return left;
}
ASTNode ParserStorage::ParseSimpleStatement() {
	// Grammar for relevant rules:
	//   callStatement : expression '(' argList ')' | expression argList
	//   assignmentStatement : lvalue '=' expression
	//   expressionStatement : expression
	//
	// The key distinction:
	// - If we see IDENTIFIER followed by '=', it's an assignment
	// - If we see IDENTIFIER followed by '(' or an infix operator, parse as expression
	// - If we see IDENTIFIER followed by something that can START a call argument
	//   (number, string, identifier, '[', '{', '-', 'not' but NOT '('), it's a no-parens call
	// - Otherwise, parse as expression

	if (_current.Type == TokenType::IDENTIFIER) {
		Token identToken = _current;
		Advance();

		// Check for assignment: identifier '=' expression
		if (_current.Type == TokenType::ASSIGN) {
			Advance(); // consume '='
			ASTNode value = ParseExpression();
			return  AssignmentNode::New(identToken.Text, value);
		}

		// Check for no-parens call statement: identifier argList
		// where argList starts with a token that can begin an expression
		// (but NOT '(' which would be handled as func(args) by expression parsing).
		// IMPORTANT: Whitespace is required between identifier and argument to
		// distinguish "print [1,2,3]" (call) from "list[0]" (index expression).
		if (_current.AfterSpace && CanStartCallArgument(_current.Type)) {
			// This is a call statement like "print 42" or "print x, y"
			List<ASTNode> args =  List<ASTNode>::New();
			args.Add(ParseExpression());
			while (Match(TokenType::COMMA)) {
				args.Add(ParseExpression());
			}
			return  CallNode::New(identToken.Text, args);
		}

		// Otherwise, it's either:
		// - A function call with parens: foo(args) - handled by expression parser
		// - An expression with operators: x + y - handled by expression parser
		// - Just a plain identifier: x
		// Continue parsing as expression with the identifier as the left operand
		ASTNode left =  IdentifierNode::New(identToken.Text);
		return ParseExpressionFrom(left);
	}

	// Not an identifier - parse as expression statement
	return ParseExpression();
}
ASTNode ParserStorage::ParseStatement() {
	// Skip leading blank lines
	while (_current.Type == TokenType::EOL) {
		Advance();
	}

	if (_current.Type == TokenType::END_OF_INPUT) {
		return nullptr;
	}

	return ParseSimpleStatement();
}
List<ASTNode> ParserStorage::ParseProgram() {
	List<ASTNode> statements =  List<ASTNode>::New();

	while (_current.Type != TokenType::END_OF_INPUT) {
		// Skip blank lines
		while (_current.Type == TokenType::EOL) {
			Advance();
		}
		if (_current.Type == TokenType::END_OF_INPUT) break;

		ASTNode stmt = ParseSimpleStatement();
		if (!IsNull(stmt)) {
			statements.Add(stmt);
		}

		// Expect EOL or EOF after statement
		if (_current.Type != TokenType::EOL && _current.Type != TokenType::END_OF_INPUT) {
			ReportError(Interp("Expected end of line, got: {}", _current.Text));
			// Try to recover by skipping to next line
			while (_current.Type != TokenType::EOL && _current.Type != TokenType::END_OF_INPUT) {
				Advance();
			}
		}
	}

	return statements;
}
ASTNode ParserStorage::Parse(String source) {
	Init(source);

	// Skip leading blank lines (EOL tokens)
	while (_current.Type == TokenType::EOL) {
		Advance();
	}

	// Parse a single statement
	ASTNode result = ParseStatement();

	// Check for trailing tokens (except END_OF_INPUT or EOL)
	if (_current.Type != TokenType::END_OF_INPUT && _current.Type != TokenType::EOL) {
		ReportError(Interp("Unexpected token after statement: {}", _current.Text));
	}

	return result;
}
void ParserStorage::ReportError(String message) {
	_hadError = Boolean(true);
	String error = Interp("Parse error at line {}, column {}: {}", _current.Line, _current.Column, message);
	_errors.Add(error);
	IOHelper::Print(error);
}
Boolean ParserStorage::HadError() {
	return _hadError;
}
List<String> ParserStorage::GetErrors() {
	return _errors;
}


} // end of namespace MiniScript
