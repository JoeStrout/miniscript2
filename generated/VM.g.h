// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VM.cs

#pragma once
#include "core_includes.h"
#include "value.h"
#include "FuncDef.g.h"
#include "ErrorPool.g.h"
#include "value_map.h"


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




// Call stack frame (return info)
struct CallInfo {
	public: Int32 ReturnPC; // where to continue in caller (PC index)
	public: Int32 ReturnBase; // caller's base pointer (stack index)
	public: Int32 ReturnFuncIndex; // caller's function index in functions list
	public: Int32 CopyResultToReg; // register number to copy result to, or -1
	public: Value LocalVarMap; // VarMap representing locals, if any
	public: Value OuterVarMap; // VarMap representing outer variables (closure context)

	public: CallInfo(Int32 returnPC, Int32 returnBase, Int32 returnFuncIndex, Int32 copyToReg=-1);

	public: CallInfo(Int32 returnPC, Int32 returnBase, Int32 returnFuncIndex, Int32 copyToReg, Value outerVars);

	public: Value GetLocalVarMap(List<Value> registers, List<Value> names, int baseIdx, int regCount);
}; // end of struct CallInfo





























































class VMStorage : public std::enable_shared_from_this<VMStorage> {
	friend struct VM;
	public: Boolean DebugMode = false;
	private: List<Value> stack;
	private: List<Value> names; // Variable names parallel to stack (null if unnamed)
	public: std::function<void(const String&)> _printCallback;
	public: void SetPrintCallback(std::function<void(const String&)> cb) { _printCallback = cb; }
	public: std::function<void(const String&)> GetPrintCallback() { return _printCallback; }
	public: static std::function<void(const String&)> sPrintCallback;
	private: List<CallInfo> callStack;
	private: Int32 callStackTop; // Index of next free call stack slot
	private: List<FuncDef> functions; // functions addressed by CALLF
	public: Int32 PC;
	private: Int32 _currentFuncIndex = -1;
	public: FuncDef CurrentFunction;
	public: Boolean IsRunning;
	public: Int32 BaseIndex;
	public: String RuntimeError;
	public: ErrorPool Errors;

	// Print callback: if set, print output goes here instead of IOHelper.Print
	// H: public: std::function<void(const String&)> _printCallback;
	// H: public: void SetPrintCallback(std::function<void(const String&)> cb) { _printCallback = cb; }
	// H: public: std::function<void(const String&)> GetPrintCallback() { return _printCallback; }

	// Static callback for C++ (accessible from VM wrapper)
	// H: public: static std::function<void(const String&)> sPrintCallback;



	// Execution state (persistent across RunSteps calls)

	public: Int32 StackSize();
	public: Int32 CallStackDepth();

	public: Value GetStackValue(Int32 index);

	public: Value GetStackName(Int32 index);

	public: CallInfo GetCallStackFrame(Int32 index);

	public: String GetFunctionName(Int32 funcIndex);

	public: VMStorage(Int32 stackSlots=1024, Int32 callSlots=256);

	private: void InitVM(Int32 stackSlots, Int32 callSlots);
	
	private: void CleanupVM();
	static void MarkRoots(void* user_data);
	public: ~VMStorage() { CleanupVM(); }

	// H: static void MarkRoots(void* user_data);
	// H: public: ~VMStorage() { CleanupVM(); }

	public: void RegisterFunction(FuncDef funcDef);

	public: void Reset(List<FuncDef> allFunctions);

	public: void RaiseRuntimeError(String message);

	public: bool ReportRuntimeError();

	// Helper for argument processing (FUNCTION_CALLS.md steps 1-3):
	// Process ARG instructions, validate argument count, and set up parameter registers.
	// Returns the PC after the CALL instruction, or -1 on error.
	private: Int32 ProcessArguments(Int32 argCount, Int32 startPC, Int32 callerBase, Int32 calleeBase, FuncDef callee, List<UInt32> code);

	// Helper for call setup (FUNCTION_CALLS.md steps 4-6):
	// Initialize remaining parameters with defaults and clear callee's registers.
	// Note: Parameters start at r1 (r0 is reserved for return value)
	private: void SetupCallFrame(Int32 argCount, Int32 calleeBase, FuncDef callee);

	public: Value Execute(FuncDef entry);

	public: Value Execute(FuncDef entry, UInt32 maxCycles);

