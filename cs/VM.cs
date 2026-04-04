using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
// H: #include "value.h"
// H: #include "FuncDef.g.h"
// H: #include "IntrinsicAPI.g.h"
// H: #include "ErrorPool.g.h"
// H: #include "value_map.h"
// H: #include <vector>
// H: #include "gc.h"
// CPP: #include "value_list.h"
// CPP: #include "value_string.h"
// CPP: #include "Bytecode.g.h"
// CPP: #include "Intrinsic.g.h"
// CPP: #include "CoreIntrinsics.g.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "Disassembler.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "dispatch_macros.h"
// CPP: #include "vm_error.h"
// CPP: #include "Interpreter.g.h"
// CPP: #include <chrono>

using static MiniScript.ValueHelpers;

namespace MiniScript {

using FuncDefRef = FuncDef; // H: typedef const FuncDefStorage& FuncDefRef;

// Call stack frame (return info)
public struct CallInfo {
	public Int32 ReturnPC;        // where to continue in caller (PC index)
	public Int32 ReturnBase;      // caller's base pointer (stack index)
	public Int32 ReturnFuncIndex; // caller's function index in functions list
	public Int32 CopyResultToReg; // register number to copy result to, or -1
	public Value LocalVarMap;     // VarMap representing locals, if any
	public Value OuterVarMap;     // VarMap representing outer variables (closure context)

	public CallInfo(Int32 returnPC, Int32 returnBase, Int32 returnFuncIndex, Int32 copyToReg=-1) {
		ReturnPC = returnPC;
		ReturnBase = returnBase;
		ReturnFuncIndex = returnFuncIndex;
		CopyResultToReg = copyToReg;
		LocalVarMap = val_null;
		OuterVarMap = val_null;
	}

	public CallInfo(Int32 returnPC, Int32 returnBase, Int32 returnFuncIndex, Int32 copyToReg, Value outerVars) {
		ReturnPC = returnPC;
		ReturnBase = returnBase;
		ReturnFuncIndex = returnFuncIndex;
		CopyResultToReg = copyToReg;
		LocalVarMap = val_null;
		OuterVarMap = outerVars;
	}

