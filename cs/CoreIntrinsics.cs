// CoreIntrinsics.cs - Definitions of all built-in intrinsic functions.

using System;
using System.Collections.Generic;
using static MiniScript.Value;
// H: #include "value.h"
// H: #include "ErrorTypes.g.h"
// CPP: #include "Intrinsic.g.h"
// H: #include "GCManager.g.h"
// CPP: #include "value_list.h"
// CPP: #include "value_string.h"
// CPP: #include "value_map.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "VM.g.h"
// CPP: #include "IntrinsicAPI.g.h"
// CPP: #include "CS_Math.h"
// CPP: #include "CS_value_util.h"
// CPP: #include "Interpreter.g.h"
// CPP: #include "PRNG.g.h"

/*** BEGIN CPP_ONLY ***
#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <fstream>
#include <string>
#endif
*** END CPP_ONLY ***/

namespace MiniScript {

// H: typedef void (*VoidCallback)();

public static class CoreIntrinsics {

	public static String BuildDate() {
		//*** BEGIN CS_ONLY ***
		String buildDate = "unknown";
		String asmPath = System.Reflection.Assembly.GetExecutingAssembly().Location;
		if (asmPath != null && asmPath.Length > 0) {
			System.DateTime dt = System.IO.File.GetLastWriteTime(asmPath);
			buildDate = dt.ToString("yyyy-MM-dd");
		}
		return buildDate;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		String mmm_dd_yyyy(__DATE__);
		String dd = mmm_dd_yyyy.Substring(4, 2).Replace(' ', '0');
		String yyyy = mmm_dd_yyyy.Substring(7, 4);
		String mmm = mmm_dd_yyyy.Substring(0, 3);
		String mm;
		if (mmm == "Jan") mm = "01";
		else if (mmm == "Feb") mm = "02";
		else if (mmm == "Mar") mm = "03";
		else if (mmm == "Apr") mm = "04";
		else if (mmm == "May") mm = "05";
		else if (mmm == "Jun") mm = "06";
		else if (mmm == "Jul") mm = "07";
		else if (mmm == "Aug") mm = "08";
		else if (mmm == "Sep") mm = "09";
		else if (mmm == "Oct") mm = "10";
		else if (mmm == "Nov") mm = "11";
		else if (mmm == "Dec") mm = "12";
		else mm = mmm;
		return yyyy + "-" + mm + "-" + dd;
		*** END CPP_ONLY ***/
	}
	
	public static String PlatformName() {
		//*** BEGIN CS_ONLY ***
		return System.Runtime.InteropServices.RuntimeInformation.OSDescription;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		#if defined(__APPLE__)
		String platform = "macOS";
		char osversion[32];
		size_t osversion_len = sizeof(osversion);
		if (sysctlbyname("kern.osproductversion", osversion, &osversion_len, NULL, 0) == 0) {
			platform = String("macOS ") + osversion;
		}
		return platform;
		#elif defined(_WIN32)
		String platform = "Windows";
		typedef LONG(WINAPI* RtlGetVersionPtr)(OSVERSIONINFOW*);
		HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
		if (ntdll) {
			RtlGetVersionPtr fn = (RtlGetVersionPtr)GetProcAddress(ntdll, "RtlGetVersion");
			if (fn) {
				OSVERSIONINFOW osvi = {};
				osvi.dwOSVersionInfoSize = sizeof(osvi);
				if (fn(&osvi) == 0) {
					platform = Interp("Windows {}.{}", (int)osvi.dwMajorVersion, (int)osvi.dwMinorVersion);
				}
			}
		}
		return platform;
		#elif defined(__linux__)
		String platform = "Linux";
		{
			std::ifstream osrelease("/etc/os-release");
			std::string line;
			while (std::getline(osrelease, line)) {
				if (line.compare(0, 12, "PRETTY_NAME=") == 0) {
					std::string val = line.substr(12);
					if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
						val = val.substr(1, val.size() - 2);
					}
					platform = String(val.c_str());
					break;
				}
			}
		}
		return platform;
		#else
		return String("Unknown");
		#endif
		*** END CPP_ONLY ***/
	}

	// Host app identity — set these before the first call to `version`.
	public static String hostName = "";
	public static String hostInfo = "";
	public static String hostVersion = "";

	// Coerce a numeric argument per the language's number/string policy.
	// Returns Value.Null on success (with `result` set); otherwise returns an
	// error Value to be returned from the intrinsic: a TypeError when the value
	// is the wrong type (not a number or string), or a FormatError when it is a
	// string that does not parse as a number.  Callers should check/propagate
	// is_error(v) before calling this.
	private static Value RequireNumber(Value v, out double result) {
		if (is_number(v)) { result = numeric_val(v); return Value.Null; }
		if (is_string(v)) {
			if (StringUtils.TryParseDouble(as_cstring(v), out result)) return Value.Null;
			result = 0.0;
			return ErrorTypes.FormatError(StringUtils.Format("'{0}' is not a valid number", as_cstring(v)));
		}
		result = 0.0;
		return ErrorTypes.TypeError("number", v);
	}

	private static void AddIntrinsicToMap(Value map, String methodName) {
		Intrinsic intr = Intrinsic.GetByName(methodName);
		if (intr != null) {
			map_set(map, methodName, intr.GetFunc());
		} else {
			IOHelper.Print(StringUtils.Format("Intrinsic not found: {0}", methodName));
		}
	}

	// 
	// ListType: a static map that represents the `list` type, and provides
	// intrinsic methods that can be invoked on it via dot syntax.
	// 
	public static Value ListType() {
		if (is_null(_listType)) {
			_listType = make_map(16);
			AddIntrinsicToMap(_listType, "hasIndex");
			AddIntrinsicToMap(_listType, "indexes");
			AddIntrinsicToMap(_listType, "indexOf");
			AddIntrinsicToMap(_listType, "insert");
			AddIntrinsicToMap(_listType, "join");
			AddIntrinsicToMap(_listType, "len");
			AddIntrinsicToMap(_listType, "pop");
			AddIntrinsicToMap(_listType, "pull");
			AddIntrinsicToMap(_listType, "push");
			AddIntrinsicToMap(_listType, "shuffle");
			AddIntrinsicToMap(_listType, "sort");
			AddIntrinsicToMap(_listType, "sum");
			AddIntrinsicToMap(_listType, "remove");
			AddIntrinsicToMap(_listType, "replace");
			AddIntrinsicToMap(_listType, "values");
			Intrinsic.AddShortName(_listType, "list");
		}
		return _listType;
	}
	private static Value _listType = Value.Null;

