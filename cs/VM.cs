using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
// H: #include "value.h"
// H: #include "FuncDef.g.h"
// H: #include "ErrorPool.g.h"
// H: #include "value_map.h"
// CPP: #include "gc.h"
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

using static MiniScript.ValueHelpers;

namespace MiniScript {

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

	// Print callback: if set, print output goes here instead of IOHelper.Print
	//*** BEGIN CS_ONLY ***
	private Action<String> _printCallback = null;
	public void SetPrintCallback(Action<String> callback) { _printCallback = callback; }
	public Action<String> GetPrintCallback() { return _printCallback; }
	//*** END CS_ONLY ***
	// H: public: std::function<void(const String&)> _printCallback;
	// H: public: void SetPrintCallback(std::function<void(const String&)> cb) { _printCallback = cb; }
	// H: public: std::function<void(const String&)> GetPrintCallback() { return _printCallback; }

	// Static callback for C++ (accessible from VM wrapper)
	// H: public: static std::function<void(const String&)> sPrintCallback;
	// CPP: std::function<void(const String&)> VMStorage::sPrintCallback;

	private List<CallInfo> callStack;
	private Int32 callStackTop;	   // Index of next free call stack slot

	private List<FuncDef> functions; // functions addressed by CALLF
	private Dictionary<String, Value> _intrinsics; // intrinsic name -> FuncRef Value

	// Execution state (persistent across RunSteps calls)
	public Int32 PC { get; private set; }
	private Int32 _currentFuncIndex = -1;
	public FuncDef CurrentFunction { get; private set; }
	public Boolean IsRunning { get; private set; }
	public Int32 BaseIndex { get; private set; }
	public String RuntimeError { get; private set; }
	public ErrorPool Errors;

	// Pending self/super for method calls, set by METHFIND/SETSELF,
	// consumed by the next CALL instruction
	private Value pendingSelf;
	private Value pendingSuper;
	private bool hasPendingContext;

	// Thread-local active VM: set during Run(), so value operations
	// (like list_push) can report errors without passing ErrorPool around.
	[ThreadStatic] private static VM _activeVM;
	public static VM ActiveVM() {
		return _activeVM;
	}

	public Int32 StackSize() {
		return stack.Count;
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
	}	
	*** END CPP_ONLY ***/

	public void RegisterFunction(FuncDef funcDef) {
		functions.Add(funcDef);
	}


