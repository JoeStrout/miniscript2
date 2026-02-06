// Parser.cs - Pratt parser for MiniScript
// Uses parselets to handle operator precedence and associativity.
// Structure follows the grammar in miniscript.g4:
//   program : (eol | statement)* EOF
//   statement : simpleStatement eol
//   simpleStatement : callStatement | assignmentStatement | expressionStatement
//   callStatement : expression '(' argList ')' | expression argList
//   expressionStatement : expression

using System;
using System.Collections.Generic;
// H: #include "LangConstants.g.h"
// H: #include "Lexer.g.h"
// H: #include "Parselet.g.h"
// CPP: #include "AST.g.h"
// CPP: #include "IOHelper.g.h"


namespace MiniScript {

// Parser: the main parsing engine.
// Uses a Pratt parser algorithm with parselets to handle operator precedence.
public class Parser : IParser {
	private Lexer _lexer;
	private Token _current;
	private Boolean _hadError;
	private List<String> _errors;

	// Parselet tables - indexed by TokenType
	private Dictionary<TokenType, PrefixParselet> _prefixParselets;
	private Dictionary<TokenType, InfixParselet> _infixParselets;

	public Parser() {
		_errors = new List<String>();
		_prefixParselets = new Dictionary<TokenType, PrefixParselet>();
		_infixParselets = new Dictionary<TokenType, InfixParselet>();

		RegisterParselets();
	}

	// Register all parselets
	private void RegisterParselets() {
		// Prefix parselets (tokens that can start an expression)
		RegisterPrefix(TokenType.NUMBER, new NumberParselet());
		RegisterPrefix(TokenType.STRING, new StringParselet());
		RegisterPrefix(TokenType.IDENTIFIER, new IdentifierParselet());
		RegisterPrefix(TokenType.LPAREN, new GroupParselet());
		RegisterPrefix(TokenType.LBRACKET, new ListParselet());
		RegisterPrefix(TokenType.LBRACE, new MapParselet());

		// Unary operators
		RegisterPrefix(TokenType.MINUS, new UnaryOpParselet(Op.MINUS, Precedence.UNARY_MINUS));
		RegisterPrefix(TokenType.NOT, new UnaryOpParselet(Op.NOT, Precedence.UNARY_MINUS));
		RegisterPrefix(TokenType.ADDRESS_OF, new UnaryOpParselet(Op.ADDRESS_OF, Precedence.ADDRESS_OF));

		// Binary operators
		RegisterInfix(TokenType.PLUS, new BinaryOpParselet(Op.PLUS, Precedence.SUM));
		RegisterInfix(TokenType.MINUS, new BinaryOpParselet(Op.MINUS, Precedence.SUM));
		RegisterInfix(TokenType.TIMES, new BinaryOpParselet(Op.TIMES, Precedence.PRODUCT));
		RegisterInfix(TokenType.DIVIDE, new BinaryOpParselet(Op.DIVIDE, Precedence.PRODUCT));
		RegisterInfix(TokenType.MOD, new BinaryOpParselet(Op.MOD, Precedence.PRODUCT));
		RegisterInfix(TokenType.CARET, new BinaryOpParselet(Op.POWER, Precedence.POWER, true)); // right-assoc

		// Comparison operators
		RegisterInfix(TokenType.EQUALS, new BinaryOpParselet(Op.EQUALS, Precedence.EQUALITY));
		RegisterInfix(TokenType.NOT_EQUAL, new BinaryOpParselet(Op.NOT_EQUAL, Precedence.EQUALITY));
		RegisterInfix(TokenType.LESS_THAN, new BinaryOpParselet(Op.LESS_THAN, Precedence.COMPARISON));
		RegisterInfix(TokenType.GREATER_THAN, new BinaryOpParselet(Op.GREATER_THAN, Precedence.COMPARISON));
		RegisterInfix(TokenType.LESS_EQUAL, new BinaryOpParselet(Op.LESS_EQUAL, Precedence.COMPARISON));
		RegisterInfix(TokenType.GREATER_EQUAL, new BinaryOpParselet(Op.GREATER_EQUAL, Precedence.COMPARISON));

		// Logical operators
		RegisterInfix(TokenType.AND, new BinaryOpParselet(Op.AND, Precedence.AND));
		RegisterInfix(TokenType.OR, new BinaryOpParselet(Op.OR, Precedence.OR));

		// Call and index operators
		RegisterInfix(TokenType.LPAREN, new CallParselet());
		RegisterInfix(TokenType.LBRACKET, new IndexParselet());
		RegisterInfix(TokenType.DOT, new MemberParselet());
	}

