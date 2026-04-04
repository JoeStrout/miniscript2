// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VM.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
#include "value.h"
#include "FuncDef.g.h"
#include "IntrinsicAPI.g.h"
#include "ErrorPool.g.h"
#include "value_map.h"
#include <vector>
#include "gc.h"

namespace MiniScript {
typedef const FuncDefStorage& FuncDefRef;

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
	private: InterpreterStorage* interpreter;

	// Reference to the Interpreter that owns this VM (may be null if VM is used standalone).
	// (Note that in C++, this is a raw pointer to the InterpreterStorage, due to annoying
	// circular-header-dependency issues.)  The same Get/Set interface works on both C#/C++.
	public: void SetInterpreter(Interpreter interp); // NO_INLINE
	public: Interpreter GetInterpreter(); // NO_INLINE
	private: List<CallInfo> callStack;
	private: Int32 callStackTop; // Index of next free call stack slot
	private: List<FuncDef> functions; // functions addressed by CALLF
	private: std::vector<FuncDefStorage*> functionsRaw;
	private: Dictionary<String, Value> _intrinsics; // intrinsic name -> FuncRef Value
	public: Int32 PC;
	private: Int32 _currentFuncIndex = -1;
	public: FuncDef CurrentFunction;
	public: Boolean IsRunning;
	public: Int32 BaseIndex;
	public: String RuntimeError;
	public: ErrorPool Errors;
	public: Value ReplGlobals = val_null;
	private: Value pendingSelf;
	private: Value pendingSuper;
	private: bool hasPendingContext;
	private: NativeCallbackDelegate _pendingCallback;
	private: Int32 _pendingCalleeBase; // base index for reconstructing Context
	private: Int32 _pendingArgCount; // arg count for reconstructing Context
	private: Int32 _pendingResultIndex; // absolute stack index for result (and partial result)
	public: bool yielding;
	private: std::chrono::steady_clock::time_point _startTime;

	

	// Execution state (persistent across RunSteps calls)

	// REPL mode: persistent globals VarMap shared across REPL entries.
	// When set (not val_null), used instead of callStack[0].GetLocalVarMap for globals.

	// Pending self/super for method calls, set by METHFIND/SETSELF,
	// consumed by the next CALL instruction

	// Pending intrinsic continuation: when an intrinsic returns done=false,
	// we store the state here so Run can re-invoke it on the next call.
	// The partial result value is stored in stack[_pendingResultIndex].

	// Set by the "yield" intrinsic; host app can check and clear this.

	// Wall-clock start time, set in Reset(), used by the "time" intrinsic.
	
	public: double ElapsedTime();
	private: thread_local static VM _activeVM;

	// Thread-local active VM: set during Run(), so value operations
	// (like list_push) can report errors without passing ErrorPool around.
	public: static VM ActiveVM();

	public: Int32 StackSize();
	
	public: List<Value> GetStack();
	
	public: List<Value> GetNames();

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

	public: void RegisterFunction(FuncDef funcDef);

	public: void Reset(List<FuncDef> allFunctions);

	public: void Reset(List<FuncDef> allFunctions, Value replGlobals);

	public: void Stop();

	public: void RaiseRuntimeError(String message);

	public: bool ReportRuntimeError();

	public: Int32 FunctionCount();

	public: List<FuncDef> GetFunctions();

	// Helper for argument processing (FUNCTION_CALLS.md steps 1-3):
	// Process ARG instructions, validate argument count, and set up parameter registers.
	// Returns the PC after the CALL instruction, or -1 on error.
	// Check whether pendingSelf should be injected as the first parameter.
	// Returns 1 if the callee's first param is named "self" and we have pending context, else 0.
	private: Int32 SelfParamOffset(FuncDefRef callee);

	private: Int32 ProcessArguments(Int32 argCount, Int32 selfParam, Int32 startPC, Int32 callerBase, Int32 calleeBase, FuncDefRef callee, List<UInt32> code);

	// Apply pending self/super context to a callee's frame, if any.
	// Called after SetupCallFrame to populate the callee's self/super registers.
	private: void ApplyPendingContext(Int32 calleeBase, FuncDefRef callee);

