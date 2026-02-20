// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Lexer.cs

#include "Lexer.g.h"
#include "StringUtils.g.h"
#include "IOHelper.g.h"

namespace MiniScript {

Token::Token(TokenType type,String text,Int32 line,Int32 column) {
	Type = type;
	Text = text;
	IntValue = 0;
	DoubleValue = 0;
	Line = line;
	Column = column;
	AfterSpace = Boolean(false);
}

Lexer::Lexer(String source) {
	_input = source;
	_position = 0;
	_line = 1;
	_column = 1;
	Errors = ErrorPool();
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
	Boolean hadWhitespace = SkipWhitespace();

	Int32 startLine = _line;
	Int32 startColumn = _column;

	// End of _input
	if (_position >= _input.Length()) {
		Token eofTok = Token(TokenType::END_OF_INPUT, "", startLine, startColumn);
		eofTok.AfterSpace = hadWhitespace;
		return eofTok;
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
		tok.AfterSpace = hadWhitespace;
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
		Token tok;
		// Check for keywords
		if (text == "and") {
			tok = Token(TokenType::AND, text, startLine, startColumn);
		} else if (text == "or") {
			tok = Token(TokenType::OR, text, startLine, startColumn);
		} else if (text == "not") {
			tok = Token(TokenType::NOT, text, startLine, startColumn);
		} else if (text == "while") {
			tok = Token(TokenType::WHILE, text, startLine, startColumn);
		} else if (text == "for") {
			tok = Token(TokenType::FOR, text, startLine, startColumn);
		} else if (text == "in") {
			tok = Token(TokenType::IN, text, startLine, startColumn);
		} else if (text == "if") {
			tok = Token(TokenType::IF, text, startLine, startColumn);
		} else if (text == "then") {
			tok = Token(TokenType::THEN, text, startLine, startColumn);
		} else if (text == "else") {
			tok = Token(TokenType::ELSE, text, startLine, startColumn);
		} else if (text == "break") {
			tok = Token(TokenType::BREAK, text, startLine, startColumn);
		} else if (text == "continue") {
			tok = Token(TokenType::CONTINUE, text, startLine, startColumn);
		} else if (text == "function") {
			tok = Token(TokenType::FUNCTION, text, startLine, startColumn);
		} else if (text == "return") {
			tok = Token(TokenType::RETURN, text, startLine, startColumn);
		} else if (text == "new") {
			tok = Token(TokenType::NEW, text, startLine, startColumn);
		} else if (text == "isa") {
			tok = Token(TokenType::ISA, text, startLine, startColumn);
		} else if (text == "self") {
			tok = Token(TokenType::SELF, text, startLine, startColumn);
		} else if (text == "super") {
			tok = Token(TokenType::SUPER, text, startLine, startColumn);
		} else if (text == "end") {
			tok = Token(TokenType::END, text, startLine, startColumn);
		} else {
			tok = Token(TokenType::IDENTIFIER, text, startLine, startColumn);
		}
		tok.AfterSpace = hadWhitespace;
		return tok;
	}

	// String literals
	if (c == '"') {
		Advance(); // consume opening quote
		Int32 start = _position;
		List<String> parts = nullptr;
		while (_position < _input.Length()) {
			if (_input[_position] == '"') {
				// Check for doubled quote (escaped literal quote)
				if (_position + 1 < _input.Length() && _input[_position + 1] == '"') {
					if (IsNull(parts)) parts =  List<String>::New();
					parts.Add(_input.Substring(start, _position - start));
					parts.Add("\"");
					Advance(); Advance(); // skip both quotes
					start = _position;
					continue;
				}
				break; // closing quote
			}
			if (_input[_position] == '\\' && _position + 1 < _input.Length()) {
				Advance(); // skip escape character
			}
			Advance();
		}
		String text;
		if (IsNull(parts)) {
			text = _input.Substring(start, _position - start);
		} else {
			parts.Add(_input.Substring(start, _position - start));
			text = String::Join("", parts);
		}
		if (Peek() == '"') Advance(); // consume closing quote
		Token tok = Token(TokenType::STRING, text, startLine, startColumn);
		tok.AfterSpace = hadWhitespace;
		return tok;
	}

	// Multi-character operators (check before consuming)
	Token multiTok;
	if (c == '=' && _position + 1 < _input.Length() && _input[_position + 1] == '=') {
		Advance(); Advance();
		multiTok = Token(TokenType::EQUALS, "==", startLine, startColumn);
		multiTok.AfterSpace = hadWhitespace;
		return multiTok;
	}
	if (c == '!' && _position + 1 < _input.Length() && _input[_position + 1] == '=') {
		Advance(); Advance();
		multiTok = Token(TokenType::NOT_EQUAL, "!=", startLine, startColumn);
		multiTok.AfterSpace = hadWhitespace;
		return multiTok;
	}
	if (c == '<' && _position + 1 < _input.Length() && _input[_position + 1] == '=') {
		Advance(); Advance();
		multiTok = Token(TokenType::LESS_EQUAL, "<=", startLine, startColumn);
		multiTok.AfterSpace = hadWhitespace;
		return multiTok;
	}
	if (c == '>' && _position + 1 < _input.Length() && _input[_position + 1] == '=') {
		Advance(); Advance();
		multiTok = Token(TokenType::GREATER_EQUAL, ">=", startLine, startColumn);
		multiTok.AfterSpace = hadWhitespace;
		return multiTok;
	}

	// Compound assignment operators: +=, -=, *=, /=, %=, ^=
	if (_position + 1 < _input.Length() && _input[_position + 1] == '=') {
		TokenType compoundType = TokenType::ERROR;
		String compoundText = "";
		if (c == '+') { compoundType = TokenType::PLUS_ASSIGN; compoundText = "+="; }
		else if (c == '-') { compoundType = TokenType::MINUS_ASSIGN; compoundText = "-="; }
		else if (c == '*') { compoundType = TokenType::TIMES_ASSIGN; compoundText = "*="; }
		else if (c == '/') { compoundType = TokenType::DIVIDE_ASSIGN; compoundText = "/="; }
		else if (c == '%') { compoundType = TokenType::MOD_ASSIGN; compoundText = "%="; }
		else if (c == '^') { compoundType = TokenType::POWER_ASSIGN; compoundText = "^="; }
		if (compoundType != TokenType::ERROR) {
			Advance(); Advance();
			multiTok = Token(compoundType, compoundText, startLine, startColumn);
			multiTok.AfterSpace = hadWhitespace;
			return multiTok;
		}
	}

	// Comments: // to end of line (must check before /= which is handled below)
	if (c == '/' && _position + 1 < _input.Length() && _input[_position + 1] == '/') {
		Int32 start = _position;
		// Consume until end of line (but not the newline itself)
		while (_position < _input.Length() && _input[_position] != '\n') {
			Advance();
		}
		String text = _input.Substring(start, _position - start);
		Token commentTok = Token(TokenType::COMMENT, text, startLine, startColumn);
		commentTok.AfterSpace = hadWhitespace;
		return commentTok;
	}

	// Single-character operators and punctuation
	Advance();
	Token singleTok;
	switch (c) {
		case '+': singleTok = Token(TokenType::PLUS, "+", startLine, startColumn); break;
		case '-': singleTok = Token(TokenType::MINUS, "-", startLine, startColumn); break;
		case '*': singleTok = Token(TokenType::TIMES, "*", startLine, startColumn); break;
		case '/': singleTok = Token(TokenType::DIVIDE, "/", startLine, startColumn); break;
		case '%': singleTok = Token(TokenType::MOD, "%", startLine, startColumn); break;
		case '^': singleTok = Token(TokenType::CARET, "^", startLine, startColumn); break;
		case '(': singleTok = Token(TokenType::LPAREN, "(", startLine, startColumn); break;
		case ')': singleTok = Token(TokenType::RPAREN, ")", startLine, startColumn); break;
		case '[': singleTok = Token(TokenType::LBRACKET, "[", startLine, startColumn); break;
		case ']': singleTok = Token(TokenType::RBRACKET, "]", startLine, startColumn); break;
		case '{': singleTok = Token(TokenType::LBRACE, "{", startLine, startColumn); break;
		case '}': singleTok = Token(TokenType::RBRACE, "}", startLine, startColumn); break;
		case '=': singleTok = Token(TokenType::ASSIGN, "=", startLine, startColumn); break;
		case '<': singleTok = Token(TokenType::LESS_THAN, "<", startLine, startColumn); break;
		case '>': singleTok = Token(TokenType::GREATER_THAN, ">", startLine, startColumn); break;
		case ',': singleTok = Token(TokenType::COMMA, ",", startLine, startColumn); break;
		case ':': singleTok = Token(TokenType::COLON, ":", startLine, startColumn); break;
		case '.': singleTok = Token(TokenType::DOT, ".", startLine, startColumn); break;
		case '@': singleTok = Token(TokenType::ADDRESS_OF, "@", startLine, startColumn); break;
		case ';': singleTok = Token(TokenType::EOL, ";", startLine, startColumn); break;
		case '\n': singleTok = Token(TokenType::EOL, "\n", startLine, startColumn); break;
		default:
			singleTok = Token(TokenType::ERROR, StringUtils::Str(c), startLine, startColumn); break;
	}
	singleTok.AfterSpace = hadWhitespace;
	return singleTok;
}
void Lexer::Error(String message) {
	Errors.Add(StringUtils::Format("Compiler Error: {0} [line {1}]", message, _line));
}

} // end of namespace MiniScript
