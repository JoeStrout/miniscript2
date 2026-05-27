// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VM.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
#include "value.h"
#include "FuncDef.g.h"
#include "IntrinsicAPI.g.h"
#include "ErrorTypes.g.h"
#include "value_map.h"
#include <vector>
#include <chrono>
#include "GCManager.g.h"

namespace MiniScript {

// DECLARATIONS

// Call stack frame (return info)
struct CallInfo {
	public: Int32 ReturnPC; // where to continue in caller (PC index)
	public: Int32 ReturnBase; // caller's base pointer (stack index)
	public: FuncDef ReturnFunc; // caller's function definition
	public: Int32 CopyResultToReg; // register number to copy result to, or -1
	public: Value LocalVarMap; // VarMap representing locals, if any
	public: Value OuterVarMap; // VarMap representing outer variables (closure context)

	public: CallInfo(Int32 returnPC, Int32 returnBase, FuncDef returnFunc, Int32 copyToReg=-1);

	public: CallInfo(Int32 returnPC, Int32 returnBase, FuncDef returnFunc, Int32 copyToReg, Value outerVars);

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
	private: Int32 callStackTop;
	private: Dictionary<String, Value> _intrinsics; // intrinsic name -> FuncRef Value
	public: Int32 PC;
	public: FuncDef CurrentFunction;
	public: Boolean IsRunning;
	public: Int32 BaseIndex;
	public: Value Error;
	private: Boolean _errorStackPending = false;
	public: Value ReplGlobals = val_null;
	private: Value pendingSelf;
	private: Value pendingSuper;
	private: bool hasPendingContext;
	private: NativeCallbackDelegate _pendingCallback;
	private: Int32 _pendingCalleeBase; // base index for reconstructing Context
	private: Int32 _pendingArgCount; // arg count for reconstructing Context
	private: Int32 _pendingResultIndex; // absolute stack index for result (and partial result)
	private: Boolean _hasPendingManualCall;
	private: Int32 _pendingManualCallDepth; // callStackTop value after the push
	public: Value ManualCallResult; // return value of the manually-pushed call
	public: bool yielding;
	private: std::chrono::steady_clock::time_point _startTime;

	// callStack is indexed by execution depth: callStack[0] is always @main's execution
	// context (globals live here), callStack[1] is the first user function call, etc.
	// callStackTop is the index of the next free slot (== current depth + 1).
	// Invariant: callStackTop >= 1 during all execution (set up in Reset).

	// Execution state (persistent across RunSteps calls)

	// True when a runtime error has been raised but its stack trace has not
	// yet been attached.  Errors are often raised mid-instruction, before VM
	// state (PC, CurrentFunction) has been saved; the stack trace is therefore
	// built later, at the next SaveState, when that state is accurate.

	// REPL mode: persistent globals VarMap shared across REPL entries.
	// When set (not val_null), used instead of callStack[0].GetLocalVarMap for globals.

	// Pending self/super for method calls, set by METHFIND/SETSELF,
	// consumed by the next CALL instruction

	// Pending intrinsic continuation: when an intrinsic returns done=false,
	// we store the state here so Run can re-invoke it on the next call.
	// The partial result value is stored in stack[_pendingResultIndex].

	// Support for manually-pushed calls (used by the import intrinsic).
	// When _hasPendingManualCall is true, Run() skips callback re-invocation
	// and runs RunInner so the pushed function can execute first.

	// Set by the "yield" intrinsic; host app can check and clear this.

	// Wall-clock start time, set in Reset(), used by the "time" intrinsic.
	
	public: double ElapsedTime();
	private: thread_local static VM _activeVM;

	// Thread-local active VM: set during Run(), so value operations
	// (like list_push) can report errors via the VM.
	public: static VM ActiveVM();

	public: Int32 StackSize();
	
	public: List<Value> GetStack();
	
	public: List<Value> GetNames();

	public: Int32 CallStackDepth();

	public: Value GetStackValue(Int32 index);