	public: Value Run(UInt32 maxCycles=0);

	private: void EnsureFrame(Int32 baseIndex, UInt16 neededRegs);

	private: Value LookupVariable(Value varName);
	private: static const Value FuncNamePrint;
	private: static const Value FuncNameInput;
	private: static const Value FuncNameVal;
	private: static const Value FuncNameLen;
	private: static const Value FuncNameRemove;
	
	
	private: void DoIntrinsic(Value funcName, Int32 baseReg);
}; // end of class VMStorage


// VM state
struct VM {
	friend class VMStorage;
	protected: std::shared_ptr<VMStorage> storage;
  public:
	VM(std::shared_ptr<VMStorage> stor) : storage(stor) {}
	VM() : storage(nullptr) {}
	VM(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const VM& inst) { return inst.storage == nullptr; }
	private: VMStorage* get() const;

	public: Boolean DebugMode();
	public: void set_DebugMode(Boolean _v);
	private: List<Value> stack();
	private: void set_stack(List<Value> _v);
	private: List<Value> names(); // Variable names parallel to stack (null if unnamed)
	private: void set_names(List<Value> _v); // Variable names parallel to stack (null if unnamed)
	private: List<CallInfo> callStack();
	private: void set_callStack(List<CallInfo> _v);
	private: Int32 callStackTop(); // Index of next free call stack slot
	private: void set_callStackTop(Int32 _v); // Index of next free call stack slot
	private: List<FuncDef> functions(); // functions addressed by CALLF
	private: void set_functions(List<FuncDef> _v); // functions addressed by CALLF
	public: Int32 PC();
	public: void set_PC(Int32 _v);
	private: Int32 _currentFuncIndex();
	private: void set__currentFuncIndex(Int32 _v);
	public: FuncDef CurrentFunction();
	public: void set_CurrentFunction(FuncDef _v);
	public: Boolean IsRunning();
	public: void set_IsRunning(Boolean _v);
	public: Int32 BaseIndex();
	public: void set_BaseIndex(Int32 _v);
	public: String RuntimeError();
	public: void set_RuntimeError(String _v);
	public: ErrorPool Errors();
	public: void set_Errors(ErrorPool _v);

	// Print callback: if set, print output goes here instead of IOHelper.Print
	// H: public: std::function<void(const String&)> _printCallback;
	// H: public: void SetPrintCallback(std::function<void(const String&)> cb) { _printCallback = cb; }
	// H: public: std::function<void(const String&)> GetPrintCallback() { return _printCallback; }

	// Static callback for C++ (accessible from VM wrapper)
	// H: public: static std::function<void(const String&)> sPrintCallback;



	// Execution state (persistent across RunSteps calls)

	public: Int32 StackSize() { return get()->StackSize(); }
	public: Int32 CallStackDepth() { return get()->CallStackDepth(); }

	public: Value GetStackValue(Int32 index) { return get()->GetStackValue(index); }

	public: Value GetStackName(Int32 index) { return get()->GetStackName(index); }

	public: CallInfo GetCallStackFrame(Int32 index) { return get()->GetCallStackFrame(index); }

	public: String GetFunctionName(Int32 funcIndex) { return get()->GetFunctionName(funcIndex); }

	public: static VM New(Int32 stackSlots=1024, Int32 callSlots=256) {
		return VM(std::make_shared<VMStorage>(stackSlots, callSlots));
	}

	private: void InitVM(Int32 stackSlots, Int32 callSlots) { return get()->InitVM(stackSlots, callSlots); }
	
	private: void CleanupVM() { return get()->CleanupVM(); }

	// H: static void MarkRoots(void* user_data);
	// H: public: ~VMStorage() { CleanupVM(); }

	public: void RegisterFunction(FuncDef funcDef) { return get()->RegisterFunction(funcDef); }

	public: void Reset(List<FuncDef> allFunctions) { return get()->Reset(allFunctions); }

	public: void RaiseRuntimeError(String message) { return get()->RaiseRuntimeError(message); }

	public: bool ReportRuntimeError() { return get()->ReportRuntimeError(); }

