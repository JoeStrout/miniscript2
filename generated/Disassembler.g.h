// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Disassembler.cs

#pragma once
#include "core_includes.h"
#include "Bytecode.g.h"

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

// DECLARATIONS


class Disassembler {

	// Return the short pseudo-opcode for the given instruction
	// (e.g.: LOAD instead of LOAD_rA_iBC, etc.)
	public: static String AssemOp(Opcode opcode);
	
	public: static String ToString(UInt32 instruction);

	// Disassemble the given function.  If detailed=true, include extra
	// details for debugging, like line numbers and instruction hex code.
	public: static void Disassemble(FuncDef funcDef, List<String> output, Boolean detailed=true);

	// Disassemble the whole program (list of functions).  If detailed=true, include 
	// extra details for debugging, like line numbers and instruction hex code.
	public: static List<String> Disassemble(List<FuncDef> functions, Boolean detailed=true);

}; // end of struct Disassembler
























































// INLINE METHODS

} // end of namespace MiniScript
