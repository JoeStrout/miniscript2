// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: ShellIntrinsics.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// ShellIntrinsics.cs - Intrinsics specific to the command-line shell environment.
// These are not part of the core MiniScript language; they are registered by the
// host app (App.cs) at startup via ShellIntrinsics.Init().

#include "value.h"
#include "GCManager.g.h"

namespace MiniScript {

// DECLARATIONS

class ShellIntrinsics {
	public: static Boolean ExitASAP;
	public: static Int32 ExitCode;
	private: static Value _shellArgs;
	private: static Value _envMap;

	// Populate _shellArgs from the given argument list, starting at startIdx.
	// Call this from App.MainProgram after parsing command-line switches.
	public: static void SetShellArgs(List<String> args, Int32 startIdx);

	// Build and cache the environment-variable map.
	// C# reads System.Environment; C++ reads the POSIX environ array.
	private: static Value GetEnvMap();

	// GC root callback — keep our cached Values alive.
	public: static void MarkRoots(object userData);

	// Drop cached values so they are rebuilt on next use.
	// Call from VM reset if needed.
	public: static void InvalidateCaches();

	// Register all shell intrinsics.  Must be called before any Interpreter is Reset.
	public: static void Init();
}; // end of struct ShellIntrinsics

// INLINE METHODS

} // end of namespace MiniScript
