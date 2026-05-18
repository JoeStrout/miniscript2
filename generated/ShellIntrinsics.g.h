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

	// Push all entries in _envMap to the real process environment.
	// Only called when _envMap is non-null (i.e., the user has accessed `env`).
	private: static void SyncEnvMap();

	// Launch a shell subprocess and return a job-index handle (as a double Value).
	private: static Value BeginExec(String cmd);

	// Poll the job identified by `handle`. Reads available output without blocking.
	// Returns done=true with result map when the subprocess has exited, or
	// done=false with the same handle so the VM will call us again.
	private: static IntrinsicResult FinishExec(Value handle);

	// Split a string on a single-character (string) delimiter, returning all parts
	// (empty parts are skipped).
	private: static List<String> SplitOn(String s, String delim);

	// Expand shell variable references ($VAR, ${VAR}) in a path string,
	// looking up values in the cached env map.
	private: static String ExpandVariables(String path);

	// Try to read a source file. Returns the file contents if the file exists,
	// or null (empty string in C++) if it does not.  Used by the import intrinsic.
	private: static String TryReadSource(String path);

	// Register all shell intrinsics.  Must be called before any Interpreter is Reset.
	public: static void Init();
}; // end of struct ShellIntrinsics

// INLINE METHODS

} // end of namespace MiniScript
