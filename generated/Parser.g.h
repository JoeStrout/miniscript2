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
#include "ErrorPool.g.h"


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
struct SelfParselet;
class SelfParseletStorage;
struct SuperParselet;
class SuperParseletStorage;
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
struct IndexedAssignmentNode;
class IndexedAssignmentNodeStorage;
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
struct SliceNode;
class SliceNodeStorage;
struct MemberNode;
class MemberNodeStorage;
struct MethodCallNode;
class MethodCallNodeStorage;
struct ExprCallNode;
class ExprCallNodeStorage;
struct WhileNode;
class WhileNodeStorage;
struct IfNode;
class IfNodeStorage;
struct ForNode;
class ForNodeStorage;
struct BreakNode;
class BreakNodeStorage;
struct ContinueNode;
class ContinueNodeStorage;
struct FunctionNode;
class FunctionNodeStorage;
struct SelfNode;
class SelfNodeStorage;
struct SuperNode;
class SuperNodeStorage;
struct ReturnNode;
class ReturnNodeStorage;

// DECLARATIONS






































































class ParserStorage : public std::enable_shared_from_this<ParserStorage>, public IParser {
	friend struct Parser;
	private: Lexer _lexer;
	private: Token _current;
	private: TokenType _previousType;
	public: ErrorPool Errors;
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

	// Advance to the next token, skipping comments and line continuations.
	// A line continuation is an EOL that follows a token which naturally
	// expects more input (comma, open bracket/paren/brace, binary operator).
	private: void Advance();

	// Return true if the given token type allows a line continuation after it.
	// That is, an EOL following this token should be silently ignored.
	private: static Boolean AllowsLineContinuation(TokenType type);

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
	// Handles: callStatement, assignmentStatement, breakStatement, continueStatement, expressionStatement
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
	// This includes simple statements, nested if statements, and return
	// (but not, for example, for/while loops, which are invalid
	// in the context of a single-line `if`).
	private: ASTNode ParseSingleLineStatement();

	// Parse single-line if body (after "if condition then ")
	private: ASTNode ParseSingleLineIfBody(ASTNode condition);

	// Parse a while statement: WHILE already consumed
	private: ASTNode ParseWhileStatement();

	// Parse a for statement: FOR already consumed
	// Syntax: for <identifier> in <expression> <EOL> <body> end for
	private: ASTNode ParseForStatement();

	// Parse a function expression: FUNCTION already consumed
	// Syntax: function(param1, param2, ...) <body> end function
	// The parentheses are optional for no-parameter functions.
	private: ASTNode ParseFunctionExpression();

	// Parse a statement (handles both simple statements and block statements)
	public: ASTNode ParseStatement();

	// Parse a program (grammar: program : (eol | statement)* EOF)
	// Returns a list of statement AST nodes
	public: List<ASTNode> ParseProgram();

	// Parse a complete source string (convenience method)
	// For single expressions/statements, returns the AST node
	public: ASTNode Parse(String source);

	// Describe a token for use in error messages, matching MiniScript 1.x format
	private: String TokenDescription(Token tok);

	// Format an error in the 1.x style: "got X where Y is required"
	private: String GotExpected(String expected);

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
	friend class ParserStorage;
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
	private: TokenType _previousType();
	private: void set__previousType(TokenType _v);
	public: ErrorPool Errors();
	public: void set_Errors(ErrorPool _v);
	private: Dictionary<TokenType, PrefixParselet> _prefixParselets();
	private: void set__prefixParselets(Dictionary<TokenType, PrefixParselet> _v);
	private: Dictionary<TokenType, InfixParselet> _infixParselets();
	private: void set__infixParselets(Dictionary<TokenType, InfixParselet> _v);

	// Parselet tables - indexed by TokenType

	public: static Parser New() {
		return Parser(std::make_shared<ParserStorage>());
	}

	// Register all parselets
	private: inline void RegisterParselets();

	private: inline void RegisterPrefix(TokenType type, PrefixParselet parselet);

	private: inline void RegisterInfix(TokenType type, InfixParselet parselet);

	// Initialize the parser with source code
	public: inline void Init(String source);

	// Advance to the next token, skipping comments and line continuations.
	// A line continuation is an EOL that follows a token which naturally
	// expects more input (comma, open bracket/paren/brace, binary operator).
	private: inline void Advance();

