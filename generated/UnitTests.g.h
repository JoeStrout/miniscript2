// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: UnitTests.cs

#pragma once
#include "core_includes.h"
// This module gathers together all the unit tests for this prototype.
// Each test returns true on success, false on failure.


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









class UnitTests {

	public: static Boolean Assert(bool condition, String message);
	
	public: static Boolean AssertEqual(String actual, String expected);
		
	public: static Boolean AssertEqual(UInt32 actual, UInt32 expected);
		
	public: static Boolean AssertEqual(Int32 actual, Int32 expected);
		
	public: static Boolean AssertEqual(List<String> actual, List<String> expected);
		
	public: static Boolean TestStringUtils();
	
	public: static Boolean TestDisassembler();
	
	public: static Boolean TestAssembler();

	public: static Boolean TestValueMap();

	// Helper for parser tests: parse, simplify, and check result
	private: static Boolean CheckParse(Parser parser, String input, String expected);

	public: static Boolean TestParser();

	public: static Boolean RunAll();
}; // end of struct UnitTests









































// INLINE METHODS

} // end of namespace MiniScript
