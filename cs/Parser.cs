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
// H: #include "ErrorPool.g.h"
// CPP: #include "AST.g.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "StringUtils.g.h"


namespace MiniScript {

// Parser: the main parsing engine.
// Uses a Pratt parser algorithm with parselets to handle operator precedence.
public class Parser : IParser {
	private Lexer _lexer;
	private Token _current;
	private TokenType _previousType;
	public ErrorPool Errors;

	// Parselet tables - indexed by TokenType
	private Dictionary<TokenType, PrefixParselet> _prefixParselets;
	private Dictionary<TokenType, InfixParselet> _infixParselets;

	public Parser() {
		Errors = new ErrorPool();
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
		Errors.Clear();
		_lexer.Errors = Errors;  // Share the same error pool
		Advance();  // Prime the pump with the first token
	}

	// Advance to the next token, skipping comments and line continuations.
	// A line continuation is an EOL that follows a token which naturally
	// expects more input (comma, open bracket/paren/brace, binary operator).
	private void Advance() {
		_previousType = _current.Type;
		do {
			_current = _lexer.NextToken();
		} while (_current.Type == TokenType.COMMENT
			|| (_current.Type == TokenType.EOL && AllowsLineContinuation(_previousType)));
	}

	// Return true if the given token type allows a line continuation after it.
	// That is, an EOL following this token should be silently ignored.
	private static Boolean AllowsLineContinuation(TokenType type) {
		return type == TokenType.COMMA
			|| type == TokenType.LPAREN
			|| type == TokenType.LBRACKET
			|| type == TokenType.LBRACE
			|| type == TokenType.PLUS
			|| type == TokenType.MINUS
			|| type == TokenType.TIMES
			|| type == TokenType.DIVIDE
			|| type == TokenType.MOD
			|| type == TokenType.CARET
			|| type == TokenType.EQUALS
			|| type == TokenType.NOT_EQUAL
			|| type == TokenType.LESS_THAN
			|| type == TokenType.GREATER_THAN
			|| type == TokenType.LESS_EQUAL
			|| type == TokenType.GREATER_EQUAL
			|| type == TokenType.AND
			|| type == TokenType.OR
			|| type == TokenType.COLON
			|| type == TokenType.ASSIGN;
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

	// Check if a token type can start an expression
	public Boolean CanStartExpression(TokenType type) {
		return type == TokenType.NUMBER
			|| type == TokenType.STRING
			|| type == TokenType.IDENTIFIER
			|| type == TokenType.LPAREN
			|| type == TokenType.LBRACKET
			|| type == TokenType.LBRACE
			|| type == TokenType.MINUS
			|| type == TokenType.ADDRESS_OF
			|| type == TokenType.NOT
			|| type == TokenType.FUNCTION;
	}

	// Parse an expression with the given minimum precedence (Pratt parser core)
	public ASTNode ParseExpression(Precedence minPrecedence) {
		Token token = _current;
		Advance();

		// Special case: function expression (spans multiple lines)
		if (token.Type == TokenType.FUNCTION) {
			return ParseFunctionExpression();
		}

		// Look up the prefix parselet for this token
		PrefixParselet prefix = null;
		if (!_prefixParselets.TryGetValue(token.Type, out prefix)) {
			ReportError($"Unexpected token: {TokenDescription(token)}");
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
	// Handles: callStatement, assignmentStatement, breakStatement, continueStatement, expressionStatement
	private ASTNode ParseSimpleStatement() {
		// Check for break statement
		if (_current.Type == TokenType.BREAK) {
			Advance();  // consume BREAK
			return new BreakNode();
		}

		// Check for continue statement
		if (_current.Type == TokenType.CONTINUE) {
			Advance();  // consume CONTINUE
			return new ContinueNode();
		}

		// Check for return statement
		if (_current.Type == TokenType.RETURN) {
			Advance();  // consume RETURN
			// Parse optional return value (if something that can start an expression follows)
			ASTNode returnValue = null;
			if (_current.Type != TokenType.EOL && _current.Type != TokenType.END_OF_INPUT
				&& _current.Type != TokenType.ELSE && CanStartExpression(_current.Type)) {
				returnValue = ParseExpression();
			}
			return new ReturnNode(returnValue);
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
			if (_current.AfterSpace && CanStartExpression(_current.Type)) {
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
			ASTNode expr = ParseExpressionFrom(left);

			// Check for indexed assignment: expr[index] = value
			IndexNode idxNode = expr as IndexNode;
			if (idxNode != null && _current.Type == TokenType.ASSIGN) {
				Advance(); // consume '='
				ASTNode value = ParseExpression();
				return new IndexedAssignmentNode(idxNode.Target, idxNode.Index, value);
			}

			// Check for no-parens call on an expression result, e.g. funcs[0] 10
			if (_current.AfterSpace && CanStartExpression(_current.Type)) {
				List<ASTNode> args = new List<ASTNode>();
				args.Add(ParseExpression());
				while (Match(TokenType.COMMA)) {
					args.Add(ParseExpression());
				}
				return new ExprCallNode(expr, args);
			}

			return expr;
		}

		// Not an identifier - parse as expression statement
		ASTNode expr2 = ParseExpression();
		IndexNode idxNode2 = expr2 as IndexNode;
		if (idxNode2 != null && _current.Type == TokenType.ASSIGN) {
			Advance(); // consume '='
			ASTNode value = ParseExpression();
			return new IndexedAssignmentNode(idxNode2.Target, idxNode2.Index, value);
		}
		return expr2;
	}

	// Check if current token is a block terminator
	private Boolean IsBlockTerminator(TokenType t1, TokenType t2) {
		return _current.Type == TokenType.END_OF_INPUT
			|| _current.Type == t1
			|| _current.Type == t2;
	}

	// Parse a block of statements until we hit a terminator token.
	// Used for block bodies in while, if, for, function, etc.
	// Terminators are not consumed.
	private List<ASTNode> ParseBlock(TokenType terminator1, TokenType terminator2) {
		List<ASTNode> body = new List<ASTNode>();

		while (true) {
			// Skip blank lines
			while (_current.Type == TokenType.EOL) {
				Advance();
			}

			// Check for terminator or end of input
			if (IsBlockTerminator(terminator1, terminator2)) {
				break;
			}

			// Parse a statement
			ASTNode stmt = ParseStatement();
			if (stmt != null) {
				body.Add(stmt);
			}

			// Expect EOL after statement
			if (_current.Type != TokenType.EOL && !IsBlockTerminator(terminator1, terminator2)) {
				ReportError($"Expected end of line, got: {_current.Text}");
				// Try to recover by skipping to next line
				while (_current.Type != TokenType.EOL && _current.Type != TokenType.END_OF_INPUT) {
					Advance();
				}
			}
		}

		return body;
	}

	// Require "end <keyword>" and consume it, reporting error if not found
	private void RequireEndKeyword(TokenType keyword, String keywordName) {
		if (_current.Type != TokenType.END) {
			ReportError($"Expected 'end {keywordName}'");
			return;
		}
		Advance();  // consume END
		if (_current.Type != keyword) {
			ReportError(GotExpected(StringUtils.Format("Keyword({0})", keywordName)));
			return;
		}
		Advance();  // consume keyword
	}

	// Parse an if statement: IF already consumed
	// Handles both block form and single-line form
	private ASTNode ParseIfStatement() {
		ASTNode condition = ParseExpression();

		// Expect THEN
		if (_current.Type != TokenType.THEN) {
			ReportError(GotExpected("Keyword(then)"));
			return new IfNode(condition, new List<ASTNode>(), new List<ASTNode>());
		}
		Advance();  // consume THEN

		// Check if block or single-line form
		if (_current.Type == TokenType.EOL || _current.Type == TokenType.END_OF_INPUT) {
			// Block form
			List<ASTNode> thenBody = ParseBlock(TokenType.ELSE, TokenType.END);
			List<ASTNode> elseBody = ParseElseClause();
			if (_current.Type == TokenType.END) {
				RequireEndKeyword(TokenType.IF, "if");
			}
			return new IfNode(condition, thenBody, elseBody);
		} else {
			// Single-line form
			return ParseSingleLineIfBody(condition);
		}
	}

	// Parse else/else-if clause for block if statements
	// Returns the else body (which may contain a nested IfNode for else-if)
	private List<ASTNode> ParseElseClause() {
		List<ASTNode> elseBody = new List<ASTNode>();

		if (_current.Type != TokenType.ELSE) {
			return elseBody;
		}
		Advance();  // consume ELSE

		if (_current.Type == TokenType.IF) {
			// else if - parse as nested if (which handles its own "end if")
			Advance();  // consume IF
			elseBody.Add(ParseIfStatement());
		} else {
			// plain else - expect EOL then body
			if (_current.Type != TokenType.EOL && _current.Type != TokenType.END_OF_INPUT) {
				ReportError($"Expected end of line after 'else', got: {_current.Text}");
			}
			elseBody = ParseBlock(TokenType.END, TokenType.END);  // only END terminates
		}

		return elseBody;
	}

	// Parse a statement that can appear in single-line if context
	// This includes simple statements, nested if statements, and return
	// (but not, for example, for/while loops, which are invalid
	// in the context of a single-line `if`).
	private ASTNode ParseSingleLineStatement() {
		if (_current.Type == TokenType.IF) {
			Advance();  // consume IF
			return ParseIfStatement();
		}
		return ParseSimpleStatement();
	}

	// Parse single-line if body (after "if condition then ")
	private ASTNode ParseSingleLineIfBody(ASTNode condition) {
		List<ASTNode> thenBody = new List<ASTNode>();
		ASTNode thenStmt = ParseSingleLineStatement();
		if (thenStmt != null) {
			thenBody.Add(thenStmt);
		}

		List<ASTNode> elseBody = new List<ASTNode>();
		if (_current.Type == TokenType.ELSE) {
			Advance();  // consume ELSE
			ASTNode elseStmt = ParseSingleLineStatement();
			if (elseStmt != null) {
				elseBody.Add(elseStmt);
			}
		}

		return new IfNode(condition, thenBody, elseBody);
	}

	// Parse a while statement: WHILE already consumed
	private ASTNode ParseWhileStatement() {
		ASTNode condition = ParseExpression();

		// Expect EOL after condition
		if (_current.Type != TokenType.EOL && _current.Type != TokenType.END_OF_INPUT) {
			ReportError($"Expected end of line after while condition, got: {_current.Text}");
		}

		List<ASTNode> body = ParseBlock(TokenType.END, TokenType.END);
		RequireEndKeyword(TokenType.WHILE, "while");

		return new WhileNode(condition, body);
	}

	// Parse a for statement: FOR already consumed
	// Syntax: for <identifier> in <expression> <EOL> <body> end for
	private ASTNode ParseForStatement() {
		// Expect identifier (loop variable)
		if (_current.Type != TokenType.IDENTIFIER) {
			ReportError($"Expected identifier after 'for', got: {_current.Text}");
			return new ForNode("_", new NumberNode(0), new List<ASTNode>());
		}
		String varName = _current.Text;
		Advance();  // consume identifier

		// Expect IN
		if (_current.Type != TokenType.IN) {
			ReportError(GotExpected("Keyword(in)"));
		} else {
			Advance();  // consume IN
		}

		// Parse iterable expression
		ASTNode iterable = ParseExpression();

		// Expect EOL after expression
		if (_current.Type != TokenType.EOL && _current.Type != TokenType.END_OF_INPUT) {
			ReportError($"Expected end of line after for expression, got: {_current.Text}");
		}

		List<ASTNode> body = ParseBlock(TokenType.END, TokenType.END);
		RequireEndKeyword(TokenType.FOR, "for");

		return new ForNode(varName, iterable, body);
	}

	// Parse a function expression: FUNCTION already consumed
	// Syntax: function(param1, param2, ...) <body> end function
	// The parentheses are optional for no-parameter functions.
	private ASTNode ParseFunctionExpression() {
		// Parse parameter list (parentheses optional for no-param functions)
		List<String> paramNames = new List<String>();
		List<ASTNode> paramDefaults = new List<ASTNode>();
		if (_current.Type == TokenType.LPAREN) {
			Advance();  // consume '('
			if (_current.Type != TokenType.RPAREN) {
				do {
					Token paramToken = Expect(TokenType.IDENTIFIER, "Expected parameter name");
					if (paramToken.Type != TokenType.ERROR) {
						paramNames.Add(paramToken.Text);
						if (Match(TokenType.ASSIGN)) {
							// Parse default value expression
							ASTNode defaultExpr = ParseExpression(Precedence.NONE);
							paramDefaults.Add(defaultExpr);
						} else {
							paramDefaults.Add(null);
						}
					}
				} while (Match(TokenType.COMMA));
			}
			Expect(TokenType.RPAREN, "Expected ')' after parameters");
		}

		// Expect EOL after parameter list
		if (_current.Type != TokenType.EOL && _current.Type != TokenType.END_OF_INPUT) {
			ReportError($"Expected end of line after function parameters, got: {_current.Text}");
		}

		// Parse body until "end function"
		List<ASTNode> body = ParseBlock(TokenType.END, TokenType.END);
		RequireEndKeyword(TokenType.FUNCTION, "function");

		return new FunctionNode(paramNames, paramDefaults, body);
	}

	// Parse a statement (handles both simple statements and block statements)
	public ASTNode ParseStatement() {
		// Skip leading blank lines
		while (_current.Type == TokenType.EOL) {
			Advance();
		}

		if (_current.Type == TokenType.END_OF_INPUT) {
			return null;
		}

		// Check for block statements
		if (_current.Type == TokenType.WHILE) {
			Advance();  // consume WHILE
			return ParseWhileStatement();
		}

		if (_current.Type == TokenType.FOR) {
			Advance();  // consume FOR
			return ParseForStatement();
		}

		if (_current.Type == TokenType.IF) {
			Advance();  // consume IF
			return ParseIfStatement();
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

			ASTNode stmt = ParseStatement();
			if (stmt != null) {
				statements.Add(stmt);
			}

			// Expect EOL or EOF after statement
			// (block statements like while handle their own EOL consumption)
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

	// Describe a token for use in error messages, matching MiniScript 1.x format
	private String TokenDescription(Token tok) {
		if (tok.Type == TokenType.EOL || tok.Type == TokenType.END_OF_INPUT) return "EOL";
		if (tok.Type == TokenType.NUMBER) return tok.Text;
		if (tok.Type == TokenType.STRING) return StringUtils.Format("\"{0}\"", tok.Text);
		if (tok.Type == TokenType.IDENTIFIER) return tok.Text;
		// Keywords
		if (tok.Type == TokenType.WHILE) return "Keyword(while)";
		if (tok.Type == TokenType.FOR) return "Keyword(for)";
		if (tok.Type == TokenType.IN) return "Keyword(in)";
		if (tok.Type == TokenType.IF) return "Keyword(if)";
		if (tok.Type == TokenType.THEN) return "Keyword(then)";
		if (tok.Type == TokenType.ELSE) return "Keyword(else)";
		if (tok.Type == TokenType.END) return "Keyword(end)";
		if (tok.Type == TokenType.BREAK) return "Keyword(break)";
		if (tok.Type == TokenType.CONTINUE) return "Keyword(continue)";
		if (tok.Type == TokenType.FUNCTION) return "Keyword(function)";
		if (tok.Type == TokenType.RETURN) return "Keyword(return)";
		if (tok.Type == TokenType.AND) return "Keyword(and)";
		if (tok.Type == TokenType.OR) return "Keyword(or)";
		if (tok.Type == TokenType.NOT) return "Keyword(not)";
		return tok.Text;
	}

	// Format an error in the 1.x style: "got X where Y is required"
	private String GotExpected(String expected) {
		return StringUtils.Format("got {0} where {1} is required", TokenDescription(_current), expected);
	}

	// Report an error
	public void ReportError(String message) {
		Errors.Add(StringUtils.Format("Compiler Error: {0} [line {1}]", message, _current.Line));
	}

	// Check if any errors occurred
	public Boolean HadError() {
		return Errors.HasError();
	}

	// Get the list of errors
	public List<String> GetErrors() {
		return Errors.GetErrors();
	}
}

}
