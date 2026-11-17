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

	// Built-in type map templates.
	// ListType, StringType, etc. here are process-wide *templates*.  Scripts
	// never see them: each global namespace makes its own shallow copy of a
	// template the first time it needs it (see Globals.ListType), so changes a
	// script makes to `list`, `string`, etc. stay within that program, and
	// `reset` can discard them.  A host may add methods to a template to make
	// them available to every program; do that before running scripts, since a
	// namespace that has already copied a template does not see later changes.
	// ListType: the template for the `list` type map, which provides the
	// intrinsic methods that can be invoked on a list via dot syntax.
	public: static Value ListType();
	private: static Value _listType;

	// 
	// StringType: the template for the `string` type map, which provides the
	// intrinsic methods that can be invoked on a string via dot syntax.
	// 
	public: static Value StringType();
	private: static Value _stringType;

	// 
	// MapType: the template for the `map` type map, which provides the
	// intrinsic methods that can be invoked on a map via dot syntax.
	// 
	public: static Value MapType();
	private: static Value _mapType;
	
	// 
	// NumberType: the template for the `number` type map.
	// 
	public: static Value NumberType();
	private: static Value _numberType;

	// 
	// FunctionType: the template for the `funcRef` type map.
	// 
	public: static Value FunctionType();
	private: static Value _functionType;

	// ErrorType: the template for the `error` type map, which provides the
	// intrinsic methods that can be invoked on an error via dot syntax
	// (notably `err` for creating a specialization).
	public: static Value ErrorType();
	private: static Value _errorType;
	private: static Intrinsic _errorErrIntr;
	private: static PerProgram _listTypePP;
	private: static PerProgram _stringTypePP;
	private: static PerProgram _mapTypePP;
	private: static PerProgram _numberTypePP;
	private: static PerProgram _functionTypePP;
	private: static PerProgram _errorTypePP;

	// ── Each program's own type maps ─────────────────────────────────────────
	// Each program gets a shallow copy of each template above, the first time it
	// needs one (see PerProgram.cs).  The VM reaches them through these
	// XxxTypeIn methods (see VM.ListType and friends).

	private: static Value CopyListType();

	private: static Value CopyStringType();

	private: static Value CopyMapType();

	private: static Value CopyNumberType();

	private: static Value CopyFunctionType();

	private: static Value CopyErrorType();

	private: static void EnsureTypeMapPerPrograms();

	public: static Value ListTypeIn(Globals g);

	public: static Value StringTypeIn(Globals g);

	public: static Value MapTypeIn(Globals g);

	public: static Value NumberTypeIn(Globals g);

	public: static Value FunctionTypeIn(Globals g);

	public: static Value ErrorTypeIn(Globals g);
	private: static Value _EOL;
	public: static Value replInList;
	public: static Value replOutList;

	// REPL history lists, set by App.RunREPL at startup and by the reset intrinsic.

	public: static void MarkRoots(object user_data);

	public: static void Init();

	public: static Value GCMap();
	private: static Value _gcMap;
	private: static Intrinsic _gcCollectIntr;
	private: static Intrinsic _gcStatsIntr;
	private: static Value _versionMap;
	
	private: static List<VoidCallback> _invalidateCallbacks;

	public: static void RegisterInvalidateCallback(VoidCallback callback);

	// Discard the type map templates (and other cached maps), so they are rebuilt
	// from the current intrinsics on next use.  This affects only namespaces
	// that have not yet copied a template; to reset a running program's type
	// maps, use Globals.ResetTypeMaps (as the `reset` intrinsic does).
	public: static void InvalidateTypeMaps();

}; // end of struct CoreIntrinsics

// INLINE METHODS

} // end of namespace MiniScript
