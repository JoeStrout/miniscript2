// ShellIntrinsics.cs - Intrinsics specific to the command-line shell environment.
// These are not part of the core MiniScript language; they are registered by the
// host app (App.cs) at startup via ShellIntrinsics.Init().

using System;
using System.Collections.Generic;
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
// CPP: #include "CS_value_util.h"
// CPP: #include "CoreIntrinsics.g.h"
// CPP: #include "DateTimeUtils.g.h"
// CPP: #include "keyboard.h"
// CPP: #include <cstdlib>
// CPP: #include <cstring>
// CPP: #include <cstdio>

/*** BEGIN CPP_ONLY ***
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #include <sys/stat.h>
    #ifndef getcwd
        #define getcwd _getcwd
    #endif
    #ifndef chdir
        #define chdir _chdir
    #endif
    #define PATHSEP "\\"
#else
    #include <sys/wait.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <libgen.h>
    #include <sys/stat.h>
    #include <stdlib.h>
    #if defined(__APPLE__) || defined(__FreeBSD__)
        #include <copyfile.h>
    #else
        #include <sys/sendfile.h>
    #endif
    #define PATHSEP "/"
	extern "C" {
		extern char **environ;
	}
#endif
#include <cerrno>
#include <ctime>
*** END CPP_ONLY ***/

namespace MiniScript {

public static class ShellIntrinsics {
	public static Boolean ExitASAP = false;
	public static Int32 ExitCode = 0;
	private static Value _shellArgs = Value.Null;
	private static Value _envMap = Value.Null;

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

	// ── File module static fields ─────────────────────────────────────────────
	private static Value _fileModuleMap = Value.Null;
	private static Value _fileHandleClassMap = Value.Null;
	private static Value _rawDataClassMap = Value.Null;
	private static Int32 _rdStart = -1, _fhStart = -1, _fmStart = -1;
	private static List<String> _rdKeys = null;
	private static List<String> _fhKeys = null;
	private static List<String> _fmKeys = null;

	// ── Key module static fields ──────────────────────────────────────────────
	private static Value _keyModuleMap = Value.Null;
	private static Int32 _keyStart = -1;
	private static List<String> _keyKeys = null;

	// Platform-specific wrapper types for FileHandle and RawData.
	//*** BEGIN CS_ONLY ***
	private class CsFileHandle { public System.IO.FileStream Stream; }
	private class CsRawBuf {
		public byte[] Bytes;
		public Int32 Length { get { return Bytes != null ? Bytes.Length : 0; } }
	}
	//*** END CS_ONLY ***
	/*** BEGIN CPP_ONLY ***
	struct CppFileHandle { FILE* f; };
	struct CppRawBuf    { uint8_t* bytes; int length; };
	*** END CPP_ONLY ***/

	// Populate _shellArgs from the given argument list, starting at startIdx.
	// Call this from App.MainProgram after parsing command-line switches.
	public static void SetShellArgs(List<String> args, Int32 startIdx) {
		Int32 count = args.Count - startIdx;
		if (count < 0) count = 0;
		_shellArgs = Value.make_list(count);
		for (Int32 i = startIdx; i < args.Count; i++) {
			Value.list_push(_shellArgs, Value.make_string(args[i]));
		}
		Value.freeze_value(_shellArgs);
	}

	// Build and cache the environment-variable map.
	// C# reads System.Environment; C++ reads the POSIX environ array.
	private static Value GetEnvMap() {
		if (!_envMap.IsNull()) return _envMap;
		_envMap = Value.make_map(64);
		//*** BEGIN CS_ONLY ***
		System.Collections.IDictionary envVars = System.Environment.GetEnvironmentVariables();
		foreach (System.Collections.DictionaryEntry entry in envVars) {
			String k = entry.Key as String;
			String v = entry.Value as String;
			if (k != null && v != null) {
				Value.map_set(_envMap, k, v);
			}
		}
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		for (char **current = environ; *current; current++) {
			char* eqPos = strchr(*current, '=');
			if (!eqPos) continue;
			String varName(*current, (size_t)(eqPos - *current));
			String valueStr(eqPos + 1);
			Value::map_set(_envMap, Value::make_string(varName), Value::make_string(valueStr));
		}
		*** END CPP_ONLY ***/
		return _envMap;
	}

	// GC root callback — keep our cached Values alive.
	public static void MarkRoots(object userData) {
		GCManager.Mark(_shellArgs);
		GCManager.Mark(_envMap);
		GCManager.Mark(_fileModuleMap);
		GCManager.Mark(_fileHandleClassMap);
		GCManager.Mark(_rawDataClassMap);
		GCManager.Mark(_keyModuleMap);
	}

	// Drop cached values so they are rebuilt on next use.
	// Call from VM reset if needed.
	public static void InvalidateCaches() {
		_shellArgs = Value.Null;
		_envMap = Value.Null;
		_fileModuleMap = Value.Null;
		_fileHandleClassMap = Value.Null;
		_rawDataClassMap = Value.Null;
		_keyModuleMap = Value.Null;
	}