	// Return true if the given token type allows a line continuation after it.
	// That is, an EOL following this token should be silently ignored.
	private: static Boolean AllowsLineContinuation(TokenType type) { return ParserStorage::AllowsLineContinuation(type); }

	// Check if current token matches the given type (without consuming)
	public: inline Boolean Check(TokenType type);

	// Check if current token matches and consume it if so
	public: inline Boolean Match(TokenType type);

	// Consume the current token (used by parselets)
	public: inline Token Consume();

	// Expect a specific token type, report error if not found
	public: inline Token Expect(TokenType type, String errorMessage);

	// Get the precedence of the infix parselet for the current token
	private: inline Precedence GetPrecedence();

	// Check if a token type can start an expression
	public: inline Boolean CanStartExpression(TokenType type);

	// Parse an expression with the given minimum precedence (Pratt parser core)
	public: inline ASTNode ParseExpression(Precedence minPrecedence);

	// Parse an expression (convenience method with default precedence)
	public: inline ASTNode ParseExpression();

	// Continue parsing an expression given a starting left operand.
	// Used when we've already consumed a token (like an identifier) and need
	// to continue with any infix operators that follow.
	private: inline ASTNode ParseExpressionFrom(ASTNode left);

	// Parse a simple statement (grammar: simpleStatement)
	// Handles: callStatement, assignmentStatement, breakStatement, continueStatement, expressionStatement
	private: inline ASTNode ParseSimpleStatement();

	// Check if current token is a block terminator
	private: inline Boolean IsBlockTerminator(TokenType t1, TokenType t2);

	// Parse a block of statements until we hit a terminator token.
	// Used for block bodies in while, if, for, function, etc.
	// Terminators are not consumed.
	private: inline List<ASTNode> ParseBlock(TokenType terminator1, TokenType terminator2);

	// Require "end <keyword>" and consume it, reporting error if not found
	private: inline void RequireEndKeyword(TokenType keyword, String keywordName);

	// Parse an if statement: IF already consumed
	// Handles both block form and single-line form
	private: inline ASTNode ParseIfStatement();

	// Parse else/else-if clause for block if statements
	// Returns the else body (which may contain a nested IfNode for else-if)
	private: inline List<ASTNode> ParseElseClause();

	// Parse a statement that can appear in single-line if context
	// This includes simple statements, nested if statements, and return
	// (but not, for example, for/while loops, which are invalid
	// in the context of a single-line `if`).
	private: inline ASTNode ParseSingleLineStatement();

	// Parse single-line if body (after "if condition then ")
	private: inline ASTNode ParseSingleLineIfBody(ASTNode condition);

	// Parse a while statement: WHILE already consumed
	private: inline ASTNode ParseWhileStatement();

	// Parse a for statement: FOR already consumed
	// Syntax: for <identifier> in <expression> <EOL> <body> end for
	private: inline ASTNode ParseForStatement();

	// Parse a function expression: FUNCTION already consumed
	// Syntax: function(param1, param2, ...) <body> end function
	// The parentheses are optional for no-parameter functions.
	private: inline ASTNode ParseFunctionExpression();

	// Parse a statement (handles both simple statements and block statements)
	public: inline ASTNode ParseStatement();

	// Parse a program (grammar: program : (eol | statement)* EOF)
	// Returns a list of statement AST nodes
	public: inline List<ASTNode> ParseProgram();

	// Parse a complete source string (convenience method)
	// For single expressions/statements, returns the AST node
	public: inline ASTNode Parse(String source);

	// Describe a token for use in error messages, matching MiniScript 1.x format
	private: inline String TokenDescription(Token tok);

	// Format an error in the 1.x style: "got X where Y is required"
	private: inline String GotExpected(String expected);

	// Report an error
	public: inline void ReportError(String message);

	// Check if any errors occurred
	public: inline Boolean HadError();

	// Get the list of errors
	public: inline List<String> GetErrors();
}; // end of struct Parser


// INLINE METHODS

