// Hand-converted MiniScript Lexer from C# code
// Simple expression tokenizer

#include "MiniScriptLexer.h"
#include <cctype>
#include <cstdlib>
#include <iostream>

namespace MiniScript {

Lexer::Lexer(const std::string& source) : input(source), position(0) {
}

Lexer::~Lexer() {
}

int Lexer::yylex() {
	// Skip whitespace
	while (position < input.length() && isspace(input[position])) {
		position++;
	}

	// End of input
	if (position >= input.length()) {
		return (int)Tokens::END_OF_FILE;
	}

	char c = input[position];

	// Numbers
	if (isdigit(c)) {
		size_t start = position;
		while (position < input.length() && isdigit(input[position])) {
			position++;
		}
		std::string numStr = input.substr(start, position - start);
		yylval = atoi(numStr.c_str());
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
			std::cout << "Unexpected character: " << c << std::endl;
			return (int)Tokens::error;
	}
}

void Lexer::yyerror(const String& message) {
	std::cout << "Lexer error: " << message.c_str() << std::endl;
}

} // namespace MiniScript
