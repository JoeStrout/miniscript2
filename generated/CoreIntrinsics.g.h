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
typedef void (*VoidCallback)();

// DECLARATIONS

class CoreIntrinsics {

	public: static String BuildDate();
	
	public: static String PlatformName();
	public: static String hostName;
	public: static String hostInfo;
	public: static String hostVersion;

	// Host app identity — set these before the first call to `version`.

	// Coerce a numeric argument per the language's number/string policy.
	// Returns Value.Null on success (with `result` set); otherwise returns an
	// error Value to be returned from the intrinsic: a TypeError when the value
	// is the wrong type (not a number or string), or a FormatError when it is a
	// string that does not parse as a number.  (v is never an error: the VM
	// refuses error arguments before an intrinsic runs; see VM.RefusedErrorArg.)
	private: static Value RequireNumber(Value v, double* result);

	// Parse the longest prefix of s (after leading whitespace) that forms a
	// decimal number, as MiniScript 1.x's val did: "12abc" gives 12, and "abc"
	// gives 0.  Written out here, rather than leaning on a platform parser, so
	// that C# and C++ agree exactly.
	private: static Double ParseNumericPrefix(String s);

	// Convert an argument used as a substring (by remove, replace, etc.) to a
	// string, as `str` would; but null becomes the empty string.
	private: static Value ArgAsString(Value v, Context ctx);

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

	// ErrorType: a static map that represents the `error` type, and provides
	// intrinsic methods that can be invoked on an error via dot syntax
	// (notably `err` for creating a specialization).
	public: static Value ErrorType();
	private: static Value _errorType;
	private: static Intrinsic _errorErrIntr;
	private: static Value _EOL;
	public: static Value replInList;
	public: static Value replOutList;

	// REPL history lists, set by App.RunREPL at startup and by the reset intrinsic.

	public: static void MarkRoots(object user_data);

	public: static void Init();

	public: static Value IntrinsicsMap();
	private: static Value _intrinsicsMap;

	public: static Value GCMap();
	private: static Value _gcMap;
	private: static Intrinsic _gcCollectIntr;
	private: static Intrinsic _gcStatsIntr;
	private: static Value _versionMap;
	
	private: static List<VoidCallback> _invalidateCallbacks;

	public: static void RegisterInvalidateCallback(VoidCallback callback);

	public: static void InvalidateTypeMaps();

}; // end of struct CoreIntrinsics

// INLINE METHODS

} // end of namespace MiniScript