	private void RegisterPrefix(TokenType type, PrefixParselet parselet) {
		_prefixParselets[type] = parselet;
	}

	private void RegisterInfix(TokenType type, InfixParselet parselet) {
		_infixParselets[type] = parselet;
	}

	// Initialize the parser with source code
	public void Init(String source) {
		_lexer = new Lexer(source);
		_hadError = false;
		_errors.Clear();
		Advance();  // Prime the pump with the first token
	}

	// Advance to the next token, skipping comments
	private void Advance() {
		do {
			_current = _lexer.NextToken();
		} while (_current.Type == TokenType.COMMENT);
	}

	// Check if current token matches the given type (without consuming)
	public Boolean Check(TokenType type) {
		return _current.Type == type;
	}

	// Check if current token matches and consume it if so
	public Boolean Match(TokenType type) {
		if (_current.Type == type) {
			Advance();
			return true;
		}
		return false;
	}

	// Consume the current token (used by parselets)
	public Token Consume() {
		Token tok = _current;
		Advance();
		return tok;
	}

	// Expect a specific token type, report error if not found
	public Token Expect(TokenType type, String errorMessage) {
		if (_current.Type == type) {
			Token tok = _current;
			Advance();
			return tok;
		}
		ReportError(errorMessage);
		return new Token(TokenType.ERROR, "", _current.Line, _current.Column);
	}

	// Get the precedence of the infix parselet for the current token
	private Precedence GetPrecedence() {
		InfixParselet parselet = null;
		if (_infixParselets.TryGetValue(_current.Type, out parselet)) {
			return parselet.Prec;
		}
		return Precedence.NONE;
	}

	// Check if a token type can start an expression (used as prefix)
	public Boolean CanStartExpression(TokenType type) {
		return type == TokenType.NUMBER
			|| type == TokenType.STRING
			|| type == TokenType.IDENTIFIER
			|| type == TokenType.LPAREN
			|| type == TokenType.LBRACKET
			|| type == TokenType.LBRACE
			|| type == TokenType.MINUS
			|| type == TokenType.ADDRESS_OF
			|| type == TokenType.NOT;
	}

	// Check if a token type can start an argument in a no-parens call statement.
	// Note: The parser uses Token.AfterSpace to distinguish between:
	//   "print [1,2,3]" (call with list arg - has whitespace before '[')
	//   "list[0]" (index expression - no whitespace before '[')
	//   "print (2+3)*4" (call with grouped expr - has whitespace before '(')
	//   "func(args)" (call syntax via CallParselet - NO whitespace before '(')
	private Boolean CanStartCallArgument(TokenType type) {
		return type == TokenType.NUMBER
			|| type == TokenType.STRING
			|| type == TokenType.IDENTIFIER
			|| type == TokenType.LPAREN
			|| type == TokenType.LBRACKET
			|| type == TokenType.LBRACE
			|| type == TokenType.MINUS
			|| type == TokenType.ADDRESS_OF
			|| type == TokenType.NOT;
	}

	// Parse an expression with the given minimum precedence (Pratt parser core)
	public ASTNode ParseExpression(Precedence minPrecedence) {
		Token token = _current;
		Advance();

		// Look up the prefix parselet for this token
		PrefixParselet prefix = null;
		if (!_prefixParselets.TryGetValue(token.Type, out prefix)) {
			ReportError($"Unexpected token: {token.Text}");
			return new NumberNode(0);
		}

		ASTNode left = prefix.Parse(this, token);

		// Continue parsing infix expressions while precedence allows
		while ((Int32)minPrecedence < (Int32)GetPrecedence()) {
			token = _current;
			Advance();

			InfixParselet infix = null;
			if (_infixParselets.TryGetValue(token.Type, out infix)) {
				left = infix.Parse(this, left, token);
			}
		}

		return left;
	}

	// Parse an expression (convenience method with default precedence)
	public ASTNode ParseExpression() {
		return ParseExpression(Precedence.NONE);
	}

