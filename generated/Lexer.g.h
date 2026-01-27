// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Lexer.cs

#pragma once
#include "core_includes.h"
// Hand-written lexer for MiniScript
// Simple expression tokenizer (to be expanded for full MiniScript grammar)


namespace MiniScript {

// FORWARD DECLARATIONS

struct VM;
class VMStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;

// DECLARATIONS














// Token types returned by the lexer
enum class TokenType : Byte {
	END_OF_INPUT,
	NUMBER,
	STRING,
	IDENTIFIER,
	PLUS,
	MINUS,
	TIMES,
	DIVIDE,
	LPAREN,
	RPAREN,
	ERROR
}; // end of enum TokenType


// Represents a single token from the lexer
struct Token {
	public: TokenType Type;
	public: String Text;
	public: Int32 IntValue;
	public: Double DoubleValue;
	public: Int32 Line;
	public: Int32 Column;

	public: Token(TokenType type, String text, Int32 line, Int32 column);
}; // end of struct Token


struct Lexer {
	private: String _input;
	private: Int32 _position;
	private: Int32 _line;
	private: Int32 _column;

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
	private: void SkipWhitespace();

	// Get the next token from _input
	public: Token NextToken();

	// Report an error
	public: void Error(String message);
}; // end of struct Lexer


// INLINE METHODS

inline Boolean Lexer::IsDigit(Char c) {
	return '0' <= c && c <= '9';
}
inline Boolean Lexer::IsWhiteSpace(Char c) {
	// ToDo: rework Lexer(shared_from_this()) whole file to be fully Unicode-savvy in both C# and C++
	return UnicodeCharIsWhitespace((long)c);
}
inline Boolean Lexer::IsIdentifierStartChar(Char c) {
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
	    || ((int)c > 127 && !IsWhiteSpace(c)));
}
inline Boolean Lexer::IsIdentifierChar(Char c) {
	return IsIdentifierStartChar(c) || IsDigit(c);
}
inline void Lexer::SkipWhitespace() {
	while (_position < _input.Length() && IsWhiteSpace(_input[_position])) {
		Advance();
	}
}

} // end of namespace MiniScript
