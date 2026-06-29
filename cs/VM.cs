using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
// H: #include "value.h"
// H: #include "FuncDef.g.h"
// H: #include "IntrinsicAPI.g.h"
// H: #include "ErrorTypes.g.h"
// H: #include "value_map.h"
// H: #include <vector>
// H: #include <chrono>
// H: #include "GCManager.g.h"
// CPP: #include "value_list.h"
// CPP: #include "value_string.h"
// CPP: #include "Bytecode.g.h"
// CPP: #include "Intrinsic.g.h"
// CPP: #include "CoreIntrinsics.g.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "Disassembler.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "CS_value_util.h"
// CPP: #include "dispatch_macros.h"
// CPP: #include "vm_error.h"
// CPP: #include "Interpreter.g.h"
// CPP: #include <chrono>

using static MiniScript.Value;

namespace MiniScript {

// Call stack frame (return info)
public struct CallInfo {
	public Int32 ReturnPC;        // where to continue in caller (PC index)
	public Int32 ReturnBase;      // caller's base pointer (stack index)
	public FuncDef ReturnFunc;    // caller's function definition
	public Int32 CopyResultToReg; // register number to copy result to, or -1
	public Value LocalVarMap;     // VarMap representing locals, if any
	public Value OuterVarMap;     // VarMap representing outer variables (closure context)

	public CallInfo(Int32 returnPC, Int32 returnBase, FuncDef returnFunc, Int32 copyToReg=-1) {
		ReturnPC = returnPC;
		ReturnBase = returnBase;
		ReturnFunc = returnFunc;
		CopyResultToReg = copyToReg;
		LocalVarMap = Value.Null;
		OuterVarMap = Value.Null;
	}

	public CallInfo(Int32 returnPC, Int32 returnBase, FuncDef returnFunc, Int32 copyToReg, Value outerVars) {
		ReturnPC = returnPC;
		ReturnBase = returnBase;
		ReturnFunc = returnFunc;
		CopyResultToReg = copyToReg;
		LocalVarMap = Value.Null;
		OuterVarMap = outerVars;
	}

	public Value GetLocalVarMap(List<Value> registers, List<Value> names, int baseIdx, int regCount) {
		if (LocalVarMap.IsNull()) {
			// Create a new VarMap with references to VM's stack and names arrays
			if (regCount == 0) {
				// We have no local vars at all!  Make an ordinary map.
				LocalVarMap = make_map(4);	// This is safe, right?
			} else {
				LocalVarMap = make_varmap(registers, names, baseIdx, regCount);
			}
		}
		return LocalVarMap;
	}
}

// VM state
public class VM {
	public Boolean DebugMode = false;
	private List<Value> stack;
	private List<Value> names;		// Variable names parallel to stack (null if unnamed)

	// Reference to the Interpreter that owns this VM (may be null if VM is used standalone).
	// (Note that in C++, this is a raw pointer to the InterpreterStorage, due to annoying
	// circular-header-dependency issues.)  The same Get/Set interface works on both C#/C++.
	private Interpreter interpreter = null; // H: private: InterpreterStorage* interpreter = nullptr;
	public void SetInterpreter(Interpreter interp) { // NO_INLINE
		interpreter = interp; // CPP: interpreter = interp.get_storage();
	}
	// H_WRAPPER: public: void SetInterpreter(InterpreterStorage* p) { get()->interpreter = p; }
	// H_WRAPPER: public: operator void*() { return storage.get(); }
	// H_WRAPPER: // Allows passing a VM where void* vm is expected, same as VMStorage.
	public Interpreter GetInterpreter() { // NO_INLINE
		return interpreter; // CPP: return Interpreter(interpreter);
	}

	// callStack is indexed by execution depth: callStack[0] is always @main's execution
	// context (globals live here), callStack[1] is the first user function call, etc.
	// callStackTop is the index of the next free slot (== current depth + 1).
	// Invariant: callStackTop >= 1 during all execution (set up in Reset).
	private List<CallInfo> callStack;
	private Int32 callStackTop;

	private Dictionary<String, Value> _intrinsics; // intrinsic name -> FuncRef Value

	// Execution state (persistent across RunSteps calls)
	public Int32 PC { get; private set; }
	public FuncDef CurrentFunction { get; private set; }
	public Boolean IsRunning { get; private set; }
	public Int32 BaseIndex { get; private set; }
	public Value Error { get; private set; }

	// True when a runtime error has been raised but its stack trace has not
	// yet been attached.  Errors are often raised mid-instruction, before VM
	// state (PC, CurrentFunction) has been saved; the stack trace is therefore
	// built later, at the next SaveState, when that state is accurate.
	private Boolean _errorStackPending = false;

	// REPL mode: persistent globals VarMap shared across REPL entries.
	// When set (not Value.Null), used instead of callStack[0].GetLocalVarMap for globals.
	public Value ReplGlobals = Value.Null;

	// Pending self/super for method calls, set by METHFIND/SETSELF,
	// consumed by the next CALL instruction
	private Value pendingSelf;
	private Value pendingSuper;
	private bool hasPendingContext;

	// Pending intrinsic continuation: when an intrinsic returns done=false,
	// we store the state here so Run can re-invoke it on the next call.
	// The partial result value is stored in stack[_pendingResultIndex].
	private NativeCallbackDelegate _pendingCallback = null;
	private Int32 _pendingCalleeBase = 0;   // base index for reconstructing Context
	private Int32 _pendingArgCount = 0;     // arg count for reconstructing Context
	private Int32 _pendingResultIndex = 0;  // absolute stack index for result (and partial result)

	// Support for manually-pushed calls (used by the import intrinsic).
	// When _hasPendingManualCall is true, Run() skips callback re-invocation
	// and runs RunInner so the pushed function can execute first.
	private Boolean _hasPendingManualCall = false;
	private Int32 _pendingManualCallDepth = 0;  // callStackTop value after the push
	public Value ManualCallResult = Value.Null;    // return value of the manually-pushed call

	// Set by the "yield" intrinsic; host app can check and clear this.
	public bool yielding = false;

	// Wall-clock start time, set in Reset(), used by the "time" intrinsic.
	//*** BEGIN CS_ONLY ***
	private System.Diagnostics.Stopwatch _stopwatch = new System.Diagnostics.Stopwatch();
	//*** END CS_ONLY ***
	/*** BEGIN H_ONLY ***
	private: std::chrono::steady_clock::time_point _startTime;
	*** END H_ONLY ***/
	
	public double ElapsedTime() {
		// CPP: auto now = std::chrono::steady_clock::now();
		return _stopwatch.Elapsed.TotalSeconds; // CPP: return std::chrono::duration<double>(now - _startTime).count();
	}

	// Thread-local active VM: set during Run(), so value operations
	// (like list_push) can report errors via the VM.
	[ThreadStatic] private static VM _activeVM;
	public static VM ActiveVM() {
		return _activeVM;
	}

	public Int32 StackSize() {
		return stack.Count;
	}
	
	public List<Value> GetStack() {
		return stack;
	}
	
	public List<Value> GetNames() {
		return names;
	}

	public Int32 CallStackDepth() {
		return callStackTop;
	}

	public Value GetStackValue(Int32 index) {
		if (index < 0 || index >= stack.Count) return Value.Null;
		return stack[index];
	}

	public Value GetStackName(Int32 index) {
		if (index < 0 || index >= names.Count) return Value.Null;
		return names[index];
	}

	public CallInfo GetCallStackFrame(Int32 index) {
		if (index < 0 || index >= callStackTop) return new CallInfo(0, 0, null);
		return callStack[index];
	}

	public String FindShortName(Value v) {
		// Search global variable names directly on the stack — allocation-free and GC-safe.
		FuncDef rf = callStack[0].ReturnFunc;
		Int32 regCount = rf.MaxRegs;
		for (Int32 i = 0; i < regCount; i++) {
			if (!names[i].IsNull() && value_identical(stack[i], v) && !value_identical(names[i], v))
				return to_String(names[i]);
		}
		// Fall back to the intrinsic short-name registry (type maps, etc.)
		return Intrinsic.GetShortName(v);
	}

	public VM(Int32 stackSlots=10240, Int32 callSlots=256) {
		InitVM(stackSlots, callSlots);
	}

	private void InitVM(Int32 stackSlots, Int32 callSlots) {
		stack = new List<Value>();
		names = new List<Value>();
		callStack = new List<CallInfo>();
		callStackTop = 0;
		Error = Value.Null;

		// Initialize stack with null values
		for (Int32 i = 0; i < stackSlots; i++) {
			stack.Add(Value.Null);
			names.Add(Value.Null);		// No variable name initially
		}
		
		
		// Pre-allocate call stack capacity
		for (Int32 i = 0; i < callSlots; i++) {
			callStack.Add(new CallInfo(0, 0, null));
		}
		
		// Register as a source of roots for the GC system.
		GCManager.RegisterMarkCallback(MarkRoots, this); // CPP: GCManager::RegisterMarkCallback(VMStorage::MarkRoots, this);

		/*** BEGIN CPP_ONLY ***
		// Ensure that runtime errors are routed through the active VM
		vm_error_set_callback([](const char* msg) {
			VM vm = VMStorage::ActiveVM();
			if (!IsNull(vm)) vm.RaiseRuntimeError(String(msg));
		});

		// Wire code_form's short-name hook to this VM's FindShortName. The
		// hook lives in value.cpp (layer 2A) and takes a void* vm so it can
		// stay free of the VM layer; we cast back here.
		set_short_name_lookup([](void* vm_ptr, Value v) -> Value {
			String s = static_cast<VMStorage*>(vm_ptr)->FindShortName(v);
			if (s.Length() == 0) return Value::Null;
			return make_string(s.c_str());
		});

		// Wire value.cpp's runtime-error constructor hook (layer 2A) to the
		// active VM's MakeRuntimeError, so errors created from value_mult and
		// friends carry the runtime __isa *and* an accurate stack trace, matching
		// the C# side.  Falls back to a bare error if there is no active VM.
		set_runtime_error_maker([](const char* msg) -> Value {
			VM vm = VMStorage::ActiveVM();
			if (IsNull(vm)) return make_error(make_string(msg), Value::Null, Value::Null, Value::Null);
			return vm.MakeRuntimeError(String(msg));
		});

		// Wire value.cpp's stack-trace hook (layer 2A) to the active VM, so
		// ErrorTypes.RuntimeError can attach an accurate stack trace to every
		// runtime error value it creates.
		set_stack_trace_hook([]() -> Value {
			VM vm = VMStorage::ActiveVM();
			if (IsNull(vm)) return Value::Null;
			return vm.CurrentStackTrace();
		});
		*** END CPP_ONLY ***/
	}

