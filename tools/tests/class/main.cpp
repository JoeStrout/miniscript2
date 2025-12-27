#include <iostream>
#include "expected/AST.g.h"

using namespace MiniScript;

int main() {
	// Create some AST nodes
	ASTNode num1 = NumberNode(42.0);
	ASTNode num2 = NumberNode(3.14);

	std::cout << "num1: " << num1.Str() << std::endl;
	std::cout << "num2: " << num2.Str() << std::endl;

	// Create a binary operation
	ASTNode sum = BinaryOpNode("PLUS", num1, num2);
	std::cout << "sum: " << sum.Str() << std::endl;

	// Test simplification (should compute 42 + 3.14)
	ASTNode simplified = sum.Simplify();
	std::cout << "simplified: " << simplified.Str() << std::endl;

	// Test with non-constant nodes (create a more complex tree)
	ASTNode expr = BinaryOpNode("TIMES",
		NumberNode(2.0),
		BinaryOpNode("PLUS", NumberNode(3.0), NumberNode(4.0))
	);
	std::cout << "expr: " << expr.Str() << std::endl;
	std::cout << "expr simplified: " << expr.Simplify().Str() << std::endl;

	return 0;
}
