// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IntrinsicAPI.cs

#pragma once
#include "core_includes.h"
#include "value.h"

namespace MiniScript {
class VMStorage;
typedef VMStorage& VMRef;

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
struct ScopeParselet;
class ScopeParseletStorage;
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
struct ScopeNode;
class ScopeNodeStorage;
struct ReturnNode;
class ReturnNodeStorage;

// DECLARATIONS

// Context passed to native (intrinsic) callback functions.  This gives the
// intrinsic function access to the call arguments, as well as the VM.
struct Context {
	public: VMRef vm;
	public: List<Value> stack;
	public: Int32 baseIndex; // index of return register; arguments follow this
	public: Int32 argCount; // how many arguments we have
	public: Context(VMRef vm, List<Value> stack, Int32 baseIndex, Int32 argCount)
		: vm(vm), stack(stack), baseIndex(baseIndex), argCount(argCount) {
	}
	
	
	// Get an argument from the stack, by number (the first argument is index 0,
	// the second is index 1, etc.).
	public: Value GetArg(int zeroBasedIndex);
	
	// Look up a variable or parameter by name.  This is provided mostly for
	// compatibility with MiniScript 1.x; in most cases, intrinsics only need
	// argument values, which are far more efficiently found via GetArg (above).
	public: Value GetVar(String variableName);
}; // end of struct Context

// IntrinsicResult: represents the result of calling an intrinsic function
// (i.e. a function defined by the host app, for use in MiniScript code).
// This may be a final or "done" result, containing the return value of
// the intrinsic; or it may be a partial or "not done yet" result, in which
// case the intrinsic will be invoked again, with this partial result
// passed back so the intrinsic can continue its work.
struct IntrinsicResult {
	public: Boolean done; // set to true if done, false if there is pending work
	public: Value result; // final result if done; in-progress data if not done

	public: IntrinsicResult(Value result, Boolean done = true);

	// For backwards compatibility with 1.x:
	public: Boolean Done();
	public: static const IntrinsicResult Null;
	public: static const IntrinsicResult EmptyString;
	public: static const IntrinsicResult Zero;
	public: static const IntrinsicResult One;

	// Some standard results you can efficiently use:
}; // end of struct IntrinsicResult

// INLINE METHODS

inline Value Context::GetArg(int zeroBasedIndex) {
	// The base index is the position of the return register; the arguments
	// start right after that.  Note that we don't do any range checking here;
	// be careful not to ask for arguments beyond the declared parameters.
	return stack[baseIndex + 1 + zeroBasedIndex];
}

inline Boolean IntrinsicResult::Done() {
	return done;
}

} // end of namespace MiniScript
