// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Token.cs

#pragma once
#include "core_includes.h"

namespace MiniScript {

// TokenType: enum for all token types
enum class TokenType : Int32 {
	UNKNOWN = 0,
	NUMBER,
	IDENTIFIER,
	PLUS,
	MINUS,
	STAR,
	_QTY_TOKENS
}; // end of enum class TokenType

// Token: represents a single token with its type and optional text
struct Token {
	public: TokenType type;
	public: String text;  // null for operators; non-null for NUMBER and IDENTIFIER

	public: Token(TokenType type, String text = nullptr);

	// Convert to a string for debugging purposes
	public: String Str();

}; // end of struct Token

inline Token::Token(TokenType type, String text ) {
	this->type = type;
	this->text = text;
}

} // end of namespace MiniScript