	public: Value GetStackName(Int32 index);

	public: CallInfo GetCallStackFrame(Int32 index);

	public: String FindShortName(Value v);

	public: VMStorage(Int32 stackSlots=1024, Int32 callSlots=256);

	private: void InitVM(Int32 stackSlots, Int32 callSlots);

	private: void CleanupVM();
	public: ~VMStorage() { CleanupVM(); }
	public: operator void*() { return this; }
	// Allows passing ctx.vm (VMStorage&) where void* vm is expected (e.g. to_string,
	// string_insert, etc.) without explicit casts at every call site.

	// GC mark callback responsible for protecting our stack, names, and
	// intrinsics from collection.
	public: static void MarkRoots(object user_data);

	// Mark a function's compile-time constants (used by the GC root scan).
	// Recursion into nested-function templates happens via GCFunction.MarkChildren.
	private: void MarkFuncConstants(FuncDef func);

	// Push a manually-constructed call to a set of compiled functions (used by import).
	// The first function in importFunctions is treated as @main for the pushed call.
	// After this returns, the VM will run that function before re-invoking the pending
	// intrinsic callback.  The function's return value is placed in ManualCallResult.
	// intrinsicCalleeBase is ctx.baseIndex from the calling intrinsic.
	public: void ManuallyPushCall(Int32 intrinsicCalleeBase, FuncDef importMain);

	// Set a variable by name in the current frame's locals VarMap.
	// Matches the runtime behavior of user code "locals[varName] = value":
	// if varName is already a named register, the value lands there directly;
	// otherwise a plain map entry is created and LookupVariable will find it.
	// In REPL mode (ReplGlobals != null) at the top level, writes to ReplGlobals
	// so the variable persists across REPL iterations.
	public: void SetVar(String varName, Value value);

	public: void Reset(List<FuncDef> allFunctions);

	public: void Reset(List<FuncDef> allFunctions, Value replGlobals);

	public: void Stop();

	// Stop the VM with a runtime error described by a string message.
	// Creates an error Value and stores it in Error.  The stack trace is
	// attached later (see FinalizeErrorStackTrace), once VM state has been
	// saved and accurately reflects the failing instruction.
	public: void RaiseRuntimeError(String message);

	// Attach a stack trace to a pending runtime error.  Called from SaveState,
	// so that PC and CurrentFunction reflect the instruction that failed.
	private: void FinalizeErrorStackTrace();

	// Stop the VM with a pre-built error value (e.g. an uncaught user error).
	public: void RaiseRuntimeError(Value error);

	// Called when user code silently ignored an error value and then tried
	// to use it in an operation that doesn't tolerate errors.
	// Returns IntrinsicResult.Null for convenience.
	public: IntrinsicResult RaiseUncaughtError(Value error);

	// Print the current error to stdout (useful for debug/standalone use).
	// Returns false if there is no error.
	public: bool ReportRuntimeError();

	// Build and return the current call stack as a frozen list of strings,
	// innermost (most recent) frame first.  Each entry has the form
	// "{file} line {lineNum}".  PC is the saved program counter at the
	// point of the call (typically vm.PC - 1 at the call site).
	public: Value BuildStackTrace();

	// Collect every function reachable from @main, by walking constant pools
	// for funcref templates.  Used for disassembly and debug output.
	public: List<FuncDef> GetFunctions();

	private: static void CollectFunctions(FuncDef func, List<FuncDef> outList);

	// Helper for argument processing (FUNCTION_CALLS.md steps 1-3):
	// Process ARG instructions, validate argument count, and set up parameter registers.
	// Returns the PC after the CALL instruction, or -1 on error.
	// Check whether pendingSelf should be injected as the first parameter.
	// Returns 1 if the callee's first param is named "self" and we have pending context, else 0.
	private: Int32 SelfParamOffset(FuncDef callee);

	private: Int32 ProcessArguments(Int32 argCount, Int32 selfParam, Int32 startPC, Int32 callerBase, Int32 calleeBase, FuncDef callee, List<UInt32> code);