	// Continue parsing an expression given a starting left operand.
	// Used when we've already consumed a token (like an identifier) and need
	// to continue with any infix operators that follow.
	private ASTNode ParseExpressionFrom(ASTNode left) {
		while ((Int32)Precedence.NONE < (Int32)GetPrecedence()) {
			Token token = _current;
			Advance();
			InfixParselet infix = null;
			if (_infixParselets.TryGetValue(token.Type, out infix)) {
				left = infix.Parse(this, left, token);
			}
		}
		return left;
	}

	// Parse a simple statement (grammar: simpleStatement)
	// Handles: callStatement, assignmentStatement, expressionStatement
	private ASTNode ParseSimpleStatement() {
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

		if (_current.Type == TokenType.IDENTIFIER) {
			Token identToken = _current;
			Advance();

			// Check for assignment: identifier '=' expression
			if (_current.Type == TokenType.ASSIGN) {
				Advance(); // consume '='
				ASTNode value = ParseExpression();
				return new AssignmentNode(identToken.Text, value);
			}

			// Check for no-parens call statement: identifier argList
			// where argList starts with a token that can begin an expression
			// (but NOT '(' which would be handled as func(args) by expression parsing).
			// IMPORTANT: Whitespace is required between identifier and argument to
			// distinguish "print [1,2,3]" (call) from "list[0]" (index expression).
			if (_current.AfterSpace && CanStartCallArgument(_current.Type)) {
				// This is a call statement like "print 42" or "print x, y"
				List<ASTNode> args = new List<ASTNode>();
				args.Add(ParseExpression());
				while (Match(TokenType.COMMA)) {
					args.Add(ParseExpression());
				}
				return new CallNode(identToken.Text, args);
			}

			// Otherwise, it's either:
			// - A function call with parens: foo(args) - handled by expression parser
			// - An expression with operators: x + y - handled by expression parser
			// - Just a plain identifier: x
			// Continue parsing as expression with the identifier as the left operand
			ASTNode left = new IdentifierNode(identToken.Text);
			return ParseExpressionFrom(left);
		}

		// Not an identifier - parse as expression statement
		return ParseExpression();
	}

	// Parse a statement (grammar: statement : simpleStatement eol)
	// For now, we only handle simpleStatement (no block statements yet)
	public ASTNode ParseStatement() {
		// Skip leading blank lines
		while (_current.Type == TokenType.EOL) {
			Advance();
		}

		if (_current.Type == TokenType.END_OF_INPUT) {
			return null;
		}

		return ParseSimpleStatement();
	}

	// Parse a program (grammar: program : (eol | statement)* EOF)
	// Returns a list of statement AST nodes
	public List<ASTNode> ParseProgram() {
		List<ASTNode> statements = new List<ASTNode>();

		while (_current.Type != TokenType.END_OF_INPUT) {
			// Skip blank lines
			while (_current.Type == TokenType.EOL) {
				Advance();
			}
			if (_current.Type == TokenType.END_OF_INPUT) break;

			ASTNode stmt = ParseSimpleStatement();
			if (stmt != null) {
				statements.Add(stmt);
			}

			// Expect EOL or EOF after statement
			if (_current.Type != TokenType.EOL && _current.Type != TokenType.END_OF_INPUT) {
				ReportError($"Expected end of line, got: {_current.Text}");
				// Try to recover by skipping to next line
				while (_current.Type != TokenType.EOL && _current.Type != TokenType.END_OF_INPUT) {
					Advance();
				}
			}
		}

		return statements;
	}

	// Parse a complete source string (convenience method)
	// For single expressions/statements, returns the AST node
	public ASTNode Parse(String source) {
		Init(source);

		// Skip leading blank lines (EOL tokens)
		while (_current.Type == TokenType.EOL) {
			Advance();
		}

		// Parse a single statement
		ASTNode result = ParseStatement();

		// Check for trailing tokens (except END_OF_INPUT or EOL)
		if (_current.Type != TokenType.END_OF_INPUT && _current.Type != TokenType.EOL) {
			ReportError($"Unexpected token after statement: {_current.Text}");
		}

		return result;
	}

	// Report an error
	public void ReportError(String message) {
		_hadError = true;
		String error = $"Parse error at line {_current.Line}, column {_current.Column}: {message}";
		_errors.Add(error);
		IOHelper.Print(error);
	}

	// Check if any errors occurred
	public Boolean HadError() {
		return _hadError;
	}

	// Get the list of errors
	public List<String> GetErrors() {
		return _errors;
	}
}

}
