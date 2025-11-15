// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VM.cs

#ifndef __VM_H
#define __VM_H

#include "core_includes.h"
#include "value.h"
#include "value_list.h"
#include "value_string.h"
#include "Bytecode.g.h"
#include "FuncDef.g.h"
#include "IOHelper.g.h"
#include "Disassembler.g.h"
#include "StringUtils.g.h"
#include "dispatch_macros.h"


namespace MiniScript {

	
	// Call stack frame (return info)
	class CallInfo {
		public: Int32 ReturnPC;        // where to continue in caller (PC index)
		public: Int32 ReturnBase;      // caller's base pointer (stack index)
		public: Int32 ReturnFuncIndex; // caller's function index in functions list
		public: Int32 CopyResultToReg; // register number to copy result to, or -1
		public: Value LocalVarMap;     // VarMap representing locals, if any
		public: Value OuterVarMap;     // VarMap representing outer variables (closure context)

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
	}; // end of class CallInfo

	// VM state
	class VM {
		public: Boolean DebugMode = false;
		private: List<Value> stack;
		private: List<Value> names;		// Variable names parallel to stack (null if unnamed)

		private: List<CallInfo> callStack;
		private: Int32 callStackTop;	   // Index of next free call stack slot

		private: List<FuncDef> functions; // functions addressed by CALLF

		// Execution state (persistent across RunSteps calls)
		public: Int32 PC;
		private: Int32 _currentFuncIndex = -1;
		public: FuncDef CurrentFunction;
		public: Boolean IsRunning;
		public: Int32 BaseIndex;
		public: String RuntimeError;

		public: Int32 StackSize();
		public: Int32 CallStackDepth();

		public: Value GetStackValue(Int32 index);

		public: Value GetStackName(Int32 index);

		public: CallInfo GetCallStackFrame(Int32 index);

		public: String GetFunctionName(Int32 funcIndex);

		public: VM();
		
		public: VM(Int32 stackSlots, Int32 callSlots);

		private: void InitVM(Int32 stackSlots, Int32 callSlots);

		public: void RegisterFunction(FuncDef funcDef);

		public: void Reset(List<FuncDef> allFunctions);

		public: void RaiseRuntimeError(String message);
		
		public: bool ReportRuntimeError();

		// Helper for argument processing (FUNCTION_CALLS.md steps 1-3):
		// Process ARG instructions, validate argument count, and set up parameter registers.
		// Returns the PC after the CALL instruction, or -1 on error.
		private: Int32 ProcessArguments(Int32 argCount, Int32 startPC, Int32 callerBase, Int32 calleeBase, FuncDef callee, List<UInt32>& code);

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
		private: static const Value FuncNameRemove;
		
		private: void DoIntrinsic(Value funcName, Int32 baseReg);
	}; // end of class VM
}


#endif // __VM_H