	// Apply pending self/super context to a callee's frame, if any.
	// Called after SetupCallFrame to populate the callee's self/super registers.
	private: void ApplyPendingContext(Int32 calleeBase, FuncDef callee);

	// Helper for call setup (FUNCTION_CALLS.md steps 4-6):
	// Initialize remaining parameters with defaults and clear callee's registers.
	// Note: Parameters start at r1 (r0 is reserved for return value)
	// selfParam is 1 if pendingSelf was injected as the first arg, else 0.
	private: void SetupCallFrame(Int32 argCount, Int32 selfParam, Int32 calleeBase, FuncDef callee);

	// Auto-invoke a zero-arg funcref (used by LOADC and CALLIFREF).
	// Resolves the funcref, then either:
	//   - Native callback (done):    stores result, returns -1.
	//   - Native callback (pending): stores pending state, returns -2.
	//   - User function: pushes CallInfo and sets up callee frame, returns 0
	//     and sets calleeOut to the callee FuncDef.  Caller must switch its
	//     local execution state (pc, baseIndex, curFunc, etc.).
	// On error, calls RaiseRuntimeError and returns -1.
	private: Int32 AutoInvokeFuncRef(Value funcRefVal, Int32 resultReg, Int32 returnPC, Int32 baseIndex, FuncDef currentFunc, FuncDef* calleeOut);

	// Invoke a native callback and handle the result.  If done, writes the
	// result to stack[absoluteResultIndex] and returns true.  If not done,
	// stores the pending state for re-invocation and returns false.
	private: bool InvokeNativeCallback(NativeCallbackDelegate callback, Int32 calleeBase, Int32 argCount, IntrinsicResult partialResult, Int32 absoluteResultIndex);

	public: Value Execute(FuncDef entry);

	public: Value Execute(FuncDef entry, UInt32 maxCycles);

	public: Value Run(UInt32 maxCycles=0);

	private: Value RunInner(UInt32 maxCycles);

	private: void EnsureFrame(Int32 baseIndex, UInt16 neededRegs);
	void SwitchFrame(const FuncDef& currentFunc, Int32 baseIndex, FuncDefStorage* &curFuncRaw, Int32 &codeCount, UInt32* &curCode, Value* &curConstants, Value* &localStack, Value* stackPtr);

	// Switch all frame-local execution state to the given function.

	// Get the globals VarMap.  In REPL mode, returns the persistent ReplGlobals.
	// Otherwise, returns @main's cached callStack[0].LocalVarMap (creating it on
	// first use).  The cache stays current because NAME_rA_kBC keeps a live frame
	// VarMap in sync as new variables are declared.  callStack[0].ReturnFunc
	// holds @main's own function index (by convention for this slot), used to
	// find @main's MaxRegs.
	private: Value GetGlobalsVarMap();

	// Get or create a VarMap for the current call frame's local variables.
	// callStack[callStackTop-1] is always the current execution-context frame
	// (callStackTop >= 1 is guaranteed since @main's frame is always at callStack[0]).
	private: Value GetCurrentLocalVarMap(Int32 baseIndex, UInt16 maxRegs);