	// 
	// StringType: a static map that represents the `string` type, and provides
	// intrinsic methods that can be invoked on it via dot syntax.
	// 
	public static Value StringType() {
		if (is_null(_stringType)) {
			_stringType = make_map(16);
			AddIntrinsicToMap(_stringType, "hasIndex");
			AddIntrinsicToMap(_stringType, "indexes");
			AddIntrinsicToMap(_stringType, "indexOf");
			AddIntrinsicToMap(_stringType, "insert");
			AddIntrinsicToMap(_stringType, "code");
			AddIntrinsicToMap(_stringType, "len");
			AddIntrinsicToMap(_stringType, "lower");
			AddIntrinsicToMap(_stringType, "val");
			AddIntrinsicToMap(_stringType, "remove");
			AddIntrinsicToMap(_stringType, "replace");
			AddIntrinsicToMap(_stringType, "split");
			AddIntrinsicToMap(_stringType, "upper");
			AddIntrinsicToMap(_stringType, "values");
			Intrinsic.AddShortName(_stringType, "string");
		}
		return _stringType;
	}
	private static Value _stringType = Value.Null;

	// 
	// MapType: a static map that represents the `map` type, and provides
	// intrinsic methods that can be invoked on it via dot syntax.
	// 
	public static Value MapType() {
		if (is_null(_mapType)) {
			_mapType = make_map(16);
			AddIntrinsicToMap(_mapType, "hasIndex");
			AddIntrinsicToMap(_mapType, "indexes");
			AddIntrinsicToMap(_mapType, "indexOf");
			AddIntrinsicToMap(_mapType, "len");
			AddIntrinsicToMap(_mapType, "pop");
			AddIntrinsicToMap(_mapType, "push");
			AddIntrinsicToMap(_mapType, "pull");
			AddIntrinsicToMap(_mapType, "shuffle");
			AddIntrinsicToMap(_mapType, "sum");
			AddIntrinsicToMap(_mapType, "remove");
			AddIntrinsicToMap(_mapType, "replace");
			AddIntrinsicToMap(_mapType, "values");
			Intrinsic.AddShortName(_mapType, "map");
		}
		return _mapType;
	}
	private static Value _mapType = Value.Null;
	
	// 
	// NumberType: a static map that represents the `number` type.
	// 
	public static Value NumberType() {
		if (is_null(_numberType)) {
			_numberType = make_map(4);
			Intrinsic.AddShortName(_numberType, "number");
		}
		return _numberType;
	}
	private static Value _numberType = Value.Null;	

	// 
	// FunctionType: a static map that represents the `funcRef` type.
	// 
	public static Value FunctionType() {
		if (is_null(_functionType)) {
			_functionType = make_map(4);
			Intrinsic.AddShortName(_functionType, "funcRef");
		}
		return _functionType;
	}
	private static Value _functionType = Value.Null;

	//
	// ErrorType: a static map that represents the `error` type, and provides
	// intrinsic methods that can be invoked on an error via dot syntax
	// (notably `err` for creating a specialization).
	//
	public static Value ErrorType() {
		if (is_null(_errorType)) {
			_errorType = make_map(4);
			if (_errorErrIntr != null) {
				map_set(_errorType, "err", _errorErrIntr.GetFunc());
			}
			Intrinsic.AddShortName(_errorType, "error");
		}
		return _errorType;
	}
	private static Value _errorType = Value.Null;
	private static Intrinsic _errorErrIntr = null;

	private static Value _EOL = make_string("\n");

	// REPL history lists, set by App.RunREPL at startup and by the reset intrinsic.
	public static Value replInList = Value.Null;
	public static Value replOutList = Value.Null;

	public static void MarkRoots(object user_data) {
		GCManager.Mark(_listType);
		GCManager.Mark(_stringType);
		GCManager.Mark(_mapType);
		GCManager.Mark(_numberType);
		GCManager.Mark(_functionType);
		GCManager.Mark(_errorType);
		GCManager.Mark(_gcMap);
		GCManager.Mark(_versionMap);
		GCManager.Mark(replInList);
		GCManager.Mark(replOutList);
	}

	public static void Init() {
		GCManager.RegisterMarkCallback(MarkRoots, null); // CPP: GCManager::RegisterMarkCallback(CoreIntrinsics::MarkRoots, nullptr);

		Intrinsic f;

		// print(s="")
		f = Intrinsic.Create("print");
		f.AddParam("s", make_string(""));
		f.AddParam("delimiter", _EOL);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			String s = as_cstring(to_string(ctx.GetArg(0), ctx.vm));
			Value delimiterVal = ctx.GetArg(1);
			Interpreter interp = ctx.vm.GetInterpreter();
			if (interp != null && interp.standardOutput != null) {
				if (is_null(delimiterVal)) {
					interp.standardOutput(s, true);
				} else {
					String delimiter = as_cstring(delimiterVal);
					if (delimiter == "\n") {
						interp.standardOutput(s, true);
					} else {
						interp.standardOutput(s + delimiter, false);
					}
				}
			} else {
				IOHelper.Print(s);
			}
			return new IntrinsicResult(Value.Null);
		};

