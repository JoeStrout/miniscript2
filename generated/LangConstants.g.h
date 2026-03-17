// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: LangConstants.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// Constants used as part of the language definition: token types,
// operator precedence, that sort of thing.

namespace MiniScript {

// DECLARATIONS

// Precedence levels (higher precedence binds more strongly)
enum class Precedence : Int32 {
	NONE = 0,
	ASSIGNMENT,
	OR,
	AND,
	NOT,
	EQUALITY,        // == !=
	COMPARISON,      // < > <= >=
	ISA,             // isa
	SUM,             // + -
	PRODUCT,         // * / %
	UNARY_MINUS,     // -
	POWER,           // ^
	CALL,            // () []
	ADDRESS_OF,      // @
	PRIMARY
}; // end of enum Precedence

// Token types returned by the lexer
enum class TokenType : Int32 {
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
	PLUS_ASSIGN,
	MINUS_ASSIGN,
	TIMES_ASSIGN,
	DIVIDE_ASSIGN,
	MOD_ASSIGN,
	POWER_ASSIGN,
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
	FUNCTION,
	RETURN,
	NEW,
	ISA,
	SELF,
	SUPER,
	LOCALS,
	OUTER,
	GLOBALS,
	END,
	EOL,
	COMMENT,
	ERROR
}; // end of enum TokenType

// INLINE METHODS

} // end of namespace MiniScript
