// Parser.cs - Pratt parser for MiniScript expressions
// Uses parselets to handle operator precedence and associativity.

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
		RegisterPrefix(TokenType.MINUS, new UnaryOpParselet(Op.MINUS, Precedence.UNARY));
		RegisterPrefix(TokenType.NOT, new UnaryOpParselet(Op.NOT, Precedence.UNARY));

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

	// Advance to the next token
	private void Advance() {
		_current = _lexer.NextToken();
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

	// Parse an expression with the given minimum precedence
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

	// Parse a complete expression from source code
	public ASTNode Parse(String source) {
		Init(source);
		ASTNode result = ParseExpression();

		// Check for trailing tokens (except END_OF_INPUT)
		if (_current.Type != TokenType.END_OF_INPUT && _current.Type != TokenType.EOL) {
			ReportError($"Unexpected token after expression: {_current.Text}");
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
