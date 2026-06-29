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
	private: static Value _fileModuleMap;
	private: static Value _fileHandleClassMap;
	private: static Value _rawDataClassMap;
	private: static Int32 _rdStart;
	private: static List<String> _rdKeys;
	private: static List<String> _fhKeys;
	private: static List<String> _fmKeys;
	private: static Value _keyModuleMap;
	private: static Int32 _keyStart;
	private: static List<String> _keyKeys;

	// ── File module static fields ─────────────────────────────────────────────

	// ── Key module static fields ──────────────────────────────────────────────

	// Platform-specific wrapper types for FileHandle and RawData.

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

	// ── GCHandle finalizers ───────────────────────────────────────────────────

	private: static void FileHandleFinalizer(object userData);

	private: static void RawBufFinalizer(object userData);

	// ── CS-only buffer-access helpers ────────────────────────────────────────

	// ── RawData buffer helpers ────────────────────────────────────────────────

	// Allocate a GCHandle wrapping a zero-filled buffer of `size` bytes.
	private: static Value AllocRawBuf(Int32 size);

	// Return the byte length of the buffer in `handleVal`, or -1 if invalid.
	private: static Int32 GetRawBufLen(Value handleVal);

	// Allocate new buffer of `newSize` bytes, copy old data, update self._handle.
	private: static void ResizeRawBuf(Value self, Int32 newSize);

	// Create a new RawData instance map backed by `handleVal`.
	private: static Value NewRawDataInstance(Value handleVal);

	// ── RawData typed accessors ───────────────────────────────────────────────
	// Each getter returns -1 (or 0.0) on invalid handle/offset.
	// Each setter silently no-ops on invalid inputs.

	private: static Int32 RawGetByte(Value h, Int32 off);
	private: static void RawSetByte(Value h, Int32 off, Int32 val);
	private: static Int32 RawGetSByte(Value h, Int32 off);
	private: static void RawSetSByte(Value h, Int32 off, Int32 val);
	private: static Int32 RawGetU16(Value h, Int32 off, Boolean le);
	private: static void RawSetU16(Value h, Int32 off, Int32 val, Boolean le);
	private: static Int32 RawGetI16(Value h, Int32 off, Boolean le);
	private: static void RawSetI16(Value h, Int32 off, Int32 val, Boolean le);
	private: static Double RawGetU32(Value h, Int32 off, Boolean le);
	private: static void RawSetU32(Value h, Int32 off, Double dval, Boolean le);
	private: static Int32 RawGetI32(Value h, Int32 off, Boolean le);
	private: static void RawSetI32(Value h, Int32 off, Int32 val, Boolean le);
	private: static Double RawGetF32(Value h, Int32 off, Boolean le);
	private: static void RawSetF32(Value h, Int32 off, Double dval, Boolean le);
	private: static Double RawGetF64(Value h, Int32 off, Boolean le);
	private: static void RawSetF64(Value h, Int32 off, Double dval, Boolean le);
	private: static String RawGetUTF8(Value h, Int32 off, Int32 byteCount);
	private: static Int32 RawSetUTF8(Value h, Int32 off, String val);

	// ── FileHandle helpers ────────────────────────────────────────────────────

	// Open a file and return a GCHandle value wrapping the native file handle.
	// Returns Value.Null on failure.
	private: static Value MakeFileHandle(String path, String mode);

	// Close the file and mark the handle as closed (set Stream/f to null).
	private: static void CloseFileHandle(Value handleVal);

	private: static Boolean IsFileHandleOpen(Value handleVal);

	private: static Int32 WriteToFile(Value handleVal, String data);

	// Read up to `byteCount` bytes (or all remaining if < 0). Returns string.
	private: static String ReadFromFile(Value handleVal, Int32 byteCount);

	// Read the next line (stripped of trailing \n/\r\n). Returns Value.Null at EOF.
	private: static Value ReadLineFromFile(Value handleVal);

	private: static Int32 GetFilePosition(Value handleVal);

	private: static Boolean IsFileAtEnd(Value handleVal);

	private: static void SeekFilePosition(Value handleVal, Int32 pos);

	// ── Filesystem helpers ────────────────────────────────────────────────────

	private: static String FsCurrentDir();

	private: static Boolean FsSetDir(String path);

	private: static Value FsChildren(String path);

	private: static String FsBasename(String path);

	private: static String FsDirname(String path);

	private: static String FsChild(String parent, String child);

	private: static Boolean FsExists(String path);

	// Returns a map {path, isDirectory, size, date} or Value.Null if path not found.
	private: static Value FsInfo(String path);

	private: static Boolean FsMakeDir(String path);

	private: static Boolean FsMove(String oldPath, String newPath);

	private: static Boolean FsCopy(String oldPath, String newPath);

	private: static Boolean FsDelete(String path);

	private: static Value FsReadLines(String path);

	private: static Value FsWriteLines(String path, Value lines);

	private: static Value FsLoadRaw(String path);

	private: static Value FsSaveRaw(String path, Value rawDataMap);

	// ── File module lazy map builders ────────────────────────────────────────
	// These are called on first access, after Intrinsic.RegisterAll() has
	// assigned real function indices to all intrinsics.

	private: static Value GetRawDataClassMap();

	private: static Value GetFileHandleClassMap();

	private: static Value GetFileModuleMap();

	// ── File module init ──────────────────────────────────────────────────────

	// Register file/FileHandle/RawData intrinsics with internal names.
	// Maps are built lazily via Get*Map() on first access after RegisterAll().
	private: static void InitFileIntrinsics();

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
	private: static void AssertRawMode();

	// Return true if a keystroke is waiting, without blocking.
	private: static Boolean KeyAvailableImpl();

	// Block until a keystroke is available, then return its code. With raw=false,
	// editing keys are translated to portable UnifiedKey codes; with raw=true,
	// the platform-native byte/code is returned. Returns 0 for a consumed but
	// unrecognized sequence and -1 on end-of-input. The key is never echoed.
	private: static Int32 KeyGetImpl(Boolean raw);

	private: static Value GetKeyModuleMap();

	private: static void InitKeyIntrinsics();
	private: static Double _dateTimeEpoch;

	// ── Date/time helpers ─────────────────────────────────────────────────────

	// Offset (in Unix-epoch seconds) of MiniScript's date-value epoch, which is
	// 2000-01-01 00:00:00 local time.  A MiniScript numeric date value plus this
	// offset gives seconds since the Unix epoch (the form FormatDate expects).
	private: static Double DateTimeEpoch();

	// Current time as seconds since the Unix epoch.
	private: static Double NowSeconds();

	// Register all shell intrinsics.  Must be called before any Interpreter is Reset.
	public: static void Init();
}; // end of struct ShellIntrinsics

// INLINE METHODS

} // end of namespace MiniScript
