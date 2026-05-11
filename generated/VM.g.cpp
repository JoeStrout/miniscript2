// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VM.cs

#include "VM.g.h"
#include "value_list.h"
#include "value_string.h"
#include "Bytecode.g.h"
#include "Intrinsic.g.h"
#include "CoreIntrinsics.g.h"
#include "IOHelper.g.h"
#include "Disassembler.g.h"
#include "StringUtils.g.h"
#include "dispatch_macros.h"
#include "vm_error.h"
#include "Interpreter.g.h"
#include <chrono>

namespace MiniScript {

CallInfo::CallInfo(Int32 returnPC,Int32 returnBase,Int32 returnFuncIndex,Int32 copyToReg) {
	ReturnPC = returnPC;
	ReturnBase = returnBase;
	ReturnFuncIndex = returnFuncIndex;
	CopyResultToReg = copyToReg;
	LocalVarMap = val_null;
	OuterVarMap = val_null;
}
CallInfo::CallInfo(Int32 returnPC,Int32 returnBase,Int32 returnFuncIndex,Int32 copyToReg,Value outerVars) {
	ReturnPC = returnPC;
	ReturnBase = returnBase;
	ReturnFuncIndex = returnFuncIndex;
	CopyResultToReg = copyToReg;
	LocalVarMap = val_null;
	OuterVarMap = outerVars;
}
Value CallInfo::GetLocalVarMap(List<Value> registers,List<Value> names,int baseIdx,int regCount) {
	if (is_null(LocalVarMap)) {
		// Create a new VarMap with references to VM's stack and names arrays
		if (regCount == 0) {
			// We have no local vars at all!  Make an ordinary map.
			LocalVarMap = make_map(4);	// This is safe, right?
		} else {
			LocalVarMap = make_varmap(&registers[0], &names[0], baseIdx, regCount);
		}
	}
	return LocalVarMap;
}

void VM::SetInterpreter(Interpreter interp) { return get()->SetInterpreter(interp); } // NO_INLINE
void VMStorage::SetInterpreter(Interpreter interp) { // NO_INLINE
	interpreter = interp.get_storage();
}
Interpreter VM::GetInterpreter() { return get()->GetInterpreter(); } // NO_INLINE
Interpreter VMStorage::GetInterpreter() { // NO_INLINE
	return Interpreter(interpreter);
}
double VMStorage::ElapsedTime() {
	auto now = std::chrono::steady_clock::now();
	return std::chrono::duration<double>(now - _startTime).count();
}
	thread_local VM VMStorage::_activeVM;
VM VMStorage::ActiveVM() {
	return _activeVM;
}
Int32 VMStorage::StackSize() {
	return stack.Count();
}
List<Value> VMStorage::GetStack() {
	return stack;
}
List<Value> VMStorage::GetNames() {
	return names;
}
Int32 VMStorage::CallStackDepth() {
	return callStackTop;
}
Value VMStorage::GetStackValue(Int32 index) {
	if (index < 0 || index >= stack.Count()) return val_null;
	return stack[index];
}
Value VMStorage::GetStackName(Int32 index) {
	if (index < 0 || index >= names.Count()) return val_null;
	return names[index];
}
CallInfo VMStorage::GetCallStackFrame(Int32 index) {
	if (index < 0 || index >= callStackTop) return CallInfo(0, 0, -1);
	return callStack[index];
}
String VMStorage::GetFunctionName(Int32 funcIndex) {
	if (funcIndex < 0 || funcIndex >= functions.Count()) return "???";
	return functions[funcIndex].Name();
}
FuncDef VMStorage::GetFuncDef(Int32 funcIndex) {
	if (funcIndex < 0 || funcIndex >= functions.Count()) return  FuncDef::New();
	return functions[funcIndex];
}
String VMStorage::FindShortName(Value v) {
	// Search global variable names directly on the stack — allocation-free and GC-safe.
	Int32 regCount = functions[callStack[0].ReturnFuncIndex].MaxRegs();
	for (Int32 i = 0; i < regCount; i++) {
		if (!is_null(names[i]) && value_identical(stack[i], v) && !value_identical(names[i], v))
			return as_cstring(names[i]);
	}
	// Fall back to the intrinsic short-name registry (type maps, etc.)
	return Intrinsic::GetShortName(v);
}
VMStorage::VMStorage(Int32 stackSlots,Int32 callSlots) {
	InitVM(stackSlots, callSlots);
}
void VMStorage::InitVM(Int32 stackSlots,Int32 callSlots) {
	stack =  List<Value>::New();
	names =  List<Value>::New();
	callStack =  List<CallInfo>::New();
	functions =  List<FuncDef>::New();
	callStackTop = 0;
	Error = val_null;

	// Initialize stack with null values
	for (Int32 i = 0; i < stackSlots; i++) {
		stack.Add(val_null);
		names.Add(val_null);		// No variable name initially
	}
	
	
	// Pre-allocate call stack capacity
	for (Int32 i = 0; i < callSlots; i++) {
		callStack.Add(CallInfo(0, 0, -1)); // -1 = invalid function index
	}
	
	// Register as a source of roots for the GC system
	GCManager::Instance().RegisterMarkCallback(VMStorage::MarkRoots, this);

	// And, ensure that runtime errors are routed through the active VM
	vm_error_set_callback([](const char* msg) {
		VM vm = VMStorage::ActiveVM();
		if (!IsNull(vm)) vm.RaiseRuntimeError(String(msg));
	});

	// Wire code_form's short-name hook to this VM's FindShortName. The
	// hook lives in value.cpp (layer 2A) and takes a void* vm so it can
	// stay free of the VM layer; we cast back here.
	set_short_name_lookup([](void* vm_ptr, Value v) -> Value {
		String s = static_cast<VMStorage*>(vm_ptr)->FindShortName(v);
		if (s.Length() == 0) return val_null;
		return make_string(s.c_str());
	});
}
void VMStorage::CleanupVM() {
	GCManager::Instance().UnregisterMarkCallback(VMStorage::MarkRoots, this);
}
// GC mark callback responsible for protecting our stack and names
// from garbage collection
void VMStorage::MarkRoots(void* user_data, GCManager& gc) {
	VMStorage* vm = static_cast<VMStorage*>(user_data);
	for (int i = 0; i < vm->stack.Count(); i++) {
		gc.Mark(vm->stack[i]);
		gc.Mark(vm->names[i]);
	}
	// Mark intrinsics dictionary values (funcrefs are GC-allocated)
	if (!IsNull(vm->_intrinsics)) {
		for (Value val : vm->_intrinsics.GetValues()) {
			gc.Mark(val);
		}
	}
}
void VMStorage::RegisterFunction(FuncDef funcDef) {
	functions.Add(funcDef);
}
void VMStorage::Reset(List<FuncDef> allFunctions) {
	Reset(allFunctions, val_null);
}
void VMStorage::Reset(List<FuncDef> allFunctions,Value replGlobals) {
	bool partialReset = !is_null(replGlobals);

	Int32 mainIdx = -1;
	if (partialReset) {
		// REPL mode: intrinsics and previous user functions are already in the list.
		// Just append the newly compiled functions (which were compiled with an
		// offset matching the current functions.Count, so indices line up).
		for (Int32 i = 0; i < allFunctions.Count(); i++) {
			if (allFunctions[i].Name() == "@main") mainIdx = functions.Count();
			functions.Add(allFunctions[i]);
		}
	} else {
		// Full reset: register intrinsics first, then add user functions.
		// User functions are compiled with FunctionIndexOffset = intrinsic count,
		// so their indices in the bytecode already account for the intrinsics.
		functions.Clear();
		_intrinsics =  Dictionary<String, Value>::New();
		Intrinsic::RegisterAll(functions, _intrinsics);
		for (Int32 i = 0; i < allFunctions.Count(); i++) {
			if (allFunctions[i].Name() == "@main") mainIdx = functions.Count();
			functions.Add(allFunctions[i]);
		}
	}
	
	// Basic validation
	if (mainIdx < 0) {
		IOHelper::Print("ERROR: No @main function found in VM.Reset");
		return;
	}
	FuncDef mainFunc = functions[mainIdx];
	_currentFuncIndex = mainIdx;

	if (mainFunc.Code().Count() == 0) {
		IOHelper::Print("Entry function has no code");
		return;
	}

	// C++ only: copy functions into functionsRaw vector for quick access
	functionsRaw.clear();
	functionsRaw.reserve(functions.Count());
	for (Int32 i = 0; i < functions.Count(); i++) {
		functionsRaw.push_back(functions[i].get_storage());
	}

	// Initialize execution state
	BaseIndex = 0;			  // entry executes at stack base
	PC = 0;				 // start at entry code
	CurrentFunction = mainFunc;
	IsRunning = Boolean(true);
	callStackTop = 0;
	// Push @main's own execution-context frame at callStack[0] so that
	// globals (= @main's locals) are always accessible via callStack[0].
	// This means real function calls start at callStack[1], callStack[2], etc.
	// ReturnFuncIndex is set to mainIdx so GetGlobalsVarMap can find @main's MaxRegs.
	callStack[0] = CallInfo(0, 0, mainIdx);
	callStackTop = 1;
	Error = val_null;
	pendingSelf = val_null;
	pendingSuper = val_null;
	hasPendingContext = Boolean(false);

	EnsureFrame(BaseIndex, CurrentFunction.MaxRegs());

	// In REPL mode, rebind the persistent globals VarMap to the new stack arrays
	if (partialReset) {
		ReplGlobals = replGlobals;
		Int32 capacity = stack.Count();
		// careful: don't release old stacks until after varmap_rebind (below)
		List<Value> newStack =  List<Value>::New(capacity);	
		List<Value> newNames =  List<Value>::New(capacity);
		for (int i=0; i<capacity; i++) {
			newStack.Add(val_null);
			newNames.Add(val_null);
		}
		varmap_rebind(ReplGlobals, &newStack[0], &newNames[0]);
		stack = newStack;
		names = newNames;
	}

	// Start the run timer (e.g. for the `time` intrinsic)
	_startTime = std::chrono::steady_clock::now();

	if (DebugMode) {
		IOHelper::Print(StringUtils::Format("VM Reset: Executing {0} out of {1} functions", mainFunc.Name(), functions.Count()));
	}
}
void VMStorage::Stop() {
	IsRunning = Boolean(false);
}
void VMStorage::RaiseRuntimeError(String message) {
	Value stack = (IsRunning && CurrentFunction) ? BuildStackTrace() : val_null;
	Error = ErrorType::RuntimeError(message, stack);
	IsRunning = Boolean(false);
}
void VMStorage::RaiseRuntimeError(Value error) {
	Error = error;
	IsRunning = Boolean(false);
}
IntrinsicResult VMStorage::RaiseUncaughtError(Value error) {
	String msg = StringUtils::Format("Uncaught {0}", error_message(error));
	RaiseRuntimeError(msg);
	return IntrinsicResult::Null;
}
bool VMStorage::ReportRuntimeError() {
	if (is_null(Error)) return Boolean(false);
	IOHelper::Print(StringUtils::Format("Runtime Error: {0}", StringUtils::Format("{0}", error_message(Error))));
	return Boolean(true);
}
Value VMStorage::BuildStackTrace() {
	Value result = make_list(8);
	Int32 callSitePC = PC - 1;
	if (callSitePC < 0) callSitePC = 0;
	String curFile = CurrentFunction.FileName();
	if (curFile == "") curFile = "(current program)";
	list_push(result, make_string(StringUtils::Format("{0} line {1}", curFile, CurrentFunction.GetLineNumber(callSitePC))));
	// callStack[0] is @main's own frame (not a caller), so stop at i=1.
	for (Int32 i = CallStackDepth() - 1; i >= 1; i--) {
		CallInfo ci = GetCallStackFrame(i);
		FuncDef callerFunc = GetFuncDef(ci.ReturnFuncIndex);
		Int32 callerPC = ci.ReturnPC - 1;
		if (callerPC < 0) callerPC = 0;
		String callerFile = callerFunc.FileName();
		if (callerFile == "") callerFile = "(current program)";
		list_push(result, make_string(StringUtils::Format("{0} line {1}", callerFile, callerFunc.GetLineNumber(callerPC))));
	}
	freeze_value(result);
	return result;
}
Int32 VMStorage::FunctionCount() {
	return functions.Count();
}
List<FuncDef> VMStorage::GetFunctions() {
	return functions;
}
Int32 VMStorage::SelfParamOffset(FuncDefRef callee) {
	if (hasPendingContext && callee.ParamNames.Count() > 0 && value_equal(callee.ParamNames[0], val_self)) {
		return 1;
	}
	return 0;
}
Int32 VMStorage::ProcessArguments(Int32 argCount,Int32 selfParam,Int32 startPC,Int32 callerBase,Int32 calleeBase,FuncDefRef callee,List<UInt32> code) {
	Int32 paramCount = callee.ParamNames.Count();

	// Step 1: Validate argument count (selfParam accounts for the injected self)
	if (argCount + selfParam > paramCount) {
		RaiseRuntimeError(StringUtils::Format("Too many arguments: got {0}, expected {1}",
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
		Opcode argOp = (Opcode)BytecodeUtil::OP(argInstruction);

		argValue = val_null;
		if (argOp == Opcode::ARG_rA) {
			// Argument from register
			Byte srcReg = BytecodeUtil::Au(argInstruction);
			argValue = stack[callerBase + srcReg];
		} else if (argOp == Opcode::ARG_iABC) {
			// Argument immediate value
			Int32 immediate = BytecodeUtil::ABCs(argInstruction);
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
void VMStorage::ApplyPendingContext(Int32 calleeBase,FuncDefRef callee) {
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
	hasPendingContext = Boolean(false);
}
void VMStorage::SetupCallFrame(Int32 argCount,Int32 selfParam,Int32 calleeBase,FuncDefRef callee) {
	Int32 paramCount = callee.ParamNames.Count();

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
Int32 VMStorage::AutoInvokeFuncRef(Value funcRefVal,Int32 resultReg,Int32 returnPC,Int32 baseIndex,Int32 currentFuncIndex,FuncDefRef curFunc) {
	Int32 funcIndex = funcref_index(funcRefVal);
	if (funcIndex < 0 || funcIndex >= functions.Count()) {
		RaiseRuntimeError("Auto-invoke: Invalid function index");
		return -1;
	}

	FuncDefRef callee =
	   *functionsRaw[funcIndex];
	Value outerVars = funcref_outer_vars(funcRefVal);

	Int32 calleeBase = baseIndex + curFunc.MaxRegs;

	Int32 selfParam = SelfParamOffset(callee);

	// Native intrinsic: invoke callback directly, no frame push
	if (!IsNull(callee.NativeCallback)) {
		EnsureFrame(calleeBase, callee.MaxRegs);
		SetupCallFrame(0, selfParam, calleeBase, callee);
		if (selfParam > 0) {
			stack[calleeBase + 1] = pendingSelf;
			names[calleeBase + 1] = val_self;
			pendingSelf = val_null;
			hasPendingContext = Boolean(false);
		}
		SaveState(returnPC, baseIndex, currentFuncIndex);
		if (InvokeNativeCallback(callee.NativeCallback, calleeBase, selfParam, IntrinsicResult::Null, baseIndex + resultReg)) {
			return -1;  // done
		}
		return -2;  // pending
	}

	// User function: push CallInfo and set up callee frame
	if (callStackTop >= callStack.Count()) {
		RaiseRuntimeError("Call stack overflow");
		return -1;
	}
	callStack[callStackTop] = CallInfo(returnPC, baseIndex, currentFuncIndex, resultReg, outerVars);
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
bool VMStorage::InvokeNativeCallback(NativeCallbackDelegate callback,Int32 calleeBase,Int32 argCount,IntrinsicResult partialResult,Int32 absoluteResultIndex) {
	Context context = Context(
		*this,
		stack,
		calleeBase,
		argCount);
	IntrinsicResult ir = callback(context, partialResult);
	stack[absoluteResultIndex] = ir.result;
	if (ir.done) return Boolean(true);
	_pendingCallback = callback;
	_pendingCalleeBase = calleeBase;
	_pendingArgCount = argCount;
	_pendingResultIndex = absoluteResultIndex;
	return Boolean(false);
}
Value VMStorage::Execute(FuncDef entry) {
	return Execute(entry, 0);
}
Value VMStorage::Execute(FuncDef entry,UInt32 maxCycles) {
	// Legacy method - convert to Reset + Run pattern
	List<FuncDef> singleFunc =  List<FuncDef>::New();
	singleFunc.Add(entry);
	Reset(singleFunc);
	return Run(maxCycles);
}
Value VMStorage::Run(UInt32 maxCycles) {
	VM _this(std::static_pointer_cast<VMStorage>(shared_from_this()));
	if (!IsRunning || !CurrentFunction) {
		return val_null;
	}

	// Set thread-local active VM (save/restore for nested calls)
	VM previousVM = _activeVM;
	_activeVM = _this;

	// If we have a pending intrinsic continuation, re-invoke it
	// before resuming normal execution.
	if (!IsNull(_pendingCallback)) {
		IntrinsicResult partialResult = IntrinsicResult(stack[_pendingResultIndex], Boolean(false));
		if (!InvokeNativeCallback(_pendingCallback, _pendingCalleeBase, _pendingArgCount, partialResult, _pendingResultIndex)) {
			// Still not done; return without running any bytecode
			_activeVM = previousVM;
			return val_null;
		}
		_pendingCallback = nullptr;
	}

	Value runResult = RunInner(maxCycles);
	_activeVM = previousVM;
	return runResult;
}
Value VMStorage::RunInner(UInt32 maxCycles) {
	VM _this(std::static_pointer_cast<VMStorage>(shared_from_this()));
	// Copy instance variables to locals for performance
	Int32 pc = PC;
	Int32 baseIndex = BaseIndex;
	Int32 currentFuncIndex = _currentFuncIndex;

	Value* stackPtr = &stack[0];
	// Note: CollectionsMarshal.AsSpan requires .NET 5+; not compatible with Mono.
	// This gives us direct array access without copying, for performance.
	
	// Variables reflecting the current call frame:
	FuncDefStorage* curFuncRaw = nullptr;
	Int32 codeCount = 0;
	UInt32* curCode = nullptr;
	Value* curConstants = nullptr;
	Value* localStack = nullptr;
	
	SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);

	UInt32 cyclesLeft = maxCycles;
	if (maxCycles == 0) cyclesLeft--;  // wraps to MAX_UINT32

	// Reusable Value variables (declared outside loop for GC safety in C++)
	// ToDo: see if we can reduce these to a more reasonable number.
	Value val;
	Value valA, valB, valC, valD;
	Value typeMap;

#if VM_USE_COMPUTED_GOTO
	static void* const vm_labels[(int)Opcode::OP__COUNT] = { VM_OPCODES(VM_LABEL_LIST) };
	if (DebugMode) IOHelper::Print("(Running with computed-goto dispatch)");
#else
	if (DebugMode) IOHelper::Print("(Running with switch-based dispatch)");
#endif

	while (IsRunning) {
		VM_DISPATCH_TOP();
		if (cyclesLeft == 0) {
			SaveState(pc, baseIndex, currentFuncIndex);
			return val_null;
		}
		cyclesLeft--;

		if (pc >= codeCount) {
			IsRunning = Boolean(false);
			SaveState(pc, baseIndex, currentFuncIndex);
			return val_null;
		}

		UInt32 instruction = curCode[pc++];

		if (DebugMode) {
			IOHelper::Print(StringUtils::Format("{0} {1}: {2}     r0:{3}, r1:{4}, r2:{5}",
				curFuncRaw->Name,
				StringUtils::ZeroPad(pc-1, 4),
				Disassembler::ToString(instruction),
				localStack[0], localStack[1], localStack[2]));
		}

		Opcode opcode = (Opcode)BytecodeUtil::OP(instruction);
		
		VM_DISPATCH_BEGIN();
		
			VM_CASE(NOOP) {
				// No operation
				VM_NEXT();
			}

			VM_CASE(LOAD_rA_rB) {
				// R[A] = R[B] (equivalent to MOVE)
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				localStack[a] = localStack[b];
				VM_NEXT();
			}

			VM_CASE(LOAD_rA_iBC) {
				// R[A] = BC (signed 16-bit immediate as integer)
				Byte a = BytecodeUtil::Au(instruction);
				short bc = BytecodeUtil::BCs(instruction);
				localStack[a] = make_int(bc);
				VM_NEXT();
			}

			VM_CASE(LOAD_rA_kBC) {
				// R[A] = constants[BC]
				Byte a = BytecodeUtil::Au(instruction);
				UInt16 constIdx = BytecodeUtil::BCu(instruction);
				localStack[a] = curConstants[constIdx];
				VM_NEXT();
			}

			VM_CASE(LOADNULL_rA) {
				// R[A] = null
				Byte a = BytecodeUtil::Au(instruction);
				localStack[a] = val_null;
				VM_NEXT();
			}

			VM_CASE(LOADV_rA_rB_kC) {
				// R[A] = R[B], but verify that register B has name matching constants[C]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);

				// Check if the source register has the expected name
				valC = curConstants[c];  // expected name
				valB = names[baseIndex + b];  // actual name
				if (value_identical(valC, valB)) {
					localStack[a] = localStack[b];
				} else {
					// Variable not found in current scope, look in outer context
					localStack[a] = LookupVariable(valC);
				}
				VM_NEXT();
			}

			VM_CASE(LOADC_rA_rB_kC) {
				// R[A] = R[B], but verify that register B has name matching constants[C]
				// and call the function if the value is a function reference
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);

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
					  *functionsRaw[currentFuncIndex]);
					if (calleeIdx == -2) {
						// Native callback pending — exit RunInner
						cyclesLeft = 0;
					} else if (calleeIdx >= 0) {
						// Frame was pushed — switch to callee
						baseIndex += curFuncRaw->MaxRegs;
						pc = 0;
						currentFuncIndex = calleeIdx;
						SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
					}
				}
				VM_NEXT();
			}

			VM_CASE(FUNCREF_iA_iBC) {
				// R[A] := make_funcref(BC) with closure context
				Byte a = BytecodeUtil::Au(instruction);
				Int16 funcIndex = BytecodeUtil::BCs(instruction);

				// Create function reference with our locals as the closure context
				val = GetCurrentLocalVarMap(baseIndex, curFuncRaw->MaxRegs);
				localStack[a] = make_funcref(funcIndex, val);
				VM_NEXT();
			}

			VM_CASE(ASSIGN_rA_rB_kC) {
				// R[A] = R[B] and names[baseIndex + A] = constants[C]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				localStack[a] = localStack[b];
				valC = curConstants[c];
				names[baseIndex + a] = valC;
				// In REPL mode, register this variable in the globals VarMap
				if (baseIndex == 0 && !is_null(ReplGlobals)) {
					varmap_map_to_register(ReplGlobals, valC, 
						&stack[0],
						baseIndex + a);
				}
				VM_NEXT();
			}

			VM_CASE(NAME_rA_kBC) {
				// names[baseIndex + A] = constants[BC] (without changing R[A])
				Byte a = BytecodeUtil::Au(instruction);
				UInt16 constIdx = BytecodeUtil::BCu(instruction);
				valC = curConstants[constIdx];
				names[baseIndex + a] = valC;
				// In REPL mode, register this variable in the globals VarMap
				if (baseIndex == 0 && !is_null(ReplGlobals)) {
					varmap_map_to_register(ReplGlobals, valC,
						&stack[0],
						baseIndex + a);
				}
				VM_NEXT();
			}

			VM_CASE(ADD_rA_rB_rC) {
				// R[A] = R[B] + R[C]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				localStack[a] = value_add(localStack[b], localStack[c], _this);
				VM_NEXT();
			}

			VM_CASE(SUB_rA_rB_rC) {
				// R[A] = R[B] - R[C]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				localStack[a] = value_sub(localStack[b], localStack[c]);
				VM_NEXT();
			}

			VM_CASE(MULT_rA_rB_rC) {
				// R[A] = R[B] * R[C]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				localStack[a] = value_mult(localStack[b], localStack[c]);
				VM_NEXT();
			}

			VM_CASE(DIV_rA_rB_rC) {
				// R[A] = R[B] * R[C]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				localStack[a] = value_div(localStack[b], localStack[c]);
				VM_NEXT();
			}

			VM_CASE(MOD_rA_rB_rC) {
				// R[A] = R[B] % R[C]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				localStack[a] = value_mod(localStack[b], localStack[c]);
				VM_NEXT();
			}

			VM_CASE(POW_rA_rB_rC) {
				// R[A] = R[B] ^ R[C]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				localStack[a] = value_pow(localStack[b], localStack[c]);
				VM_NEXT();
			}

			VM_CASE(AND_rA_rB_rC) {
				// R[A] = R[B] and R[C] (fuzzy logic: AbsClamp01(a * b))
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				localStack[a] = value_and(localStack[b], localStack[c]);
				VM_NEXT();
			}

			VM_CASE(OR_rA_rB_rC) {
				// R[A] = R[B] or R[C] (fuzzy logic: AbsClamp01(a + b - a*b))
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				localStack[a] = value_or(localStack[b], localStack[c]);
				VM_NEXT();
			}

			VM_CASE(NOT_rA_rB) {
				// R[A] = not R[B] (fuzzy logic: 1 - AbsClamp01(b))
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				localStack[a] = value_not(localStack[b]);
				VM_NEXT();
			}

			VM_CASE(LIST_rA_iBC) {
				// R[A] = make_list(BC)
				Byte a = BytecodeUtil::Au(instruction);
				Int16 capacity = BytecodeUtil::BCs(instruction);
				localStack[a] = make_list(capacity);
				VM_NEXT();
			}

			VM_CASE(MAP_rA_iBC) {
				// R[A] = make_map(BC)
				Byte a = BytecodeUtil::Au(instruction);
				Int16 capacity = BytecodeUtil::BCs(instruction);
				localStack[a] = make_map(capacity);
				VM_NEXT();
			}

			VM_CASE(PUSH_rA_rB) {
				// list_push(R[A], R[B])
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				list_push(localStack[a], localStack[b]);
				VM_NEXT();
			}

			VM_CASE(INDEX_rA_rB_rC) {
				// R[A] = R[B][R[C]] (supports lists, maps, and strings)
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				valB = localStack[b];  // container
				valC = localStack[c];  // index

				if (is_error(valB)) {
					// Address-of on an error is always an lvalue operation; terminate.
					RaiseRuntimeError("Cannot take reference into an error value");
					localStack[a] = val_null;
					VM_NEXT();
				}
				if (is_list(valB)) {
					// ToDo: add a list_try_get and use it here, like we do with map below
					localStack[a] = list_get(valB, as_int(valC));
				} else if (is_map(valB)) {
					if (!map_lookup(valB, valC, &val)) {
						RaiseRuntimeError(StringUtils::Format("Key Not Found: '{0}' not found in map", valC));
					}
					localStack[a] = val;
				} else if (is_string(valB)) {
					Int32 idx = as_int(valC);
					localStack[a] = string_substring(valB, idx, 1);
				} else {
					RaiseRuntimeError(StringUtils::Format("Can't index into {0}", valB));
					localStack[a] = val_null;
				}
				VM_NEXT();
			}

			VM_CASE(IDXSET_rA_rB_rC) {
				// R[A][R[B]] = R[C] (supports both lists and maps)
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				valA = localStack[a];  // container
				valB = localStack[b];  // index
				valC = localStack[c];  // value

				if (is_error(valA)) {
					RaiseRuntimeError("Cannot assign into an error value");
					VM_NEXT();
				}
				if (is_list(valA)) {
					list_set(valA, as_int(valB), valC);
				} else if (is_map(valA)) {
					map_set(valA, valB, valC);
				} else {
					RaiseRuntimeError(StringUtils::Format("Can't set indexed value in {0}", valA));
				}
				VM_NEXT();
			}

			VM_CASE(SLICE_rA_rB_rC) {
				// R[A] = R[B][R[C]:R[C+1]] (slice; end index in adjacent register)
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				valB = localStack[b];
				valC = localStack[c];
				valD = localStack[c + 1];

				if (is_error(valB)) { RaiseUncaughtError(valB); localStack[a] = val_null; break; }
				if (is_error(valC)) { RaiseUncaughtError(valC); localStack[a] = val_null; break; }
				if (is_error(valD)) { RaiseUncaughtError(valD); localStack[a] = val_null; break; }

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
					RaiseRuntimeError(StringUtils::Format("Can't slice {0}", valB));
					localStack[a] = val_null;
				}
				VM_NEXT();
			}

			VM_CASE(LOCALS_rA) {
				// Create VarMap for local variables and store in R[A].
				// Temporarily clear this register's name while building the VarMap, so that
				// the destination register does not appear as a self-referential entry (e.g.,
				// "locals" pointing to the VarMap itself).  Restore afterward so that if the
				// user wrote `myLocals = locals`, the name "myLocals" remains findable.
				Byte a = BytecodeUtil::Au(instruction);
				val = names[baseIndex + a];  // saved name
				names[baseIndex + a] = val_null;   // temporarily clear it
				localStack[a] = GetCurrentLocalVarMap(baseIndex, curFuncRaw->MaxRegs);
				names[baseIndex + a] = val;  // restore name
				VM_NEXT();
			}

			VM_CASE(OUTER_rA) {
				// Return VarMap for outer scope; fall back to globals if none.
				// Same temporary-clear pattern as LOCALS_rA (see above).
				Byte a = BytecodeUtil::Au(instruction);
				val = names[baseIndex + a];  // saved name
				names[baseIndex + a] = val_null;   // temporarily clear it
				if (callStackTop > 0 && !is_null(callStack[callStackTop - 1].OuterVarMap)) {
					localStack[a] = callStack[callStackTop - 1].OuterVarMap;
				} else {
					// No enclosing scope or at global scope: outer == globals
					localStack[a] = GetGlobalsVarMap();
				}
				names[baseIndex + a] = val;   // restore name
				VM_NEXT();
			}

			VM_CASE(GLOBALS_rA) {
				// Return VarMap for global variables.
				// Same temporary-clear pattern as LOCALS_rA (see above).
				Byte a = BytecodeUtil::Au(instruction);
				val = names[baseIndex + a];  // saved name
				names[baseIndex + a] = val_null;   // temporarily clear it
				localStack[a] = GetGlobalsVarMap();
				names[baseIndex + a] = val;  // restore name
				VM_NEXT();
			}

			VM_CASE(JUMP_iABC) {
				// Jump by signed 24-bit ABC offset from current PC
				Int32 offset = BytecodeUtil::ABCs(instruction);
				pc += offset;
				VM_NEXT();
			}

			VM_CASE(LT_rA_rB_rC) {
				// if R[A] = R[B] < R[C]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				if (is_error(localStack[b])) { localStack[a] = localStack[b]; break; }
				if (is_error(localStack[c])) { localStack[a] = localStack[c]; break; }
				localStack[a] = make_int(value_lt(localStack[b], localStack[c]));
				VM_NEXT();
			}

			VM_CASE(LT_rA_rB_iC) {
				// if R[A] = R[B] < C (immediate)
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				SByte c = BytecodeUtil::Cs(instruction);
				if (is_error(localStack[b])) { localStack[a] = localStack[b]; break; }
				localStack[a] = make_int(value_lt(localStack[b], make_int(c)));
				VM_NEXT();
			}

			VM_CASE(LT_rA_iB_rC) {
				// if R[A] = B (immediate) < R[C]
				Byte a = BytecodeUtil::Au(instruction);
				SByte b = BytecodeUtil::Bs(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				if (is_error(localStack[c])) { localStack[a] = localStack[c]; break; }
				localStack[a] = make_int(value_lt(make_int(b), localStack[c]));
				VM_NEXT();
			}

			VM_CASE(LE_rA_rB_rC) {
				// if R[A] = R[B] <= R[C]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				if (is_error(localStack[b])) { localStack[a] = localStack[b]; break; }
				if (is_error(localStack[c])) { localStack[a] = localStack[c]; break; }
				localStack[a] = make_int(value_le(localStack[b], localStack[c]));
				VM_NEXT();
			}

			VM_CASE(LE_rA_rB_iC) {
				// if R[A] = R[B] <= C (immediate)
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				SByte c = BytecodeUtil::Cs(instruction);
				if (is_error(localStack[b])) { localStack[a] = localStack[b]; break; }
				localStack[a] = make_int(value_le(localStack[b], make_int(c)));
				VM_NEXT();
			}

			VM_CASE(LE_rA_iB_rC) {
				// if R[A] = B (immediate) <= R[C]
				Byte a = BytecodeUtil::Au(instruction);
				SByte b = BytecodeUtil::Bs(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				if (is_error(localStack[c])) { localStack[a] = localStack[c]; break; }
				localStack[a] = make_int(value_le(make_int(b), localStack[c]));
				VM_NEXT();
			}

			VM_CASE(EQ_rA_rB_rC) {
				// if R[A] = R[B] == R[C]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);

				localStack[a] = make_int(value_equal(localStack[b], localStack[c]));
				VM_NEXT();
			}

			VM_CASE(EQ_rA_rB_iC) {
				// if R[A] = R[B] == C (immediate)
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				SByte c = BytecodeUtil::Cs(instruction);
				
				localStack[a] = make_int(value_equal(localStack[b], make_int(c)));
				VM_NEXT();
			}

			VM_CASE(NE_rA_rB_rC) {
				// if R[A] = R[B] != R[C]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);

				localStack[a] = make_int(!value_equal(localStack[b], localStack[c]));
				VM_NEXT();
			}

			VM_CASE(NE_rA_rB_iC) {
				// if R[A] = R[B] != C (immediate)
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				SByte c = BytecodeUtil::Cs(instruction);
				
				localStack[a] = make_int(!value_equal(localStack[b], make_int(c)));
				VM_NEXT();
			}

			VM_CASE(BRTRUE_rA_iBC) {
				Byte a = BytecodeUtil::Au(instruction);
				Int32 offset = BytecodeUtil::BCs(instruction);
				if (is_error(localStack[a])) {
					RaiseRuntimeError(StringUtils::Format("Error used in conditional: {0}", localStack[a]));
					VM_NEXT();
				}
				if (is_truthy(localStack[a])){
					pc += offset;
				}
				VM_NEXT();
			}

			VM_CASE(BRFALSE_rA_iBC) {
				Byte a = BytecodeUtil::Au(instruction);
				Int32 offset = BytecodeUtil::BCs(instruction);
				if (is_error(localStack[a])) {
					RaiseRuntimeError(StringUtils::Format("Error used in conditional: {0}", localStack[a]));
					VM_NEXT();
				}
				if (!is_truthy(localStack[a])){
					pc += offset;
				}
				VM_NEXT();
			}

			VM_CASE(BRLT_rA_rB_iC) {
				// if R[A] < R[B] then jump offset C.
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				SByte offset = BytecodeUtil::Cs(instruction);
				if (is_error(localStack[a]) || is_error(localStack[b])) {
					RaiseRuntimeError("Error used in conditional");
					VM_NEXT();
				}
				if (value_lt(localStack[a], localStack[b])){
					pc += offset;
				}
				VM_NEXT();
			}

			VM_CASE(BRLT_rA_iB_iC) {
				// if R[A] < B (immediate) then jump offset C.
				Byte a = BytecodeUtil::Au(instruction);
				SByte b = BytecodeUtil::Bs(instruction);
				SByte offset = BytecodeUtil::Cs(instruction);
				if (is_error(localStack[a])) {
					RaiseRuntimeError("Error used in conditional");
					VM_NEXT();
				}
				if (value_lt(localStack[a], make_int(b))){
					pc += offset;
				}
				VM_NEXT();
			}

			VM_CASE(BRLT_iA_rB_iC) {
				// if A (immediate) < R[B] then jump offset C.
				SByte a = BytecodeUtil::As(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				SByte offset = BytecodeUtil::Cs(instruction);
				if (is_error(localStack[b])) {
					RaiseRuntimeError("Error used in conditional");
					VM_NEXT();
				}
				if (value_lt(make_int(a), localStack[b])){
					pc += offset;
				}
				VM_NEXT();
			}

			VM_CASE(BRLE_rA_rB_iC) {
				// if R[A] <= R[B] then jump offset C.
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				SByte offset = BytecodeUtil::Cs(instruction);
				if (is_error(localStack[a]) || is_error(localStack[b])) {
					RaiseRuntimeError("Error used in conditional");
					VM_NEXT();
				}
				if (value_le(localStack[a], localStack[b])){
					pc += offset;
				}
				VM_NEXT();
			}

			VM_CASE(BRLE_rA_iB_iC) {
				// if R[A] <= B (immediate) then jump offset C.
				Byte a = BytecodeUtil::Au(instruction);
				SByte b = BytecodeUtil::Bs(instruction);
				SByte offset = BytecodeUtil::Cs(instruction);
				if (is_error(localStack[a])) {
					RaiseRuntimeError("Error used in conditional");
					VM_NEXT();
				}
				if (value_le(localStack[a], make_int(b))){
					pc += offset;
				}
				VM_NEXT();
			}

			VM_CASE(BRLE_iA_rB_iC) {
				// if A (immediate) <= R[B] then jump offset C.
				SByte a = BytecodeUtil::As(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				SByte offset = BytecodeUtil::Cs(instruction);
				if (is_error(localStack[b])) {
					RaiseRuntimeError("Error used in conditional");
					VM_NEXT();
				}
				if (value_le(make_int(a), localStack[b])){
					pc += offset;
				}
				VM_NEXT();
			}

			VM_CASE(BREQ_rA_rB_iC) {
				// if R[A] == R[B] then jump offset C.
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				SByte offset = BytecodeUtil::Cs(instruction);
				if (value_equal(localStack[a], localStack[b])){
					pc += offset;
				}
				VM_NEXT();
			}

			VM_CASE(BREQ_rA_iB_iC) {
				// if R[A] == B (immediate) then jump offset C.
				Byte a = BytecodeUtil::Au(instruction);
				SByte b = BytecodeUtil::Bs(instruction);
				SByte offset = BytecodeUtil::Cs(instruction);
				if (value_equal(localStack[a], make_int(b))){
					pc += offset;
				}
				VM_NEXT();
			}

			VM_CASE(BRNE_rA_rB_iC) {
				// if R[A] != R[B] then jump offset C.
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				SByte offset = BytecodeUtil::Cs(instruction);
				if (!value_equal(localStack[a], localStack[b])){
					pc += offset;
				}
				VM_NEXT();
			}

			VM_CASE(BRNE_rA_iB_iC) {
				// if R[A] != B (immediate) then jump offset C.
				Byte a = BytecodeUtil::Au(instruction);
				SByte b = BytecodeUtil::Bs(instruction);
				SByte offset = BytecodeUtil::Cs(instruction);
				if (!value_equal(localStack[a], make_int(b))){
					pc += offset;
				}
				VM_NEXT();
			}

			VM_CASE(IFLT_rA_rB) {
				// if R[A] < R[B] is false, skip next instruction
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				if (is_error(localStack[a]) || is_error(localStack[b])) {
					RaiseRuntimeError("Error used in conditional"); break;
				}
				if (!value_lt(localStack[a], localStack[b])) {
					pc++; // Skip next instruction
				}
				VM_NEXT();
			}

			VM_CASE(IFLT_rA_iBC) {
				// if R[A] < BC (immediate) is false, skip next instruction
				Byte a = BytecodeUtil::Au(instruction);
				short bc = BytecodeUtil::BCs(instruction);
				if (is_error(localStack[a])) { RaiseRuntimeError("Error used in conditional"); break; }
				if (!value_lt(localStack[a], make_int(bc))) {
					pc++; // Skip next instruction
				}
				VM_NEXT();
			}

			VM_CASE(IFLT_iAB_rC) {
				// if AB (immediate) < R[C] is false, skip next instruction
				short ab = BytecodeUtil::ABs(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				if (is_error(localStack[c])) { RaiseRuntimeError("Error used in conditional"); break; }
				if (!value_lt(make_int(ab), localStack[c])) {
					pc++; // Skip next instruction
				}
				VM_NEXT();
			}

			VM_CASE(IFLE_rA_rB) {
				// if R[A] <= R[B] is false, skip next instruction
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				if (is_error(localStack[a]) || is_error(localStack[b])) {
					RaiseRuntimeError("Error used in conditional"); break;
				}
				if (!value_le(localStack[a], localStack[b])) {
					pc++; // Skip next instruction
				}
				VM_NEXT();
			}

			VM_CASE(IFLE_rA_iBC) {
				// if R[A] <= BC (immediate) is false, skip next instruction
				Byte a = BytecodeUtil::Au(instruction);
				short bc = BytecodeUtil::BCs(instruction);
				if (is_error(localStack[a])) { RaiseRuntimeError("Error used in conditional"); break; }
				if (!value_le(localStack[a], make_int(bc))) {
					pc++; // Skip next instruction
				}
				VM_NEXT();
			}

			VM_CASE(IFLE_iAB_rC) {
				// if AB (immediate) <= R[C] is false, skip next instruction
				short ab = BytecodeUtil::ABs(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				if (is_error(localStack[c])) { RaiseRuntimeError("Error used in conditional"); break; }
				if (!value_le(make_int(ab), localStack[c])) {
					pc++; // Skip next instruction
				}
				VM_NEXT();
			}

			VM_CASE(IFEQ_rA_rB) {
				// if R[A] == R[B] is false, skip next instruction
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				if (!value_equal(localStack[a], localStack[b])) {
					pc++; // Skip next instruction
				}
				VM_NEXT();
			}

			VM_CASE(IFEQ_rA_iBC) {
				// if R[A] == BC (immediate) is false, skip next instruction
				Byte a = BytecodeUtil::Au(instruction);
				short bc = BytecodeUtil::BCs(instruction);
				if (!value_equal(localStack[a], make_int(bc))) {
					pc++; // Skip next instruction
				}
				VM_NEXT();
			}

			VM_CASE(IFNE_rA_rB) {
				// if R[A] != R[B] is false, skip next instruction
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				if (value_equal(localStack[a], localStack[b])) {
					pc++; // Skip next instruction
				}
				VM_NEXT();
			}

			VM_CASE(IFNE_rA_iBC) {
				// if R[A] != BC (immediate) is false, skip next instruction
				Byte a = BytecodeUtil::Au(instruction);
				short bc = BytecodeUtil::BCs(instruction);
				if (value_equal(localStack[a], make_int(bc))) {
					pc++; // Skip next instruction
				}
				VM_NEXT();
			}

			VM_CASE(NEXT_rA_rB) {
				// Advance iterator R[A] to next entry in collection R[B].
				// If there is a next entry, skip next instruction (the JUMP to end).
				// Iterator values are opaque: for lists/strings they are sequential
				// indices; for maps they encode position via map_iter_next.
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
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
					hasMore = Boolean(false);
				}
				localStack[a] = make_int(iter);
				if (hasMore) {
					pc++; // Skip next instruction (the JUMP to end of loop)
				}
				VM_NEXT();
			}

			VM_CASE(ARGBLK_iABC) {
				// Begin argument block with specified count
				// ABC: number of ARG instructions that follow
				Int32 argCount = BytecodeUtil::ABCs(instruction);

				// Look ahead to find the CALL instruction (argCount instructions ahead)
				Int32 callPC = pc + argCount;
				if (callPC >= codeCount) {
					RaiseRuntimeError("ARGBLK: CALL instruction out of range");
					return val_null;
				}
				UInt32 callInstruction = curCode[callPC];
				Opcode callOp = (Opcode)BytecodeUtil::OP(callInstruction);
				if (callOp != Opcode::CALL_rA_rB_rC) {
					RaiseRuntimeError("ARGBLK must be followed by CALL");
					return val_null;
				}

				// CALL r[A], r[B], r[C]: invoke funcref in r[C], frame at r[B], result to r[A]
				Byte a = BytecodeUtil::Au(callInstruction);
				Byte b = BytecodeUtil::Bu(callInstruction);
				Byte c = BytecodeUtil::Cu(callInstruction);

				valC = localStack[c];  // func ref
				if (!is_funcref(valC)) {
					RaiseRuntimeError("ARGBLK/CALL: Not a function reference");
					return val_null;
				}

				Int32 funcIndex = funcref_index(valC);
				if (funcIndex < 0 || funcIndex >= functions.Count()) {
					RaiseRuntimeError("ARGBLK/CALL: Invalid function index");
					return val_null;
				}
				FuncDefRef callee = 
				  *functionsRaw[funcIndex];
				Int32 calleeBase = baseIndex + b;
				Int32 resultReg = a;

				// Check for self-injection: if pending context and first param is "self",
				// inject pendingSelf as the first argument
				Int32 selfParam = SelfParamOffset(callee);
				Int32 nextPC = ProcessArguments(argCount, selfParam, pc, baseIndex, calleeBase, callee, 
				  curFuncRaw->Code);
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
				if (!IsNull(callee.NativeCallback)) {
					pc = nextPC;
					SaveState(pc, baseIndex, currentFuncIndex);
					if (!InvokeNativeCallback(callee.NativeCallback, calleeBase, argCount + selfParam, IntrinsicResult::Null, baseIndex + resultReg)) {
						cyclesLeft = 0;
					}
					VM_NEXT();
				}

				// Now execute the CALL (step 6): push CallInfo and switch to callee
				if (callStackTop >= callStack.Count()) {
					RaiseRuntimeError("Call stack overflow");
					return val_null;
				}

				val = funcref_outer_vars(valC);
				callStack[callStackTop] = CallInfo(nextPC, baseIndex, currentFuncIndex, resultReg, val);
				callStackTop++;

				baseIndex = calleeBase;
				currentFuncIndex = funcIndex; // Switch to callee function
				pc = 0; // Start at beginning of callee code
				SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
				EnsureFrame(baseIndex, callee.MaxRegs);
				VM_NEXT();
			}

			VM_CASE(ARG_rA) {
				// The VM should never encounter this opcode on its own; it will
				// be processed as part of the ARGBLK opcode.  So if we get
				// here, it's an error.
				RaiseRuntimeError("Internal error: ARG without ARGBLK");
				return val_null;
			}

			VM_CASE(ARG_iABC) {
				// The VM should never encounter this opcode on its own; it will
				// be processed as part of the ARGBLK opcode.  So if we get
				// here, it's an error.
				RaiseRuntimeError("Internal error: ARG without ARGBLK");
				return val_null;
			}

			VM_CASE(CALLF_iA_iBC) {
				// A: arg window start (callee executes with base = base + A)
				// BC: function index
				Byte a = BytecodeUtil::Au(instruction);
				UInt16 funcIndex = BytecodeUtil::BCu(instruction);
				
				if (funcIndex >= functions.Count()) {
					RaiseRuntimeError("CALLF: Invalid function index");
					VM_NEXT();
				}

				FuncDefRef callee =
				  *functionsRaw[funcIndex];

				// Push return info
				if (callStackTop >= callStack.Count()) {
					RaiseRuntimeError("Call stack overflow");
					VM_NEXT();
				}
				callStack[callStackTop] = CallInfo(pc, baseIndex, currentFuncIndex);
				callStackTop++;

				// Switch to callee frame: base slides to argument window
				baseIndex += a;
				// Note: ApplyPendingContext skipped for CALLF (only needed for method dispatch via CALL)
				pc = 0; // Start at beginning of callee code
				currentFuncIndex = funcIndex; // Switch to callee function
				SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);

				EnsureFrame(baseIndex, callee.MaxRegs);
				VM_NEXT();
			}

			VM_CASE(CALLFN_iA_kBC) {
				// DEPRECATED: no longer emitted by the compiler.
				// Intrinsics are now callable FuncRefs resolved via LOADV + CALL.
				RaiseRuntimeError("CALLFN is no longer supported; use LOADV + CALL instead");
				VM_NEXT();
			}

			VM_CASE(CALL_rA_rB_rC) {
				// Invoke the FuncRef in R[C], with a stack frame starting at our register B,
				// and upon return, copy the result from r[B] to r[A].
				//
				// A: destination register for result
				// B: stack frame start register for callee
				// C: register containing FuncRef to call
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);

				valC = localStack[c];  // func reference
				if (!is_funcref(valC)) {
					RaiseRuntimeError("CALL: Value in register is not a function reference");
					VM_NEXT();
				}

				Int32 funcIndex = funcref_index(valC);
				if (funcIndex < 0 || funcIndex >= functions.Count()) {
					RaiseRuntimeError("CALL: Invalid function index in FuncRef");
					VM_NEXT();
				}
				FuncDefRef callee =
				   *functionsRaw[funcIndex];
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
				if (!IsNull(callee.NativeCallback)) {
					SaveState(pc, baseIndex, currentFuncIndex);
					if (!InvokeNativeCallback(callee.NativeCallback, calleeBase, selfParam, IntrinsicResult::Null, baseIndex + a)) {
						cyclesLeft = 0;
					}
					VM_NEXT();
				}

				if (callStackTop >= callStack.Count()) {
					RaiseRuntimeError("Call stack overflow");
					VM_NEXT();
				}
				callStack[callStackTop] = CallInfo(pc, baseIndex, currentFuncIndex, a, valD);
				callStackTop++;

				// Set up call frame starting at baseIndex + b
				baseIndex = calleeBase;
				pc = 0; // Start at beginning of callee code
				currentFuncIndex = funcIndex; // Switch to callee function
				SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
				EnsureFrame(baseIndex, callee.MaxRegs);
				VM_NEXT();
			}

			VM_CASE(NEW_rA_rB) {
				// R[A] = new map with __isa set to R[B]
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				val = make_map(2);
				map_set(val, val_isa_key, localStack[b]);
				localStack[a] = val;
				VM_NEXT();
			}

			VM_CASE(ISA_rA_rB_rC) {
				// R[A] = (R[B] isa R[C])
				// True if:
				//   1. both are null
				//   2. R[C] is a built-in type map matching the type of R[B]
				//   3. R[B] and R[C] are the same map reference, or R[C]
				//      appears anywhere in R[B]'s __isa chain
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				valB = localStack[b];  // left-hand side
				valC = localStack[c];  // right-hand side
				Int32 isaResult = 0;
				if (is_null(valB) && is_null(valC)) {
					isaResult = 1;
				} else if (value_identical(valB, valC)) {
					isaResult = 1;
				} else if (is_error(valB)) {
					// Error-specific isa rules:
					//   e isa error   -> 1
					//   e1 isa e2     -> 1 if e2 is in e1's __isa chain
					if (value_identical(valC, CoreIntrinsics::ErrorType())) {
						isaResult = 1;
					} else if (is_error(valC) && error_isa_contains(valB, valC)) {
						isaResult = 1;
					}
					localStack[a] = make_int(isaResult);
					break;
				} else if (is_map(valC)) {
					// Walk valB's __isa chain looking for valC
					if (is_map(valB)) {
						val = valB;  // val is "current"; valA (below) is "next" in the __isa chain
						for (Int32 depth = 0; depth < 256; depth++) {
							if (!map_try_get(val, val_isa_key, &valA)) break;
							if (value_identical(valA, valC)) {
								isaResult = 1;
								break;
							}
							val = valA;
						}
					}
					// If not found via __isa chain, check built-in type maps
					if (isaResult == 0) {
						if (is_number(valB) && value_identical(valC, CoreIntrinsics::NumberType())) {
							isaResult = 1;
						} else if (is_string(valB) && value_identical(valC, CoreIntrinsics::StringType())) {
							isaResult = 1;
						} else if (is_list(valB) && value_identical(valC, CoreIntrinsics::ListType())) {
							isaResult = 1;
						} else if (is_map(valB) && value_identical(valC, CoreIntrinsics::MapType())) {
							isaResult = 1;
						} else if (is_funcref(valB) && value_identical(valC, CoreIntrinsics::FunctionType())) {
							isaResult = 1;
						}
					}
				}
				localStack[a] = make_int(isaResult);
				VM_NEXT();
			}

			VM_CASE(METHFIND_rA_rB_rC) {
				// Method lookup: walk __isa chain of R[B] looking for key R[C]
				// R[A] = the value found
				// Side effects: pendingSelf = R[B], pendingSuper = containing map's __isa
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
				valB = localStack[b];  // container
				valC = localStack[c];  // index
				typeMap = val_null;
				
				if (is_error(valB)) {
					// Error field access: return field directly for reserved names,
					// else fall back to ErrorType() for method lookup (e.g., e.err).
					// Any other key terminates per language spec.
					if (is_string(valC)) {
						String keyStr = as_cstring(valC);
						if (keyStr == "message") { localStack[a] = error_message(valB); hasPendingContext = Boolean(false); break; }
						if (keyStr == "inner")   { localStack[a] = error_inner(valB);   hasPendingContext = Boolean(false); break; }
						if (keyStr == "stack")   { localStack[a] = error_stack(valB);   hasPendingContext = Boolean(false); break; }
						if (keyStr == "__isa")   { localStack[a] = error_isa(valB);     hasPendingContext = Boolean(false); break; }
					}
					typeMap = CoreIntrinsics::ErrorType();
					if (map_try_get(typeMap, valC, &val)) {
						localStack[a] = val;
						pendingSelf = valB;
						pendingSuper = val_null;
						hasPendingContext = Boolean(true);
						VM_NEXT();
					}
					// Wrap the error as inner in a new termination error
					RaiseRuntimeError(StringUtils::Format("Undefined error field '{0}'", valC));
					localStack[a] = val_null;
					VM_NEXT();
				}
				if (is_map(valB)) {
					// For maps: first do lookup in the map itself, with inheritance
					// (valD: the "super" value, i.e., __isa of the map in which valC
					// was actually found.)
					if (map_lookup_with_origin(valB, valC, &val, &valD)) {
						localStack[a] = val;
						pendingSelf = valB;
						pendingSuper = valD;
						hasPendingContext = Boolean(true);
						VM_NEXT();
					}
					// ...falling back on the map type map
					typeMap = CoreIntrinsics::MapType();
				} else if (is_list(valB)) {
					typeMap = CoreIntrinsics::ListType();
				} else if (is_string(valB)) {
					typeMap = CoreIntrinsics::StringType();
				} else if (is_number(valB)) {
					typeMap = CoreIntrinsics::NumberType();
				}
				if (is_null(typeMap)) {
					// If we didn't get a type map, then user is trying to index
					// into something not indexable
					RaiseRuntimeError(StringUtils::Format("Can't index into {0}", valB));
					localStack[a] = val_null;
				} else if (map_try_get(typeMap, valC, &val)) {
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
						RaiseRuntimeError(StringUtils::Format("Can't index into {0}", valB));
						localStack[a] = val_null;
					}
				} else {
					RaiseRuntimeError(StringUtils::Format("Key Not Found: '{0}' not found in map", valC));
					localStack[a] = val_null;
				}
				hasPendingContext = Boolean(true);
				VM_NEXT();
			}

			VM_CASE(SETSELF_rA) {
				// Override pendingSelf with R[A] (used for super.method() calls)
				Byte a = BytecodeUtil::Au(instruction);
				pendingSelf = localStack[a];
				hasPendingContext = Boolean(true);
				VM_NEXT();
			}

			VM_CASE(CALLIFREF_rA) {
				// If R[A] is a funcref and we have pending context, auto-invoke it
				// (like LOADC does for variable references). If not a funcref,
				// just clear pending context.
				Byte a = BytecodeUtil::Au(instruction);
				val = localStack[a];

				if (!is_funcref(val) || !hasPendingContext) {
					// Not a funcref or no context — clear pending state and leave value as-is
					hasPendingContext = Boolean(false);
					pendingSelf = val_null;
					pendingSuper = val_null;
					VM_NEXT();
				}

				// Auto-invoke the funcref with zero args
				Int32 calleeIdx = AutoInvokeFuncRef(val, a, pc, baseIndex, currentFuncIndex,
				  *functionsRaw[currentFuncIndex]);
				if (calleeIdx == -2) {
					// Native callback pending — exit RunInner
					cyclesLeft = 0;
				} else if (calleeIdx >= 0) {
					// Frame was pushed — switch to callee
					baseIndex += curFuncRaw->MaxRegs;
					pc = 0;
					currentFuncIndex = calleeIdx;
					SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);
				}
				VM_NEXT();
			}

			VM_CASE(RETURN) {
				// Return value convention: value is in base[0]
				val = stack[baseIndex];

				// Gather the current execution-context frame's locals VarMap if one exists
				// (makes closure values survive beyond the function's lifetime).
				CallInfo frame = callStack[callStackTop - 1];
				if (!is_null(frame.LocalVarMap)) {
					varmap_gather(frame.LocalVarMap);
					frame.LocalVarMap = val_null;
					callStack[callStackTop - 1] = frame;  // write back (CallInfo is a struct)
				}

				// Pop the current execution-context frame.
				callStackTop--;
				if (callStackTop == 0) {
					// We just popped @main's frame — execution is complete.
					SaveState(pc, baseIndex, currentFuncIndex);
					IsRunning = Boolean(false);
					return val;
				}

				// Restore the caller's execution state from its frame.
				CallInfo callInfo = callStack[callStackTop];
				pc = callInfo.ReturnPC;
				baseIndex = callInfo.ReturnBase;
				currentFuncIndex = callInfo.ReturnFuncIndex;
				SwitchFrame(currentFuncIndex, baseIndex, curFuncRaw, codeCount, curCode, curConstants, localStack, stackPtr);

				if (callInfo.CopyResultToReg >= 0) {
					stack[baseIndex + callInfo.CopyResultToReg] = val;
				}
				VM_NEXT();
			}

			VM_CASE(ITERGET_rA_rB_rC) {
				// R[A] = entry from R[B] at iterator position R[C]
				// For lists: same as list[index]
				// For strings: same as string[index]
				// For maps: returns {"key":k, "value":v} via map_iter_entry
				Byte a = BytecodeUtil::Au(instruction);
				Byte b = BytecodeUtil::Bu(instruction);
				Byte c = BytecodeUtil::Cu(instruction);
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
				VM_NEXT();
			}

			VM_DISPATCH_END();
	}
	VM_DISPATCH_BOTTOM();
	
	// Save state after loop exit (e.g. from error condition)
	SaveState(pc, baseIndex, currentFuncIndex);
	return val_null;
}
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
Value VMStorage::GetGlobalsVarMap() {
	if (!is_null(ReplGlobals)) return ReplGlobals;
	CallInfo gframe = callStack[0];
	Int32 regCount = functions[gframe.ReturnFuncIndex].MaxRegs();
	return make_varmap(&stack[0], &names[0], 0, regCount);
}
Value VMStorage::LookupParamByName(String varName) {
	// Look up a parameter by name in the current frame.  This is provided
	// mainly for compatibility with 1.x; modern code should find parameters
	// by position, which is more efficient than searching by name.
	// Returns the value if found, or null if not found.
	FuncDef func = CurrentFunction;
	Value nameVal = make_string(varName);
	for (Int32 i = 0; i < func.ParamNames().Count(); i++) {
		if (value_equal(func.ParamNames()[i], nameVal)) {
			return stack[BaseIndex + 1 + i];
		}
	}
	return val_null;
}
Value VMStorage::LookupVariable(Value varName) {
	// Look up a variable in outer context (and eventually globals)
	// Returns the value if found, or null if not found
	Value result;		
	if (callStackTop > 0) {
		CallInfo currentFrame = callStack[callStackTop - 1];  // Current frame, not next frame
		if (!is_null(currentFrame.OuterVarMap)) {
			if (map_try_get(currentFrame.OuterVarMap, varName, &result)) {
				return result;
			}
		}
	}

	// Check global variables via VarMap (registers at base 0 in the @main frame)
	Value globalMap;
	if (callStackTop > 0 || !is_null(ReplGlobals)) {
		globalMap = GetGlobalsVarMap();
		if (map_try_get(globalMap, varName, &result)) {
			return result;
		}
	}

	// Check intrinsics table
	String nameStr = as_cstring(varName);
	if (_intrinsics.TryGetValue(nameStr, &result)) {
		return result;
	}

	// self/super return null when not in a method context
	if (value_equal(varName, val_self) || value_equal(varName, val_super)) {
		return val_null;
	}

	// Variable not found anywhere — raise an error
	RaiseRuntimeError(StringUtils::Format("Undefined Identifier: '{0}' is unknown in this context", varName));
	return val_null;
}

} // end of namespace MiniScript
