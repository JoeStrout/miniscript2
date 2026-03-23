// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IntrinsicAPI.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
#include "value.h"

namespace MiniScript {
class VMStorage;
typedef VMStorage& VMRef;
typedef void (*TextOutputMethod)(String, Boolean);
inline bool IsNull(TextOutputMethod f) { return f == nullptr; }

// DECLARATIONS

// Delegate method for text output.  This is used for `print` output,
// and also for errors and REPL implicit results.

// Context passed to native (intrinsic) callback functions.  This gives the
// intrinsic function access to the call arguments, as well as the VM.
struct Context {
	public: VMRef vm; // virtual machine
	public: List<Value> stack; // VM value stack
	public: Int32 baseIndex; // index of return register; arguments follow this
	public: Int32 argCount; // how many arguments we have
	public: Context(VMRef vm, List<Value> stack, Int32 baseIndex, Int32 argCount)
		: vm(vm), stack(stack), baseIndex(baseIndex), argCount(argCount) {
	}
	
	
	// Get an argument from the stack, by number (the first argument is index 0,
	// the second is index 1, etc.).
	public: Value GetArg(int zeroBasedIndex);
	
	// Look up a variable or parameter by name.  This is provided mostly for
	// compatibility with MiniScript 1.x; in most cases, intrinsics only need
	// argument values, which are far more efficiently found via GetArg (above).
	public: Value GetVar(String variableName);
	
	public: Interpreter Interpreter();
}; // end of struct Context

// IntrinsicResult: represents the result of calling an intrinsic function
// (i.e. a function defined by the host app, for use in MiniScript code).
// This may be a final or "done" result, containing the return value of
// the intrinsic; or it may be a partial or "not done yet" result, in which
// case the intrinsic will be invoked again, with this partial result
// passed back so the intrinsic can continue its work.
struct IntrinsicResult {
	public: Boolean done; // set to true if done, false if there is pending work
	public: Value result; // final result if done; in-progress data if not done

	public: IntrinsicResult(Value result, Boolean done = true);

	// For backwards compatibility with 1.x:
	public: Boolean Done();
	public: static const IntrinsicResult Null;
	public: static const IntrinsicResult EmptyString;
	public: static const IntrinsicResult Zero;
	public: static const IntrinsicResult One;

	// Some standard results you can efficiently use:
}; // end of struct IntrinsicResult

// INLINE METHODS

inline Value Context::GetArg(int zeroBasedIndex) {
	// The base index is the position of the return register; the arguments
	// start right after that.  Note that we don't do any range checking here;
	// be careful not to ask for arguments beyond the declared parameters.
	return stack[baseIndex + 1 + zeroBasedIndex];
}

inline Boolean IntrinsicResult::Done() {
	return done;
}

} // end of namespace MiniScript