	private: void SaveState(Int32 pc, Int32 baseIndex, FuncDef currentFunc);

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
	public: operator void*() { return storage.get(); }
	// Allows passing a VM where void* vm is expected, same as VMStorage.
	public: Interpreter GetInterpreter(); // NO_INLINE
	private: List<CallInfo> callStack();
	private: void set_callStack(List<CallInfo> _v);
	private: Int32 callStackTop();
	private: void set_callStackTop(Int32 _v);
	private: Dictionary<String, Value> _intrinsics(); // intrinsic name -> FuncRef Value
	private: void set__intrinsics(Dictionary<String, Value> _v); // intrinsic name -> FuncRef Value
	public: Int32 PC();
	public: void set_PC(Int32 _v);
	public: FuncDef CurrentFunction();
	public: void set_CurrentFunction(FuncDef _v);
	public: Boolean IsRunning();
	public: void set_IsRunning(Boolean _v);
	public: Int32 BaseIndex();
	public: void set_BaseIndex(Int32 _v);
	public: Value Error();
	public: void set_Error(Value _v);
	private: Boolean _errorStackPending();
	private: void set__errorStackPending(Boolean _v);
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
	private: Boolean _hasPendingManualCall();
	private: void set__hasPendingManualCall(Boolean _v);
	private: Int32 _pendingManualCallDepth(); // callStackTop value after the push
	private: void set__pendingManualCallDepth(Int32 _v); // callStackTop value after the push
	public: Value ManualCallResult(); // return value of the manually-pushed call
	public: void set_ManualCallResult(Value _v); // return value of the manually-pushed call
	public: bool yielding();
	public: void set_yielding(bool _v);

	// callStack is indexed by execution depth: callStack[0] is always @main's execution
	// context (globals live here), callStack[1] is the first user function call, etc.
	// callStackTop is the index of the next free slot (== current depth + 1).
	// Invariant: callStackTop >= 1 during all execution (set up in Reset).

	// Execution state (persistent across RunSteps calls)

	// True when a runtime error has been raised but its stack trace has not
	// yet been attached.  Errors are often raised mid-instruction, before VM
	// state (PC, CurrentFunction) has been saved; the stack trace is therefore
	// built later, at the next SaveState, when that state is accurate.

	// REPL mode: persistent globals VarMap shared across REPL entries.
	// When set (not val_null), used instead of callStack[0].GetLocalVarMap for globals.

	// Pending self/super for method calls, set by METHFIND/SETSELF,
	// consumed by the next CALL instruction

	// Pending intrinsic continuation: when an intrinsic returns done=false,
	// we store the state here so Run can re-invoke it on the next call.
	// The partial result value is stored in stack[_pendingResultIndex].

	// Support for manually-pushed calls (used by the import intrinsic).
	// When _hasPendingManualCall is true, Run() skips callback re-invocation
	// and runs RunInner so the pushed function can execute first.

	// Set by the "yield" intrinsic; host app can check and clear this.

	// Wall-clock start time, set in Reset(), used by the "time" intrinsic.
	
	public: inline double ElapsedTime();
	private: VM _activeVM();
	private: void set__activeVM(VM _v);

	// Thread-local active VM: set during Run(), so value operations
	// (like list_push) can report errors via the VM.
	public: static VM ActiveVM() { return VMStorage::ActiveVM(); }

	public: inline Int32 StackSize();
	
	public: inline List<Value> GetStack();
	
	public: inline List<Value> GetNames();

	public: inline Int32 CallStackDepth();

	public: inline Value GetStackValue(Int32 index);

	public: inline Value GetStackName(Int32 index);

	public: inline CallInfo GetCallStackFrame(Int32 index);

	public: inline String FindShortName(Value v);

	public: static VM New(Int32 stackSlots=1024, Int32 callSlots=256) {
		return VM(std::make_shared<VMStorage>(stackSlots, callSlots));
	}

	private: inline void InitVM(Int32 stackSlots, Int32 callSlots);

	private: inline void CleanupVM();

	// GC mark callback responsible for protecting our stack, names, and
	// intrinsics from collection.
	public: static void MarkRoots(object user_data) { return VMStorage::MarkRoots(user_data); }

	// Mark a function's compile-time constants (used by the GC root scan).
	// Recursion into nested-function templates happens via GCFunction.MarkChildren.
	private: inline void MarkFuncConstants(FuncDef func);

	// Push a manually-constructed call to a set of compiled functions (used by import).
	// The first function in importFunctions is treated as @main for the pushed call.
	// After this returns, the VM will run that function before re-invoking the pending
	// intrinsic callback.  The function's return value is placed in ManualCallResult.
	// intrinsicCalleeBase is ctx.baseIndex from the calling intrinsic.
	public: inline void ManuallyPushCall(Int32 intrinsicCalleeBase, FuncDef importMain);