		// input(prompt=null)
		f = Intrinsic.Create("input");
		f.AddParam("prompt");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			String prompt = new String("");
			if (!is_null(ctx.GetArg(0))) {
				prompt = StringUtils.Format("{0}", ctx.GetArg(0));
			}
			String result = IOHelper.Input(prompt);
			return new IntrinsicResult(make_string(result));
		};

		// err(msg, inner=null) — global intrinsic: create a new error value.
		f = Intrinsic.Create("err");
		f.AddParam("msg");
		f.AddParam("inner");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value msg = ctx.GetArg(0);
			Value inner = ctx.GetArg(1);
			if (!is_string(msg)) msg = to_string(msg, ctx.vm);
			return new IntrinsicResult(make_error(msg, inner, ctx.vm.BuildStackTrace(), Value.Null));
		};

		// err method on ErrorType: se.err(msg, inner=null) creates a new error
		// whose __isa is se.  Terminates if this would create an __isa cycle.
		_errorErrIntr = Intrinsic.Create("");
		f = _errorErrIntr;
		f.AddParam("self");
		f.AddParam("msg");
		f.AddParam("inner");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			Value msg = ctx.GetArg(1);
			Value inner = ctx.GetArg(2);
			if (!is_error(self)) {
				ctx.vm.RaiseRuntimeError("err method called on non-error value");
				return IntrinsicResult.Null;
			}
			if (!is_string(msg)) msg = to_string(msg, ctx.vm);
			// Build the new error with self as __isa.  Then verify no cycle.
			Value newErr = make_error(msg, inner, ctx.vm.BuildStackTrace(), self);
			// Walk chain from newErr to check for loop (if newErr appears again).
			Value current = self;
			for (int depth = 0; depth < 256; depth++) {
				if (is_null(current)) break;
				if (!is_error(current)) break;
				if (value_identical(current, newErr)) {
					ctx.vm.RaiseRuntimeError("err: __isa chain would form a cycle");
					return IntrinsicResult.Null;
				}
				current = error_isa(current);
			}
			return new IntrinsicResult(newErr);
		};

		// info(ref)
		f = Intrinsic.Create("info");
		f.AddParam("ref");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value arg = ctx.GetArg(0);
			Value result = make_map(8);
			Value parameters = Value.Null;
			Value pinfo = Value.Null;
			map_set(result, "type", value_type_name(arg));
			if (is_list(arg)) {
				Boolean computed = GCManager.Lists.Get(value_item_index(arg)).Computed;
				map_set(result, "computed", make_int(computed));
				map_set(result, "frozen", make_int(is_frozen(arg)));
			} else if (is_map(arg)) {
				map_set(result, "frozen", make_int(is_frozen(arg)));
			} else if (is_funcref(arg)) {
				FuncDef func = funcref_funcdef(arg);
				map_set(result, "name", func.Name);
				map_set(result, "note", func.Note);
				parameters = make_list(func.ParamNames.Count);
				for (int i=0; i < func.ParamNames.Count; i++) {
					pinfo = make_map(2);
					map_set(pinfo, "name", func.ParamNames[i]);
					map_set(pinfo, "default", func.ParamDefaults[i]);
					list_push(parameters, pinfo);
				}
				map_set(result, "params", parameters);
				if (is_null(funcref_outer_vars(arg))) {
					map_set(result, "closure", Value.zero);
				} else {
					map_set(result, "closure", Value.one);
				}
			} else if (is_error(arg)) {
				map_set(result, "message", error_message(arg));
				map_set(result, "inner", error_inner(arg));
				map_set(result, "stack", error_stack(arg));
				map_set(result, "isa", error_isa(arg));
			}
			freeze_value(result);
			return new IntrinsicResult(result);
		};

		// val(self=0)
		f = Intrinsic.Create("val");
		f.AddParam("self", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			if (is_number(v)) return new IntrinsicResult(v);
			if (is_string(v)) return new IntrinsicResult(to_number(v));
			return new IntrinsicResult(Value.Null);
		};

		// str(x="")
		f = Intrinsic.Create("str");
		f.AddParam("x", make_string(""));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			if (is_null(v)) return new IntrinsicResult(make_string(""));
			return new IntrinsicResult(to_string(v, ctx.vm));
		};

		// upper(self)
		f = Intrinsic.Create("upper");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(string_upper(v));
		};

		// lower(self)
		f = Intrinsic.Create("lower");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(string_lower(v));
		};

		// char(codePoint=65)
		f = Intrinsic.Create("char");
		f.AddParam("codePoint", make_int(65));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			double cp;
			{ Value e = RequireNumber(v, out cp); if (!is_null(e)) return new IntrinsicResult(e); }
			return new IntrinsicResult(string_from_code_point((int)cp));
		};

		// code(self)
		f = Intrinsic.Create("code");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			if (!is_string(v)) return new IntrinsicResult(ErrorTypes.TypeError("string", v));
			return new IntrinsicResult(make_int(string_code_point(v)));
		};

		// len(x)
		f = Intrinsic.Create("len");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value container = ctx.GetArg(0);
			if (is_error(container)) return new IntrinsicResult(container);
			Value result = Value.Null;
			if (is_list(container)) {
				result = make_int(list_count(container));
			} else if (is_string(container)) {
				result = make_int(string_length(container));
			} else if (is_map(container)) {
				result = make_int(map_count(container));
			}
			return new IntrinsicResult(result);
		};

		// remove(self, index)
		f = Intrinsic.Create("remove");
		f.AddParam("self");
		f.AddParam("index");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value container = ctx.GetArg(0);
			if (is_error(container)) return ctx.vm.RaiseUncaughtError(container);
			int result = 0;
			if (is_list(container)) {
				result = list_remove(container, as_int(ctx.GetArg(1))) ? 1 : 0;
			} else if (is_map(container)) {
				result = map_remove(container, ctx.GetArg(1)) ? 1 : 0;
			} else {
				return new IntrinsicResult(ErrorTypes.TypeError("list or map", container));
			}
			return new IntrinsicResult(make_int(result));
		};

		// freeze(x)
		f = Intrinsic.Create("freeze");
		f.AddParam("x");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return ctx.vm.RaiseUncaughtError(v);
			freeze_value(v);
			return new IntrinsicResult(Value.Null);
		};

		// isFrozen(x)
		f = Intrinsic.Create("isFrozen");
		f.AddParam("x");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(make_int(is_frozen(v)));
		};

		// frozenCopy(x)
		f = Intrinsic.Create("frozenCopy");
		f.AddParam("x");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(frozen_copy(v));
		};

		// abs(x=0)
		f = Intrinsic.Create("abs");
		f.AddParam("x", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			double x;
			{ Value e = RequireNumber(v, out x); if (!is_null(e)) return new IntrinsicResult(e); }
			return new IntrinsicResult(make_double(Math.Abs(x)));
		};

		// acos(x=0)
		f = Intrinsic.Create("acos");
		f.AddParam("x", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			double x;
			{ Value e = RequireNumber(v, out x); if (!is_null(e)) return new IntrinsicResult(e); }
			return new IntrinsicResult(make_double(Math.Acos(x)));
		};

		// asin(x=0)
		f = Intrinsic.Create("asin");
		f.AddParam("x", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			double x;
			{ Value e = RequireNumber(v, out x); if (!is_null(e)) return new IntrinsicResult(e); }
			return new IntrinsicResult(make_double(Math.Asin(x)));
		};

		// atan(y=0, x=1)
		f = Intrinsic.Create("atan");
		f.AddParam("y", Value.zero);
		f.AddParam("x", Value.one);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value vy = ctx.GetArg(0);
			if (is_error(vy)) return new IntrinsicResult(vy);
			Value vx = ctx.GetArg(1);
			if (is_error(vx)) return new IntrinsicResult(vx);
			double y, x;
			{ Value e = RequireNumber(vy, out y); if (!is_null(e)) return new IntrinsicResult(e); }
			{ Value e = RequireNumber(vx, out x); if (!is_null(e)) return new IntrinsicResult(e); }
			if (x == 1.0) return new IntrinsicResult(make_double(Math.Atan(y)));
			return new IntrinsicResult(make_double(Math.Atan2(y, x)));
		};

		// ceil(x=0)
		f = Intrinsic.Create("ceil");
		f.AddParam("x", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			double x;
			{ Value e = RequireNumber(v, out x); if (!is_null(e)) return new IntrinsicResult(e); }
			return new IntrinsicResult(make_double(Math.Ceiling(x)));
		};

		// cos(radians=0)
		f = Intrinsic.Create("cos");
		f.AddParam("radians", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			double x;
			{ Value e = RequireNumber(v, out x); if (!is_null(e)) return new IntrinsicResult(e); }
			return new IntrinsicResult(make_double(Math.Cos(x)));
		};

		// floor(x=0)
		f = Intrinsic.Create("floor");
		f.AddParam("x", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			double x;
			{ Value e = RequireNumber(v, out x); if (!is_null(e)) return new IntrinsicResult(e); }
			return new IntrinsicResult(make_double(Math.Floor(x)));
		};

		// log(x=0, base=10)
		f = Intrinsic.Create("log");
		f.AddParam("x", Value.zero);
		f.AddParam("base", make_int(10));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value vx = ctx.GetArg(0);
			if (is_error(vx)) return new IntrinsicResult(vx);
			Value vb = ctx.GetArg(1);
			if (is_error(vb)) return new IntrinsicResult(vb);
			double x, b;
			{ Value e = RequireNumber(vx, out x); if (!is_null(e)) return new IntrinsicResult(e); }
			{ Value e = RequireNumber(vb, out b); if (!is_null(e)) return new IntrinsicResult(e); }
			double result;
			if (Math.Abs(b - 2.718282) < 0.000001) result = Math.Log(x);
			else result = Math.Log(x) / Math.Log(b);
			return new IntrinsicResult(make_double(result));
		};

		// pi
		f = Intrinsic.Create("pi");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(make_double(Math.PI));
		};

		// round(x=0, decimalPlaces=0)
		f = Intrinsic.Create("round");
		f.AddParam("x", Value.zero);
		f.AddParam("decimalPlaces", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value vx = ctx.GetArg(0);
			if (is_error(vx)) return new IntrinsicResult(vx);
			Value vd = ctx.GetArg(1);
			if (is_error(vd)) return new IntrinsicResult(vd);
			double num, decimals;
			{ Value e = RequireNumber(vx, out num); if (!is_null(e)) return new IntrinsicResult(e); }
			{ Value e = RequireNumber(vd, out decimals); if (!is_null(e)) return new IntrinsicResult(e); }
			int decimalPlaces = (int)decimals;
			if (decimalPlaces >= 0) {
				if (decimalPlaces > 15) decimalPlaces = 15;
				num = Math.Round(num, decimalPlaces, MidpointRounding.AwayFromZero); // CPP: num = Math::Round(num, decimalPlaces);
			} else {
				double pow10 = Math.Pow(10, -decimalPlaces);
				num /= pow10;
				num = Math.Round(num, MidpointRounding.AwayFromZero); // CPP: num = Math::Round(num);
				num *= pow10;
			}
			return new IntrinsicResult(make_double(num));
		};

		// rnd(seed)
		f = Intrinsic.Create("rnd");
		f.AddParam("seed");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return ctx.vm.RaiseUncaughtError(v);
			// If a seed is supplied, reseed the generator before drawing.  null
			// means "no seed"; a number (or numeric string) reseeds; any other
			// type is a parameter error.
			if (!is_null(v)) {
				double seed;
				{ Value e = RequireNumber(v, out seed); if (!is_null(e)) return new IntrinsicResult(e); }
				PRNG.Seed((UInt64)(Int64)seed);
			}
			return new IntrinsicResult(make_double(PRNG.Next()));
		};

		// sign(x=0)
		f = Intrinsic.Create("sign");
		f.AddParam("x", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			double x;
			{ Value e = RequireNumber(v, out x); if (!is_null(e)) return new IntrinsicResult(e); }
			return new IntrinsicResult(make_int(Math.Sign(x)));
		};

		// sin(radians=0)
		f = Intrinsic.Create("sin");
		f.AddParam("radians", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			double x;
			{ Value e = RequireNumber(v, out x); if (!is_null(e)) return new IntrinsicResult(e); }
			return new IntrinsicResult(make_double(Math.Sin(x)));
		};

		// sqrt(x=0)
		f = Intrinsic.Create("sqrt");
		f.AddParam("x", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			double x;
			{ Value e = RequireNumber(v, out x); if (!is_null(e)) return new IntrinsicResult(e); }
			return new IntrinsicResult(make_double(Math.Sqrt(x)));
		};

		// tan(radians=0)
		f = Intrinsic.Create("tan");
		f.AddParam("radians", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			double x;
			{ Value e = RequireNumber(v, out x); if (!is_null(e)) return new IntrinsicResult(e); }
			return new IntrinsicResult(make_double(Math.Tan(x)));
		};
		// push(self, value)
		f = Intrinsic.Create("push");
		f.AddParam("self");
		f.AddParam("value");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
			Value value = ctx.GetArg(1);
			if (is_list(self)) {
				list_push(self, value);
				return new IntrinsicResult(self);
			} else if (is_map(self)) {
				map_set(self, value, Value.one);
				return new IntrinsicResult(self);
			}
			return new IntrinsicResult(ErrorTypes.TypeError("list or map", self));
		};

		// pop(self)
		f = Intrinsic.Create("pop");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
			Value result = Value.Null;
			if (is_list(self)) {
				result = list_pop(self);
			} else if (is_map(self)) {
				if (map_count(self) == 0) return new IntrinsicResult(Value.Null);
				MapIterator iter = map_iterator(self);
				if (map_iterator_next(ref iter)) { // CPP: if (map_iterator_next(&iter, &result, nullptr)) {
					result = iter.Key;             // CPP: // remove key that was found
					map_remove(self, result);
				}
			} else {
				return new IntrinsicResult(ErrorTypes.TypeError("list or map", self));
			}
			return new IntrinsicResult(result);
		};

		// pull(self)
		f = Intrinsic.Create("pull");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
			Value result = Value.Null;
			if (is_list(self)) {
				result = list_pull(self);
			} else if (is_map(self)) {
				if (map_count(self) == 0) return new IntrinsicResult(Value.Null);
				MapIterator iter = map_iterator(self);
				if (map_iterator_next(ref iter)) { // CPP: if (map_iterator_next(&iter, &result, nullptr)) {
					result = iter.Key;             // CPP: // remove key that was found
					map_remove(self, result);
				}
			} else {
				return new IntrinsicResult(ErrorTypes.TypeError("list or map", self));
			}
			return new IntrinsicResult(result);
		};

		// insert(self, index, value)
		f = Intrinsic.Create("insert");
		f.AddParam("self");
		f.AddParam("index");
		f.AddParam("value");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
			int index = (int)numeric_val(ctx.GetArg(1));
			Value value = ctx.GetArg(2);
			if (is_list(self)) {
				list_insert(self, index, value);
				return new IntrinsicResult(self);
			} else if (is_string(self)) {
				return new IntrinsicResult(string_insert(self, index, value, ctx.vm));
			}
			return new IntrinsicResult(ErrorTypes.TypeError("list or string", self));
		};

		// indexOf(self, value, after=null)
		f = Intrinsic.Create("indexOf");
		f.AddParam("self");
		f.AddParam("value");
		f.AddParam("after");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return new IntrinsicResult(self);
			Value value = ctx.GetArg(1);
			Value after = ctx.GetArg(2);
			Value result = Value.Null;
			// CPP: Value iterKey, iterVal;
			if (is_list(self)) {
				int afterIdx = -1;
				if (!is_null(after)) {
					afterIdx = (int)numeric_val(after);
					if (afterIdx < -1) afterIdx += list_count(self);
				}
				int idx = list_indexOf(self, value, afterIdx);
				if (idx >= 0) result = make_int(idx);
			} else if (is_string(self)) {
				if (!is_string(value)) return new IntrinsicResult(Value.Null);
				int afterIdx = -1;
				if (!is_null(after)) {
					afterIdx = (int)numeric_val(after);
					if (afterIdx < -1) afterIdx += string_length(self);
				}
				int idx = string_indexOf(self, value, afterIdx + 1);
				if (idx >= 0) result = make_int(idx);
			} else if (is_map(self)) {
				// Find key where value matches
				bool pastAfter = is_null(after);
				MapIterator iter = map_iterator(self);
				while (map_iterator_next(ref iter)) { // CPP: while (map_iterator_next(&iter, &iterKey, &iterVal)) {
					if (!pastAfter) {
						if (value_equal(iter.Key, after)) { // CPP: if (value_equal(iterKey, after)) {
							pastAfter = true;
						}
						continue;
					}
					if (value_equal(iter.Val, value)) {  // CPP: if (value_equal(iterVal, value)) {
						result = iter.Key; // CPP: result = iterKey;
						break;
					}
				}
			} else {
				return new IntrinsicResult(ErrorTypes.TypeError("list, string, or map", self));
			}
			return new IntrinsicResult(result);
		};

		// sort(self, byKey=null, ascending=1)
		f = Intrinsic.Create("sort");
		f.AddParam("self");
		f.AddParam("byKey");
		f.AddParam("ascending", Value.one);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
			Value byKey = ctx.GetArg(1);
			bool ascending = is_truthy(ctx.GetArg(2));
			if (!is_list(self)) return new IntrinsicResult(ErrorTypes.TypeError("list", self));
			if (list_count(self) < 2) return new IntrinsicResult(self);
			if (is_null(byKey)) {
				list_sort(self, ascending);
			} else {
				list_sort_by_key(self, byKey, ascending);
			}
			return new IntrinsicResult(self);
		};

		// shuffle(self)
		f = Intrinsic.Create("shuffle");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
			Value temp;
			// CPP: Value iterKey, iterVal;
			if (is_list(self)) {
				if (is_frozen(self)) { ctx.vm.RaiseRuntimeError("Attempt to modify a frozen list"); return new IntrinsicResult(Value.Null); }
				int count = list_count(self);
				for (int i = count - 1; i > 0; i--) {
					int j = (int)(PRNG.Next() * (i + 1));
					temp = list_get(self, i);
					list_set(self, i, list_get(self, j));
					list_set(self, j, temp);
				}
			} else if (is_map(self)) {
				if (is_frozen(self)) { ctx.vm.RaiseRuntimeError("Attempt to modify a frozen map"); return new IntrinsicResult(Value.Null); }
				// Collect keys and values
				int count = map_count(self);
				List<Value> keys = new List<Value>(count);
				List<Value> vals = new List<Value>(count);
				MapIterator iter = map_iterator(self);
				while (map_iterator_next(ref iter)) { // CPP: while (map_iterator_next(&iter, &iterKey, &iterVal)) {
					keys.Add(iter.Key);               // CPP: vals.Add(iterKey);
					vals.Add(iter.Val);               // CPP: vals.Add(iterVal);
				}
				// Fisher-Yates shuffle on values
				for (int i = count - 1; i > 0; i--) {
					int j = (int)(PRNG.Next() * (i + 1));
					temp = vals[i];
					vals[i] = vals[j];
					vals[j] = temp;
				}
				for (int i = 0; i < count; i++) {
					map_set(self, keys[i], vals[i]);
				}
			} else {
				return new IntrinsicResult(ErrorTypes.TypeError("list or map", self));
			}
			return new IntrinsicResult(Value.Null);
		};

		// join(self, delimiter=" ")
		f = Intrinsic.Create("join");
		f.AddParam("self");
		f.AddParam("delimiter", make_string(" "));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return new IntrinsicResult(self);
			if (!is_list(self)) return new IntrinsicResult(ErrorTypes.TypeError("list", self));
			Value delim = ctx.GetArg(1);
			String delimStr = is_null(delim) ? " " : to_String(delim);
			int count = list_count(self);
			List<String> parts = new List<String>(count);
			for (int i = 0; i < count; i++) {
				parts.Add(to_String(list_get(self, i)));
			}
			return new IntrinsicResult(make_string(String.Join(delimStr, parts)));
		};

		// split(self, delimiter=" ", maxCount=-1)
		f = Intrinsic.Create("split");
		f.AddParam("self");
		f.AddParam("delimiter", make_string(" "));
		f.AddParam("maxCount", make_int(-1));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return new IntrinsicResult(self);
			if (!is_string(self)) return new IntrinsicResult(ErrorTypes.TypeError("string", self));
			Value delim = ctx.GetArg(1);
			int maxCount = (int)numeric_val(ctx.GetArg(2));
			return new IntrinsicResult(string_split_max(self, delim, maxCount));
		};

		// replace(self, oldval, newval, maxCount=null)
		f = Intrinsic.Create("replace");
		f.AddParam("self");
		f.AddParam("oldval");
		f.AddParam("newval");
		f.AddParam("maxCount");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
			Value oldVal = ctx.GetArg(1);
			Value newVal = ctx.GetArg(2);
			Value maxCountVal = ctx.GetArg(3);
			// CPP: Value iterKey, iterVal;
			int maxCount = is_null(maxCountVal) ? -1 : (int)numeric_val(maxCountVal);
			if (is_list(self)) {
				int count = list_count(self);
				int found = 0;
				for (int i = 0; i < count; i++) {
					if (value_equal(list_get(self, i), oldVal)) {
						list_set(self, i, newVal);
						found++;
						if (maxCount > 0 && found >= maxCount) break;
					}
				}
				return new IntrinsicResult(self);
			} else if (is_map(self)) {
				// Collect keys whose values match
				List<Value> keysToChange = new List<Value>();
				MapIterator iter = map_iterator(self);
				while (map_iterator_next(ref iter)) { // CPP: while (map_iterator_next(&iter, &iterKey, &iterVal)) {
					if (value_equal(iter.Val, oldVal)) { // CPP: if (value_equal(iterVal, oldVal)) {
						keysToChange.Add(iter.Key);  // CPP: keysToChange.Add(iterKey);
						if (maxCount > 0 && keysToChange.Count >= maxCount) break;
					}
				}
				for (int i = 0; i < keysToChange.Count; i++) {
					map_set(self, keysToChange[i], newVal);
				}
				return new IntrinsicResult(self);
			} else if (is_string(self)) {
				return new IntrinsicResult(string_replace_max(self, oldVal, newVal, maxCount));
			}
			return new IntrinsicResult(ErrorTypes.TypeError("list, map, or string", self));
		};

		// sum(self)
		f = Intrinsic.Create("sum");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return new IntrinsicResult(self);
			// CPP: Value iterVal;
			double total = 0;
			if (is_list(self)) {
				int count = list_count(self);
				for (int i = 0; i < count; i++) {
					total += numeric_val(list_get(self, i));
				}
			} else if (is_map(self)) {
				MapIterator iter = map_iterator(self);
				while (map_iterator_next(ref iter)) { // CPP: while (map_iterator_next(&iter, nullptr, &iterVal)) {
					total += numeric_val(iter.Val);   // CPP: total += numeric_val(iterVal);
				}
			} else {
				return new IntrinsicResult(Value.zero);
			}
			if (total == (int)total && total >= Int32.MinValue && total <= Int32.MaxValue) {
				return new IntrinsicResult(make_int((int)total));
			}
			return new IntrinsicResult(make_double(total));
		};

		// slice(seq, from=0, to=null)
		f = Intrinsic.Create("slice");
		f.AddParam("seq");
		f.AddParam("from", Value.zero);
		f.AddParam("to");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value seq = ctx.GetArg(0);
			if (is_error(seq)) return new IntrinsicResult(seq);
			int fromIdx = (int)numeric_val(ctx.GetArg(1));
			if (is_list(seq)) {
				int count = list_count(seq);
				int toIdx = is_null(ctx.GetArg(2)) ? count : (int)numeric_val(ctx.GetArg(2));
				return new IntrinsicResult(list_slice(seq, fromIdx, toIdx));
			} else if (is_string(seq)) {
				int slen = string_length(seq);
				int toIdx = is_null(ctx.GetArg(2)) ? slen : (int)numeric_val(ctx.GetArg(2));
				return new IntrinsicResult(string_slice(seq, fromIdx, toIdx));
			}
			return new IntrinsicResult(ErrorTypes.TypeError("list or string", seq));
		};

		// indexes(self)
		f = Intrinsic.Create("indexes");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return new IntrinsicResult(self);
			Value result = Value.Null;
			// CPP: Value iterKey;
			if (is_list(self)) {
				int count = list_count(self);
				result = make_list(count);
				for (int i = 0; i < count; i++) {
					list_push(result, make_int(i));
				}
				return new IntrinsicResult(result);
			} else if (is_string(self)) {
				int slen = string_length(self);
				result = make_list(slen);
				for (int i = 0; i < slen; i++) {
					list_push(result, make_int(i));
				}
				return new IntrinsicResult(result);
			} else if (is_map(self)) {
				result = make_list(map_count(self));
				MapIterator iter = map_iterator(self);
				while (map_iterator_next(ref iter)) { // CPP: while (map_iterator_next(&iter, &iterKey, nullptr)) {
					list_push(result, iter.Key);  // CPP: list_push(result, iterKey);
				}
			} else {
				return new IntrinsicResult(ErrorTypes.TypeError("list, string, or map", self));
			}
			return new IntrinsicResult(result);
		};

		// hasIndex(self, index)
		f = Intrinsic.Create("hasIndex");
		f.AddParam("self");
		f.AddParam("index");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return new IntrinsicResult(self);
			Value index = ctx.GetArg(1);
			if (is_list(self)) {
				if (!is_number(index)) return new IntrinsicResult(Value.zero);
				int i = (int)numeric_val(index);
				int count = list_count(self);
				return new IntrinsicResult(make_int((i >= -count && i < count) ? 1 : 0));
			} else if (is_string(self)) {
				if (!is_number(index)) return new IntrinsicResult(Value.zero);
				int i = (int)numeric_val(index);
				int slen = string_length(self);
				return new IntrinsicResult(make_int((i >= -slen && i < slen) ? 1 : 0));
			} else if (is_map(self)) {
				return new IntrinsicResult(make_int(map_has_key(self, index) ? 1 : 0));
			}
			return new IntrinsicResult(ErrorTypes.TypeError("list, string, or map", self));
		};

		// values(self)
		f = Intrinsic.Create("values");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return new IntrinsicResult(self);
			Value result = self;
			// CPP: Value iterVal;
			if (is_map(self)) {
				result = make_list(map_count(self));
				MapIterator iter = map_iterator(self);
				while (map_iterator_next(ref iter)) { // CPP: while (map_iterator_next(&iter, nullptr, &iterVal)) {
					list_push(result, iter.Val);      // CPP: list_push(result, iterVal);
				}
			} else if (is_string(self)) {
				int slen = string_length(self);
				result = make_list(slen);
				for (int i = 0; i < slen; i++) {
					list_push(result, string_substring(self, i, 1));
				}
			} else if (!is_list(self)) {
				// A list returns itself (its values are its elements); any other
				// non-container type is an error.
				return new IntrinsicResult(ErrorTypes.TypeError("list, string, or map", self));
			}
			return new IntrinsicResult(result);
		};

		// range(from=0, to=0, step=null)
		f = Intrinsic.Create("range");
		f.AddParam("from", Value.zero);
		f.AddParam("to", Value.zero);
		f.AddParam("step");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value vFrom = ctx.GetArg(0);
			if (is_error(vFrom)) return new IntrinsicResult(vFrom);
			Value vTo = ctx.GetArg(1);
			if (is_error(vTo)) return new IntrinsicResult(vTo);
			Value vStep = ctx.GetArg(2);
			if (is_error(vStep)) return new IntrinsicResult(vStep);
			double fromVal, toVal;
			{ Value e = RequireNumber(vFrom, out fromVal); if (!is_null(e)) return new IntrinsicResult(e); }
			{ Value e = RequireNumber(vTo, out toVal); if (!is_null(e)) return new IntrinsicResult(e); }
			double step;
			if (is_null(vStep)) {
				step = (toVal >= fromVal) ? 1 : -1;
			} else {
				Value e = RequireNumber(vStep, out step);
				if (!is_null(e)) return new IntrinsicResult(e);
			}
			if (step == 0) {
				return new IntrinsicResult(ErrorTypes.RuntimeError("range() step may not be zero"));
			}
			double rawCount = (toVal - fromVal) / step + 1.0;
			if (StringUtils.IsNaN(rawCount) || rawCount > MAX_COLLECTION_SIZE) {
				return new IntrinsicResult(ErrorTypes.RuntimeError(
					"range() result too large (exceeds maximum list size)"));
			}
			int count = (int)((toVal - fromVal) / step) + 1;
			if (count < 0) count = 0;
			// Build a computed list: element i is fromVal + step*i.  This is O(1)
			// to construct and materializes lazily only if the list is mutated.
			Value result = GCManager.NewComputedList(make_double(fromVal), make_double(step), count);
			return new IntrinsicResult(result);
		};

		// list
		//    Returns a map that represents the list datatype in
		//    MiniScript's core type system.  This can be used with `isa`
		//    to check whether a variable refers to a list.  You can also
		//    assign new methods here to make them available on all lists.
		f = Intrinsic.Create("list");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(ListType());
		};

		// string
		//    Returns a map that represents the string datatype in
		//    MiniScript's core type system.  This can be used with `isa`
		//    to check whether a variable refers to a string.  You can also
		//    assign new methods here to make them available on all strings.
		f = Intrinsic.Create("string");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(StringType());
		};

		// map
		//    Returns a map that represents the map datatype in
		//    MiniScript's core type system.  This can be used with `isa`
		//    to check whether a variable refers to a map.  You can also
		//    assign new methods here to make them available on all maps.
		f = Intrinsic.Create("map");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(MapType());
		};

		// number
		//    Returns a map that represents the number datatype in
		//    MiniScript's core type system.  This can be used with `isa`
		//    to check whether a variable contains a number.
		f = Intrinsic.Create("number");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(NumberType());
		};

		// funcRef
		//    Returns a map that represents the funcRef datatype in
		//    MiniScript's core type system.  This can be used with `isa`
		//    to check whether a variable refers to a function.
		//    (Remember to use @ to avoid invoking the function!)
		f = Intrinsic.Create("funcRef");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(FunctionType());
		};

		// error
		//    Returns a map that represents the error datatype in
		//    MiniScript's core type system.  This can be used with `isa`
		//    to check whether a variable refers to an error.
		f = Intrinsic.Create("error");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(ErrorType());
		};

		// time
		//    Returns the number of seconds (double) elapsed since the VM
		//    started running (i.e., since VM.Reset was called).
		f = Intrinsic.Create("time");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(make_double(ctx.vm.ElapsedTime()));
		};

		// wait(seconds=1)
		//    Pause execution of this script for some amount of time.
		// seconds (default 1.0): how many seconds to wait
		// See also: time, yield
		f = Intrinsic.Create("wait");
		f.AddParam("seconds", Value.one);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			double now = ctx.vm.ElapsedTime();
			Value vSeconds;
			if (partialResult.done) {
				// Fresh call: calculate end time and return as partial result
				vSeconds = ctx.GetArg(0);
				if (is_error(vSeconds)) return ctx.vm.RaiseUncaughtError(vSeconds);
				double interval = numeric_val(vSeconds);
				return new IntrinsicResult(make_double(now + interval), false);
			} else {
				// Continuation: check if we've waited long enough
				if (now > numeric_val(partialResult.result)) return IntrinsicResult.Null;
				return partialResult;
			}
		};

		// yield
		//    Pause execution of the script until the next "tick" of the
		//    host app.  In Mini Micro, for example, this waits until the
		//    next 60Hz frame.  If you're doing something in a tight loop,
		//    calling yield is polite to the host app or other scripts.
		f = Intrinsic.Create("yield");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			ctx.vm.yielding = true;
			return IntrinsicResult.Null;
		};

		// stackTrace
		//    Return the current call stack as a list of strings, innermost
		//    (most recent) call first.  Each string has the form
		//    "{file} line {lineNum}", e.g. "listUtil.ms line 42".
		//    The file name is "(current program)" for source loaded without
		//    a file name.
		f = Intrinsic.Create("stackTrace");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(ctx.vm.BuildStackTrace());
		};

		// _in
		//    Return the REPL input history list.  Each entry is a string
		//    containing one complete (possibly multi-line) REPL interaction.
		//    Only meaningful when running in REPL mode; otherwise returns null.
		f = Intrinsic.Create("_in");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(replInList);
		};

		// _out
		//    Return the REPL output history list.  Each entry corresponds to
		//    the implicit (expression-result) output of the matching _in entry,
		//    or null if that interaction produced no implicit output.
		//    Only meaningful when running in REPL mode; otherwise returns null.
		f = Intrinsic.Create("_out");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(replOutList);
		};

		// Note: `_` is no longer an intrinsic.  Instead, the REPL loop assigns
		// the global variable `_` to the most recent implicit result (see
		// App.cs / Interpreter.SetGlobalValue).  In non-REPL contexts `_` is
		// simply undefined unless the user assigns to it.

		// reset
		//    Clear all user-defined globals and reset the REPL history lists.
		//    Takes effect immediately: any code following reset in the same
		//    statement sees the cleared state.
		f = Intrinsic.Create("reset");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			replInList = make_list(0);
			replOutList = make_list(0);
			Interpreter interp = ctx.vm.GetInterpreter();
			if (interp != null) interp.ResetReplGlobals();
			GCManager.FullCollectGarbage();
			return IntrinsicResult.Null;
		};

		// bitAnd(i=0, j=0)
		f = Intrinsic.Create("bitAnd");
		f.AddParam("i", Value.zero);
		f.AddParam("j", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value ai = ctx.GetArg(0);
			if (is_error(ai)) return new IntrinsicResult(ai);
			Value aj = ctx.GetArg(1);
			if (is_error(aj)) return new IntrinsicResult(aj);
			Double vi, vj;
			{ Value e = RequireNumber(ai, out vi); if (!is_null(e)) return new IntrinsicResult(e); }
			{ Value e = RequireNumber(aj, out vj); if (!is_null(e)) return new IntrinsicResult(e); }
			UInt64 ui = (UInt64)Math.Abs(vi);
			UInt64 uj = (UInt64)Math.Abs(vj);
			Int32 si = vi < 0 ? 1 : 0;
			Int32 sj = vj < 0 ? 1 : 0;
			Int32 sign = si & sj;
			Double result = (Double)(ui & uj);
			return new IntrinsicResult(make_double(sign != 0 ? -result : result));
		};

		// bitOr(i=0, j=0)
		f = Intrinsic.Create("bitOr");
		f.AddParam("i", Value.zero);
		f.AddParam("j", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value ai = ctx.GetArg(0);
			if (is_error(ai)) return new IntrinsicResult(ai);
			Value aj = ctx.GetArg(1);
			if (is_error(aj)) return new IntrinsicResult(aj);
			Double vi, vj;
			{ Value e = RequireNumber(ai, out vi); if (!is_null(e)) return new IntrinsicResult(e); }
			{ Value e = RequireNumber(aj, out vj); if (!is_null(e)) return new IntrinsicResult(e); }
			UInt64 ui = (UInt64)Math.Abs(vi);
			UInt64 uj = (UInt64)Math.Abs(vj);
			Int32 si = vi < 0 ? 1 : 0;
			Int32 sj = vj < 0 ? 1 : 0;
			Int32 sign = si | sj;
			Double result = (Double)(ui | uj);
			return new IntrinsicResult(make_double(sign != 0 ? -result : result));
		};

		// bitXor(i=0, j=0)
		f = Intrinsic.Create("bitXor");
		f.AddParam("i", Value.zero);
		f.AddParam("j", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value ai = ctx.GetArg(0);
			if (is_error(ai)) return new IntrinsicResult(ai);
			Value aj = ctx.GetArg(1);
			if (is_error(aj)) return new IntrinsicResult(aj);
			Double vi, vj;
			{ Value e = RequireNumber(ai, out vi); if (!is_null(e)) return new IntrinsicResult(e); }
			{ Value e = RequireNumber(aj, out vj); if (!is_null(e)) return new IntrinsicResult(e); }
			UInt64 ui = (UInt64)Math.Abs(vi);
			UInt64 uj = (UInt64)Math.Abs(vj);
			Int32 si = vi < 0 ? 1 : 0;
			Int32 sj = vj < 0 ? 1 : 0;
			Int32 sign = si ^ sj;
			Double result = (Double)(ui ^ uj);
			return new IntrinsicResult(make_double(sign != 0 ? -result : result));
		};

		// hash(obj)
		f = Intrinsic.Create("hash");
		f.AddParam("obj");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			return new IntrinsicResult(make_int(value_hash(v)));
		};

		// refEquals(a, b)
		f = Intrinsic.Create("refEquals");
		f.AddParam("a");
		f.AddParam("b");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value a = ctx.GetArg(0);
			Value b = ctx.GetArg(1);
			return new IntrinsicResult(make_int(value_identical(a, b) ? 1 : 0));
		};

		// version
		f = Intrinsic.Create("version");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			if (is_null(_versionMap)) {
				_versionMap = make_map(6);
				map_set(_versionMap, "miniscript", "2.0");
				map_set(_versionMap, "buildDate", BuildDate());
				map_set(_versionMap, "platform", PlatformName());
				map_set(_versionMap, "host", hostVersion);
				map_set(_versionMap, "hostName", hostName);
				map_set(_versionMap, "hostInfo", hostInfo);
				freeze_value(_versionMap);
			}
			return new IntrinsicResult(_versionMap);
		};

		// gc.collect(full=false)  — underlying implementation for gc.collect
		_gcCollectIntr = Intrinsic.Create("");
		f = _gcCollectIntr;
		f.AddParam("full", Value.zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value vFull = ctx.GetArg(0);
			if (is_truthy(vFull)) {
				GCManager.FullCollectGarbage();
			} else {
				GCManager.CollectGarbage();
			}
			return IntrinsicResult.Null;
		};

		// gc.stats  — underlying implementation for gc.stats
		_gcStatsIntr = Intrinsic.Create("");
		f = _gcStatsIntr;
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value result = make_map(8);
			int bigStrings     = GCManager.BigStrings.LiveCount();
			int internedStrings = GCManager.InternedStrings.LiveCount();
			int lists          = GCManager.Lists.LiveCount();
			int maps           = GCManager.Maps.LiveCount();
			int errors         = GCManager.Errors.LiveCount();
			int functions      = GCManager.Functions.LiveCount();
			int total = bigStrings + internedStrings + lists + maps + errors + functions;
			map_set(result, "bigStrings",      make_int(bigStrings));
			map_set(result, "internedStrings", make_int(internedStrings));
			map_set(result, "lists",           make_int(lists));
			map_set(result, "maps",            make_int(maps));
			map_set(result, "errors",          make_int(errors));
			map_set(result, "functions",       make_int(functions));
			map_set(result, "total",           make_int(total));
			freeze_value(result);
			return new IntrinsicResult(result);
		};

		// gc — returns a map with GC utility functions: collect and stats.
		f = Intrinsic.Create("gc");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(GCMap());
		};

		// intrinsics — returns a map of all named intrinsic functions.
		f = Intrinsic.Create("intrinsics");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			return new IntrinsicResult(IntrinsicsMap());
		};

	}

	public static Value IntrinsicsMap() {
		if (is_null(_intrinsicsMap)) {
			int count = Intrinsic.Count();
			_intrinsicsMap = make_map(count);
			for (Int32 i = 0; i < count; i++) {
				Intrinsic intr = Intrinsic.GetByIndex(i);
				if (intr == null || intr.Name == null || intr.Name.Length == 0) continue;
				map_set(_intrinsicsMap, intr.Name, intr.GetFunc());
			}
			freeze_value(_intrinsicsMap);
		}
		return _intrinsicsMap;
	}
	private static Value _intrinsicsMap = Value.Null;

	public static Value GCMap() {
		if (is_null(_gcMap)) {
			_gcMap = make_map(2);
			if (_gcCollectIntr != null) map_set(_gcMap, "collect", _gcCollectIntr.GetFunc());
			if (_gcStatsIntr != null) map_set(_gcMap, "stats", _gcStatsIntr.GetFunc());
			freeze_value(_gcMap);
		}
		return _gcMap;
	}
	private static Value _gcMap = Value.Null;
	private static Intrinsic _gcCollectIntr = null;
	private static Intrinsic _gcStatsIntr = null;
	private static Value _versionMap = Value.Null;

	public delegate void VoidCallback(); // H: 
	private static List<VoidCallback> _invalidateCallbacks = null;

	public static void RegisterInvalidateCallback(VoidCallback callback) {
		if (_invalidateCallbacks == null) _invalidateCallbacks = new List<VoidCallback>();
		_invalidateCallbacks.Add(callback);
	}

	public static void InvalidateTypeMaps() {
		_listType = Value.Null;
		_stringType = Value.Null;
		_mapType = Value.Null;
		_numberType = Value.Null;
		_functionType = Value.Null;
		_errorType = Value.Null;
		_gcMap = Value.Null;
		_versionMap = Value.Null;
		_intrinsicsMap = Value.Null;
		if (_invalidateCallbacks != null) {
			for (Int32 i = 0; i < _invalidateCallbacks.Count; i++) _invalidateCallbacks[i]();
		}
		Intrinsic.ClearShortNames();
	}


}

}
