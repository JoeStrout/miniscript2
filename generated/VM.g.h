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
	private: Dictionary<String, Value> _intrinsics; // intrinsic name -> FuncRef Value
	public: Int32 PC;
	private: Int32 _currentFuncIndex = -1;
	public: FuncDef CurrentFunction;
	public: Boolean IsRunning;
	public: Int32 BaseIndex;
	public: String RuntimeError;
	public: ErrorPool Errors;
	private: Value pendingSelf;
	private: Value pendingSuper;
	private: bool hasPendingContext;
	private: thread_local static VM _activeVM;

	// Print callback: if set, print output goes here instead of IOHelper.Print
	// H: public: std::function<void(const String&)> _printCallback;
	// H: public: void SetPrintCallback(std::function<void(const String&)> cb) { _printCallback = cb; }
	// H: public: std::function<void(const String&)> GetPrintCallback() { return _printCallback; }

	// Static callback for C++ (accessible from VM wrapper)
	// H: public: static std::function<void(const String&)> sPrintCallback;

	// Execution state (persistent across RunSteps calls)

	// Pending self/super for method calls, set by METHFIND/SETSELF,
	// consumed by the next CALL instruction

	// Thread-local active VM: set during Run(), so value operations
	// (like list_push) can report errors without passing ErrorPool around.
	public: static VM ActiveVM();

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

	// Apply pending self/super context to a callee's frame, if any.
	// Called after SetupCallFrame to populate the callee's self/super registers.
	private: void ApplyPendingContext(Int32 calleeBase, FuncDef callee);

	// Helper for call setup (FUNCTION_CALLS.md steps 4-6):
	// Initialize remaining parameters with defaults and clear callee's registers.
	// Note: Parameters start at r1 (r0 is reserved for return value)
	private: void SetupCallFrame(Int32 argCount, Int32 calleeBase, FuncDef callee);

	// Auto-invoke a zero-arg funcref (used by LOADC and CALLIFREF).
	// Resolves the funcref, then either:
	//   - Native callback: invokes it, stores result at stack[baseIndex + resultReg], returns -1.
	//   - User function: pushes CallInfo and sets up callee frame, returns the callee function index.
	//     Caller must switch its local execution state (pc, baseIndex, curFunc, etc.).
	// On error, calls RaiseRuntimeError and returns -1.
	private: Int32 AutoInvokeFuncRef(Value funcRefVal, Int32 resultReg, Int32 returnPC, Int32 baseIndex, Int32 currentFuncIndex, FuncDef curFunc);

	public: Value Execute(FuncDef entry);

	public: Value Execute(FuncDef entry, UInt32 maxCycles);

	public: Value Run(UInt32 maxCycles=0);

	private: Value RunInner(UInt32 maxCycles);

	private: void EnsureFrame(Int32 baseIndex, UInt16 neededRegs);

	private: Value LookupVariable(Value varName);
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
	private: Dictionary<String, Value> _intrinsics(); // intrinsic name -> FuncRef Value
	private: void set__intrinsics(Dictionary<String, Value> _v); // intrinsic name -> FuncRef Value
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
	private: Value pendingSelf();
	private: void set_pendingSelf(Value _v);
	private: Value pendingSuper();
	private: void set_pendingSuper(Value _v);
	private: bool hasPendingContext();
	private: void set_hasPendingContext(bool _v);
	private: VM _activeVM();
	private: void set__activeVM(VM _v);

	// Print callback: if set, print output goes here instead of IOHelper.Print
	// H: public: std::function<void(const String&)> _printCallback;
	// H: public: void SetPrintCallback(std::function<void(const String&)> cb) { _printCallback = cb; }
	// H: public: std::function<void(const String&)> GetPrintCallback() { return _printCallback; }

	// Static callback for C++ (accessible from VM wrapper)
	// H: public: static std::function<void(const String&)> sPrintCallback;

	// Execution state (persistent across RunSteps calls)

	// Pending self/super for method calls, set by METHFIND/SETSELF,
	// consumed by the next CALL instruction

	// Thread-local active VM: set during Run(), so value operations
	// (like list_push) can report errors without passing ErrorPool around.
	public: static VM ActiveVM() { return VMStorage::ActiveVM(); }

	public: inline Int32 StackSize();
	public: inline Int32 CallStackDepth();

	public: inline Value GetStackValue(Int32 index);

	public: inline Value GetStackName(Int32 index);

	public: inline CallInfo GetCallStackFrame(Int32 index);

	public: inline String GetFunctionName(Int32 funcIndex);

	public: static VM New(Int32 stackSlots=1024, Int32 callSlots=256) {
		return VM(std::make_shared<VMStorage>(stackSlots, callSlots));
	}

	private: inline void InitVM(Int32 stackSlots, Int32 callSlots);
	
	private: inline void CleanupVM();

	// H: static void MarkRoots(void* user_data);
	// H: public: ~VMStorage() { CleanupVM(); }

	public: inline void RegisterFunction(FuncDef funcDef);

	public: inline void Reset(List<FuncDef> allFunctions);

	public: inline void RaiseRuntimeError(String message);

	public: inline bool ReportRuntimeError();

	// Helper for argument processing (FUNCTION_CALLS.md steps 1-3):
	// Process ARG instructions, validate argument count, and set up parameter registers.
	// Returns the PC after the CALL instruction, or -1 on error.
	private: inline Int32 ProcessArguments(Int32 argCount, Int32 startPC, Int32 callerBase, Int32 calleeBase, FuncDef callee, List<UInt32> code);

	// Apply pending self/super context to a callee's frame, if any.
	// Called after SetupCallFrame to populate the callee's self/super registers.
	private: inline void ApplyPendingContext(Int32 calleeBase, FuncDef callee);

	// Helper for call setup (FUNCTION_CALLS.md steps 4-6):
	// Initialize remaining parameters with defaults and clear callee's registers.
	// Note: Parameters start at r1 (r0 is reserved for return value)
	private: inline void SetupCallFrame(Int32 argCount, Int32 calleeBase, FuncDef callee);

	// Auto-invoke a zero-arg funcref (used by LOADC and CALLIFREF).
	// Resolves the funcref, then either:
	//   - Native callback: invokes it, stores result at stack[baseIndex + resultReg], returns -1.
	//   - User function: pushes CallInfo and sets up callee frame, returns the callee function index.
	//     Caller must switch its local execution state (pc, baseIndex, curFunc, etc.).
	// On error, calls RaiseRuntimeError and returns -1.
	private: inline Int32 AutoInvokeFuncRef(Value funcRefVal, Int32 resultReg, Int32 returnPC, Int32 baseIndex, Int32 currentFuncIndex, FuncDef curFunc);

	public: inline Value Execute(FuncDef entry);

	public: inline Value Execute(FuncDef entry, UInt32 maxCycles);

	public: inline Value Run(UInt32 maxCycles=0);

	private: inline Value RunInner(UInt32 maxCycles);

	private: inline void EnsureFrame(Int32 baseIndex, UInt16 neededRegs);

	private: inline Value LookupVariable(Value varName);
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
inline Dictionary<String, Value> VM::_intrinsics() { return get()->_intrinsics; } // intrinsic name -> FuncRef Value
inline void VM::set__intrinsics(Dictionary<String, Value> _v) { get()->_intrinsics = _v; } // intrinsic name -> FuncRef Value
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
inline Value VM::pendingSelf() { return get()->pendingSelf; }
inline void VM::set_pendingSelf(Value _v) { get()->pendingSelf = _v; }
inline Value VM::pendingSuper() { return get()->pendingSuper; }
inline void VM::set_pendingSuper(Value _v) { get()->pendingSuper = _v; }
inline bool VM::hasPendingContext() { return get()->hasPendingContext; }
inline void VM::set_hasPendingContext(bool _v) { get()->hasPendingContext = _v; }
inline VM VM::_activeVM() { return get()->_activeVM; }
inline void VM::set__activeVM(VM _v) { get()->_activeVM = _v; }
inline Int32 VM::StackSize() { return get()->StackSize(); }
inline Int32 VM::CallStackDepth() { return get()->CallStackDepth(); }
inline Value VM::GetStackValue(Int32 index) { return get()->GetStackValue(index); }
inline Value VM::GetStackName(Int32 index) { return get()->GetStackName(index); }
inline CallInfo VM::GetCallStackFrame(Int32 index) { return get()->GetCallStackFrame(index); }
inline String VM::GetFunctionName(Int32 funcIndex) { return get()->GetFunctionName(funcIndex); }
inline void VM::InitVM(Int32 stackSlots,Int32 callSlots) { return get()->InitVM(stackSlots, callSlots); }
inline void VM::CleanupVM() { return get()->CleanupVM(); }
inline void VM::RegisterFunction(FuncDef funcDef) { return get()->RegisterFunction(funcDef); }
inline void VM::Reset(List<FuncDef> allFunctions) { return get()->Reset(allFunctions); }
inline void VM::RaiseRuntimeError(String message) { return get()->RaiseRuntimeError(message); }
inline bool VM::ReportRuntimeError() { return get()->ReportRuntimeError(); }
inline Int32 VM::ProcessArguments(Int32 argCount,Int32 startPC,Int32 callerBase,Int32 calleeBase,FuncDef callee,List<UInt32> code) { return get()->ProcessArguments(argCount, startPC, callerBase, calleeBase, callee, code); }
inline void VM::ApplyPendingContext(Int32 calleeBase,FuncDef callee) { return get()->ApplyPendingContext(calleeBase, callee); }
inline void VM::SetupCallFrame(Int32 argCount,Int32 calleeBase,FuncDef callee) { return get()->SetupCallFrame(argCount, calleeBase, callee); }
inline Int32 VM::AutoInvokeFuncRef(Value funcRefVal,Int32 resultReg,Int32 returnPC,Int32 baseIndex,Int32 currentFuncIndex,FuncDef curFunc) { return get()->AutoInvokeFuncRef(funcRefVal, resultReg, returnPC, baseIndex, currentFuncIndex, curFunc); }
inline Value VM::Execute(FuncDef entry) { return get()->Execute(entry); }
inline Value VM::Execute(FuncDef entry,UInt32 maxCycles) { return get()->Execute(entry, maxCycles); }
inline Value VM::Run(UInt32 maxCycles) { return get()->Run(maxCycles); }
inline Value VM::RunInner(UInt32 maxCycles) { return get()->RunInner(maxCycles); }
inline void VM::EnsureFrame(Int32 baseIndex,UInt16 neededRegs) { return get()->EnsureFrame(baseIndex, neededRegs); }
inline Value VM::LookupVariable(Value varName) { return get()->LookupVariable(varName); }

} // end of namespace MiniScript