	// Helper for argument processing (FUNCTION_CALLS.md steps 1-3):
	// Process ARG instructions, validate argument count, and set up parameter registers.
	// Returns the PC after the CALL instruction, or -1 on error.
	private: Int32 ProcessArguments(Int32 argCount, Int32 startPC, Int32 callerBase, Int32 calleeBase, FuncDef callee, List<UInt32> code) { return get()->ProcessArguments(argCount, startPC, callerBase, calleeBase, callee, code); }

	// Helper for call setup (FUNCTION_CALLS.md steps 4-6):
	// Initialize remaining parameters with defaults and clear callee's registers.
	// Note: Parameters start at r1 (r0 is reserved for return value)
	private: void SetupCallFrame(Int32 argCount, Int32 calleeBase, FuncDef callee) { return get()->SetupCallFrame(argCount, calleeBase, callee); }

	public: Value Execute(FuncDef entry) { return get()->Execute(entry); }

	public: Value Execute(FuncDef entry, UInt32 maxCycles) { return get()->Execute(entry, maxCycles); }

	public: Value Run(UInt32 maxCycles=0) { return get()->Run(maxCycles); }

	private: void EnsureFrame(Int32 baseIndex, UInt16 neededRegs) { return get()->EnsureFrame(baseIndex, neededRegs); }

	private: Value LookupVariable(Value varName) { return get()->LookupVariable(varName); }
	private: Value FuncNamePrint();
	private: Value FuncNameInput();
	private: Value FuncNameVal();
	private: Value FuncNameLen();
	private: Value FuncNameRemove();
	
	
	private: void DoIntrinsic(Value funcName, Int32 baseReg) { return get()->DoIntrinsic(funcName, baseReg); }
}; // end of struct VM


// INLINE METHODS

inline VMStorage* VM::get() const { return static_cast<VMStorage*>(storage.get()); }
inline Boolean VM::DebugMode() { return get()->DebugMode; }
inline void VM::set_DebugMode(Boolean _v) { get()->DebugMode = _v; }
inline List<Value> VM::stack() { return get()->stack; }
inline void VM::set_stack(List<Value> _v) { get()->stack = _v; }
inline List<Value> VM::names() { return get()->names; } // Variable names parallel to stack (null if unnamed)
inline void VM::set_names(List<Value> _v) { get()->names = _v; } // Variable names parallel to stack (null if unnamed)
inline List<CallInfo> VM::callStack() { return get()->callStack; }
inline void VM::set_callStack(List<CallInfo> _v) { get()->callStack = _v; }
inline Int32 VM::callStackTop() { return get()->callStackTop; } // Index of next free call stack slot
inline void VM::set_callStackTop(Int32 _v) { get()->callStackTop = _v; } // Index of next free call stack slot
inline List<FuncDef> VM::functions() { return get()->functions; } // functions addressed by CALLF
inline void VM::set_functions(List<FuncDef> _v) { get()->functions = _v; } // functions addressed by CALLF
inline Int32 VM::PC() { return get()->PC; }
inline void VM::set_PC(Int32 _v) { get()->PC = _v; }
inline Int32 VM::_currentFuncIndex() { return get()->_currentFuncIndex; }
inline void VM::set__currentFuncIndex(Int32 _v) { get()->_currentFuncIndex = _v; }
inline FuncDef VM::CurrentFunction() { return get()->CurrentFunction; }
inline void VM::set_CurrentFunction(FuncDef _v) { get()->CurrentFunction = _v; }
inline Boolean VM::IsRunning() { return get()->IsRunning; }
inline void VM::set_IsRunning(Boolean _v) { get()->IsRunning = _v; }
inline Int32 VM::BaseIndex() { return get()->BaseIndex; }
inline void VM::set_BaseIndex(Int32 _v) { get()->BaseIndex = _v; }
inline String VM::RuntimeError() { return get()->RuntimeError; }
inline void VM::set_RuntimeError(String _v) { get()->RuntimeError = _v; }
inline ErrorPool VM::Errors() { return get()->Errors; }
inline void VM::set_Errors(ErrorPool _v) { get()->Errors = _v; }
inline Value VM::FuncNamePrint() { return get()->FuncNamePrint; }
inline Value VM::FuncNameInput() { return get()->FuncNameInput; }
inline Value VM::FuncNameVal() { return get()->FuncNameVal; }
inline Value VM::FuncNameLen() { return get()->FuncNameLen; }
inline Value VM::FuncNameRemove() { return get()->FuncNameRemove; }

} // end of namespace MiniScript