	// Set a variable by name in the current frame's locals VarMap.
	// Matches the runtime behavior of user code "locals[varName] = value":
	// if varName is already a named register, the value lands there directly;
	// otherwise a plain map entry is created and LookupVariable will find it.
	// In REPL mode (ReplGlobals != null) at the top level, writes to ReplGlobals
	// so the variable persists across REPL iterations.
	public: inline void SetVar(String varName, Value value);

	public: inline void Reset(List<FuncDef> allFunctions);

	public: inline void Reset(List<FuncDef> allFunctions, Value replGlobals);

	public: inline void Stop();

	// Stop the VM with a runtime error described by a string message.
	// Creates an error Value and stores it in Error.  The stack trace is
	// attached later (see FinalizeErrorStackTrace), once VM state has been
	// saved and accurately reflects the failing instruction.
	public: inline void RaiseRuntimeError(String message);

	// Attach a stack trace to a pending runtime error.  Called from SaveState,
	// so that PC and CurrentFunction reflect the instruction that failed.
	private: inline void FinalizeErrorStackTrace();

	// Stop the VM with a pre-built error value (e.g. an uncaught user error).
	public: inline void RaiseRuntimeError(Value error);

	// Called when user code silently ignored an error value and then tried
	// to use it in an operation that doesn't tolerate errors.
	// Returns IntrinsicResult.Null for convenience.
	public: inline IntrinsicResult RaiseUncaughtError(Value error);

	// Print the current error to stdout (useful for debug/standalone use).
	// Returns false if there is no error.
	public: inline bool ReportRuntimeError();

	// Build and return the current call stack as a frozen list of strings,
	// innermost (most recent) frame first.  Each entry has the form
	// "{file} line {lineNum}".  PC is the saved program counter at the
	// point of the call (typically vm.PC - 1 at the call site).
	public: inline Value BuildStackTrace();

	// Collect every function reachable from @main, by walking constant pools
	// for funcref templates.  Used for disassembly and debug output.
	public: inline List<FuncDef> GetFunctions();

	private: static void CollectFunctions(FuncDef func, List<FuncDef> outList) { return VMStorage::CollectFunctions(func, outList); }

	// Helper for argument processing (FUNCTION_CALLS.md steps 1-3):
	// Process ARG instructions, validate argument count, and set up parameter registers.
	// Returns the PC after the CALL instruction, or -1 on error.
	// Check whether pendingSelf should be injected as the first parameter.
	// Returns 1 if the callee's first param is named "self" and we have pending context, else 0.
	private: inline Int32 SelfParamOffset(FuncDef callee);

	private: inline Int32 ProcessArguments(Int32 argCount, Int32 selfParam, Int32 startPC, Int32 callerBase, Int32 calleeBase, FuncDef callee, List<UInt32> code);

	// Apply pending self/super context to a callee's frame, if any.
	// Called after SetupCallFrame to populate the callee's self/super registers.
	private: inline void ApplyPendingContext(Int32 calleeBase, FuncDef callee);

	// Helper for call setup (FUNCTION_CALLS.md steps 4-6):
	// Initialize remaining parameters with defaults and clear callee's registers.
	// Note: Parameters start at r1 (r0 is reserved for return value)
	// selfParam is 1 if pendingSelf was injected as the first arg, else 0.
	private: inline void SetupCallFrame(Int32 argCount, Int32 selfParam, Int32 calleeBase, FuncDef callee);

	// Auto-invoke a zero-arg funcref (used by LOADC and CALLIFREF).
	// Resolves the funcref, then either:
	//   - Native callback (done):    stores result, returns -1.
	//   - Native callback (pending): stores pending state, returns -2.
	//   - User function: pushes CallInfo and sets up callee frame, returns 0
	//     and sets calleeOut to the callee FuncDef.  Caller must switch its
	//     local execution state (pc, baseIndex, curFunc, etc.).
	// On error, calls RaiseRuntimeError and returns -1.
	private: inline Int32 AutoInvokeFuncRef(Value funcRefVal, Int32 resultReg, Int32 returnPC, Int32 baseIndex, FuncDef currentFunc, FuncDef* calleeOut);

