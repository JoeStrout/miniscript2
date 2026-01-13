// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VM.cs

#pragma once
#include "core_includes.h"
#include "value.h"
#include "FuncDef.g.h"
#include "value_map.h"


namespace MiniScript {

// FORWARD DECLARATIONS

struct VM;
class VMStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct App;
class AppStorage;

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
	inline Value GetLocalVarMap(List<Value>& registers, List<Value>& names, int baseIdx, int regCount) {
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

}; // end of struct CallInfo


// VM state











class VMStorage : public std::enable_shared_from_this<VMStorage> {
	friend struct VM;
	public: Boolean DebugMode = false;
	private: List<Value> stack;
	private: List<Value> names; // Variable names parallel to stack (null if unnamed)
	private: List<CallInfo> callStack;
	private: Int32 callStackTop; // Index of next free call stack slot
	private: List<FuncDef> functions; // functions addressed by CALLF
	public: Int32 PC;
	private: Int32 _currentFuncIndex = -1;
	public: FuncDef CurrentFunction;
	public: Boolean IsRunning;
	public: Int32 BaseIndex;
	public: String RuntimeError;



	// Execution state (persistent across RunSteps calls)

	public: Int32 StackSize();
	public: Int32 CallStackDepth();

	public: Value GetStackValue(Int32 index);

	public: Value GetStackName(Int32 index);

	public: CallInfo GetCallStackFrame(Int32 index);

	public: String GetFunctionName(Int32 funcIndex);

	public: VMStorage();
	
	public: VMStorage(Int32 stackSlots, Int32 callSlots);

	private: void InitVM(Int32 stackSlots, Int32 callSlots);

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
	
	
	private: void DoIntrinsic(Value funcName, Int32 baseReg);
}; // end of class VMStorage

struct VM {
	protected: std::shared_ptr<VMStorage> storage;
  public:
	VM(std::shared_ptr<VMStorage> stor) : storage(stor) {}
	VM() : storage(nullptr) {}
	static VM New() { return VM(std::make_shared<VMStorage>()); }
	friend bool IsNull(const VM& inst) { return inst.storage == nullptr; }
	private: VMStorage* get() const { return static_cast<VMStorage*>(storage.get()); }

	public: Boolean DebugMode() { return get()->DebugMode; }
	public: void set_DebugMode(Boolean _v) { get()->DebugMode = _v; }
	private: List<Value> stack() { return get()->stack; }
	private: void set_stack(List<Value> _v) { get()->stack = _v; }
	private: List<Value> names() { return get()->names; } // Variable names parallel to stack (null if unnamed)
	private: void set_names(List<Value> _v) { get()->names = _v; } // Variable names parallel to stack (null if unnamed)
	private: List<CallInfo> callStack() { return get()->callStack; }
	private: void set_callStack(List<CallInfo> _v) { get()->callStack = _v; }
	private: Int32 callStackTop() { return get()->callStackTop; } // Index of next free call stack slot
	private: void set_callStackTop(Int32 _v) { get()->callStackTop = _v; } // Index of next free call stack slot
	private: List<FuncDef> functions() { return get()->functions; } // functions addressed by CALLF
	private: void set_functions(List<FuncDef> _v) { get()->functions = _v; } // functions addressed by CALLF
	public: Int32 PC() { return get()->PC; }
	public: void set_PC(Int32 _v) { get()->PC = _v; }
	private: Int32 _currentFuncIndex() { return get()->_currentFuncIndex; }
	private: void set__currentFuncIndex(Int32 _v) { get()->_currentFuncIndex = _v; }
	public: FuncDef CurrentFunction() { return get()->CurrentFunction; }
	public: void set_CurrentFunction(FuncDef _v) { get()->CurrentFunction = _v; }
	public: Boolean IsRunning() { return get()->IsRunning; }
	public: void set_IsRunning(Boolean _v) { get()->IsRunning = _v; }
	public: Int32 BaseIndex() { return get()->BaseIndex; }
	public: void set_BaseIndex(Int32 _v) { get()->BaseIndex = _v; }
	public: String RuntimeError() { return get()->RuntimeError; }
	public: void set_RuntimeError(String _v) { get()->RuntimeError = _v; }



	// Execution state (persistent across RunSteps calls)

	public: Int32 StackSize() { return get()->StackSize(); }
	public: Int32 CallStackDepth() { return get()->CallStackDepth(); }

	public: Value GetStackValue(Int32 index) { return get()->GetStackValue(index); }

	public: Value GetStackName(Int32 index) { return get()->GetStackName(index); }

	public: CallInfo GetCallStackFrame(Int32 index) { return get()->GetCallStackFrame(index); }

	public: String GetFunctionName(Int32 funcIndex) { return get()->GetFunctionName(funcIndex); }

	

	private: void InitVM(Int32 stackSlots, Int32 callSlots) { return get()->InitVM(stackSlots, callSlots); }

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
	
	
	private: void DoIntrinsic(Value funcName, Int32 baseReg) { return get()->DoIntrinsic(funcName, baseReg); }
}; // end of struct VM


// INLINE METHODS

} // end of namespace MiniScript