	// Helper for call setup (FUNCTION_CALLS.md steps 4-6):
	// Initialize remaining parameters with defaults and clear callee's registers.
	// Note: Parameters start at r1 (r0 is reserved for return value)
	// selfParam is 1 if pendingSelf was injected as the first arg, else 0.
	private: void SetupCallFrame(Int32 argCount, Int32 selfParam, Int32 calleeBase, FuncDefRef callee);

	// Auto-invoke a zero-arg funcref (used by LOADC and CALLIFREF).
	// Resolves the funcref, then either:
	//   - Native callback (done):    stores result, returns -1.
	//   - Native callback (pending): stores pending state, returns -2.
	//   - User function: pushes CallInfo and sets up callee frame, returns the callee function index.
	//     Caller must switch its local execution state (pc, baseIndex, curFunc, etc.).
	// On error, calls RaiseRuntimeError and returns -1.
	private: Int32 AutoInvokeFuncRef(Value funcRefVal, Int32 resultReg, Int32 returnPC, Int32 baseIndex, Int32 currentFuncIndex, FuncDefRef curFunc);

	// Invoke a native callback and handle the result.  If done, writes the
	// result to stack[absoluteResultIndex] and returns true.  If not done,
	// stores the pending state for re-invocation and returns false.
	private: bool InvokeNativeCallback(NativeCallbackDelegate callback, Int32 calleeBase, Int32 argCount, IntrinsicResult partialResult, Int32 absoluteResultIndex);

	public: Value Execute(FuncDef entry);

	public: Value Execute(FuncDef entry, UInt32 maxCycles);

	public: Value Run(UInt32 maxCycles=0);

	private: Value RunInner(UInt32 maxCycles);

	private: void EnsureFrame(Int32 baseIndex, UInt16 neededRegs);
	void SwitchFrame(Int32 currentFuncIndex, Int32 baseIndex, FuncDefStorage* &curFuncRaw, Int32 &codeCount, UInt32* &curCode, Value* &curConstants, Value* &localStack, Value* stackPtr);

	// Switch all frame-local execution state to the function at currentFuncIndex.

	// Get the globals VarMap.  In REPL mode, returns the persistent ReplGlobals.
	// Otherwise, lazily creates one from callStack[0].
	private: Value GetGlobalsVarMap();

	// Get or create a VarMap for the current call frame's local variables.
	// At global scope (callStackTop == 0), creates a VarMap directly.
	private: Value GetCurrentLocalVarMap(Int32 baseIndex, UInt16 maxRegs);

	private: void SaveState(Int32 pc, Int32 baseIndex, Int32 currentFuncIndex);

	public: Value LookupParamByName(String varName);

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

	// Reference to the Interpreter that owns this VM (may be null if VM is used standalone).
	// (Note that in C++, this is a raw pointer to the InterpreterStorage, due to annoying
	// circular-header-dependency issues.)  The same Get/Set interface works on both C#/C++.
	public: void SetInterpreter(Interpreter interp); // NO_INLINE
	public: void SetInterpreter(InterpreterStorage* p) { get()->interpreter = p; }
	public: Interpreter GetInterpreter(); // NO_INLINE
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
	public: Value ReplGlobals();
	public: void set_ReplGlobals(Value _v);
	private: Value pendingSelf();
	private: void set_pendingSelf(Value _v);
	private: Value pendingSuper();
	private: void set_pendingSuper(Value _v);
	private: bool hasPendingContext();
	private: void set_hasPendingContext(bool _v);
	private: NativeCallbackDelegate _pendingCallback();
	private: void set__pendingCallback(NativeCallbackDelegate _v);
	private: Int32 _pendingCalleeBase(); // base index for reconstructing Context
	private: void set__pendingCalleeBase(Int32 _v); // base index for reconstructing Context
	private: Int32 _pendingArgCount(); // arg count for reconstructing Context
	private: void set__pendingArgCount(Int32 _v); // arg count for reconstructing Context
	private: Int32 _pendingResultIndex(); // absolute stack index for result (and partial result)
	private: void set__pendingResultIndex(Int32 _v); // absolute stack index for result (and partial result)
	public: bool yielding();
	public: void set_yielding(bool _v);

	

