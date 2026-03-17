using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
using static MiniScript.ValueHelpers;
// H: #include "value.h"
// CPP: #include "VM.g.h"

namespace MiniScript {

// H: class VMStorage;
using VMRef = VM; // H: typedef VMStorage& VMRef;

// Context passed to native (intrinsic) callback functions.  This gives the
// intrinsic function access to the call arguments, as well as the VM.
public struct Context {
	public VMRef vm;
	public List<Value> stack;
	public Int32 baseIndex;		// index of return register; arguments follow this
	public Int32 argCount;		// how many arguments we have
	
	//*** BEGIN CS_ONLY ***
	public Context(VMRef vm, List<Value> stack, Int32 baseIndex, Int32 argCount) {
		this.vm = vm;
		this.stack = stack;
		this.baseIndex = baseIndex;
		this.argCount = argCount;
	}
	//*** END CS_ONLY ***
	/*** BEGIN H_ONLY ***
	public: Context(VMRef vm, List<Value> stack, Int32 baseIndex, Int32 argCount)
		: vm(vm), stack(stack), baseIndex(baseIndex), argCount(argCount) {
	}
	*** END H_ONLY ***/
	
	// Get an argument from the stack, by number (the first argument is index 0,
	// the second is index 1, etc.).
	[MethodImpl(AggressiveInlining)]
	public Value GetArg(int zeroBasedIndex) {
		// The base index is the position of the return register; the arguments
		// start right after that.  Note that we don't do any range checking here;
		// be careful not to ask for arguments beyond the declared parameters.
		return stack[baseIndex + 1 + zeroBasedIndex];
	}
	
	// Look up a variable or parameter by name.  This is provided mostly for
	// compatibility with MiniScript 1.x; in most cases, intrinsics only need
	// argument values, which are far more efficiently found via GetArg (above).
	public Value GetVar(String variableName) {
		return vm.LookupParamByName(variableName);
	}
}

// IntrinsicResult: represents the result of calling an intrinsic function
// (i.e. a function defined by the host app, for use in MiniScript code).
// This may be a final or "done" result, containing the return value of
// the intrinsic; or it may be a partial or "not done yet" result, in which
// case the intrinsic will be invoked again, with this partial result
// passed back so the intrinsic can continue its work.
public struct IntrinsicResult {
	public Boolean done;	// set to true if done, false if there is pending work
	public Value result;	// final result if done; in-progress data if not done

	public IntrinsicResult(Value result, Boolean done = true) {
		this.result = result;
		this.done = done;
	}

	// For backwards compatibility with 1.x:
	[MethodImpl(AggressiveInlining)]
	public Boolean Done() {
		return done;
	}

	// Some standard results you can efficiently use:
	public static readonly IntrinsicResult Null = new IntrinsicResult(val_null);
	public static readonly IntrinsicResult EmptyString = new IntrinsicResult(val_empty_string);
	public static readonly IntrinsicResult Zero = new IntrinsicResult(val_zero);
	public static readonly IntrinsicResult One = new IntrinsicResult(val_one);
}


}
