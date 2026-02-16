// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: LangConstants.cs

#pragma once
#include "core_includes.h"
// Constants used as part of the language definition: token types,
// operator precedence, that sort of thing.


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
struct ReturnNode;
class ReturnNodeStorage;

// DECLARATIONS









// Precedence levels (higher precedence binds more strongly)
enum class Precedence : Int32 {
	NONE = 0,
	ASSIGNMENT,
	OR,
	AND,
	NOT,
	EQUALITY,        // == !=
	COMPARISON,      // < > <= >=
	SUM,             // + -
	PRODUCT,         // * / %
	UNARY_MINUS,     // -
	POWER,           // ^
	CALL,            // () []
	ADDRESS_OF,      // @
	PRIMARY
}; // end of enum Precedence


// Token types returned by the lexer
enum class TokenType : Int32 {
	END_OF_INPUT,
	NUMBER,
	STRING,
	IDENTIFIER,
	ADDRESS_OF,
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
	DOT,
	NOT,
	AND,
	OR,
	WHILE,
	FOR,
	IN,
	IF,
	THEN,
	ELSE,
	BREAK,
	CONTINUE,
	FUNCTION,
	RETURN,
	NEW,
	ISA,
	END,
	EOL,
	COMMENT,
	ERROR
}; // end of enum TokenType

























































// INLINE METHODS

} // end of namespace MiniScript
