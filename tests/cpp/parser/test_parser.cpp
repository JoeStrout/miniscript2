// Test program for the hand-written C++ parser and lexer

#include "MiniScriptParser.h"
#include "MiniScriptLexer.h"
#include <iostream>
#include <string>

int main() {
	const char* testCases[] = {
		"42",
		"1 + 2",
		"10 - 5",
		"3 * 4",
		"20 / 4",
		"(2 + 3) * 4",
		"2 + 3 * 4",
		"(10 - 2) / 2"
	};

	int numTests = sizeof(testCases) / sizeof(testCases[0]);

	for (int i = 0; i < numTests; i++) {
		std::string expr = testCases[i];
		std::cout << "Parsing: " << expr << std::endl;

		MiniScript::Lexer* lexer = new MiniScript::Lexer(expr);
		MiniScript::Parser* parser = new MiniScript::Parser();
		parser->SetScanner(lexer);

		bool success = parser->Parse();
		std::cout << "Success: " << (success ? "true" : "false") << std::endl;
		std::cout << std::endl;

		delete parser;
		delete lexer;
	}

	return 0;
}