	// Execution state (persistent across RunSteps calls)

	// REPL mode: persistent globals VarMap shared across REPL entries.
	// When set (not val_null), used instead of callStack[0].GetLocalVarMap for globals.

	// Pending self/super for method calls, set by METHFIND/SETSELF,
	// consumed by the next CALL instruction

	// Pending intrinsic continuation: when an intrinsic returns done=false,
	// we store the state here so Run can re-invoke it on the next call.
	// The partial result value is stored in stack[_pendingResultIndex].

	// Set by the "yield" intrinsic; host app can check and clear this.

	// Wall-clock start time, set in Reset(), used by the "time" intrinsic.
	
	public: inline double ElapsedTime();
	private: VM _activeVM();
	private: void set__activeVM(VM _v);

	// Thread-local active VM: set during Run(), so value operations
	// (like list_push) can report errors without passing ErrorPool around.
	public: static VM ActiveVM() { return VMStorage::ActiveVM(); }

	public: inline Int32 StackSize();
	
	public: inline List<Value> GetStack();
	
	public: inline List<Value> GetNames();

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

	public: inline void RegisterFunction(FuncDef funcDef);

	public: inline void Reset(List<FuncDef> allFunctions);

	public: inline void Reset(List<FuncDef> allFunctions, Value replGlobals);

	public: inline void Stop();

	public: inline void RaiseRuntimeError(String message);

	public: inline bool ReportRuntimeError();

	public: inline Int32 FunctionCount();

	public: inline List<FuncDef> GetFunctions();

	// Helper for argument processing (FUNCTION_CALLS.md steps 1-3):
	// Process ARG instructions, validate argument count, and set up parameter registers.
	// Returns the PC after the CALL instruction, or -1 on error.
	// Check whether pendingSelf should be injected as the first parameter.
	// Returns 1 if the callee's first param is named "self" and we have pending context, else 0.
	private: inline Int32 SelfParamOffset(FuncDefRef callee);

	private: inline Int32 ProcessArguments(Int32 argCount, Int32 selfParam, Int32 startPC, Int32 callerBase, Int32 calleeBase, FuncDefRef callee, List<UInt32> code);

	// Apply pending self/super context to a callee's frame, if any.
	// Called after SetupCallFrame to populate the callee's self/super registers.
	private: inline void ApplyPendingContext(Int32 calleeBase, FuncDefRef callee);

	// Helper for call setup (FUNCTION_CALLS.md steps 4-6):
	// Initialize remaining parameters with defaults and clear callee's registers.
	// Note: Parameters start at r1 (r0 is reserved for return value)
	// selfParam is 1 if pendingSelf was injected as the first arg, else 0.
	private: inline void SetupCallFrame(Int32 argCount, Int32 selfParam, Int32 calleeBase, FuncDefRef callee);

	// Auto-invoke a zero-arg funcref (used by LOADC and CALLIFREF).
	// Resolves the funcref, then either:
	//   - Native callback (done):    stores result, returns -1.
	//   - Native callback (pending): stores pending state, returns -2.
	//   - User function: pushes CallInfo and sets up callee frame, returns the callee function index.
	//     Caller must switch its local execution state (pc, baseIndex, curFunc, etc.).
	// On error, calls RaiseRuntimeError and returns -1.
	private: inline Int32 AutoInvokeFuncRef(Value funcRefVal, Int32 resultReg, Int32 returnPC, Int32 baseIndex, Int32 currentFuncIndex, FuncDefRef curFunc);

	// Invoke a native callback and handle the result.  If done, writes the
	// result to stack[absoluteResultIndex] and returns true.  If not done,
	// stores the pending state for re-invocation and returns false.
	private: inline bool InvokeNativeCallback(NativeCallbackDelegate callback, Int32 calleeBase, Int32 argCount, IntrinsicResult partialResult, Int32 absoluteResultIndex);

	public: inline Value Execute(FuncDef entry);

	public: inline Value Execute(FuncDef entry, UInt32 maxCycles);

	public: inline Value Run(UInt32 maxCycles=0);

	private: inline Value RunInner(UInt32 maxCycles);

