// Simple test program for the hand-written lexer and generated parser

using System;
using MiniScript;

class ParserTest {
	static void Main(string[] args) {
		string[] testCases = {
			"42",
			"1 + 2",
			"10 - 5",
			"3 * 4",
			"20 / 4",
			"(2 + 3) * 4",
			"2 + 3 * 4",
			"(10 - 2) / 2"
		};

		foreach (string expr in testCases) {
			Console.WriteLine("Parsing: " + expr);
			Lexer lexer = new Lexer(expr);
			Parser parser = new Parser();
			parser.Scanner = lexer;
			bool success = parser.Parse();
			Console.WriteLine("Success: " + success);
			Console.WriteLine();
		}
	}
}
