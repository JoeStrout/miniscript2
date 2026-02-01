// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CodeGenerator.cs

#pragma once
#include "core_includes.h"
// CodeGenerator.cs - Compiles AST nodes to bytecode using the visitor pattern
// Uses ICodeEmitter to support both direct bytecode and assembly text output.


namespace MiniScript {

// FORWARD DECLARATIONS

struct CodeGenerator;
class CodeGeneratorStorage;
struct VM;
class VMStorage;
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

// DECLARATIONS























































// Compiles AST nodes to bytecode
class CodeGeneratorStorage : public std::enable_shared_from_this<CodeGeneratorStorage>, IASTVisitor {
	friend struct CodeGenerator;
	private: ICodeEmitter _emitter;
	private: Int32 _nextReg; // Next available register
	private: Int32 _maxRegUsed; // High water mark for register usage

	public: CodeGeneratorStorage(ICodeEmitter& emitter);

	// Allocate a register
	private: Int32 AllocReg();

	// Free a register (only if it's the most recently allocated)
	// This enables simple stack-based register reuse
	// ToDo: consider more sophisticated register-in-use tracking,
	// so we don't have to free registers in reverse order.
	// Or possibly something that frees all registers allocated
	// after a certain point.
	private: void FreeReg(Int32 reg);

	// Compile an expression, placing result in a newly allocated register
	// Returns the register number holding the result
	public: Int32 Compile(ASTNode ast);

	// Compile a complete function from an expression
	public: FuncDef CompileFunction(ASTNode ast, String funcName);

	// --- Visit methods for each AST node type ---

	public: Int32 Visit(NumberNode node);

	public: Int32 Visit(StringNode node);

	public: Int32 Visit(IdentifierNode node);

	public: Int32 Visit(AssignmentNode node);

	public: Int32 Visit(UnaryOpNode node);

	public: Int32 Visit(BinaryOpNode node);

	public: Int32 Visit(CallNode node);

	public: Int32 Visit(GroupNode node);

	public: Int32 Visit(ListNode node);

	public: Int32 Visit(MapNode node);

	public: Int32 Visit(IndexNode node);

	public: Int32 Visit(MemberNode node);

	public: Int32 Visit(MethodCallNode node);
}; // end of class CodeGeneratorStorage


// Compiles AST nodes to bytecode
struct CodeGenerator : public IASTVisitor {
	protected: std::shared_ptr<CodeGeneratorStorage> storage;
	public: CodeGenerator(std::shared_ptr<CodeGeneratorStorage> stor) : storage(stor) {}
	private: CodeGeneratorStorage* get();
	private: ICodeEmitter _emitter();
	private: void set__emitter(ICodeEmitter _v);
	private: Int32 _nextReg(); // Next available register
	private: void set__nextReg(Int32 _v); // Next available register
	private: Int32 _maxRegUsed(); // High water mark for register usage
	private: void set__maxRegUsed(Int32 _v); // High water mark for register usage

	public: static CodeGenerator New(ICodeEmitter& emitter) {
		return CodeGenerator(std::make_shared<CodeGeneratorStorage>(emitter));
	}

	// Allocate a register
	private: Int32 AllocReg() { return get()->AllocReg(); }

	// Free a register (only if it's the most recently allocated)
	// This enables simple stack-based register reuse
	// ToDo: consider more sophisticated register-in-use tracking,
	// so we don't have to free registers in reverse order.
	// Or possibly something that frees all registers allocated
	// after a certain point.
	private: void FreeReg(Int32 reg) { return get()->FreeReg(reg); }

	// Compile an expression, placing result in a newly allocated register
	// Returns the register number holding the result
	public: Int32 Compile(ASTNode ast) { return get()->Compile(ast); }

	// Compile a complete function from an expression
	public: FuncDef CompileFunction(ASTNode ast, String funcName) { return get()->CompileFunction(ast, funcName); }

	// --- Visit methods for each AST node type ---

	public: Int32 Visit(NumberNode node) { return get()->Visit(node); }

	public: Int32 Visit(StringNode node) { return get()->Visit(node); }

	public: Int32 Visit(IdentifierNode node) { return get()->Visit(node); }

	public: Int32 Visit(AssignmentNode node) { return get()->Visit(node); }

	public: Int32 Visit(UnaryOpNode node) { return get()->Visit(node); }

	public: Int32 Visit(BinaryOpNode node) { return get()->Visit(node); }

	public: Int32 Visit(CallNode node) { return get()->Visit(node); }

	public: Int32 Visit(GroupNode node) { return get()->Visit(node); }

	public: Int32 Visit(ListNode node) { return get()->Visit(node); }

	public: Int32 Visit(MapNode node) { return get()->Visit(node); }

	public: Int32 Visit(IndexNode node) { return get()->Visit(node); }

	public: Int32 Visit(MemberNode node) { return get()->Visit(node); }

	public: Int32 Visit(MethodCallNode node) { return get()->Visit(node); }
}; // end of struct CodeGenerator


// INLINE METHODS

inline CodeGeneratorStorage* CodeGenerator::get() { return static_cast<CodeGeneratorStorage*>(storage.get()); }
inline ICodeEmitter CodeGenerator::_emitter() { return get()->_emitter; }
inline void CodeGenerator::set__emitter(ICodeEmitter _v) { get()->_emitter = _v; }
inline Int32 CodeGenerator::_nextReg() { return get()->_nextReg; } // Next available register
inline void CodeGenerator::set__nextReg(Int32 _v) { get()->_nextReg = _v; } // Next available register
inline Int32 CodeGenerator::_maxRegUsed() { return get()->_maxRegUsed; } // High water mark for register usage
inline void CodeGenerator::set__maxRegUsed(Int32 _v) { get()->_maxRegUsed = _v; } // High water mark for register usage

} // end of namespace MiniScript
