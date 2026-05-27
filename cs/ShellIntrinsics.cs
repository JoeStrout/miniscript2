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
// CPP: #include "CS_value_util.h"
// CPP: #include "CoreIntrinsics.g.h"
// CPP: #include <cstdlib>
// CPP: #include <cstring>
// CPP: #include <cstdio>

/*** BEGIN CPP_ONLY ***
#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <Fileapi.h>
    #include <io.h>
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

	// ── File module static fields ─────────────────────────────────────────────
	private static Value _fileModuleMap = val_null;
	private static Value _fileHandleClassMap = val_null;
	private static Value _rawDataClassMap = val_null;
	private static Int32 _rdStart = -1, _fhStart = -1, _fmStart = -1;
	private static List<String> _rdKeys = null;
	private static List<String> _fhKeys = null;
	private static List<String> _fmKeys = null;

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
		GCManager.Mark(_fileModuleMap);
		GCManager.Mark(_fileHandleClassMap);
		GCManager.Mark(_rawDataClassMap);
	}

	// Drop cached values so they are rebuilt on next use.
	// Call from VM reset if needed.
	public static void InvalidateCaches() {
		_shellArgs = val_null;
		_envMap = val_null;
		_fileModuleMap = val_null;
		_fileHandleClassMap = val_null;
		_rawDataClassMap = val_null;
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
		if (!is_handle(handleVal)) return null;
		GCHandle slot = GCManager.GetHandle(handleVal);
		return slot.UserData as CsFileHandle;
	}
	private static CsRawBuf GetCsRawBuf(Value handleVal) {
		if (!is_handle(handleVal)) return null;
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
		if (!is_handle(handleVal)) return -1;
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
		Value oldHandle = val_null;
		map_try_get(self, make_string("_handle"), out oldHandle);
		Value newHandle = AllocRawBuf(newSize);
		if (is_null(newHandle)) return;
		// Copy up to min(old, new) bytes from old buffer to new one.
		if (!is_null(oldHandle) && is_handle(oldHandle)) {
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
		map_set(self, make_string("_handle"), newHandle);
	}

	// Create a new RawData instance map backed by `handleVal`.
	private static Value NewRawDataInstance(Value handleVal) {
		Value instance = make_map(4);
		map_set(instance, val_isa_key, GetRawDataClassMap());
		map_set(instance, make_string("_handle"), handleVal);
		map_set(instance, make_string("littleEndian"), make_double(1.0));
		return instance;
	}

	// ── RawData typed accessors ───────────────────────────────────────────────
	// Each getter returns -1 (or 0.0) on invalid handle/offset.
	// Each setter silently no-ops on invalid inputs.

	private static Int32 RawGetByte(Value h, Int32 off) {
		if (!is_handle(h)) return -1;
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
		if (!is_handle(h)) return;
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
		if (!is_handle(h)) return -1;
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
		if (!is_handle(h)) return;
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
		if (!is_handle(h)) return -1;
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
		if (!is_handle(h)) return;
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
		if (!is_handle(h)) return 0.0;
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
		if (!is_handle(h)) return;
		UInt32 v = (UInt32)dval;
		//*** BEGIN CS_ONLY ***
		CsRawBuf b = GetCsRawBuf(h);
		if (b == null || b.Bytes == null) return;
		if (le) {
			b.Bytes[off]=(byte)(v&0xFF); b.Bytes[off+1]=(byte)((v>>8)&0xFF);
			b.Bytes[off+2]=(byte)((v>>16)&0xFF); b.Bytes[off+3]=(byte)(v>>24);
		} else {
			b.Bytes[off]=(byte)(v>>24); b.Bytes[off+1]=(byte)((v>>16)&0xFF);
			b.Bytes[off+2]=(byte)((v>>8)&0xFF); b.Bytes[off+3]=(byte)(v&0xFF);
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
		if (!is_handle(h)) return 0.0;
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
		if (!is_handle(h)) return;
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
		if (!is_handle(h)) return "";
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
		if (!is_handle(h)) return 0;
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
	// Returns val_null on failure.
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
		} catch (Exception) { return val_null; }
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
		if (!f) return val_null;
		CppFileHandle* wrapper = new CppFileHandle();
		wrapper->f = f;
		return GCManager::NewHandle(wrapper, FileHandleFinalizer);
		*** END CPP_ONLY ***/
	}

	// Close the file and mark the handle as closed (set Stream/f to null).
	private static void CloseFileHandle(Value handleVal) {
		if (!is_handle(handleVal)) return;
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
		if (!is_handle(handleVal)) return false;
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
		if (!is_handle(handleVal)) return 0;
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
		if (!is_handle(handleVal)) return "";
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

	// Read the next line (stripped of trailing \n/\r\n). Returns val_null at EOF.
	private static Value ReadLineFromFile(Value handleVal) {
		if (!is_handle(handleVal)) return val_null;
		//*** BEGIN CS_ONLY ***
		CsFileHandle h = GetCsFileHandle(handleVal);
		if (h == null || h.Stream == null) return val_null;
		var lineBytes = new System.Collections.Generic.List<byte>();
		Int32 b;
		Boolean anyRead = false;
		while ((b = h.Stream.ReadByte()) != -1) {
			anyRead = true;
			if (b == '\n') break;
			if (b != '\r') lineBytes.Add((byte)b);
		}
		if (!anyRead) return val_null;
		return make_string(System.Text.Encoding.UTF8.GetString(lineBytes.ToArray()));
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
		if (!h || !h->f) return val_null;
		char buf[4096];
		char* s = fgets(buf, sizeof(buf), h->f);
		if (!s) return val_null;
		Int32 len = (Int32)strlen(buf);
		if (len > 0 && buf[len-1] == '\n') len--;
		if (len > 0 && buf[len-1] == '\r') len--;
		return make_string(String(buf, len));
		*** END CPP_ONLY ***/
	}

	private static Int32 GetFilePosition(Value handleVal) {
		if (!is_handle(handleVal)) return -1;
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
		if (!is_handle(handleVal)) return true;
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
		if (!is_handle(handleVal)) return;
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
			Value result = make_list(entries.Length);
			foreach (String entry in entries)
				list_push(result, make_string(System.IO.Path.GetFileName(entry)));
			return result;
		} catch (Exception ex) { return ErrorType.FileError("children: " + ex.Message); }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		Value result = make_list(16);
		#ifdef _WIN32
			String searchPath = path + "\\*";
			WIN32_FIND_DATA data;
			HANDLE hFind = FindFirstFile(searchPath.c_str(), &data);
			if (hFind != INVALID_HANDLE_VALUE) {
				do {
					String name(data.cFileName);
					if (name != "." && name != "..") list_push(result, make_string(name));
				} while (FindNextFile(hFind, &data) != 0);
				FindClose(hFind);
			}
		#else
			DIR* dir = opendir(path.c_str());
			if (dir) {
				while (struct dirent* entry = readdir(dir)) {
					String name(entry->d_name);
					if (name != "." && name != "..") list_push(result, make_string(name));
				}
				closedir(dir);
			}
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

	// Returns a map {path, isDirectory, size, date} or val_null if path not found.
	private static Value FsInfo(String path) {
		if (path.Length == 0) path = FsCurrentDir(); // CPP: if (path.Length() == 0) path = FsCurrentDir();
		//*** BEGIN CS_ONLY ***
		try {
			bool isDir = System.IO.Directory.Exists(path);
			if (!isDir && !System.IO.File.Exists(path)) return val_null;
			String fullPath = System.IO.Path.GetFullPath(path);
			System.IO.FileSystemInfo info = isDir
				? (System.IO.FileSystemInfo)new System.IO.DirectoryInfo(path)
				: new System.IO.FileInfo(path);
			DateTime mtime = info.LastWriteTime;
			Int64 size = isDir ? 0 : ((System.IO.FileInfo)info).Length;
			String dateStr = mtime.ToString("yyyy-MM-dd HH:mm:ss");
			Value result = make_map(4);
			map_set(result, make_string("path"), make_string(fullPath));
			map_set(result, make_string("isDirectory"), make_double(isDir ? 1.0 : 0.0));
			map_set(result, make_string("size"), make_double((Double)size));
			map_set(result, make_string("date"), make_string(dateStr));
			return result;
		} catch (Exception) { return val_null; }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		#ifdef _WIN32
			struct _stati64 stats;
			if (_stati64(path.c_str(), &stats) != 0) return val_null;
			char pathBuf[512];
			_fullpath(pathBuf, path.c_str(), sizeof(pathBuf));
			bool isDir = (stats.st_mode & _S_IFDIR) != 0;
			struct tm t;
			localtime_s(&t, &stats.st_mtime);
		#else
			struct stat stats;
			if (stat(path.c_str(), &stats) < 0) return val_null;
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
		char dateBuf[72];
		snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d %02d:%02d:%02d",
			1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
		Value result = make_map(4);
		map_set(result, make_string("path"), make_string(String(pathBuf)));
		map_set(result, make_string("isDirectory"), make_double(isDir ? 1.0 : 0.0));
		map_set(result, make_string("size"), make_double((Double)stats.st_size));
		map_set(result, make_string("date"), make_string(String(dateBuf)));
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
			Value result = make_list(lines.Length);
			foreach (String line in lines) list_push(result, make_string(line));
			return result;
		} catch (Exception ex) { return ErrorType.FileError("readLines: " + ex.Message); }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		FILE* f = fopen(path.c_str(), "r");
		if (!f) return ErrorType::FileError("readLines: cannot open: " + path);
		Value result = make_list(16);
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
					list_push(result, make_string(line));
					if (buf[i] == '\r' && i+1 < (int)n && buf[i+1] == '\n') i++;
					start = i + 1;
				}
			}
			if (start < (int)n) partial += String(buf + start, n - start);
		}
		if (partial.Length() > 0) list_push(result, make_string(partial));
		fclose(f);
		return result;
		*** END CPP_ONLY ***/
	}

	private static Value FsWriteLines(String path, Value lines) {
		//*** BEGIN CS_ONLY ***
		try {
			System.Collections.Generic.List<String> lineList = new System.Collections.Generic.List<String>();
			if (is_list(lines)) {
				Int32 count = list_count(lines);
				for (Int32 i = 0; i < count; i++) lineList.Add(as_cstring(list_get(lines, i)));
			} else {
				lineList.Add(as_cstring(lines));
			}
			System.IO.File.WriteAllLines(path, lineList, System.Text.Encoding.UTF8);
			return make_double((Double)lineList.Count);
		} catch (Exception ex) { return ErrorType.FileError("writeLines: " + ex.Message); }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		FILE* f = fopen(path.c_str(), "w");
		if (!f) return ErrorType::FileError("writeLines: cannot open: " + path);
		Int32 count = 0;
		if (is_list(lines)) {
			Int32 n = list_count(lines);
			for (Int32 i = 0; i < n; i++) {
				String s = as_cstring(list_get(lines, i));
				fputs(s.c_str(), f); fputc('\n', f); count++;
			}
		} else {
			String s = as_cstring(lines);
			fputs(s.c_str(), f); fputc('\n', f); count = 1;
		}
		fclose(f);
		return make_double((Double)count);
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
		} catch (Exception ex) { return ErrorType.FileError("loadRaw: " + ex.Message); }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		FILE* f = fopen(path.c_str(), "rb");
		if (!f) return ErrorType::FileError("loadRaw: cannot open: " + path);
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
		Value handleVal = val_null;
		map_try_get(rawDataMap, make_string("_handle"), out handleVal);
		if (!is_handle(handleVal)) return ErrorType.FileError("saveRaw: data is not a RawData object"); // CPP: if (!is_handle(handleVal)) return ErrorType::FileError("saveRaw: data is not a RawData object");
		//*** BEGIN CS_ONLY ***
		CsRawBuf buf = GetCsRawBuf(handleVal);
		if (buf == null || buf.Bytes == null) return ErrorType.FileError("saveRaw: RawData has no buffer");
		try {
			System.IO.File.WriteAllBytes(path, buf.Bytes);
			return val_null;
		} catch (Exception ex) { return ErrorType.FileError("saveRaw: " + ex.Message); }
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		CppRawBuf* buf = (CppRawBuf*)GCManager::GetHandle(handleVal).UserData;
		if (!buf || !buf->bytes) return ErrorType::FileError("saveRaw: RawData has no buffer");
		FILE* f = fopen(path.c_str(), "wb");
		if (!f) return ErrorType::FileError("saveRaw: cannot open for writing: " + path);
		size_t written = fwrite(buf->bytes, 1, buf->length, f);
		fclose(f);
		if ((int)written != buf->length) return ErrorType::FileError("saveRaw: write error");
		return val_null;
		*** END CPP_ONLY ***/
	}

	// ── File module lazy map builders ────────────────────────────────────────
	// These are called on first access, after Intrinsic.RegisterAll() has
	// assigned real function indices to all intrinsics.

	private static Value GetRawDataClassMap() {
		if (!is_null(_rawDataClassMap)) return _rawDataClassMap;
		_rawDataClassMap = make_map(24);
		map_set(_rawDataClassMap, make_string("_handle"), val_null);
		map_set(_rawDataClassMap, make_string("littleEndian"), make_double(1.0));
		for (Int32 i = 0; i < _rdKeys.Count; i++) // CPP: for (Int32 i = 0; i < (Int32)_rdKeys.Count(); i++)
			map_set(_rawDataClassMap, make_string(_rdKeys[i]), Intrinsic.GetByIndex(_rdStart + i).GetFunc());
		return _rawDataClassMap;
	}

	private static Value GetFileHandleClassMap() {
		if (!is_null(_fileHandleClassMap)) return _fileHandleClassMap;
		_fileHandleClassMap = make_map(12);
		for (Int32 i = 0; i < _fhKeys.Count; i++) // CPP: for (Int32 i = 0; i < (Int32)_fhKeys.Count(); i++)
			map_set(_fileHandleClassMap, make_string(_fhKeys[i]), Intrinsic.GetByIndex(_fhStart + i).GetFunc());
		return _fileHandleClassMap;
	}

	private static Value GetFileModuleMap() {
		if (!is_null(_fileModuleMap)) return _fileModuleMap;
		_fileModuleMap = make_map(20);
		for (Int32 i = 0; i < _fmKeys.Count; i++) // CPP: for (Int32 i = 0; i < (Int32)_fmKeys.Count(); i++)
			map_set(_fileModuleMap, make_string(_fmKeys[i]), Intrinsic.GetByIndex(_fmStart + i).GetFunc());
		return _fileModuleMap;
	}

	// ── File module init ──────────────────────────────────────────────────────

	// Register file/FileHandle/RawData intrinsics with internal names.
	// Maps are built lazily via Get*Map() on first access after RegisterAll().
	private static void InitFileIntrinsics() {
		Intrinsic f;

		_rdKeys = new List<String>(); _rdStart = Intrinsic.AllCount();
		_fhKeys = new List<String>();
		_fmKeys = new List<String>();

		// ── RawData methods ───────────────────────────────────────────────────

		f = Intrinsic.Create("");
		f.AddParam("self", val_null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null;
			map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv);
			if (len < 0) return new IntrinsicResult(make_double(0.0));
			return new IntrinsicResult(make_double((Double)len));
		};
		_rdKeys.Add("len");

		// resize(bytes) — allocate / reallocate buffer; copies existing data
		f = Intrinsic.Create("");
		f.AddParam("self", val_null);
		f.AddParam("bytes", make_double(32.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Int32 newSize = (Int32)as_double(ctx.GetArg(1));
			if (newSize < 0) newSize = 0;
			ResizeRawBuf(self, newSize);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("resize");

		// Typed getter/setter factory lambdas.
		// byte / setByte
		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off >= len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			return new IntrinsicResult(make_double((Double)RawGetByte(hv, off)));
		};
		_rdKeys.Add("byte");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off >= len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			RawSetByte(hv, off, (Int32)as_double(ctx.GetArg(2)));
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setByte");

		// sbyte / setSbyte
		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off >= len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			return new IntrinsicResult(make_double((Double)RawGetSByte(hv, off)));
		};
		_rdKeys.Add("sbyte");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off >= len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			RawSetSByte(hv, off, (Int32)as_double(ctx.GetArg(2)));
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setSbyte");

		// ushort / setUshort / short / setShort
		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off + 2 > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Value leVal = val_null; map_try_get(self, make_string("littleEndian"), out leVal);
			Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
			return new IntrinsicResult(make_double((Double)RawGetU16(hv, off, le)));
		};
		_rdKeys.Add("ushort");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off + 2 > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Value leVal = val_null; map_try_get(self, make_string("littleEndian"), out leVal);
			Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
			RawSetU16(hv, off, (Int32)as_double(ctx.GetArg(2)), le);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setUshort");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off + 2 > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Value leVal = val_null; map_try_get(self, make_string("littleEndian"), out leVal);
			Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
			return new IntrinsicResult(make_double((Double)RawGetI16(hv, off, le)));
		};
		_rdKeys.Add("short");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off + 2 > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Value leVal = val_null; map_try_get(self, make_string("littleEndian"), out leVal);
			Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
			RawSetI16(hv, off, (Int32)as_double(ctx.GetArg(2)), le);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setShort");

		// uint / setUint / int / setInt
		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off + 4 > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Value leVal = val_null; map_try_get(self, make_string("littleEndian"), out leVal);
			Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
			return new IntrinsicResult(make_double(RawGetU32(hv, off, le)));
		};
		_rdKeys.Add("uint");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off + 4 > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Value leVal = val_null; map_try_get(self, make_string("littleEndian"), out leVal);
			Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
			RawSetU32(hv, off, as_double(ctx.GetArg(2)), le);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setUint");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off + 4 > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Value leVal = val_null; map_try_get(self, make_string("littleEndian"), out leVal);
			Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
			return new IntrinsicResult(make_double((Double)RawGetI32(hv, off, le)));
		};
		_rdKeys.Add("int");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off + 4 > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Value leVal = val_null; map_try_get(self, make_string("littleEndian"), out leVal);
			Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
			RawSetI32(hv, off, (Int32)as_double(ctx.GetArg(2)), le);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setInt");

		// float / setFloat / double / setDouble
		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off + 4 > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Value leVal = val_null; map_try_get(self, make_string("littleEndian"), out leVal);
			Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
			return new IntrinsicResult(make_double(RawGetF32(hv, off, le)));
		};
		_rdKeys.Add("float");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off + 4 > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Value leVal = val_null; map_try_get(self, make_string("littleEndian"), out leVal);
			Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
			RawSetF32(hv, off, as_double(ctx.GetArg(2)), le);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setFloat");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off + 8 > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Value leVal = val_null; map_try_get(self, make_string("littleEndian"), out leVal);
			Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
			return new IntrinsicResult(make_double(RawGetF64(hv, off, le)));
		};
		_rdKeys.Add("double");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off + 8 > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Value leVal = val_null; map_try_get(self, make_string("littleEndian"), out leVal);
			Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
			RawSetF64(hv, off, as_double(ctx.GetArg(2)), le);
			return IntrinsicResult.Null;
		};
		_rdKeys.Add("setDouble");

		// utf8 / setUtf8
		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("bytes", make_double(-1.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Int32 byteCount = (Int32)as_double(ctx.GetArg(2));
			return new IntrinsicResult(make_string(RawGetUTF8(hv, off, byteCount)));
		};
		_rdKeys.Add("utf8");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			Int32 len = GetRawBufLen(hv); if (len < 0) return new IntrinsicResult(ErrorType.FileError("RawData has no buffer"));
			Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
			if (off < 0 || off > len) return new IntrinsicResult(ErrorType.FileError("index out of bounds"));
			Int32 written = RawSetUTF8(hv, off, to_String(ctx.GetArg(2)));
			return new IntrinsicResult(make_double((Double)written));
		};
		_rdKeys.Add("setUtf8");

		// ── FileHandle methods ─────────────────────────────────────────────────
		_fhStart = Intrinsic.AllCount();

		f = Intrinsic.Create("");
		f.AddParam("self", val_null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			CloseFileHandle(hv);
			map_set(self, make_string("_handle"), val_null);
			return IntrinsicResult.Null;
		};
		_fhKeys.Add("close");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			return new IntrinsicResult(make_double(IsFileHandleOpen(hv) ? 1.0 : 0.0));
		};
		_fhKeys.Add("isOpen");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("data", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorType.FileError("file is not open"));
			Int32 n = WriteToFile(hv, to_String(ctx.GetArg(1)));
			return new IntrinsicResult(make_double((Double)n));
		};
		_fhKeys.Add("write");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("data", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorType.FileError("file is not open"));
			Int32 n = WriteToFile(hv, to_String(ctx.GetArg(1)) + "\n");
			return new IntrinsicResult(make_double((Double)n));
		};
		_fhKeys.Add("writeLine");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("byteCount", make_double(-1.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorType.FileError("file is not open"));
			return new IntrinsicResult(make_string(ReadFromFile(hv, (Int32)as_double(ctx.GetArg(1)))));
		};
		_fhKeys.Add("read");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorType.FileError("file is not open"));
			return new IntrinsicResult(ReadLineFromFile(hv));
		};
		_fhKeys.Add("readLine");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorType.FileError("file is not open"));
			return new IntrinsicResult(make_double((Double)GetFilePosition(hv)));
		};
		_fhKeys.Add("position");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null); f.AddParam("pos", make_double(0.0));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorType.FileError("file is not open"));
			SeekFilePosition(hv, (Int32)as_double(ctx.GetArg(1)));
			return IntrinsicResult.Null;
		};
		_fhKeys.Add("seek");

		f = Intrinsic.Create("");
		f.AddParam("self", val_null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value hv = val_null; map_try_get(self, make_string("_handle"), out hv);
			if (!IsFileHandleOpen(hv)) return new IntrinsicResult(ErrorType.FileError("file is not open"));
			return new IntrinsicResult(make_double(IsFileAtEnd(hv) ? 1.0 : 0.0));
		};
		_fhKeys.Add("atEnd");

		// ── file module functions ───────────────────────────────────────────────
		_fmStart = Intrinsic.AllCount();

		f = Intrinsic.Create("");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(make_string(FsCurrentDir()));
		};
		_fmKeys.Add("curdir");

		f = Intrinsic.Create("");
		f.AddParam("path", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Boolean ok = FsSetDir(to_String(ctx.GetArg(0)));
			return new IntrinsicResult(make_double(ok ? 1.0 : 0.0));	// ToDo: make_boolean
		};
		_fmKeys.Add("setdir");

		f = Intrinsic.Create("");
		f.AddParam("path", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(FsChildren(to_String(ctx.GetArg(0))));
		};
		_fmKeys.Add("children");

		f = Intrinsic.Create("");
		f.AddParam("path", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(make_string(FsBasename(to_String(ctx.GetArg(0)))));
		};
		_fmKeys.Add("name");

		f = Intrinsic.Create("");
		f.AddParam("path", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(make_string(FsDirname(to_String(ctx.GetArg(0)))));
		};
		_fmKeys.Add("parent");

		f = Intrinsic.Create("");
		f.AddParam("parentPath", make_string("")); f.AddParam("childName", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(make_string(FsChild(to_String(ctx.GetArg(0)), to_String(ctx.GetArg(1)))));
		};
		_fmKeys.Add("child");

		f = Intrinsic.Create("");
		f.AddParam("path", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(make_double(FsExists(to_String(ctx.GetArg(0))) ? 1.0 : 0.0));
		};
		_fmKeys.Add("exists");

		f = Intrinsic.Create("");
		f.AddParam("path", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value info = FsInfo(to_String(ctx.GetArg(0)));
			return new IntrinsicResult(is_null(info) ? val_null : info);
		};
		_fmKeys.Add("info");

		f = Intrinsic.Create("");
		f.AddParam("path", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(make_double(FsMakeDir(to_String(ctx.GetArg(0))) ? 1.0 : 0.0));
		};
		_fmKeys.Add("makedir");

		f = Intrinsic.Create("");
		f.AddParam("oldPath", make_string("")); f.AddParam("newPath", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(make_double(FsMove(to_String(ctx.GetArg(0)), to_String(ctx.GetArg(1))) ? 1.0 : 0.0));
		};
		_fmKeys.Add("move");

		f = Intrinsic.Create("");
		f.AddParam("oldPath", make_string("")); f.AddParam("newPath", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(make_double(FsCopy(to_String(ctx.GetArg(0)), to_String(ctx.GetArg(1))) ? 1.0 : 0.0));
		};
		_fmKeys.Add("copy");

		f = Intrinsic.Create("");
		f.AddParam("path", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(make_double(FsDelete(to_String(ctx.GetArg(0))) ? 1.0 : 0.0));
		};
		_fmKeys.Add("delete");

		f = Intrinsic.Create("");
		f.AddParam("path", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = FsReadLines(to_String(ctx.GetArg(0)));
			return new IntrinsicResult(v);
		};
		_fmKeys.Add("readLines");

		f = Intrinsic.Create("");
		f.AddParam("path", make_string("")); f.AddParam("lines", val_null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = FsWriteLines(to_String(ctx.GetArg(0)), ctx.GetArg(1));
			return new IntrinsicResult(v);
		};
		_fmKeys.Add("writeLines");

		f = Intrinsic.Create("");
		f.AddParam("path", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(FsLoadRaw(to_String(ctx.GetArg(0))));
		};
		_fmKeys.Add("loadRaw");

		f = Intrinsic.Create("");
		f.AddParam("path", make_string("")); f.AddParam("data", val_null);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value r = FsSaveRaw(to_String(ctx.GetArg(0)), ctx.GetArg(1));
			return new IntrinsicResult(r);
		};
		_fmKeys.Add("saveRaw");

		// open(path, mode) — return a FileHandle instance or an error
		f = Intrinsic.Create("");
		f.AddParam("path", make_string("")); f.AddParam("mode", make_string("r+"));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			String path = to_String(ctx.GetArg(0));
			String mode = to_String(ctx.GetArg(1));
			Value hv = MakeFileHandle(path, mode);
			if (is_null(hv)) return new IntrinsicResult(ErrorType.FileError("open: could not open: " + path));
			Value instance = make_map(4);
			map_set(instance, val_isa_key, GetFileHandleClassMap());
			map_set(instance, make_string("_handle"), hv);
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

	// Register all shell intrinsics.  Must be called before any Interpreter is Reset.
	public static void Init() {
		GCManager.RegisterMarkCallback(MarkRoots, null); // CPP: GCManager::RegisterMarkCallback(ShellIntrinsics::MarkRoots, nullptr);
		CoreIntrinsics.RegisterInvalidateCallback(InvalidateCaches); // CPP: CoreIntrinsics::RegisterInvalidateCallback(ShellIntrinsics::InvalidateCaches);
		InitFileIntrinsics();

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
			String cmd = to_String(ctx.GetArg(0));
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
			String libname = to_String(ctx.GetArg(0));
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
			List<FuncDef> fns = gen.CompileImport(stmts, libname + ".ms");
			if (!is_null(gen.Error)) {
				ctx.vm.RaiseRuntimeError(StringUtils.Format("import: compile error in {0}.ms", libname));
				return IntrinsicResult.Null;
			}
			// Push the module call; we will be re-invoked when it finishes.
			// fns[0] is the module's @main; nested functions are reachable
			// from its constant pool.
			ctx.vm.ManuallyPushCall(ctx.baseIndex, fns[0]);
			return new IntrinsicResult(make_string(libname), false);
		};
	}
}

}
