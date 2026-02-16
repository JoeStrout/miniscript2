// Hand-written lexer for MiniScript
// Simple expression tokenizer (to be expanded for full MiniScript grammar)

using System;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
// H: #include "LangConstants.g.h"
// H: #include "ErrorPool.g.h"
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
	public Boolean AfterSpace;  // True if whitespace preceded this token

	// H: public: Token() {}
	public Token(TokenType type, String text, Int32 line, Int32 column) {
		Type = type;
		Text = text;
		IntValue = 0;
		DoubleValue = 0;
		Line = line;
		Column = column;
		AfterSpace = false;
	}
}

public struct Lexer {
	private String _input;
	private Int32 _position;
	private Int32 _line;
	private Int32 _column;
	public ErrorPool Errors;

	// H: public: Lexer() {}
	public Lexer(String source) {
		_input = source;
		_position = 0;
		_line = 1;
		_column = 1;
		Errors = new ErrorPool();
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
	// Returns true if any whitespace was skipped
	[MethodImpl(AggressiveInlining)]
	private Boolean SkipWhitespace() {
		Boolean skipped = false;
		while (_position < _input.Length) {
			Char ch = _input[_position];
			if (ch == '\n') break;  // newlines are significant
			if (!IsWhiteSpace(ch)) break;
			Advance();
			skipped = true;
		}
		return skipped;
	}

	// Get the next token from _input
	public Token NextToken() {
		Boolean hadWhitespace = SkipWhitespace();

		Int32 startLine = _line;
		Int32 startColumn = _column;

		// End of _input
		if (_position >= _input.Length) {
			Token eofTok = new Token(TokenType.END_OF_INPUT, "", startLine, startColumn);
			eofTok.AfterSpace = hadWhitespace;
			return eofTok;
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
			tok.AfterSpace = hadWhitespace;
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
			Token tok;
			// Check for keywords
			if (text == "and") {
				tok = new Token(TokenType.AND, text, startLine, startColumn);
			} else if (text == "or") {
				tok = new Token(TokenType.OR, text, startLine, startColumn);
			} else if (text == "not") {
				tok = new Token(TokenType.NOT, text, startLine, startColumn);
			} else if (text == "while") {
				tok = new Token(TokenType.WHILE, text, startLine, startColumn);
			} else if (text == "for") {
				tok = new Token(TokenType.FOR, text, startLine, startColumn);
			} else if (text == "in") {
				tok = new Token(TokenType.IN, text, startLine, startColumn);
			} else if (text == "if") {
				tok = new Token(TokenType.IF, text, startLine, startColumn);
			} else if (text == "then") {
				tok = new Token(TokenType.THEN, text, startLine, startColumn);
			} else if (text == "else") {
				tok = new Token(TokenType.ELSE, text, startLine, startColumn);
			} else if (text == "break") {
				tok = new Token(TokenType.BREAK, text, startLine, startColumn);
			} else if (text == "continue") {
				tok = new Token(TokenType.CONTINUE, text, startLine, startColumn);
			} else if (text == "function") {
				tok = new Token(TokenType.FUNCTION, text, startLine, startColumn);
			} else if (text == "return") {
				tok = new Token(TokenType.RETURN, text, startLine, startColumn);
			} else if (text == "new") {
				tok = new Token(TokenType.NEW, text, startLine, startColumn);
			} else if (text == "isa") {
				tok = new Token(TokenType.ISA, text, startLine, startColumn);
			} else if (text == "end") {
				tok = new Token(TokenType.END, text, startLine, startColumn);
			} else {
				tok = new Token(TokenType.IDENTIFIER, text, startLine, startColumn);
			}
			tok.AfterSpace = hadWhitespace;
			return tok;
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
			Token tok = new Token(TokenType.STRING, text, startLine, startColumn);
			tok.AfterSpace = hadWhitespace;
			return tok;
		}

		// Multi-character operators (check before consuming)
		Token multiTok;
		if (c == '=' && _position + 1 < _input.Length && _input[_position + 1] == '=') {
			Advance(); Advance();
			multiTok = new Token(TokenType.EQUALS, "==", startLine, startColumn);
			multiTok.AfterSpace = hadWhitespace;
			return multiTok;
		}
		if (c == '!' && _position + 1 < _input.Length && _input[_position + 1] == '=') {
			Advance(); Advance();
			multiTok = new Token(TokenType.NOT_EQUAL, "!=", startLine, startColumn);
			multiTok.AfterSpace = hadWhitespace;
			return multiTok;
		}
		if (c == '<' && _position + 1 < _input.Length && _input[_position + 1] == '=') {
			Advance(); Advance();
			multiTok = new Token(TokenType.LESS_EQUAL, "<=", startLine, startColumn);
			multiTok.AfterSpace = hadWhitespace;
			return multiTok;
		}
		if (c == '>' && _position + 1 < _input.Length && _input[_position + 1] == '=') {
			Advance(); Advance();
			multiTok = new Token(TokenType.GREATER_EQUAL, ">=", startLine, startColumn);
			multiTok.AfterSpace = hadWhitespace;
			return multiTok;
		}

		// Comments: // to end of line
		if (c == '/' && _position + 1 < _input.Length && _input[_position + 1] == '/') {
			Int32 start = _position;
			// Consume until end of line (but not the newline itself)
			while (_position < _input.Length && _input[_position] != '\n') {
				Advance();
			}
			String text = _input.Substring(start, _position - start);
			Token commentTok = new Token(TokenType.COMMENT, text, startLine, startColumn);
			commentTok.AfterSpace = hadWhitespace;
			return commentTok;
		}

		// Single-character operators and punctuation
		Advance();
		Token singleTok;
		switch (c) {
			case '+': singleTok = new Token(TokenType.PLUS, "+", startLine, startColumn); break;
			case '-': singleTok = new Token(TokenType.MINUS, "-", startLine, startColumn); break;
			case '*': singleTok = new Token(TokenType.TIMES, "*", startLine, startColumn); break;
			case '/': singleTok = new Token(TokenType.DIVIDE, "/", startLine, startColumn); break;
			case '%': singleTok = new Token(TokenType.MOD, "%", startLine, startColumn); break;
			case '^': singleTok = new Token(TokenType.CARET, "^", startLine, startColumn); break;
			case '(': singleTok = new Token(TokenType.LPAREN, "(", startLine, startColumn); break;
			case ')': singleTok = new Token(TokenType.RPAREN, ")", startLine, startColumn); break;
			case '[': singleTok = new Token(TokenType.LBRACKET, "[", startLine, startColumn); break;
			case ']': singleTok = new Token(TokenType.RBRACKET, "]", startLine, startColumn); break;
			case '{': singleTok = new Token(TokenType.LBRACE, "{", startLine, startColumn); break;
			case '}': singleTok = new Token(TokenType.RBRACE, "}", startLine, startColumn); break;
			case '=': singleTok = new Token(TokenType.ASSIGN, "=", startLine, startColumn); break;
			case '<': singleTok = new Token(TokenType.LESS_THAN, "<", startLine, startColumn); break;
			case '>': singleTok = new Token(TokenType.GREATER_THAN, ">", startLine, startColumn); break;
			case ',': singleTok = new Token(TokenType.COMMA, ",", startLine, startColumn); break;
			case ':': singleTok = new Token(TokenType.COLON, ":", startLine, startColumn); break;
			case '.': singleTok = new Token(TokenType.DOT, ".", startLine, startColumn); break;
			case '@': singleTok = new Token(TokenType.ADDRESS_OF, "@", startLine, startColumn); break;
			case ';': singleTok = new Token(TokenType.EOL, ";", startLine, startColumn); break;
			case '\n': singleTok = new Token(TokenType.EOL, "\n", startLine, startColumn); break;
			default:
				singleTok = new Token(TokenType.ERROR, StringUtils.Str(c), startLine, startColumn); break;
		}
		singleTok.AfterSpace = hadWhitespace;
		return singleTok;
	}

	// Report an error
	public void Error(String message) {
		Errors.Add(StringUtils.Format("Compiler Error: {0} [line {1}]", message, _line));
	}
}

}
