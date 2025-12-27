// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Token.cs

#pragma once
#include "core_includes.h"

namespace MiniScript {

// DECLARATIONS


// TokenType: enum for all token types
public enum TokenType : Int32 {
	UNKNOWN = 0,
	NUMBER,
	IDENTIFIER,
	PLUS,
	MINUS,
	STAR,
	_QTY_TOKENS
}


// Token: represents a single token with its type and optional text
public struct Token {
	public: TokenType type;
	public: String text; // null for operators; non-null for NUMBER and IDENTIFIER
	public: Token(TokenType type, String text = null);
	public: String Str();
}


// INLINE METHODS


inline Token::Token(TokenType type, String text = null) {
	this.type = type;
	this.text = text;
}

} // end of namespace MiniScript