	public Value GetLocalVarMap(List<Value> registers, List<Value> names, int baseIdx, int regCount) {
		if (is_null(LocalVarMap)) {
			// Create a new VarMap with references to VM's stack and names arrays
			if (regCount == 0) {
				// We have no local vars at all!  Make an ordinary map.
				LocalVarMap = make_map(4);	// This is safe, right?
			} else {
				LocalVarMap = make_varmap(registers, names, baseIdx, regCount); // CPP: LocalVarMap = make_varmap(&registers[0], &names[0], baseIdx, regCount);
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
	private Interpreter interpreter; // H: private: InterpreterStorage* interpreter;
	public void SetInterpreter(Interpreter interp) { // NO_INLINE
		interpreter = interp; // CPP: interpreter = interp.get_storage();
	}
	// H_WRAPPER: public: void SetInterpreter(InterpreterStorage* p) { get()->interpreter = p; }
	public Interpreter GetInterpreter() { // NO_INLINE
		return interpreter; // CPP: return Interpreter(interpreter);
	}

	private List<CallInfo> callStack;
	private Int32 callStackTop;	   // Index of next free call stack slot

	private List<FuncDef> functions; // functions addressed by CALLF
	/*** BEGIN H_ONLY ***
	private: std::vector<FuncDefStorage*> functionsRaw;
	*** END H_ONLY ***/
	
	private Dictionary<String, Value> _intrinsics; // intrinsic name -> FuncRef Value

	// Execution state (persistent across RunSteps calls)
	public Int32 PC { get; private set; }
	private Int32 _currentFuncIndex = -1;
	public FuncDef CurrentFunction { get; private set; }
	public Boolean IsRunning { get; private set; }
	public Int32 BaseIndex { get; private set; }
	public String RuntimeError { get; private set; }
	public ErrorPool Errors;

	// REPL mode: persistent globals VarMap shared across REPL entries.
	// When set (not val_null), used instead of callStack[0].GetLocalVarMap for globals.
	public Value ReplGlobals = val_null;

	// Pending self/super for method calls, set by METHFIND/SETSELF,
	// consumed by the next CALL instruction
	private Value pendingSelf;
	private Value pendingSuper;
	private bool hasPendingContext;

	// Pending intrinsic continuation: when an intrinsic returns done=false,
	// we store the state here so Run can re-invoke it on the next call.
	// The partial result value is stored in stack[_pendingResultIndex].
	private NativeCallbackDelegate _pendingCallback;
	private Int32 _pendingCalleeBase;   // base index for reconstructing Context
	private Int32 _pendingArgCount;     // arg count for reconstructing Context
	private Int32 _pendingResultIndex;  // absolute stack index for result (and partial result)

	// Set by the "yield" intrinsic; host app can check and clear this.
	public bool yielding;

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
	// (like list_push) can report errors without passing ErrorPool around.
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
		if (index < 0 || index >= stack.Count) return val_null;
		return stack[index];
	}

	public Value GetStackName(Int32 index) {
		if (index < 0 || index >= names.Count) return val_null;
		return names[index];
	}

	public CallInfo GetCallStackFrame(Int32 index) {
		if (index < 0 || index >= callStackTop) return new CallInfo(0, 0, -1);
		return callStack[index];
	}

	public String GetFunctionName(Int32 funcIndex) {
		if (funcIndex < 0 || funcIndex >= functions.Count) return "???";
		return functions[funcIndex].Name;
	}

	public VM(Int32 stackSlots=1024, Int32 callSlots=256) {
		InitVM(stackSlots, callSlots);
	}

	private void InitVM(Int32 stackSlots, Int32 callSlots) {
		stack = new List<Value>();
		names = new List<Value>();
		callStack = new List<CallInfo>();
		functions = new List<FuncDef>();
		callStackTop = 0;
		RuntimeError = "";

		// Initialize stack with null values
		for (Int32 i = 0; i < stackSlots; i++) {
			stack.Add(val_null);
			names.Add(val_null);		// No variable name initially
		}
		
		
		// Pre-allocate call stack capacity
		for (Int32 i = 0; i < callSlots; i++) {
			callStack.Add(new CallInfo(0, 0, -1)); // -1 = invalid function index
		}
		
		/*** BEGIN CPP_ONLY ***
		// Register as a source of roots for the GC system
		gc_register_mark_callback(VMStorage::MarkRoots, this);
		
		// And, ensure that runtime errors are routed through the active VM
		vm_error_set_callback([](const char* msg) {
			VM vm = VMStorage::ActiveVM();
			if (!IsNull(vm)) vm.RaiseRuntimeError(String(msg));
		});
		*** END CPP_ONLY ***/
	}
	
	private void CleanupVM() {
		// CPP: gc_unregister_mark_callback(VMStorage::MarkRoots, this);
	}

	// H: static void MarkRoots(void* user_data);
	// H: public: ~VMStorage() { CleanupVM(); }
	/*** BEGIN CPP_ONLY ***
	// GC mark callback responsible for protecting our stack and names
	// from garbage collection
	void VMStorage::MarkRoots(void* user_data) {
		VMStorage* vm = static_cast<VMStorage*>(user_data);
		for (int i = 0; i < vm->stack.Count(); i++) {
			gc_mark_value(vm->stack[i]);
			gc_mark_value(vm->names[i]);
		}
		// Mark intrinsics dictionary values (funcrefs are GC-allocated)
		if (!IsNull(vm->_intrinsics)) {
			for (Value val : vm->_intrinsics.GetValues()) {
				gc_mark_value(val);
			}
		}
	}
	*** END CPP_ONLY ***/

	public void RegisterFunction(FuncDef funcDef) {
		functions.Add(funcDef);
	}


	public void Reset(List<FuncDef> allFunctions) {
		Reset(allFunctions, val_null);
	}

	public void Reset(List<FuncDef> allFunctions, Value replGlobals) {
		bool partialReset = !is_null(replGlobals);

		Int32 mainIdx = -1;
		if (partialReset) {
			// REPL mode: intrinsics and previous user functions are already in the list.
			// Just append the newly compiled functions (which were compiled with an
			// offset matching the current functions.Count, so indices line up).
			for (Int32 i = 0; i < allFunctions.Count; i++) {
				if (allFunctions[i].Name == "@main") mainIdx = functions.Count;
				functions.Add(allFunctions[i]);
			}
		} else {
			// Full reset: register intrinsics first, then add user functions.
			// User functions are compiled with FunctionIndexOffset = intrinsic count,
			// so their indices in the bytecode already account for the intrinsics.
			functions.Clear();
			_intrinsics = new Dictionary<String, Value>();
			Intrinsic.RegisterAll(functions, _intrinsics);
			for (Int32 i = 0; i < allFunctions.Count; i++) {
				if (allFunctions[i].Name == "@main") mainIdx = functions.Count;
				functions.Add(allFunctions[i]);
			}
		}
		
		// Basic validation
		if (mainIdx < 0) {
			IOHelper.Print("ERROR: No @main function found in VM.Reset");
			return;
		}
		FuncDef mainFunc = functions[mainIdx];
		_currentFuncIndex = mainIdx;

		if (mainFunc.Code.Count == 0) {
			IOHelper.Print("Entry function has no code");
			return;
		}

		/*** BEGIN CPP_ONLY ***
		// C++ only: copy functions into functionsRaw vector for quick access
		functionsRaw.clear();
		functionsRaw.reserve(functions.Count());
		for (Int32 i = 0; i < functions.Count(); i++) {
			functionsRaw.push_back(functions[i].get_storage());
		}
		*** END CPP_ONLY ***/

		// Initialize execution state
		BaseIndex = 0;			  // entry executes at stack base
		PC = 0;				 // start at entry code
		CurrentFunction = mainFunc;
		IsRunning = true;
		callStackTop = 0;
		RuntimeError = "";
		pendingSelf = val_null;
		pendingSuper = val_null;
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
				newStack.Add(val_null);
				newNames.Add(val_null);
			}
			varmap_rebind(ReplGlobals, newStack, newNames); // CPP: varmap_rebind(ReplGlobals, &newStack[0], &newNames[0]);
			stack = newStack;
			names = newNames;
		}

		// Start the run timer (e.g. for the `time` intrinsic)
		_stopwatch.Restart(); // CPP: _startTime = std::chrono::steady_clock::now();

		if (DebugMode) {
			IOHelper.Print(StringUtils.Format("VM Reset: Executing {0} out of {1} functions", mainFunc.Name, functions.Count));
		}
	}

	public void Stop() {
		IsRunning = false;
	}

	public void RaiseRuntimeError(String message) {
		RuntimeError = message;
		Errors.Add(StringUtils.Format("Runtime Error: {0}", message));
		IsRunning = false;
	}

	public bool ReportRuntimeError() {
		if (String.IsNullOrEmpty(RuntimeError)) return false;
		IOHelper.Print(StringUtils.Format("Runtime Error: {0} [{1} line {2}]",
		  RuntimeError, CurrentFunction.Name, PC - 1));
		return true;
	}

	public Int32 FunctionCount() {
		return functions.Count;
	}

	public List<FuncDef> GetFunctions() {
		return functions;
	}

	// Helper for argument processing (FUNCTION_CALLS.md steps 1-3):
	// Process ARG instructions, validate argument count, and set up parameter registers.
	// Returns the PC after the CALL instruction, or -1 on error.
	// Check whether pendingSelf should be injected as the first parameter.
	// Returns 1 if the callee's first param is named "self" and we have pending context, else 0.
	private Int32 SelfParamOffset(FuncDefRef callee) {
		if (hasPendingContext && callee.ParamNames.Count > 0 && value_equal(callee.ParamNames[0], val_self)) {
			return 1;
		}
		return 0;
	}