	// Invoke a native callback and handle the result.  If done, writes the
	// result to stack[absoluteResultIndex] and returns true.  If not done,
	// stores the pending state for re-invocation and returns false.
	private: inline bool InvokeNativeCallback(NativeCallbackDelegate callback, Int32 calleeBase, Int32 argCount, IntrinsicResult partialResult, Int32 absoluteResultIndex);

	public: inline Value Execute(FuncDef entry);

	public: inline Value Execute(FuncDef entry, UInt32 maxCycles);

	public: inline Value Run(UInt32 maxCycles=0);

	private: inline Value RunInner(UInt32 maxCycles);

	private: inline void EnsureFrame(Int32 baseIndex, UInt16 neededRegs);

	// Switch all frame-local execution state to the given function.

	// Get the globals VarMap.  In REPL mode, returns the persistent ReplGlobals.
	// Otherwise, returns @main's cached callStack[0].LocalVarMap (creating it on
	// first use).  The cache stays current because NAME_rA_kBC keeps a live frame
	// VarMap in sync as new variables are declared.  callStack[0].ReturnFunc
	// holds @main's own function index (by convention for this slot), used to
	// find @main's MaxRegs.
	private: inline Value GetGlobalsVarMap();

	// Get or create a VarMap for the current call frame's local variables.
	// callStack[callStackTop-1] is always the current execution-context frame
	// (callStackTop >= 1 is guaranteed since @main's frame is always at callStack[0]).
	private: inline Value GetCurrentLocalVarMap(Int32 baseIndex, UInt16 maxRegs);

