// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CoreIntrinsics.cs

#include "CoreIntrinsics.g.h"
#include "Intrinsic.g.h"
#include "value_list.h"
#include "value_string.h"
#include "value_map.h"
#include "IOHelper.g.h"
#include "StringUtils.g.h"
#include "VM.g.h"
#include "IntrinsicAPI.g.h"
#include "CS_Math.h"
#include "CS_value_util.h"
#include "Interpreter.g.h"
#include "PRNG.g.h"
#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <fstream>
#include <string>
#endif

namespace MiniScript {

String CoreIntrinsics::BuildDate() {
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
}
String CoreIntrinsics::PlatformName() {
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
}
String CoreIntrinsics::hostName = "";
String CoreIntrinsics::hostInfo = "";
String CoreIntrinsics::hostVersion = "";
Value CoreIntrinsics::RequireNumber(Value v,double* result) {
	if (is_number(v)) { *result = numeric_val(v); return val_null; }
	if (is_string(v)) {
		if (StringUtils::TryParseDouble(as_cstring(v), &*result)) return val_null;
		*result = 0.0;
		return ErrorTypes::FormatError(StringUtils::Format("'{0}' is not a valid number", as_cstring(v)));
	}
	*result = 0.0;
	return ErrorTypes::TypeError("number", v);
}
void CoreIntrinsics::AddIntrinsicToMap(Value map,String methodName) {
	Intrinsic intr = Intrinsic::GetByName(methodName);
	if (!IsNull(intr)) {
		map_set(map, methodName, intr.GetFunc());
	} else {
		IOHelper::Print(StringUtils::Format("Intrinsic not found: {0}", methodName));
	}
}
Value CoreIntrinsics::ListType() {
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
		Intrinsic::AddShortName(_listType, "list");
	}
	return _listType;
}
Value CoreIntrinsics::_listType = val_null;
Value CoreIntrinsics::StringType() {
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
		Intrinsic::AddShortName(_stringType, "string");
	}
	return _stringType;
}
Value CoreIntrinsics::_stringType = val_null;
Value CoreIntrinsics::MapType() {
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
		Intrinsic::AddShortName(_mapType, "map");
	}
	return _mapType;
}
Value CoreIntrinsics::_mapType = val_null;
Value CoreIntrinsics::NumberType() {
	if (is_null(_numberType)) {
		_numberType = make_map(4);
		Intrinsic::AddShortName(_numberType, "number");
	}
	return _numberType;
}
Value CoreIntrinsics::_numberType = val_null;
Value CoreIntrinsics::FunctionType() {
	if (is_null(_functionType)) {
		_functionType = make_map(4);
		Intrinsic::AddShortName(_functionType, "funcRef");
	}
	return _functionType;
}
Value CoreIntrinsics::_functionType = val_null;
Value CoreIntrinsics::ErrorType() {
	if (is_null(_errorType)) {
		_errorType = make_map(4);
		if (!IsNull(_errorErrIntr)) {
			map_set(_errorType, "err", _errorErrIntr.GetFunc());
		}
		Intrinsic::AddShortName(_errorType, "error");
	}
	return _errorType;
}
Value CoreIntrinsics::_errorType = val_null;
Intrinsic CoreIntrinsics::_errorErrIntr = nullptr;
Value CoreIntrinsics::_EOL = make_string("\n");
Value CoreIntrinsics::replInList = val_null;
Value CoreIntrinsics::replOutList = val_null;
void CoreIntrinsics::MarkRoots(object user_data) {
	GCManager::Mark(_listType);
	GCManager::Mark(_stringType);
	GCManager::Mark(_mapType);
	GCManager::Mark(_numberType);
	GCManager::Mark(_functionType);
	GCManager::Mark(_errorType);
	GCManager::Mark(_gcMap);
	GCManager::Mark(_versionMap);
	GCManager::Mark(replInList);
	GCManager::Mark(replOutList);
}
void CoreIntrinsics::Init() {
	GCManager::RegisterMarkCallback(CoreIntrinsics::MarkRoots, nullptr);

	Intrinsic f;

	// print(s="")
	f = Intrinsic::Create("print");
	f.AddParam("s", make_string(""));
	f.AddParam("delimiter", _EOL);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		String s = as_cstring(to_string(ctx.GetArg(0), ctx.vm));
		Value delimiterVal = ctx.GetArg(1);
		Interpreter interp = ctx.vm.GetInterpreter();
		if (!IsNull(interp) && !IsNull(interp.standardOutput())) {
			if (is_null(delimiterVal)) {
				interp.standardOutput()(s, Boolean(true));
			} else {
				String delimiter = as_cstring(delimiterVal);
				if (delimiter == "\n") {
					interp.standardOutput()(s, Boolean(true));
				} else {
					interp.standardOutput()(s + delimiter, Boolean(false));
				}
			}
		} else {
			IOHelper::Print(s);
		}
		return IntrinsicResult(val_null);
	});

	// input(prompt=null)
	f = Intrinsic::Create("input");
	f.AddParam("prompt");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		String prompt =  String::New("");
		if (!is_null(ctx.GetArg(0))) {
			prompt = StringUtils::Format("{0}", ctx.GetArg(0));
		}
		String result = IOHelper::Input(prompt);
		return IntrinsicResult(make_string(result));
	});

	// err(msg, inner=null) — global intrinsic: create a new error value.
	f = Intrinsic::Create("err");
	f.AddParam("msg");
	f.AddParam("inner");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value msg = ctx.GetArg(0);
		Value inner = ctx.GetArg(1);
		if (!is_string(msg)) msg = to_string(msg, ctx.vm);
		return IntrinsicResult(make_error(msg, inner, ctx.vm.BuildStackTrace(), val_null));
	});

	// err method on ErrorType: se.err(msg, inner=null) creates a new error
	// whose __isa is se.  Terminates if this would create an __isa cycle.
	_errorErrIntr = Intrinsic::Create("");
	f = _errorErrIntr;
	f.AddParam("self");
	f.AddParam("msg");
	f.AddParam("inner");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		Value msg = ctx.GetArg(1);
		Value inner = ctx.GetArg(2);
		if (!is_error(self)) {
			ctx.vm.RaiseRuntimeError("err method called on non-error value");
			return IntrinsicResult::Null;
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
				return IntrinsicResult::Null;
			}
			current = error_isa(current);
		}
		return IntrinsicResult(newErr);
	});

	// info(ref)
	f = Intrinsic::Create("info");
	f.AddParam("ref");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value arg = ctx.GetArg(0);
		Value result = make_map(8);
		Value parameters = val_null;
		Value pinfo = val_null;
		map_set(result, "type", value_type_name(arg));
		if (is_funcref(arg)) {
			FuncDef func = funcref_funcdef(arg);
			map_set(result, "name", func.Name());
			map_set(result, "note", func.Note());
			parameters = make_list(func.ParamNames().Count());
			for (int i=0; i < func.ParamNames().Count(); i++) {
				pinfo = make_map(2);
				map_set(pinfo, "name", func.ParamNames()[i]);
				map_set(pinfo, "default", func.ParamDefaults()[i]);
				list_push(parameters, pinfo);
			}
			map_set(result, "params", parameters);
			if (is_null(funcref_outer_vars(arg))) {
				map_set(result, "closure", val_zero);
			} else {
				map_set(result, "closure", val_one);
			}
		} else if (is_error(arg)) {
			map_set(result, "message", error_message(arg));
			map_set(result, "inner", error_inner(arg));
			map_set(result, "stack", error_stack(arg));
			map_set(result, "isa", error_isa(arg));
		}
		freeze_value(result);
		return IntrinsicResult(result);
	});

	// val(self=0)
	f = Intrinsic::Create("val");
	f.AddParam("self", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		if (is_number(v)) return IntrinsicResult(v);
		if (is_string(v)) return IntrinsicResult(to_number(v));
		return IntrinsicResult(val_null);
	});

	// str(x="")
	f = Intrinsic::Create("str");
	f.AddParam("x", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		if (is_null(v)) return IntrinsicResult(make_string(""));
		return IntrinsicResult(to_string(v, ctx.vm));
	});

	// upper(self)
	f = Intrinsic::Create("upper");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		return IntrinsicResult(string_upper(v));
	});

	// lower(self)
	f = Intrinsic::Create("lower");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		return IntrinsicResult(string_lower(v));
	});

	// char(codePoint=65)
	f = Intrinsic::Create("char");
	f.AddParam("codePoint", make_int(65));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		double cp;
		{ Value e = RequireNumber(v, &cp); if (!is_null(e)) return IntrinsicResult(e); }
		return IntrinsicResult(string_from_code_point((int)cp));
	});

	// code(self)
	f = Intrinsic::Create("code");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		if (!is_string(v)) return IntrinsicResult(ErrorTypes::TypeError("string", v));
		return IntrinsicResult(make_int(string_code_point(v)));
	});

	// len(x)
	f = Intrinsic::Create("len");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value container = ctx.GetArg(0);
		if (is_error(container)) return IntrinsicResult(container);
		Value result = val_null;
		if (is_list(container)) {
			result = make_int(list_count(container));
		} else if (is_string(container)) {
			result = make_int(string_length(container));
		} else if (is_map(container)) {
			result = make_int(map_count(container));
		}
		return IntrinsicResult(result);
	});

	// remove(self, index)
	f = Intrinsic::Create("remove");
	f.AddParam("self");
	f.AddParam("index");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value container = ctx.GetArg(0);
		if (is_error(container)) return ctx.vm.RaiseUncaughtError(container);
		int result = 0;
		if (is_list(container)) {
			result = list_remove(container, as_int(ctx.GetArg(1))) ? 1 : 0;
		} else if (is_map(container)) {
			result = map_remove(container, ctx.GetArg(1)) ? 1 : 0;
		} else {
			return IntrinsicResult(ErrorTypes::TypeError("list or map", container));
		}
		return IntrinsicResult(make_int(result));
	});

	// freeze(x)
	f = Intrinsic::Create("freeze");
	f.AddParam("x");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return ctx.vm.RaiseUncaughtError(v);
		freeze_value(v);
		return IntrinsicResult(val_null);
	});

	// isFrozen(x)
	f = Intrinsic::Create("isFrozen");
	f.AddParam("x");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		return IntrinsicResult(make_int(is_frozen(v)));
	});

	// frozenCopy(x)
	f = Intrinsic::Create("frozenCopy");
	f.AddParam("x");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		return IntrinsicResult(frozen_copy(v));
	});

	// abs(x=0)
	f = Intrinsic::Create("abs");
	f.AddParam("x", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		double x;
		{ Value e = RequireNumber(v, &x); if (!is_null(e)) return IntrinsicResult(e); }
		return IntrinsicResult(make_double(Math::Abs(x)));
	});

	// acos(x=0)
	f = Intrinsic::Create("acos");
	f.AddParam("x", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		double x;
		{ Value e = RequireNumber(v, &x); if (!is_null(e)) return IntrinsicResult(e); }
		return IntrinsicResult(make_double(Math::Acos(x)));
	});

	// asin(x=0)
	f = Intrinsic::Create("asin");
	f.AddParam("x", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		double x;
		{ Value e = RequireNumber(v, &x); if (!is_null(e)) return IntrinsicResult(e); }
		return IntrinsicResult(make_double(Math::Asin(x)));
	});

	// atan(y=0, x=1)
	f = Intrinsic::Create("atan");
	f.AddParam("y", val_zero);
	f.AddParam("x", val_one);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value vy = ctx.GetArg(0);
		if (is_error(vy)) return IntrinsicResult(vy);
		Value vx = ctx.GetArg(1);
		if (is_error(vx)) return IntrinsicResult(vx);
		double y, x;
		{ Value e = RequireNumber(vy, &y); if (!is_null(e)) return IntrinsicResult(e); }
		{ Value e = RequireNumber(vx, &x); if (!is_null(e)) return IntrinsicResult(e); }
		if (x == 1.0) return IntrinsicResult(make_double(Math::Atan(y)));
		return IntrinsicResult(make_double(Math::Atan2(y, x)));
	});

	// ceil(x=0)
	f = Intrinsic::Create("ceil");
	f.AddParam("x", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		double x;
		{ Value e = RequireNumber(v, &x); if (!is_null(e)) return IntrinsicResult(e); }
		return IntrinsicResult(make_double(Math::Ceiling(x)));
	});

	// cos(radians=0)
	f = Intrinsic::Create("cos");
	f.AddParam("radians", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		double x;
		{ Value e = RequireNumber(v, &x); if (!is_null(e)) return IntrinsicResult(e); }
		return IntrinsicResult(make_double(Math::Cos(x)));
	});

	// floor(x=0)
	f = Intrinsic::Create("floor");
	f.AddParam("x", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		double x;
		{ Value e = RequireNumber(v, &x); if (!is_null(e)) return IntrinsicResult(e); }
		return IntrinsicResult(make_double(Math::Floor(x)));
	});

	// log(x=0, base=10)
	f = Intrinsic::Create("log");
	f.AddParam("x", val_zero);
	f.AddParam("base", make_int(10));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value vx = ctx.GetArg(0);
		if (is_error(vx)) return IntrinsicResult(vx);
		Value vb = ctx.GetArg(1);
		if (is_error(vb)) return IntrinsicResult(vb);
		double x, b;
		{ Value e = RequireNumber(vx, &x); if (!is_null(e)) return IntrinsicResult(e); }
		{ Value e = RequireNumber(vb, &b); if (!is_null(e)) return IntrinsicResult(e); }
		double result;
		if (Math::Abs(b - 2.718282) < 0.000001) result = Math::Log(x);
		else result = Math::Log(x) / Math::Log(b);
		return IntrinsicResult(make_double(result));
	});

	// pi
	f = Intrinsic::Create("pi");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(Math::PI));
	});

	// round(x=0, decimalPlaces=0)
	f = Intrinsic::Create("round");
	f.AddParam("x", val_zero);
	f.AddParam("decimalPlaces", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value vx = ctx.GetArg(0);
		if (is_error(vx)) return IntrinsicResult(vx);
		Value vd = ctx.GetArg(1);
		if (is_error(vd)) return IntrinsicResult(vd);
		double num, decimals;
		{ Value e = RequireNumber(vx, &num); if (!is_null(e)) return IntrinsicResult(e); }
		{ Value e = RequireNumber(vd, &decimals); if (!is_null(e)) return IntrinsicResult(e); }
		int decimalPlaces = (int)decimals;
		if (decimalPlaces >= 0) {
			if (decimalPlaces > 15) decimalPlaces = 15;
			num = Math::Round(num, decimalPlaces);
		} else {
			double pow10 = Math::Pow(10, -decimalPlaces);
			num /= pow10;
			num = Math::Round(num);
			num *= pow10;
		}
		return IntrinsicResult(make_double(num));
	});

	// rnd(seed)
	f = Intrinsic::Create("rnd");
	f.AddParam("seed");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return ctx.vm.RaiseUncaughtError(v);
		// If a seed is supplied, reseed the generator before drawing.  null
		// means "no seed"; a number (or numeric string) reseeds; any other
		// type is a parameter error.
		if (!is_null(v)) {
			double seed;
			{ Value e = RequireNumber(v, &seed); if (!is_null(e)) return IntrinsicResult(e); }
			PRNG::Seed((UInt64)(Int64)seed);
		}
		return IntrinsicResult(make_double(PRNG::Next()));
	});

	// sign(x=0)
	f = Intrinsic::Create("sign");
	f.AddParam("x", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		double x;
		{ Value e = RequireNumber(v, &x); if (!is_null(e)) return IntrinsicResult(e); }
		return IntrinsicResult(make_int(Math::Sign(x)));
	});

	// sin(radians=0)
	f = Intrinsic::Create("sin");
	f.AddParam("radians", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		double x;
		{ Value e = RequireNumber(v, &x); if (!is_null(e)) return IntrinsicResult(e); }
		return IntrinsicResult(make_double(Math::Sin(x)));
	});

	// sqrt(x=0)
	f = Intrinsic::Create("sqrt");
	f.AddParam("x", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		double x;
		{ Value e = RequireNumber(v, &x); if (!is_null(e)) return IntrinsicResult(e); }
		return IntrinsicResult(make_double(Math::Sqrt(x)));
	});

	// tan(radians=0)
	f = Intrinsic::Create("tan");
	f.AddParam("radians", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (is_error(v)) return IntrinsicResult(v);
		double x;
		{ Value e = RequireNumber(v, &x); if (!is_null(e)) return IntrinsicResult(e); }
		return IntrinsicResult(make_double(Math::Tan(x)));
	});
	// push(self, value)
	f = Intrinsic::Create("push");
	f.AddParam("self");
	f.AddParam("value");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
		Value value = ctx.GetArg(1);
		if (is_list(self)) {
			list_push(self, value);
			return IntrinsicResult(self);
		} else if (is_map(self)) {
			map_set(self, value, val_one);
			return IntrinsicResult(self);
		}
		return IntrinsicResult(ErrorTypes::TypeError("list or map", self));
	});

	// pop(self)
	f = Intrinsic::Create("pop");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
		Value result = val_null;
		if (is_list(self)) {
			result = list_pop(self);
		} else if (is_map(self)) {
			if (map_count(self) == 0) return IntrinsicResult(val_null);
			MapIterator iter = map_iterator(self);
			if (map_iterator_next(&iter, &result, nullptr)) {
				// remove key that was found
				map_remove(self, result);
			}
		} else {
			return IntrinsicResult(ErrorTypes::TypeError("list or map", self));
		}
		return IntrinsicResult(result);
	});

	// pull(self)
	f = Intrinsic::Create("pull");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
		Value result = val_null;
		if (is_list(self)) {
			result = list_pull(self);
		} else if (is_map(self)) {
			if (map_count(self) == 0) return IntrinsicResult(val_null);
			MapIterator iter = map_iterator(self);
			if (map_iterator_next(&iter, &result, nullptr)) {
				// remove key that was found
				map_remove(self, result);
			}
		} else {
			return IntrinsicResult(ErrorTypes::TypeError("list or map", self));
		}
		return IntrinsicResult(result);
	});

	// insert(self, index, value)
	f = Intrinsic::Create("insert");
	f.AddParam("self");
	f.AddParam("index");
	f.AddParam("value");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
		int index = (int)numeric_val(ctx.GetArg(1));
		Value value = ctx.GetArg(2);
		if (is_list(self)) {
			list_insert(self, index, value);
			return IntrinsicResult(self);
		} else if (is_string(self)) {
			return IntrinsicResult(string_insert(self, index, value, ctx.vm));
		}
		return IntrinsicResult(ErrorTypes::TypeError("list or string", self));
	});

	// indexOf(self, value, after=null)
	f = Intrinsic::Create("indexOf");
	f.AddParam("self");
	f.AddParam("value");
	f.AddParam("after");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return IntrinsicResult(self);
		Value value = ctx.GetArg(1);
		Value after = ctx.GetArg(2);
		Value result = val_null;
		Value iterKey, iterVal;
		if (is_list(self)) {
			int afterIdx = -1;
			if (!is_null(after)) {
				afterIdx = (int)numeric_val(after);
				if (afterIdx < -1) afterIdx += list_count(self);
			}
			int idx = list_indexOf(self, value, afterIdx);
			if (idx >= 0) result = make_int(idx);
		} else if (is_string(self)) {
			if (!is_string(value)) return IntrinsicResult(val_null);
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
			while (map_iterator_next(&iter, &iterKey, &iterVal)) {
				if (!pastAfter) {
					if (value_equal(iterKey, after)) {
						pastAfter = Boolean(true);
					}
					continue;
				}
				if (value_equal(iterVal, value)) {
					result = iterKey;
					break;
				}
			}
		} else {
			return IntrinsicResult(ErrorTypes::TypeError("list, string, or map", self));
		}
		return IntrinsicResult(result);
	});

	// sort(self, byKey=null, ascending=1)
	f = Intrinsic::Create("sort");
	f.AddParam("self");
	f.AddParam("byKey");
	f.AddParam("ascending", val_one);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
		Value byKey = ctx.GetArg(1);
		bool ascending = is_truthy(ctx.GetArg(2));
		if (!is_list(self)) return IntrinsicResult(ErrorTypes::TypeError("list", self));
		if (list_count(self) < 2) return IntrinsicResult(self);
		if (is_null(byKey)) {
			list_sort(self, ascending);
		} else {
			list_sort_by_key(self, byKey, ascending);
		}
		return IntrinsicResult(self);
	});

	// shuffle(self)
	f = Intrinsic::Create("shuffle");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
		Value temp;
		Value iterKey, iterVal;
		if (is_list(self)) {
			if (is_frozen(self)) { ctx.vm.RaiseRuntimeError("Attempt to modify a frozen list"); return IntrinsicResult(val_null); }
			int count = list_count(self);
			for (int i = count - 1; i > 0; i--) {
				int j = (int)(PRNG::Next() * (i + 1));
				temp = list_get(self, i);
				list_set(self, i, list_get(self, j));
				list_set(self, j, temp);
			}
		} else if (is_map(self)) {
			if (is_frozen(self)) { ctx.vm.RaiseRuntimeError("Attempt to modify a frozen map"); return IntrinsicResult(val_null); }
			// Collect keys and values
			int count = map_count(self);
			List<Value> keys =  List<Value>::New(count);
			List<Value> vals =  List<Value>::New(count);
			MapIterator iter = map_iterator(self);
			while (map_iterator_next(&iter, &iterKey, &iterVal)) {
				vals.Add(iterKey);
				vals.Add(iterVal);
			}
			// Fisher-Yates shuffle on values
			for (int i = count - 1; i > 0; i--) {
				int j = (int)(PRNG::Next() * (i + 1));
				temp = vals[i];
				vals[i] = vals[j];
				vals[j] = temp;
			}
			for (int i = 0; i < count; i++) {
				map_set(self, keys[i], vals[i]);
			}
		} else {
			return IntrinsicResult(ErrorTypes::TypeError("list or map", self));
		}
		return IntrinsicResult(val_null);
	});

	// join(self, delimiter=" ")
	f = Intrinsic::Create("join");
	f.AddParam("self");
	f.AddParam("delimiter", make_string(" "));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return IntrinsicResult(self);
		if (!is_list(self)) return IntrinsicResult(ErrorTypes::TypeError("list", self));
		Value delim = ctx.GetArg(1);
		String delimStr = is_null(delim) ? " " : to_String(delim);
		int count = list_count(self);
		List<String> parts =  List<String>::New(count);
		for (int i = 0; i < count; i++) {
			parts.Add(to_String(list_get(self, i)));
		}
		return IntrinsicResult(make_string(String::Join(delimStr, parts)));
	});

	// split(self, delimiter=" ", maxCount=-1)
	f = Intrinsic::Create("split");
	f.AddParam("self");
	f.AddParam("delimiter", make_string(" "));
	f.AddParam("maxCount", make_int(-1));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return IntrinsicResult(self);
		if (!is_string(self)) return IntrinsicResult(ErrorTypes::TypeError("string", self));
		Value delim = ctx.GetArg(1);
		int maxCount = (int)numeric_val(ctx.GetArg(2));
		return IntrinsicResult(string_split_max(self, delim, maxCount));
	});

	// replace(self, oldval, newval, maxCount=null)
	f = Intrinsic::Create("replace");
	f.AddParam("self");
	f.AddParam("oldval");
	f.AddParam("newval");
	f.AddParam("maxCount");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
		Value oldVal = ctx.GetArg(1);
		Value newVal = ctx.GetArg(2);
		Value maxCountVal = ctx.GetArg(3);
		Value iterKey, iterVal;
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
			return IntrinsicResult(self);
		} else if (is_map(self)) {
			// Collect keys whose values match
			List<Value> keysToChange =  List<Value>::New();
			MapIterator iter = map_iterator(self);
			while (map_iterator_next(&iter, &iterKey, &iterVal)) {
				if (value_equal(iterVal, oldVal)) {
					keysToChange.Add(iterKey);
					if (maxCount > 0 && keysToChange.Count() >= maxCount) break;
				}
			}
			for (int i = 0; i < keysToChange.Count(); i++) {
				map_set(self, keysToChange[i], newVal);
			}
			return IntrinsicResult(self);
		} else if (is_string(self)) {
			return IntrinsicResult(string_replace_max(self, oldVal, newVal, maxCount));
		}
		return IntrinsicResult(ErrorTypes::TypeError("list, map, or string", self));
	});

	// sum(self)
	f = Intrinsic::Create("sum");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return IntrinsicResult(self);
		Value iterVal;
		double total = 0;
		if (is_list(self)) {
			int count = list_count(self);
			for (int i = 0; i < count; i++) {
				total += numeric_val(list_get(self, i));
			}
		} else if (is_map(self)) {
			MapIterator iter = map_iterator(self);
			while (map_iterator_next(&iter, nullptr, &iterVal)) {
				total += numeric_val(iterVal);
			}
		} else {
			return IntrinsicResult(val_zero);
		}
		if (total == (int)total && total >= Int32MinValue && total <= Int32MaxValue) {
			return IntrinsicResult(make_int((int)total));
		}
		return IntrinsicResult(make_double(total));
	});

	// slice(seq, from=0, to=null)
	f = Intrinsic::Create("slice");
	f.AddParam("seq");
	f.AddParam("from", val_zero);
	f.AddParam("to");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value seq = ctx.GetArg(0);
		if (is_error(seq)) return IntrinsicResult(seq);
		int fromIdx = (int)numeric_val(ctx.GetArg(1));
		if (is_list(seq)) {
			int count = list_count(seq);
			int toIdx = is_null(ctx.GetArg(2)) ? count : (int)numeric_val(ctx.GetArg(2));
			return IntrinsicResult(list_slice(seq, fromIdx, toIdx));
		} else if (is_string(seq)) {
			int slen = string_length(seq);
			int toIdx = is_null(ctx.GetArg(2)) ? slen : (int)numeric_val(ctx.GetArg(2));
			return IntrinsicResult(string_slice(seq, fromIdx, toIdx));
		}
		return IntrinsicResult(ErrorTypes::TypeError("list or string", seq));
	});

	// indexes(self)
	f = Intrinsic::Create("indexes");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return IntrinsicResult(self);
		Value result = val_null;
		Value iterKey;
		if (is_list(self)) {
			int count = list_count(self);
			result = make_list(count);
			for (int i = 0; i < count; i++) {
				list_push(result, make_int(i));
			}
			return IntrinsicResult(result);
		} else if (is_string(self)) {
			int slen = string_length(self);
			result = make_list(slen);
			for (int i = 0; i < slen; i++) {
				list_push(result, make_int(i));
			}
			return IntrinsicResult(result);
		} else if (is_map(self)) {
			result = make_list(map_count(self));
			MapIterator iter = map_iterator(self);
			while (map_iterator_next(&iter, &iterKey, nullptr)) {
				list_push(result, iterKey);
			}
		} else {
			return IntrinsicResult(ErrorTypes::TypeError("list, string, or map", self));
		}
		return IntrinsicResult(result);
	});

	// hasIndex(self, index)
	f = Intrinsic::Create("hasIndex");
	f.AddParam("self");
	f.AddParam("index");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return IntrinsicResult(self);
		Value index = ctx.GetArg(1);
		if (is_list(self)) {
			if (!is_number(index)) return IntrinsicResult(val_zero);
			int i = (int)numeric_val(index);
			int count = list_count(self);
			return IntrinsicResult(make_int((i >= -count && i < count) ? 1 : 0));
		} else if (is_string(self)) {
			if (!is_number(index)) return IntrinsicResult(val_zero);
			int i = (int)numeric_val(index);
			int slen = string_length(self);
			return IntrinsicResult(make_int((i >= -slen && i < slen) ? 1 : 0));
		} else if (is_map(self)) {
			return IntrinsicResult(make_int(map_has_key(self, index) ? 1 : 0));
		}
		return IntrinsicResult(ErrorTypes::TypeError("list, string, or map", self));
	});

	// values(self)
	f = Intrinsic::Create("values");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (is_error(self)) return IntrinsicResult(self);
		Value result = self;
		Value iterVal;
		if (is_map(self)) {
			result = make_list(map_count(self));
			MapIterator iter = map_iterator(self);
			while (map_iterator_next(&iter, nullptr, &iterVal)) {
				list_push(result, iterVal);
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
			return IntrinsicResult(ErrorTypes::TypeError("list, string, or map", self));
		}
		return IntrinsicResult(result);
	});

	// range(from=0, to=0, step=null)
	f = Intrinsic::Create("range");
	f.AddParam("from", val_zero);
	f.AddParam("to", val_zero);
	f.AddParam("step");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value vFrom = ctx.GetArg(0);
		if (is_error(vFrom)) return IntrinsicResult(vFrom);
		Value vTo = ctx.GetArg(1);
		if (is_error(vTo)) return IntrinsicResult(vTo);
		Value vStep = ctx.GetArg(2);
		if (is_error(vStep)) return IntrinsicResult(vStep);
		double fromVal, toVal;
		{ Value e = RequireNumber(vFrom, &fromVal); if (!is_null(e)) return IntrinsicResult(e); }
		{ Value e = RequireNumber(vTo, &toVal); if (!is_null(e)) return IntrinsicResult(e); }
		double step;
		if (is_null(vStep)) {
			step = (toVal >= fromVal) ? 1 : -1;
		} else {
			Value e = RequireNumber(vStep, &step);
			if (!is_null(e)) return IntrinsicResult(e);
		}
		if (step == 0) {
			return IntrinsicResult(ErrorTypes::RuntimeError("range() step may not be zero"));
		}
		int count = (int)((toVal - fromVal) / step) + 1;
		if (count < 0) count = 0;
		if (count > 1000000) count = 1000000;  // safety limit
		Value result = make_list(count);
		double v = fromVal;
		if (step > 0) {
			while (v <= toVal) {
				if (v == (int)v) list_push(result, make_int((int)v));
				else list_push(result, make_double(v));
				v += step;
			}
		} else {
			while (v >= toVal) {
				if (v == (int)v) list_push(result, make_int((int)v));
				else list_push(result, make_double(v));
				v += step;
			}
		}
		return IntrinsicResult(result);
	});

	// list
	//    Returns a map that represents the list datatype in
	//    MiniScript's core type system.  This can be used with `isa`
	//    to check whether a variable refers to a list.  You can also
	//    assign new methods here to make them available on all lists.
	f = Intrinsic::Create("list");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(ListType());
	});

	// string
	//    Returns a map that represents the string datatype in
	//    MiniScript's core type system.  This can be used with `isa`
	//    to check whether a variable refers to a string.  You can also
	//    assign new methods here to make them available on all strings.
	f = Intrinsic::Create("string");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(StringType());
	});

	// map
	//    Returns a map that represents the map datatype in
	//    MiniScript's core type system.  This can be used with `isa`
	//    to check whether a variable refers to a map.  You can also
	//    assign new methods here to make them available on all maps.
	f = Intrinsic::Create("map");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(MapType());
	});

	// number
	//    Returns a map that represents the number datatype in
	//    MiniScript's core type system.  This can be used with `isa`
	//    to check whether a variable contains a number.
	f = Intrinsic::Create("number");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(NumberType());
	});

	// funcRef
	//    Returns a map that represents the funcRef datatype in
	//    MiniScript's core type system.  This can be used with `isa`
	//    to check whether a variable refers to a function.
	//    (Remember to use @ to avoid invoking the function!)
	f = Intrinsic::Create("funcRef");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(FunctionType());
	});

	// error
	//    Returns a map that represents the error datatype in
	//    MiniScript's core type system.  This can be used with `isa`
	//    to check whether a variable refers to an error.
	f = Intrinsic::Create("error");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(ErrorType());
	});

	// time
	//    Returns the number of seconds (double) elapsed since the VM
	//    started running (i.e., since VM.Reset was called).
	f = Intrinsic::Create("time");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(ctx.vm.ElapsedTime()));
	});

	// wait(seconds=1)
	//    Pause execution of this script for some amount of time.
	// seconds (default 1.0): how many seconds to wait
	// See also: time, yield
	f = Intrinsic::Create("wait");
	f.AddParam("seconds", val_one);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		double now = ctx.vm.ElapsedTime();
		Value vSeconds;
		if (partialResult.done) {
			// Fresh call: calculate end time and return as partial result
			vSeconds = ctx.GetArg(0);
			if (is_error(vSeconds)) return ctx.vm.RaiseUncaughtError(vSeconds);
			double interval = numeric_val(vSeconds);
			return IntrinsicResult(make_double(now + interval), Boolean(false));
		} else {
			// Continuation: check if we've waited long enough
			if (now > numeric_val(partialResult.result)) return IntrinsicResult::Null;
			return partialResult;
		}
	});

	// yield
	//    Pause execution of the script until the next "tick" of the
	//    host app.  In Mini Micro, for example, this waits until the
	//    next 60Hz frame.  If you're doing something in a tight loop,
	//    calling yield is polite to the host app or other scripts.
	f = Intrinsic::Create("yield");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		ctx.vm.yielding = Boolean(true);
		return IntrinsicResult::Null;
	});

	// stackTrace
	//    Return the current call stack as a list of strings, innermost
	//    (most recent) call first.  Each string has the form
	//    "{file} line {lineNum}", e.g. "listUtil.ms line 42".
	//    The file name is "(current program)" for source loaded without
	//    a file name.
	f = Intrinsic::Create("stackTrace");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(ctx.vm.BuildStackTrace());
	});

	// _in
	//    Return the REPL input history list.  Each entry is a string
	//    containing one complete (possibly multi-line) REPL interaction.
	//    Only meaningful when running in REPL mode; otherwise returns null.
	f = Intrinsic::Create("_in");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(replInList);
	});

	// _out
	//    Return the REPL output history list.  Each entry corresponds to
	//    the implicit (expression-result) output of the matching _in entry,
	//    or null if that interaction produced no implicit output.
	//    Only meaningful when running in REPL mode; otherwise returns null.
	f = Intrinsic::Create("_out");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(replOutList);
	});

	// Note: `_` is no longer an intrinsic.  Instead, the REPL loop assigns
	// the global variable `_` to the most recent implicit result (see
	// App.cs / Interpreter.SetGlobalValue).  In non-REPL contexts `_` is
	// simply undefined unless the user assigns to it.

	// reset
	//    Clear all user-defined globals and reset the REPL history lists.
	//    Takes effect immediately: any code following reset in the same
	//    statement sees the cleared state.
	f = Intrinsic::Create("reset");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		replInList = make_list(0);
		replOutList = make_list(0);
		Interpreter interp = ctx.vm.GetInterpreter();
		if (!IsNull(interp)) interp.ResetReplGlobals();
		GCManager::FullCollectGarbage();
		return IntrinsicResult::Null;
	});

	// bitAnd(i=0, j=0)
	f = Intrinsic::Create("bitAnd");
	f.AddParam("i", val_zero);
	f.AddParam("j", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value ai = ctx.GetArg(0);
		if (is_error(ai)) return IntrinsicResult(ai);
		Value aj = ctx.GetArg(1);
		if (is_error(aj)) return IntrinsicResult(aj);
		Double vi, vj;
		{ Value e = RequireNumber(ai, &vi); if (!is_null(e)) return IntrinsicResult(e); }
		{ Value e = RequireNumber(aj, &vj); if (!is_null(e)) return IntrinsicResult(e); }
		UInt64 ui = (UInt64)Math::Abs(vi);
		UInt64 uj = (UInt64)Math::Abs(vj);
		Int32 si = vi < 0 ? 1 : 0;
		Int32 sj = vj < 0 ? 1 : 0;
		Int32 sign = si & sj;
		Double result = (Double)(ui & uj);
		return IntrinsicResult(make_double(sign != 0 ? -result : result));
	});

	// bitOr(i=0, j=0)
	f = Intrinsic::Create("bitOr");
	f.AddParam("i", val_zero);
	f.AddParam("j", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value ai = ctx.GetArg(0);
		if (is_error(ai)) return IntrinsicResult(ai);
		Value aj = ctx.GetArg(1);
		if (is_error(aj)) return IntrinsicResult(aj);
		Double vi, vj;
		{ Value e = RequireNumber(ai, &vi); if (!is_null(e)) return IntrinsicResult(e); }
		{ Value e = RequireNumber(aj, &vj); if (!is_null(e)) return IntrinsicResult(e); }
		UInt64 ui = (UInt64)Math::Abs(vi);
		UInt64 uj = (UInt64)Math::Abs(vj);
		Int32 si = vi < 0 ? 1 : 0;
		Int32 sj = vj < 0 ? 1 : 0;
		Int32 sign = si | sj;
		Double result = (Double)(ui | uj);
		return IntrinsicResult(make_double(sign != 0 ? -result : result));
	});

	// bitXor(i=0, j=0)
	f = Intrinsic::Create("bitXor");
	f.AddParam("i", val_zero);
	f.AddParam("j", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value ai = ctx.GetArg(0);
		if (is_error(ai)) return IntrinsicResult(ai);
		Value aj = ctx.GetArg(1);
		if (is_error(aj)) return IntrinsicResult(aj);
		Double vi, vj;
		{ Value e = RequireNumber(ai, &vi); if (!is_null(e)) return IntrinsicResult(e); }
		{ Value e = RequireNumber(aj, &vj); if (!is_null(e)) return IntrinsicResult(e); }
		UInt64 ui = (UInt64)Math::Abs(vi);
		UInt64 uj = (UInt64)Math::Abs(vj);
		Int32 si = vi < 0 ? 1 : 0;
		Int32 sj = vj < 0 ? 1 : 0;
		Int32 sign = si ^ sj;
		Double result = (Double)(ui ^ uj);
		return IntrinsicResult(make_double(sign != 0 ? -result : result));
	});

	// hash(obj)
	f = Intrinsic::Create("hash");
	f.AddParam("obj");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		return IntrinsicResult(make_int(value_hash(v)));
	});

	// refEquals(a, b)
	f = Intrinsic::Create("refEquals");
	f.AddParam("a");
	f.AddParam("b");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value a = ctx.GetArg(0);
		Value b = ctx.GetArg(1);
		return IntrinsicResult(make_int(value_identical(a, b) ? 1 : 0));
	});

	// version
	f = Intrinsic::Create("version");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
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
		return IntrinsicResult(_versionMap);
	});

	// gc.collect(full=false)  — underlying implementation for gc.collect
	_gcCollectIntr = Intrinsic::Create("");
	f = _gcCollectIntr;
	f.AddParam("full", val_zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value vFull = ctx.GetArg(0);
		if (is_truthy(vFull)) {
			GCManager::FullCollectGarbage();
		} else {
			GCManager::CollectGarbage();
		}
		return IntrinsicResult::Null;
	});

	// gc.stats  — underlying implementation for gc.stats
	_gcStatsIntr = Intrinsic::Create("");
	f = _gcStatsIntr;
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value result = make_map(8);
		int bigStrings     = GCManager::BigStrings.LiveCount();
		int internedStrings = GCManager::InternedStrings.LiveCount();
		int lists          = GCManager::Lists.LiveCount();
		int maps           = GCManager::Maps.LiveCount();
		int errors         = GCManager::Errors.LiveCount();
		int functions      = GCManager::Functions.LiveCount();
		int total = bigStrings + internedStrings + lists + maps + errors + functions;
		map_set(result, "bigStrings",      make_int(bigStrings));
		map_set(result, "internedStrings", make_int(internedStrings));
		map_set(result, "lists",           make_int(lists));
		map_set(result, "maps",            make_int(maps));
		map_set(result, "errors",          make_int(errors));
		map_set(result, "functions",       make_int(functions));
		map_set(result, "total",           make_int(total));
		freeze_value(result);
		return IntrinsicResult(result);
	});

	// gc — returns a map with GC utility functions: collect and stats.
	f = Intrinsic::Create("gc");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(GCMap());
	});

	// intrinsics — returns a map of all named intrinsic functions.
	f = Intrinsic::Create("intrinsics");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(IntrinsicsMap());
	});

}
Value CoreIntrinsics::IntrinsicsMap() {
	if (is_null(_intrinsicsMap)) {
		int count = Intrinsic::Count();
		_intrinsicsMap = make_map(count);
		for (Int32 i = 0; i < count; i++) {
			Intrinsic intr = Intrinsic::GetByIndex(i);
			if (IsNull(intr) || IsNull(intr.Name()) || intr.Name().Length() == 0) continue;
			map_set(_intrinsicsMap, intr.Name(), intr.GetFunc());
		}
		freeze_value(_intrinsicsMap);
	}
	return _intrinsicsMap;
}
Value CoreIntrinsics::_intrinsicsMap = val_null;
Value CoreIntrinsics::GCMap() {
	if (is_null(_gcMap)) {
		_gcMap = make_map(2);
		if (!IsNull(_gcCollectIntr)) map_set(_gcMap, "collect", _gcCollectIntr.GetFunc());
		if (!IsNull(_gcStatsIntr)) map_set(_gcMap, "stats", _gcStatsIntr.GetFunc());
		freeze_value(_gcMap);
	}
	return _gcMap;
}
Value CoreIntrinsics::_gcMap = val_null;
Intrinsic CoreIntrinsics::_gcCollectIntr = nullptr;
Intrinsic CoreIntrinsics::_gcStatsIntr = nullptr;
Value CoreIntrinsics::_versionMap = val_null;
List<VoidCallback> CoreIntrinsics::_invalidateCallbacks = nullptr;
void CoreIntrinsics::RegisterInvalidateCallback(VoidCallback callback) {
	if (IsNull(_invalidateCallbacks)) _invalidateCallbacks =  List<VoidCallback>::New();
	_invalidateCallbacks.Add(callback);
}
void CoreIntrinsics::InvalidateTypeMaps() {
	_listType = val_null;
	_stringType = val_null;
	_mapType = val_null;
	_numberType = val_null;
	_functionType = val_null;
	_errorType = val_null;
	_gcMap = val_null;
	_versionMap = val_null;
	_intrinsicsMap = val_null;
	if (!IsNull(_invalidateCallbacks)) {
		for (Int32 i = 0; i < _invalidateCallbacks.Count(); i++) _invalidateCallbacks[i]();
	}
	Intrinsic::ClearShortNames();
}

} // end of namespace MiniScript
