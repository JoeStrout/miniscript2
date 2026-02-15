// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CodeGenerator.cs

#pragma once
#include "core_includes.h"
// CodeGenerator.cs - Compiles AST nodes to bytecode using the visitor pattern
// Uses CodeEmitterBase to support both direct bytecode and assembly text output.

#include "AST.g.h"
#include "CodeEmitter.g.h"
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

































































class CodeGeneratorStorage : public std::enable_shared_from_this<CodeGeneratorStorage>, public IASTVisitor {
	friend struct CodeGenerator;
	private: CodeEmitterBase _emitter;
	private: List<Boolean> _regInUse; // Which registers are currently in use
	private: Int32 _firstAvailable; // Lowest index that might be free
	private: Int32 _maxRegUsed; // High water mark for register usage
	private: Dictionary<String, Int32> _variableRegs; // variable name -> register
	private: Int32 _targetReg; // Target register for next expression (-1 = allocate)
	private: List<Int32> _loopExitLabels; // Stack of loop exit labels for break
	private: List<Int32> _loopContinueLabels; // Stack of loop continue labels for continue
	private: List<FuncDef> _functions; // All compiled functions (shared across inner generators)
	public: ErrorPool Errors;

	public: CodeGeneratorStorage(CodeEmitterBase emitter);

	// Get all compiled functions (index 0 = @main, 1+ = inner functions)
	public: List<FuncDef> GetFunctions();

	// Allocate a register
	private: Int32 AllocReg();

	// Free a register so it can be reused
	private: void FreeReg(Int32 reg);

	// Allocate a block of consecutive registers
	// Returns the first register of the block
	private: Int32 AllocConsecutiveRegs(Int32 count);

	// Compile an expression into a specific target register
	// The target register should already be allocated by the caller
	private: Int32 CompileInto(ASTNode node, Int32 targetReg);

	// Get target register if set, otherwise allocate a new one
	// IMPORTANT: Call this at the START of each Visit method, before any recursive calls
	private: Int32 GetTargetOrAlloc();

	// Compile an expression, placing result in a newly allocated register
	// Returns the register number holding the result
	public: Int32 Compile(ASTNode ast);

	// Reset temporary registers before compiling a new statement.
	// Keeps r0 and all variable registers; frees everything else.
	private: void ResetTempRegisters();

	// Compile a list of statements (a block body).
	// Resets temporary registers before each statement.
	private: void CompileBody(List<ASTNode> body);

	// Compile a complete function from a single expression/statement
	public: FuncDef CompileFunction(ASTNode ast, String funcName);

	// Compile a complete function from a list of statements (program)
	public: FuncDef CompileProgram(List<ASTNode> statements, String funcName);

	// --- Visit methods for each AST node type ---

	public: Int32 Visit(NumberNode node);

	public: Int32 Visit(StringNode node);

	private: Int32 VisitIdentifier(IdentifierNode node, bool addressOf);

	public: Int32 Visit(IdentifierNode node);

	public: Int32 Visit(AssignmentNode node);

	public: Int32 Visit(IndexedAssignmentNode node);

	public: Int32 Visit(UnaryOpNode node);

	public: Int32 Visit(BinaryOpNode node);

	public: Int32 Visit(CallNode node);

	// Compile a call to a user-defined function (funcref in a register)
	private: Int32 CompileUserCall(CallNode node, Int32 funcVarReg, Int32 explicitTarget);

	public: Int32 Visit(GroupNode node);

	public: Int32 Visit(ListNode node);

	public: Int32 Visit(MapNode node);

	public: Int32 Visit(IndexNode node);

	public: Int32 Visit(MemberNode node);

	public: Int32 Visit(ExprCallNode node);

	public: Int32 Visit(MethodCallNode node);

	public: Int32 Visit(WhileNode node);

	public: Int32 Visit(IfNode node);

	public: Int32 Visit(ForNode node);

	public: Int32 Visit(BreakNode node);

	public: Int32 Visit(ContinueNode node);

	public: Int32 Visit(FunctionNode node);

	public: Int32 Visit(ReturnNode node);
}; // end of class CodeGeneratorStorage



// Compiles AST nodes to bytecode
struct CodeGenerator : public IASTVisitor {
	friend class CodeGeneratorStorage;
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
	private: Dictionary<String, Int32> _variableRegs(); // variable name -> register
	private: void set__variableRegs(Dictionary<String, Int32> _v); // variable name -> register
	private: Int32 _targetReg(); // Target register for next expression (-1 = allocate)
	private: void set__targetReg(Int32 _v); // Target register for next expression (-1 = allocate)
	private: List<Int32> _loopExitLabels(); // Stack of loop exit labels for break
	private: void set__loopExitLabels(List<Int32> _v); // Stack of loop exit labels for break
	private: List<Int32> _loopContinueLabels(); // Stack of loop continue labels for continue
	private: void set__loopContinueLabels(List<Int32> _v); // Stack of loop continue labels for continue
	private: List<FuncDef> _functions(); // All compiled functions (shared across inner generators)
	private: void set__functions(List<FuncDef> _v); // All compiled functions (shared across inner generators)
	public: ErrorPool Errors();
	public: void set_Errors(ErrorPool _v);

	public: static CodeGenerator New(CodeEmitterBase emitter) {
		return CodeGenerator(std::make_shared<CodeGeneratorStorage>(emitter));
	}

	// Get all compiled functions (index 0 = @main, 1+ = inner functions)
	public: List<FuncDef> GetFunctions() { return get()->GetFunctions(); }

