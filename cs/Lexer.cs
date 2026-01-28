// Hand-written lexer for MiniScript
// Simple expression tokenizer (to be expanded for full MiniScript grammar)

using System;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
// H: #include "LangConstants.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "IOHelper.g.h"

namespace MiniScript {

// Represents a single token from the lexer
public struct Token {
	public TokenType Type;
	public String Text;
	public Int32 IntValue;
	public Double DoubleValue;
	public Int32 Line;
	public Int32 Column;

	// CPP: public: Token() = default;
	public Token(TokenType type, String text, Int32 line, Int32 column) {
		Type = type;
		Text = text;
		IntValue = 0;
		DoubleValue = 0;
		Line = line;
		Column = column;
	}
}

public struct Lexer {
	private String _input;
	private Int32 _position;
	private Int32 _line;
	private Int32 _column;

	// CPP: public: Lexer() = default;
	public Lexer(String source) {
		_input = source;
		_position = 0;
		_line = 1;
		_column = 1;
	}

	// Peek at current character without advancing
	private Char Peek() {
		if (_position >= _input.Length) return '\0';
		return _input[_position];
	}

	// Advance to next character
	private Char Advance() {
		Char c = Peek();
		_position++;
		if (c == '\n') {
			_line++;
			_column = 1;
		} else {
			_column++;
		}
		return c;
	}

	[MethodImpl(AggressiveInlining)]
	public static Boolean IsDigit(Char c) {
		return '0' <= c && c <= '9';
	}
	
	[MethodImpl(AggressiveInlining)]
	public static Boolean IsWhiteSpace(Char c) {
		// ToDo: rework this whole file to be fully Unicode-savvy in both C# and C++
		return Char.IsWhiteSpace(c); // CPP: return UnicodeCharIsWhitespace((long)c);
	}
		
	[MethodImpl(AggressiveInlining)]
	public static Boolean IsIdentifierStartChar(Char c) {
		return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
		    || ((int)c > 127 && !IsWhiteSpace(c)));
	}

	[MethodImpl(AggressiveInlining)]
	public static Boolean IsIdentifierChar(Char c) {
		return IsIdentifierStartChar(c) || IsDigit(c);
	}

	// Skip whitespace (but not newlines, which may be significant)
	[MethodImpl(AggressiveInlining)]
	private void SkipWhitespace() {
		while (_position < _input.Length) {
			Char ch = _input[_position];
			if (ch == '\n') break;  // newlines are significant
			if (!IsWhiteSpace(ch)) break;
			Advance();
		}
	}