	private: inline void EnsureFrame(Int32 baseIndex, UInt16 neededRegs);

	// Switch all frame-local execution state to the function at currentFuncIndex.

	// Get the globals VarMap.  In REPL mode, returns the persistent ReplGlobals.
	// Otherwise, lazily creates one from callStack[0].
	private: inline Value GetGlobalsVarMap();

	// Get or create a VarMap for the current call frame's local variables.
	// At global scope (callStackTop == 0), creates a VarMap directly.
	private: inline Value GetCurrentLocalVarMap(Int32 baseIndex, UInt16 maxRegs);

	private: inline void SaveState(Int32 pc, Int32 baseIndex, Int32 currentFuncIndex);

	public: inline Value LookupParamByName(String varName);

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
inline Value VM::ReplGlobals() { return get()->ReplGlobals; }
inline void VM::set_ReplGlobals(Value _v) { get()->ReplGlobals = _v; }
inline Value VM::pendingSelf() { return get()->pendingSelf; }
inline void VM::set_pendingSelf(Value _v) { get()->pendingSelf = _v; }
inline Value VM::pendingSuper() { return get()->pendingSuper; }
inline void VM::set_pendingSuper(Value _v) { get()->pendingSuper = _v; }
inline bool VM::hasPendingContext() { return get()->hasPendingContext; }
inline void VM::set_hasPendingContext(bool _v) { get()->hasPendingContext = _v; }
inline NativeCallbackDelegate VM::_pendingCallback() { return get()->_pendingCallback; }
inline void VM::set__pendingCallback(NativeCallbackDelegate _v) { get()->_pendingCallback = _v; }
inline Int32 VM::_pendingCalleeBase() { return get()->_pendingCalleeBase; } // base index for reconstructing Context
inline void VM::set__pendingCalleeBase(Int32 _v) { get()->_pendingCalleeBase = _v; } // base index for reconstructing Context
inline Int32 VM::_pendingArgCount() { return get()->_pendingArgCount; } // arg count for reconstructing Context
inline void VM::set__pendingArgCount(Int32 _v) { get()->_pendingArgCount = _v; } // arg count for reconstructing Context
inline Int32 VM::_pendingResultIndex() { return get()->_pendingResultIndex; } // absolute stack index for result (and partial result)
inline void VM::set__pendingResultIndex(Int32 _v) { get()->_pendingResultIndex = _v; } // absolute stack index for result (and partial result)
inline bool VM::yielding() { return get()->yielding; }
inline void VM::set_yielding(bool _v) { get()->yielding = _v; }
inline double VM::ElapsedTime() { return get()->ElapsedTime(); }
inline VM VM::_activeVM() { return get()->_activeVM; }
inline void VM::set__activeVM(VM _v) { get()->_activeVM = _v; }
inline Int32 VM::StackSize() { return get()->StackSize(); }
inline List<Value> VM::GetStack() { return get()->GetStack(); }
inline List<Value> VM::GetNames() { return get()->GetNames(); }
inline Int32 VM::CallStackDepth() { return get()->CallStackDepth(); }
inline Value VM::GetStackValue(Int32 index) { return get()->GetStackValue(index); }
inline Value VM::GetStackName(Int32 index) { return get()->GetStackName(index); }
inline CallInfo VM::GetCallStackFrame(Int32 index) { return get()->GetCallStackFrame(index); }
inline String VM::GetFunctionName(Int32 funcIndex) { return get()->GetFunctionName(funcIndex); }
inline void VM::InitVM(Int32 stackSlots,Int32 callSlots) { return get()->InitVM(stackSlots, callSlots); }
inline void VM::CleanupVM() { return get()->CleanupVM(); }
inline void VM::RegisterFunction(FuncDef funcDef) { return get()->RegisterFunction(funcDef); }
inline void VM::Reset(List<FuncDef> allFunctions) { return get()->Reset(allFunctions); }
inline void VM::Reset(List<FuncDef> allFunctions,Value replGlobals) { return get()->Reset(allFunctions, replGlobals); }
inline void VM::Stop() { return get()->Stop(); }
inline void VM::RaiseRuntimeError(String message) { return get()->RaiseRuntimeError(message); }
inline bool VM::ReportRuntimeError() { return get()->ReportRuntimeError(); }
inline Int32 VM::FunctionCount() { return get()->FunctionCount(); }
inline List<FuncDef> VM::GetFunctions() { return get()->GetFunctions(); }
inline Int32 VM::SelfParamOffset(FuncDefRef callee) { return get()->SelfParamOffset(callee); }
inline Int32 VM::ProcessArguments(Int32 argCount,Int32 selfParam,Int32 startPC,Int32 callerBase,Int32 calleeBase,FuncDefRef callee,List<UInt32> code) { return get()->ProcessArguments(argCount, selfParam, startPC, callerBase, calleeBase, callee, code); }
inline void VM::ApplyPendingContext(Int32 calleeBase,FuncDefRef callee) { return get()->ApplyPendingContext(calleeBase, callee); }
inline void VM::SetupCallFrame(Int32 argCount,Int32 selfParam,Int32 calleeBase,FuncDefRef callee) { return get()->SetupCallFrame(argCount, selfParam, calleeBase, callee); }
inline Int32 VM::AutoInvokeFuncRef(Value funcRefVal,Int32 resultReg,Int32 returnPC,Int32 baseIndex,Int32 currentFuncIndex,FuncDefRef curFunc) { return get()->AutoInvokeFuncRef(funcRefVal, resultReg, returnPC, baseIndex, currentFuncIndex, curFunc); }
inline bool VM::InvokeNativeCallback(NativeCallbackDelegate callback,Int32 calleeBase,Int32 argCount,IntrinsicResult partialResult,Int32 absoluteResultIndex) { return get()->InvokeNativeCallback(callback, calleeBase, argCount, partialResult, absoluteResultIndex); }
inline Value VM::Execute(FuncDef entry) { return get()->Execute(entry); }
inline Value VM::Execute(FuncDef entry,UInt32 maxCycles) { return get()->Execute(entry, maxCycles); }
inline Value VM::Run(UInt32 maxCycles) { return get()->Run(maxCycles); }
inline Value VM::RunInner(UInt32 maxCycles) { return get()->RunInner(maxCycles); }
inline void VM::EnsureFrame(Int32 baseIndex,UInt16 neededRegs) { return get()->EnsureFrame(baseIndex, neededRegs); }
inline void VMStorage::EnsureFrame(Int32 baseIndex,UInt16 neededRegs) {
	if (baseIndex + neededRegs > stack.Count()) {
		RaiseRuntimeError("Stack Overflow");
	}
}
inline Value VM::GetGlobalsVarMap() { return get()->GetGlobalsVarMap(); }
inline Value VM::GetCurrentLocalVarMap(Int32 baseIndex,UInt16 maxRegs) { return get()->GetCurrentLocalVarMap(baseIndex, maxRegs); }
inline Value VMStorage::GetCurrentLocalVarMap(Int32 baseIndex,UInt16 maxRegs) {
	GC_PUSH_SCOPE();
	Value result; GC_PROTECT(&result);
	if (callStackTop > 0) {
		CallInfo frame = callStack[callStackTop - 1];
		result = frame.GetLocalVarMap(stack, names, baseIndex, maxRegs);
		callStack[callStackTop - 1] = frame;  // write back (CallInfo is a struct)
		GC_POP_SCOPE();
		return result;
	} else {
		return make_varmap(&stack[0], &names[0], baseIndex, maxRegs);
	}
	GC_POP_SCOPE();
}
inline void VM::SaveState(Int32 pc,Int32 baseIndex,Int32 currentFuncIndex) { return get()->SaveState(pc, baseIndex, currentFuncIndex); }
inline void VMStorage::SaveState(Int32 pc,Int32 baseIndex,Int32 currentFuncIndex) {
	PC = pc;
	BaseIndex = baseIndex;
	_currentFuncIndex = currentFuncIndex;
	CurrentFunction = functions[currentFuncIndex];
}
inline Value VM::LookupParamByName(String varName) { return get()->LookupParamByName(varName); }
inline Value VM::LookupVariable(Value varName) { return get()->LookupVariable(varName); }

} // end of namespace MiniScript
