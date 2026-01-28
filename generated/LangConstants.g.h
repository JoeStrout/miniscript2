// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: LangConstants.cs

#pragma once
#include "core_includes.h"
// Constants used as part of the language definition: token types,
// operator precedence, that sort of thing.


namespace MiniScript {

// FORWARD DECLARATIONS

struct VM;
class VMStorage;
struct Assembler;
class AssemblerStorage;
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







// Precedence levels (higher precedence binds more strongly)
enum class Precedence : Int32 {
	NONE = 0,
	ASSIGNMENT = 1,
	OR = 2,
	AND = 3,
	EQUALITY = 4,        // == !=
	COMPARISON = 5,      // < > <= >=
	SUM = 6,             // + -
	PRODUCT = 7,         // * / %
	POWER = 8,           // ^
	UNARY = 9,           // - not
	CALL = 10,           // () []
	PRIMARY = 11
}; // end of enum Precedence


// Token types returned by the lexer
enum class TokenType : Int32 {
	END_OF_INPUT,
	NUMBER,
	STRING,
	IDENTIFIER,
	PLUS,
	MINUS,
	TIMES,
	DIVIDE,
	MOD,
	CARET,
	LPAREN,
	RPAREN,
	LBRACKET,
	RBRACKET,
	LBRACE,
	RBRACE,
	ASSIGN,
	EQUALS,
	NOT_EQUAL,
	LESS_THAN,
	GREATER_THAN,
	LESS_EQUAL,
	GREATER_EQUAL,
	COMMA,
	COLON,
	SEMICOLON,
	DOT,
	NOT,
	AND,
	OR,
	EOL,
	ERROR
}; // end of enum TokenType



























// INLINE METHODS

} // end of namespace MiniScript
