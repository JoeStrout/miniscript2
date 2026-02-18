// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VMVis.cs

#pragma once
#include "core_includes.h"
#include "VM.g.h"



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








struct VMVis {
	private: static const String Esc;
	private: static const String Clear;
	private: static const String Reset;
	private: static const String Bold;
	private: static const String Dim;
	private: static const String Underline;
	private: static const String Inverse;
	private: static const String Normal;
	private: static const String CursorHome;
	private: static const Int32 CodeDisplayColumn;
	private: static const Int32 RegisterDisplayColumn;
	private: static const Int32 CallStackDisplayColumn;
	private: Int32 _screenWidth = 0;
	private: Int32 _screenHeight = 0;
	private: VM _vm;
	

	
	public: VMVis(VM vm);

	public: void UpdateScreenSize();


	public: String CursorGoTo(int column, int row);

	private: void Write(String s);

	public: void ClearScreen();
	

	public: void GoTo(int column, int row);

	private: void DrawCodeDisplay();

	private: String GetValueTypeCode(Value v);

	private: String GetValueDisplayString(Value v);

	private: String GetVariableNameDisplay(Value nameVal);

	private: void DrawOneRegister(Int32 stackIndex, String label, Int32 displayRow);

	private: void DrawRegisters();

	private: void DrawCallStack();

	public: void UpdateDisplay();
}; // end of struct VMVis
































































// INLINE METHODS

} // end of namespace MiniScript
