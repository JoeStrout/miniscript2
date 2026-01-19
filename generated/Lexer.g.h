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
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct App;
class AppStorage;

// DECLARATIONS















// INLINE METHODS

} // end of namespace MiniScript

// Represents a single token from the lexer
public struct Token {
	public TokenType Type;
	public string Text;
	public int IntValue;
	public double DoubleValue;
	public int Line;
	public int Column;

	public Token(TokenType type, string text, int line, int column) {
		Type = type;
		Text = text;
		IntValue = 0;
		DoubleValue = 0;
		Line = line;
		Column = column;
	}
}

public class Lexer {
	private string input;
	private int position;
	private int line;
	private int column;

	public Lexer(string source) {
		input = source;
		position = 0;
		line = 1;
		column = 1;
	}

	// Peek at current character without advancing
	private char Peek() {
		if (position >= input.Length) return '\0';
		return input[position];
	}

	// Advance to next character
	private char Advance() {
		char c = Peek();
		position++;
		if (c == '\n') {
			line++;
			column = 1;
		} else {
			column++;
		}
		return c;
	}

	// Skip whitespace (but not newlines, which may be significant)
	private void SkipWhitespace() {
		while (position < input.Length && char.IsWhiteSpace(input[position])) {
			Advance();
		}
	}

	// Get the next token from input
	public Token NextToken() {
		SkipWhitespace();

		int startLine = line;
		int startColumn = column;

		// End of input
		if (position >= input.Length) {
			return new Token(TokenType.EOF, "", startLine, startColumn);
		}

		char c = Peek();

		// Numbers
		if (char.IsDigit(c)) {
			int start = position;
			while (position < input.Length && char.IsDigit(input[position])) {
				Advance();
			}
			// Check for decimal point
			if (Peek() == '.' && position + 1 < input.Length && char.IsDigit(input[position + 1])) {
				Advance(); // consume '.'
				while (position < input.Length && char.IsDigit(input[position])) {
					Advance();
				}
			}
			string numStr = input.Substring(start, position - start);
			Token tok = new Token(TokenType.NUMBER, numStr, startLine, startColumn);
			if (numStr.Contains(".")) {
				tok.DoubleValue = double.Parse(numStr);
			} else {
				tok.IntValue = int.Parse(numStr);
				tok.DoubleValue = tok.IntValue;
			}
			return tok;
		}

		// Identifiers and keywords
		if (char.IsLetter(c) || c == '_') {
			int start = position;
			while (position < input.Length && (char.IsLetterOrDigit(input[position]) || input[position] == '_')) {
				Advance();
			}
			string text = input.Substring(start, position - start);
			return new Token(TokenType.IDENTIFIER, text, startLine, startColumn);
		}

		// String literals
		if (c == '"') {
			Advance(); // consume opening quote
			int start = position;
			while (position < input.Length && input[position] != '"') {
				if (input[position] == '\\' && position + 1 < input.Length) {
					Advance(); // skip escape character
				}
				Advance();
			}
			string text = input.Substring(start, position - start);
			if (Peek() == '"') Advance(); // consume closing quote
			return new Token(TokenType.STRING, text, startLine, startColumn);
		}

		// Single-character operators and parentheses
		Advance();
		switch (c) {
			case '+': return new Token(TokenType.PLUS, "+", startLine, startColumn);
			case '-': return new Token(TokenType.MINUS, "-", startLine, startColumn);
			case '*': return new Token(TokenType.TIMES, "*", startLine, startColumn);
			case '/': return new Token(TokenType.DIVIDE, "/", startLine, startColumn);
			case '(': return new Token(TokenType.LPAREN, "(", startLine, startColumn);
			case ')': return new Token(TokenType.RPAREN, ")", startLine, startColumn);
			default:
				return new Token(TokenType.ERROR, c.ToString(), startLine, startColumn);
		}
	}

	// Report an error
	public void Error(string message) {
		Console.WriteLine($"Lexer error at line {line}, column {column}: {message}");
	}
}

}
