// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Lexer.cs

#include "Lexer.g.h"
#include "StringUtils.g.h"
#include "IOHelper.g.h"

namespace MiniScript {




Token::Token(TokenType type, String text, Int32 line, Int32 column) {
	Type = type;
	Text = text;
	IntValue = 0;
	DoubleValue = 0;
	Line = line;
	Column = column;
}


Lexer::Lexer(String source) {
	_input = source;
	_position = 0;
	_line = 1;
	_column = 1;
}
Char Lexer::Peek() {
	if (_position >= _input.Length()) return '\0';
	return _input[_position];
}
Char Lexer::Advance() {
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
Token Lexer::NextToken() {
	SkipWhitespace();

	Int32 startLine = _line;
	Int32 startColumn = _column;

	// End of _input
	if (_position >= _input.Length()) {
		return Token(TokenType::END_OF_INPUT, "", startLine, startColumn);
	}

	Char c = Peek();

	// Numbers
	if (IsDigit(c)) {
		Int32 start = _position;
		while (_position < _input.Length() && IsDigit(_input[_position])) {
			Advance();
		}
		// Check for decimal point
		if (Peek() == '.' && _position + 1 < _input.Length() && IsDigit(_input[_position + 1])) {
			Advance(); // consume '.'
			while (_position < _input.Length() && IsDigit(_input[_position])) {
				Advance();
			}
		}
		String numStr = _input.Substring(start, _position - start);
		Token tok = Token(TokenType::NUMBER, numStr, startLine, startColumn);
		if (numStr.Contains(".")) {
			tok.DoubleValue = StringUtils::ParseDouble(numStr);
		} else {
			tok.IntValue = StringUtils::ParseInt32(numStr);
			tok.DoubleValue = tok.IntValue;
		}
		return tok;
	}

	// Identifiers and keywords
	if (IsIdentifierStartChar(c)) {
		Int32 start = _position;
		while (_position < _input.Length() && IsIdentifierChar(_input[_position])) {
			Advance();
		}
		String text = _input.Substring(start, _position - start);
		// Check for keywords
		if (text == "and") return Token(TokenType::AND, text, startLine, startColumn);
		if (text == "or") return Token(TokenType::OR, text, startLine, startColumn);
		if (text == "not") return Token(TokenType::NOT, text, startLine, startColumn);
		return Token(TokenType::IDENTIFIER, text, startLine, startColumn);
	}

	// String literals
	if (c == '"') {
		Advance(); // consume opening quote
		Int32 start = _position;
		while (_position < _input.Length() && _input[_position] != '"') {
			if (_input[_position] == '\\' && _position + 1 < _input.Length()) {
				Advance(); // skip escape character
			}
			Advance();
		}
		String text = _input.Substring(start, _position - start);
		if (Peek() == '"') Advance(); // consume closing quote
		return Token(TokenType::STRING, text, startLine, startColumn);
	}

	// Multi-character operators (check before consuming)
	if (c == '=' && _position + 1 < _input.Length() && _input[_position + 1] == '=') {
		Advance(); Advance();
		return Token(TokenType::EQUALS, "==", startLine, startColumn);
	}
	if (c == '!' && _position + 1 < _input.Length() && _input[_position + 1] == '=') {
		Advance(); Advance();
		return Token(TokenType::NOT_EQUAL, "!=", startLine, startColumn);
	}
	if (c == '<' && _position + 1 < _input.Length() && _input[_position + 1] == '=') {
		Advance(); Advance();
		return Token(TokenType::LESS_EQUAL, "<=", startLine, startColumn);
	}
	if (c == '>' && _position + 1 < _input.Length() && _input[_position + 1] == '=') {
		Advance(); Advance();
		return Token(TokenType::GREATER_EQUAL, ">=", startLine, startColumn);
	}

	// Single-character operators and punctuation
	Advance();
	switch (c) {
		case '+': return Token(TokenType::PLUS, "+", startLine, startColumn);
		case '-': return Token(TokenType::MINUS, "-", startLine, startColumn);
		case '*': return Token(TokenType::TIMES, "*", startLine, startColumn);
		case '/': return Token(TokenType::DIVIDE, "/", startLine, startColumn);
		case '%': return Token(TokenType::MOD, "%", startLine, startColumn);
		case '^': return Token(TokenType::CARET, "^", startLine, startColumn);
		case '(': return Token(TokenType::LPAREN, "(", startLine, startColumn);
		case ')': return Token(TokenType::RPAREN, ")", startLine, startColumn);
		case '[': return Token(TokenType::LBRACKET, "[", startLine, startColumn);
		case ']': return Token(TokenType::RBRACKET, "]", startLine, startColumn);
		case '{': return Token(TokenType::LBRACE, "{", startLine, startColumn);
		case '}': return Token(TokenType::RBRACE, "}", startLine, startColumn);
		case '=': return Token(TokenType::ASSIGN, "=", startLine, startColumn);
		case '<': return Token(TokenType::LESS_THAN, "<", startLine, startColumn);
		case '>': return Token(TokenType::GREATER_THAN, ">", startLine, startColumn);
		case ',': return Token(TokenType::COMMA, ",", startLine, startColumn);
		case ':': return Token(TokenType::COLON, ":", startLine, startColumn);
		case ';': return Token(TokenType::SEMICOLON, ";", startLine, startColumn);
		case '.': return Token(TokenType::DOT, ".", startLine, startColumn);
		case '\n': return Token(TokenType::EOL, "\n", startLine, startColumn);
		default:
			return Token(TokenType::ERROR, StringUtils::Str(c), startLine, startColumn);
	}
}
void Lexer::Error(String message) {
	IOHelper::Print(Interp("Lexer error at _line {}, _column {}: {}", _line, _column, message));
}


} // end of namespace MiniScript