	// Allocate a register
	private: Int32 AllocReg() { return get()->AllocReg(); }

	// Free a register so it can be reused
	private: void FreeReg(Int32 reg) { return get()->FreeReg(reg); }

	// Allocate a block of consecutive registers
	// Returns the first register of the block
	private: Int32 AllocConsecutiveRegs(Int32 count) { return get()->AllocConsecutiveRegs(count); }

	// Compile an expression into a specific target register
	// The target register should already be allocated by the caller
	private: Int32 CompileInto(ASTNode node, Int32 targetReg) { return get()->CompileInto(node, targetReg); }

	// Get target register if set, otherwise allocate a new one
	// IMPORTANT: Call this at the START of each Visit method, before any recursive calls
	private: Int32 GetTargetOrAlloc() { return get()->GetTargetOrAlloc(); }

	// Compile an expression, placing result in a newly allocated register
	// Returns the register number holding the result
	public: Int32 Compile(ASTNode ast) { return get()->Compile(ast); }

	// Reset temporary registers before compiling a new statement.
	// Keeps r0 and all variable registers; frees everything else.
	private: void ResetTempRegisters() { return get()->ResetTempRegisters(); }

	// Compile a list of statements (a block body).
	// Resets temporary registers before each statement.
	private: void CompileBody(List<ASTNode> body) { return get()->CompileBody(body); }

	// Compile a complete function from a single expression/statement
	public: FuncDef CompileFunction(ASTNode ast, String funcName) { return get()->CompileFunction(ast, funcName); }

	// Compile a complete function from a list of statements (program)
	public: FuncDef CompileProgram(List<ASTNode> statements, String funcName) { return get()->CompileProgram(statements, funcName); }

	// --- Visit methods for each AST node type ---

	public: Int32 Visit(NumberNode node) { return get()->Visit(node); }

	public: Int32 Visit(StringNode node) { return get()->Visit(node); }

	private: Int32 VisitIdentifier(IdentifierNode node, bool addressOf) { return get()->VisitIdentifier(node, addressOf); }

	public: Int32 Visit(IdentifierNode node) { return get()->Visit(node); }

	public: Int32 Visit(AssignmentNode node) { return get()->Visit(node); }

	public: Int32 Visit(IndexedAssignmentNode node) { return get()->Visit(node); }

	public: Int32 Visit(UnaryOpNode node) { return get()->Visit(node); }

	public: Int32 Visit(BinaryOpNode node) { return get()->Visit(node); }

	public: Int32 Visit(CallNode node) { return get()->Visit(node); }

	// Compile a call to a user-defined function (funcref in a register)
	private: Int32 CompileUserCall(CallNode node, Int32 funcVarReg, Int32 explicitTarget) { return get()->CompileUserCall(node, funcVarReg, explicitTarget); }

	public: Int32 Visit(GroupNode node) { return get()->Visit(node); }

	public: Int32 Visit(ListNode node) { return get()->Visit(node); }

	public: Int32 Visit(MapNode node) { return get()->Visit(node); }

	public: Int32 Visit(IndexNode node) { return get()->Visit(node); }

	public: Int32 Visit(MemberNode node) { return get()->Visit(node); }

	public: Int32 Visit(ExprCallNode node) { return get()->Visit(node); }

	public: Int32 Visit(MethodCallNode node) { return get()->Visit(node); }

	public: Int32 Visit(WhileNode node) { return get()->Visit(node); }

	public: Int32 Visit(IfNode node) { return get()->Visit(node); }

	public: Int32 Visit(ForNode node) { return get()->Visit(node); }

	public: Int32 Visit(BreakNode node) { return get()->Visit(node); }

	public: Int32 Visit(ContinueNode node) { return get()->Visit(node); }

	public: Int32 Visit(FunctionNode node) { return get()->Visit(node); }

	public: Int32 Visit(ReturnNode node) { return get()->Visit(node); }
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
inline Dictionary<String, Int32> CodeGenerator::_variableRegs() { return get()->_variableRegs; } // variable name -> register
inline void CodeGenerator::set__variableRegs(Dictionary<String, Int32> _v) { get()->_variableRegs = _v; } // variable name -> register
inline Int32 CodeGenerator::_targetReg() { return get()->_targetReg; } // Target register for next expression (-1 = allocate)
inline void CodeGenerator::set__targetReg(Int32 _v) { get()->_targetReg = _v; } // Target register for next expression (-1 = allocate)
inline List<Int32> CodeGenerator::_loopExitLabels() { return get()->_loopExitLabels; } // Stack of loop exit labels for break
inline void CodeGenerator::set__loopExitLabels(List<Int32> _v) { get()->_loopExitLabels = _v; } // Stack of loop exit labels for break
inline List<Int32> CodeGenerator::_loopContinueLabels() { return get()->_loopContinueLabels; } // Stack of loop continue labels for continue
inline void CodeGenerator::set__loopContinueLabels(List<Int32> _v) { get()->_loopContinueLabels = _v; } // Stack of loop continue labels for continue
inline List<FuncDef> CodeGenerator::_functions() { return get()->_functions; } // All compiled functions (shared across inner generators)
inline void CodeGenerator::set__functions(List<FuncDef> _v) { get()->_functions = _v; } // All compiled functions (shared across inner generators)
inline ErrorPool CodeGenerator::Errors() { return get()->Errors; }
inline void CodeGenerator::set_Errors(ErrorPool _v) { get()->Errors = _v; }

} // end of namespace MiniScript
