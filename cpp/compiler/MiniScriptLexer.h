// Hand-converted MiniScript Lexer from C# code
// Simple expression tokenizer

#pragma once
#include "ShiftReduceParserCode.h"
#include "MiniScriptParser.g.h"
#include <string>

namespace MiniScript {

class Lexer : public QUT::Gppg::AbstractScanner<int, QUT::Gppg::LexLocation> {
private:
	std::string input;
	size_t position;

public:
	Lexer(const std::string& source);
	virtual ~Lexer();

	// Override abstract methods
	virtual int yylex() override;
	virtual void yyerror(const String& message) override;
};

} // namespace MiniScript
