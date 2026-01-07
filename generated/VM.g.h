// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VM.cs

#pragma once
#include "core_includes.h"


namespace MiniScript {

// FORWARD DECLARATIONS

struct CallInfo;
class CallInfoStorage;
struct VMVis;
class VMVisStorage;
struct Assembler;
class AssemblerStorage;
struct FuncDef;
class FuncDefStorage;
struct App;
class AppStorage;
struct Lexer;
class LexerStorage;

// DECLARATIONS



	
	// Call stack frame (return info)


	// VM state



		// Execution state (persistent across RunSteps calls)













class CallInfoStorage : public std::enable_shared_from_this<CallInfoStorage> {
	public: Int32 ReturnPC; // where to continue in caller (PC index)
	public: Int32 ReturnBase; // caller's base pointer (stack index)
	public: Int32 ReturnFuncIndex; // caller's function index in functions list
	public: Int32 CopyResultToReg; // register number to copy result to, or -1
	public: Value LocalVarMap; // VarMap representing locals, if any
	public: Value OuterVarMap; // VarMap representing outer variables (closure context)
	public: CallInfoStorage(Int32 returnPC, Int32 returnBase, Int32 returnFuncIndex, Int32 copyToReg=-1);
	public: Boolean DebugMode = false;
	private: List<Value> stack;
	private: List<Value> names; // Variable names parallel to stack (null if unnamed)
	private: List<CallInfo> callStack;
	private: Int32 callStackTop; // Index of next free call stack slot
	private: List<FuncDef> functions; // functions addressed by CALLF
	private: Int32 _currentFuncIndex = -1;
	public: Int32 StackSize();
}; // end of class CallInfoStorage

struct CallInfo {
	protected: std::shared_ptr<CallInfoStorage> storage;
  public:
	CallInfo(std::shared_ptr<CallInfoStorage> stor) : storage(stor) {}
	CallInfo() : storage(nullptr) {}
	friend bool IsNull(CallInfo inst) { return inst.storage == nullptr; }
	private: CallInfoStorage* get() { return static_cast<CallInfoStorage*>(storage.get()); }

	public: Int32 ReturnPC() { return get()->ReturnPC; } // where to continue in caller (PC index)
	public: void set_ReturnPC(Int32 _v) { get()->ReturnPC = _v; } // where to continue in caller (PC index)
	public: Int32 ReturnBase() { return get()->ReturnBase; } // caller's base pointer (stack index)
	public: void set_ReturnBase(Int32 _v) { get()->ReturnBase = _v; } // caller's base pointer (stack index)
	public: Int32 ReturnFuncIndex() { return get()->ReturnFuncIndex; } // caller's function index in functions list
	public: void set_ReturnFuncIndex(Int32 _v) { get()->ReturnFuncIndex = _v; } // caller's function index in functions list
	public: Int32 CopyResultToReg() { return get()->CopyResultToReg; } // register number to copy result to, or -1
	public: void set_CopyResultToReg(Int32 _v) { get()->CopyResultToReg = _v; } // register number to copy result to, or -1
	public: Value LocalVarMap() { return get()->LocalVarMap; } // VarMap representing locals, if any
	public: void set_LocalVarMap(Value _v) { get()->LocalVarMap = _v; } // VarMap representing locals, if any
	public: Value OuterVarMap() { return get()->OuterVarMap; } // VarMap representing outer variables (closure context)
	public: void set_OuterVarMap(Value _v) { get()->OuterVarMap = _v; } // VarMap representing outer variables (closure context)
	public: CallInfo(Int32 returnPC, Int32 returnBase, Int32 returnFuncIndex, Int32 copyToReg=-1) : CallInfo(std::make_shared<CallInfoStorage>(returnPC, returnBase, returnFuncIndex, copyToReg)) {}
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
	private: Int32 _currentFuncIndex() { return get()->_currentFuncIndex; }
	private: void set__currentFuncIndex(Int32 _v) { get()->_currentFuncIndex = _v; }
}; // end of struct CallInfo


// INLINE METHODS

