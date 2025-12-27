// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Token.cs


namespace MiniScript {




String Token::Str() {
	switch (type) {
		case TokenType.UNKNOWN: return "UNKNOWN";
		case TokenType.NUMBER: return Interp("NUMBER({})", text);
		case TokenType.IDENTIFIER: return Interp("IDENTIFIER({})", text);
		case TokenType.PLUS: return "PLUS";
		case TokenType.MINUS: return "MINUS";
		case TokenType.STAR: return "STAR";
	}
	return "";
}


} // end of namespace MiniScript
