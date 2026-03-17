// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CoreIntrinsics.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// CoreIntrinsics.cs - Definitions of all built-in intrinsic functions.

#include "value.h"

namespace MiniScript {

// DECLARATIONS

class CoreIntrinsics {

	
	// If given a nonzero seed, seed our PRNG accordingly.
	// Then (in either case), return the next random number drawn
	// from the range [0, 1) with a uniform distribution.
	private: static double GetNextRandom(int seed=0);

	private: static void AddIntrinsicToMap(Value map, String methodName);

	/// <summary>
	/// ListType: a static map that represents the `list` type, and provides
	/// intrinsic methods that can be invoked on it via dot syntax.
	/// </summary>
	public: static Value ListType();
	private: static Value _listType;

	/// <summary>
	/// StringType: a static map that represents the `string` type, and provides
	/// intrinsic methods that can be invoked on it via dot syntax.
	/// </summary>
	public: static Value StringType();
	private: static Value _stringType;

	/// <summary>
	/// MapType: a static map that represents the `map` type, and provides
	/// intrinsic methods that can be invoked on it via dot syntax.
	/// </summary>
	public: static Value MapType();
	private: static Value _mapType;
	
	/// <summary>
	/// NumberType: a static map that represents the `number` type.
	/// </summary>
	public: static Value NumberType();
	private: static Value _numberType;

	/// <summary>
	/// FunctionType: a static map that represents the `funcRef` type.
	/// </summary>
	public: static Value FunctionType();
	private: static Value _functionType;
	static void MarkRoots(void* user_data);

	public: static void Init();

	public: static void InvalidateTypeMaps();

}; // end of struct CoreIntrinsics

// INLINE METHODS

} // end of namespace MiniScript
