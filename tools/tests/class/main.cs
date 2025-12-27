using System;

namespace MiniScript {

class Program {
	static void Main() {
		// Create some AST nodes
		ASTNode num1 = new NumberNode(42.0);
		ASTNode num2 = new NumberNode(3.14);

		Console.WriteLine("num1: " + num1.Str());
		Console.WriteLine("num2: " + num2.Str());

		// Create a binary operation
		ASTNode sum = new BinaryOpNode("PLUS", num1, num2);
		Console.WriteLine("sum: " + sum.Str());

		// Test simplification (should compute 42 + 3.14)
		ASTNode simplified = sum.Simplify();
		Console.WriteLine("simplified: " + simplified.Str());

		// Test with non-constant nodes (create a more complex tree)
		ASTNode expr = new BinaryOpNode("TIMES",
			new NumberNode(2.0),
			new BinaryOpNode("PLUS", new NumberNode(3.0), new NumberNode(4.0))
		);
		Console.WriteLine("expr: " + expr.Str());
		Console.WriteLine("expr simplified: " + expr.Simplify().Str());
	}
}

}