	private Int32 ProcessArguments(Int32 argCount, Int32 selfParam, Int32 startPC, Int32 callerBase, Int32 calleeBase, FuncDefRef callee, List<UInt32> code) {
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
		Value argValue = val_null;  // Declared outside loop for GC safety
		for (Int32 i = 0; i < argCount; i++) {
			UInt32 argInstruction = code[currentPC];
			Opcode argOp = (Opcode)BytecodeUtil.OP(argInstruction);

			argValue = val_null;
			if (argOp == Opcode.ARG_rA) {
				// Argument from register
				Byte srcReg = BytecodeUtil.Au(argInstruction);
				argValue = stack[callerBase + srcReg];
			} else if (argOp == Opcode.ARG_iABC) {
				// Argument immediate value
				Int32 immediate = BytecodeUtil.ABCs(argInstruction);
				argValue = make_int(immediate);
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
	private void ApplyPendingContext(Int32 calleeBase, FuncDefRef callee) {
		if (!hasPendingContext) return;
		if (callee.SelfReg >= 0) {
			stack[calleeBase + callee.SelfReg] = pendingSelf;
			names[calleeBase + callee.SelfReg] = val_self;
		}
		if (callee.SuperReg >= 0) {
			stack[calleeBase + callee.SuperReg] = pendingSuper;
			names[calleeBase + callee.SuperReg] = val_super;
		}
		pendingSelf = val_null;
		pendingSuper = val_null;
		hasPendingContext = false;
	}

	// Helper for call setup (FUNCTION_CALLS.md steps 4-6):
	// Initialize remaining parameters with defaults and clear callee's registers.
	// Note: Parameters start at r1 (r0 is reserved for return value)
	// selfParam is 1 if pendingSelf was injected as the first arg, else 0.
	private void SetupCallFrame(Int32 argCount, Int32 selfParam, Int32 calleeBase, FuncDefRef callee) {
		Int32 paramCount = callee.ParamNames.Count;

		// Step 4: Set up remaining parameters with default values
		// Parameters start at r1, so offset by 1
		for (Int32 i = argCount + selfParam; i < paramCount; i++) {
			stack[calleeBase + 1 + i] = callee.ParamDefaults[i];
			names[calleeBase + 1 + i] = callee.ParamNames[i];
		}

		// Step 5: Clear remaining registers (r0, and any beyond parameters)
		stack[calleeBase] = val_null;
		names[calleeBase] = val_null;
		for (Int32 i = paramCount + 1; i < callee.MaxRegs; i++) {
			stack[calleeBase + i] = val_null;
			names[calleeBase + i] = val_null;
		}

		// Step 6 is handled by the caller (pushing CallInfo, switching frame, etc.)
	}

	// Auto-invoke a zero-arg funcref (used by LOADC and CALLIFREF).
	// Resolves the funcref, then either:
	//   - Native callback (done):    stores result, returns -1.
	//   - Native callback (pending): stores pending state, returns -2.
	//   - User function: pushes CallInfo and sets up callee frame, returns the callee function index.
	//     Caller must switch its local execution state (pc, baseIndex, curFunc, etc.).
	// On error, calls RaiseRuntimeError and returns -1.
	private Int32 AutoInvokeFuncRef(Value funcRefVal, Int32 resultReg, Int32 returnPC, Int32 baseIndex, Int32 currentFuncIndex, FuncDefRef curFunc) {
		Int32 funcIndex = funcref_index(funcRefVal);
		if (funcIndex < 0 || funcIndex >= functions.Count) {
			RaiseRuntimeError("Auto-invoke: Invalid function index");
			return -1;
		}

		FuncDefRef callee =
		   functions[funcIndex]; // CPP: *functionsRaw[funcIndex];
		Value outerVars = funcref_outer_vars(funcRefVal);

		Int32 calleeBase = baseIndex + curFunc.MaxRegs;

		Int32 selfParam = SelfParamOffset(callee);

		// Native intrinsic: invoke callback directly, no frame push
		if (callee.NativeCallback != null) {
			EnsureFrame(calleeBase, callee.MaxRegs);
			SetupCallFrame(0, selfParam, calleeBase, callee);
			if (selfParam > 0) {
				stack[calleeBase + 1] = pendingSelf;
				names[calleeBase + 1] = val_self;
				pendingSelf = val_null;
				hasPendingContext = false;
			}
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
		callStack[callStackTop] = new CallInfo(returnPC, baseIndex, currentFuncIndex, resultReg, outerVars);
		callStackTop++;

		if (selfParam > 0) {
			stack[calleeBase + 1] = pendingSelf;
			names[calleeBase + 1] = val_self;
		}
		SetupCallFrame(0, selfParam, calleeBase, callee);
		ApplyPendingContext(calleeBase, callee);
		EnsureFrame(calleeBase, callee.MaxRegs);
		return funcIndex;
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
			return val_null;
		}

		// Set thread-local active VM (save/restore for nested calls)
		VM previousVM = _activeVM;
		_activeVM = this;

		// If we have a pending intrinsic continuation, re-invoke it
		// before resuming normal execution.
		if (_pendingCallback != null) {
			IntrinsicResult partialResult = new IntrinsicResult(stack[_pendingResultIndex], false);
			if (!InvokeNativeCallback(_pendingCallback, _pendingCalleeBase, _pendingArgCount, partialResult, _pendingResultIndex)) {
				// Still not done; return without running any bytecode
				_activeVM = previousVM;
				return val_null;
			}
			_pendingCallback = null;
		}

		Value runResult = RunInner(maxCycles);
		_activeVM = previousVM;
		return runResult;
	}

	private Value RunInner(UInt32 maxCycles) {
		// Copy instance variables to locals for performance
		Int32 pc = PC;
		Int32 baseIndex = BaseIndex;
		Int32 currentFuncIndex = _currentFuncIndex;

		// CPP: Value* stackPtr = &stack[0];
		// Note: CollectionsMarshal.AsSpan requires .NET 5+; not compatible with Mono.
		// This gives us direct array access without copying, for performance.
		
		// Variables reflecting the current call frame:
		FuncDef curFunc = null; // CPP: FuncDefStorage* curFuncRaw = nullptr;
		Int32 codeCount = 0;
		List<UInt32> curCode = null; // CPP: UInt32* curCode = nullptr;
		List<Value> curConstants = null; // CPP: Value* curConstants = nullptr;
		Span<Value> localStack = default; // CPP: Value* localStack = nullptr;
		
		SwitchFrame(currentFuncIndex, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
		// CPP: SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);

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
				SaveState(pc, baseIndex, currentFuncIndex);
				return val_null;
			}
			cyclesLeft--;

			if (pc >= codeCount) {
				IsRunning = false;
				SaveState(pc, baseIndex, currentFuncIndex);
				return val_null;
			}

			UInt32 instruction = curCode[pc++];

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
					localStack[a] = make_int(bc);
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
					localStack[a] = val_null;
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

					if (!is_funcref(valB)) {
						// Simple case: value is not a funcref, so just copy it
						localStack[a] = valB;
					} else {
						// Value is a funcref — auto-invoke with zero args
						Int32 calleeIdx = AutoInvokeFuncRef(valB, a, pc, baseIndex, currentFuncIndex,
						  functions[currentFuncIndex]); // CPP: *functionsRaw[currentFuncIndex]);
						if (calleeIdx == -2) {
							// Native callback pending — exit RunInner
							cyclesLeft = 0;
						} else if (calleeIdx >= 0) {
							// Frame was pushed — switch to callee
							baseIndex += curFunc.MaxRegs; // CPP: baseIndex += curFuncRaw->MaxRegs;
							pc = 0;
							currentFuncIndex = calleeIdx;
							SwitchFrame(currentFuncIndex, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
							// CPP: SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
						}
					}
					break;
				}

				case Opcode.FUNCREF_iA_iBC: {
					// R[A] := make_funcref(BC) with closure context
					Byte a = BytecodeUtil.Au(instruction);
					Int16 funcIndex = BytecodeUtil.BCs(instruction);

					// Create function reference with our locals as the closure context
					val = GetCurrentLocalVarMap(baseIndex, curFunc.MaxRegs); // CPP: val = GetCurrentLocalVarMap(baseIndex, curFuncRaw->MaxRegs);
					localStack[a] = make_funcref(funcIndex, val);
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
					if (baseIndex == 0 && !is_null(ReplGlobals)) {
						varmap_map_to_register(ReplGlobals, valC, 
							stack,  // CPP: &stack[0],
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
					// In REPL mode, register this variable in the globals VarMap
					if (baseIndex == 0 && !is_null(ReplGlobals)) {
						varmap_map_to_register(ReplGlobals, valC,
							stack,  // CPP: &stack[0],
							baseIndex + a);
					}
					break;
				}

				case Opcode.ADD_rA_rB_rC: {
					// R[A] = R[B] + R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					localStack[a] = value_add(localStack[b], localStack[c]);
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

				case Opcode.MULT_rA_rB_rC: {
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

					if (is_list(valB)) {
						// ToDo: add a list_try_get and use it here, like we do with map below
						localStack[a] = list_get(valB, as_int(valC));
					} else if (is_map(valB)) {
						if (!map_lookup(valB, valC, out val)) {
							RaiseRuntimeError(StringUtils.Format("Key Not Found: '{0}' not found in map", valC));
						}
						localStack[a] = val;
					} else if (is_string(valB)) {
						Int32 idx = as_int(valC);
						localStack[a] = string_substring(valB, idx, 1);
					} else {
						RaiseRuntimeError(StringUtils.Format("Can't index into {0}", valB));
						localStack[a] = val_null;
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

					if (is_list(valA)) {
						list_set(valA, as_int(valB), valC);
					} else if (is_map(valA)) {
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

					if (is_string(valB)) {
						Int32 len = string_length(valB);
						Int32 startIdx = is_null(valC) ? 0 : as_int(valC);
						Int32 endIdx = is_null(valD) ? len : as_int(valD);
						localStack[a] = string_slice(valB, startIdx, endIdx);
					} else if (is_list(valB)) {
						Int32 len = list_count(valB);
						Int32 startIdx = is_null(valC) ? 0 : as_int(valC);
						Int32 endIdx = is_null(valD) ? len : as_int(valD);
						localStack[a] = list_slice(valB, startIdx, endIdx);
					} else {
						RaiseRuntimeError(StringUtils.Format("Can't slice {0}", valB));
						localStack[a] = val_null;
					}
					break;
				}

				case Opcode.LOCALS_rA: {
					// Create VarMap for local variables and store in R[A]
					Byte a = BytecodeUtil.Au(instruction);
					localStack[a] = GetCurrentLocalVarMap(baseIndex, curFunc.MaxRegs); // CPP: localStack[a] = GetCurrentLocalVarMap(baseIndex, curFuncRaw->MaxRegs);
					names[baseIndex+a] = val_null;
					break;
				}

				case Opcode.OUTER_rA: {
					// Return VarMap for outer scope; fall back to globals if none
					Byte a = BytecodeUtil.Au(instruction);
					if (callStackTop > 0 && !is_null(callStack[callStackTop - 1].OuterVarMap)) {
						localStack[a] = callStack[callStackTop - 1].OuterVarMap;
					} else {
						// No enclosing scope or at global scope: outer == globals
						localStack[a] = GetGlobalsVarMap();
					}
					names[baseIndex+a] = val_null;
					break;
				}

				case Opcode.GLOBALS_rA: {
					// Return VarMap for global variables
					Byte a = BytecodeUtil.Au(instruction);
					localStack[a] = GetGlobalsVarMap();
					names[baseIndex+a] = val_null;
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

					localStack[a] = make_int(value_lt(localStack[b], localStack[c]));
					break;
				}

				case Opcode.LT_rA_rB_iC: {
					// if R[A] = R[B] < C (immediate)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte c = BytecodeUtil.Cs(instruction);
					
					localStack[a] = make_int(value_lt(localStack[b], make_int(c)));
					break;
				}

				case Opcode.LT_rA_iB_rC: {
					// if R[A] = B (immediate) < R[C]
					Byte a = BytecodeUtil.Au(instruction);
					SByte b = BytecodeUtil.Bs(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					
					localStack[a] = make_int(value_lt(make_int(b), localStack[c]));
					break;
				}

				case Opcode.LE_rA_rB_rC: {
					// if R[A] = R[B] <= R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);

					localStack[a] = make_int(value_le(localStack[b], localStack[c]));
					break;
				}

				case Opcode.LE_rA_rB_iC: {
					// if R[A] = R[B] <= C (immediate)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte c = BytecodeUtil.Cs(instruction);
					
					localStack[a] = make_int(value_le(localStack[b], make_int(c)));
					break;
				}

				case Opcode.LE_rA_iB_rC: {
					// if R[A] = B (immediate) <= R[C]
					Byte a = BytecodeUtil.Au(instruction);
					SByte b = BytecodeUtil.Bs(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					
					localStack[a] = make_int(value_le(make_int(b), localStack[c]));
					break;
				}

				case Opcode.EQ_rA_rB_rC: {
					// if R[A] = R[B] == R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);

					localStack[a] = make_int(value_equal(localStack[b], localStack[c]));
					break;
				}

				case Opcode.EQ_rA_rB_iC: {
					// if R[A] = R[B] == C (immediate)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte c = BytecodeUtil.Cs(instruction);
					
					localStack[a] = make_int(value_equal(localStack[b], make_int(c)));
					break;
				}

				case Opcode.NE_rA_rB_rC: {
					// if R[A] = R[B] != R[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);

					localStack[a] = make_int(!value_equal(localStack[b], localStack[c]));
					break;
				}

				case Opcode.NE_rA_rB_iC: {
					// if R[A] = R[B] != C (immediate)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte c = BytecodeUtil.Cs(instruction);
					
					localStack[a] = make_int(!value_equal(localStack[b], make_int(c)));
					break;
				}

				case Opcode.BRTRUE_rA_iBC: {
					Byte a = BytecodeUtil.Au(instruction);
					Int32 offset = BytecodeUtil.BCs(instruction);
					if (is_truthy(localStack[a])){
						pc += offset;
					}
					break;
				}

				case Opcode.BRFALSE_rA_iBC: {
					Byte a = BytecodeUtil.Au(instruction);
					Int32 offset = BytecodeUtil.BCs(instruction);
					if (!is_truthy(localStack[a])){
						pc += offset;
					}
					break;
				}

				case Opcode.BRLT_rA_rB_iC: {
					// if R[A] < R[B] then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
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
					if (value_lt(localStack[a], make_int(b))){
						pc += offset;
					}
					break;
				}

				case Opcode.BRLT_iA_rB_iC: {
					// if A (immediate) < R[B] then jump offset C.
					SByte a = BytecodeUtil.As(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (value_lt(make_int(a), localStack[b])){
						pc += offset;
					}
					break;
				}

				case Opcode.BRLE_rA_rB_iC: {
					// if R[A] <= R[B] then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
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
					if (value_le(localStack[a], make_int(b))){
						pc += offset;
					}
					break;
				}

				case Opcode.BRLE_iA_rB_iC: {
					// if A (immediate) <= R[B] then jump offset C.
					SByte a = BytecodeUtil.As(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (value_le(make_int(a), localStack[b])){
						pc += offset;
					}
					break;
				}

				case Opcode.BREQ_rA_rB_iC: {
					// if R[A] == R[B] then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (value_equal(localStack[a], localStack[b])){
						pc += offset;
					}
					break;
				}

				case Opcode.BREQ_rA_iB_iC: {
					// if R[A] == B (immediate) then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					SByte b = BytecodeUtil.Bs(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (value_equal(localStack[a], make_int(b))){
						pc += offset;
					}
					break;
				}

				case Opcode.BRNE_rA_rB_iC: {
					// if R[A] != R[B] then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (!value_equal(localStack[a], localStack[b])){
						pc += offset;
					}
					break;
				}

				case Opcode.BRNE_rA_iB_iC: {
					// if R[A] != B (immediate) then jump offset C.
					Byte a = BytecodeUtil.Au(instruction);
					SByte b = BytecodeUtil.Bs(instruction);
					SByte offset = BytecodeUtil.Cs(instruction);
					if (!value_equal(localStack[a], make_int(b))){
						pc += offset;
					}
					break;
				}

				case Opcode.IFLT_rA_rB: {
					// if R[A] < R[B] is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					if (!value_lt(localStack[a], localStack[b])) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFLT_rA_iBC: {
					// if R[A] < BC (immediate) is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					short bc = BytecodeUtil.BCs(instruction);
					if (!value_lt(localStack[a], make_int(bc))) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFLT_iAB_rC: {
					// if AB (immediate) < R[C] is false, skip next instruction
					short ab = BytecodeUtil.ABs(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					if (!value_lt(make_int(ab), localStack[c])) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFLE_rA_rB: {
					// if R[A] <= R[B] is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					if (!value_le(localStack[a], localStack[b])) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFLE_rA_iBC: {
					// if R[A] <= BC (immediate) is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					short bc = BytecodeUtil.BCs(instruction);
					if (!value_le(localStack[a], make_int(bc))) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFLE_iAB_rC: {
					// if AB (immediate) <= R[C] is false, skip next instruction
					short ab = BytecodeUtil.ABs(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					if (!value_le(make_int(ab), localStack[c])) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFEQ_rA_rB: {
					// if R[A] == R[B] is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					if (!value_equal(localStack[a], localStack[b])) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFEQ_rA_iBC: {
					// if R[A] == BC (immediate) is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					short bc = BytecodeUtil.BCs(instruction);
					if (!value_equal(localStack[a], make_int(bc))) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFNE_rA_rB: {
					// if R[A] != R[B] is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					if (value_equal(localStack[a], localStack[b])) {
						pc++; // Skip next instruction
					}
					break;
				}

				case Opcode.IFNE_rA_iBC: {
					// if R[A] != BC (immediate) is false, skip next instruction
					Byte a = BytecodeUtil.Au(instruction);
					short bc = BytecodeUtil.BCs(instruction);
					if (value_equal(localStack[a], make_int(bc))) {
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
					if (is_list(valB)) {
						iter++;
						hasMore = (iter < list_count(valB));
					} else if (is_map(valB)) {
						iter = map_iter_next(valB, iter);
						hasMore = (iter != MAP_ITER_DONE);
					} else if (is_string(valB)) {
						iter++;
						hasMore = (iter < string_length(valB));
					} else {
						hasMore = false;
					}
					localStack[a] = make_int(iter);
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
						return val_null;
					}
					UInt32 callInstruction = curCode[callPC];
					Opcode callOp = (Opcode)BytecodeUtil.OP(callInstruction);
					if (callOp != Opcode.CALL_rA_rB_rC) {
						RaiseRuntimeError("ARGBLK must be followed by CALL");
						return val_null;
					}

					// CALL r[A], r[B], r[C]: invoke funcref in r[C], frame at r[B], result to r[A]
					Byte a = BytecodeUtil.Au(callInstruction);
					Byte b = BytecodeUtil.Bu(callInstruction);
					Byte c = BytecodeUtil.Cu(callInstruction);

					valC = localStack[c];  // func ref
					if (!is_funcref(valC)) {
						RaiseRuntimeError("ARGBLK/CALL: Not a function reference");
						return val_null;
					}

					Int32 funcIndex = funcref_index(valC);
					if (funcIndex < 0 || funcIndex >= functions.Count) {
						RaiseRuntimeError("ARGBLK/CALL: Invalid function index");
						return val_null;
					}
					FuncDefRef callee = 
					  functions[funcIndex]; // CPP: *functionsRaw[funcIndex];
					Int32 calleeBase = baseIndex + b;
					Int32 resultReg = a;

					// Check for self-injection: if pending context and first param is "self",
					// inject pendingSelf as the first argument
					Int32 selfParam = SelfParamOffset(callee);
					Int32 nextPC = ProcessArguments(argCount, selfParam, pc, baseIndex, calleeBase, callee, 
					  curFunc.Code); // CPP: curFuncRaw->Code);
					if (nextPC < 0) return val_null; // Error already raised
					if (selfParam > 0) {
						stack[calleeBase + 1] = pendingSelf;
						names[calleeBase + 1] = val_self;
					}

					// Set up call frame using helper
					SetupCallFrame(argCount, selfParam, calleeBase, callee);
					ApplyPendingContext(calleeBase, callee);
					EnsureFrame(calleeBase, callee.MaxRegs);

					// Native intrinsic: invoke callback directly, no frame push
					if (callee.NativeCallback != null) {
						pc = nextPC;
						if (!InvokeNativeCallback(callee.NativeCallback, calleeBase, argCount + selfParam, IntrinsicResult.Null, baseIndex + resultReg)) {
							cyclesLeft = 0;
						}
						break;
					}

					// Now execute the CALL (step 6): push CallInfo and switch to callee
					if (callStackTop >= callStack.Count) {
						RaiseRuntimeError("Call stack overflow");
						return val_null;
					}

					val = funcref_outer_vars(valC);
					callStack[callStackTop] = new CallInfo(nextPC, baseIndex, currentFuncIndex, resultReg, val);
					callStackTop++;

					baseIndex = calleeBase;
					currentFuncIndex = funcIndex; // Switch to callee function
					pc = 0; // Start at beginning of callee code
					SwitchFrame(currentFuncIndex, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
					// CPP: SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
					EnsureFrame(baseIndex, callee.MaxRegs);
					break;
				}

				case Opcode.ARG_rA: {
					// The VM should never encounter this opcode on its own; it will
					// be processed as part of the ARGBLK opcode.  So if we get
					// here, it's an error.
					RaiseRuntimeError("Internal error: ARG without ARGBLK");
					return val_null;
				}

				case Opcode.ARG_iABC: {
					// The VM should never encounter this opcode on its own; it will
					// be processed as part of the ARGBLK opcode.  So if we get
					// here, it's an error.
					RaiseRuntimeError("Internal error: ARG without ARGBLK");
					return val_null;
				}

				case Opcode.CALLF_iA_iBC: {
					// A: arg window start (callee executes with base = base + A)
					// BC: function index
					Byte a = BytecodeUtil.Au(instruction);
					UInt16 funcIndex = BytecodeUtil.BCu(instruction);
					
					if (funcIndex >= functions.Count) {
						RaiseRuntimeError("CALLF: Invalid function index");
						break;
					}

					FuncDefRef callee =
					  functions[funcIndex]; // CPP: *functionsRaw[funcIndex];

					// Push return info
					if (callStackTop >= callStack.Count) {
						RaiseRuntimeError("Call stack overflow");
						break;
					}
					callStack[callStackTop] = new CallInfo(pc, baseIndex, currentFuncIndex);
					callStackTop++;

					// Switch to callee frame: base slides to argument window
					baseIndex += a;
					// Note: ApplyPendingContext skipped for CALLF (only needed for method dispatch via CALL)
					pc = 0; // Start at beginning of callee code
					currentFuncIndex = funcIndex; // Switch to callee function
					SwitchFrame(currentFuncIndex, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
					// CPP: SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);

					EnsureFrame(baseIndex, callee.MaxRegs);
					break;
				}

				case Opcode.CALLFN_iA_kBC: {
					// DEPRECATED: no longer emitted by the compiler.
					// Intrinsics are now callable FuncRefs resolved via LOADV + CALL.
					RaiseRuntimeError("CALLFN is no longer supported; use LOADV + CALL instead");
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
					if (!is_funcref(valC)) {
						RaiseRuntimeError("CALL: Value in register is not a function reference");
						break;
					}

					Int32 funcIndex = funcref_index(valC);
					if (funcIndex < 0 || funcIndex >= functions.Count) {
						RaiseRuntimeError("CALL: Invalid function index in FuncRef");
						break;
					}
					FuncDefRef callee =
					   functions[funcIndex]; // CPP: *functionsRaw[funcIndex];
					valD = funcref_outer_vars(valC);  // valD: "outer" VarMap of func valC

					// For naked CALL (without ARGBLK): set up parameters with defaults
					Int32 calleeBase = baseIndex + b;
					Int32 selfParam = SelfParamOffset(callee);
					SetupCallFrame(0, selfParam, calleeBase, callee);
					if (selfParam > 0) {
						stack[calleeBase + 1] = pendingSelf;
						names[calleeBase + 1] = val_self;
					}
					ApplyPendingContext(calleeBase, callee);
					EnsureFrame(calleeBase, callee.MaxRegs);

					// Native intrinsic: invoke callback directly, no frame push
					if (callee.NativeCallback != null) {
						if (!InvokeNativeCallback(callee.NativeCallback, calleeBase, selfParam, IntrinsicResult.Null, baseIndex + a)) {
							cyclesLeft = 0;
						}
						break;
					}

					if (callStackTop >= callStack.Count) {
						RaiseRuntimeError("Call stack overflow");
						break;
					}
					callStack[callStackTop] = new CallInfo(pc, baseIndex, currentFuncIndex, a, valD);
					callStackTop++;

					// Set up call frame starting at baseIndex + b
					baseIndex = calleeBase;
					pc = 0; // Start at beginning of callee code
					currentFuncIndex = funcIndex; // Switch to callee function
					SwitchFrame(currentFuncIndex, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
					// CPP: SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
					EnsureFrame(baseIndex, callee.MaxRegs);
					break;
				}

				case Opcode.NEW_rA_rB: {
					// R[A] = new map with __isa set to R[B]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					val = make_map(2);
					map_set(val, val_isa_key, localStack[b]);
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
					if (is_null(valB) && is_null(valC)) {
						isaResult = 1;
					} else if (value_identical(valB, valC)) {
						isaResult = 1;
					} else if (is_map(valC)) {
						// Walk valB's __isa chain looking for valC
						if (is_map(valB)) {
							val = valB;  // val is "current"; valA (below) is "next" in the __isa chain
							for (Int32 depth = 0; depth < 256; depth++) {
								if (!map_try_get(val, val_isa_key, out valA)) break;
								if (value_identical(valA, valC)) {
									isaResult = 1;
									break;
								}
								val = valA;
							}
						}
						// If not found via __isa chain, check built-in type maps
						if (isaResult == 0) {
							if (is_number(valB) && value_identical(valC, CoreIntrinsics.NumberType())) {
								isaResult = 1;
							} else if (is_string(valB) && value_identical(valC, CoreIntrinsics.StringType())) {
								isaResult = 1;
							} else if (is_list(valB) && value_identical(valC, CoreIntrinsics.ListType())) {
								isaResult = 1;
							} else if (is_map(valB) && value_identical(valC, CoreIntrinsics.MapType())) {
								isaResult = 1;
							} else if (is_funcref(valB) && value_identical(valC, CoreIntrinsics.FunctionType())) {
								isaResult = 1;
							}
						}
					}
					localStack[a] = make_int(isaResult);
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
					typeMap = val_null;
					
					if (is_map(valB)) {
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
					} else if (is_list(valB)) {
						typeMap = CoreIntrinsics.ListType();
					} else if (is_string(valB)) {
						typeMap = CoreIntrinsics.StringType();
					} else if (is_number(valB)) {
						typeMap = CoreIntrinsics.NumberType();
					}
					if (is_null(typeMap)) {
						// If we didn't get a type map, then user is trying to index
						// into something not indexable
						RaiseRuntimeError(StringUtils.Format("Can't index into {0}", valB));
						localStack[a] = val_null;
					} else if (map_try_get(typeMap, valC, out val)) {
						// found what we're looking for in the type map
						localStack[a] = val;
						pendingSelf = valB;
						pendingSuper = val_null;
					} else if (is_number(valC)) {
						// try indexing numerically
						int index = as_int(valC);
						if (is_list(valB)) {
							localStack[a] = list_get(valB, index);
						} else if (is_string(valB)) {
							localStack[a] = string_substring(valB, index, 1);
						} else {
							RaiseRuntimeError(StringUtils.Format("Can't index into {0}", valB));
							localStack[a] = val_null;
						}
					} else {
						RaiseRuntimeError(StringUtils.Format("Key Not Found: '{0}' not found in map", valC));
						localStack[a] = val_null;
					}
					hasPendingContext = true;
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

					if (!is_funcref(val) || !hasPendingContext) {
						// Not a funcref or no context — clear pending state and leave value as-is
						hasPendingContext = false;
						pendingSelf = val_null;
						pendingSuper = val_null;
						break; // CPP: VM_NEXT();
					}

					// Auto-invoke the funcref with zero args
					Int32 calleeIdx = AutoInvokeFuncRef(val, a, pc, baseIndex, currentFuncIndex,
					  functions[currentFuncIndex]); // CPP: *functionsRaw[currentFuncIndex]);
					if (calleeIdx == -2) {
						// Native callback pending — exit RunInner
						cyclesLeft = 0;
					} else if (calleeIdx >= 0) {
						// Frame was pushed — switch to callee
						baseIndex += curFunc.MaxRegs; // CPP: baseIndex += curFuncRaw->MaxRegs;
						pc = 0;
						currentFuncIndex = calleeIdx;
						SwitchFrame(currentFuncIndex, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
						// CPP: SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
					}
					break; // CPP: VM_NEXT();
				}

				case Opcode.RETURN: {
					// Return value convention: value is in base[0]
					val = stack[baseIndex];
					if (callStackTop == 0) {
						// Returning from main function
						SaveState(pc, baseIndex, currentFuncIndex);
						IsRunning = false;
						return val;
					}
					
					// If current call frame had a locals VarMap, gather it up
					CallInfo frame = callStack[callStackTop - 1];
					if (!is_null(frame.LocalVarMap)) {
						varmap_gather(frame.LocalVarMap);
						frame.LocalVarMap = val_null;
						callStack[callStackTop - 1] = frame;  // write back (CallInfo is a struct)
					}

					// Pop call stack
					callStackTop--;
					CallInfo callInfo = callStack[callStackTop];
					pc = callInfo.ReturnPC;
					baseIndex = callInfo.ReturnBase;
					currentFuncIndex = callInfo.ReturnFuncIndex; // Restore the caller's function
					SwitchFrame(currentFuncIndex, baseIndex, ref curFunc, ref codeCount, ref curCode, ref curConstants, ref localStack); // CPP:
					// CPP: SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
					
					if (callInfo.CopyResultToReg >= 0) {
						stack[baseIndex + callInfo.CopyResultToReg] = val;
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

					if (is_list(valB)) {
						localStack[a] = list_get(valB, idx);
					} else if (is_map(valB)) {
						localStack[a] = map_iter_entry(valB, idx);
					} else if (is_string(valB)) {
						localStack[a] = string_substring(valB, idx, 1);
					} else {
						localStack[a] = val_null;
					}
					break;
				}

				// CPP: VM_DISPATCH_END();
//*** BEGIN CS_ONLY ***
				default:
					IOHelper.Print("Unknown opcode");
					SaveState(pc, baseIndex, currentFuncIndex);
					return val_null;
			}
//*** END CS_ONLY ***
		}
		// CPP: VM_DISPATCH_BOTTOM();
		
		// Save state after loop exit (e.g. from error condition)
		SaveState(pc, baseIndex, currentFuncIndex);
		return val_null;
	}

	[MethodImpl(AggressiveInlining)]
	private void EnsureFrame(Int32 baseIndex, UInt16 neededRegs) {
		if (baseIndex + neededRegs > stack.Count) {
			RaiseRuntimeError("Stack Overflow");
		}
	}

	// Switch all frame-local execution state to the function at currentFuncIndex.
	//*** BEGIN CS_ONLY ***
	[MethodImpl(AggressiveInlining)]
	private void SwitchFrame(Int32 currentFuncIndex, Int32 baseIndex,
			ref FuncDef curFunc, ref Int32 codeCount,
			ref List<UInt32> curCode, ref List<Value> curConstants,
			ref Span<Value> localStack) {
		curFunc = functions[currentFuncIndex];
		codeCount = curFunc.Code.Count;
		curCode = curFunc.Code;
		curConstants = curFunc.Constants;
		localStack = CollectionsMarshal.AsSpan(stack).Slice(baseIndex);
	}
	//*** END CS_ONLY ***
	// H: void SwitchFrame(Int32 currentFuncIndex, Int32 baseIndex, FuncDefStorage* &curFuncRaw, Int32 &codeCount, UInt32* &curCode, Value* &curConstants, Value* &localStack, Value* stackPtr);
	/*** BEGIN CPP_ONLY ***
	FORCE_INLINE void VMStorage::SwitchFrame(Int32 currentFuncIndex, Int32 baseIndex,
			FuncDefStorage* &curFuncRaw, Int32 &codeCount,
			UInt32* &curCode, Value* &curConstants,
			Value* &localStack, Value* stackPtr) {
		curFuncRaw = functions[currentFuncIndex].get_storage();
		codeCount = curFuncRaw->Code.Count();
		curCode = &curFuncRaw->Code[0];
		curConstants = &curFuncRaw->Constants[0];
		localStack = stackPtr + baseIndex;
	}
	*** END CPP_ONLY ***/

	// Get the globals VarMap.  In REPL mode, returns the persistent ReplGlobals.
	// Otherwise, lazily creates one from callStack[0].
	private Value GetGlobalsVarMap() {
		if (!is_null(ReplGlobals)) return ReplGlobals;
		CallInfo gframe = callStack[0];
		Int32 regCount;
		if (callStackTop == 0) {
			regCount = CurrentFunction.MaxRegs;
		} else {
			regCount = functions[gframe.ReturnFuncIndex].MaxRegs;
		}
		Value globalMap = gframe.GetLocalVarMap(stack, names, 0, regCount);
		callStack[0] = gframe;  // write back (CallInfo is a struct)
		return globalMap;
	}

	// Get or create a VarMap for the current call frame's local variables.
	// At global scope (callStackTop == 0), creates a VarMap directly.
	[MethodImpl(AggressiveInlining)]
	private Value GetCurrentLocalVarMap(Int32 baseIndex, UInt16 maxRegs) {
		Value result;
		if (callStackTop > 0) {
			CallInfo frame = callStack[callStackTop - 1];
			result = frame.GetLocalVarMap(stack, names, baseIndex, maxRegs);
			callStack[callStackTop - 1] = frame;  // write back (CallInfo is a struct)
			return result;
		} else {
			return make_varmap(stack, names, baseIndex, maxRegs); // CPP: return make_varmap(&stack[0], &names[0], baseIndex, maxRegs);
		}
	}

	[MethodImpl(AggressiveInlining)]
	private void SaveState(Int32 pc, Int32 baseIndex, Int32 currentFuncIndex) {
		PC = pc;
		BaseIndex = baseIndex;
		_currentFuncIndex = currentFuncIndex;
		CurrentFunction = functions[currentFuncIndex];
	}

	public Value LookupParamByName(String varName) {
		// Look up a parameter by name in the current frame.  This is provided
		// mainly for compatibility with 1.x; modern code should find parameters
		// by position, which is more efficient than searching by name.
		// Returns the value if found, or null if not found.
		FuncDef func = CurrentFunction;
		Value nameVal = make_string(varName);
		for (Int32 i = 0; i < func.ParamNames.Count; i++) {
			if (value_equal(func.ParamNames[i], nameVal)) {
				return stack[BaseIndex + 1 + i];
			}
		}
		return val_null;
	}

	private Value LookupVariable(Value varName) {
		// Look up a variable in outer context (and eventually globals)
		// Returns the value if found, or null if not found
		Value result;		
		if (callStackTop > 0) {
			CallInfo currentFrame = callStack[callStackTop - 1];  // Current frame, not next frame
			if (!is_null(currentFrame.OuterVarMap)) {
				if (map_try_get(currentFrame.OuterVarMap, varName, out result)) {
					return result;
				}
			}
		}

		// Check global variables via VarMap (registers at base 0 in the @main frame)
		Value globalMap;
		if (callStackTop > 0 || !is_null(ReplGlobals)) {
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
		if (value_equal(varName, val_self) || value_equal(varName, val_super)) {
			return val_null;
		}

		// Variable not found anywhere — raise an error
		RaiseRuntimeError(StringUtils.Format("Undefined Identifier: '{0}' is unknown in this context", varName));
		return val_null;
	}
}

}
