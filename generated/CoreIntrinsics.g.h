// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CoreIntrinsics.cs

#pragma once
#include "core_includes.h"
// CoreIntrinsics.cs - Definitions of all built-in intrinsic functions.

#include "value.h"
#include "Intrinsic.g.h"

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
struct ComparisonParselet;
class ComparisonParseletStorage;
struct CallParselet;
class CallParseletStorage;
struct IndexParselet;
class IndexParseletStorage;
struct MemberParselet;
class MemberParseletStorage;
struct Intrinsic;
class IntrinsicStorage;
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
struct ComparisonChainNode;
class ComparisonChainNodeStorage;
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

class CoreIntrinsics {

	
	// If given a nonzero seed, seed our PRNG accordingly.
	// Then (in either case), return the next random number drawn
	// from the range [0, 1) with a uniform distribution.
	private: static double GetNextRandom(int seed=0);

	private: static void AddIntrinsicToMap(Value map, String methodName);

	/// <summary>
	/// ListType: a static map that represents the List type, and provides
	/// intrinsic methods that can be invoked on it via dot syntax.
	/// </summary>
	public: static Value ListType();
	private: static Value _listType;

	/// <summary>
	/// StringType: a static map that represents the String type, and provides
	/// intrinsic methods that can be invoked on it via dot syntax.
	/// </summary>
	public: static Value StringType();
	private: static Value _stringType;

	/// <summary>
	/// MapType: a static map that represents the Map type, and provides
	/// intrinsic methods that can be invoked on it via dot syntax.
	/// </summary>
	public: static Value MapType();
	private: static Value _mapType;
	
	/// <summary>
	/// NumberType: a static map that represents the Number type.
	/// </summary>
	public: static Value NumberType();
	private: static Value _numberType;

	public: static void Init();

}; // end of struct CoreIntrinsics

// INLINE METHODS

} // end of namespace MiniScript