	public void Reset(List<FuncDef> allFunctions) {
		// Store all functions for CALLF instructions, and find @main
		FuncDef mainFunc = null;
		functions.Clear();
		for (Int32 i = 0; i < allFunctions.Count; i++) {
			functions.Add(allFunctions[i]);
			if (functions[i].Name == "@main") mainFunc = functions[i];
		}

		_intrinsics = new Dictionary<String, Value>();
		Intrinsic.RegisterAll(functions, _intrinsics);

		if (!mainFunc) {
			IOHelper.Print("ERROR: No @main function found in VM.Reset");
			return;
		}

		// Basic validation - simplified for C++ transpilation
		if (mainFunc.Code.Count == 0) {
			IOHelper.Print("Entry function has no code");
			return;
		}

		// Find the entry function index
		_currentFuncIndex = -1;
		for (Int32 i = 0; i < functions.Count; i++) {
			if (functions[i].Name == mainFunc.Name) {
				_currentFuncIndex = i;
				break;
			}
		}

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

		if (DebugMode) {
			IOHelper.Print(StringUtils.Format("VM Reset: Executing {0} out of {1} functions", mainFunc.Name, functions.Count));
		}
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

	// Helper for argument processing (FUNCTION_CALLS.md steps 1-3):
	// Process ARG instructions, validate argument count, and set up parameter registers.
	// Returns the PC after the CALL instruction, or -1 on error.
	// Check whether pendingSelf should be injected as the first parameter.
	// Returns 1 if the callee's first param is named "self" and we have pending context, else 0.
	private Int32 SelfParamOffset(FuncDef callee) {
		if (hasPendingContext && callee.ParamNames.Count > 0 && value_equal(callee.ParamNames[0], val_self)) {
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
	private void ApplyPendingContext(Int32 calleeBase, FuncDef callee) {
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
	private void SetupCallFrame(Int32 argCount, Int32 selfParam, Int32 calleeBase, FuncDef callee) {
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
	//   - Native callback: invokes it, stores result at stack[baseIndex + resultReg], returns -1.
	//   - User function: pushes CallInfo and sets up callee frame, returns the callee function index.
	//     Caller must switch its local execution state (pc, baseIndex, curFunc, etc.).
	// On error, calls RaiseRuntimeError and returns -1.
	private Int32 AutoInvokeFuncRef(Value funcRefVal, Int32 resultReg, Int32 returnPC, Int32 baseIndex, Int32 currentFuncIndex, FuncDef curFunc) {
		Int32 funcIndex = funcref_index(funcRefVal);
		if (funcIndex < 0 || funcIndex >= functions.Count) {
			RaiseRuntimeError("Auto-invoke: Invalid function index");
			return -1;
		}

		FuncDef callee = functions[funcIndex];
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
			stack[baseIndex + resultReg] = callee.NativeCallback(stack, calleeBase, selfParam);
			return -1;
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
		Value runResult = RunInner(maxCycles);
		_activeVM = previousVM;
		return runResult;
	}

	private Value RunInner(UInt32 maxCycles) {
		// Copy instance variables to locals for performance
		Int32 pc = PC;
		Int32 baseIndex = BaseIndex;
		Int32 currentFuncIndex = _currentFuncIndex;

		FuncDef curFunc = CurrentFunction;
		Int32 codeCount = curFunc.Code.Count;
		var curCode = curFunc.Code; // CPP: UInt32* curCode = &curFunc.Code()[0];
		var curConstants = curFunc.Constants; // CPP: Value* curConstants = &curFunc.Constants()[0];

		UInt32 cyclesLeft = maxCycles;
		if (maxCycles == 0) cyclesLeft--;  // wraps to MAX_UINT32

		// Reusable Value variables (declared outside loop for GC safety in C++)
		// ToDo: see if we can reduce these to a more reasonable number.
		Value val = val_null;
		Value outerVars = val_null;
		Value container = val_null;
		Value indexVal = val_null;
		Value result = val_null;
		Value valueArg = val_null;
		Value funcRefValue = val_null;
		Value funcName = val_null;
		Value expectedName = val_null;
		Value actualName = val_null;
		Value locals = val_null;
		Value lhs;
		Value rhs;
		Value current;
		Value next;
		Value superVal;
		Value startVal;
		Value endVal;
		Value typeMap;

/*** BEGIN CPP_ONLY ***
		Value* stackPtr = &stack[0];
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
				// Update instance variables before returning
				PC = pc;
				BaseIndex = baseIndex;
				_currentFuncIndex = currentFuncIndex;
				CurrentFunction = curFunc;
				return val_null;
			}
			cyclesLeft--;

			if (pc >= codeCount) {
				IOHelper.Print("VM: PC out of bounds");
				IsRunning = false;
				// Update instance variables before returning
				PC = pc;
				BaseIndex = baseIndex;
				_currentFuncIndex = currentFuncIndex;
				CurrentFunction = curFunc;
				return val_null;
			}

			UInt32 instruction = curCode[pc++];
			// Note: CollectionsMarshal.AsSpan requires .NET 5+; not compatible with Mono.
			// This gives us direct array access without copying, for performance.
			Span<Value> localStack = CollectionsMarshal.AsSpan(stack).Slice(baseIndex); // CPP: Value* localStack = stackPtr + baseIndex;

			if (DebugMode) {
				// Debug output disabled for C++ transpilation
				IOHelper.Print(StringUtils.Format("{0} {1}: {2}     r0:{3}, r1:{4}, r2:{5}",
					curFunc.Name,
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
					expectedName = curConstants[c];
					actualName = names[baseIndex + b];
					if (value_identical(expectedName, actualName)) {
						localStack[a] = localStack[b];
					} else {
						// Variable not found in current scope, look in outer context
						localStack[a] = LookupVariable(expectedName);
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
					expectedName = curConstants[c];
					actualName = names[baseIndex + b];
					if (value_identical(expectedName, actualName)) {
						val = localStack[b];
					} else {
						// Variable not found in current scope, look in outer context
						val = LookupVariable(expectedName);
					}

					if (!is_funcref(val)) {
						// Simple case: value is not a funcref, so just copy it
						localStack[a] = val;
					} else {
						// Value is a funcref — auto-invoke with zero args
						Int32 calleeIdx = AutoInvokeFuncRef(val, a, pc, baseIndex, currentFuncIndex, curFunc);
						if (calleeIdx >= 0) {
							// Frame was pushed — switch to callee
							baseIndex += curFunc.MaxRegs;
							pc = 0;
							currentFuncIndex = calleeIdx;
							curFunc = functions[calleeIdx];
							codeCount = curFunc.Code.Count;
							curCode = curFunc.Code; // CPP: curCode = &curFunc.Code()[0];
							curConstants = curFunc.Constants; // CPP: curConstants = &curFunc.Constants()[0];
						}
					}
					break;
				}

				case Opcode.FUNCREF_iA_iBC: {
					// R[A] := make_funcref(BC) with closure context
					Byte a = BytecodeUtil.Au(instruction);
					Int16 funcIndex = BytecodeUtil.BCs(instruction);

					// Create function reference with our locals as the closure context
					CallInfo frame = callStack[callStackTop];
					locals = frame.GetLocalVarMap(stack, names, baseIndex, curFunc.MaxRegs);
					callStack[callStackTop] = frame;  // write back (CallInfo is a struct)
					localStack[a] = make_funcref(funcIndex, locals);
					break;
				}

				case Opcode.ASSIGN_rA_rB_kC: {
					// R[A] = R[B] and names[baseIndex + A] = constants[C]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					localStack[a] = localStack[b];
					names[baseIndex + a] = curConstants[c];	// OFI: keep localNames?
					break;
				}

				case Opcode.NAME_rA_kBC: {
					// names[baseIndex + A] = constants[BC] (without changing R[A])
					Byte a = BytecodeUtil.Au(instruction);
					UInt16 constIdx = BytecodeUtil.BCu(instruction);
					names[baseIndex + a] = curConstants[constIdx];	// OFI: keep localNames?
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
					container = localStack[b];
					indexVal = localStack[c];

					if (is_list(container)) {
						// ToDo: add a list_try_get and use it here, like we do with map below
						localStack[a] = list_get(container, as_int(indexVal));
					} else if (is_map(container)) {
						if (!map_lookup(container, indexVal, out result)) {
							RaiseRuntimeError(StringUtils.Format("Key Not Found: '{0}' not found in map", indexVal));
						}
						localStack[a] = result;
					} else if (is_string(container)) {
						Int32 idx = as_int(indexVal);
						localStack[a] = string_substring(container, idx, 1);
					} else {
						RaiseRuntimeError(StringUtils.Format("Can't index into {0}", container));
						localStack[a] = val_null;
					}
					break;
				}

				case Opcode.IDXSET_rA_rB_rC: {
					// R[A][R[B]] = R[C] (supports both lists and maps)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					container = localStack[a];
					indexVal = localStack[b];
					valueArg = localStack[c];

					if (is_list(container)) {
						list_set(container, as_int(indexVal), valueArg);
					} else if (is_map(container)) {
						map_set(container, indexVal, valueArg);
					} else {
						RaiseRuntimeError(StringUtils.Format("Can't set indexed value in {0}", container));
					}
					break;
				}

				case Opcode.SLICE_rA_rB_rC: {
					// R[A] = R[B][R[C]:R[C+1]] (slice; end index in adjacent register)
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					container = localStack[b];
					startVal = localStack[c];
					endVal = localStack[c + 1];

					if (is_string(container)) {
						Int32 len = string_length(container);
						Int32 startIdx = is_null(startVal) ? 0 : as_int(startVal);
						Int32 endIdx = is_null(endVal) ? len : as_int(endVal);
						localStack[a] = string_slice(container, startIdx, endIdx);
					} else if (is_list(container)) {
						Int32 len = list_count(container);
						Int32 startIdx = is_null(startVal) ? 0 : as_int(startVal);
						Int32 endIdx = is_null(endVal) ? len : as_int(endVal);
						localStack[a] = list_slice(container, startIdx, endIdx);
					} else {
						RaiseRuntimeError(StringUtils.Format("Can't slice {0}", container));
						localStack[a] = val_null;
					}
					break;
				}

				case Opcode.LOCALS_rA: {
					// Create VarMap for local variables and store in R[A]
					Byte a = BytecodeUtil.Au(instruction);

					CallInfo frame = callStack[callStackTop];
					localStack[a] = frame.GetLocalVarMap(stack, names, baseIndex, curFunc.MaxRegs);
					callStack[callStackTop] = frame;  // write back (CallInfo is a struct)
					names[baseIndex+a] = val_null;
					break;
				}

				case Opcode.OUTER_rA: {
					// Create VarMap for outer variables and store in R[A]
					// TODO: Implement outer variable map access
					Byte a = BytecodeUtil.Au(instruction);
					CallInfo frame = callStack[callStackTop - 1];
					localStack[a] = frame.OuterVarMap;
					names[baseIndex+a] = val_null;
					break;
				}

				case Opcode.GLOBALS_rA: {
					// Create VarMap for global variables and store in R[A]
					// TODO: Implement global variable map access
					Byte a = BytecodeUtil.Au(instruction);
					Int32 globalRegCount = functions[callStack[0].ReturnFuncIndex].MaxRegs;
					localStack[a] = callStack[0].GetLocalVarMap(stack, names, 0, globalRegCount);
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
					container = localStack[b];
					bool hasMore;
					if (is_list(container)) {
						iter++;
						hasMore = (iter < list_count(container));
					} else if (is_map(container)) {
						iter = map_iter_next(container, iter);
						hasMore = (iter != MAP_ITER_DONE);
					} else if (is_string(container)) {
						iter++;
						hasMore = (iter < string_length(container));
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

					// Extract call parameters based on opcode type
					FuncDef callee;
					Int32 calleeBase = 0;
					Int32 resultReg = -1;

					if (callOp == Opcode.CALL_rA_rB_rC) {
						// CALL r[A], r[B], r[C]: invoke funcref in r[C], frame at r[B], result to r[A]
						Byte a = BytecodeUtil.Au(callInstruction);
						Byte b = BytecodeUtil.Bu(callInstruction);
						Byte c = BytecodeUtil.Cu(callInstruction);

						funcRefValue = localStack[c];
						if (!is_funcref(funcRefValue)) {
							RaiseRuntimeError("ARGBLK/CALL: Not a function reference");
							return val_null;
						}

						Int32 funcIndex = funcref_index(funcRefValue);
						if (funcIndex < 0 || funcIndex >= functions.Count) {
							RaiseRuntimeError("ARGBLK/CALL: Invalid function index");
							return val_null;
						}
						callee = functions[funcIndex];
						calleeBase = baseIndex + b;
						resultReg = a;
					} else {
						RaiseRuntimeError("ARGBLK must be followed by CALL");
						return val_null;
					}

					// Check for self-injection: if pending context and first param is "self",
					// inject pendingSelf as the first argument
					Int32 selfParam = SelfParamOffset(callee);
					Int32 nextPC = ProcessArguments(argCount, selfParam, pc, baseIndex, calleeBase, callee, curFunc.Code);
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
						localStack[resultReg] = callee.NativeCallback(stack, calleeBase, argCount + selfParam);
						pc = nextPC;
						break;
					}

					// Now execute the CALL (step 6): push CallInfo and switch to callee
					if (callStackTop >= callStack.Count) {
						RaiseRuntimeError("Call stack overflow");
						return val_null;
					}

					Int32 funcIndex2 = funcref_index(localStack[BytecodeUtil.Cu(callInstruction)]);
					outerVars = funcref_outer_vars(localStack[BytecodeUtil.Cu(callInstruction)]);
					callStack[callStackTop] = new CallInfo(nextPC, baseIndex, currentFuncIndex, resultReg, outerVars);
					callStackTop++;

					baseIndex = calleeBase;
					pc = 0; // Start at beginning of callee code
					curFunc = callee; // Switch to callee function
					codeCount = curFunc.Code.Count;
					curCode = curFunc.Code; // CPP: curCode = &curFunc.Code()[0];
					curConstants = curFunc.Constants; // CPP: curConstants = &curFunc.Constants()[0];
					currentFuncIndex = funcIndex2;
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
						IOHelper.Print("CALLF to invalid func");
						return val_null;
					}
					
					FuncDef callee = functions[funcIndex];

					// Push return info
					if (callStackTop >= callStack.Count) {
						IOHelper.Print("Call stack overflow");
						return val_null;
					}
					callStack[callStackTop] = new CallInfo(pc, baseIndex, currentFuncIndex);
					callStackTop++;

					// Switch to callee frame: base slides to argument window
					baseIndex += a;
					ApplyPendingContext(baseIndex, callee);
					pc = 0; // Start at beginning of callee code
					curFunc = callee; // Switch to callee function
					codeCount = curFunc.Code.Count;
					curCode = curFunc.Code; // CPP: curCode = &curFunc.Code()[0];
					curConstants = curFunc.Constants; // CPP: curConstants = &curFunc.Constants()[0];
					currentFuncIndex = funcIndex; // Switch to callee function index

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

					funcRefValue = localStack[c];
					if (!is_funcref(funcRefValue)) {
						IOHelper.Print("CALL: Value in register is not a function reference");
						localStack[a] = funcRefValue;
						break;
					}

					Int32 funcIndex = funcref_index(funcRefValue);
					if (funcIndex < 0 || funcIndex >= functions.Count) {
						IOHelper.Print("CALL: Invalid function index in FuncRef");
						return val_null;
					}
					FuncDef callee = functions[funcIndex];
					outerVars = funcref_outer_vars(funcRefValue);

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
						localStack[a] = callee.NativeCallback(stack, calleeBase, selfParam);
						break;
					}

					if (callStackTop >= callStack.Count) {
						IOHelper.Print("Call stack overflow");
						return val_null;
					}
					callStack[callStackTop] = new CallInfo(pc, baseIndex, currentFuncIndex, a, outerVars);
					callStackTop++;

					// Set up call frame starting at baseIndex + b
					baseIndex = calleeBase;
					pc = 0; // Start at beginning of callee code
					curFunc = callee; // Switch to callee function
					codeCount = curFunc.Code.Count;
					curCode = curFunc.Code; // CPP: curCode = &curFunc.Code()[0];
					curConstants = curFunc.Constants; // CPP: curConstants = &curFunc.Constants()[0];
					currentFuncIndex = funcIndex; // Switch to callee function index
					EnsureFrame(baseIndex, callee.MaxRegs);
					break;
				}

				case Opcode.NEW_rA_rB: {
					// R[A] = new map with __isa set to R[B]
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					result = make_map(2);
					map_set(result, val_isa_key, localStack[b]);
					localStack[a] = result;
					break;
				}

				case Opcode.ISA_rA_rB_rC: { // CPP: VM_CASE(ISA_rA_rB_rC) {
					// R[A] = (R[B] isa R[C])
					// True if R[B] and R[C] are the same reference, or if R[C]
					// appears anywhere in R[B]'s __isa chain.
					Byte a = BytecodeUtil.Au(instruction);
					Byte b = BytecodeUtil.Bu(instruction);
					Byte c = BytecodeUtil.Cu(instruction);
					lhs = localStack[b];
					rhs = localStack[c];
					Int32 isaResult = 0;
					if (value_identical(lhs, rhs)) {
						isaResult = 1;
					} else {
						current = lhs;
						for (Int32 depth = 0; depth < 256; depth++) {
							if (!map_try_get(current, val_isa_key, out next)) break;
							if (value_identical(next, rhs)) {
								isaResult = 1;
								break;
							}
							current = next;
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
					container = localStack[b];
					indexVal = localStack[c];
					typeMap = val_null;
					
					if (is_map(container)) {
						// For maps: first do lookup in the map itself, with inheritance
						if (map_lookup_with_origin(container, indexVal, out result, out superVal)) {
							localStack[a] = result;
							pendingSelf = container;
							pendingSuper = superVal;
							hasPendingContext = true;
							break; // CPP: VM_NEXT();
						}
						// ...falling back on the map type map
						typeMap = CoreIntrinsics.MapType();
					} else if (is_list(container)) {
						typeMap = CoreIntrinsics.ListType();
					} else if (is_string(container)) {
						typeMap = CoreIntrinsics.StringType();
					} else if (is_number(container)) {
						typeMap = CoreIntrinsics.NumberType();
					}
					if (is_null(typeMap)) {
						// If we didn't get a type map, then user is trying to index
						// into something not indexable
						RaiseRuntimeError(StringUtils.Format("Can't index into {0}", container));
						localStack[a] = val_null;
					} else if (map_try_get(typeMap, indexVal, out result)) {
						// found what we're looking for in the type map
						localStack[a] = result;
						pendingSelf = container;
						pendingSuper = val_null;
					} else if (is_number(indexVal)) {
						// try indexing numerically
						int index = as_int(indexVal);
						if (is_list(container)) {
							localStack[a] = list_get(container, index);
						} else if (is_string(container)) {
							localStack[a] = string_substring(container, index, 1);
						} else {
							RaiseRuntimeError(StringUtils.Format("Can't index into {0}", container));
							localStack[a] = val_null;
						}
					} else {
						RaiseRuntimeError(StringUtils.Format("Key Not Found: '{0}' not found in map", indexVal));
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
					Int32 calleeIdx = AutoInvokeFuncRef(val, a, pc, baseIndex, currentFuncIndex, curFunc);
					if (calleeIdx >= 0) {
						// Frame was pushed — switch to callee
						baseIndex += curFunc.MaxRegs;
						pc = 0;
						currentFuncIndex = calleeIdx;
						curFunc = functions[calleeIdx];
						codeCount = curFunc.Code.Count;
						curCode = curFunc.Code; // CPP: curCode = &curFunc.Code()[0];
						curConstants = curFunc.Constants; // CPP: curConstants = &curFunc.Constants()[0];
					}
					break; // CPP: VM_NEXT();
				}

				case Opcode.RETURN: {
					// Return value convention: value is in base[0]
					result = stack[baseIndex];
					if (callStackTop == 0) {
						// Returning from main function: update instance vars and set IsRunning = false
						PC = pc;
						BaseIndex = baseIndex;
						_currentFuncIndex = currentFuncIndex;
						CurrentFunction = curFunc;
						IsRunning = false;
						return result;
					}
					
					// If current call frame had a locals VarMap, gather it up
					CallInfo frame = callStack[callStackTop];
					if (!is_null(frame.LocalVarMap)) {
						varmap_gather(frame.LocalVarMap);
						frame.LocalVarMap = val_null;  // then clear from call frame
					}

					// Pop call stack
					callStackTop--;
					CallInfo callInfo = callStack[callStackTop];
					pc = callInfo.ReturnPC;
					baseIndex = callInfo.ReturnBase;
					currentFuncIndex = callInfo.ReturnFuncIndex; // Restore the caller's function index
					curFunc = functions[currentFuncIndex]; // Restore the caller's function
					codeCount = curFunc.Code.Count;
					curCode = curFunc.Code; // CPP: curCode = &curFunc.Code()[0];
					curConstants = curFunc.Constants; // CPP: curConstants = &curFunc.Constants()[0];
					
					if (callInfo.CopyResultToReg >= 0) {
						stack[baseIndex + callInfo.CopyResultToReg] = result;
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
					container = localStack[b];
					Int32 idx = as_int(localStack[c]);

					if (is_list(container)) {
						localStack[a] = list_get(container, idx);
					} else if (is_map(container)) {
						localStack[a] = map_iter_entry(container, idx);
					} else if (is_string(container)) {
						localStack[a] = string_substring(container, idx, 1);
					} else {
						localStack[a] = val_null;
					}
					break;
				}

				// CPP: VM_DISPATCH_END();
//*** BEGIN CS_ONLY ***
				default:
					IOHelper.Print("Unknown opcode");
					// Update instance variables before returning
					PC = pc;
					BaseIndex = baseIndex;
					_currentFuncIndex = currentFuncIndex;
					CurrentFunction = curFunc;
					return val_null;
			}
//*** END CS_ONLY ***
		}
		// CPP: VM_DISPATCH_BOTTOM();

		// Update instance variables after loop exit (e.g. from error condition)
		PC = pc;
		BaseIndex = baseIndex;
		_currentFuncIndex = currentFuncIndex;
		CurrentFunction = curFunc;
		return val_null;
	}

	private void EnsureFrame(Int32 baseIndex, UInt16 neededRegs) {
		// Simple implementation - just check bounds
		if (baseIndex + neededRegs > stack.Count) {
			// Simple error handling - just print and continue
			IOHelper.Print("Stack overflow error");
		}
	}

	private Value LookupVariable(Value varName) {
		// Look up a variable in outer context (and eventually globals)
		// Returns the value if found, or null if not found
		Value outerValue;
		if (callStackTop > 0) {
			CallInfo currentFrame = callStack[callStackTop - 1];  // Current frame, not next frame
			if (!is_null(currentFrame.OuterVarMap)) {
				if (map_try_get(currentFrame.OuterVarMap, varName, out outerValue)) {
					return outerValue;
				}
			}
		}

		// ToDo: check globals!

		// Check intrinsics table
		Value intrinsicRef;
		String nameStr = as_cstring(varName);
		if (_intrinsics.TryGetValue(nameStr, out intrinsicRef)) {
			return intrinsicRef;
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

//*** END CS_ONLY ***