	// Push all entries in _envMap to the real process environment.
	// Only called when _envMap is non-null (i.e., the user has accessed `env`).
	private static void SyncEnvMap() {
		MapIterator iter = Value.map_iterator(_envMap);
		// CPP: Value iterKey, iterVal;
		while (Value.map_iterator_next(ref iter)) { // CPP: while (map_iterator_next(&iter, &iterKey, &iterVal)) {
			String key = Value.as_cstring(iter.Key); // CPP: String key = Value::as_cstring(iterKey);
			String val = Value.as_cstring(iter.Val); // CPP: String val = Value::as_cstring(iterVal);
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
		System.Diagnostics.Process proc;
		try { proc = System.Diagnostics.Process.Start(psi); }
		catch (Exception) { return Value.Null; }
		if (proc == null) return Value.Null;
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
		return new Value(idx);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		Int32 idx = _execJobCount++;
		_cppExecOutputs[idx] = "";
		_cppExecStatus[idx] = 0;
		_cppExecDone[idx] = false;
		// If the `key` module has the terminal in raw mode, drop to cooked first
		// so the child process (which inherits this terminal on stdin/stderr)
		// gets normal echo and line editing. Left cooked afterward; the next
		// key operation re-enters raw mode.
		Keyboard::EnterCookedMode();
		#ifdef _WIN32
			_cppExecPipes[idx] = _popen(cmd.c_str(), "r");
		#else
			_cppExecPipes[idx] = popen(cmd.c_str(), "r");
			if (_cppExecPipes[idx]) {
				fcntl(fileno(_cppExecPipes[idx]), F_SETFL, O_NONBLOCK);
			}
		#endif
		if (!_cppExecPipes[idx]) return Value::null;
		return Value(idx);
		*** END CPP_ONLY ***/
	}

	// Poll the job identified by `handle`. Reads available output without blocking.
	// Returns done=true with result map when the subprocess has exited, or
	// done=false with the same handle so the VM will call us again.
	private static IntrinsicResult FinishExec(Value handle) {
		Int32 idx = (Int32)handle.DoubleValue();
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
		Value result = Value.make_map(3);
		Value.map_set(result, "output", output);
		Value.map_set(result, "errors", errors);
		Value.map_set(result, "status", new Value(status));
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
			if (Value.map_try_get(envMap, Value.make_string(varName), out varVal)) repl = Value.as_cstring(varVal);
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
				if (Value.map_try_get(envMap, Value.make_string(varName), out varVal)) repl = Value.as_cstring(varVal);
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

	// ── GCHandle finalizers ───────────────────────────────────────────────────

	private static void FileHandleFinalizer(object userData) {
		//*** BEGIN CS_ONLY ***
		CsFileHandle h = userData as CsFileHandle;
		if (h != null && h.Stream != null) {
			try { h.Stream.Close(); } catch (Exception) {}
			h.Stream = null;
		}
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppFileHandle* h = (CppFileHandle*)userData;
		if (h) { if (h->f) fclose(h->f); delete h; }
		*** END CPP_ONLY ***/
	}

	private static void RawBufFinalizer(object userData) {
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)userData;
		if (b) { free(b->bytes); delete b; }
		*** END CPP_ONLY ***/
		// C#: byte[] is managed; nothing explicit needed.
	}

	// ── CS-only buffer-access helpers ────────────────────────────────────────
	//*** BEGIN CS_ONLY ***
	private static CsFileHandle GetCsFileHandle(Value handleVal) {
		if (!handleVal.IsHandle()) return null;
		GCHandle slot = GCManager.GetHandle(handleVal);
		return slot.UserData as CsFileHandle;
	}
	private static CsRawBuf GetCsRawBuf(Value handleVal) {
		if (!handleVal.IsHandle()) return null;
		GCHandle slot = GCManager.GetHandle(handleVal);
		return slot.UserData as CsRawBuf;
	}
	//*** END CS_ONLY ***

	// ── RawData buffer helpers ────────────────────────────────────────────────

	// Allocate a GCHandle wrapping a zero-filled buffer of `size` bytes.
	private static Value AllocRawBuf(Int32 size) {
		//*** BEGIN CS_ONLY ***
		CsRawBuf buf = new CsRawBuf();
		buf.Bytes = size > 0 ? new byte[size] : null;
		return GCManager.NewHandle(buf, RawBufFinalizer);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* buf = new CppRawBuf();
		buf->bytes = size > 0 ? (uint8_t*)calloc(size, 1) : nullptr;
		buf->length = size;
		return GCManager::NewHandle(buf, RawBufFinalizer);
		*** END CPP_ONLY ***/
	}

	// Return the byte length of the buffer in `handleVal`, or -1 if invalid.
	private static Int32 GetRawBufLen(Value handleVal) {
		if (!handleVal.IsHandle()) return -1;
		//*** BEGIN CS_ONLY ***
		CsRawBuf buf = GetCsRawBuf(handleVal);
		return buf != null ? buf.Length : -1;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* buf = (CppRawBuf*)GCManager::GetHandle(handleVal).UserData;
		return buf ? buf->length : -1;
		*** END CPP_ONLY ***/
	}

	// Allocate new buffer of `newSize` bytes, copy old data, update self._handle.
	private static void ResizeRawBuf(Value self, Int32 newSize) {
		Value oldHandle = Value.Null;
		Value.map_try_get(self, Value.make_string("_handle"), out oldHandle);
		Value newHandle = AllocRawBuf(newSize);
		if (newHandle.IsNull()) return;
		// Copy up to min(old, new) bytes from old buffer to new one.
		if (!oldHandle.IsNull() && oldHandle.IsHandle()) {
			Int32 oldLen = GetRawBufLen(oldHandle);
			if (oldLen > 0 && newSize > 0) {
				Int32 copyLen = oldLen < newSize ? oldLen : newSize;
				//*** BEGIN CS_ONLY ***
				CsRawBuf ob = GetCsRawBuf(oldHandle);
				CsRawBuf nb = GetCsRawBuf(newHandle);
				if (ob != null && ob.Bytes != null && nb != null && nb.Bytes != null)
					System.Array.Copy(ob.Bytes, nb.Bytes, copyLen);
				//*** END CS_ONLY ***
				/*** BEGIN CPP_ONLY ***
				CppRawBuf* ob = (CppRawBuf*)GCManager::GetHandle(oldHandle).UserData;
				CppRawBuf* nb = (CppRawBuf*)GCManager::GetHandle(newHandle).UserData;
				if (ob && ob->bytes && nb && nb->bytes) memcpy(nb->bytes, ob->bytes, copyLen);
				*** END CPP_ONLY ***/
			}
		}
		Value.map_set(self, "_handle", newHandle);
	}

	// Create a new RawData instance map backed by `handleVal`.
	private static Value NewRawDataInstance(Value handleVal) {
		Value instance = Value.make_map(4);
		Value.map_set(instance, Value.magicIsA, GetRawDataClassMap());
		Value.map_set(instance, "_handle", handleVal);
		Value.map_set(instance, "littleEndian", Value.one);
		return instance;
	}

	// ── RawData typed accessors ───────────────────────────────────────────────
	// Each getter returns -1 (or 0.0) on invalid handle/offset.
	// Each setter silently no-ops on invalid inputs.

	private static Int32 RawGetByte(Value h, Int32 off) {
		if (!h.IsHandle()) return -1;
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		return (b != null && b.Bytes != null) ? (Int32)(Byte)b.Bytes[off] : -1;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
		return (b && b->bytes) ? (Int32)(Byte)b->bytes[off] : -1;
		*** END CPP_ONLY ***/
	}
	private static void RawSetByte(Value h, Int32 off, Int32 val) {
		if (!h.IsHandle()) return;
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		if (b != null && b.Bytes != null) b.Bytes[off] = (byte)val;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
		if (b && b->bytes) b->bytes[off] = (uint8_t)val;
		*** END CPP_ONLY ***/
	}
	private static Int32 RawGetSByte(Value h, Int32 off) {
		if (!h.IsHandle()) return -1;
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		return (b != null && b.Bytes != null) ? (Int32)(SByte)b.Bytes[off] : -1;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
		return (b && b->bytes) ? (Int32)(int8_t)b->bytes[off] : -1;
		*** END CPP_ONLY ***/
	}
	private static void RawSetSByte(Value h, Int32 off, Int32 val) {
		if (!h.IsHandle()) return;
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		if (b != null && b.Bytes != null) b.Bytes[off] = (byte)(SByte)val;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
		if (b && b->bytes) b->bytes[off] = (uint8_t)(int8_t)val;
		*** END CPP_ONLY ***/
	}
	private static Int32 RawGetU16(Value h, Int32 off, Boolean le) {
		if (!h.IsHandle()) return -1;
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		if (b == null || b.Bytes == null) return -1;
		UInt16 v = le ? (UInt16)((UInt16)b.Bytes[off] | ((UInt16)b.Bytes[off+1] << 8))
		              : (UInt16)(((UInt16)b.Bytes[off] << 8) | (UInt16)b.Bytes[off+1]);
		return (Int32)v;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
		if (!b || !b->bytes) return -1;
		UInt16 v = le ? ((UInt16)b->bytes[off] | ((UInt16)b->bytes[off+1] << 8))
		              : (((UInt16)b->bytes[off] << 8) | (UInt16)b->bytes[off+1]);
		return (Int32)v;
		*** END CPP_ONLY ***/
	}
	private static void RawSetU16(Value h, Int32 off, Int32 val, Boolean le) {
		if (!h.IsHandle()) return;
		UInt16 v = (UInt16)val;
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		if (b == null || b.Bytes == null) return;
		if (le) { b.Bytes[off] = (byte)(v & 0xFF); b.Bytes[off+1] = (byte)(v >> 8); }
		else    { b.Bytes[off] = (byte)(v >> 8);   b.Bytes[off+1] = (byte)(v & 0xFF); }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
		if (!b || !b->bytes) return;
		if (le) { b->bytes[off] = (uint8_t)(v & 0xFF); b->bytes[off+1] = (uint8_t)(v >> 8); }
		else    { b->bytes[off] = (uint8_t)(v >> 8);   b->bytes[off+1] = (uint8_t)(v & 0xFF); }
		*** END CPP_ONLY ***/
	}
	private static Int32 RawGetI16(Value h, Int32 off, Boolean le) {
		return (Int32)(Int16)RawGetU16(h, off, le);
	}
	private static void RawSetI16(Value h, Int32 off, Int32 val, Boolean le) {
		RawSetU16(h, off, (Int32)(UInt16)(Int16)val, le);
	}
	private static Double RawGetU32(Value h, Int32 off, Boolean le) {
		if (!h.IsHandle()) return 0.0;
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		if (b == null || b.Bytes == null) return 0.0;
		UInt32 v = le
			? ((UInt32)b.Bytes[off] | ((UInt32)b.Bytes[off+1]<<8) | ((UInt32)b.Bytes[off+2]<<16) | ((UInt32)b.Bytes[off+3]<<24))
			: (((UInt32)b.Bytes[off]<<24) | ((UInt32)b.Bytes[off+1]<<16) | ((UInt32)b.Bytes[off+2]<<8) | (UInt32)b.Bytes[off+3]);
		return (Double)v;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
		if (!b || !b->bytes) return 0.0;
		UInt32 v = le
			? ((UInt32)b->bytes[off] | ((UInt32)b->bytes[off+1]<<8) | ((UInt32)b->bytes[off+2]<<16) | ((UInt32)b->bytes[off+3]<<24))
			: (((UInt32)b->bytes[off]<<24) | ((UInt32)b->bytes[off+1]<<16) | ((UInt32)b->bytes[off+2]<<8) | (UInt32)b->bytes[off+3]);
		return (Double)v;
		*** END CPP_ONLY ***/
	}
	private static void RawSetU32(Value h, Int32 off, Double dval, Boolean le) {
		if (!h.IsHandle()) return;
		UInt32 v = (UInt32)dval;
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		if (b == null || b.Bytes == null) return;
		if (le) {
			b.Bytes[off]=(byte)(v&0xFF);
			b.Bytes[off+1]=(byte)((v>>8)&0xFF);
			b.Bytes[off+2]=(byte)((v>>16)&0xFF);
			b.Bytes[off+3]=(byte)(v>>24);
		} else {
			b.Bytes[off]=(byte)(v>>24);
			b.Bytes[off+1]=(byte)((v>>16)&0xFF);
			b.Bytes[off+2]=(byte)((v>>8)&0xFF);
			b.Bytes[off+3]=(byte)(v&0xFF);
		}
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
		if (!b || !b->bytes) return;
		if (le) {
			b->bytes[off]=(uint8_t)(v&0xFF); b->bytes[off+1]=(uint8_t)((v>>8)&0xFF);
			b->bytes[off+2]=(uint8_t)((v>>16)&0xFF); b->bytes[off+3]=(uint8_t)(v>>24);
		} else {
			b->bytes[off]=(uint8_t)(v>>24); b->bytes[off+1]=(uint8_t)((v>>16)&0xFF);
			b->bytes[off+2]=(uint8_t)((v>>8)&0xFF); b->bytes[off+3]=(uint8_t)(v&0xFF);
		}
		*** END CPP_ONLY ***/
	}
	private static Int32 RawGetI32(Value h, Int32 off, Boolean le) {
		return (Int32)(UInt32)RawGetU32(h, off, le);
	}
	private static void RawSetI32(Value h, Int32 off, Int32 val, Boolean le) {
		RawSetU32(h, off, (Double)(UInt32)val, le);
	}
	private static Double RawGetF32(Value h, Int32 off, Boolean le) {
		//*** BEGIN CS_ONLY ***
		Int32 bits = RawGetI32(h, off, le);
		return (Double)BitConverter.Int32BitsToSingle(bits);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		UInt32 bits = (UInt32)RawGetU32(h, off, le);
		Single fval;
		memcpy(&fval, &bits, sizeof(Single));
		return (Double)fval;
		*** END CPP_ONLY ***/
	}
	private static void RawSetF32(Value h, Int32 off, Double dval, Boolean le) {
		//*** BEGIN CS_ONLY ***
		Int32 bits = BitConverter.SingleToInt32Bits((Single)dval);
		RawSetI32(h, off, bits, le);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		Single fval = (Single)dval;
		UInt32 bits;
		memcpy(&bits, &fval, sizeof(Single));
		RawSetU32(h, off, (Double)bits, le);
		*** END CPP_ONLY ***/
	}
	private static Double RawGetF64(Value h, Int32 off, Boolean le) {
		if (!h.IsHandle()) return 0.0;
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		if (b == null || b.Bytes == null) return 0.0;
		Int64 bits = le
			? ((Int64)b.Bytes[off] | ((Int64)b.Bytes[off+1]<<8) | ((Int64)b.Bytes[off+2]<<16) | ((Int64)b.Bytes[off+3]<<24)
			   | ((Int64)b.Bytes[off+4]<<32) | ((Int64)b.Bytes[off+5]<<40) | ((Int64)b.Bytes[off+6]<<48) | ((Int64)b.Bytes[off+7]<<56))
			: (((Int64)b.Bytes[off]<<56) | ((Int64)b.Bytes[off+1]<<48) | ((Int64)b.Bytes[off+2]<<40) | ((Int64)b.Bytes[off+3]<<32)
			   | ((Int64)b.Bytes[off+4]<<24) | ((Int64)b.Bytes[off+5]<<16) | ((Int64)b.Bytes[off+6]<<8) | (Int64)b.Bytes[off+7]);
		return BitConverter.Int64BitsToDouble(bits);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
		if (!b || !b->bytes) return 0.0;
		UInt64 bits = le
			? ((UInt64)b->bytes[off] | ((UInt64)b->bytes[off+1]<<8) | ((UInt64)b->bytes[off+2]<<16) | ((UInt64)b->bytes[off+3]<<24)
			   | ((UInt64)b->bytes[off+4]<<32) | ((UInt64)b->bytes[off+5]<<40) | ((UInt64)b->bytes[off+6]<<48) | ((UInt64)b->bytes[off+7]<<56))
			: (((UInt64)b->bytes[off]<<56) | ((UInt64)b->bytes[off+1]<<48) | ((UInt64)b->bytes[off+2]<<40) | ((UInt64)b->bytes[off+3]<<32)
			   | ((UInt64)b->bytes[off+4]<<24) | ((UInt64)b->bytes[off+5]<<16) | ((UInt64)b->bytes[off+6]<<8) | (UInt64)b->bytes[off+7]);
		Double result;
		memcpy(&result, &bits, sizeof(Double));
		return result;
		*** END CPP_ONLY ***/
	}
	private static void RawSetF64(Value h, Int32 off, Double dval, Boolean le) {
		if (!h.IsHandle()) return;
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		if (b == null || b.Bytes == null) return;
		Int64 bits = BitConverter.DoubleToInt64Bits(dval);
		if (le) {
			for (Int32 i = 0; i < 8; i++) b.Bytes[off+i] = (byte)(bits >> (i*8));
		} else {
			for (Int32 i = 0; i < 8; i++) b.Bytes[off+i] = (byte)(bits >> (56 - i*8));
		}
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
		if (!b || !b->bytes) return;
		UInt64 bits;
		memcpy(&bits, &dval, sizeof(Double));
		if (le) {
			for (int i = 0; i < 8; i++) b->bytes[off+i] = (uint8_t)(bits >> (i*8));
		} else {
			for (int i = 0; i < 8; i++) b->bytes[off+i] = (uint8_t)(bits >> (56 - i*8));
		}
		*** END CPP_ONLY ***/
	}
	private static String RawGetUTF8(Value h, Int32 off, Int32 byteCount) {
		if (!h.IsHandle()) return "";
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		if (b == null || b.Bytes == null) return "";
		if (off < 0) return "";
		if (byteCount < 0) byteCount = b.Length - off;
		if (off + byteCount > b.Length) byteCount = b.Length - off;
		// Trim at null terminator.
		for (Int32 i = 0; i < byteCount; i++) {
			if (b.Bytes[off + i] == 0) { byteCount = i; break; }
		}
		return System.Text.Encoding.UTF8.GetString(b.Bytes, off, byteCount);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
		if (!b || !b->bytes || off < 0) return String("");
		if (byteCount < 0) byteCount = b->length - off;
		if (off + byteCount > b->length) byteCount = b->length - off;
		Int32 actual = 0;
		while (actual < byteCount && b->bytes[off + actual] != 0) actual++;
		return String((const char*)(b->bytes + off), actual);
		*** END CPP_ONLY ***/
	}
	private static Int32 RawSetUTF8(Value h, Int32 off, String val) {
		if (!h.IsHandle()) return 0;
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		if (b == null || b.Bytes == null || off < 0) return 0;
		byte[] encoded = System.Text.Encoding.UTF8.GetBytes(val);
		Int32 copyLen = encoded.Length < b.Length - off ? encoded.Length : b.Length - off;
		if (copyLen < 0) copyLen = 0;
		System.Array.Copy(encoded, 0, b.Bytes, off, copyLen);
		return copyLen;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
		if (!b || !b->bytes || off < 0) return 0;
		Int32 copyLen = (Int32)val.Length();
		if (off + copyLen > b->length) copyLen = b->length - off;
		if (copyLen < 0) copyLen = 0;
		if (copyLen > 0) memcpy(b->bytes + off, val.c_str(), copyLen);
		return copyLen;
		*** END CPP_ONLY ***/
	}

	// ── FileHandle helpers ────────────────────────────────────────────────────

	// Open a file and return a GCHandle value wrapping the native file handle.
	// Returns Value.Null on failure.
	private static Value MakeFileHandle(String path, String mode) {
		//*** BEGIN CS_ONLY ***
		System.IO.FileStream stream = null;
		try {
			if (mode == "r") {
				stream = new System.IO.FileStream(path, System.IO.FileMode.Open, System.IO.FileAccess.Read);
			} else if (mode == "w") {
				stream = new System.IO.FileStream(path, System.IO.FileMode.Create, System.IO.FileAccess.Write);
			} else if (mode == "a") {
				stream = new System.IO.FileStream(path, System.IO.FileMode.Append, System.IO.FileAccess.Write);
			} else {
				stream = new System.IO.FileStream(path, System.IO.FileMode.OpenOrCreate, System.IO.FileAccess.ReadWrite);
			}
		} catch (Exception) { return Value.Null; }
		CsFileHandle wrapper = new CsFileHandle();
		wrapper.Stream = stream;
		return GCManager.NewHandle(wrapper, FileHandleFinalizer);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		FILE* f = nullptr;
		if (mode == "r") {
			f = fopen(path.c_str(), "r");
		} else if (mode == "w") {
			f = fopen(path.c_str(), "w");
		} else if (mode == "a") {
			f = fopen(path.c_str(), "a");
		} else {
			f = fopen(path.c_str(), "r+");
			if (!f) f = fopen(path.c_str(), "w+");
		}
		if (!f) return Value::null;
		CppFileHandle* wrapper = new CppFileHandle();
		wrapper->f = f;
		return GCManager::NewHandle(wrapper, FileHandleFinalizer);
		*** END CPP_ONLY ***/
	}

	// Close the file and mark the handle as closed (set Stream/f to null).
	private static void CloseFileHandle(Value handleVal) {
		if (!handleVal.IsHandle()) return;
		//*** BEGIN CS_ONLY ***
		CsFileHandle h = GetCsFileHandle(handleVal);
		if (h != null && h.Stream != null) {
			try { h.Stream.Close(); } catch (Exception) {}
			h.Stream = null;
		}
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
		if (h && h->f) { fclose(h->f); h->f = nullptr; }
		*** END CPP_ONLY ***/
	}

	private static Boolean IsFileHandleOpen(Value handleVal) {
		if (!handleVal.IsHandle()) return false;
		//*** BEGIN CS_ONLY ***
		CsFileHandle h = GetCsFileHandle(handleVal);
		return h != null && h.Stream != null;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
		return h != nullptr && h->f != nullptr;
		*** END CPP_ONLY ***/
	}

	private static Int32 WriteToFile(Value handleVal, String data) {
		if (!handleVal.IsHandle()) return 0;
		//*** BEGIN CS_ONLY ***
		CsFileHandle h = GetCsFileHandle(handleVal);
		if (h == null || h.Stream == null) return 0;
		byte[] bytes = System.Text.Encoding.UTF8.GetBytes(data);
		h.Stream.Write(bytes, 0, bytes.Length);
		return bytes.Length;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
		if (!h || !h->f) return 0;
		return (Int32)fwrite(data.c_str(), 1, data.Length(), h->f);
		*** END CPP_ONLY ***/
	}

	// Read up to `byteCount` bytes (or all remaining if < 0). Returns string.
	private static String ReadFromFile(Value handleVal, Int32 byteCount) {
		if (!handleVal.IsHandle()) return "";
		//*** BEGIN CS_ONLY ***
		CsFileHandle h = GetCsFileHandle(handleVal);
		if (h == null || h.Stream == null) return "";
		if (byteCount == 0) return "";
		if (byteCount < 0) {
			// Read all remaining bytes.
			Int64 remaining = h.Stream.Length - h.Stream.Position;
			if (remaining <= 0) return "";
			byte[] all = new byte[remaining];
			h.Stream.Read(all, 0, (Int32)remaining);
			return System.Text.Encoding.UTF8.GetString(all);
		}
		byte[] buf = new byte[byteCount];
		Int32 n = h.Stream.Read(buf, 0, byteCount);
		return n > 0 ? System.Text.Encoding.UTF8.GetString(buf, 0, n) : "";
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
		if (!h || !h->f) return String("");
		if (byteCount == 0) return String("");
		String result;
		char buf[1024];
		long toRead = byteCount;
		while (!feof(h->f) && toRead != 0) {
			size_t n = fread(buf, 1, (toRead > 0 && toRead < 1024) ? (size_t)toRead : 1024, h->f);
			if (n == 0) break;
			result += String(buf, n);
			if (toRead > 0) toRead -= (long)n;
		}
		return result;
		*** END CPP_ONLY ***/
	}

	// Read the next line (stripped of trailing \n/\r\n). Returns Value.Null at EOF.
	private static Value ReadLineFromFile(Value handleVal) {
		if (!handleVal.IsHandle()) return Value.Null;
		//*** BEGIN CS_ONLY ***
		CsFileHandle h = GetCsFileHandle(handleVal);
		if (h == null || h.Stream == null) return Value.Null;
		var lineBytes = new System.Collections.Generic.List<byte>();
		Int32 b;
		Boolean anyRead = false;
		while ((b = h.Stream.ReadByte()) != -1) {
			anyRead = true;
			if (b == '\n') break;
			if (b != '\r') lineBytes.Add((byte)b);
		}
		if (!anyRead) return Value.Null;
		return Value.make_string(System.Text.Encoding.UTF8.GetString(lineBytes.ToArray()));
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
		if (!h || !h->f) return Value::null;
		char buf[4096];
		char* s = fgets(buf, sizeof(buf), h->f);
		if (!s) return Value::null;
		Int32 len = (Int32)strlen(buf);
		if (len > 0 && buf[len-1] == '\n') len--;
		if (len > 0 && buf[len-1] == '\r') len--;
		return Value::make_string(String(buf, len));
		*** END CPP_ONLY ***/
	}

	private static Int32 GetFilePosition(Value handleVal) {
		if (!handleVal.IsHandle()) return -1;
		//*** BEGIN CS_ONLY ***
		CsFileHandle h = GetCsFileHandle(handleVal);
		return (h != null && h.Stream != null) ? (Int32)h.Stream.Position : -1;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
		return (h && h->f) ? (Int32)ftell(h->f) : -1;
		*** END CPP_ONLY ***/
	}

	private static Boolean IsFileAtEnd(Value handleVal) {
		if (!handleVal.IsHandle()) return true;
		//*** BEGIN CS_ONLY ***
		CsFileHandle h = GetCsFileHandle(handleVal);
		return h == null || h.Stream == null || h.Stream.Position >= h.Stream.Length;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
		return !h || !h->f || feof(h->f) != 0;
		*** END CPP_ONLY ***/
	}

	private static void SeekFilePosition(Value handleVal, Int32 pos) {
		if (!handleVal.IsHandle()) return;
		//*** BEGIN CS_ONLY ***
		CsFileHandle h = GetCsFileHandle(handleVal);
		if (h != null && h.Stream != null && h.Stream.CanSeek) h.Stream.Seek(pos, System.IO.SeekOrigin.Begin);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
		if (h && h->f) fseek(h->f, pos, SEEK_SET);
		*** END CPP_ONLY ***/
	}

	// ── Filesystem helpers ────────────────────────────────────────────────────

	private static String FsCurrentDir() {
		//*** BEGIN CS_ONLY ***
		return System.IO.Directory.GetCurrentDirectory();
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		char buf[4096];
		return getcwd(buf, sizeof(buf)) ? String(buf) : String(".");
		*** END CPP_ONLY ***/
	}

	private static Boolean FsSetDir(String path) {
		//*** BEGIN CS_ONLY ***
		try { System.IO.Directory.SetCurrentDirectory(path); return true; }
		catch (Exception) { return false; }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		return chdir(path.c_str()) == 0;
		*** END CPP_ONLY ***/
	}

	private static Value FsChildren(String path) {
		if (path.Length == 0) path = FsCurrentDir(); // CPP: if (path.Length() == 0) path = FsCurrentDir();
		//*** BEGIN CS_ONLY ***
		try {
			String[] entries = System.IO.Directory.GetFileSystemEntries(path);
			Value result = Value.make_list(entries.Length);
			foreach (String entry in entries)
				Value.list_push(result, Value.make_string(System.IO.Path.GetFileName(entry)));
			return result;
		} catch (Exception ex) { return ErrorTypes.FileError("children: " + ex.Message); }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		Value result = Value::make_list(16);
		#ifdef _WIN32
			String searchPath = path + "\\*";
			WIN32_FIND_DATA data;
			HANDLE hFind = FindFirstFile(searchPath.c_str(), &data);
			if (hFind == INVALID_HANDLE_VALUE) {
				return ErrorTypes::FileError("children: cannot read directory: " + path);
			}
			do {
				String name(data.cFileName);
				if (name != "." && name != "..") Value::list_push(result, Value::make_string(name));
			} while (FindNextFile(hFind, &data) != 0);
			FindClose(hFind);
		#else
			DIR* dir = opendir(path.c_str());
			if (!dir) {
				return ErrorTypes::FileError("children: cannot read directory: " + path);
			}
			while (struct dirent* entry = readdir(dir)) {
				String name(entry->d_name);
				if (name != "." && name != "..") Value::list_push(result, Value::make_string(name));
			}
			closedir(dir);
		#endif
		return result;
		*** END CPP_ONLY ***/
	}

	private static String FsBasename(String path) {
		//*** BEGIN CS_ONLY ***
		return System.IO.Path.GetFileName(path);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		#ifdef _WIN32
			char nameBuf[256], extBuf[256];
			_splitpath_s(path.c_str(), nullptr, 0, nullptr, 0, nameBuf, sizeof(nameBuf), extBuf, sizeof(extBuf));
			return String(nameBuf) + String(extBuf);
		#else
			char tmp[4096];
			strncpy(tmp, path.c_str(), sizeof(tmp)-1);
			tmp[sizeof(tmp)-1] = 0;
			return String(basename(tmp));
		#endif
		*** END CPP_ONLY ***/
	}

	private static String FsDirname(String path) {
		//*** BEGIN CS_ONLY ***
		String d = System.IO.Path.GetDirectoryName(path);
		return d != null ? d : "";
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		#ifdef _WIN32
			char driveBuf[3], dirBuf[256];
			_splitpath_s(path.c_str(), driveBuf, sizeof(driveBuf), dirBuf, sizeof(dirBuf), nullptr, 0, nullptr, 0);
			return String(driveBuf) + String(dirBuf);
		#else
			char tmp[4096];
			strncpy(tmp, path.c_str(), sizeof(tmp)-1);
			tmp[sizeof(tmp)-1] = 0;
			return String(dirname(tmp));
		#endif
		*** END CPP_ONLY ***/
	}

	private static String FsChild(String parent, String child) {
		if (parent == "") return child;
		if (parent.EndsWith("/") || parent.EndsWith("\\")) return parent + child;
		//*** BEGIN CS_ONLY ***
		return parent + System.IO.Path.DirectorySeparatorChar + child;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		return parent + PATHSEP + child;
		*** END CPP_ONLY ***/
	}

	private static Boolean FsExists(String path) {
		//*** BEGIN CS_ONLY ***
		return System.IO.File.Exists(path) || System.IO.Directory.Exists(path);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		#ifdef _WIN32
			DWORD attr = GetFileAttributes(path.c_str());
			return attr != INVALID_FILE_ATTRIBUTES;
		#else
			return access(path.c_str(), F_OK) == 0;
		#endif
		*** END CPP_ONLY ***/
	}

	// Returns a map {path, isDirectory, size, date} or Value.Null if path not found.
	private static Value FsInfo(String path) {
		if (path.Length == 0) path = FsCurrentDir(); // CPP: if (path.Length() == 0) path = FsCurrentDir();
		//*** BEGIN CS_ONLY ***
		try {
			bool isDir = System.IO.Directory.Exists(path);
			if (!isDir && !System.IO.File.Exists(path)) return Value.Null;
			String fullPath = System.IO.Path.GetFullPath(path);
			System.IO.FileSystemInfo info = isDir
				? (System.IO.FileSystemInfo)new System.IO.DirectoryInfo(path)
				: new System.IO.FileInfo(path);
			DateTime mtime = info.LastWriteTime;
			Int64 size = isDir ? 0 : ((System.IO.FileInfo)info).Length;
			String dateStr = mtime.ToString("yyyy-MM-dd HH:mm:ss");
			Value result = Value.make_map(4);
			Value.map_set(result, "path", fullPath);
			Value.map_set(result, "isDirectory", Value.Truth(isDir));
			Value.map_set(result, "size", new Value((Double)size));
			Value.map_set(result, "date", dateStr);
			return result;
		} catch (Exception) { return Value.Null; }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		#ifdef _WIN32
			struct _stati64 stats;
			if (_stati64(path.c_str(), &stats) != 0) return Value::null;
			char pathBuf[512];
			_fullpath(pathBuf, path.c_str(), sizeof(pathBuf));
			bool isDir = (stats.st_mode & _S_IFDIR) != 0;
			struct tm t;
			localtime_s(&t, &stats.st_mtime);
		#else
			struct stat stats;
			if (stat(path.c_str(), &stats) < 0) return Value::null;
			char pathBuf[4096];
			realpath(path.c_str(), pathBuf);
			bool isDir = S_ISDIR(stats.st_mode);
			struct tm t;
			#if defined(__APPLE__) || defined(__FreeBSD__)
				localtime_r(&stats.st_mtimespec.tv_sec, &t);
			#else
				localtime_r(&stats.st_mtime, &t);
			#endif
		#endif
		char dateBuf[72];  // (never need more than 20, but some compilers are dumb)
		snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d %02d:%02d:%02d",
			1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
		Value result = Value::make_map(4);
		Value::map_set(result, Value::make_string("path"), Value::make_string(String(pathBuf)));
		Value::map_set(result, Value::make_string("isDirectory"), Value::Truth(isDir));
		Value::map_set(result, Value::make_string("size"), Value((Double)stats.st_size));
		Value::map_set(result, Value::make_string("date"), Value::make_string(String(dateBuf)));
		return result;
		*** END CPP_ONLY ***/
	}

	private static Boolean FsMakeDir(String path) {
		//*** BEGIN CS_ONLY ***
		try { System.IO.Directory.CreateDirectory(path); return true; }
		catch (Exception) { return false; }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		#ifdef _WIN32
			return CreateDirectory(path.c_str(), nullptr) != 0;
		#else
			return mkdir(path.c_str(), 0755) == 0;
		#endif
		*** END CPP_ONLY ***/
	}

	private static Boolean FsMove(String oldPath, String newPath) {
		//*** BEGIN CS_ONLY ***
		try {
			if (System.IO.Directory.Exists(oldPath)) System.IO.Directory.Move(oldPath, newPath);
			else System.IO.File.Move(oldPath, newPath, true);
			return true;
		} catch (Exception) { return false; }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		return rename(oldPath.c_str(), newPath.c_str()) == 0;
		*** END CPP_ONLY ***/
	}

	private static Boolean FsCopy(String oldPath, String newPath) {
		//*** BEGIN CS_ONLY ***
		try { System.IO.File.Copy(oldPath, newPath, true); return true; }
		catch (Exception) { return false; }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		#ifdef _WIN32
			return CopyFile(oldPath.c_str(), newPath.c_str(), false) != 0;
		#elif defined(__APPLE__) || defined(__FreeBSD__)
			int input = open(oldPath.c_str(), O_RDONLY);
			if (input == -1) return false;
			int output = creat(newPath.c_str(), 0660);
			if (output == -1) { close(input); return false; }
			int ok = fcopyfile(input, output, 0, COPYFILE_ALL);
			close(input); close(output);
			return ok == 0;
		#else
			String cmd = String("cp -p \"") + oldPath + "\" \"" + newPath + "\"";
			return system(cmd.c_str()) == 0;
		#endif
		*** END CPP_ONLY ***/
	}

	private static Boolean FsDelete(String path) {
		//*** BEGIN CS_ONLY ***
		try {
			if (System.IO.Directory.Exists(path)) { System.IO.Directory.Delete(path); return true; }
			if (System.IO.File.Exists(path)) { System.IO.File.Delete(path); return true; }
			return false;
		} catch (Exception) { return false; }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		#ifdef _WIN32
			struct _stati64 stats;
			if (_stati64(path.c_str(), &stats) == 0 && (stats.st_mode & _S_IFDIR))
				return RemoveDirectory(path.c_str()) != 0;
			return DeleteFile(path.c_str()) != 0;
		#else
			return remove(path.c_str()) == 0;
		#endif
		*** END CPP_ONLY ***/
	}

	private static Value FsReadLines(String path) {
		//*** BEGIN CS_ONLY ***
		try {
			String[] lines = System.IO.File.ReadAllLines(path, System.Text.Encoding.UTF8);
			Value result = Value.make_list(lines.Length);
			foreach (String line in lines) Value.list_push(result, Value.make_string(line));
			return result;
		} catch (Exception ex) { return ErrorTypes.FileError("readLines: " + ex.Message); }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		FILE* f = fopen(path.c_str(), "r");
		if (!f) return ErrorTypes::FileError("readLines: cannot open: " + path);
		Value result = Value::make_list(16);
		char buf[4096];
		String partial;
		while (!feof(f)) {
			size_t n = fread(buf, 1, sizeof(buf), f);
			if (n == 0) break;
			int start = 0;
			for (int i = 0; i < (int)n; i++) {
				if (buf[i] == '\n' || buf[i] == '\r') {
					String line(buf + start, i - start);
					if (partial.Length() > 0) { line = partial + line; partial = String(""); }
					Value::list_push(result, Value::make_string(line));
					if (buf[i] == '\r' && i+1 < (int)n && buf[i+1] == '\n') i++;
					start = i + 1;
				}
			}
			if (start < (int)n) partial += String(buf + start, n - start);
		}
		if (partial.Length() > 0) Value::list_push(result, Value::make_string(partial));
		fclose(f);
		return result;
		*** END CPP_ONLY ***/
	}

	private static Value FsWriteLines(String path, Value lines) {
		//*** BEGIN CS_ONLY ***
		try {
			System.Collections.Generic.List<String> lineList = new System.Collections.Generic.List<String>();
			if (lines.IsList()) {
				Int32 count = Value.list_count(lines);
				for (Int32 i = 0; i < count; i++) lineList.Add(Value.as_cstring(Value.list_get(lines, i)));
			} else {
				lineList.Add(Value.as_cstring(lines));
			}
			System.IO.File.WriteAllLines(path, lineList, System.Text.Encoding.UTF8);
			return new Value((Double)lineList.Count);
		} catch (Exception ex) { return ErrorTypes.FileError("writeLines: " + ex.Message); }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		FILE* f = fopen(path.c_str(), "w");
		if (!f) return ErrorTypes::FileError("writeLines: cannot open: " + path);
		Int32 count = 0;
		if (lines.IsList()) {
			Int32 n = Value::list_count(lines);
			for (Int32 i = 0; i < n; i++) {
				String s = Value::as_cstring(Value::list_get(lines, i));
				fputs(s.c_str(), f); fputc('\n', f); count++;
			}
		} else {
			String s = Value::as_cstring(lines);
			fputs(s.c_str(), f); fputc('\n', f); count = 1;
		}
		fclose(f);
		return Value((Double)count);
		*** END CPP_ONLY ***/
	}

	private static Value FsLoadRaw(String path) {
		//*** BEGIN CS_ONLY ***
		try {
			byte[] data = System.IO.File.ReadAllBytes(path);
			CsRawBuf buf = new CsRawBuf();
			buf.Bytes = data;
			Value handleVal = GCManager.NewHandle(buf, RawBufFinalizer);
			return NewRawDataInstance(handleVal);
		} catch (Exception ex) { return ErrorTypes.FileError("loadRaw: " + ex.Message); }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		FILE* f = fopen(path.c_str(), "rb");
		if (!f) return ErrorTypes::FileError("loadRaw: cannot open: " + path);
		fseek(f, 0, SEEK_END);
		int size = (int)ftell(f);
		fseek(f, 0, SEEK_SET);
		CppRawBuf* buf = new CppRawBuf();
		buf->length = size;
		buf->bytes = size > 0 ? (uint8_t*)malloc(size) : nullptr;
		if (size > 0) fread(buf->bytes, 1, size, f);
		fclose(f);
		Value handleVal = GCManager::NewHandle(buf, RawBufFinalizer);
		return NewRawDataInstance(handleVal);
		*** END CPP_ONLY ***/
	}

	private static Value FsSaveRaw(String path, Value rawDataMap) {
		Value handleVal = Value.Null;
		Value.map_try_get(rawDataMap, Value.make_string("_handle"), out handleVal);
		if (!handleVal.IsHandle()) return ErrorTypes.FileError("saveRaw: data is not a RawData object"); // CPP: if (!handleVal.IsHandle()) return ErrorTypes::FileError("saveRaw: data is not a RawData object");
		//*** BEGIN CS_ONLY ***
		CsRawBuf buf = GetCsRawBuf(handleVal);
		if (buf == null || buf.Bytes == null) return ErrorTypes.FileError("saveRaw: RawData has no buffer");
		try {
			System.IO.File.WriteAllBytes(path, buf.Bytes);
			return Value.Null;
		} catch (Exception ex) { return ErrorTypes.FileError("saveRaw: " + ex.Message); }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* buf = (CppRawBuf*)GCManager::GetHandle(handleVal).UserData;
		if (!buf || !buf->bytes) return ErrorTypes::FileError("saveRaw: RawData has no buffer");
		FILE* f = fopen(path.c_str(), "wb");
		if (!f) return ErrorTypes::FileError("saveRaw: cannot open for writing: " + path);
		size_t written = fwrite(buf->bytes, 1, buf->length, f);
		fclose(f);
		if ((int)written != buf->length) return ErrorTypes::FileError("saveRaw: write error");
		return Value::null;
		*** END CPP_ONLY ***/
	}

	// ── File module lazy map builders ────────────────────────────────────────
	// These are called on first access, after Intrinsic.RegisterAll() has
	// assigned real function indices to all intrinsics.

	private static Value GetRawDataClassMap() {
		if (!_rawDataClassMap.IsNull()) return _rawDataClassMap;
		_rawDataClassMap = Value.make_map(24);
		Value.map_set(_rawDataClassMap, "_handle", Value.Null);
		Value.map_set(_rawDataClassMap, "littleEndian", Value.one);
		for (Int32 i = 0; i < _rdKeys.Count; i++) // CPP: for (Int32 i = 0; i < (Int32)_rdKeys.Count(); i++)
			Value.map_set(_rawDataClassMap, _rdKeys[i], Intrinsic.GetByIndex(_rdStart + i).GetFunc());
		return _rawDataClassMap;
	}

	private static Value GetFileHandleClassMap() {
		if (!_fileHandleClassMap.IsNull()) return _fileHandleClassMap;
		_fileHandleClassMap = Value.make_map(12);
		for (Int32 i = 0; i < _fhKeys.Count; i++) // CPP: for (Int32 i = 0; i < (Int32)_fhKeys.Count(); i++)
			Value.map_set(_fileHandleClassMap, _fhKeys[i], Intrinsic.GetByIndex(_fhStart + i).GetFunc());
		return _fileHandleClassMap;
	}

	private static Value GetFileModuleMap() {
		if (!_fileModuleMap.IsNull()) return _fileModuleMap;
		_fileModuleMap = Value.make_map(20);
		for (Int32 i = 0; i < _fmKeys.Count; i++) // CPP: for (Int32 i = 0; i < (Int32)_fmKeys.Count(); i++)
			Value.map_set(_fileModuleMap, _fmKeys[i], Intrinsic.GetByIndex(_fmStart + i).GetFunc());
		return _fileModuleMap;
	}

	// ── File module init ──────────────────────────────────────────────────────

	// Register file/FileHandle/RawData intrinsics with internal names.
	// Maps are built lazily via Get*Map() on first access after RegisterAll().
	private static void InitFileIntrinsics() {
		Intrinsic f;

		_rdKeys = new List<String>();
		_rdStart = Intrinsic.Count();
		_fhKeys = new List<String>();
		_fmKeys = new List<String>();

		// ── RawData methods ───────────────────────────────────────────────────

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(Value.zero);
			return new IntrinsicResult(new Value((Double)len));
		};
		_rdKeys.Add("len");

		// resize(bytes) — allocate / reallocate buffer; copies existing data
		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("bytes", new Value(32.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Int32 newSize = (Int32)ctx.GetArg(1).DoubleValue();
			if (newSize < 0) newSize = 0;
			ResizeRawBuf(self, newSize);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("resize");

		// Typed getter/setter factory lambdas.
		// byte / setByte
		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off >= len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			return new IntrinsicResult(new Value((Double)RawGetByte(hv, off)));
		};
		_rdKeys.Add("byte");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.AddParam("value", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off >= len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			RawSetByte(hv, off, (Int32)ctx.GetArg(2).DoubleValue());
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setByte");

		// sbyte / setSbyte
		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off >= len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			return new IntrinsicResult(new Value((Double)RawGetSByte(hv, off)));
		};
		_rdKeys.Add("sbyte");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.AddParam("value", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off >= len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			RawSetSByte(hv, off, (Int32)ctx.GetArg(2).DoubleValue());
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setSbyte");

		// ushort / setUshort / short / setShort
		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off + 2 > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Value leVal = Value.Null;
			Value.map_try_get(self, Value.make_string("littleEndian"), out leVal);
			Boolean le = leVal.IsNull() || leVal.DoubleValue() != 0.0;
			return new IntrinsicResult(new Value((Double)RawGetU16(hv, off, le)));
		};
		_rdKeys.Add("ushort");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.AddParam("value", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off + 2 > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Value leVal = Value.Null;
			Value.map_try_get(self, Value.make_string("littleEndian"), out leVal);
			Boolean le = leVal.IsNull() || leVal.DoubleValue() != 0.0;
			RawSetU16(hv, off, (Int32)ctx.GetArg(2).DoubleValue(), le);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setUshort");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off + 2 > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Value leVal = Value.Null;
			Value.map_try_get(self, Value.make_string("littleEndian"), out leVal);
			Boolean le = leVal.IsNull() || leVal.DoubleValue() != 0.0;
			return new IntrinsicResult(new Value((Double)RawGetI16(hv, off, le)));
		};
		_rdKeys.Add("short");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.AddParam("value", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off + 2 > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Value leVal = Value.Null;
			Value.map_try_get(self, Value.make_string("littleEndian"), out leVal);
			Boolean le = leVal.IsNull() || leVal.DoubleValue() != 0.0;
			RawSetI16(hv, off, (Int32)ctx.GetArg(2).DoubleValue(), le);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setShort");

		// uint / setUint / int / setInt
		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off + 4 > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Value leVal = Value.Null;
			Value.map_try_get(self, Value.make_string("littleEndian"), out leVal);
			Boolean le = leVal.IsNull() || leVal.DoubleValue() != 0.0;
			return new IntrinsicResult(new Value(RawGetU32(hv, off, le)));
		};
		_rdKeys.Add("uint");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.AddParam("value", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off + 4 > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Value leVal = Value.Null;
			Value.map_try_get(self, Value.make_string("littleEndian"), out leVal);
			Boolean le = leVal.IsNull() || leVal.DoubleValue() != 0.0;
			RawSetU32(hv, off, ctx.GetArg(2).DoubleValue(), le);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setUint");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off + 4 > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Value leVal = Value.Null;
			Value.map_try_get(self, Value.make_string("littleEndian"), out leVal);
			Boolean le = leVal.IsNull() || leVal.DoubleValue() != 0.0;
			return new IntrinsicResult(new Value((Double)RawGetI32(hv, off, le)));
		};
		_rdKeys.Add("int");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.AddParam("value", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off + 4 > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Value leVal = Value.Null;
			Value.map_try_get(self, Value.make_string("littleEndian"), out leVal);
			Boolean le = leVal.IsNull() || leVal.DoubleValue() != 0.0;
			RawSetI32(hv, off, (Int32)ctx.GetArg(2).DoubleValue(), le);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setInt");

		// float / setFloat / double / setDouble
		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off + 4 > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Value leVal = Value.Null;
			Value.map_try_get(self, Value.make_string("littleEndian"), out leVal);
			Boolean le = leVal.IsNull() || leVal.DoubleValue() != 0.0;
			return new IntrinsicResult(new Value(RawGetF32(hv, off, le)));
		};
		_rdKeys.Add("float");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.AddParam("value", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off + 4 > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Value leVal = Value.Null;
			Value.map_try_get(self, Value.make_string("littleEndian"), out leVal);
			Boolean le = leVal.IsNull() || leVal.DoubleValue() != 0.0;
			RawSetF32(hv, off, ctx.GetArg(2).DoubleValue(), le);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setFloat");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off + 8 > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Value leVal = Value.Null;
			Value.map_try_get(self, Value.make_string("littleEndian"), out leVal);
			Boolean le = leVal.IsNull() || leVal.DoubleValue() != 0.0;
			return new IntrinsicResult(new Value(RawGetF64(hv, off, le)));
		};
		_rdKeys.Add("double");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.AddParam("value", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off + 8 > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Value leVal = Value.Null;
			Value.map_try_get(self, Value.make_string("littleEndian"), out leVal);
			Boolean le = leVal.IsNull() || leVal.DoubleValue() != 0.0;
			RawSetF64(hv, off, ctx.GetArg(2).DoubleValue(), le);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setDouble");

		// utf8 / setUtf8
		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.AddParam("bytes", new Value(-1.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Int32 byteCount = (Int32)ctx.GetArg(2).DoubleValue();
			return new IntrinsicResult(Value.make_string(RawGetUTF8(hv, off, byteCount)));
		};
		_rdKeys.Add("utf8");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("offset", Value.zero);
		f.AddParam("value", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(ErrorTypes.FileError("RawData has no buffer"));
			Int32 off = (Int32)ctx.GetArg(1).DoubleValue();
			if (off < 0) off += len;
			if (off < 0 || off > len) return new IntrinsicResult(ErrorTypes.FileError("index out of bounds"));
			Int32 written = RawSetUTF8(hv, off, Value.to_String(ctx.GetArg(2)));
			return new IntrinsicResult(new Value((Double)written));
		};
		_rdKeys.Add("setUtf8");

		// ── FileHandle methods ─────────────────────────────────────────────────
		_fhStart = Intrinsic.Count();

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			CloseFileHandle(hv);
			Value.map_set(self, "_handle", Value.Null);
			return IntrinsicResult.Null;
		};
		_fhKeys.Add("close");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			return new IntrinsicResult(new Value(IsFileHandleOpen(hv) ? 1.0 : 0.0));
		};
		_fhKeys.Add("isOpen");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("data", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorTypes.FileError("file is not open"));
			Int32 n = WriteToFile(hv, Value.to_String(ctx.GetArg(1)));
			return new IntrinsicResult(new Value((Double)n));
		};
		_fhKeys.Add("write");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("data", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorTypes.FileError("file is not open"));
			Int32 n = WriteToFile(hv, Value.to_String(ctx.GetArg(1)) + "\n");
			return new IntrinsicResult(new Value((Double)n));
		};
		_fhKeys.Add("writeLine");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("byteCount", new Value(-1.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorTypes.FileError("file is not open"));
			return new IntrinsicResult(Value.make_string(ReadFromFile(hv, (Int32)ctx.GetArg(1).DoubleValue())));
		};
		_fhKeys.Add("read");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorTypes.FileError("file is not open"));
			return new IntrinsicResult(ReadLineFromFile(hv));
		};
		_fhKeys.Add("readLine");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorTypes.FileError("file is not open"));
			return new IntrinsicResult(new Value((Double)GetFilePosition(hv)));
		};
		_fhKeys.Add("position");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.AddParam("pos", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorTypes.FileError("file is not open"));
			SeekFilePosition(hv, (Int32)ctx.GetArg(1).DoubleValue());
			return IntrinsicResult.Null;
		};
		_fhKeys.Add("seek");

		f = Intrinsic.Create("");
		f.AddParam("self", Value.Null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = Value.Null;
			Value.map_try_get(self, Value.make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorTypes.FileError("file is not open"));
			return new IntrinsicResult(new Value(IsFileAtEnd(hv) ? 1.0 : 0.0));
		};
		_fhKeys.Add("atEnd");

		// ── file module functions ───────────────────────────────────────────────
		_fmStart = Intrinsic.Count();

		f = Intrinsic.Create("");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(Value.make_string(FsCurrentDir()));
		};
		_fmKeys.Add("curdir");

		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			String path = Value.to_String(ctx.GetArg(0));
			if (!FsSetDir(path)) return new IntrinsicResult(ErrorTypes.FileError("setdir: could not change directory to: " + path));
			return IntrinsicResult.Null;
		};
		_fmKeys.Add("setdir");

		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(FsChildren(Value.to_String(ctx.GetArg(0))));
		};
		_fmKeys.Add("children");

		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(Value.make_string(FsBasename(Value.to_String(ctx.GetArg(0)))));
		};
		_fmKeys.Add("name");

		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(Value.make_string(FsDirname(Value.to_String(ctx.GetArg(0)))));
		};
		_fmKeys.Add("parent");

		f = Intrinsic.Create("");
		f.AddParam("parentPath", Value.emptyString);
		f.AddParam("childName", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(Value.make_string(FsChild(Value.to_String(ctx.GetArg(0)), Value.to_String(ctx.GetArg(1)))));
		};
		_fmKeys.Add("child");

		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(new Value(FsExists(Value.to_String(ctx.GetArg(0))) ? 1.0 : 0.0));
		};
		_fmKeys.Add("exists");

		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value info = FsInfo(Value.to_String(ctx.GetArg(0)));
			return new IntrinsicResult(info.IsNull() ? Value.Null : info);
		};
		_fmKeys.Add("info");

		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			String path = Value.to_String(ctx.GetArg(0));
			if (!FsMakeDir(path)) return new IntrinsicResult(ErrorTypes.FileError("makedir: could not create directory: " + path));
			return IntrinsicResult.Null;
		};
		_fmKeys.Add("makedir");

		f = Intrinsic.Create("");
		f.AddParam("oldPath", Value.emptyString);
		f.AddParam("newPath", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			String oldPath = Value.to_String(ctx.GetArg(0));
			String newPath = Value.to_String(ctx.GetArg(1));
			if (!FsMove(oldPath, newPath)) return new IntrinsicResult(ErrorTypes.FileError("move: could not move " + oldPath + " to " + newPath));
			return IntrinsicResult.Null;
		};
		_fmKeys.Add("move");

		f = Intrinsic.Create("");
		f.AddParam("oldPath", Value.emptyString);
		f.AddParam("newPath", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			String oldPath = Value.to_String(ctx.GetArg(0));
			String newPath = Value.to_String(ctx.GetArg(1));
			if (!FsCopy(oldPath, newPath)) return new IntrinsicResult(ErrorTypes.FileError("copy: could not copy " + oldPath + " to " + newPath));
			return IntrinsicResult.Null;
		};
		_fmKeys.Add("copy");

		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			String path = Value.to_String(ctx.GetArg(0));
			if (!FsDelete(path)) return new IntrinsicResult(ErrorTypes.FileError("delete: could not delete: " + path));
			return IntrinsicResult.Null;
		};
		_fmKeys.Add("delete");

		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = FsReadLines(Value.to_String(ctx.GetArg(0)));
			return new IntrinsicResult(v);
		};
		_fmKeys.Add("readLines");

		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.AddParam("lines", Value.Null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = FsWriteLines(Value.to_String(ctx.GetArg(0)), ctx.GetArg(1));
			return new IntrinsicResult(v);
		};
		_fmKeys.Add("writeLines");

		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(FsLoadRaw(Value.to_String(ctx.GetArg(0))));
		};
		_fmKeys.Add("loadRaw");

		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.AddParam("data", Value.Null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value r = FsSaveRaw(Value.to_String(ctx.GetArg(0)), ctx.GetArg(1));
			return new IntrinsicResult(r);
		};
		_fmKeys.Add("saveRaw");

		// open(path, mode) — return a FileHandle instance or an error
		f = Intrinsic.Create("");
		f.AddParam("path", Value.emptyString);
		f.AddParam("mode", Value.make_string("r+"));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			String path = Value.to_String(ctx.GetArg(0));
			String mode = Value.to_String(ctx.GetArg(1));
			Value hv = MakeFileHandle(path, mode);
			if (hv.IsNull()) return new IntrinsicResult(ErrorTypes.FileError("open: could not open: " + path));
			Value instance = Value.make_map(4);
			Value.map_set(instance, Value.magicIsA, GetFileHandleClassMap());
			Value.map_set(instance, "_handle", hv);
			return new IntrinsicResult(instance);
		};
		_fmKeys.Add("open");

		// ── Global intrinsics ─────────────────────────────────────────────────

		f = Intrinsic.Create("file");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(GetFileModuleMap());
		};

		f = Intrinsic.Create("FileHandle");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(GetFileHandleClassMap());
		};

		f = Intrinsic.Create("RawData");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(GetRawDataClassMap());
		};
	}

	// ── Key module ────────────────────────────────────────────────────────────
	// `key` exposes single-keystroke console input:
	//   key.available   — true if a keystroke is waiting (non-blocking)
	//   key.get         — read the next keystroke, blocking until one arrives;
	//                     returns a one-character string (special keys come back
	//                     as char(code), using the cross-platform UnifiedKey codes)
	//   key.raw         — flag (default 0): when false, multi-byte editing keys are
	//                     translated to portable codes; when true, raw platform
	//                     bytes are returned and the script decodes them itself
	// Keys read via key.get are never echoed automatically (the terminal's echo
	// is off while the module is active); a script prints what it wants itself.

	// Assert that the terminal is in per-key (raw/cbreak) mode. Called at the
	// start of every key operation, so after an `input` or `exec` has dropped
	// the terminal back to cooked mode it is re-entered here (last-writer-wins).
	// On C++ this is an idempotent cbreak switch; on C# the .NET console needs
	// no special setup.
	private static void AssertRawMode() {
		/*** BEGIN CPP_ONLY ***
		Keyboard::EnterRawMode();
		*** END CPP_ONLY ***/
	}

	// Return true if a keystroke is waiting, without blocking.
	private static Boolean KeyAvailableImpl() {
		AssertRawMode();
		//*** BEGIN CS_ONLY ***
		try { return Console.KeyAvailable; }
		catch (Exception) { return false; }  // input not from an interactive console
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		return Keyboard::KeyAvailable();
		*** END CPP_ONLY ***/
	}

	// Block until a keystroke is available, then return its code. With raw=false,
	// editing keys are translated to portable UnifiedKey codes; with raw=true,
	// the platform-native byte/code is returned. Returns 0 for a consumed but
	// unrecognized sequence and -1 on end-of-input. The key is never echoed.
	private static Int32 KeyGetImpl(Boolean raw) {
		AssertRawMode();
		Int32 code;
		//*** BEGIN CS_ONLY ***
		ConsoleKeyInfo info = Console.ReadKey(true);  // intercept: never auto-echo
		code = (Int32)info.KeyChar;
		if (!raw) {
			switch (info.Key) {
				case ConsoleKey.LeftArrow:  code = 17;
				break;
				case ConsoleKey.RightArrow: code = 18;
				break;
				case ConsoleKey.UpArrow:    code = 19;
				break;
				case ConsoleKey.DownArrow:  code = 20;
				break;
				case ConsoleKey.Home:       code = 1;
				break;
				case ConsoleKey.End:        code = 5;
				break;
				case ConsoleKey.PageUp:     code = 21;
				break;
				case ConsoleKey.PageDown:   code = 22;
				break;
				case ConsoleKey.Backspace:  code = 8;
				break;
				case ConsoleKey.Delete:     code = 127;
				break;
				case ConsoleKey.Enter:      code = 10;
				break;
				default: break;
			}
		}
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		code = raw ? Keyboard::ReadKey() : Keyboard::ReadKeyTranslated();
		*** END CPP_ONLY ***/
		return code;
	}

	private static Value GetKeyModuleMap() {
		if (!_keyModuleMap.IsNull()) return _keyModuleMap;
		_keyModuleMap = Value.make_map(8);
		for (Int32 i = 0; i < _keyKeys.Count; i++) // CPP: for (Int32 i = 0; i < (Int32)_keyKeys.Count(); i++)
			Value.map_set(_keyModuleMap, _keyKeys[i], Intrinsic.GetByIndex(_keyStart + i).GetFunc());
		Value.map_set(_keyModuleMap, "raw", Value.zero);
		return _keyModuleMap;
	}

	private static void InitKeyIntrinsics() {
		Intrinsic f;
		_keyKeys = new List<String>();
		_keyStart = Intrinsic.Count();

		// available — true if a keystroke is waiting.
		f = Intrinsic.Create("");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(Value.Truth(KeyAvailableImpl()));
		};
		_keyKeys.Add("available");

		// get — block for the next keystroke, return it as a one-char string.
		f = Intrinsic.Create("");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value modMap = GetKeyModuleMap();
			Value rawVal = Value.Null;
			Value.map_try_get(modMap, Value.make_string("raw"), out rawVal);
			Int32 code = KeyGetImpl(rawVal.BoolValue());
			if (code <= 0) return new IntrinsicResult(Value.emptyString);
			return new IntrinsicResult(Value.string_from_code_point(code));
		};
		_keyKeys.Add("get");

		// global `key` intrinsic — return the module map.
		f = Intrinsic.Create("key");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(GetKeyModuleMap());
		};
	}

	// ── Date/time helpers ─────────────────────────────────────────────────────

	// Offset (in Unix-epoch seconds) of MiniScript's date-value epoch, which is
	// 2000-01-01 00:00:00 local time.  A MiniScript numeric date value plus this
	// offset gives seconds since the Unix epoch (the form FormatDate expects).
	private static Double _dateTimeEpoch = 0;
	private static Double DateTimeEpoch() {
		if (_dateTimeEpoch == 0) {
			//*** BEGIN CS_ONLY ***
			DateTime baseDate = new DateTime(2000, 1, 1, 0, 0, 0, DateTimeKind.Local);
			_dateTimeEpoch = (baseDate.ToUniversalTime() - DateTime.UnixEpoch).TotalSeconds;
			//*** END CS_ONLY ***
			/*** BEGIN CPP_ONLY ***
			tm baseDate;
			memset(&baseDate, 0, sizeof(tm));
			baseDate.tm_year = 2000 - 1900;	// (because tm_year is years since 1900!)
			baseDate.tm_mon = 0;
			baseDate.tm_mday = 1;
			_dateTimeEpoch = (double)mktime(&baseDate);
			*** END CPP_ONLY ***/
		}
		return _dateTimeEpoch;
	}

	// Current time as seconds since the Unix epoch.
	private static Double NowSeconds() {
		//*** BEGIN CS_ONLY ***
		return (DateTime.UtcNow - DateTime.UnixEpoch).TotalSeconds;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		time_t t;
		time(&t);
		return (double)t;
		*** END CPP_ONLY ***/
	}

	// Register all shell intrinsics.  Must be called before any Interpreter is Reset.
	public static void Init() {
		GCManager.RegisterMarkCallback(MarkRoots, null); // CPP: GCManager::RegisterMarkCallback(ShellIntrinsics::MarkRoots, nullptr);
		CoreIntrinsics.RegisterInvalidateCallback(InvalidateCaches); // CPP: CoreIntrinsics::RegisterInvalidateCallback(ShellIntrinsics::InvalidateCaches);
		InitFileIntrinsics();
		InitKeyIntrinsics();

		Intrinsic f;

		// exit(resultCode=null) — stop the VM and signal the host to exit.
		f = Intrinsic.Create("exit");
		f.AddParam("resultCode", Value.Null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			ShellIntrinsics.ExitASAP = true;
			Value resultCode = ctx.GetArg(0);
			if (!resultCode.IsNull()) {
				ShellIntrinsics.ExitCode = (Int32)resultCode.DoubleValue();
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
		f.AddParam("cmd", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			if (!partialResult.done) {
				return FinishExec(partialResult.result);
			}
			Value cmdArg = ctx.GetArg(0);
			if (cmdArg.IsError()) return new IntrinsicResult(cmdArg);
			String cmd = Value.to_String(cmdArg);
			if (!_envMap.IsNull()) {
				SyncEnvMap();
			}
			Value handle = BeginExec(cmd);
			if (handle.IsNull()) {
				return new IntrinsicResult(ErrorTypes.RuntimeError(StringUtils.Format("exec: failed to start command: {0}", cmd)));
			}
			return FinishExec(handle);
		};

		// _dateVal(dateStr) — convert to a numeric MiniScript date value (seconds
		// since 2000-01-01 00:00:00 local time).  `dateStr` may be null (meaning
		// now), a number (returned unchanged, already a date value), or a string
		// (parsed via DateTimeUtils.ParseDate).
		f = Intrinsic.Create("_dateVal");
		f.AddParam("dateStr");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value date = ctx.GetArg(0);
			if (date.IsError()) return new IntrinsicResult(date);
			Double t;
			if (date.IsNull()) {
				t = NowSeconds();
			} else if (date.IsNumber()) {
				return new IntrinsicResult(date);
			} else {
				if (!DateTimeUtils.TryParseDate(Value.to_String(date), out t)) {
					return new IntrinsicResult(ErrorTypes.FormatError(StringUtils.Format("'{0}' is not a valid date", Value.to_String(date))));
				}
			}
			return new IntrinsicResult(new Value(t - DateTimeEpoch()));
		};

		// _dateStr(date, format) — format a date as a string.  `date` may be null
		// (meaning now), a number (a date value: seconds since 2000-01-01 local),
		// or a string (parsed as a date).  `format` is a .NET-style format string.
		f = Intrinsic.Create("_dateStr");
		f.AddParam("date");
		f.AddParam("format", Value.make_string("yyyy-MM-dd HH:mm:ss"));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value date = ctx.GetArg(0);
			if (date.IsError()) return new IntrinsicResult(date);
			Value format = ctx.GetArg(1);
			if (format.IsError()) return new IntrinsicResult(format);
			String formatStr;
			if (format.IsNull()) formatStr = "yyyy-MM-dd HH:mm:ss";
			else formatStr = Value.to_String(format);
			Double d;
			if (date.IsNull()) {
				d = NowSeconds();
			} else if (date.IsNumber()) {
				d = date.DoubleValue() + DateTimeEpoch();
			} else {
				if (!DateTimeUtils.TryParseDate(Value.to_String(date), out d)) {
					return new IntrinsicResult(ErrorTypes.FormatError(StringUtils.Format("'{0}' is not a valid date", Value.to_String(date))));
				}
			}
			String formatted;
			if (!DateTimeUtils.TryFormatDate(d, formatStr, out formatted)) {
				return new IntrinsicResult(ErrorTypes.FormatError(StringUtils.Format("'{0}' is not a valid date format", formatStr)));
			}
			return new IntrinsicResult(Value.make_string(formatted));
		};

		// import(libname) — load and run a MiniScript module, then store its locals
		// under the library name in the caller's scope.  Uses the partialResult
		// mechanism and ManuallyPushCall so the module runs inside the current VM.
		// Search path comes from the MS_IMPORT_PATH env var (colon-separated);
		// defaults to "$MS_SCRIPT_DIR:$MS_SCRIPT_DIR/lib:$MS_EXE_DIR/lib".
		f = Intrinsic.Create("import");
		f.AddParam("libname", Value.emptyString);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			if (!partialResult.done) {
				// Phase 2: the module finished running.  Store its locals map
				// under the library name in the caller's scope, and also return
				// it as the result value (for the `foo = import("foo")` form).
				String lib = Value.as_cstring(partialResult.result);
				Value importedValues = ctx.vm.ManualCallResult;
				ctx.vm.SetVar(lib, importedValues);
				return new IntrinsicResult(importedValues, true);
			}
			// Phase 1: find, parse, compile, and push the module.
			Value libnameArg = ctx.GetArg(0);
			if (libnameArg.IsError()) return new IntrinsicResult(libnameArg);
			String libname = Value.to_String(libnameArg);
			if (libname.Length == 0) {
				return new IntrinsicResult(ErrorTypes.FileError("import: no library name given"));
			}
			// Determine the search path.
			Value pathVal;
			String searchPath;
			if (!Value.map_try_get(GetEnvMap(), Value.make_string("MS_IMPORT_PATH"), out pathVal) || pathVal.IsNull()) {
				searchPath = "$MS_SCRIPT_DIR:$MS_SCRIPT_DIR/lib:$MS_EXE_DIR/lib";
			} else {
				searchPath = Value.as_cstring(pathVal);
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
				return new IntrinsicResult(ErrorTypes.FileError(StringUtils.Format("import: library not found: {0}", libname)));
			}
			// Parse the module source.
			Parser p = new Parser();
			p.Init(source);
			List<ASTNode> stmts = p.ParseProgram();
			if (p.HadError()) {
				return new IntrinsicResult(ErrorTypes.CompilerError(StringUtils.Format("import: parse error in {0}.ms", libname)));
			}
			// Simplify AST (constant folding, etc.).
			for (Int32 i = 0; i < stmts.Count; i++) {
				stmts[i] = stmts[i].Simplify();
			}
			// Compile the module to bytecode.
			BytecodeEmitter emitter = new BytecodeEmitter();
			CodeGenerator gen = new CodeGenerator(emitter);
			gen.FileName = libname + ".ms";
			List<FuncDef> fns = gen.CompileImport(stmts, libname + ".ms");
			if (!gen.Error.IsNull()) {
				return new IntrinsicResult(gen.Error);
			}
			// Push the module call; we will be re-invoked when it finishes.
			// fns[0] is the module's @main; nested functions are reachable
			// from its constant pool.
			ctx.vm.ManuallyPushCall(ctx.baseIndex, fns[0]);
			return new IntrinsicResult(Value.make_string(libname), false);
		};
	}
}

}
