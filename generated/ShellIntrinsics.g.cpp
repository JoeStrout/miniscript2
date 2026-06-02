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
#include "CS_value_util.h"
#include "CoreIntrinsics.g.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
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
Value ShellIntrinsics::_fileModuleMap = val_null;
Value ShellIntrinsics::_fileHandleClassMap = val_null;
Value ShellIntrinsics::_rawDataClassMap = val_null;
Int32 ShellIntrinsics::_rdStart = -1, _fhStart = -1, _fmStart = -1;
List<String> ShellIntrinsics::_rdKeys = nullptr;
List<String> ShellIntrinsics::_fhKeys = nullptr;
List<String> ShellIntrinsics::_fmKeys = nullptr;
struct CppFileHandle { FILE* f; };
struct CppRawBuf    { uint8_t* bytes; int length; };
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
	GCManager::Mark(_fileModuleMap);
	GCManager::Mark(_fileHandleClassMap);
	GCManager::Mark(_rawDataClassMap);
}
void ShellIntrinsics::InvalidateCaches() {
	_shellArgs = val_null;
	_envMap = val_null;
	_fileModuleMap = val_null;
	_fileHandleClassMap = val_null;
	_rawDataClassMap = val_null;
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
void ShellIntrinsics::FileHandleFinalizer(object userData) {
	CppFileHandle* h = (CppFileHandle*)userData;
	if (h) { if (h->f) fclose(h->f); delete h; }
}
void ShellIntrinsics::RawBufFinalizer(object userData) {
	CppRawBuf* b = (CppRawBuf*)userData;
	if (b) { free(b->bytes); delete b; }
	// C#: byte[] is managed; nothing explicit needed.
}
Value ShellIntrinsics::AllocRawBuf(Int32 size) {
	CppRawBuf* buf = new CppRawBuf();
	buf->bytes = size > 0 ? (uint8_t*)calloc(size, 1) : nullptr;
	buf->length = size;
	return GCManager::NewHandle(buf, RawBufFinalizer);
}
Int32 ShellIntrinsics::GetRawBufLen(Value handleVal) {
	if (!is_handle(handleVal)) return -1;
	CppRawBuf* buf = (CppRawBuf*)GCManager::GetHandle(handleVal).UserData;
	return buf ? buf->length : -1;
}
void ShellIntrinsics::ResizeRawBuf(Value self,Int32 newSize) {
	Value oldHandle = val_null;
	map_try_get(self, make_string("_handle"), &oldHandle);
	Value newHandle = AllocRawBuf(newSize);
	if (is_null(newHandle)) return;
	// Copy up to min(old, new) bytes from old buffer to new one.
	if (!is_null(oldHandle) && is_handle(oldHandle)) {
		Int32 oldLen = GetRawBufLen(oldHandle);
		if (oldLen > 0 && newSize > 0) {
			Int32 copyLen = oldLen < newSize ? oldLen : newSize;
			CppRawBuf* ob = (CppRawBuf*)GCManager::GetHandle(oldHandle).UserData;
			CppRawBuf* nb = (CppRawBuf*)GCManager::GetHandle(newHandle).UserData;
			if (ob && ob->bytes && nb && nb->bytes) memcpy(nb->bytes, ob->bytes, copyLen);
		}
	}
	map_set(self, make_string("_handle"), newHandle);
}
Value ShellIntrinsics::NewRawDataInstance(Value handleVal) {
	Value instance = make_map(4);
	map_set(instance, val_isa_key, GetRawDataClassMap());
	map_set(instance, make_string("_handle"), handleVal);
	map_set(instance, make_string("littleEndian"), make_double(1.0));
	return instance;
}
Int32 ShellIntrinsics::RawGetByte(Value h,Int32 off) {
	if (!is_handle(h)) return -1;
	CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
	return (b && b->bytes) ? (Int32)(Byte)b->bytes[off] : -1;
}
void ShellIntrinsics::RawSetByte(Value h,Int32 off,Int32 val) {
	if (!is_handle(h)) return;
	CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
	if (b && b->bytes) b->bytes[off] = (uint8_t)val;
}
Int32 ShellIntrinsics::RawGetSByte(Value h,Int32 off) {
	if (!is_handle(h)) return -1;
	CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
	return (b && b->bytes) ? (Int32)(int8_t)b->bytes[off] : -1;
}
void ShellIntrinsics::RawSetSByte(Value h,Int32 off,Int32 val) {
	if (!is_handle(h)) return;
	CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
	if (b && b->bytes) b->bytes[off] = (uint8_t)(int8_t)val;
}
Int32 ShellIntrinsics::RawGetU16(Value h,Int32 off,Boolean le) {
	if (!is_handle(h)) return -1;
	CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
	if (!b || !b->bytes) return -1;
	UInt16 v = le ? ((UInt16)b->bytes[off] | ((UInt16)b->bytes[off+1] << 8))
	              : (((UInt16)b->bytes[off] << 8) | (UInt16)b->bytes[off+1]);
	return (Int32)v;
}
void ShellIntrinsics::RawSetU16(Value h,Int32 off,Int32 val,Boolean le) {
	if (!is_handle(h)) return;
	UInt16 v = (UInt16)val;
	CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
	if (!b || !b->bytes) return;
	if (le) { b->bytes[off] = (uint8_t)(v & 0xFF); b->bytes[off+1] = (uint8_t)(v >> 8); }
	else    { b->bytes[off] = (uint8_t)(v >> 8);   b->bytes[off+1] = (uint8_t)(v & 0xFF); }
}
Int32 ShellIntrinsics::RawGetI16(Value h,Int32 off,Boolean le) {
	return (Int32)(Int16)RawGetU16(h, off, le);
}
void ShellIntrinsics::RawSetI16(Value h,Int32 off,Int32 val,Boolean le) {
	RawSetU16(h, off, (Int32)(UInt16)(Int16)val, le);
}
Double ShellIntrinsics::RawGetU32(Value h,Int32 off,Boolean le) {
	if (!is_handle(h)) return 0.0;
	CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
	if (!b || !b->bytes) return 0.0;
	UInt32 v = le
		? ((UInt32)b->bytes[off] | ((UInt32)b->bytes[off+1]<<8) | ((UInt32)b->bytes[off+2]<<16) | ((UInt32)b->bytes[off+3]<<24))
		: (((UInt32)b->bytes[off]<<24) | ((UInt32)b->bytes[off+1]<<16) | ((UInt32)b->bytes[off+2]<<8) | (UInt32)b->bytes[off+3]);
	return (Double)v;
}
void ShellIntrinsics::RawSetU32(Value h,Int32 off,Double dval,Boolean le) {
	if (!is_handle(h)) return;
	UInt32 v = (UInt32)dval;
	CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
	if (!b || !b->bytes) return;
	if (le) {
		b->bytes[off]=(uint8_t)(v&0xFF); b->bytes[off+1]=(uint8_t)((v>>8)&0xFF);
		b->bytes[off+2]=(uint8_t)((v>>16)&0xFF); b->bytes[off+3]=(uint8_t)(v>>24);
	} else {
		b->bytes[off]=(uint8_t)(v>>24); b->bytes[off+1]=(uint8_t)((v>>16)&0xFF);
		b->bytes[off+2]=(uint8_t)((v>>8)&0xFF); b->bytes[off+3]=(uint8_t)(v&0xFF);
	}
}
Int32 ShellIntrinsics::RawGetI32(Value h,Int32 off,Boolean le) {
	return (Int32)(UInt32)RawGetU32(h, off, le);
}
void ShellIntrinsics::RawSetI32(Value h,Int32 off,Int32 val,Boolean le) {
	RawSetU32(h, off, (Double)(UInt32)val, le);
}
Double ShellIntrinsics::RawGetF32(Value h,Int32 off,Boolean le) {
	UInt32 bits = (UInt32)RawGetU32(h, off, le);
	Single fval;
	memcpy(&fval, &bits, sizeof(Single));
	return (Double)fval;
}
void ShellIntrinsics::RawSetF32(Value h,Int32 off,Double dval,Boolean le) {
	Single fval = (Single)dval;
	UInt32 bits;
	memcpy(&bits, &fval, sizeof(Single));
	RawSetU32(h, off, (Double)bits, le);
}
Double ShellIntrinsics::RawGetF64(Value h,Int32 off,Boolean le) {
	if (!is_handle(h)) return 0.0;
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
}
void ShellIntrinsics::RawSetF64(Value h,Int32 off,Double dval,Boolean le) {
	if (!is_handle(h)) return;
	CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
	if (!b || !b->bytes) return;
	UInt64 bits;
	memcpy(&bits, &dval, sizeof(Double));
	if (le) {
		for (int i = 0; i < 8; i++) b->bytes[off+i] = (uint8_t)(bits >> (i*8));
	} else {
		for (int i = 0; i < 8; i++) b->bytes[off+i] = (uint8_t)(bits >> (56 - i*8));
	}
}
String ShellIntrinsics::RawGetUTF8(Value h,Int32 off,Int32 byteCount) {
	if (!is_handle(h)) return "";
	CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
	if (!b || !b->bytes || off < 0) return String("");
	if (byteCount < 0) byteCount = b->length - off;
	if (off + byteCount > b->length) byteCount = b->length - off;
	Int32 actual = 0;
	while (actual < byteCount && b->bytes[off + actual] != 0) actual++;
	return String((const char*)(b->bytes + off), actual);
}
Int32 ShellIntrinsics::RawSetUTF8(Value h,Int32 off,String val) {
	if (!is_handle(h)) return 0;
	CppRawBuf* b = (CppRawBuf*)GCManager::GetHandle(h).UserData;
	if (!b || !b->bytes || off < 0) return 0;
	Int32 copyLen = (Int32)val.Length();
	if (off + copyLen > b->length) copyLen = b->length - off;
	if (copyLen < 0) copyLen = 0;
	if (copyLen > 0) memcpy(b->bytes + off, val.c_str(), copyLen);
	return copyLen;
}
Value ShellIntrinsics::MakeFileHandle(String path,String mode) {
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
}
void ShellIntrinsics::CloseFileHandle(Value handleVal) {
	if (!is_handle(handleVal)) return;
	CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
	if (h && h->f) { fclose(h->f); h->f = nullptr; }
}
Boolean ShellIntrinsics::IsFileHandleOpen(Value handleVal) {
	if (!is_handle(handleVal)) return Boolean(false);
	CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
	return h != nullptr && h->f != nullptr;
}
Int32 ShellIntrinsics::WriteToFile(Value handleVal,String data) {
	if (!is_handle(handleVal)) return 0;
	CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
	if (!h || !h->f) return 0;
	return (Int32)fwrite(data.c_str(), 1, data.Length(), h->f);
}
String ShellIntrinsics::ReadFromFile(Value handleVal,Int32 byteCount) {
	if (!is_handle(handleVal)) return "";
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
}
Value ShellIntrinsics::ReadLineFromFile(Value handleVal) {
	if (!is_handle(handleVal)) return val_null;
	CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
	if (!h || !h->f) return val_null;
	char buf[4096];
	char* s = fgets(buf, sizeof(buf), h->f);
	if (!s) return val_null;
	Int32 len = (Int32)strlen(buf);
	if (len > 0 && buf[len-1] == '\n') len--;
	if (len > 0 && buf[len-1] == '\r') len--;
	return make_string(String(buf, len));
}
Int32 ShellIntrinsics::GetFilePosition(Value handleVal) {
	if (!is_handle(handleVal)) return -1;
	CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
	return (h && h->f) ? (Int32)ftell(h->f) : -1;
}
Boolean ShellIntrinsics::IsFileAtEnd(Value handleVal) {
	if (!is_handle(handleVal)) return Boolean(true);
	CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
	return !h || !h->f || feof(h->f) != 0;
}
void ShellIntrinsics::SeekFilePosition(Value handleVal,Int32 pos) {
	if (!is_handle(handleVal)) return;
	CppFileHandle* h = (CppFileHandle*)GCManager::GetHandle(handleVal).UserData;
	if (h && h->f) fseek(h->f, pos, SEEK_SET);
}
String ShellIntrinsics::FsCurrentDir() {
	char buf[4096];
	return getcwd(buf, sizeof(buf)) ? String(buf) : String(".");
}
Boolean ShellIntrinsics::FsSetDir(String path) {
	return chdir(path.c_str()) == 0;
}
Value ShellIntrinsics::FsChildren(String path) {
	if (path.Length() == 0) path = FsCurrentDir();
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
}
String ShellIntrinsics::FsBasename(String path) {
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
}
String ShellIntrinsics::FsDirname(String path) {
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
}
String ShellIntrinsics::FsChild(String parent,String child) {
	if (parent == "") return child;
	if (parent.EndsWith("/") || parent.EndsWith("\\")) return parent + child;
	return parent + PATHSEP + child;
}
Boolean ShellIntrinsics::FsExists(String path) {
	#ifdef _WIN32
		DWORD attr = GetFileAttributes(path.c_str());
		return attr != INVALID_FILE_ATTRIBUTES;
	#else
		return access(path.c_str(), F_OK) == 0;
	#endif
}
Value ShellIntrinsics::FsInfo(String path) {
	if (path.Length() == 0) path = FsCurrentDir();
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
	char dateBuf[72];  // (never need more than 20, but some compilers are dumb)
	snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d %02d:%02d:%02d",
		1900 + t.tm_year, 1 + t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	Value result = make_map(4);
	map_set(result, make_string("path"), make_string(String(pathBuf)));
	map_set(result, make_string("isDirectory"), make_double(isDir ? 1.0 : 0.0));
	map_set(result, make_string("size"), make_double((Double)stats.st_size));
	map_set(result, make_string("date"), make_string(String(dateBuf)));
	return result;
}
Boolean ShellIntrinsics::FsMakeDir(String path) {
	#ifdef _WIN32
		return CreateDirectory(path.c_str(), nullptr) != 0;
	#else
		return mkdir(path.c_str(), 0755) == 0;
	#endif
}
Boolean ShellIntrinsics::FsMove(String oldPath,String newPath) {
	return rename(oldPath.c_str(), newPath.c_str()) == 0;
}
Boolean ShellIntrinsics::FsCopy(String oldPath,String newPath) {
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
}
Boolean ShellIntrinsics::FsDelete(String path) {
	#ifdef _WIN32
		struct _stati64 stats;
		if (_stati64(path.c_str(), &stats) == 0 && (stats.st_mode & _S_IFDIR))
			return RemoveDirectory(path.c_str()) != 0;
		return DeleteFile(path.c_str()) != 0;
	#else
		return remove(path.c_str()) == 0;
	#endif
}
Value ShellIntrinsics::FsReadLines(String path) {
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
}
Value ShellIntrinsics::FsWriteLines(String path,Value lines) {
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
}
Value ShellIntrinsics::FsLoadRaw(String path) {
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
}
Value ShellIntrinsics::FsSaveRaw(String path,Value rawDataMap) {
	Value handleVal = val_null;
	map_try_get(rawDataMap, make_string("_handle"), &handleVal);
	if (!is_handle(handleVal)) return ErrorType::FileError("saveRaw: data is not a RawData object");
	CppRawBuf* buf = (CppRawBuf*)GCManager::GetHandle(handleVal).UserData;
	if (!buf || !buf->bytes) return ErrorType::FileError("saveRaw: RawData has no buffer");
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) return ErrorType::FileError("saveRaw: cannot open for writing: " + path);
	size_t written = fwrite(buf->bytes, 1, buf->length, f);
	fclose(f);
	if ((int)written != buf->length) return ErrorType::FileError("saveRaw: write error");
	return val_null;
}
Value ShellIntrinsics::GetRawDataClassMap() {
	if (!is_null(_rawDataClassMap)) return _rawDataClassMap;
	_rawDataClassMap = make_map(24);
	map_set(_rawDataClassMap, make_string("_handle"), val_null);
	map_set(_rawDataClassMap, make_string("littleEndian"), make_double(1.0));
	for (Int32 i = 0; i < (Int32)_rdKeys.Count(); i++)
		map_set(_rawDataClassMap, make_string(_rdKeys[i]), Intrinsic::GetByIndex(_rdStart + i).GetFunc());
	return _rawDataClassMap;
}
Value ShellIntrinsics::GetFileHandleClassMap() {
	if (!is_null(_fileHandleClassMap)) return _fileHandleClassMap;
	_fileHandleClassMap = make_map(12);
	for (Int32 i = 0; i < (Int32)_fhKeys.Count(); i++)
		map_set(_fileHandleClassMap, make_string(_fhKeys[i]), Intrinsic::GetByIndex(_fhStart + i).GetFunc());
	return _fileHandleClassMap;
}
Value ShellIntrinsics::GetFileModuleMap() {
	if (!is_null(_fileModuleMap)) return _fileModuleMap;
	_fileModuleMap = make_map(20);
	for (Int32 i = 0; i < (Int32)_fmKeys.Count(); i++)
		map_set(_fileModuleMap, make_string(_fmKeys[i]), Intrinsic::GetByIndex(_fmStart + i).GetFunc());
	return _fileModuleMap;
}
void ShellIntrinsics::InitFileIntrinsics() {
	Intrinsic f;

	_rdKeys =  List<String>::New(); _rdStart = Intrinsic::AllCount();
	_fhKeys =  List<String>::New();
	_fmKeys =  List<String>::New();

	// ── RawData methods ───────────────────────────────────────────────────

	f = Intrinsic::Create("");
	f.AddParam("self", val_null);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null;
		map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv);
		if (len < 0) return IntrinsicResult(make_double(0.0));
		return IntrinsicResult(make_double((Double)len));
	});
	_rdKeys.Add("len");

	// resize(bytes) — allocate / reallocate buffer; copies existing data
	f = Intrinsic::Create("");
	f.AddParam("self", val_null);
	f.AddParam("bytes", make_double(32.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Int32 newSize = (Int32)as_double(ctx.GetArg(1));
		if (newSize < 0) newSize = 0;
		ResizeRawBuf(self, newSize);
		return IntrinsicResult::Null;
	});
	_rdKeys.Add("resize");

	// Typed getter/setter factory lambdas.
	// byte / setByte
	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off >= len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		return IntrinsicResult(make_double((Double)RawGetByte(hv, off)));
	});
	_rdKeys.Add("byte");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off >= len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		RawSetByte(hv, off, (Int32)as_double(ctx.GetArg(2)));
		return IntrinsicResult::Null;
	});
	_rdKeys.Add("setByte");

	// sbyte / setSbyte
	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off >= len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		return IntrinsicResult(make_double((Double)RawGetSByte(hv, off)));
	});
	_rdKeys.Add("sbyte");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off >= len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		RawSetSByte(hv, off, (Int32)as_double(ctx.GetArg(2)));
		return IntrinsicResult::Null;
	});
	_rdKeys.Add("setSbyte");

	// ushort / setUshort / short / setShort
	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off + 2 > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Value leVal = val_null; map_try_get(self, make_string("littleEndian"), &leVal);
		Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
		return IntrinsicResult(make_double((Double)RawGetU16(hv, off, le)));
	});
	_rdKeys.Add("ushort");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off + 2 > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Value leVal = val_null; map_try_get(self, make_string("littleEndian"), &leVal);
		Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
		RawSetU16(hv, off, (Int32)as_double(ctx.GetArg(2)), le);
		return IntrinsicResult::Null;
	});
	_rdKeys.Add("setUshort");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off + 2 > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Value leVal = val_null; map_try_get(self, make_string("littleEndian"), &leVal);
		Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
		return IntrinsicResult(make_double((Double)RawGetI16(hv, off, le)));
	});
	_rdKeys.Add("short");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off + 2 > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Value leVal = val_null; map_try_get(self, make_string("littleEndian"), &leVal);
		Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
		RawSetI16(hv, off, (Int32)as_double(ctx.GetArg(2)), le);
		return IntrinsicResult::Null;
	});
	_rdKeys.Add("setShort");

	// uint / setUint / int / setInt
	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off + 4 > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Value leVal = val_null; map_try_get(self, make_string("littleEndian"), &leVal);
		Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
		return IntrinsicResult(make_double(RawGetU32(hv, off, le)));
	});
	_rdKeys.Add("uint");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);

		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off + 4 > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Value leVal = val_null; map_try_get(self, make_string("littleEndian"), &leVal);
		Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
		RawSetU32(hv, off, as_double(ctx.GetArg(2)), le);
		return IntrinsicResult::Null;
	});
	_rdKeys.Add("setUint");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off + 4 > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Value leVal = val_null; map_try_get(self, make_string("littleEndian"), &leVal);
		Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
		return IntrinsicResult(make_double((Double)RawGetI32(hv, off, le)));
	});
	_rdKeys.Add("int");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off + 4 > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Value leVal = val_null; map_try_get(self, make_string("littleEndian"), &leVal);
		Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
		RawSetI32(hv, off, (Int32)as_double(ctx.GetArg(2)), le);
		return IntrinsicResult::Null;
	});
	_rdKeys.Add("setInt");

	// float / setFloat / double / setDouble
	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off + 4 > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Value leVal = val_null; map_try_get(self, make_string("littleEndian"), &leVal);
		Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
		return IntrinsicResult(make_double(RawGetF32(hv, off, le)));
	});
	_rdKeys.Add("float");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off + 4 > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Value leVal = val_null; map_try_get(self, make_string("littleEndian"), &leVal);
		Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
		RawSetF32(hv, off, as_double(ctx.GetArg(2)), le);
		return IntrinsicResult::Null;
	});
	_rdKeys.Add("setFloat");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off + 8 > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Value leVal = val_null; map_try_get(self, make_string("littleEndian"), &leVal);
		Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
		return IntrinsicResult(make_double(RawGetF64(hv, off, le)));
	});
	_rdKeys.Add("double");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off + 8 > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Value leVal = val_null; map_try_get(self, make_string("littleEndian"), &leVal);
		Boolean le = is_null(leVal) || as_double(leVal) != 0.0;
		RawSetF64(hv, off, as_double(ctx.GetArg(2)), le);
		return IntrinsicResult::Null;
	});
	_rdKeys.Add("setDouble");

	// utf8 / setUtf8
	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("bytes", make_double(-1.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Int32 byteCount = (Int32)as_double(ctx.GetArg(2));
		return IntrinsicResult(make_string(RawGetUTF8(hv, off, byteCount)));
	});
	_rdKeys.Add("utf8");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("offset", make_double(0.0)); f.AddParam("value", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		Int32 len = GetRawBufLen(hv); if (len < 0) return IntrinsicResult(ErrorType::FileError("RawData has no buffer"));
		Int32 off = (Int32)as_double(ctx.GetArg(1)); if (off < 0) off += len;
		if (off < 0 || off > len) return IntrinsicResult(ErrorType::FileError("index out of bounds"));
		Int32 written = RawSetUTF8(hv, off, to_String(ctx.GetArg(2)));
		return IntrinsicResult(make_double((Double)written));
	});
	_rdKeys.Add("setUtf8");

	// ── FileHandle methods ─────────────────────────────────────────────────
	_fhStart = Intrinsic::AllCount();

	f = Intrinsic::Create("");
	f.AddParam("self", val_null);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		CloseFileHandle(hv);
		map_set(self, make_string("_handle"), val_null);
		return IntrinsicResult::Null;
	});
	_fhKeys.Add("close");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		return IntrinsicResult(make_double(IsFileHandleOpen(hv) ? 1.0 : 0.0));
	});
	_fhKeys.Add("isOpen");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("data", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		if (!IsFileHandleOpen(hv)) return IntrinsicResult(ErrorType::FileError("file is not open"));
		Int32 n = WriteToFile(hv, to_String(ctx.GetArg(1)));
		return IntrinsicResult(make_double((Double)n));
	});
	_fhKeys.Add("write");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("data", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		if (!IsFileHandleOpen(hv)) return IntrinsicResult(ErrorType::FileError("file is not open"));
		Int32 n = WriteToFile(hv, to_String(ctx.GetArg(1)) + "\n");
		return IntrinsicResult(make_double((Double)n));
	});
	_fhKeys.Add("writeLine");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("byteCount", make_double(-1.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		if (!IsFileHandleOpen(hv)) return IntrinsicResult(ErrorType::FileError("file is not open"));
		return IntrinsicResult(make_string(ReadFromFile(hv, (Int32)as_double(ctx.GetArg(1)))));
	});
	_fhKeys.Add("read");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		if (!IsFileHandleOpen(hv)) return IntrinsicResult(ErrorType::FileError("file is not open"));
		return IntrinsicResult(ReadLineFromFile(hv));
	});
	_fhKeys.Add("readLine");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		if (!IsFileHandleOpen(hv)) return IntrinsicResult(ErrorType::FileError("file is not open"));
		return IntrinsicResult(make_double((Double)GetFilePosition(hv)));
	});
	_fhKeys.Add("position");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null); f.AddParam("pos", make_double(0.0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		if (!IsFileHandleOpen(hv)) return IntrinsicResult(ErrorType::FileError("file is not open"));
		SeekFilePosition(hv, (Int32)as_double(ctx.GetArg(1)));
		return IntrinsicResult::Null;
	});
	_fhKeys.Add("seek");

	f = Intrinsic::Create("");
	f.AddParam("self", val_null);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value hv = val_null; map_try_get(self, make_string("_handle"), &hv);
		if (!IsFileHandleOpen(hv)) return IntrinsicResult(ErrorType::FileError("file is not open"));
		return IntrinsicResult(make_double(IsFileAtEnd(hv) ? 1.0 : 0.0));
	});
	_fhKeys.Add("atEnd");

	// ── file module functions ───────────────────────────────────────────────
	_fmStart = Intrinsic::AllCount();

	f = Intrinsic::Create("");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_string(FsCurrentDir()));
	});
	_fmKeys.Add("curdir");

	f = Intrinsic::Create("");
	f.AddParam("path", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Boolean ok = FsSetDir(to_String(ctx.GetArg(0)));
		return IntrinsicResult(make_double(ok ? 1.0 : 0.0));	// ToDo: make_boolean
	});
	_fmKeys.Add("setdir");

	f = Intrinsic::Create("");
	f.AddParam("path", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(FsChildren(to_String(ctx.GetArg(0))));
	});
	_fmKeys.Add("children");

	f = Intrinsic::Create("");
	f.AddParam("path", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_string(FsBasename(to_String(ctx.GetArg(0)))));
	});
	_fmKeys.Add("name");

	f = Intrinsic::Create("");
	f.AddParam("path", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_string(FsDirname(to_String(ctx.GetArg(0)))));
	});
	_fmKeys.Add("parent");

	f = Intrinsic::Create("");
	f.AddParam("parentPath", make_string("")); f.AddParam("childName", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_string(FsChild(to_String(ctx.GetArg(0)), to_String(ctx.GetArg(1)))));
	});
	_fmKeys.Add("child");

	f = Intrinsic::Create("");
	f.AddParam("path", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(FsExists(to_String(ctx.GetArg(0))) ? 1.0 : 0.0));
	});
	_fmKeys.Add("exists");

	f = Intrinsic::Create("");
	f.AddParam("path", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value info = FsInfo(to_String(ctx.GetArg(0)));
		return IntrinsicResult(is_null(info) ? val_null : info);
	});
	_fmKeys.Add("info");

	f = Intrinsic::Create("");
	f.AddParam("path", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(FsMakeDir(to_String(ctx.GetArg(0))) ? 1.0 : 0.0));
	});
	_fmKeys.Add("makedir");

	f = Intrinsic::Create("");
	f.AddParam("oldPath", make_string("")); f.AddParam("newPath", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(FsMove(to_String(ctx.GetArg(0)), to_String(ctx.GetArg(1))) ? 1.0 : 0.0));
	});
	_fmKeys.Add("move");

	f = Intrinsic::Create("");
	f.AddParam("oldPath", make_string("")); f.AddParam("newPath", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(FsCopy(to_String(ctx.GetArg(0)), to_String(ctx.GetArg(1))) ? 1.0 : 0.0));
	});
	_fmKeys.Add("copy");

	f = Intrinsic::Create("");
	f.AddParam("path", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(FsDelete(to_String(ctx.GetArg(0))) ? 1.0 : 0.0));
	});
	_fmKeys.Add("delete");

	f = Intrinsic::Create("");
	f.AddParam("path", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = FsReadLines(to_String(ctx.GetArg(0)));
		return IntrinsicResult(v);
	});
	_fmKeys.Add("readLines");

	f = Intrinsic::Create("");
	f.AddParam("path", make_string("")); f.AddParam("lines", val_null);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = FsWriteLines(to_String(ctx.GetArg(0)), ctx.GetArg(1));
		return IntrinsicResult(v);
	});
	_fmKeys.Add("writeLines");

	f = Intrinsic::Create("");
	f.AddParam("path", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(FsLoadRaw(to_String(ctx.GetArg(0))));
	});
	_fmKeys.Add("loadRaw");

	f = Intrinsic::Create("");
	f.AddParam("path", make_string("")); f.AddParam("data", val_null);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value r = FsSaveRaw(to_String(ctx.GetArg(0)), ctx.GetArg(1));
		return IntrinsicResult(r);
	});
	_fmKeys.Add("saveRaw");

	// open(path, mode) — return a FileHandle instance or an error
	f = Intrinsic::Create("");
	f.AddParam("path", make_string("")); f.AddParam("mode", make_string("r+"));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		String path = to_String(ctx.GetArg(0));
		String mode = to_String(ctx.GetArg(1));
		Value hv = MakeFileHandle(path, mode);
		if (is_null(hv)) return IntrinsicResult(ErrorType::FileError("open: could not open: " + path));
		Value instance = make_map(4);
		map_set(instance, val_isa_key, GetFileHandleClassMap());
		map_set(instance, make_string("_handle"), hv);
		return IntrinsicResult(instance);
	});
	_fmKeys.Add("open");

	// ── Global intrinsics ─────────────────────────────────────────────────

	f = Intrinsic::Create("file");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(GetFileModuleMap());
	});

	f = Intrinsic::Create("FileHandle");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(GetFileHandleClassMap());
	});

	f = Intrinsic::Create("RawData");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(GetRawDataClassMap());
	});
}
void ShellIntrinsics::Init() {
	GCManager::RegisterMarkCallback(ShellIntrinsics::MarkRoots, nullptr);
	CoreIntrinsics::RegisterInvalidateCallback(ShellIntrinsics::InvalidateCaches);
	InitFileIntrinsics();

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
		String cmd = to_String(ctx.GetArg(0));
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
		String libname = to_String(ctx.GetArg(0));
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
		List<FuncDef> fns = gen.CompileImport(stmts, libname + ".ms");
		if (!is_null(gen.Error())) {
			ctx.vm.RaiseRuntimeError(StringUtils::Format("import: compile error in {0}.ms", libname));
			return IntrinsicResult::Null;
		}
		// Push the module call; we will be re-invoked when it finishes.
		// fns[0] is the module's @main; nested functions are reachable
		// from its constant pool.
		ctx.vm.ManuallyPushCall(ctx.baseIndex, fns[0]);
		return IntrinsicResult(make_string(libname), Boolean(false));
	});
}

} // end of namespace MiniScript