	private void CleanupVM() {
		GCManager.UnregisterMarkCallback(MarkRoots, this); // CPP: GCManager::UnregisterMarkCallback(VMStorage::MarkRoots, this);
	}

	// H: public: ~VMStorage() { CleanupVM(); }
	// H: public: operator void*() { return this; }
	// H: // Allows passing ctx.vm (VMStorage&) where void* vm is expected (e.g. to_string,
	// H: // string_insert, etc.) without explicit casts at every call site.

	// GC mark callback responsible for protecting our stack, names, and
	// intrinsics from collection.
	public static void MarkRoots(object user_data) {
		VM vm = (VM)user_data; // CPP: VM vm(static_cast<VMStorage*>(user_data)->shared_from_this());
		Int32 liveTop = (vm.CurrentFunction != null) ? vm.BaseIndex + vm.CurrentFunction.MaxRegs : 0;
		for (Int32 i = 0; i < liveTop; i++) {
			GCManager.Mark(vm.stack[i]);
			GCManager.Mark(vm.names[i]);
		}
		// Intrinsic funcrefs are permanent GC roots (added via GCManager.AddRoot in
		// Intrinsic.EnsureBuilt), so they don't need to be marked here.
		// Mark compile-time constants of every function on the call chain.
		// Marking a function's constants reaches its nested-function templates
		// (themselves funcrefs), which cascade through GCFunction.MarkChildren —
		// so this transitively keeps the whole reachable FuncDef graph alive.
		vm.MarkFuncConstants(vm.CurrentFunction);
		// Mark LocalVarMap and OuterVarMap stored in CallInfo structs, plus the
		// caller FuncDef recorded in each frame.  These are not reachable from
		// the stack scan and must be marked explicitly.
		for (Int32 ci = 0; ci < vm.callStackTop; ci++) {
			GCManager.Mark(vm.callStack[ci].LocalVarMap);
			GCManager.Mark(vm.callStack[ci].OuterVarMap);
			vm.MarkFuncConstants(vm.callStack[ci].ReturnFunc);
		}
		GCManager.Mark(vm.ManualCallResult);
	}

	// Mark a function's compile-time constants (used by the GC root scan).
	// Recursion into nested-function templates happens via GCFunction.MarkChildren.
	private void MarkFuncConstants(FuncDef func) {
		if (func == null) return;
		List<Value> consts = func.Constants;
		for (Int32 i = 0; i < consts.Count; i++) GCManager.Mark(consts[i]);
	}

	// Push a manually-constructed call to a set of compiled functions (used by import).
	// The first function in importFunctions is treated as @main for the pushed call.
	// After this returns, the VM will run that function before re-invoking the pending
	// intrinsic callback.  The function's return value is placed in ManualCallResult.
	// intrinsicCalleeBase is ctx.baseIndex from the calling intrinsic.
	public void ManuallyPushCall(Int32 intrinsicCalleeBase, FuncDef importMain) {
		// Module frame sits just above the import intrinsic's 2-register frame (r0 + libname).
		Int32 calleeBase = intrinsicCalleeBase + 2;
		if (!EnsureFrame(calleeBase, importMain.MaxRegs)) return;
		for (Int32 i = 0; i < importMain.MaxRegs; i++) {
			stack[calleeBase + i] = Value.Null;
			names[calleeBase + i] = Value.Null;
		}

		if (callStackTop >= callStack.Count) callStack.Add(new CallInfo(0, 0, null));
		callStack[callStackTop] = new CallInfo(PC, BaseIndex, CurrentFunction);
		callStackTop++;

		BaseIndex = calleeBase;
		CurrentFunction = importMain;
		PC = 0;

		_hasPendingManualCall = true;
		_pendingManualCallDepth = callStackTop;
		ManualCallResult = Value.Null;
	}

	// Set a variable by name in the current frame's locals VarMap.
	// Matches the runtime behavior of user code "locals[varName] = value":
	// if varName is already a named register, the value lands there directly;
	// otherwise a plain map entry is created and LookupVariable will find it.
	// In REPL mode (ReplGlobals != null) at the top level, writes to ReplGlobals
	// so the variable persists across REPL iterations.
	public void SetVar(String varName, Value value) {
		Value targetMap;
		if (!ReplGlobals.IsNull() && callStackTop <= 1) {
			targetMap = ReplGlobals;
		} else {
			targetMap = GetCurrentLocalVarMap(BaseIndex, CurrentFunction.MaxRegs);
		}
		map_set(targetMap, varName, value);
	}


	public void Reset(List<FuncDef> allFunctions) {
		Reset(allFunctions, Value.Null);
	}

	public void Reset(List<FuncDef> allFunctions, Value replGlobals) {
		bool partialReset = !replGlobals.IsNull();

		// Locate @main.  All other functions are reachable from @main via its
		// constant pool (nested-function templates), so the VM keeps no
		// functions list — it just needs the entry point.
		FuncDef mainFunc = null;
		for (Int32 i = 0; i < allFunctions.Count; i++) {
			if (allFunctions[i].Name == "@main") mainFunc = allFunctions[i];
		}

		// Intrinsics are built once and shared; (re)build the name->funcref
		// table on a full reset.  In REPL mode it already exists.
		if (!partialReset) {
			_intrinsics = new Dictionary<String, Value>();
			Intrinsic.RegisterAll(_intrinsics);
		}

		// Basic validation
		if (mainFunc == null) {
			IOHelper.Print("ERROR: No @main function found in VM.Reset");
			return;
		}

		if (mainFunc.Code.Count == 0) {
			IOHelper.Print("Entry function has no code");
			return;
		}

		// Initialize execution state
		BaseIndex = 0;			  // entry executes at stack base
		PC = 0;				 // start at entry code
		CurrentFunction = mainFunc;
		IsRunning = true;
		callStackTop = 0;
		// Push @main's own execution-context frame at callStack[0] so that
		// globals (= @main's locals) are always accessible via callStack[0].
		// This means real function calls start at callStack[1], callStack[2], etc.
		// ReturnFunc is set to @main so GetGlobalsVarMap can find @main's MaxRegs.
		callStack[0] = new CallInfo(0, 0, mainFunc);
		callStackTop = 1;
		Error = Value.Null;
		pendingSelf = Value.Null;
		pendingSuper = Value.Null;
		hasPendingContext = false;

		EnsureFrame(BaseIndex, CurrentFunction.MaxRegs);

		// In REPL mode, rebind the persistent globals VarMap to the new stack arrays
		if (partialReset) {
			ReplGlobals = replGlobals;
			Int32 capacity = stack.Count;
			// careful: don't release old stacks until after varmap_rebind (below)
			List<Value> newStack = new List<Value>(capacity);	
			List<Value> newNames = new List<Value>(capacity);
			for (int i=0; i<capacity; i++) {
				newStack.Add(Value.Null);
				newNames.Add(Value.Null);
			}
			varmap_rebind(ReplGlobals, newStack, newNames);
			stack = newStack;
			names = newNames;
		}

		// Start the run timer (e.g. for the `time` intrinsic)
		_stopwatch.Restart(); // CPP: _startTime = std::chrono::steady_clock::now();

		if (DebugMode) {
			IOHelper.Print(StringUtils.Format("VM Reset: Executing {0}", mainFunc.Name));
		}
	}

	public void Stop() {
		IsRunning = false;
	}

	// Stop the VM with a runtime error described by a string message.
	// Creates an error Value and stores it in Error.  The stack trace is
	// attached later (see FinalizeErrorStackTrace), once VM state has been
	// saved and accurately reflects the failing instruction.
	public void RaiseRuntimeError(String message) {
		Error = ErrorTypes.RuntimeError(message);
		_errorStackPending = true;
		IsRunning = false;
	}

	// Attach a stack trace to a pending runtime error.  Called from SaveState,
	// so that PC and CurrentFunction reflect the instruction that failed.
	private void FinalizeErrorStackTrace() {
		_errorStackPending = false;
		if (!Error.IsError()) return;
		Value stack = CurrentFunction ? BuildStackTrace() : Value.Null;
		Int32 idx = value_item_index(Error);
		GCError ge = GCManager.Errors.Get(idx);
		GCManager.Errors.SetFields(idx, ge.Message, ge.Inner, stack, ge.Isa);
	}

	// Stop the VM with a pre-built error value (e.g. an uncaught user error).
	public void RaiseRuntimeError(Value error) {
		Error = error;
		IsRunning = false;
	}

	// Return the current call stack as a Value (frozen list of strings), or
	// Value.Null if there is no active function.  Guarded wrapper around
	// BuildStackTrace used by the stack-trace hook and value_current_stack_trace.
	public Value CurrentStackTrace() {
		if (!CurrentFunction) return Value.Null;
		return BuildStackTrace();
	}

	// Build a complete runtime error *value* (runtime __isa + an accurate stack
	// trace) without stopping the VM.  This is the non-halting counterpart to
	// RaiseRuntimeError: core value operations (e.g. value_mult) use it to return
	// an error as a value that propagates normally and only terminates the program
	// if the caller misuses it.  The stack trace is attached by ErrorTypes.RuntimeError
	// itself (via value_current_stack_trace), so this just delegates.
	public Value MakeRuntimeError(String message) {
		return ErrorTypes.RuntimeError(message);
	}

	// Called when user code silently ignored an error value and then tried
	// to use it in an operation that doesn't tolerate errors.
	// Returns IntrinsicResult.Null for convenience.
	public IntrinsicResult RaiseUncaughtError(Value error) {
		String msg = StringUtils.Format("Uncaught {0}", error_message(error));
		RaiseRuntimeError(msg);
		return IntrinsicResult.Null;
	}

	// Print the current error to stdout (useful for debug/standalone use).
	// Returns false if there is no error.
	public bool ReportRuntimeError() {
		if (Error.IsNull()) return false;
		String msg = StringUtils.Format("{0}", error_message(Error));
		String loc = "";
		Value stack = error_stack(Error);
		if (stack.IsList() && list_count(stack) > 0) {
			loc = StringUtils.Format("{0}", list_get(stack, 0));
			// Drop the "(current program) " prefix used for the top-level
			// script, leaving just "line N" for the common case.
			String prefix = "(current program) ";
			if (loc.Length >= prefix.Length && loc.Left(prefix.Length) == prefix) {
				loc = loc.Substring(prefix.Length);
			}
		}
		if (loc == "") {
			IOHelper.Print(StringUtils.Format("Runtime Error: {0}", msg));
		} else {
			IOHelper.Print(StringUtils.Format("Runtime Error: {0} [{1}]", msg, loc));
		}
		return true;
	}

