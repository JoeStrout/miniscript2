// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: ShellIntrinsics.cs

#include "ShellIntrinsics.g.h"
#include "Intrinsic.g.h"
#include "IntrinsicAPI.g.h"
#include "VM.g.h"
#include "Parser.g.h"
#include "CodeGenerator.g.h"
#include "CodeEmitter.g.h"
#include "AST.g.h"
#include "FuncDef.g.h"
#include "value_list.h"
#include "value_map.h"
#include "value_string.h"
#include "StringUtils.g.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#ifndef _WIN32
    #include <sys/wait.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif
#include <cerrno>
extern "C" {
extern char **environ;
}

namespace MiniScript {

Boolean ShellIntrinsics::ExitASAP = Boolean(false);
Int32 ShellIntrinsics::ExitCode = 0;
Value ShellIntrinsics::_shellArgs = val_null;
Value ShellIntrinsics::_envMap = val_null;
// C++ exec state: parallel arrays capped at 64 concurrent jobs.
static FILE* _cppExecPipes[64];
static String _cppExecOutputs[64];
static Int32 _cppExecStatus[64];
static bool _cppExecDone[64];
static Int32 _execJobCount = 0;
void ShellIntrinsics::SetShellArgs(List<String> args,Int32 startIdx) {
	Int32 count = args.Count() - startIdx;
	if (count < 0) count = 0;
	_shellArgs = make_list(count);
	for (Int32 i = startIdx; i < args.Count(); i++) {
		list_push(_shellArgs, make_string(args[i]));
	}
	freeze_value(_shellArgs);
}
Value ShellIntrinsics::GetEnvMap() {
	if (!is_null(_envMap)) return _envMap;
	_envMap = make_map(64);
	for (char **current = environ; *current; current++) {
		char* eqPos = strchr(*current, '=');
		if (!eqPos) continue;
		String varName(*current, (size_t)(eqPos - *current));
		String valueStr(eqPos + 1);
		map_set(_envMap, make_string(varName), make_string(valueStr));
	}
	return _envMap;
}
void ShellIntrinsics::MarkRoots(object userData) {
	GCManager::Mark(_shellArgs);
	GCManager::Mark(_envMap);
}
void ShellIntrinsics::InvalidateCaches() {
	_shellArgs = val_null;
	_envMap = val_null;
}
void ShellIntrinsics::SyncEnvMap() {
	MapIterator iter = map_iterator(_envMap);
	Value iterKey, iterVal;
	while (map_iterator_next(&iter, &iterKey, &iterVal)) {
		String key = as_cstring(iterKey);
		String val = as_cstring(iterVal);
		#ifdef _WIN32
			_putenv_s(key.c_str(), val.c_str());
		#else
			setenv(key.c_str(), val.c_str(), 1);
		#endif
	}
}
Value ShellIntrinsics::BeginExec(String cmd) {
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
}
IntrinsicResult ShellIntrinsics::FinishExec(Value handle) {
	Int32 idx = (Int32)as_double(handle);
	String output = "";
	String errors = "";
	Int32 status = 0;
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
	// Trim one trailing \n or \r\n from output and errors (matching MS1 behavior).
	if (output.EndsWith("\r\n")) output = output.Substring(0, output.Length() - 2);
	else if (output.EndsWith("\n")) output = output.Substring(0, output.Length() - 1);
	if (errors.EndsWith("\r\n")) errors = errors.Substring(0, errors.Length() - 2);
	else if (errors.EndsWith("\n")) errors = errors.Substring(0, errors.Length() - 1);
	Value result = make_map(3);
	map_set(result, make_string("output"), make_string(output));
	map_set(result, make_string("errors"), make_string(errors));
	map_set(result, make_string("status"), make_double(status));
	return IntrinsicResult(result, Boolean(true));
}
List<String> ShellIntrinsics::SplitOn(String s,String delim) {
	List<String> result =  List<String>::New();
	String remaining = s;
	while (remaining.Length() > 0) {
		Int32 pos = remaining.IndexOf(delim);
		if (pos < 0) {
			result.Add(remaining);
			remaining = "";
		} else {
			if (pos > 0) result.Add(remaining.Substring(0, pos));
			remaining = remaining.Substring(pos + delim.Length());
		}
	}
	return result;
}
String ShellIntrinsics::ExpandVariables(String path) {
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
		if (map_try_get(envMap, make_string(varName), &varVal)) repl = as_cstring(varVal);
		path = path.Substring(0, p0) + repl + path.Substring(p1 + 1);
		p0 = path.IndexOf("${");
	}
	// Handle $VAR form (name = alphanumerics + underscore)
	p0 = path.IndexOf("$");
	while (p0 >= 0 && p0 < path.Length() - 1) {
		Int32 p1 = p0 + 1;
		while (p1 < path.Length()) {
			Int32 code = (Int32)(unsigned char)path.AtB(p1);
			if (!((code >= (Int32)'A' && code <= (Int32)'Z') || (code >= (Int32)'a' && code <= (Int32)'z')
					|| (code >= (Int32)'0' && code <= (Int32)'9') || code == (Int32)'_')) break;
			p1++;
		}
		if (p1 > p0 + 1) {
			varName = path.Substring(p0 + 1, p1 - p0 - 1);
			String repl = "";
			if (map_try_get(envMap, make_string(varName), &varVal)) repl = as_cstring(varVal);
			path = path.Substring(0, p0) + repl + path.Substring(p1);
			p0 = path.IndexOf("$");
		} else {
			break;  // lone $ with no valid name; stop
		}
	}
	return path;
}
String ShellIntrinsics::TryReadSource(String path) {
	FILE* handle = fopen(path.c_str(), "r");
	if (!handle) return String("");
	String result;
	char buf[4096];
	while (fgets(buf, sizeof(buf), handle)) result += buf;
	fclose(handle);
	return result;
}
void ShellIntrinsics::Init() {
	GCManager::RegisterMarkCallback(ShellIntrinsics::MarkRoots, nullptr);

	Intrinsic f;

	// exit(resultCode=null) — stop the VM and signal the host to exit.
	f = Intrinsic::Create("exit");
	f.AddParam("resultCode", val_null);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		ShellIntrinsics::ExitASAP = Boolean(true);
		Value resultCode = ctx.GetArg(0);
		if (!is_null(resultCode)) {
			ShellIntrinsics::ExitCode = (Int32)as_double(resultCode);
		}
		ctx.vm.Stop();
		return IntrinsicResult::Null;
	});

	// shellArgs — return the command-line arguments following the script path.
	f = Intrinsic::Create("shellArgs");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(_shellArgs);
	});

	// env — return a map of all environment variables.
	// Mutations to this map are synced to the real process environment inside `exec`.
	f = Intrinsic::Create("env");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(GetEnvMap());
	});

	// exec(cmd) — run a shell command, returning a map with:
	//   "output"  — stdout captured as a string
	//   "errors"  — stderr captured as a string (C# only; empty in C++)
	//   "status"  — exit code as a number
	// Uses the partialResult mechanism so the VM is not blocked while waiting.
	f = Intrinsic::Create("exec");
	f.AddParam("cmd", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		if (!partialResult.done) {
			return FinishExec(partialResult.result);
		}
		String cmd = as_cstring(ctx.GetArg(0));
		if (!is_null(_envMap)) {
			SyncEnvMap();
		}
		Value handle = BeginExec(cmd);
		return FinishExec(handle);
	});

	// import(libname) — load and run a MiniScript module, then store its locals
	// under the library name in the caller's scope.  Uses the partialResult
	// mechanism and ManuallyPushCall so the module runs inside the current VM.
	// Search path comes from the MS_IMPORT_PATH env var (colon-separated);
	// defaults to "$MS_SCRIPT_DIR:$MS_SCRIPT_DIR/lib:$MS_EXE_DIR/lib".
	f = Intrinsic::Create("import");
	f.AddParam("libname", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		if (!partialResult.done) {
			// Phase 2: the module finished running.  Store its locals map
			// under the library name in the caller's scope, and also return
			// it as the result value (for the `foo = import("foo")` form).
			String lib = as_cstring(partialResult.result);
			Value importedValues = ctx.vm.ManualCallResult;
			ctx.vm.SetVar(lib, importedValues);
			return IntrinsicResult(importedValues, Boolean(true));
		}
		// Phase 1: find, parse, compile, and push the module.
		String libname = as_cstring(ctx.GetArg(0));
		if (libname.Length() == 0) {
			ctx.vm.RaiseRuntimeError("import: libname required");
			return IntrinsicResult::Null;
		}
		// Determine the search path.
		Value pathVal;
		String searchPath;
		if (!map_try_get(GetEnvMap(), make_string("MS_IMPORT_PATH"), &pathVal) || is_null(pathVal)) {
			searchPath = "$MS_SCRIPT_DIR:$MS_SCRIPT_DIR/lib:$MS_EXE_DIR/lib";
		} else {
			searchPath = as_cstring(pathVal);
		}
		List<String> libDirs = SplitOn(searchPath, ":");
		// Find and read the module source file.
		String source = "";
		Boolean found = Boolean(false);
		for (Int32 i = 0; i < libDirs.Count(); i++) {
			String dir = libDirs[i];
			if (dir.Length() == 0) dir = ".";
			else if (!dir.EndsWith("/") && !dir.EndsWith("\\")) dir += "/";
			String path = ExpandVariables(dir) + libname + ".ms";
			String src = TryReadSource(path);
			if (src.Length() == 0) continue;
			source = src;
			found = Boolean(true);
			break;
		}
		if (!found) {
			ctx.vm.RaiseRuntimeError(StringUtils::Format("import: library not found: {0}", libname));
			return IntrinsicResult::Null;
		}
		// Parse the module source.
		Parser p =  Parser::New();
		p.Init(source);
		List<ASTNode> stmts = p.ParseProgram();
		if (p.HadError()) {
			ctx.vm.RaiseRuntimeError(StringUtils::Format("import: parse error in {0}.ms", libname));
			return IntrinsicResult::Null;
		}
		// Simplify AST (constant folding, etc.).
		for (Int32 i = 0; i < stmts.Count(); i++) {
			stmts[i] = stmts[i].Simplify();
		}
		// Compile the module to bytecode.
		BytecodeEmitter emitter =  BytecodeEmitter::New();
		CodeGenerator gen =  CodeGenerator::New(emitter);
		gen.set_FileName(libname + ".ms");
		gen.set_FunctionIndexOffset(ctx.vm.FunctionCount());
		List<FuncDef> fns = gen.CompileImport(stmts, libname + ".ms");
		if (!is_null(gen.Error())) {
			ctx.vm.RaiseRuntimeError(StringUtils::Format("import: compile error in {0}.ms", libname));
			return IntrinsicResult::Null;
		}
		// Push the module call; we will be re-invoked when it finishes.
		ctx.vm.ManuallyPushCall(ctx.baseIndex, fns);
		return IntrinsicResult(make_string(libname), Boolean(false));
	});
}

} // end of namespace MiniScript
