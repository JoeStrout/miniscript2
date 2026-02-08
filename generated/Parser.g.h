// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Parser.cs

#pragma once
#include "core_includes.h"
// Parser.cs - Pratt parser for MiniScript
// Uses parselets to handle operator precedence and associativity.
// Structure follows the grammar in miniscript.g4:
//   program : (eol | statement)* EOF
//   statement : simpleStatement eol
//   simpleStatement : callStatement | assignmentStatement | expressionStatement
//   callStatement : expression '(' argList ')' | expression argList
//   expressionStatement : expression

#include "LangConstants.g.h"
#include "Lexer.g.h"
#include "Parselet.g.h"


namespace MiniScript {

// FORWARD DECLARATIONS

struct CodeGenerator;
class CodeGeneratorStorage;
struct VM;
class VMStorage;
struct CodeEmitterBase;
class CodeEmitterBaseStorage;
struct BytecodeEmitter;
class BytecodeEmitterStorage;
struct AssemblyEmitter;
class AssemblyEmitterStorage;
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
struct WhileNode;
class WhileNodeStorage;
struct IfNode;
class IfNodeStorage;
struct BreakNode;
class BreakNodeStorage;

// DECLARATIONS


























































class ParserStorage : public std::enable_shared_from_this<ParserStorage>, public IParser {
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

	// Advance to the next token, skipping comments
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

	// Check if a token type can start an expression
	public: Boolean CanStartExpression(TokenType type);

	// Parse an expression with the given minimum precedence (Pratt parser core)
	public: ASTNode ParseExpression(Precedence minPrecedence);

	// Parse an expression (convenience method with default precedence)
	public: ASTNode ParseExpression();

	// Continue parsing an expression given a starting left operand.
	// Used when we've already consumed a token (like an identifier) and need
	// to continue with any infix operators that follow.
	private: ASTNode ParseExpressionFrom(ASTNode left);

	// Parse a simple statement (grammar: simpleStatement)
	// Handles: callStatement, assignmentStatement, breakStatement, expressionStatement
	private: ASTNode ParseSimpleStatement();

	// Check if current token is a block terminator
	private: Boolean IsBlockTerminator(TokenType t1, TokenType t2);

	// Parse a block of statements until we hit a terminator token.
	// Used for block bodies in while, if, for, function, etc.
	// Terminators are not consumed.
	private: List<ASTNode> ParseBlock(TokenType terminator1, TokenType terminator2);

	// Require "end <keyword>" and consume it, reporting error if not found
	private: void RequireEndKeyword(TokenType keyword, String keywordName);

	// Parse an if statement: IF already consumed
	// Handles both block form and single-line form
	private: ASTNode ParseIfStatement();

	// Parse else/else-if clause for block if statements
	// Returns the else body (which may contain a nested IfNode for else-if)
	private: List<ASTNode> ParseElseClause();

	// Parse a statement that can appear in single-line if context
	// This includes simple statements AND nested if statements
	// (but not, for example, for/while loops, which are invalid 
	// in the context of a single-line `if`).
	private: ASTNode ParseSingleLineStatement();

	// Parse single-line if body (after "if condition then ")
	private: ASTNode ParseSingleLineIfBody(ASTNode condition);

	// Parse a while statement: WHILE already consumed
	private: ASTNode ParseWhileStatement();

	// Parse a statement (handles both simple statements and block statements)
	public: ASTNode ParseStatement();

	// Parse a program (grammar: program : (eol | statement)* EOF)
	// Returns a list of statement AST nodes
	public: List<ASTNode> ParseProgram();

	// Parse a complete source string (convenience method)
	// For single expressions/statements, returns the AST node
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
  public:
	Parser(std::shared_ptr<ParserStorage> stor) : storage(stor) {}
	Parser() : storage(nullptr) {}
	Parser(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const Parser& inst) { return inst.storage == nullptr; }
	private: ParserStorage* get() const;

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

	// Advance to the next token, skipping comments
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

	// Check if a token type can start an expression
	public: Boolean CanStartExpression(TokenType type) { return get()->CanStartExpression(type); }

	// Parse an expression with the given minimum precedence (Pratt parser core)
	public: ASTNode ParseExpression(Precedence minPrecedence) { return get()->ParseExpression(minPrecedence); }