	// Build and return the current call stack as a frozen list of strings,
	// innermost (most recent) frame first.  Each entry has the form
	// "{file} line {lineNum}".  PC is the saved program counter at the
	// point of the call (typically vm.PC - 1 at the call site).
	public Value BuildStackTrace() {
		Value result = make_list(8);
		Int32 callSitePC = PC - 1;
		if (callSitePC < 0) callSitePC = 0;
		String curFile = CurrentFunction.FileName;
		if (curFile == "") curFile = "(current program)";
		list_push(result, make_string(StringUtils.Format("{0} line {1}", curFile, CurrentFunction.GetLineNumber(callSitePC))));
		// callStack[0] is @main's own frame (not a caller), so stop at i=1.
		for (Int32 i = CallStackDepth() - 1; i >= 1; i--) {
			CallInfo ci = GetCallStackFrame(i);
			FuncDef callerFunc = ci.ReturnFunc;
			Int32 callerPC = ci.ReturnPC - 1;
			if (callerPC < 0) callerPC = 0;
			String callerFile = callerFunc.FileName;
			if (callerFile == "") callerFile = "(current program)";
			list_push(result, make_string(StringUtils.Format("{0} line {1}", callerFile, callerFunc.GetLineNumber(callerPC))));
		}
		freeze_value(result);
		return result;
	}

	// Collect every function reachable from @main, by walking constant pools
	// for funcref templates.  Used for disassembly and debug output.
	public List<FuncDef> GetFunctions() {
		List<FuncDef> result = new List<FuncDef>();
		CollectFunctions(CurrentFunction, result);
		return result;
	}

	private static void CollectFunctions(FuncDef func, List<FuncDef> outList) {
		if (func == null) return;
		for (Int32 i = 0; i < outList.Count; i++) {
			if (outList[i].Name == func.Name) return;  // already collected
		}
		outList.Add(func);
		List<Value> consts = func.Constants;
		for (Int32 i = 0; i < consts.Count; i++) {
			if (consts[i].IsFuncRef()) CollectFunctions(funcref_funcdef(consts[i]), outList);
		}
	}

	// Helper for argument processing (FUNCTION_CALLS.md steps 1-3):
	// Process ARG instructions, validate argument count, and set up parameter registers.
	// Returns the PC after the CALL instruction, or -1 on error.
	// Check whether pendingSelf should be injected as the first parameter.
	// Returns 1 if the callee's first param is named "self" and we have pending context, else 0.
	private Int32 SelfParamOffset(FuncDef callee) {
		if (hasPendingContext && callee.ParamNames.Count > 0 && callee.ParamNames[0] == Value.selfString) {
			return 1;
		}
		return 0;
	}

	private Int32 ProcessArguments(Int32 argCount, Int32 selfParam, Int32 startPC, Int32 callerBase, Int32 calleeBase, FuncDef callee, List<UInt32> code) {
		Int32 paramCount = callee.ParamNames.Count;

		// Step 1: Validate argument count (selfParam accounts for the injected self)
		if (argCount + selfParam > paramCount) {
			RaiseRuntimeError(StringUtils.Format("Too many arguments: got {0}, expected {1}",
							  argCount + selfParam, paramCount));
			return -1;
		}

		// Step 2-3: Process ARG instructions, copying values to parameter registers
		// Note: Parameters start at r1 (r0 is reserved for return value)
		// selfParam offsets explicit args so they go after the injected self parameter
		Int32 currentPC = startPC;
		Value argValue = Value.Null;  // Declared outside loop for GC safety
		for (Int32 i = 0; i < argCount; i++) {
			UInt32 argInstruction = code[currentPC];
			Opcode argOp = (Opcode)BytecodeUtil.OP(argInstruction);

			argValue = Value.Null;
			if (argOp == Opcode.ARG_rA) {
				// Argument from register
				Byte srcReg = BytecodeUtil.Au(argInstruction);
				argValue = stack[callerBase + srcReg];
			} else if (argOp == Opcode.ARG_iABC) {
				// Argument immediate value
				Int32 immediate = BytecodeUtil.ABCs(argInstruction);
				argValue = new Value(immediate);
			} else {
				RaiseRuntimeError("Expected ARG opcode in ARGBLK");
				return -1;
			}

			// Copy argument value to callee's parameter register and assign name
			// Parameters start at r1, so offset by 1, plus selfParam
			stack[calleeBase + 1 + selfParam + i] = argValue;
			names[calleeBase + 1 + selfParam + i] = callee.ParamNames[selfParam + i];

			currentPC++;
		}

		return currentPC + 1; // Return PC after the CALL instruction
	}

	// Apply pending self/super context to a callee's frame, if any.
	// Called after SetupCallFrame to populate the callee's self/super registers.
	private void ApplyPendingContext(Int32 calleeBase, FuncDef callee) {
		if (!hasPendingContext) return;
		if (callee.SelfReg >= 0) {
			stack[calleeBase + callee.SelfReg] = pendingSelf;
			names[calleeBase + callee.SelfReg] = Value.selfString;
		}
		if (callee.SuperReg >= 0) {
			stack[calleeBase + callee.SuperReg] = pendingSuper;
			names[calleeBase + callee.SuperReg] = Value.superString;
		}
		pendingSelf = Value.Null;
		pendingSuper = Value.Null;
		hasPendingContext = false;
	}

	// Helper for call setup (FUNCTION_CALLS.md steps 4-6):
	// Initialize remaining parameters with defaults and clear callee's registers.
	// Note: Parameters start at r1 (r0 is reserved for return value)
	// selfParam is 1 if pendingSelf was injected as the first arg, else 0.
	private void SetupCallFrame(Int32 argCount, Int32 selfParam, Int32 calleeBase, FuncDef callee) {
		Int32 paramCount = callee.ParamNames.Count;

		// Step 4: Set up remaining parameters with default values
		// Parameters start at r1, so offset by 1
		for (Int32 i = argCount + selfParam; i < paramCount; i++) {
			stack[calleeBase + 1 + i] = callee.ParamDefaults[i];
			names[calleeBase + 1 + i] = callee.ParamNames[i];
		}

		// Step 5: Clear remaining registers (r0, and any beyond parameters)
		stack[calleeBase] = Value.Null;
		names[calleeBase] = Value.Null;
		for (Int32 i = paramCount + 1; i < callee.MaxRegs; i++) {
			stack[calleeBase + i] = Value.Null;
			names[calleeBase + i] = Value.Null;
		}

		// Step 6 is handled by the caller (pushing CallInfo, switching frame, etc.)
	}

	// Auto-invoke a zero-arg funcref (used by LOADC and CALLIFREF).
	// Resolves the funcref, then either:
	//   - Native callback (done):    stores result, returns -1.
	//   - Native callback (pending): stores pending state, returns -2.
	//   - User function: pushes CallInfo and sets up callee frame, returns 0
	//     and sets calleeOut to the callee FuncDef.  Caller must switch its
	//     local execution state (pc, baseIndex, curFunc, etc.).
	// On error, calls RaiseRuntimeError and returns -1.
	private Int32 AutoInvokeFuncRef(Value funcRefVal, Int32 resultReg, Int32 returnPC, Int32 baseIndex, FuncDef currentFunc, ref FuncDef calleeOut) {
		FuncDef callee = funcref_funcdef(funcRefVal);
		if (callee == null) {
			RaiseRuntimeError("Auto-invoke: Invalid function reference");
			return -1;
		}

		Value outerVars = funcref_outer_vars(funcRefVal);

		Int32 calleeBase = baseIndex + currentFunc.MaxRegs;

		Int32 selfParam = SelfParamOffset(callee);

		// Bounds-check the callee frame BEFORE writing into it (covers both the
		// native and user-function paths below).
		if (!EnsureFrame(calleeBase, callee.MaxRegs)) return -1;

		// Native intrinsic: invoke callback directly, no frame push
		if (callee.NativeCallback != null) {
			SetupCallFrame(0, selfParam, calleeBase, callee);
			if (selfParam > 0) {
				stack[calleeBase + 1] = pendingSelf;
				names[calleeBase + 1] = Value.selfString;
				pendingSelf = Value.Null;
				hasPendingContext = false;
			}
			SaveState(returnPC, baseIndex, currentFunc);
			if (InvokeNativeCallback(callee.NativeCallback, calleeBase, selfParam, IntrinsicResult.Null, baseIndex + resultReg)) {
				return -1;  // done
			}
			return -2;  // pending
		}

		// User function: push CallInfo and set up callee frame
		if (callStackTop >= callStack.Count) {
			RaiseRuntimeError("Call stack overflow");
			return -1;
		}
		callStack[callStackTop] = new CallInfo(returnPC, baseIndex, currentFunc, resultReg, outerVars);
		callStackTop++;

		if (selfParam > 0) {
			stack[calleeBase + 1] = pendingSelf;
			names[calleeBase + 1] = Value.selfString;
		}
		SetupCallFrame(0, selfParam, calleeBase, callee);
		ApplyPendingContext(calleeBase, callee);
		calleeOut = callee;
		return 0;
	}

	// Invoke a native callback and handle the result.  If done, writes the
	// result to stack[absoluteResultIndex] and returns true.  If not done,
	// stores the pending state for re-invocation and returns false.
	private bool InvokeNativeCallback(NativeCallbackDelegate callback, Int32 calleeBase, Int32 argCount, IntrinsicResult partialResult, Int32 absoluteResultIndex) {
		Context context = new Context(
			this, // CPP: *this,
			stack,
			calleeBase,
			argCount);
		IntrinsicResult ir = callback(context, partialResult);
		stack[absoluteResultIndex] = ir.result;
		if (ir.done) return true;
		_pendingCallback = callback;
		_pendingCalleeBase = calleeBase;
		_pendingArgCount = argCount;
		_pendingResultIndex = absoluteResultIndex;
		return false;
	}

	public Value Execute(FuncDef entry) {
		return Execute(entry, 0);
	}

	public Value Execute(FuncDef entry, UInt32 maxCycles) {
		// Legacy method - convert to Reset + Run pattern
		List<FuncDef> singleFunc = new List<FuncDef>();
		singleFunc.Add(entry);
		Reset(singleFunc);
		return Run(maxCycles);
	}

