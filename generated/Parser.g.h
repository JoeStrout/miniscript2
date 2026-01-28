// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Parser.cs

#pragma once
#include "core_includes.h"
// Parser.cs - Pratt parser for MiniScript expressions
// Uses parselets to handle operator precedence and associativity.

#include "LangConstants.g.h"
#include "Lexer.g.h"
#include "Parselet.g.h"


namespace MiniScript {

// FORWARD DECLARATIONS

struct VM;
class VMStorage;
struct Assembler;
class AssemblerStorage;
struct Parselet;
class ParseletStorage;
struct PrefixParselet;
class PrefixParseletStorage;
struct InfixParselet;
class InfixParseletStorage;
struct NumberParselet;
class NumberParseletStorage;
struct StringParselet;
class StringParseletStorage;
struct IdentifierParselet;
class IdentifierParseletStorage;
struct UnaryOpParselet;
class UnaryOpParseletStorage;
struct GroupParselet;
class GroupParseletStorage;
struct ListParselet;
class ListParseletStorage;
struct MapParselet;
class MapParseletStorage;
struct BinaryOpParselet;
class BinaryOpParseletStorage;
struct CallParselet;
class CallParseletStorage;
struct IndexParselet;
class IndexParseletStorage;
struct MemberParselet;
class MemberParseletStorage;
struct Parser;
class ParserStorage;
struct FuncDef;
class FuncDefStorage;
struct ASTNode;
class ASTNodeStorage;
struct NumberNode;
class NumberNodeStorage;
struct StringNode;
class StringNodeStorage;
struct IdentifierNode;
class IdentifierNodeStorage;
struct AssignmentNode;
class AssignmentNodeStorage;
struct UnaryOpNode;
class UnaryOpNodeStorage;
struct BinaryOpNode;
class BinaryOpNodeStorage;
struct CallNode;
class CallNodeStorage;
struct GroupNode;
class GroupNodeStorage;
struct ListNode;
class ListNodeStorage;
struct MapNode;
class MapNodeStorage;
struct IndexNode;
class IndexNodeStorage;
struct MemberNode;
class MemberNodeStorage;
struct MethodCallNode;
class MethodCallNodeStorage;

// DECLARATIONS

















































// Parser: the main parsing engine.
// Uses a Pratt parser algorithm with parselets to handle operator precedence.
class ParserStorage {
	friend struct Parser;
	private: Lexer _lexer;
	private: Token _current;
	private: Boolean _hadError;
	private: List<String> _errors;
	private: Dictionary<TokenType, PrefixParselet> _prefixParselets;
	private: Dictionary<TokenType, InfixParselet> _infixParselets;

	// Parselet tables - indexed by TokenType

	public: ParserStorage();

	// Register all parselets
	private: void RegisterParselets();

	private: void RegisterPrefix(TokenType type, PrefixParselet parselet);

	private: void RegisterInfix(TokenType type, InfixParselet parselet);

	// Initialize the parser with source code
	public: void Init(String source);

	// Advance to the next token
	private: void Advance();

	// Check if current token matches the given type (without consuming)
	public: Boolean Check(TokenType type);

	// Check if current token matches and consume it if so
	public: Boolean Match(TokenType type);

	// Consume the current token (used by parselets)
	public: Token Consume();

	// Expect a specific token type, report error if not found
	public: Token Expect(TokenType type, String errorMessage);

	// Get the precedence of the infix parselet for the current token
	private: Precedence GetPrecedence();

	// Parse an expression with the given minimum precedence
	public: ASTNode ParseExpression(Precedence minPrecedence);

	// Parse an expression (convenience method with default precedence)
	public: ASTNode ParseExpression();

	// Parse a complete expression from source code
	public: ASTNode Parse(String source);

	// Report an error
	public: void ReportError(String message);

	// Check if any errors occurred
	public: Boolean HadError();