	// Get the next token from _input
	public Token NextToken() {
		SkipWhitespace();

		Int32 startLine = _line;
		Int32 startColumn = _column;

		// End of _input
		if (_position >= _input.Length) {
			return new Token(TokenType.END_OF_INPUT, "", startLine, startColumn);
		}

		Char c = Peek();

		// Numbers
		if (IsDigit(c)) {
			Int32 start = _position;
			while (_position < _input.Length && IsDigit(_input[_position])) {
				Advance();
			}
			// Check for decimal point
			if (Peek() == '.' && _position + 1 < _input.Length && IsDigit(_input[_position + 1])) {
				Advance(); // consume '.'
				while (_position < _input.Length && IsDigit(_input[_position])) {
					Advance();
				}
			}
			String numStr = _input.Substring(start, _position - start);
			Token tok = new Token(TokenType.NUMBER, numStr, startLine, startColumn);
			if (numStr.Contains(".")) {
				tok.DoubleValue = StringUtils.ParseDouble(numStr);
			} else {
				tok.IntValue = StringUtils.ParseInt32(numStr);
				tok.DoubleValue = tok.IntValue;
			}
			return tok;
		}

		// Identifiers and keywords
		if (IsIdentifierStartChar(c)) {
			Int32 start = _position;
			while (_position < _input.Length && IsIdentifierChar(_input[_position])) {
				Advance();
			}
			String text = _input.Substring(start, _position - start);
			// Check for keywords
			if (text == "and") return new Token(TokenType.AND, text, startLine, startColumn);
			if (text == "or") return new Token(TokenType.OR, text, startLine, startColumn);
			if (text == "not") return new Token(TokenType.NOT, text, startLine, startColumn);
			return new Token(TokenType.IDENTIFIER, text, startLine, startColumn);
		}

		// String literals
		if (c == '"') {
			Advance(); // consume opening quote
			Int32 start = _position;
			while (_position < _input.Length && _input[_position] != '"') {
				if (_input[_position] == '\\' && _position + 1 < _input.Length) {
					Advance(); // skip escape character
				}
				Advance();
			}
			String text = _input.Substring(start, _position - start);
			if (Peek() == '"') Advance(); // consume closing quote
			return new Token(TokenType.STRING, text, startLine, startColumn);
		}

		// Multi-character operators (check before consuming)
		if (c == '=' && _position + 1 < _input.Length && _input[_position + 1] == '=') {
			Advance(); Advance();
			return new Token(TokenType.EQUALS, "==", startLine, startColumn);
		}
		if (c == '!' && _position + 1 < _input.Length && _input[_position + 1] == '=') {
			Advance(); Advance();
			return new Token(TokenType.NOT_EQUAL, "!=", startLine, startColumn);
		}
		if (c == '<' && _position + 1 < _input.Length && _input[_position + 1] == '=') {
			Advance(); Advance();
			return new Token(TokenType.LESS_EQUAL, "<=", startLine, startColumn);
		}
		if (c == '>' && _position + 1 < _input.Length && _input[_position + 1] == '=') {
			Advance(); Advance();
			return new Token(TokenType.GREATER_EQUAL, ">=", startLine, startColumn);
		}

		// Single-character operators and punctuation
		Advance();
		switch (c) {
			case '+': return new Token(TokenType.PLUS, "+", startLine, startColumn);
			case '-': return new Token(TokenType.MINUS, "-", startLine, startColumn);
			case '*': return new Token(TokenType.TIMES, "*", startLine, startColumn);
			case '/': return new Token(TokenType.DIVIDE, "/", startLine, startColumn);
			case '%': return new Token(TokenType.MOD, "%", startLine, startColumn);
			case '^': return new Token(TokenType.CARET, "^", startLine, startColumn);
			case '(': return new Token(TokenType.LPAREN, "(", startLine, startColumn);
			case ')': return new Token(TokenType.RPAREN, ")", startLine, startColumn);
			case '[': return new Token(TokenType.LBRACKET, "[", startLine, startColumn);
			case ']': return new Token(TokenType.RBRACKET, "]", startLine, startColumn);
			case '{': return new Token(TokenType.LBRACE, "{", startLine, startColumn);
			case '}': return new Token(TokenType.RBRACE, "}", startLine, startColumn);
			case '=': return new Token(TokenType.ASSIGN, "=", startLine, startColumn);
			case '<': return new Token(TokenType.LESS_THAN, "<", startLine, startColumn);
			case '>': return new Token(TokenType.GREATER_THAN, ">", startLine, startColumn);
			case ',': return new Token(TokenType.COMMA, ",", startLine, startColumn);
			case ':': return new Token(TokenType.COLON, ":", startLine, startColumn);
			case ';': return new Token(TokenType.SEMICOLON, ";", startLine, startColumn);
			case '.': return new Token(TokenType.DOT, ".", startLine, startColumn);
			case '\n': return new Token(TokenType.EOL, "\n", startLine, startColumn);
			default:
				return new Token(TokenType.ERROR, StringUtils.Str(c), startLine, startColumn);
		}
	}

	// Report an error
	public void Error(String message) {
		IOHelper.Print($"Lexer error at _line {_line}, _column {_column}: {message}");
	}
}

}
