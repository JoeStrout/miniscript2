// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CodeGenerator.cs

#pragma once
#include "core_includes.h"
// CodeGenerator.cs - Compiles AST nodes to bytecode using the visitor pattern
// Uses CodeEmitterBase to support both direct bytecode and assembly text output.

#include "AST.g.h"
#include "CodeEmitter.g.h"

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

// DECLARATIONS























































class CodeGeneratorStorage : public std::enable_shared_from_this<CodeGeneratorStorage>, public IASTVisitor {
	friend struct CodeGenerator;
	private: CodeEmitterBase _emitter;
	private: List<Boolean> _regInUse; // Which registers are currently in use
	private: Int32 _firstAvailable; // Lowest index that might be free
	private: Int32 _maxRegUsed; // High water mark for register usage

	public: CodeGeneratorStorage(CodeEmitterBase emitter);

	// Allocate a register
	private: Int32 AllocReg();

	// Free a register so it can be reused
	private: void FreeReg(Int32 reg);

	// Compile an expression, placing result in a newly allocated register
	// Returns the register number holding the result
	public: Int32 Compile(ASTNode ast);

	// Compile a complete function from a single expression/statement
	public: FuncDef CompileFunction(ASTNode ast, String funcName);

	// Compile a complete function from a list of statements (program)
	public: FuncDef CompileProgram(List<ASTNode> statements, String funcName);

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
  public:
	CodeGenerator(std::shared_ptr<CodeGeneratorStorage> stor) : storage(stor) {}
	CodeGenerator() : storage(nullptr) {}
	CodeGenerator(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const CodeGenerator& inst) { return inst.storage == nullptr; }
	private: CodeGeneratorStorage* get() const;

	private: CodeEmitterBase _emitter();
	private: void set__emitter(CodeEmitterBase _v);
	private: List<Boolean> _regInUse(); // Which registers are currently in use
	private: void set__regInUse(List<Boolean> _v); // Which registers are currently in use
	private: Int32 _firstAvailable(); // Lowest index that might be free
	private: void set__firstAvailable(Int32 _v); // Lowest index that might be free
	private: Int32 _maxRegUsed(); // High water mark for register usage
	private: void set__maxRegUsed(Int32 _v); // High water mark for register usage

	public: static CodeGenerator New(CodeEmitterBase emitter) {
		return CodeGenerator(std::make_shared<CodeGeneratorStorage>(emitter));
	}

	// Allocate a register
	private: Int32 AllocReg() { return get()->AllocReg(); }

	// Free a register so it can be reused
	private: void FreeReg(Int32 reg) { return get()->FreeReg(reg); }

	// Compile an expression, placing result in a newly allocated register
	// Returns the register number holding the result
	public: Int32 Compile(ASTNode ast) { return get()->Compile(ast); }

	// Compile a complete function from a single expression/statement
	public: FuncDef CompileFunction(ASTNode ast, String funcName) { return get()->CompileFunction(ast, funcName); }

	// Compile a complete function from a list of statements (program)
	public: FuncDef CompileProgram(List<ASTNode> statements, String funcName) { return get()->CompileProgram(statements, funcName); }

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

inline CodeGeneratorStorage* CodeGenerator::get() const { return static_cast<CodeGeneratorStorage*>(storage.get()); }
inline CodeEmitterBase CodeGenerator::_emitter() { return get()->_emitter; }
inline void CodeGenerator::set__emitter(CodeEmitterBase _v) { get()->_emitter = _v; }
inline List<Boolean> CodeGenerator::_regInUse() { return get()->_regInUse; } // Which registers are currently in use
inline void CodeGenerator::set__regInUse(List<Boolean> _v) { get()->_regInUse = _v; } // Which registers are currently in use
inline Int32 CodeGenerator::_firstAvailable() { return get()->_firstAvailable; } // Lowest index that might be free
inline void CodeGenerator::set__firstAvailable(Int32 _v) { get()->_firstAvailable = _v; } // Lowest index that might be free
inline Int32 CodeGenerator::_maxRegUsed() { return get()->_maxRegUsed; } // High water mark for register usage
inline void CodeGenerator::set__maxRegUsed(Int32 _v) { get()->_maxRegUsed = _v; } // High water mark for register usage

} // end of namespace MiniScript