	public Value Run(UInt32 maxCycles=0) {
		if (!IsRunning || !CurrentFunction) {
			return Value.Null;
		}

		// Set thread-local active VM (save/restore for nested calls)
		VM previousVM = _activeVM;
		_activeVM = this;

		// If we have a pending intrinsic continuation, handle it.
		if (_pendingCallback != null) {
			if (_hasPendingManualCall) {
				// A manually-pushed call (e.g. from the import intrinsic) is still
				// running.  Fall through to RunInner so the module code can execute.
			} else {
				// Normal case: re-invoke the pending intrinsic callback.
				IntrinsicResult partialResult = new IntrinsicResult(stack[_pendingResultIndex], false);
				if (!InvokeNativeCallback(_pendingCallback, _pendingCalleeBase, _pendingArgCount, partialResult, _pendingResultIndex)) {
					// Still not done; return without running any bytecode
					_activeVM = previousVM;
					return Value.Null;
				}
				_pendingCallback = null;
			}
		}

		Value runResult = RunInner(maxCycles);
		_activeVM = previousVM;
		return runResult;
	}

	private Value RunInner(UInt32 maxCycles) {
		// Copy instance variables to locals for performance
		Int32 pc = PC;
		Int32 baseIndex = BaseIndex;
		FuncDef currentFunc = CurrentFunction;

		// CPP: Value* stackPtr = &stack[0];
		// Note: CollectionsMarshal.AsSpan requires .NET 5+; not compatible with Mono.
		// This gives us direct array access without copying, for performance.
		
		// Variables reflecting the current call frame:
		FuncDef curFunc = null; // CPP: FuncDefStorage* curFuncRaw = nullptr;
		Int32 codeCount = 0;
		List<UInt32> curCode = null; // CPP: UInt32* curCode = nullptr;
		List<Value> curConstants = null; // CPP: Value* curConstants = nullptr;
		Span<Value> localStack = default; // CPP: Value* localStack = nullptr;
		
		SwitchFrame(currentFunc, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
		// CPP: SwitchFrame(currentFunc, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);

		UInt32 cyclesLeft = maxCycles;
		if (maxCycles == 0) cyclesLeft--;  // wraps to MAX_UINT32

		// Reusable Value variables (declared outside loop for GC safety in C++)
		// ToDo: see if we can reduce these to a more reasonable number.
		Value val;
		Value valA, valB, valC, valD;
		Value typeMap;

/*** BEGIN CPP_ONLY ***
#if VM_USE_COMPUTED_GOTO
		static void* const vm_labels[(int)Opcode::OP__COUNT] = { VM_OPCODES(VM_LABEL_LIST) };
		if (DebugMode) IOHelper::Print("(Running with computed-goto dispatch)");
#else
		if (DebugMode) IOHelper::Print("(Running with switch-based dispatch)");
#endif
*** END CPP_ONLY ***/

		while (IsRunning) {
			// CPP: VM_DISPATCH_TOP();
			if (cyclesLeft == 0) {
				SaveState(pc, baseIndex, currentFunc);
				return Value.Null;
			}
			cyclesLeft--;

			if (pc >= codeCount) {
				IsRunning = false;
				SaveState(pc, baseIndex, currentFunc);
				return Value.Null;
			}

			UInt32 instruction = curCode[pc++];

			// Keep the PC field current (one store per instruction) so that any
			// code which builds a stack trace mid-instruction -- e.g. value_mult
			// calling MakeRuntimeError -- sees an accurate location.  pc is already
			// post-incremented here, matching BuildStackTrace's `PC - 1` convention.
			// CurrentFunction and BaseIndex are kept current in SwitchFrame.
			PC = pc;

			if (DebugMode) {
				IOHelper.Print(StringUtils.Format("{0} {1}: {2}     r0:{3}, r1:{4}, r2:{5}",
					curFunc.Name, // CPP: curFuncRaw->Name,
					StringUtils.ZeroPad(pc-1, 4),
					Disassembler.ToString(instruction),
					localStack[0], localStack[1], localStack[2]));
			}

			Opcode opcode = (Opcode)BytecodeUtil.OP(instruction);
			
			switch (opcode) { // CPP: VM_DISPATCH_BEGIN();
			
				case Opcode.NOOP: {
					// No operation
					break;
				}

				case Opcode.LOAD_rA_rB: {
					// R[A] = R[B] (equivalent to MOVE)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					localStack[a] = localStack[b];
					break;
				}

				case Opcode.LOAD_rA_iBC: {
					// R[A] = BC (signed 16-bit immediate as integer)
					Byte a = BytecodeUtil.Au(instruction);
					short bc = BytecodeUtil.BCs(instruction);
					localStack[a] = new Value(bc);
					break;
				}

				case Opcode.LOAD_rA_kBC: {
					// R[A] = constants[BC]
					Byte a = BytecodeUtil.Au(instruction);
					UInt16 constIdx = BytecodeUtil.BCu(instruction);
					localStack[a] = curConstants[constIdx];
					break;
				}

				case Opcode.LOADNULL_rA: {
					// R[A] = null
					Byte a = BytecodeUtil.Au(instruction);
					localStack[a] = Value.Null;
					break;
				}

				case Opcode.LOADV_rA_rB_kC: {
					// R[A] = R[B], but verify that register B has name matching constants[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);

					// Check if the source register has the expected name
					valC = curConstants[c];  // expected name
					valB = names[baseIndex + b];  // actual name
					if (value_identical(valC, valB)) {
						localStack[a] = localStack[b];
					} else {
						// Variable not found in current scope, look in outer context
						localStack[a] = LookupVariable(valC);
					}
					break;
				}

				case Opcode.LOADC_rA_rB_kC: {
					// R[A] = R[B], but verify that register B has name matching constants[C]
					// and call the function if the value is a function reference
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);

					// Check if the source register has the expected name
					valC = curConstants[c];  // expected name
					valB = names[baseIndex + b];  // actual name
					if (value_identical(valC, valB)) {
						valB = localStack[b];
					} else {
						// Variable not found in current scope, look in outer context
						valB = LookupVariable(valC);
					}

					if (!valB.IsFuncRef()) {
						// Simple case: value is not a funcref, so just copy it
						localStack[a] = valB;
					} else {
						// Value is a funcref — auto-invoke with zero args
						FuncDef autoCallee = null;
						Int32 status = AutoInvokeFuncRef(valB, a, pc, baseIndex, currentFunc, ref autoCallee);
						if (status == -2) {
							// Native callback pending — exit RunInner
							cyclesLeft = 0;
						} else if (status == 0) {
							// Frame was pushed — switch to callee
							baseIndex += curFunc.MaxRegs; // CPP: baseIndex += curFuncRaw->MaxRegs;
							pc = 0;
							currentFunc = autoCallee;
							SwitchFrame(currentFunc, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
							// CPP: SwitchFrame(currentFunc, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
						}
					}
					break;
				}

				case Opcode.FUNCREF_iA_iBC: {
					// R[A] := a closure: the FuncDef from the template funcref at
					// constants[BC], bound with our locals as the closure context.
					Byte a = BytecodeUtil.Au(instruction);
					UInt16 constIdx = BytecodeUtil.BCu(instruction);
					FuncDef func = funcref_funcdef(curConstants[constIdx]);

					// Create function reference with our locals as the closure context
					val = GetCurrentLocalVarMap(baseIndex, curFunc.MaxRegs); // CPP: val = GetCurrentLocalVarMap(baseIndex, curFuncRaw->MaxRegs);
					localStack[a] = make_funcref(func, val);
					break;
				}

				case Opcode.ASSIGN_rA_rB_kC: {
					// R[A] = R[B] and names[baseIndex + A] = constants[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					localStack[a] = localStack[b];
					valC = curConstants[c];
					names[baseIndex + a] = valC;
					// In REPL mode, register this variable in the globals VarMap
					if (baseIndex == 0 && !ReplGlobals.IsNull()) {
						varmap_map_to_register(ReplGlobals, valC, 
							stack,
							baseIndex + a);
					}
					break;
				}

				case Opcode.NAME_rA_kBC: {
					// names[baseIndex + A] = constants[BC] (without changing R[A])
					Byte a = BytecodeUtil.Au(instruction);
					UInt16 constIdx = BytecodeUtil.BCu(instruction);
					valC = curConstants[constIdx];
					names[baseIndex + a] = valC;
					// Keep any live VarMap for this scope in sync with the new
					// variable.  In REPL mode at the top level that is ReplGlobals;
					// otherwise it is the current frame's LocalVarMap, if one has
					// already been created (e.g. by a FUNCREF closure capture or a
					// `locals` reference earlier in the function).  Without this,
					// variables declared after the first closure/`locals` use would
					// be missing from the locals map.
					if (baseIndex == 0 && !ReplGlobals.IsNull()) {
						varmap_map_to_register(ReplGlobals, valC,
							stack,
							baseIndex + a);
					} else {
						CallInfo nameFrame = callStack[callStackTop - 1];
						if (!nameFrame.LocalVarMap.IsNull()) {
							varmap_map_to_register(nameFrame.LocalVarMap, valC,
								stack,
								baseIndex + a);
						}
					}
					break;
				}

				case Opcode.ADD_rA_rB_rC: {
					// R[A] = R[B] + R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					localStack[a] = value_add(localStack[b], localStack[c], this);
					break;
				}

				case Opcode.SUB_rA_rB_rC: {
					// R[A] = R[B] - R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					localStack[a] = value_sub(localStack[b], localStack[c]);
					break;
				}

				case Opcode.MUL_rA_rB_rC: {
					// R[A] = R[B] * R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					localStack[a] = value_mult(localStack[b], localStack[c]);
					break;
				}

				case Opcode.DIV_rA_rB_rC: {
					// R[A] = R[B] * R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					localStack[a] = value_div(localStack[b], localStack[c]);
					break;
				}

				case Opcode.MOD_rA_rB_rC: {
					// R[A] = R[B] % R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					localStack[a] = value_mod(localStack[b], localStack[c]);
					break;
				}

				case Opcode.POW_rA_rB_rC: { // CPP: VM_CASE(POW_rA_rB_rC) {
					// R[A] = R[B] ^ R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					localStack[a] = value_pow(localStack[b], localStack[c]);
					break; // CPP: VM_NEXT();
				}

				case Opcode.AND_rA_rB_rC: { // CPP: VM_CASE(AND_rA_rB_rC) {
					// R[A] = R[B] and R[C] (fuzzy logic: AbsClamp01(a * b))
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					localStack[a] = value_and(localStack[b], localStack[c]);
					break; // CPP: VM_NEXT();
				}

				case Opcode.OR_rA_rB_rC: { // CPP: VM_CASE(OR_rA_rB_rC) {
					// R[A] = R[B] or R[C] (fuzzy logic: AbsClamp01(a + b - a*b))
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					localStack[a] = value_or(localStack[b], localStack[c]);
					break; // CPP: VM_NEXT();
				}

				case Opcode.NOT_rA_rB: { // CPP: VM_CASE(NOT_rA_rB) {
					// R[A] = not R[B] (fuzzy logic: 1 - AbsClamp01(b))
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					localStack[a] = value_not(localStack[b]);
					break; // CPP: VM_NEXT();
				}

				case Opcode.LIST_rA_iBC: {
					// R[A] = make_list(BC)
					Byte a = BytecodeUtil.Au(instruction);
					Int16 capacity = BytecodeUtil.BCs(instruction);
					localStack[a] = make_list(capacity);
					break;
				}

				case Opcode.MAP_rA_iBC: {
					// R[A] = make_map(BC)
					Byte a = BytecodeUtil.Au(instruction);
					Int16 capacity = BytecodeUtil.BCs(instruction);
					localStack[a] = make_map(capacity);
					break;
				}

				case Opcode.PUSH_rA_rB: {
					// list_push(R[A], R[B])
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					list_push(localStack[a], localStack[b]);
					break;
				}

				case Opcode.INDEX_rA_rB_rC: {
					// R[A] = R[B][R[C]] (supports lists, maps, and strings)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					valB = localStack[b];  // container
					valC = localStack[c];  // index

					if (valB.IsError()) {
						// Address-of on an error is always an lvalue operation; terminate.
						RaiseRuntimeError("Cannot take reference into an error value");
						localStack[a] = Value.Null;
						break;
					}
					if (valB.IsList()) {
						// ToDo: add a list_try_get and use it here, like we do with map below
						localStack[a] = list_get(valB, as_int(valC));
					} else if (valB.IsMap()) {
						if (!map_lookup(valB, valC, out val)) {
							RaiseRuntimeError(StringUtils.Format("Key Not Found: '{0}' not found in map", valC));
						}
						localStack[a] = val;
					} else if (valB.IsString()) {
						Int32 idx = as_int(valC);
						localStack[a] = string_substring(valB, idx, 1);
					} else {
						RaiseRuntimeError(StringUtils.Format("Can't index into {0}", valB));
						localStack[a] = Value.Null;
					}
					break;
				}

				case Opcode.IDXSET_rA_rB_rC: {
					// R[A][R[B]] = R[C] (supports both lists and maps)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					valA = localStack[a];  // container
					valB = localStack[b];  // index
					valC = localStack[c];  // value

					if (valA.IsError()) {
						RaiseRuntimeError("Cannot assign into an error value");
						break;
					}
					if (valA.IsList()) {
						list_set(valA, as_int(valB), valC);
					} else if (valA.IsMap()) {
						map_set(valA, valB, valC);
					} else {
						RaiseRuntimeError(StringUtils.Format("Can't set indexed value in {0}", valA));
					}
					break;
				}

				case Opcode.SLICE_rA_rB_rC: {
					// R[A] = R[B][R[C]:R[C+1]] (slice; end index in adjacent register)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					valB = localStack[b];
					valC = localStack[c];
					valD = localStack[c + 1];

					if (valB.IsError()) { RaiseUncaughtError(valB); localStack[a] = Value.Null; break; }
					if (valC.IsError()) { RaiseUncaughtError(valC); localStack[a] = Value.Null; break; }
					if (valD.IsError()) { RaiseUncaughtError(valD); localStack[a] = Value.Null; break; }

					if (valB.IsString()) {
						Int32 len = string_length(valB);
						Int32 startIdx = valC.IsNull() ? 0 : as_int(valC);
						Int32 endIdx = valD.IsNull() ? len : as_int(valD);
						localStack[a] = string_slice(valB, startIdx, endIdx);
					} else if (valB.IsList()) {
						Int32 len = list_count(valB);
						Int32 startIdx = valC.IsNull() ? 0 : as_int(valC);
						Int32 endIdx = valD.IsNull() ? len : as_int(valD);
						localStack[a] = list_slice(valB, startIdx, endIdx);
					} else {
						RaiseRuntimeError(StringUtils.Format("Can't slice {0}", valB));
						localStack[a] = Value.Null;
					}
					break;
				}

				case Opcode.LOCALS_rA: {
					// Create VarMap for local variables and store in R[A].
					// Temporarily clear this register's name while building the VarMap, so that
					// the destination register does not appear as a self-referential entry (e.g.,
					// "locals" pointing to the VarMap itself).  Restore afterward so that if the
					// user wrote `myLocals = locals`, the name "myLocals" remains findable.
					Byte a = BytecodeUtil.Au(instruction);
					val = names[baseIndex + a];  // saved name
					names[baseIndex + a] = Value.Null;   // temporarily clear it
					localStack[a] = GetCurrentLocalVarMap(baseIndex, curFunc.MaxRegs); // CPP: localStack[a] = GetCurrentLocalVarMap(baseIndex, curFuncRaw->MaxRegs);
					names[baseIndex + a] = val;  // restore name
					break;
				}

				case Opcode.OUTER_rA: {
					// Return VarMap for outer scope; fall back to globals if none.
					// Same temporary-clear pattern as LOCALS_rA (see above).
					Byte a = BytecodeUtil.Au(instruction);
					val = names[baseIndex + a];  // saved name
					names[baseIndex + a] = Value.Null;   // temporarily clear it
					if (callStackTop > 0 && !callStack[callStackTop - 1].OuterVarMap.IsNull()) {
						localStack[a] = callStack[callStackTop - 1].OuterVarMap;
					} else {
						// No enclosing scope or at global scope: outer == globals
						localStack[a] = GetGlobalsVarMap();
					}
					names[baseIndex + a] = val;   // restore name
					break;
				}

				case Opcode.GLOBALS_rA: {
					// Return VarMap for global variables.
					// Same temporary-clear pattern as LOCALS_rA (see above).
					Byte a = BytecodeUtil.Au(instruction);
					val = names[baseIndex + a];  // saved name
					names[baseIndex + a] = Value.Null;   // temporarily clear it
					localStack[a] = GetGlobalsVarMap();
					names[baseIndex + a] = val;  // restore name
					break;
				}

				case Opcode.JUMP_iABC: {
					// Jump by signed 24-bit ABC offset from current PC
					Int32 offset = BytecodeUtil.ABCs(instruction);
					pc += offset;
					break;
				}

				case Opcode.LT_rA_rB_rC: {
					// if R[A] = R[B] < R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					if (localStack[b].IsError()) { localStack[a] = localStack[b]; break; }
					if (localStack[c].IsError()) { localStack[a] = localStack[c]; break; }
					localStack[a] = Truth(value_lt(localStack[b], localStack[c]));
					break;
				}

				case Opcode.LT_rA_rB_iC: {
					// if R[A] = R[B] < C (immediate)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte c = BytecodeUtil.Cs(instruction);
					if (localStack[b].IsError()) { localStack[a] = localStack[b]; break; }
					localStack[a] = Truth(value_lt(localStack[b], new Value(c)));
					break;
				}

				case Opcode.LT_rA_iB_rC: {
					// if R[A] = B (immediate) < R[C]
					Byte a = BytecodeUtil.Au(instruction);
					SByte b = BytecodeUtil.Bs(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					if (localStack[c].IsError()) { localStack[a] = localStack[c]; break; }
					localStack[a] = Truth(value_lt(new Value(b), localStack[c]));
					break;
				}

				case Opcode.LE_rA_rB_rC: {
					// if R[A] = R[B] <= R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					if (localStack[b].IsError()) { localStack[a] = localStack[b]; break; }
					if (localStack[c].IsError()) { localStack[a] = localStack[c]; break; }
					localStack[a] = Truth(value_le(localStack[b], localStack[c]));
					break;
				}

				case Opcode.LE_rA_rB_iC: {
					// if R[A] = R[B] <= C (immediate)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte c = BytecodeUtil.Cs(instruction);
					if (localStack[b].IsError()) { localStack[a] = localStack[b]; break; }
					localStack[a] = Truth(value_le(localStack[b], new Value(c)));
					break;
				}

				case Opcode.LE_rA_iB_rC: {
					// if R[A] = B (immediate) <= R[C]
					Byte a = BytecodeUtil.Au(instruction);
					SByte b = BytecodeUtil.Bs(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					if (localStack[c].IsError()) { localStack[a] = localStack[c]; break; }
					localStack[a] = Truth(value_le(new Value(b), localStack[c]));
					break;
				}

				case Opcode.EQ_rA_rB_rC: {
					// if R[A] = R[B] == R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);

					localStack[a] = Truth(localStack[b] == localStack[c]);
					break;
				}

				case Opcode.EQ_rA_rB_iC: {
					// if R[A] = R[B] == C (immediate)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte c = BytecodeUtil.Cs(instruction);
					
					localStack[a] = Truth(localStack[b] == new Value(c));
					break;
				}

				case Opcode.NE_rA_rB_rC: {
					// if R[A] = R[B] != R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);

					localStack[a] = Truth(localStack[b] != localStack[c]);
					break;
				}

				case Opcode.NE_rA_rB_iC: {
					// if R[A] = R[B] != C (immediate)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte c = BytecodeUtil.Cs(instruction);
					
					localStack[a] = Truth(localStack[b] != new Value(c));
					break;
				}

				case Opcode.BRTRUE_rA_iBC: {
					Byte a = BytecodeUtil.Au(instruction);
					Int32 offset = BytecodeUtil.BCs(instruction);
					if (localStack[a].IsError()) {
						RaiseRuntimeError(StringUtils.Format("Error used in conditional: {0}", localStack[a]));
						break;
					}
					if (localStack[a].BoolValue()){
						pc += offset;
					}
					break;
				}

				case Opcode.BRFALSE_rA_iBC: {
					Byte a = BytecodeUtil.Au(instruction);
					Int32 offset = BytecodeUtil.BCs(instruction);
					if (localStack[a].IsError()) {
						RaiseRuntimeError(StringUtils.Format("Error used in conditional: {0}", localStack[a]));
						break;
					}
					if (!localStack[a].BoolValue()){
						pc += offset;
					}
					break;
				}

				case Opcode.BRERR_rA_iBC: {
					// if R[A] is an error then jump offset BC (no throw).
					// Used for short-circuit 'and'/'or' to peel off the error
					// case before the (throwing) BRTRUE/BRFALSE test.
					Byte a = BytecodeUtil.Au(instruction);
					Int32 offset = BytecodeUtil.BCs(instruction);
					if (localStack[a].IsError()) {
						pc += offset;
					}
					break;
				}

				case Opcode.BRLT_rA_rB_iC: {
					// if R[A] < R[B] then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (localStack[a].IsError() || localStack[b].IsError()) {
						RaiseRuntimeError("Error used in conditional");
						break;
					}
					if (value_lt(localStack[a], localStack[b])){
						pc += offset;
					}
					break;
				}

				case Opcode.BRLT_rA_iB_iC: {
					// if R[A] < B (immediate) then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					SByte b = BytecodeUtil.Bs(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (localStack[a].IsError()) {
						RaiseRuntimeError("Error used in conditional");
						break;
					}
					if (value_lt(localStack[a], new Value(b))){
						pc += offset;
					}
					break;
				}

				case Opcode.BRLT_iA_rB_iC: {
					// if A (immediate) < R[B] then jump offset C.
					SByte a = BytecodeUtil.As(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (localStack[b].IsError()) {
						RaiseRuntimeError("Error used in conditional");
						break;
					}
					if (value_lt(new Value(a), localStack[b])){
						pc += offset;
					}
					break;
				}

				case Opcode.BRLE_rA_rB_iC: {
					// if R[A] <= R[B] then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (localStack[a].IsError() || localStack[b].IsError()) {
						RaiseRuntimeError("Error used in conditional");
						break;
					}
					if (value_le(localStack[a], localStack[b])){
						pc += offset;
					}
					break;
				}

				case Opcode.BRLE_rA_iB_iC: {
					// if R[A] <= B (immediate) then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					SByte b = BytecodeUtil.Bs(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (localStack[a].IsError()) {
						RaiseRuntimeError("Error used in conditional");
						break;
					}
					if (value_le(localStack[a], new Value(b))){
						pc += offset;
					}
					break;
				}

				case Opcode.BRLE_iA_rB_iC: {
					// if A (immediate) <= R[B] then jump offset C.
					SByte a = BytecodeUtil.As(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (localStack[b].IsError()) {
						RaiseRuntimeError("Error used in conditional");
						break;
					}
					if (value_le(new Value(a), localStack[b])){
						pc += offset;
					}
					break;
				}

				case Opcode.BREQ_rA_rB_iC: {
					// if R[A] == R[B] then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (localStack[a] == localStack[b]){
						pc += offset;
					}
					break;
				}

				case Opcode.BREQ_rA_iB_iC: {
					// if R[A] == B (immediate) then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					SByte b = BytecodeUtil.Bs(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (localStack[a] == new Value(b)){
						pc += offset;
					}
					break;
				}

				case Opcode.BRNE_rA_rB_iC: {
					// if R[A] != R[B] then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (localStack[a] != localStack[b]){
						pc += offset;
					}
					break;
				}

				case Opcode.BRNE_rA_iB_iC: {
					// if R[A] != B (immediate) then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					SByte b = BytecodeUtil.Bs(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (localStack[a] != new Value(b)){
						pc += offset;
					}
					break;
				}

