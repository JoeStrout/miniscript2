// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Lexer.cs

#include "Lexer.g.h"

namespace MiniScript {


LexerStorage::LexerStorage(string source) {
	input = source;
	position = 0;
}
int LexerStorage::yylex() {
	// Skip whitespace
	while (position < input::Length && char::IsWhiteSpace(input[position])) {
		position++;
	}

	// End of input
	if (position >= input::Length) {
		return (int)Tokens::END_OF_FILE;
	}

	char c = input[position];

	// Numbers
	if (char::IsDigit(c)) {
		int start = position;
		while (position < input::Length && char::IsDigit(input[position])) {
			position++;
		}
		string numStr = input::Substring(start, position - start);
		yylval = int::Parse(numStr);
		return (int)Tokens::NUMBER;
	}

	// Single-character operators and parentheses
	position++;
	switch (c) {
		case '+': return (int)Tokens::PLUS;
		case '-': return (int)Tokens::MINUS;
		case '*': return (int)Tokens::TIMES;
		case '/': return (int)Tokens::DIVIDE;
		case '(': return (int)Tokens::LPAREN;
		case ')': return (int)Tokens::RPAREN;
		default:
			Console::WriteLine("Unexpected character: " + c);
			return (int)Tokens::error;
	}
}
void LexerStorage::yyerror(string message, params object[] args) {
	Console::WriteLine("Lexer error: " + string::Format(message, args));
}


} // end of namespace MiniScript
