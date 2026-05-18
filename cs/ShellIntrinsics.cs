// ShellIntrinsics.cs - Intrinsics specific to the command-line shell environment.
// These are not part of the core MiniScript language; they are registered by the
// host app (App.cs) at startup via ShellIntrinsics.Init().

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// H: #include "value.h"
// H: #include "GCManager.g.h"
// CPP: #include "Intrinsic.g.h"
// CPP: #include "IntrinsicAPI.g.h"
// CPP: #include "VM.g.h"
// CPP: #include "value_list.h"
// CPP: #include "value_map.h"
// CPP: #include "value_string.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include <cstdlib>
// CPP: #include <cstring>

/*** BEGIN CPP_ONLY ***
extern "C" {
	extern char **environ;
}
*** END CPP_ONLY ***/

namespace MiniScript {

public static class ShellIntrinsics {
	public static Boolean ExitASAP = false;
	public static Int32 ExitCode = 0;
	private static Value _shellArgs = val_null;
	private static Value _envMap = val_null;

	// Populate _shellArgs from the given argument list, starting at startIdx.
	// Call this from App.MainProgram after parsing command-line switches.
	public static void SetShellArgs(List<String> args, Int32 startIdx) {
		Int32 count = args.Count - startIdx;
		if (count < 0) count = 0;
		_shellArgs = make_list(count);
		for (Int32 i = startIdx; i < args.Count; i++) {
			list_push(_shellArgs, make_string(args[i]));
		}
		freeze_value(_shellArgs);
	}

	// Build and cache the environment-variable map.
	// C# reads System.Environment; C++ reads the POSIX environ array.
	private static Value GetEnvMap() {
		if (!is_null(_envMap)) return _envMap;
		_envMap = make_map(64);
		//*** BEGIN CS_ONLY ***
		System.Collections.IDictionary envVars = System.Environment.GetEnvironmentVariables();
		foreach (System.Collections.DictionaryEntry entry in envVars) {
			String k = entry.Key as String;
			String v = entry.Value as String;
			if (k != null && v != null) {
				map_set(_envMap, make_string(k), make_string(v));
			}
		}
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		for (char **current = environ; *current; current++) {
			char* eqPos = strchr(*current, '=');
			if (!eqPos) continue;
			String varName(*current, (size_t)(eqPos - *current));
			String valueStr(eqPos + 1);
			map_set(_envMap, make_string(varName), make_string(valueStr));
		}
		*** END CPP_ONLY ***/
		return _envMap;
	}

	// GC root callback — keep our cached Values alive.
	public static void MarkRoots(object userData) {
		GCManager.Mark(_shellArgs);
		GCManager.Mark(_envMap);
	}

	// Drop cached values so they are rebuilt on next use.
	// Call from VM reset if needed.
	public static void InvalidateCaches() {
		_shellArgs = val_null;
		_envMap = val_null;
	}

	// Register all shell intrinsics.  Must be called before any Interpreter is Reset.
	public static void Init() {
		GCManager.RegisterMarkCallback(MarkRoots, null); // CPP: GCManager::RegisterMarkCallback(ShellIntrinsics::MarkRoots, nullptr);

		Intrinsic f;

		// exit(resultCode=null) — stop the VM and signal the host to exit.
		f = Intrinsic.Create("exit");
		f.AddParam("resultCode", val_null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			ShellIntrinsics.ExitASAP = true;
			Value resultCode = ctx.GetArg(0);
			if (!is_null(resultCode)) {
				ShellIntrinsics.ExitCode = (Int32)as_double(resultCode);
			}
			ctx.vm.Stop();
			return IntrinsicResult.Null;
		};

		// shellArgs — return the command-line arguments following the script path.
		f = Intrinsic.Create("shellArgs");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(_shellArgs);
		};

		// env — return a map of all environment variables.
		// NOTE: Currently, mutating this map does NOT update the corresponding
		// shell environment variables, unlike MS1.  ToDo: solve this, either
		// by using an assignment override, or by applying them on every call
		// to `exec`.
		f = Intrinsic.Create("env");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(GetEnvMap());
		};
	}
}

}
