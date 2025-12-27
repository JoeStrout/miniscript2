#include <iostream>
#include "expected/Token.g.h"

using namespace MiniScript;

int main() {
	Token t1(TokenType::NUMBER, "42");
	std::cout << t1.Str() << std::endl;

	Token t2(TokenType::PLUS);
	std::cout << t2.Str() << std::endl;

	Token t3(TokenType::IDENTIFIER, "foo");
	std::cout << t3.Str() << std::endl;

	return 0;
}
