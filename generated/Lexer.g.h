// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Lexer.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// Hand-written lexer for MiniScript
// Simple expression tokenizer (to be expanded for full MiniScript grammar)

#include "LangConstants.g.h"
#include "ErrorTypes.g.h"

namespace MiniScript {

// DECLARATIONS

// Represents a single token from the lexer
struct Token {
	public: TokenType Type;
	public: String Text;
	public: Int32 IntValue;
	public: Double DoubleValue;
	public: Int32 Line;
	public: Int32 Column;
	public: Boolean AfterSpace; // True if whitespace preceded this token
	public: Token() {}

	public: Token(TokenType type, String text, Int32 line, Int32 column);
}; // end of struct Token

struct Lexer {
	private: String _input;
	private: Int32 _position;
	private: Int32 _line;
	private: Int32 _column;
	public: Value Error;
	public: Lexer() {}

	public: Lexer(String source);

	// Peek at current character without advancing
	private: Char Peek();

	// Advance to next character
	private: Char Advance();

	public: static Boolean IsDigit(Char c);
	
	public: static Boolean IsWhiteSpace(Char c);
		
	public: static Boolean IsIdentifierStartChar(Char c);

	public: static Boolean IsIdentifierChar(Char c);

	// Skip whitespace (but not newlines, which may be significant)
	// Returns true if any whitespace was skipped
	private: Boolean SkipWhitespace();

	// Get the next token from _input
	public: Token NextToken();

	// Record a compiler error.  Only the first error is kept.
	public: void ReportError(String message);

	public: Boolean HadError();
}; // end of struct Lexer

// INLINE METHODS

inline Boolean Lexer::IsDigit(Char c) {
	return '0' <= c && c <= '9';
}
inline Boolean Lexer::IsWhiteSpace(Char c) {
	// ToDo: rework this whole file to be fully Unicode-savvy in both C# and C++
	return UnicodeCharIsWhitespace((long)c);
}
inline Boolean Lexer::IsIdentifierStartChar(Char c) {
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
	    || ((int)c > 127 && !IsWhiteSpace(c)));
}
inline Boolean Lexer::IsIdentifierChar(Char c) {
	return IsIdentifierStartChar(c) || IsDigit(c);
}
inline Boolean Lexer::SkipWhitespace() {
	Boolean skipped = Boolean(false);
	while (_position < _input.Length()) {
		Char ch = _input[_position];
		if (ch == '\n') break;  // newlines are significant
		if (!IsWhiteSpace(ch)) break;
		Advance();
		skipped = Boolean(true);
	}
	return skipped;
}

} // end of namespace MiniScript
