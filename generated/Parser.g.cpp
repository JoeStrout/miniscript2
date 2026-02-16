// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Parser.cs

#include "Parser.g.h"
#include "AST.g.h"
#include "IOHelper.g.h"
#include "StringUtils.g.h"

namespace MiniScript {


ParserStorage::ParserStorage() {
	Errors = ErrorPool();
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
	RegisterPrefix(TokenType::MINUS,  UnaryOpParselet::New(Op::MINUS, Precedence::UNARY_MINUS));
	RegisterPrefix(TokenType::NOT,  UnaryOpParselet::New(Op::NOT, Precedence::UNARY_MINUS));
	RegisterPrefix(TokenType::ADDRESS_OF,  UnaryOpParselet::New(Op::ADDRESS_OF, Precedence::ADDRESS_OF));
	RegisterPrefix(TokenType::NEW,  UnaryOpParselet::New(Op::NEW, Precedence::UNARY_MINUS));

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
	Errors.Clear();
	_lexer.Errors = Errors;  // Share the same error pool
	Advance();  // Prime the pump with the first token
}
void ParserStorage::Advance() {
	_previousType = _current.Type;
	do {
		_current = _lexer.NextToken();
	} while (_current.Type == TokenType::COMMENT
		|| (_current.Type == TokenType::EOL && AllowsLineContinuation(_previousType)));
}
Boolean ParserStorage::AllowsLineContinuation(TokenType type) {
	return type == TokenType::COMMA
		|| type == TokenType::LPAREN
		|| type == TokenType::LBRACKET
		|| type == TokenType::LBRACE
		|| type == TokenType::PLUS
		|| type == TokenType::MINUS
		|| type == TokenType::TIMES
		|| type == TokenType::DIVIDE
		|| type == TokenType::MOD
		|| type == TokenType::CARET
		|| type == TokenType::EQUALS
		|| type == TokenType::NOT_EQUAL
		|| type == TokenType::LESS_THAN
		|| type == TokenType::GREATER_THAN
		|| type == TokenType::LESS_EQUAL
		|| type == TokenType::GREATER_EQUAL
		|| type == TokenType::AND
		|| type == TokenType::OR
		|| type == TokenType::COLON
		|| type == TokenType::ASSIGN;
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
		|| type == TokenType::ADDRESS_OF
		|| type == TokenType::NOT
		|| type == TokenType::NEW
		|| type == TokenType::FUNCTION;
}
ASTNode ParserStorage::ParseExpression(Precedence minPrecedence) {
	Parser _this(std::static_pointer_cast<ParserStorage>(shared_from_this()));
	Token token = _current;
	Advance();

	// Special case: function expression (spans multiple lines)
	if (token.Type == TokenType::FUNCTION) {
		return ParseFunctionExpression();
	}

	// Look up the prefix parselet for this token
	PrefixParselet prefix = nullptr;
	if (!_prefixParselets.TryGetValue(token.Type, &prefix)) {
		ReportError(Interp("Unexpected token: {}", TokenDescription(token)));
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
	// Check for break statement
	if (_current.Type == TokenType::BREAK) {
		Advance();  // consume BREAK
		return  BreakNode::New();
	}

	// Check for continue statement
	if (_current.Type == TokenType::CONTINUE) {
		Advance();  // consume CONTINUE
		return  ContinueNode::New();
	}

	// Check for return statement
	if (_current.Type == TokenType::RETURN) {
		Advance();  // consume RETURN
		// Parse optional return value (if something that can start an expression follows)
		ASTNode returnValue = nullptr;
		if (_current.Type != TokenType::EOL && _current.Type != TokenType::END_OF_INPUT
			&& _current.Type != TokenType::ELSE && CanStartExpression(_current.Type)) {
			returnValue = ParseExpression();
		}
		return  ReturnNode::New(returnValue);
	}

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
		if (_current.AfterSpace && CanStartExpression(_current.Type)) {
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
		ASTNode expr = ParseExpressionFrom(left);

		// Check for indexed assignment: expr[index] = value
		IndexNode idxNode = As<IndexNode, IndexNodeStorage>(expr);
		if (!IsNull(idxNode) && _current.Type == TokenType::ASSIGN) {
			Advance(); // consume '='
			ASTNode value = ParseExpression();
			return  IndexedAssignmentNode::New(idxNode.Target(), idxNode.Index(), value);
		}

		// Check for member assignment: expr.member = value
		MemberNode memNode = As<MemberNode, MemberNodeStorage>(expr);
		if (!IsNull(memNode) && _current.Type == TokenType::ASSIGN) {
			Advance(); // consume '='
			ASTNode value = ParseExpression();
			ASTNode index =  StringNode::New(memNode.Member());
			return  IndexedAssignmentNode::New(memNode.Target(), index, value);
		}

		// Check for no-parens call on an expression result, e.g. funcs[0] 10
		if (_current.AfterSpace && CanStartExpression(_current.Type)) {
			List<ASTNode> args =  List<ASTNode>::New();
			args.Add(ParseExpression());
			while (Match(TokenType::COMMA)) {
				args.Add(ParseExpression());
			}
			return  ExprCallNode::New(expr, args);
		}

		return expr;
	}

	// Not an identifier - parse as expression statement
	ASTNode expr2 = ParseExpression();
	IndexNode idxNode2 = As<IndexNode, IndexNodeStorage>(expr2);
	if (!IsNull(idxNode2) && _current.Type == TokenType::ASSIGN) {
		Advance(); // consume '='
		ASTNode value = ParseExpression();
		return  IndexedAssignmentNode::New(idxNode2.Target(), idxNode2.Index(), value);
	}
	MemberNode memNode2 = As<MemberNode, MemberNodeStorage>(expr2);
	if (!IsNull(memNode2) && _current.Type == TokenType::ASSIGN) {
		Advance(); // consume '='
		ASTNode value = ParseExpression();
		ASTNode index =  StringNode::New(memNode2.Member());
		return  IndexedAssignmentNode::New(memNode2.Target(), index, value);
	}
	return expr2;
}
Boolean ParserStorage::IsBlockTerminator(TokenType t1, TokenType t2) {
	return _current.Type == TokenType::END_OF_INPUT
		|| _current.Type == t1
		|| _current.Type == t2;
}
List<ASTNode> ParserStorage::ParseBlock(TokenType terminator1, TokenType terminator2) {
	List<ASTNode> body =  List<ASTNode>::New();

	while (Boolean(true)) {
		// Skip blank lines
		while (_current.Type == TokenType::EOL) {
			Advance();
		}

		// Check for terminator or end of input
		if (IsBlockTerminator(terminator1, terminator2)) {
			break;
		}

		// Parse a statement
		ASTNode stmt = ParseStatement();
		if (!IsNull(stmt)) {
			body.Add(stmt);
		}

		// Expect EOL after statement
		if (_current.Type != TokenType::EOL && !IsBlockTerminator(terminator1, terminator2)) {
			ReportError(Interp("Expected end of line, got: {}", _current.Text));
			// Try to recover by skipping to next line
			while (_current.Type != TokenType::EOL && _current.Type != TokenType::END_OF_INPUT) {
				Advance();
			}
		}
	}

	return body;
}
void ParserStorage::RequireEndKeyword(TokenType keyword, String keywordName) {
	if (_current.Type != TokenType::END) {
		ReportError(Interp("Expected 'end {}'", keywordName));
		return;
	}
	Advance();  // consume END
	if (_current.Type != keyword) {
		ReportError(GotExpected(StringUtils::Format("Keyword({0})", keywordName)));
		return;
	}
	Advance();  // consume keyword
}
ASTNode ParserStorage::ParseIfStatement() {
	ASTNode condition = ParseExpression();

	// Expect THEN
	if (_current.Type != TokenType::THEN) {
		ReportError(GotExpected("Keyword(then)"));
		return  IfNode::New(condition,  List<ASTNode>::New(),  List<ASTNode>::New());
	}
	Advance();  // consume THEN

	// Check if block or single-line form
	if (_current.Type == TokenType::EOL || _current.Type == TokenType::END_OF_INPUT) {
		// Block form
		List<ASTNode> thenBody = ParseBlock(TokenType::ELSE, TokenType::END);
		List<ASTNode> elseBody = ParseElseClause();
		if (_current.Type == TokenType::END) {
			RequireEndKeyword(TokenType::IF, "if");
		}
		return  IfNode::New(condition, thenBody, elseBody);
	} else {
		// Single-line form
		return ParseSingleLineIfBody(condition);
	}
}
List<ASTNode> ParserStorage::ParseElseClause() {
	List<ASTNode> elseBody =  List<ASTNode>::New();

	if (_current.Type != TokenType::ELSE) {
		return elseBody;
	}
	Advance();  // consume ELSE

	if (_current.Type == TokenType::IF) {
		// else if - parse as nested if (which handles its own "end if")
		Advance();  // consume IF
		elseBody.Add(ParseIfStatement());
	} else {
		// plain else - expect EOL then body
		if (_current.Type != TokenType::EOL && _current.Type != TokenType::END_OF_INPUT) {
			ReportError(Interp("Expected end of line after 'else', got: {}", _current.Text));
		}
		elseBody = ParseBlock(TokenType::END, TokenType::END);  // only END terminates
	}

	return elseBody;
}
ASTNode ParserStorage::ParseSingleLineStatement() {
	if (_current.Type == TokenType::IF) {
		Advance();  // consume IF
		return ParseIfStatement();
	}
	return ParseSimpleStatement();
}
ASTNode ParserStorage::ParseSingleLineIfBody(ASTNode condition) {
	List<ASTNode> thenBody =  List<ASTNode>::New();
	ASTNode thenStmt = ParseSingleLineStatement();
	if (!IsNull(thenStmt)) {
		thenBody.Add(thenStmt);
	}

	List<ASTNode> elseBody =  List<ASTNode>::New();
	if (_current.Type == TokenType::ELSE) {
		Advance();  // consume ELSE
		ASTNode elseStmt = ParseSingleLineStatement();
		if (!IsNull(elseStmt)) {
			elseBody.Add(elseStmt);
		}
	}

	return  IfNode::New(condition, thenBody, elseBody);
}
ASTNode ParserStorage::ParseWhileStatement() {
	ASTNode condition = ParseExpression();

	// Expect EOL after condition
	if (_current.Type != TokenType::EOL && _current.Type != TokenType::END_OF_INPUT) {
		ReportError(Interp("Expected end of line after while condition, got: {}", _current.Text));
	}

	List<ASTNode> body = ParseBlock(TokenType::END, TokenType::END);
	RequireEndKeyword(TokenType::WHILE, "while");

	return  WhileNode::New(condition, body);
}
ASTNode ParserStorage::ParseForStatement() {
	// Expect identifier (loop variable)
	if (_current.Type != TokenType::IDENTIFIER) {
		ReportError(Interp("Expected identifier after 'for', got: {}", _current.Text));
		return  ForNode::New("_",  NumberNode::New(0),  List<ASTNode>::New());
	}
	String varName = _current.Text;
	Advance();  // consume identifier

	// Expect IN
	if (_current.Type != TokenType::IN) {
		ReportError(GotExpected("Keyword(in)"));
	} else {
		Advance();  // consume IN
	}

	// Parse iterable expression
	ASTNode iterable = ParseExpression();

	// Expect EOL after expression
	if (_current.Type != TokenType::EOL && _current.Type != TokenType::END_OF_INPUT) {
		ReportError(Interp("Expected end of line after for expression, got: {}", _current.Text));
	}

	List<ASTNode> body = ParseBlock(TokenType::END, TokenType::END);
	RequireEndKeyword(TokenType::FOR, "for");

	return  ForNode::New(varName, iterable, body);
}
ASTNode ParserStorage::ParseFunctionExpression() {
	// Parse parameter list (parentheses optional for no-param functions)
	List<String> paramNames =  List<String>::New();
	List<ASTNode> paramDefaults =  List<ASTNode>::New();
	if (_current.Type == TokenType::LPAREN) {
		Advance();  // consume '('
		if (_current.Type != TokenType::RPAREN) {
			do {
				Token paramToken = Expect(TokenType::IDENTIFIER, "Expected parameter name");
				if (paramToken.Type != TokenType::ERROR) {
					paramNames.Add(paramToken.Text);
					if (Match(TokenType::ASSIGN)) {
						// Parse default value expression
						ASTNode defaultExpr = ParseExpression(Precedence::NONE);
						paramDefaults.Add(defaultExpr);
					} else {
						paramDefaults.Add(nullptr);
					}
				}
			} while (Match(TokenType::COMMA));
		}
		Expect(TokenType::RPAREN, "Expected ')' after parameters");
	}

	// Expect EOL after parameter list
	if (_current.Type != TokenType::EOL && _current.Type != TokenType::END_OF_INPUT) {
		ReportError(Interp("Expected end of line after function parameters, got: {}", _current.Text));
	}

	// Parse body until "end function"
	List<ASTNode> body = ParseBlock(TokenType::END, TokenType::END);
	RequireEndKeyword(TokenType::FUNCTION, "function");

	return  FunctionNode::New(paramNames, paramDefaults, body);
}
ASTNode ParserStorage::ParseStatement() {
	// Skip leading blank lines
	while (_current.Type == TokenType::EOL) {
		Advance();
	}

	if (_current.Type == TokenType::END_OF_INPUT) {
		return nullptr;
	}

	// Check for block statements
	if (_current.Type == TokenType::WHILE) {
		Advance();  // consume WHILE
		return ParseWhileStatement();
	}

	if (_current.Type == TokenType::FOR) {
		Advance();  // consume FOR
		return ParseForStatement();
	}

	if (_current.Type == TokenType::IF) {
		Advance();  // consume IF
		return ParseIfStatement();
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

		ASTNode stmt = ParseStatement();
		if (!IsNull(stmt)) {
			statements.Add(stmt);
		}

		// Expect EOL or EOF after statement
		// (block statements like while handle their own EOL consumption)
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
String ParserStorage::TokenDescription(Token tok) {
	if (tok.Type == TokenType::EOL || tok.Type == TokenType::END_OF_INPUT) return "EOL";
	if (tok.Type == TokenType::NUMBER) return tok.Text;
	if (tok.Type == TokenType::STRING) return StringUtils::Format("\"{0}\"", tok.Text);
	if (tok.Type == TokenType::IDENTIFIER) return tok.Text;
	// Keywords
	if (tok.Type == TokenType::WHILE) return "Keyword(while)";
	if (tok.Type == TokenType::FOR) return "Keyword(for)";
	if (tok.Type == TokenType::IN) return "Keyword(in)";
	if (tok.Type == TokenType::IF) return "Keyword(if)";
	if (tok.Type == TokenType::THEN) return "Keyword(then)";
	if (tok.Type == TokenType::ELSE) return "Keyword(else)";
	if (tok.Type == TokenType::END) return "Keyword(end)";
	if (tok.Type == TokenType::BREAK) return "Keyword(break)";
	if (tok.Type == TokenType::CONTINUE) return "Keyword(continue)";
	if (tok.Type == TokenType::FUNCTION) return "Keyword(function)";
	if (tok.Type == TokenType::RETURN) return "Keyword(return)";
	if (tok.Type == TokenType::AND) return "Keyword(and)";
	if (tok.Type == TokenType::OR) return "Keyword(or)";
	if (tok.Type == TokenType::NOT) return "Keyword(not)";
	if (tok.Type == TokenType::NEW) return "Keyword(new)";
	return tok.Text;
}
String ParserStorage::GotExpected(String expected) {
	return StringUtils::Format("got {0} where {1} is required", TokenDescription(_current), expected);
}
void ParserStorage::ReportError(String message) {
	Errors.Add(StringUtils::Format("Compiler Error: {0} [line {1}]", message, _current.Line));
}
Boolean ParserStorage::HadError() {
	return Errors.HasError();
}
List<String> ParserStorage::GetErrors() {
	return Errors.GetErrors();
}


} // end of namespace MiniScript
