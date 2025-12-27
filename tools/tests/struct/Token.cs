using System;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;

namespace MiniScript {

// TokenType: enum for all token types
public enum TokenType : Int32 {
	UNKNOWN = 0,
	NUMBER,
	IDENTIFIER,
	PLUS,
	MINUS,
	STAR,
	_QTY_TOKENS
}

// Token: represents a single token with its type and optional text
public struct Token {
	public TokenType type;
	public String text;  // null for operators; non-null for NUMBER and IDENTIFIER

	[MethodImpl(AggressiveInlining)]
	public Token(TokenType type, String text = null) {
		this.type = type;
		this.text = text;
	}
	
	// Convert to a string for debugging purposes
	public String Str() {
		switch (type) {
			case TokenType.UNKNOWN: return "UNKNOWN";
			case TokenType.NUMBER: return $"NUMBER({text})";
			case TokenType.IDENTIFIER: return $"IDENTIFIER({text})";
			case TokenType.PLUS: return "PLUS";
			case TokenType.MINUS: return "MINUS";
			case TokenType.STAR: return "STAR";
		}
		return "";
	}
			
}

}
