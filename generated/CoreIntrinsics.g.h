// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CoreIntrinsics.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// CoreIntrinsics.cs - Definitions of all built-in intrinsic functions.

#include "value.h"
#include "ErrorTypes.g.h"
#include "GCManager.g.h"

namespace MiniScript {

// DECLARATIONS

class CoreIntrinsics {

	
	// If given a nonzero seed, seed our PRNG accordingly.
	// Then (in either case), return the next random number drawn
	// from the range [0, 1) with a uniform distribution.
	private: static double GetNextRandom(int seed=0);

	private: static void AddIntrinsicToMap(Value map, String methodName);

	// 
	// ListType: a static map that represents the `list` type, and provides
	// intrinsic methods that can be invoked on it via dot syntax.
	// 
	public: static Value ListType();
	private: static Value _listType;

	// 
	// StringType: a static map that represents the `string` type, and provides
	// intrinsic methods that can be invoked on it via dot syntax.
	// 
	public: static Value StringType();
	private: static Value _stringType;

	// 
	// MapType: a static map that represents the `map` type, and provides
	// intrinsic methods that can be invoked on it via dot syntax.
	// 
	public: static Value MapType();
	private: static Value _mapType;
	
	// 
	// NumberType: a static map that represents the `number` type.
	// 
	public: static Value NumberType();
	private: static Value _numberType;

	// 
	// FunctionType: a static map that represents the `funcRef` type.
	// 
	public: static Value FunctionType();
	private: static Value _functionType;

	// 
	// ErrorType: a static map that represents the `error` type, and provides
	// intrinsic methods that can be invoked on an error via dot syntax
	// (notably `err` for creating a specialization).
	// 
	public: static Value ErrorType();
	private: static Value _errorType;
	private: static Value _EOL;
	public: static Value replInList;
	public: static Value replOutList;

	// REPL history lists, set by App.RunREPL at startup and by the reset intrinsic.

	public: static void MarkRoots(object user_data);

	public: static void Init();

	public: static void InvalidateTypeMaps();

}; // end of struct CoreIntrinsics

// INLINE METHODS

} // end of namespace MiniScript
