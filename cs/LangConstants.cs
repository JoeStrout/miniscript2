// Constants used as part of the language definition: token types,
// operator precedence, that sort of thing.

using System;

namespace MiniScript {

// Precedence levels (higher precedence binds more strongly)
public enum Precedence : Int32 {
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
}

// Token types returned by the lexer
public enum TokenType : Int32 {
	END_OF_INPUT,
	NUMBER,
	STRING,
	IDENTIFIER,
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
	SEMICOLON,
	DOT,
	NOT,
	AND,
	OR,
	EOL,
	COMMENT,
	ERROR
}

}
