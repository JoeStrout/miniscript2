// Constants used as part of the language definition: token types,
// operator precedence, that sort of thing.

using System;

namespace MiniScript {

// Precedence levels (higher precedence binds more strongly)
public enum Precedence : Int32 {
	NONE = 0,
	ASSIGNMENT,
	OR,
	AND,
	NOT,
	EQUALITY,        // == !=
	COMPARISON,      // < > <= >=
	SUM,             // + -
	PRODUCT,         // * / %
	UNARY_MINUS,     // -
	POWER,           // ^
	CALL,            // () []
	ADDRESS_OF,      // @
	PRIMARY
}

// Token types returned by the lexer
public enum TokenType : Int32 {
	END_OF_INPUT,
	NUMBER,
	STRING,
	IDENTIFIER,
	ADDRESS_OF,
	PLUS,
	MINUS,
	TIMES,
	DIVIDE,
	MOD,
	CARET,
	LPAREN,
	RPAREN,
	LBRACKET,
	RBRACKET,
	LBRACE,
	RBRACE,
	ASSIGN,
	EQUALS,
	NOT_EQUAL,
	LESS_THAN,
	GREATER_THAN,
	LESS_EQUAL,
	GREATER_EQUAL,
	COMMA,
	COLON,
	DOT,
	NOT,
	AND,
	OR,
	WHILE,
	FOR,
	IN,
	IF,
	THEN,
	ELSE,
	BREAK,
	CONTINUE,
	END,
	EOL,
	COMMENT,
	ERROR
}

}