				case Opcode.IFLT_rA_rB: {
					// if R[A] < R[B] is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					if (localStack[a].IsError() || localStack[b].IsError()) {
						RaiseRuntimeError("Error used in conditional"); break;
					}
					if (!value_lt(localStack[a], localStack[b])) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFLT_rA_iBC: {
					// if R[A] < BC (immediate) is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					short bc = BytecodeUtil.BCs(instruction);
					if (localStack[a].IsError()) { RaiseRuntimeError("Error used in conditional"); break; }
					if (!value_lt(localStack[a], new Value(bc))) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFLT_iAB_rC: {
					// if AB (immediate) < R[C] is false, skip next instruction
					short ab = BytecodeUtil.ABs(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					if (localStack[c].IsError()) { RaiseRuntimeError("Error used in conditional"); break; }
					if (!value_lt(new Value(ab), localStack[c])) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFLE_rA_rB: {
					// if R[A] <= R[B] is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					if (localStack[a].IsError() || localStack[b].IsError()) {
						RaiseRuntimeError("Error used in conditional"); break;
					}
					if (!value_le(localStack[a], localStack[b])) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFLE_rA_iBC: {
					// if R[A] <= BC (immediate) is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					short bc = BytecodeUtil.BCs(instruction);
					if (localStack[a].IsError()) { RaiseRuntimeError("Error used in conditional"); break; }
					if (!value_le(localStack[a], new Value(bc))) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFLE_iAB_rC: {
					// if AB (immediate) <= R[C] is false, skip next instruction
					short ab = BytecodeUtil.ABs(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					if (localStack[c].IsError()) { RaiseRuntimeError("Error used in conditional"); break; }
					if (!value_le(new Value(ab), localStack[c])) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFEQ_rA_rB: {
					// if R[A] == R[B] is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					if (localStack[a] != localStack[b]) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFEQ_rA_iBC: {
					// if R[A] == BC (immediate) is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					short bc = BytecodeUtil.BCs(instruction);
					if (localStack[a] != new Value(bc)) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFNE_rA_rB: {
					// if R[A] != R[B] is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					if (localStack[a] == localStack[b]) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFNE_rA_iBC: {
					// if R[A] != BC (immediate) is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					short bc = BytecodeUtil.BCs(instruction);
					if (localStack[a] == new Value(bc)) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.NEXT_rA_rB: {
					// Advance iterator R[A] to next entry in collection R[B].
					// If there is a next entry, skip next instruction (the JUMP to end).
					// Iterator values are opaque: for lists/strings they are sequential
					// indices; for maps they encode position via map_iter_next.
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Int32 iter = as_int(localStack[a]);
					valB = localStack[b];  // collection
					bool hasMore;
					if (valB.IsList()) {
						iter++;
						hasMore = (iter < list_count(valB));
					} else if (valB.IsMap()) {
						iter = map_iter_next(valB, iter);
						hasMore = (iter != MAP_ITER_DONE);
					} else if (valB.IsString()) {
						iter++;
						hasMore = (iter < string_length(valB));
					} else {
						hasMore = false;
					}
					localStack[a] = new Value(iter);
					if (hasMore) {
						pc++; // Skip next instruction (the JUMP to end of loop)
					}
					break;
				}

				case Opcode.ARGBLK_iABC: {
					// Begin argument block with specified count
					// ABC: number of ARG instructions that follow
					Int32 argCount = BytecodeUtil.ABCs(instruction);

					// Look ahead to find the CALL instruction (argCount instructions ahead)
					Int32 callPC = pc + argCount;
					if (callPC >= codeCount) {
						RaiseRuntimeError("ARGBLK: CALL instruction out of range");
						return Value.Null;
					}
					UInt32 callInstruction = curCode[callPC];
					Opcode callOp = (Opcode)BytecodeUtil.OP(callInstruction);
					if (callOp != Opcode.CALL_rA_rB_rC) {
						RaiseRuntimeError("ARGBLK must be followed by CALL");
						return Value.Null;
					}

					// CALL r[A], r[B], r[C]: invoke funcref in r[C], frame at r[B], result to r[A]
					Byte a = BytecodeUtil.Au(callInstruction);
					Byte b = BytecodeUtil.Bu(callInstruction);
					Byte c = BytecodeUtil.Cu(callInstruction);

					valC = localStack[c];  // func ref
					if (!valC.IsFuncRef()) {
						RaiseRuntimeError("ARGBLK/CALL: Not a function reference");
						return Value.Null;
					}

					FuncDef callee = funcref_funcdef(valC);
					if (callee == null) {
						RaiseRuntimeError("ARGBLK/CALL: Invalid function reference");
						return Value.Null;
					}
					Int32 calleeBase = baseIndex + b;
					Int32 resultReg = a;

					// Check for self-injection: if pending context and first param is "self",
					// inject pendingSelf as the first argument
					Int32 selfParam = SelfParamOffset(callee);
					// Bounds-check the callee frame BEFORE writing any arguments or
					// defaults into it (otherwise deep recursion writes out of range).
					if (!EnsureFrame(calleeBase, callee.MaxRegs)) return Value.Null;
					Int32 nextPC = ProcessArguments(argCount, selfParam, pc, baseIndex, calleeBase, callee,
					  curFunc.Code); // CPP: curFuncRaw->Code);
					if (nextPC < 0) return Value.Null; // Error already raised
					if (selfParam > 0) {
						stack[calleeBase + 1] = pendingSelf;
						names[calleeBase + 1] = Value.selfString;
					}

					// Set up call frame using helper
					SetupCallFrame(argCount, selfParam, calleeBase, callee);
					ApplyPendingContext(calleeBase, callee);

					// Native intrinsic: invoke callback directly, no frame push
					if (callee.NativeCallback != null) {
						pc = nextPC;
						SaveState(pc, baseIndex, currentFunc);
						if (!InvokeNativeCallback(callee.NativeCallback, calleeBase, argCount + selfParam, IntrinsicResult.Null, baseIndex + resultReg)) {
							if (_hasPendingManualCall) {
								// The intrinsic pushed a manual call (e.g. import).  Resync
								// local state from the instance variables ManuallyPushCall set,
								// and continue execution — the module code runs inline.
								pc = PC;
								baseIndex = BaseIndex;
								currentFunc = CurrentFunction;
								SwitchFrame(currentFunc, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
								// CPP: SwitchFrame(currentFunc, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
							} else {
								cyclesLeft = 0;
							}
						}
						break;
					}

					// Now execute the CALL (step 6): push CallInfo and switch to callee
					if (callStackTop >= callStack.Count) {
						RaiseRuntimeError("Call stack overflow");
						break;
					}

					val = funcref_outer_vars(valC);
					callStack[callStackTop] = new CallInfo(nextPC, baseIndex, currentFunc, resultReg, val);
					callStackTop++;

					baseIndex = calleeBase;
					currentFunc = callee; // Switch to callee function
					pc = 0; // Start at beginning of callee code
					SwitchFrame(currentFunc, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
					// CPP: SwitchFrame(currentFunc, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
					break;
				}

				case Opcode.ARG_rA: {
					// The VM should never encounter this opcode on its own; it will
					// be processed as part of the ARGBLK opcode.  So if we get
					// here, it's an error.
					RaiseRuntimeError("Internal error: ARG without ARGBLK");
					break;
				}

				case Opcode.ARG_iABC: {
					// The VM should never encounter this opcode on its own; it will
					// be processed as part of the ARGBLK opcode.  So if we get
					// here, it's an error.
					RaiseRuntimeError("Internal error: ARG without ARGBLK");
					break;
				}

				case Opcode.CALLF_iA_iBC: {
					// A: arg window start (callee executes with base = base + A)
					// BC: constant-pool index of a template funcref for the callee
					Byte a = BytecodeUtil.Au(instruction);
					UInt16 constIdx = BytecodeUtil.BCu(instruction);

					FuncDef callee = funcref_funcdef(curConstants[constIdx]);
					if (callee == null) {
						RaiseRuntimeError("CALLF: Invalid function reference");
						break;
					}

					// Push return info
					if (callStackTop >= callStack.Count) {
						RaiseRuntimeError("Call stack overflow");
						break;
					}
					callStack[callStackTop] = new CallInfo(pc, baseIndex, currentFunc);
					callStackTop++;

					// Switch to callee frame: base slides to argument window
					baseIndex += a;
					// Note: ApplyPendingContext skipped for CALLF (only needed for method dispatch via CALL)
					pc = 0; // Start at beginning of callee code
					currentFunc = callee; // Switch to callee function
					SwitchFrame(currentFunc, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
					// CPP: SwitchFrame(currentFunc, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);

					// No frame writes happen before the next opcode, so just verify
					// (it raises and halts on overflow) and fall through to break.
					EnsureFrame(baseIndex, callee.MaxRegs);
					break;
				}

				case Opcode.CALL_rA_rB_rC: {
					// Invoke the FuncRef in R[C], with a stack frame starting at our register B,
					// and upon return, copy the result from r[B] to r[A].
					//
					// A: destination register for result
					// B: stack frame start register for callee
					// C: register containing FuncRef to call
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);

					valC = localStack[c];  // func reference
					if (!valC.IsFuncRef()) {
						RaiseRuntimeError("CALL: Value in register is not a function reference");
						break;
					}

					FuncDef callee = funcref_funcdef(valC);
					if (callee == null) {
						RaiseRuntimeError("CALL: Invalid function reference");
						break;
					}
					valD = funcref_outer_vars(valC);  // valD: "outer" VarMap of func valC

					// For naked CALL (without ARGBLK): set up parameters with defaults
					Int32 calleeBase = baseIndex + b;
					Int32 selfParam = SelfParamOffset(callee);
					// Bounds-check the callee frame BEFORE writing into it.
					if (!EnsureFrame(calleeBase, callee.MaxRegs)) break;
					SetupCallFrame(0, selfParam, calleeBase, callee);
					if (selfParam > 0) {
						stack[calleeBase + 1] = pendingSelf;
						names[calleeBase + 1] = Value.selfString;
					}
					ApplyPendingContext(calleeBase, callee);

					// Native intrinsic: invoke callback directly, no frame push
					if (callee.NativeCallback != null) {
						SaveState(pc, baseIndex, currentFunc);
						if (!InvokeNativeCallback(callee.NativeCallback, calleeBase, selfParam, IntrinsicResult.Null, baseIndex + a)) {
							if (_hasPendingManualCall) {
								pc = PC;
								baseIndex = BaseIndex;
								currentFunc = CurrentFunction;
								SwitchFrame(currentFunc, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
								// CPP: SwitchFrame(currentFunc, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
							} else {
								cyclesLeft = 0;
							}
						}
						break;
					}

					if (callStackTop >= callStack.Count) {
						RaiseRuntimeError("Call stack overflow");
						break;
					}
					callStack[callStackTop] = new CallInfo(pc, baseIndex, currentFunc, a, valD);
					callStackTop++;

					// Set up call frame starting at baseIndex + b
					baseIndex = calleeBase;
					pc = 0; // Start at beginning of callee code
					currentFunc = callee; // Switch to callee function
					SwitchFrame(currentFunc, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
					// CPP: SwitchFrame(currentFunc, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
					break;
				}

				case Opcode.NEW_rA_rB: {
					// R[A] = new map with __isa set to R[B]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					val = make_map(2);
					map_set(val, Value.magicIsA, localStack[b]);
					localStack[a] = val;
					break;
				}

				case Opcode.ISA_rA_rB_rC: { // CPP: VM_CASE(ISA_rA_rB_rC) {
					// R[A] = (R[B] isa R[C])
					// True if:
					//   1. both are null
					//   2. R[C] is a built-in type map matching the type of R[B]
					//   3. R[B] and R[C] are the same map reference, or R[C]
					//      appears anywhere in R[B]'s __isa chain
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					valB = localStack[b];  // left-hand side
					valC = localStack[c];  // right-hand side
					Int32 isaResult = 0;
					if (valB.IsNull() && valC.IsNull()) {
						isaResult = 1;
					} else if (value_identical(valB, valC)) {
						isaResult = 1;
					} else if (valB.IsError()) {
						// Error-specific isa rules:
						//   e isa error   -> 1
						//   e1 isa e2     -> 1 if e2 is in e1's __isa chain
						if (value_identical(valC, CoreIntrinsics.ErrorType())) {
							isaResult = 1;
						} else if (valC.IsError() && error_isa_contains(valB, valC)) {
							isaResult = 1;
						}
						localStack[a] = Truth(isaResult);
						break;
					} else if (valC.IsMap()) {
						// Walk valB's __isa chain looking for valC
						if (valB.IsMap()) {
							val = valB;  // val is "current"; valA (below) is "next" in the __isa chain
							for (Int32 depth = 0; depth < 256; depth++) {
								if (!map_try_get(val, Value.magicIsA, out valA)) break;
								if (value_identical(valA, valC)) {
									isaResult = 1;
									break;
								}
								val = valA;
							}
						}
						// If not found via __isa chain, check built-in type maps
						if (isaResult == 0) {
							if (valB.IsNumber() && value_identical(valC, CoreIntrinsics.NumberType())) {
								isaResult = 1;
							} else if (valB.IsString() && value_identical(valC, CoreIntrinsics.StringType())) {
								isaResult = 1;
							} else if (valB.IsList() && value_identical(valC, CoreIntrinsics.ListType())) {
								isaResult = 1;
							} else if (valB.IsMap() && value_identical(valC, CoreIntrinsics.MapType())) {
								isaResult = 1;
							} else if (valB.IsFuncRef() && value_identical(valC, CoreIntrinsics.FunctionType())) {
								isaResult = 1;
							}
						}
					}
					localStack[a] = Truth(isaResult);
					break; // CPP: VM_NEXT();
				}

				case Opcode.METHFIND_rA_rB_rC: {
					// Method lookup: walk __isa chain of R[B] looking for key R[C]
					// R[A] = the value found
					// Side effects: pendingSelf = R[B], pendingSuper = containing map's __isa
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					valB = localStack[b];  // container
					valC = localStack[c];  // index
					typeMap = Value.Null;
					
					if (valB.IsError()) {
						// Error field access: return field directly for reserved names,
						// else fall back to ErrorType() for method lookup (e.g., e.err).
						// Any other key terminates per language spec.
						if (valC.IsString()) {
							String keyStr = as_cstring(valC);
							if (keyStr == "message") { localStack[a] = error_message(valB); hasPendingContext = false; break; }
							if (keyStr == "inner")   { localStack[a] = error_inner(valB);   hasPendingContext = false; break; }
							if (keyStr == "stack")   { localStack[a] = error_stack(valB);   hasPendingContext = false; break; }
							if (keyStr == "__isa")   { localStack[a] = error_isa(valB);     hasPendingContext = false; break; }
						}
						typeMap = CoreIntrinsics.ErrorType();
						if (map_try_get(typeMap, valC, out val)) {
							localStack[a] = val;
							pendingSelf = valB;
							pendingSuper = Value.Null;
							hasPendingContext = true;
							break;
						}
						// Wrap the error as inner in a new termination error
						RaiseRuntimeError(StringUtils.Format("Undefined error field '{0}'", valC));
						localStack[a] = Value.Null;
						break;
					}
					if (valB.IsMap()) {
						// For maps: first do lookup in the map itself, with inheritance
						// (valD: the "super" value, i.e., __isa of the map in which valC
						// was actually found.)
						if (map_lookup_with_origin(valB, valC, out val, out valD)) {
							localStack[a] = val;
							pendingSelf = valB;
							pendingSuper = valD;
							hasPendingContext = true;
							break; // CPP: VM_NEXT();
						}
						// ...falling back on the map type map
						typeMap = CoreIntrinsics.MapType();
					} else if (valB.IsList()) {
						typeMap = CoreIntrinsics.ListType();
					} else if (valB.IsString()) {
						typeMap = CoreIntrinsics.StringType();
					} else if (valB.IsNumber()) {
						typeMap = CoreIntrinsics.NumberType();
					}
					if (typeMap.IsNull()) {
						// If we didn't get a type map, then user is trying to index
						// into something not indexable
						RaiseRuntimeError(StringUtils.Format("Can't index into {0}", valB));
						localStack[a] = Value.Null;
					} else if (map_try_get(typeMap, valC, out val)) {
						// found what we're looking for in the type map
						localStack[a] = val;
						pendingSelf = valB;
						pendingSuper = Value.Null;
					} else if (valC.IsNumber()) {
						// try indexing numerically
						int index = as_int(valC);
						if (valB.IsList()) {
							localStack[a] = list_get(valB, index);
						} else if (valB.IsString()) {
							localStack[a] = string_substring(valB, index, 1);
						} else {
							RaiseRuntimeError(StringUtils.Format("Can't index into {0}", valB));
							localStack[a] = Value.Null;
						}
					} else {
						RaiseRuntimeError(StringUtils.Format("Key Not Found: '{0}' not found in map", valC));
						localStack[a] = Value.Null;
					}
					hasPendingContext = true;
					break; // CPP: VM_NEXT();
				}

				case Opcode.IDXGET_rA_rB_rC: {
					// Bracket-index lookup: R[A] = R[B][R[C]], with type-map fallback,
					// but never auto-invokes a funcRef result.
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					valB = localStack[b];  // container
					valC = localStack[c];  // index
					typeMap = Value.Null;

					if (valB.IsMap()) {
						if (map_lookup_with_origin(valB, valC, out val, out valD)) {
							localStack[a] = val;
							hasPendingContext = false;
							break; // CPP: VM_NEXT();
						}
						typeMap = CoreIntrinsics.MapType();
					} else if (valB.IsList()) {
						typeMap = CoreIntrinsics.ListType();
					} else if (valB.IsString()) {
						typeMap = CoreIntrinsics.StringType();
					} else if (valB.IsNumber()) {
						typeMap = CoreIntrinsics.NumberType();
					}
					if (typeMap.IsNull()) {
						RaiseRuntimeError(StringUtils.Format("Can't index into {0}", valB));
						localStack[a] = Value.Null;
					} else if (map_try_get(typeMap, valC, out val)) {
						localStack[a] = val;
					} else if (valC.IsNumber()) {
						int index = as_int(valC);
						if (valB.IsList()) {
							localStack[a] = list_get(valB, index);
						} else if (valB.IsString()) {
							localStack[a] = string_substring(valB, index, 1);
						} else {
							RaiseRuntimeError(StringUtils.Format("Can't index into {0}", valB));
							localStack[a] = Value.Null;
						}
					} else {
						RaiseRuntimeError(StringUtils.Format("Key Not Found: '{0}' not found in map", valC));
						localStack[a] = Value.Null;
					}
					hasPendingContext = false;
					break; // CPP: VM_NEXT();
				}

				case Opcode.SETSELF_rA: {
					// Override pendingSelf with R[A] (used for super.method() calls)
					Byte a = BytecodeUtil.Au(instruction);
					pendingSelf = localStack[a];
					hasPendingContext = true;
					break; // CPP: VM_NEXT();
				}

				case Opcode.CALLIFREF_rA: {
					// If R[A] is a funcref and we have pending context, auto-invoke it
					// (like LOADC does for variable references). If not a funcref,
					// just clear pending context.
					Byte a = BytecodeUtil.Au(instruction);
					val = localStack[a];

					if (!val.IsFuncRef() || !hasPendingContext) {
						// Not a funcref or no context — clear pending state and leave value as-is
						hasPendingContext = false;
						pendingSelf = Value.Null;
						pendingSuper = Value.Null;
						break; // CPP: VM_NEXT();
					}

					// Auto-invoke the funcref with zero args
					FuncDef autoCallee = null;
					Int32 status = AutoInvokeFuncRef(val, a, pc, baseIndex, currentFunc, ref autoCallee);
					if (status == -2) {
						// Native callback pending — exit RunInner
						cyclesLeft = 0;
					} else if (status == 0) {
						// Frame was pushed — switch to callee
						baseIndex += curFunc.MaxRegs; // CPP: baseIndex += curFuncRaw->MaxRegs;
						pc = 0;
						currentFunc = autoCallee;
						SwitchFrame(currentFunc, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
						// CPP: SwitchFrame(currentFunc, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
					}
					break; // CPP: VM_NEXT();
				}

				case Opcode.RETURN: {
					// Return value convention: value is in base[0]
					val = stack[baseIndex];

					// Gather the current execution-context frame's locals VarMap if one exists
					// (makes closure values survive beyond the function's lifetime).
					CallInfo frame = callStack[callStackTop - 1];
					if (!frame.LocalVarMap.IsNull()) {
						varmap_gather(frame.LocalVarMap);
						frame.LocalVarMap = Value.Null;
						callStack[callStackTop - 1] = frame;  // write back (CallInfo is a struct)
					}

					// Pop the current execution-context frame.
					callStackTop--;
					if (callStackTop == 0) {
						// We just popped @main's frame — execution is complete.
						SaveState(pc, baseIndex, currentFunc);
						IsRunning = false;
						return val;
					}

					// Restore the caller's execution state from its frame.
					CallInfo callInfo = callStack[callStackTop];
					pc = callInfo.ReturnPC;
					baseIndex = callInfo.ReturnBase;
					currentFunc = callInfo.ReturnFunc;
					SwitchFrame(currentFunc, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
					// CPP: SwitchFrame(currentFunc, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);

					if (callInfo.CopyResultToReg >= 0) {
						stack[baseIndex + callInfo.CopyResultToReg] = val;
					}

					// Detect return from a manually-pushed call (e.g. import module).
					// Save the result and exit RunInner so Run() re-invokes the callback.
					if (_hasPendingManualCall && callStackTop == _pendingManualCallDepth - 1) {
						ManualCallResult = val;
						_hasPendingManualCall = false;
						cyclesLeft = 0;
					}
					break;
				}

				case Opcode.ITERGET_rA_rB_rC: {
					// R[A] = entry from R[B] at iterator position R[C]
					// For lists: same as list[index]
					// For strings: same as string[index]
					// For maps: returns {"key":k, "value":v} via map_iter_entry
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					valB = localStack[b];  // container
					Int32 idx = as_int(localStack[c]);

					if (valB.IsList()) {
						localStack[a] = list_get(valB, idx);
					} else if (valB.IsMap()) {
						localStack[a] = map_iter_entry(valB, idx);
					} else if (valB.IsString()) {
						localStack[a] = string_substring(valB, idx, 1);
					} else {
						localStack[a] = Value.Null;
					}
					break;
				}

				// CPP: VM_DISPATCH_END();
//*** BEGIN CS_ONLY ***
				default:
					IOHelper.Print("Unknown opcode");
					SaveState(pc, baseIndex, currentFunc);
					return Value.Null;
			}
//*** END CS_ONLY ***
		}
		// CPP: VM_DISPATCH_BOTTOM();
		
		// Save state after loop exit (e.g. from error condition)
		SaveState(pc, baseIndex, currentFunc);
		return Value.Null;
	}

	// Verify that the callee frame [baseIndex, baseIndex + neededRegs) fits within
	// the register stack.  Returns true if OK; on overflow, raises a runtime error
	// and returns false.  Callers MUST bail out (without writing into the frame)
	// when this returns false, since RaiseRuntimeError does not stop the current
	// opcode handler on its own.
	[MethodImpl(AggressiveInlining)]
	private bool EnsureFrame(Int32 baseIndex, UInt16 neededRegs) {
		if (baseIndex + neededRegs > stack.Count) {
			RaiseRuntimeError("Stack Overflow");
			return false;
		}
		return true;
	}

	// Switch all frame-local execution state to the given function.
	//*** BEGIN CS_ONLY ***
	[MethodImpl(AggressiveInlining)]
	private void SwitchFrame(FuncDef currentFunc, Int32 baseIndex,
			ref FuncDef curFunc, ref Int32 codeCount,
			ref List<UInt32> curCode, ref List<Value> curConstants,
			ref Span<Value> localStack) {
		// Keep the frame-identifying fields current at every frame switch, so
		// BuildStackTrace is accurate at any point (paired with PC = pc in the loop).
		CurrentFunction = currentFunc;
		BaseIndex = baseIndex;
		curFunc = currentFunc;
		codeCount = curFunc.Code.Count;
		curCode = curFunc.Code;
		curConstants = curFunc.Constants;
		localStack = CollectionsMarshal.AsSpan(stack).Slice(baseIndex);
	}
	//*** END CS_ONLY ***
	// H: void SwitchFrame(const FuncDef& currentFunc, Int32 baseIndex, FuncDefStorage* &curFuncRaw, Int32 &codeCount, UInt32* &curCode, Value* &curConstants, Value* &localStack, Value* stackPtr);
	/*** BEGIN CPP_ONLY ***
	FORCE_INLINE void VMStorage::SwitchFrame(const FuncDef& currentFunc, Int32 baseIndex,
			FuncDefStorage* &curFuncRaw, Int32 &codeCount,
			UInt32* &curCode, Value* &curConstants,
			Value* &localStack, Value* stackPtr) {
		// Keep the frame-identifying fields current at every frame switch, so
		// BuildStackTrace is accurate at any point (paired with PC = pc in the loop).
		CurrentFunction = currentFunc;
		BaseIndex = baseIndex;
		curFuncRaw = currentFunc.get_storage();
		codeCount = curFuncRaw->Code.Count();
		curCode = &curFuncRaw->Code[0];
		curConstants = &curFuncRaw->Constants[0];
		localStack = stackPtr + baseIndex;
	}
	*** END CPP_ONLY ***/

	// Get the globals VarMap.  In REPL mode, returns the persistent ReplGlobals.
	// Otherwise, returns @main's cached callStack[0].LocalVarMap (creating it on
	// first use).  The cache stays current because NAME_rA_kBC keeps a live frame
	// VarMap in sync as new variables are declared.  callStack[0].ReturnFunc
	// holds @main's own function index (by convention for this slot), used to
	// find @main's MaxRegs.
	private Value GetGlobalsVarMap() {
		if (!ReplGlobals.IsNull()) return ReplGlobals;
		CallInfo gframe = callStack[0];
		FuncDef rf = gframe.ReturnFunc;
		Int32 regCount = rf.MaxRegs;
		Value result = gframe.GetLocalVarMap(stack, names, 0, regCount);
		callStack[0] = gframe;  // write back (CallInfo is a struct)
		return result;
	}

	// Get or create a VarMap for the current call frame's local variables.
	// callStack[callStackTop-1] is always the current execution-context frame
	// (callStackTop >= 1 is guaranteed since @main's frame is always at callStack[0]).
	[MethodImpl(AggressiveInlining)]
	private Value GetCurrentLocalVarMap(Int32 baseIndex, UInt16 maxRegs) {
		// At the top level (@main), locals and globals are the same object.
		if (callStackTop == 1) return GetGlobalsVarMap();
		CallInfo frame = callStack[callStackTop - 1];
		Value result = frame.GetLocalVarMap(stack, names, baseIndex, maxRegs);
		callStack[callStackTop - 1] = frame;  // write back (CallInfo is a struct)
		return result;
	}

	[MethodImpl(AggressiveInlining)]
	private void SaveState(Int32 pc, Int32 baseIndex, FuncDef currentFunc) {
		PC = pc;
		BaseIndex = baseIndex;
		CurrentFunction = currentFunc;
		if (_errorStackPending) FinalizeErrorStackTrace();
	}

	public Value LookupParamByName(String varName) {
		// Look up a parameter by name in the current frame.  This is provided
		// mainly for compatibility with 1.x; modern code should find parameters
		// by position, which is more efficient than searching by name.
		// Returns the value if found, or null if not found.
		FuncDef func = CurrentFunction;
		Value nameVal = make_string(varName);
		for (Int32 i = 0; i < func.ParamNames.Count; i++) {
			if (func.ParamNames[i] == nameVal) {
				return stack[BaseIndex + 1 + i];
			}
		}
		return Value.Null;
	}

	private Value LookupVariable(Value varName) {
		// Look up a variable in outer context (and eventually globals)
		// Returns the value if found, or null if not found
		Value result;		
		if (callStackTop > 0) {
			CallInfo currentFrame = callStack[callStackTop - 1];  // Current frame, not next frame
			// Check locals VarMap for variables set dynamically (e.g. by the import intrinsic).
			// This mirrors what user code "locals[varName] = x" does at runtime.
			if (!currentFrame.LocalVarMap.IsNull()) {
				if (map_try_get(currentFrame.LocalVarMap, varName, out result)) {
					return result;
				}
			}
			if (!currentFrame.OuterVarMap.IsNull()) {
				if (map_try_get(currentFrame.OuterVarMap, varName, out result)) {
					return result;
				}
			}
		}

		// Check global variables via VarMap (registers at base 0 in the @main frame)
		Value globalMap;
		if (callStackTop > 0 || !ReplGlobals.IsNull()) {
			globalMap = GetGlobalsVarMap();
			if (map_try_get(globalMap, varName, out result)) {
				return result;
			}
		}

		// Check intrinsics table
		String nameStr = as_cstring(varName);
		if (_intrinsics.TryGetValue(nameStr, out result)) {
			return result;
		}

		// self/super return null when not in a method context
		if (varName == Value.selfString || varName == Value.superString) {
			return Value.Null;
		}

		// Variable not found anywhere — raise an error
		RaiseRuntimeError(StringUtils.Format("Undefined Identifier: '{0}' is unknown in this context", varName));
		return Value.Null;
	}
}

}
