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
// CPP: #include "Parser.g.h"
// CPP: #include "CodeGenerator.g.h"
// CPP: #include "CodeEmitter.g.h"
// CPP: #include "AST.g.h"
// CPP: #include "FuncDef.g.h"
// CPP: #include "value_list.h"
// CPP: #include "value_map.h"
// CPP: #include "value_string.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include <cstdlib>
// CPP: #include <cstring>
// CPP: #include <cstdio>

/*** BEGIN CPP_ONLY ***
#ifndef _WIN32
    #include <sys/wait.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif
#include <cerrno>
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

	//*** BEGIN CS_ONLY ***
	// C# exec state: one slot per running job; slots accumulate and are never reused.
	private static List<System.Diagnostics.Process> _csExecProcs = null;
	private static List<String> _csExecOutputs = null;
	private static List<String> _csExecErrors = null;
	private static List<Boolean> _csExecOutDone = null;
	private static List<Boolean> _csExecErrDone = null;
	//*** END CS_ONLY ***
	/*** BEGIN CPP_ONLY ***
	// C++ exec state: parallel arrays capped at 64 concurrent jobs.
	static FILE* _cppExecPipes[64];
	static String _cppExecOutputs[64];
	static Int32 _cppExecStatus[64];
	static bool _cppExecDone[64];
	static Int32 _execJobCount = 0;
	*** END CPP_ONLY ***/

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

	// Push all entries in _envMap to the real process environment.
	// Only called when _envMap is non-null (i.e., the user has accessed `env`).
	private static void SyncEnvMap() {
		MapIterator iter = map_iterator(_envMap);
		// CPP: Value iterKey, iterVal;
		while (map_iterator_next(ref iter)) { // CPP: while (map_iterator_next(&iter, &iterKey, &iterVal)) {
			String key = as_cstring(iter.Key); // CPP: String key = as_cstring(iterKey);
			String val = as_cstring(iter.Val); // CPP: String val = as_cstring(iterVal);
			//*** BEGIN CS_ONLY ***
			System.Environment.SetEnvironmentVariable(key, val);
			//*** END CS_ONLY ***
			/*** BEGIN CPP_ONLY ***
			#ifdef _WIN32
				_putenv_s(key.c_str(), val.c_str());
			#else
				setenv(key.c_str(), val.c_str(), 1);
			#endif
			*** END CPP_ONLY ***/
		}
	}

	// Launch a shell subprocess and return a job-index handle (as a double Value).
	private static Value BeginExec(String cmd) {
		//*** BEGIN CS_ONLY ***
		if (_csExecProcs == null) {
			_csExecProcs = new List<System.Diagnostics.Process>();
			_csExecOutputs = new List<String>();
			_csExecErrors = new List<String>();
			_csExecOutDone = new List<Boolean>();
			_csExecErrDone = new List<Boolean>();
		}
		System.Diagnostics.ProcessStartInfo psi;
		Boolean isWindows = System.Runtime.InteropServices.RuntimeInformation.IsOSPlatform(
			System.Runtime.InteropServices.OSPlatform.Windows);
		if (isWindows) {
			psi = new System.Diagnostics.ProcessStartInfo("cmd.exe", "/c " + cmd);
		} else {
			psi = new System.Diagnostics.ProcessStartInfo("/bin/sh", "-c " + cmd);
		}
		psi.RedirectStandardOutput = true;
		psi.RedirectStandardError = true;
		psi.UseShellExecute = false;
		psi.CreateNoWindow = true;
		System.Diagnostics.Process proc = System.Diagnostics.Process.Start(psi);
		Int32 idx = _csExecProcs.Count;
		_csExecProcs.Add(proc);
		_csExecOutputs.Add("");
		_csExecErrors.Add("");
		_csExecOutDone.Add(false);
		_csExecErrDone.Add(false);
		// Read stdout and stderr on background threads to prevent pipe-buffer deadlock.
		System.Threading.Tasks.Task.Run(() => {
			_csExecOutputs[idx] = proc.StandardOutput.ReadToEnd();
			_csExecOutDone[idx] = true;
		});
		System.Threading.Tasks.Task.Run(() => {
			_csExecErrors[idx] = proc.StandardError.ReadToEnd();
			_csExecErrDone[idx] = true;
		});
		return make_double(idx);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		Int32 idx = _execJobCount++;
		_cppExecOutputs[idx] = "";
		_cppExecStatus[idx] = 0;
		_cppExecDone[idx] = false;
		#ifdef _WIN32
			_cppExecPipes[idx] = _popen(cmd.c_str(), "r");
		#else
			_cppExecPipes[idx] = popen(cmd.c_str(), "r");
			if (_cppExecPipes[idx]) {
				fcntl(fileno(_cppExecPipes[idx]), F_SETFL, O_NONBLOCK);
			}
		#endif
		return make_double(idx);
		*** END CPP_ONLY ***/
	}

	// Poll the job identified by `handle`. Reads available output without blocking.
	// Returns done=true with result map when the subprocess has exited, or
	// done=false with the same handle so the VM will call us again.
	private static IntrinsicResult FinishExec(Value handle) {
		Int32 idx = (Int32)as_double(handle);
		String output = "";
		String errors = "";
		Int32 status = 0;
		//*** BEGIN CS_ONLY ***
		if (!(_csExecOutDone[idx] && _csExecErrDone[idx])) {
			return new IntrinsicResult(handle, false);
		}
		_csExecProcs[idx].WaitForExit();
		output = _csExecOutputs[idx];
		errors = _csExecErrors[idx];
		status = _csExecProcs[idx].ExitCode;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		if (!_cppExecDone[idx] && _cppExecPipes[idx]) {
			char buf[256];
			#ifdef _WIN32
				int n = (int)fread(buf, 1, sizeof(buf) - 1, _cppExecPipes[idx]);
				if (n > 0) { buf[n] = '\0'; _cppExecOutputs[idx] += buf; }
				else if (feof(_cppExecPipes[idx])) {
					_cppExecStatus[idx] = _pclose(_cppExecPipes[idx]);
					_cppExecPipes[idx] = nullptr;
					_cppExecDone[idx] = true;
				}
			#else
				ssize_t n = read(fileno(_cppExecPipes[idx]), buf, sizeof(buf) - 1);
				if (n > 0) { buf[n] = '\0'; _cppExecOutputs[idx] += buf; }
				else if (n == 0 || (n < 0 && errno != EAGAIN)) {
					int ws = pclose(_cppExecPipes[idx]);
					_cppExecStatus[idx] = WIFEXITED(ws) ? WEXITSTATUS(ws) : -1;
					_cppExecPipes[idx] = nullptr;
					_cppExecDone[idx] = true;
				}
			#endif
		}
		if (!_cppExecDone[idx]) return IntrinsicResult(handle, false);
		output = _cppExecOutputs[idx];
		status = _cppExecStatus[idx];
		*** END CPP_ONLY ***/
		// Trim one trailing \n or \r\n from output and errors (matching MS1 behavior).
		if (output.EndsWith("\r\n")) output = output.Substring(0, output.Length - 2);
		else if (output.EndsWith("\n")) output = output.Substring(0, output.Length - 1);
		if (errors.EndsWith("\r\n")) errors = errors.Substring(0, errors.Length - 2);
		else if (errors.EndsWith("\n")) errors = errors.Substring(0, errors.Length - 1);
		Value result = make_map(3);
		map_set(result, make_string("output"), make_string(output));
		map_set(result, make_string("errors"), make_string(errors));
		map_set(result, make_string("status"), make_double(status));
		return new IntrinsicResult(result, true);
	}

	// Split a string on a single-character (string) delimiter, returning all parts
	// (empty parts are skipped).
	private static List<String> SplitOn(String s, String delim) {
		List<String> result = new List<String>();
		String remaining = s;
		while (remaining.Length > 0) {
			Int32 pos = remaining.IndexOf(delim);
			if (pos < 0) {
				result.Add(remaining);
				remaining = "";
			} else {
				if (pos > 0) result.Add(remaining.Substring(0, pos));
				remaining = remaining.Substring(pos + delim.Length);
			}
		}
		return result;
	}

	// Expand shell variable references ($VAR, ${VAR}) in a path string,
	// looking up values in the cached env map.
	private static String ExpandVariables(String path) {
		Value envMap = GetEnvMap();
		Value varVal;
		String varName;
		// Handle ${VAR} form
		Int32 p0 = path.IndexOf("${");
		while (p0 >= 0) {
			Int32 p1 = path.IndexOf("}", p0 + 1);
			if (p1 < 0) break;
			varName = path.Substring(p0 + 2, p1 - p0 - 2);
			String repl = "";
			if (map_try_get(envMap, make_string(varName), out varVal)) repl = as_cstring(varVal);
			path = path.Substring(0, p0) + repl + path.Substring(p1 + 1);
			p0 = path.IndexOf("${");
		}
		// Handle $VAR form (name = alphanumerics + underscore)
		p0 = path.IndexOf("$");
		while (p0 >= 0 && p0 < path.Length - 1) {
			Int32 p1 = p0 + 1;
			while (p1 < path.Length) {
				Int32 code = (Int32)path[p1]; // CPP: Int32 code = (Int32)(unsigned char)path.AtB(p1);
				if (!((code >= (Int32)'A' && code <= (Int32)'Z') || (code >= (Int32)'a' && code <= (Int32)'z')
						|| (code >= (Int32)'0' && code <= (Int32)'9') || code == (Int32)'_')) break;
				p1++;
			}
			if (p1 > p0 + 1) {
				varName = path.Substring(p0 + 1, p1 - p0 - 1);
				String repl = "";
				if (map_try_get(envMap, make_string(varName), out varVal)) repl = as_cstring(varVal);
				path = path.Substring(0, p0) + repl + path.Substring(p1);
				p0 = path.IndexOf("$");
			} else {
				break;  // lone $ with no valid name; stop
			}
		}
		return path;
	}

	// Try to read a source file. Returns the file contents if the file exists,
	// or null (empty string in C++) if it does not.  Used by the import intrinsic.
	private static String TryReadSource(String path) {
		//*** BEGIN CS_ONLY ***
		if (!System.IO.File.Exists(path)) return null;
		return System.IO.File.ReadAllText(path);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		FILE* handle = fopen(path.c_str(), "r");
		if (!handle) return String("");
		String result;
		char buf[4096];
		while (fgets(buf, sizeof(buf), handle)) result += buf;
		fclose(handle);
		return result;
		*** END CPP_ONLY ***/
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
		// Mutations to this map are synced to the real process environment inside `exec`.
		f = Intrinsic.Create("env");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(GetEnvMap());
		};

		// exec(cmd) — run a shell command, returning a map with:
		//   "output"  — stdout captured as a string
		//   "errors"  — stderr captured as a string (C# only; empty in C++)
		//   "status"  — exit code as a number
		// Uses the partialResult mechanism so the VM is not blocked while waiting.
		f = Intrinsic.Create("exec");
		f.AddParam("cmd", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			if (!partialResult.done) {
				return FinishExec(partialResult.result);
			}
			String cmd = as_cstring(ctx.GetArg(0));
			if (!is_null(_envMap)) {
				SyncEnvMap();
			}
			Value handle = BeginExec(cmd);
			return FinishExec(handle);
		};

		// import(libname) — load and run a MiniScript module, then store its locals
		// under the library name in the caller's scope.  Uses the partialResult
		// mechanism and ManuallyPushCall so the module runs inside the current VM.
		// Search path comes from the MS_IMPORT_PATH env var (colon-separated);
		// defaults to "$MS_SCRIPT_DIR:$MS_SCRIPT_DIR/lib:$MS_EXE_DIR/lib".
		f = Intrinsic.Create("import");
		f.AddParam("libname", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			if (!partialResult.done) {
				// Phase 2: the module finished running.  Store its locals map
				// under the library name in the caller's scope, and also return
				// it as the result value (for the `foo = import("foo")` form).
				String lib = as_cstring(partialResult.result);
				Value importedValues = ctx.vm.ManualCallResult;
				ctx.vm.SetVar(lib, importedValues);
				return new IntrinsicResult(importedValues, true);
			}
			// Phase 1: find, parse, compile, and push the module.
			String libname = as_cstring(ctx.GetArg(0));
			if (libname.Length == 0) {
				ctx.vm.RaiseRuntimeError("import: libname required");
				return IntrinsicResult.Null;
			}
			// Determine the search path.
			Value pathVal;
			String searchPath;
			if (!map_try_get(GetEnvMap(), make_string("MS_IMPORT_PATH"), out pathVal) || is_null(pathVal)) {
				searchPath = "$MS_SCRIPT_DIR:$MS_SCRIPT_DIR/lib:$MS_EXE_DIR/lib";
			} else {
				searchPath = as_cstring(pathVal);
			}
			List<String> libDirs = SplitOn(searchPath, ":");
			// Find and read the module source file.
			String source = "";
			Boolean found = false;
			for (Int32 i = 0; i < libDirs.Count; i++) {
				String dir = libDirs[i];
				if (dir.Length == 0) dir = ".";
				else if (!dir.EndsWith("/") && !dir.EndsWith("\\")) dir += "/";
				String path = ExpandVariables(dir) + libname + ".ms";
				String src = TryReadSource(path);
				if (src == null) continue; // CPP: if (src.Length() == 0) continue;
				source = src;
				found = true;
				break;
			}
			if (!found) {
				ctx.vm.RaiseRuntimeError(StringUtils.Format("import: library not found: {0}", libname));
				return IntrinsicResult.Null;
			}
			// Parse the module source.
			Parser p = new Parser();
			p.Init(source);
			List<ASTNode> stmts = p.ParseProgram();
			if (p.HadError()) {
				ctx.vm.RaiseRuntimeError(StringUtils.Format("import: parse error in {0}.ms", libname));
				return IntrinsicResult.Null;
			}
			// Simplify AST (constant folding, etc.).
			for (Int32 i = 0; i < stmts.Count; i++) {
				stmts[i] = stmts[i].Simplify();
			}
			// Compile the module to bytecode.
			BytecodeEmitter emitter = new BytecodeEmitter();
			CodeGenerator gen = new CodeGenerator(emitter);
			gen.FileName = libname + ".ms";
			gen.FunctionIndexOffset = ctx.vm.FunctionCount();
			List<FuncDef> fns = gen.CompileImport(stmts, libname + ".ms");
			if (!is_null(gen.Error)) {
				ctx.vm.RaiseRuntimeError(StringUtils.Format("import: compile error in {0}.ms", libname));
				return IntrinsicResult.Null;
			}
			// Push the module call; we will be re-invoked when it finishes.
			ctx.vm.ManuallyPushCall(ctx.baseIndex, fns);
			return new IntrinsicResult(make_string(libname), false);
		};
	}
}

}