	// Get the list of errors
	public: List<String> GetErrors();
}; // end of class ParserStorage


// Parser: the main parsing engine.
// Uses a Pratt parser algorithm with parselets to handle operator precedence.
struct Parser : public IParser {
	protected: std::shared_ptr<ParserStorage> storage;
	public: Parser(std::shared_ptr<ParserStorage> stor) : storage(stor) {}
	private: ParserStorage* get();
	private: Lexer _lexer();
	private: void set__lexer(Lexer _v);
	private: Token _current();
	private: void set__current(Token _v);
	private: Boolean _hadError();
	private: void set__hadError(Boolean _v);
	private: List<String> _errors();
	private: void set__errors(List<String> _v);
	private: Dictionary<TokenType, PrefixParselet> _prefixParselets();
	private: void set__prefixParselets(Dictionary<TokenType, PrefixParselet> _v);
	private: Dictionary<TokenType, InfixParselet> _infixParselets();
	private: void set__infixParselets(Dictionary<TokenType, InfixParselet> _v);

	// Parselet tables - indexed by TokenType

	public: static Parser New() {
		return Parser(std::make_shared<ParserStorage>());
	}

	// Register all parselets
	private: void RegisterParselets() { return get()->RegisterParselets(); }

	private: void RegisterPrefix(TokenType type, PrefixParselet parselet) { return get()->RegisterPrefix(type, parselet); }

	private: void RegisterInfix(TokenType type, InfixParselet parselet) { return get()->RegisterInfix(type, parselet); }

	// Initialize the parser with source code
	public: void Init(String source) { return get()->Init(source); }

	// Advance to the next token
	private: void Advance() { return get()->Advance(); }

	// Check if current token matches the given type (without consuming)
	public: Boolean Check(TokenType type) { return get()->Check(type); }

	// Check if current token matches and consume it if so
	public: Boolean Match(TokenType type) { return get()->Match(type); }

	// Consume the current token (used by parselets)
	public: Token Consume() { return get()->Consume(); }

	// Expect a specific token type, report error if not found
	public: Token Expect(TokenType type, String errorMessage) { return get()->Expect(type, errorMessage); }

	// Get the precedence of the infix parselet for the current token
	private: Precedence GetPrecedence() { return get()->GetPrecedence(); }

	// Parse an expression with the given minimum precedence
	public: ASTNode ParseExpression(Precedence minPrecedence) { return get()->ParseExpression(minPrecedence); }

	// Parse an expression (convenience method with default precedence)
	public: ASTNode ParseExpression() { return get()->ParseExpression(); }

	// Parse a complete expression from source code
	public: ASTNode Parse(String source) { return get()->Parse(source); }

	// Report an error
	public: void ReportError(String message) { return get()->ReportError(message); }

	// Check if any errors occurred
	public: Boolean HadError() { return get()->HadError(); }

	// Get the list of errors
	public: List<String> GetErrors() { return get()->GetErrors(); }
}; // end of struct Parser


// INLINE METHODS

	ParserStorage* Parser::get() { return static_cast<ParserStorage*>(storage.get()); }
	Lexer Parser::_lexer() { return get()->_lexer; }
	void Parser::set__lexer(Lexer _v) { get()->_lexer = _v; }
	Token Parser::_current() { return get()->_current; }
	void Parser::set__current(Token _v) { get()->_current = _v; }
	Boolean Parser::_hadError() { return get()->_hadError; }
	void Parser::set__hadError(Boolean _v) { get()->_hadError = _v; }
	List<String> Parser::_errors() { return get()->_errors; }
	void Parser::set__errors(List<String> _v) { get()->_errors = _v; }
	Dictionary<TokenType, PrefixParselet> Parser::_prefixParselets() { return get()->_prefixParselets; }
	void Parser::set__prefixParselets(Dictionary<TokenType, PrefixParselet> _v) { get()->_prefixParselets = _v; }
	Dictionary<TokenType, InfixParselet> Parser::_infixParselets() { return get()->_infixParselets; }
	void Parser::set__infixParselets(Dictionary<TokenType, InfixParselet> _v) { get()->_infixParselets = _v; }

} // end of namespace MiniScript