	private: inline void SaveState(Int32 pc, Int32 baseIndex, FuncDef currentFunc);

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
inline Int32 VM::callStackTop() { return get()->callStackTop; }
inline void VM::set_callStackTop(Int32 _v) { get()->callStackTop = _v; }
inline Dictionary<String, Value> VM::_intrinsics() { return get()->_intrinsics; } // intrinsic name -> FuncRef Value
inline void VM::set__intrinsics(Dictionary<String, Value> _v) { get()->_intrinsics = _v; } // intrinsic name -> FuncRef Value
inline Int32 VM::PC() { return get()->PC; }
inline void VM::set_PC(Int32 _v) { get()->PC = _v; }
inline FuncDef VM::CurrentFunction() { return get()->CurrentFunction; }
inline void VM::set_CurrentFunction(FuncDef _v) { get()->CurrentFunction = _v; }
inline Boolean VM::IsRunning() { return get()->IsRunning; }
inline void VM::set_IsRunning(Boolean _v) { get()->IsRunning = _v; }
inline Int32 VM::BaseIndex() { return get()->BaseIndex; }
inline void VM::set_BaseIndex(Int32 _v) { get()->BaseIndex = _v; }
inline Value VM::Error() { return get()->Error; }
inline void VM::set_Error(Value _v) { get()->Error = _v; }
inline Boolean VM::_errorStackPending() { return get()->_errorStackPending; }
inline void VM::set__errorStackPending(Boolean _v) { get()->_errorStackPending = _v; }
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
inline Boolean VM::_hasPendingManualCall() { return get()->_hasPendingManualCall; }
inline void VM::set__hasPendingManualCall(Boolean _v) { get()->_hasPendingManualCall = _v; }
inline Int32 VM::_pendingManualCallDepth() { return get()->_pendingManualCallDepth; } // callStackTop value after the push
inline void VM::set__pendingManualCallDepth(Int32 _v) { get()->_pendingManualCallDepth = _v; } // callStackTop value after the push
inline Value VM::ManualCallResult() { return get()->ManualCallResult; } // return value of the manually-pushed call
inline void VM::set_ManualCallResult(Value _v) { get()->ManualCallResult = _v; } // return value of the manually-pushed call
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
inline String VM::FindShortName(Value v) { return get()->FindShortName(v); }
inline void VM::InitVM(Int32 stackSlots,Int32 callSlots) { return get()->InitVM(stackSlots, callSlots); }
inline void VM::CleanupVM() { return get()->CleanupVM(); }
inline void VM::MarkFuncConstants(FuncDef func) { return get()->MarkFuncConstants(func); }
inline void VM::ManuallyPushCall(Int32 intrinsicCalleeBase,FuncDef importMain) { return get()->ManuallyPushCall(intrinsicCalleeBase, importMain); }
inline void VM::SetVar(String varName,Value value) { return get()->SetVar(varName, value); }
inline void VM::Reset(List<FuncDef> allFunctions) { return get()->Reset(allFunctions); }
inline void VM::Reset(List<FuncDef> allFunctions,Value replGlobals) { return get()->Reset(allFunctions, replGlobals); }
inline void VM::Stop() { return get()->Stop(); }
inline void VM::RaiseRuntimeError(String message) { return get()->RaiseRuntimeError(message); }
inline void VM::FinalizeErrorStackTrace() { return get()->FinalizeErrorStackTrace(); }
inline void VM::RaiseRuntimeError(Value error) { return get()->RaiseRuntimeError(error); }
inline IntrinsicResult VM::RaiseUncaughtError(Value error) { return get()->RaiseUncaughtError(error); }
inline bool VM::ReportRuntimeError() { return get()->ReportRuntimeError(); }
inline Value VM::BuildStackTrace() { return get()->BuildStackTrace(); }
inline List<FuncDef> VM::GetFunctions() { return get()->GetFunctions(); }
inline Int32 VM::SelfParamOffset(FuncDef callee) { return get()->SelfParamOffset(callee); }
inline Int32 VM::ProcessArguments(Int32 argCount,Int32 selfParam,Int32 startPC,Int32 callerBase,Int32 calleeBase,FuncDef callee,List<UInt32> code) { return get()->ProcessArguments(argCount, selfParam, startPC, callerBase, calleeBase, callee, code); }
inline void VM::ApplyPendingContext(Int32 calleeBase,FuncDef callee) { return get()->ApplyPendingContext(calleeBase, callee); }
inline void VM::SetupCallFrame(Int32 argCount,Int32 selfParam,Int32 calleeBase,FuncDef callee) { return get()->SetupCallFrame(argCount, selfParam, calleeBase, callee); }
inline Int32 VM::AutoInvokeFuncRef(Value funcRefVal,Int32 resultReg,Int32 returnPC,Int32 baseIndex,FuncDef currentFunc,FuncDef* calleeOut) { return get()->AutoInvokeFuncRef(funcRefVal, resultReg, returnPC, baseIndex, currentFunc, calleeOut); }
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
	// At the top level (@main), locals and globals are the same object.
	if (callStackTop == 1) return GetGlobalsVarMap();
	CallInfo frame = callStack[callStackTop - 1];
	Value result = frame.GetLocalVarMap(stack, names, baseIndex, maxRegs);
	callStack[callStackTop - 1] = frame;  // write back (CallInfo is a struct)
	return result;
}
inline void VM::SaveState(Int32 pc,Int32 baseIndex,FuncDef currentFunc) { return get()->SaveState(pc, baseIndex, currentFunc); }
inline void VMStorage::SaveState(Int32 pc,Int32 baseIndex,FuncDef currentFunc) {
	PC = pc;
	BaseIndex = baseIndex;
	CurrentFunction = currentFunc;
	if (_errorStackPending) FinalizeErrorStackTrace();
}
inline Value VM::LookupParamByName(String varName) { return get()->LookupParamByName(varName); }
inline Value VM::LookupVariable(Value varName) { return get()->LookupVariable(varName); }

} // end of namespace MiniScript
