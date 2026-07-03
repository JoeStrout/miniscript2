using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
// H: #include "value.h"
// CPP: #include "VM.g.h"
// CPP: #include "Interpreter.g.h"
// CPP: #include <unordered_map>

namespace MiniScript {

// H: class VMStorage;
using VMRef = VM; // H: typedef VMStorage& VMRef;

// Delegate method for text output.  This is used for `print` output,
// and also for errors and REPL implicit results.
// H: typedef void (*TextOutputMethod)(String, Boolean);
// H: inline bool IsNull(TextOutputMethod f) { return f == nullptr; }
public delegate void TextOutputMethod(String output, Boolean addLineBreak); // CPP:

// Context passed to native (intrinsic) callback functions.  This gives the
// intrinsic function access to the call arguments, as well as the VM.
public struct Context {
	public VMRef vm;			// virtual machine
	public List<Value> stack;	// VM value stack
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
	
	public Interpreter GetInterpreter() {
		return vm.GetInterpreter();
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
	public static readonly IntrinsicResult Null = new IntrinsicResult(Value.Null);
	public static readonly IntrinsicResult EmptyString = new IntrinsicResult(Value.emptyString);
	public static readonly IntrinsicResult Zero = new IntrinsicResult(Value.zero);
	public static readonly IntrinsicResult One = new IntrinsicResult(Value.one);
}

/*** BEGIN H_ONLY ***
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
*** END H_ONLY ***/

/*** BEGIN CPP_ONLY ***
namespace MiniScript {
	Value StaticMap(ValueDict& d) {
		// Key the cache on the address of the caller's ValueDict.  Because the
		// parameter is a non-const reference, a temporary won't bind, so the caller
		// must pass a stable lvalue (its static ValueDict); the address is valid even
		// when the dictionary is empty, avoiding the null-storage collision we would
		// get from keying on the underlying table pointer.
		static std::unordered_map<const void*, Value> cache;
		const void* key = &d;
		auto it = cache.find(key);
		if (it != cache.end()) return it->second;
	
		Value m = GCManager::NewMapFromDict(d);   // shares d's storage; no entry copy
		GCManager::AddRoot(m);                     // keep the map (and contents) alive
		cache[key] = m;
		return m;
	}

	Value DynamicMap(ValueDict& d) {
		// Wrap d's storage in a map Value and hand it back WITHOUT rooting it.
		// The returned map shares d's underlying storage (so it stays alive as
		// long as the Value references it), but because it is not a GC root, it
		// becomes collectable as soon as the caller drops its last reference.
		return GCManager::NewMapFromDict(d);
	}

	Value DynamicList(ValueList& items) {
		// Build a fresh GC list from the host list's elements and hand it back
		// WITHOUT rooting it, so it is collectable once the caller drops it.
		// (No shared-storage attach path exists for lists as it does for maps,
		// so we copy the element Values -- each a cheap 8-byte handle.)
		Value lst = Value::make_list(items.Count());
		for (int i = 0; i < items.Count(); i++) lst.Push(items[i]);
		return lst;
	}
}
*** END CPP_ONLY ***/


}