inline ParserStorage* Parser::get() const { return static_cast<ParserStorage*>(storage.get()); }
inline Lexer Parser::_lexer() { return get()->_lexer; }
inline void Parser::set__lexer(Lexer _v) { get()->_lexer = _v; }
inline Token Parser::_current() { return get()->_current; }
inline void Parser::set__current(Token _v) { get()->_current = _v; }
inline TokenType Parser::_previousType() { return get()->_previousType; }
inline void Parser::set__previousType(TokenType _v) { get()->_previousType = _v; }
inline ErrorPool Parser::Errors() { return get()->Errors; }
inline void Parser::set_Errors(ErrorPool _v) { get()->Errors = _v; }
inline Dictionary<TokenType, PrefixParselet> Parser::_prefixParselets() { return get()->_prefixParselets; }
inline void Parser::set__prefixParselets(Dictionary<TokenType, PrefixParselet> _v) { get()->_prefixParselets = _v; }
inline Dictionary<TokenType, InfixParselet> Parser::_infixParselets() { return get()->_infixParselets; }
inline void Parser::set__infixParselets(Dictionary<TokenType, InfixParselet> _v) { get()->_infixParselets = _v; }
inline void Parser::RegisterParselets() { return get()->RegisterParselets(); }
inline void Parser::RegisterPrefix(TokenType type, PrefixParselet parselet) { return get()->RegisterPrefix(type, parselet); }
inline void Parser::RegisterInfix(TokenType type, InfixParselet parselet) { return get()->RegisterInfix(type, parselet); }
inline void Parser::Init(String source) { return get()->Init(source); }
inline void Parser::Advance() { return get()->Advance(); }
inline Boolean Parser::Check(TokenType type) { return get()->Check(type); }
inline Boolean Parser::Match(TokenType type) { return get()->Match(type); }
inline Token Parser::Consume() { return get()->Consume(); }
inline Token Parser::Expect(TokenType type, String errorMessage) { return get()->Expect(type, errorMessage); }
inline Precedence Parser::GetPrecedence() { return get()->GetPrecedence(); }
inline Boolean Parser::CanStartExpression(TokenType type) { return get()->CanStartExpression(type); }
inline ASTNode Parser::ParseExpression(Precedence minPrecedence) { return get()->ParseExpression(minPrecedence); }
inline ASTNode Parser::ParseExpression() { return get()->ParseExpression(); }
inline ASTNode Parser::ParseExpressionFrom(ASTNode left) { return get()->ParseExpressionFrom(left); }
inline ASTNode Parser::ParseSimpleStatement() { return get()->ParseSimpleStatement(); }
inline Boolean Parser::IsBlockTerminator(TokenType t1, TokenType t2) { return get()->IsBlockTerminator(t1, t2); }
inline List<ASTNode> Parser::ParseBlock(TokenType terminator1, TokenType terminator2) { return get()->ParseBlock(terminator1, terminator2); }
inline void Parser::RequireEndKeyword(TokenType keyword, String keywordName) { return get()->RequireEndKeyword(keyword, keywordName); }
inline ASTNode Parser::ParseIfStatement() { return get()->ParseIfStatement(); }
inline List<ASTNode> Parser::ParseElseClause() { return get()->ParseElseClause(); }
inline ASTNode Parser::ParseSingleLineStatement() { return get()->ParseSingleLineStatement(); }
inline ASTNode Parser::ParseSingleLineIfBody(ASTNode condition) { return get()->ParseSingleLineIfBody(condition); }
inline ASTNode Parser::ParseWhileStatement() { return get()->ParseWhileStatement(); }
inline ASTNode Parser::ParseForStatement() { return get()->ParseForStatement(); }
inline ASTNode Parser::ParseFunctionExpression() { return get()->ParseFunctionExpression(); }
inline ASTNode Parser::ParseStatement() { return get()->ParseStatement(); }
inline List<ASTNode> Parser::ParseProgram() { return get()->ParseProgram(); }
inline ASTNode Parser::Parse(String source) { return get()->Parse(source); }
inline String Parser::TokenDescription(Token tok) { return get()->TokenDescription(tok); }
inline String Parser::GotExpected(String expected) { return get()->GotExpected(expected); }
inline void Parser::ReportError(String message) { return get()->ReportError(message); }
inline Boolean Parser::HadError() { return get()->HadError(); }
inline List<String> Parser::GetErrors() { return get()->GetErrors(); }

} // end of namespace MiniScript