	// Parse an expression (convenience method with default precedence)
	public: ASTNode ParseExpression() { return get()->ParseExpression(); }

	// Continue parsing an expression given a starting left operand.
	// Used when we've already consumed a token (like an identifier) and need
	// to continue with any infix operators that follow.
	private: ASTNode ParseExpressionFrom(ASTNode left) { return get()->ParseExpressionFrom(left); }

	// Parse a simple statement (grammar: simpleStatement)
	// Handles: callStatement, assignmentStatement, breakStatement, expressionStatement
	private: ASTNode ParseSimpleStatement() { return get()->ParseSimpleStatement(); }

	// Check if current token is a block terminator
	private: Boolean IsBlockTerminator(TokenType t1, TokenType t2) { return get()->IsBlockTerminator(t1, t2); }

	// Parse a block of statements until we hit a terminator token.
	// Used for block bodies in while, if, for, function, etc.
	// Terminators are not consumed.
	private: List<ASTNode> ParseBlock(TokenType terminator1, TokenType terminator2) { return get()->ParseBlock(terminator1, terminator2); }

	// Require "end <keyword>" and consume it, reporting error if not found
	private: void RequireEndKeyword(TokenType keyword, String keywordName) { return get()->RequireEndKeyword(keyword, keywordName); }

	// Parse an if statement: IF already consumed
	// Handles both block form and single-line form
	private: ASTNode ParseIfStatement() { return get()->ParseIfStatement(); }

	// Parse else/else-if clause for block if statements
	// Returns the else body (which may contain a nested IfNode for else-if)
	private: List<ASTNode> ParseElseClause() { return get()->ParseElseClause(); }

	// Parse a statement that can appear in single-line if context
	// This includes simple statements AND nested if statements
	// (but not, for example, for/while loops, which are invalid 
	// in the context of a single-line `if`).
	private: ASTNode ParseSingleLineStatement() { return get()->ParseSingleLineStatement(); }

	// Parse single-line if body (after "if condition then ")
	private: ASTNode ParseSingleLineIfBody(ASTNode condition) { return get()->ParseSingleLineIfBody(condition); }

	// Parse a while statement: WHILE already consumed
	private: ASTNode ParseWhileStatement() { return get()->ParseWhileStatement(); }

	// Parse a statement (handles both simple statements and block statements)
	public: ASTNode ParseStatement() { return get()->ParseStatement(); }

	// Parse a program (grammar: program : (eol | statement)* EOF)
	// Returns a list of statement AST nodes
	public: List<ASTNode> ParseProgram() { return get()->ParseProgram(); }

	// Parse a complete source string (convenience method)
	// For single expressions/statements, returns the AST node
	public: ASTNode Parse(String source) { return get()->Parse(source); }

	// Report an error
	public: void ReportError(String message) { return get()->ReportError(message); }

	// Check if any errors occurred
	public: Boolean HadError() { return get()->HadError(); }

	// Get the list of errors
	public: List<String> GetErrors() { return get()->GetErrors(); }
}; // end of struct Parser


// INLINE METHODS

inline ParserStorage* Parser::get() const { return static_cast<ParserStorage*>(storage.get()); }
inline Lexer Parser::_lexer() { return get()->_lexer; }
inline void Parser::set__lexer(Lexer _v) { get()->_lexer = _v; }
inline Token Parser::_current() { return get()->_current; }
inline void Parser::set__current(Token _v) { get()->_current = _v; }
inline Boolean Parser::_hadError() { return get()->_hadError; }
inline void Parser::set__hadError(Boolean _v) { get()->_hadError = _v; }
inline List<String> Parser::_errors() { return get()->_errors; }
inline void Parser::set__errors(List<String> _v) { get()->_errors = _v; }
inline Dictionary<TokenType, PrefixParselet> Parser::_prefixParselets() { return get()->_prefixParselets; }
inline void Parser::set__prefixParselets(Dictionary<TokenType, PrefixParselet> _v) { get()->_prefixParselets = _v; }
inline Dictionary<TokenType, InfixParselet> Parser::_infixParselets() { return get()->_infixParselets; }
inline void Parser::set__infixParselets(Dictionary<TokenType, InfixParselet> _v) { get()->_infixParselets = _v; }

} // end of namespace MiniScript
