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
	
	public: Interpreter GetInterpreter();
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

	public: IntrinsicResult(Value result, Boolean done = Boolean(true));

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

// Wrap a host-built ValueDict as a persistent, GC-rooted map Value.  This is a
// convenience for the common MiniScript 1.x pattern of building a "module" map
// once and returning it from an intrinsic on every call.  The wrapper is cached
// (keyed on the address of the passed-in dictionary), so repeated calls with the
// same static ValueDict return the same map, and the map is added to the GC root
// set exactly once so its contents are never collected.  See
// notes/CPP_HOST_UPDATE_GUIDE.md.
Value StaticMap(ValueDict& d);

// Wrap a host-built ValueDict as a NON-rooted (collectable) map Value.  Unlike
// StaticMap, this does NOT add the map to the GC root set: it simply wraps the
// dictionary's storage in a Value and hands it back.  This is the right choice
// when an intrinsic builds a map on the fly and returns it to the caller, and
// you want that map (and its contents) to be freed once the caller no longer
// references it -- i.e. an ordinary dynamically-created return value, NOT a
// persistent "module"/"class"/prototype map.
//
// Rule of thumb:
//   * A map you build ONCE and return on every call (a module, a type/class
//     prototype used with `isa`)         -> StaticMap  (rooted, stable identity)
//   * A map you build fresh each call and return as data
//                                         -> DynamicMap (collectable)
//
// Like StaticMap, the parameter is a non-const reference, so a temporary won't
// bind; pass the lvalue ValueDict you built.  See notes/CPP_HOST_UPDATE_GUIDE.md.
Value DynamicMap(ValueDict& d);

// List counterpart of DynamicMap: wrap a host-built ValueList as a NON-rooted
// (collectable) list Value.  The right choice when an intrinsic builds a list on
// the fly and returns it, and you want it freed once the caller drops it.  (Note
// that, unlike DynamicMap which shares the dictionary's storage, this copies the
// element Values into a fresh GC list -- elements are cheap 8-byte handles, and
// there is no shared-storage attach path for lists as there is for maps.)  As
// with the map helpers, the parameter is a non-const reference so a temporary
// won't bind; pass the lvalue ValueList you built.
Value DynamicList(ValueList& items);
} // end of namespace MiniScript
