using System;

namespace MiniScript {

class Program {
	static void Main() {
		Token t1 = new Token(TokenType.NUMBER, "42");
		Console.WriteLine(t1.Str());

		Token t2 = new Token(TokenType.PLUS);
		Console.WriteLine(t2.Str());

		Token t3 = new Token(TokenType.IDENTIFIER, "foo");
		Console.WriteLine(t3.Str());
	}
}

}
