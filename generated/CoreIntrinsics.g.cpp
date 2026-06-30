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
	if (v.IsNumber()) { *result = Value::numeric_val(v); return Value::Null; }
	if (v.IsString()) {
		if (StringUtils::TryParseDouble(Value::as_cstring(v), &*result)) return Value::Null;
		*result = 0.0;
		return ErrorTypes::FormatError(StringUtils::Format("'{0}' is not a valid number", Value::as_cstring(v)));
	}
	*result = 0.0;
	return ErrorTypes::TypeError("number", v);
}
void CoreIntrinsics::AddIntrinsicToMap(Value map,String methodName) {
	Intrinsic intr = Intrinsic::GetByName(methodName);
	if (!IsNull(intr)) {
		Value::map_set(map, methodName, intr.GetFunc());
	} else {
		IOHelper::Print(StringUtils::Format("Intrinsic not found: {0}", methodName));
	}
}
Value CoreIntrinsics::ListType() {
	if (_listType.IsNull()) {
		_listType = Value::make_map(16);
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
Value CoreIntrinsics::_listType = Value::Null;
Value CoreIntrinsics::StringType() {
	if (_stringType.IsNull()) {
		_stringType = Value::make_map(16);
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
Value CoreIntrinsics::_stringType = Value::Null;
Value CoreIntrinsics::MapType() {
	if (_mapType.IsNull()) {
		_mapType = Value::make_map(16);
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
Value CoreIntrinsics::_mapType = Value::Null;
Value CoreIntrinsics::NumberType() {
	if (_numberType.IsNull()) {
		_numberType = Value::make_map(4);
		Intrinsic::AddShortName(_numberType, "number");
	}
	return _numberType;
}
Value CoreIntrinsics::_numberType = Value::Null;
Value CoreIntrinsics::FunctionType() {
	if (_functionType.IsNull()) {
		_functionType = Value::make_map(4);
		Intrinsic::AddShortName(_functionType, "funcRef");
	}
	return _functionType;
}
Value CoreIntrinsics::_functionType = Value::Null;
Value CoreIntrinsics::ErrorType() {
	if (_errorType.IsNull()) {
		_errorType = Value::make_map(4);
		if (!IsNull(_errorErrIntr)) {
			Value::map_set(_errorType, "err", _errorErrIntr.GetFunc());
		}
		Intrinsic::AddShortName(_errorType, "error");
	}
	return _errorType;
}
Value CoreIntrinsics::_errorType = Value::Null;
Intrinsic CoreIntrinsics::_errorErrIntr = nullptr;
Value CoreIntrinsics::_EOL = Value::make_string("\n");
Value CoreIntrinsics::replInList = Value::Null;
Value CoreIntrinsics::replOutList = Value::Null;
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
	f.AddParam("s", Value::make_string(""));
	f.AddParam("delimiter", _EOL);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		String s = Value::as_cstring(Value::to_string(ctx.GetArg(0), ctx.vm));
		Value delimiterVal = ctx.GetArg(1);
		Interpreter interp = ctx.vm.GetInterpreter();
		if (!IsNull(interp) && !IsNull(interp.standardOutput())) {
			if (delimiterVal.IsNull()) {
				interp.standardOutput()(s, Boolean(true));
			} else {
				String delimiter = Value::as_cstring(delimiterVal);
				if (delimiter == "\n") {
					interp.standardOutput()(s, Boolean(true));
				} else {
					interp.standardOutput()(s + delimiter, Boolean(false));
				}
			}
		} else {
			IOHelper::Print(s);
		}
		return IntrinsicResult(Value::Null);
	});

	// input(prompt=null)
	f = Intrinsic::Create("input");
	f.AddParam("prompt");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		String prompt =  String::New("");
		if (!ctx.GetArg(0).IsNull()) {
			prompt = StringUtils::Format("{0}", ctx.GetArg(0));
		}
		String result = IOHelper::Input(prompt);
		return IntrinsicResult(Value::make_string(result));
	});

	// err(msg, inner=null) — global intrinsic: create a new error value.
	f = Intrinsic::Create("err");
	f.AddParam("msg");
	f.AddParam("inner");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value msg = ctx.GetArg(0);
		Value inner = ctx.GetArg(1);
		if (!msg.IsString()) msg = Value::to_string(msg, ctx.vm);
		return IntrinsicResult(Value::make_error(msg, inner, ctx.vm.BuildStackTrace(), Value::Null));
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
		if (!self.IsError()) {
			ctx.vm.RaiseRuntimeError("err method called on non-error value");
			return IntrinsicResult::Null;
		}
		if (!msg.IsString()) msg = Value::to_string(msg, ctx.vm);
		// Build the new error with self as __isa.  Then verify no cycle.
		Value newErr = Value::make_error(msg, inner, ctx.vm.BuildStackTrace(), self);
		// Walk chain from newErr to check for loop (if newErr appears again).
		Value current = self;
		for (int depth = 0; depth < 256; depth++) {
			if (current.IsNull()) break;
			if (!current.IsError()) break;
			if (Value::value_identical(current, newErr)) {
				ctx.vm.RaiseRuntimeError("err: __isa chain would form a cycle");
				return IntrinsicResult::Null;
			}
			current = Value::error_isa(current);
		}
		return IntrinsicResult(newErr);
	});

	// info(ref)
	f = Intrinsic::Create("info");
	f.AddParam("ref");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value arg = ctx.GetArg(0);
		Value result = Value::make_map(8);
		Value parameters = Value::Null;
		Value pinfo = Value::Null;
		Value::map_set(result, "type", Value::value_type_name(arg));
		if (arg.IsList()) {
			Boolean computed = GCManager::Lists.Get(Value::value_item_index(arg)).Computed;
			Value::map_set(result, "computed", Value::Truth(computed));
			Value::map_set(result, "frozen", Value::Truth(Value::is_frozen(arg)));
		} else if (arg.IsMap()) {
			Value::map_set(result, "frozen", Value::Truth(Value::is_frozen(arg)));
		} else if (arg.IsFuncRef()) {
			FuncDef func = Value::funcref_funcdef(arg);
			Value::map_set(result, "name", func.Name());
			Value::map_set(result, "note", func.Note());
			parameters = Value::make_list(func.ParamNames().Count());
			for (int i=0; i < func.ParamNames().Count(); i++) {
				pinfo = Value::make_map(2);
				Value::map_set(pinfo, "name", func.ParamNames()[i]);
				Value::map_set(pinfo, "default", func.ParamDefaults()[i]);
				Value::list_push(parameters, pinfo);
			}
			Value::map_set(result, "params", parameters);
			if (Value::funcref_outer_vars(arg).IsNull()) {
				Value::map_set(result, "closure", Value::zero);
			} else {
				Value::map_set(result, "closure", Value::one);
			}
		} else if (arg.IsError()) {
			Value::map_set(result, "message", Value::error_message(arg));
			Value::map_set(result, "inner", Value::error_inner(arg));
			Value::map_set(result, "stack", Value::error_stack(arg));
			Value::map_set(result, "isa", Value::error_isa(arg));
		}
		Value::freeze_value(result);
		return IntrinsicResult(result);
	});

	// val(self=0)
	f = Intrinsic::Create("val");
	f.AddParam("self", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		if (v.IsNumber()) return IntrinsicResult(v);
		if (v.IsString()) return IntrinsicResult(Value::to_number(v));
		return IntrinsicResult(Value::Null);
	});

	// str(x="")
	f = Intrinsic::Create("str");
	f.AddParam("x", Value::make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		if (v.IsNull()) return IntrinsicResult(Value::make_string(""));
		return IntrinsicResult(Value::to_string(v, ctx.vm));
	});

	// upper(self)
	f = Intrinsic::Create("upper");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		return IntrinsicResult(Value::string_upper(v));
	});

	// lower(self)
	f = Intrinsic::Create("lower");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		return IntrinsicResult(Value::string_lower(v));
	});

	// char(codePoint=65)
	f = Intrinsic::Create("char");
	f.AddParam("codePoint", Value(65));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		double cp;
		Value e = RequireNumber(v, &cp);
		if (!e.IsNull()) return IntrinsicResult(e);
		return IntrinsicResult(Value::string_from_code_point((int)cp));
	});

	// code(self)
	f = Intrinsic::Create("code");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		if (!v.IsString()) return IntrinsicResult(ErrorTypes::TypeError("string", v));
		return IntrinsicResult(Value(Value::string_code_point(v)));
	});

	// len(x)
	f = Intrinsic::Create("len");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value container = ctx.GetArg(0);
		if (container.IsError()) return IntrinsicResult(container);
		Value result = Value::Null;
		if (container.IsList()) {
			result = Value(Value::list_count(container));
		} else if (container.IsString()) {
			result = Value(Value::string_length(container));
		} else if (container.IsMap()) {
			result = Value(Value::map_count(container));
		}
		return IntrinsicResult(result);
	});

	// remove(self, index)
	f = Intrinsic::Create("remove");
	f.AddParam("self");
	f.AddParam("index");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value container = ctx.GetArg(0);
		if (container.IsError()) return ctx.vm.RaiseUncaughtError(container);
		int result = 0;
		if (container.IsList()) {
			result = Value::list_remove(container, ctx.GetArg(1).IntValue()) ? 1 : 0;
		} else if (container.IsMap()) {
			result = Value::map_remove(container, ctx.GetArg(1)) ? 1 : 0;
		} else {
			return IntrinsicResult(ErrorTypes::TypeError("list or map", container));
		}
		return IntrinsicResult(Value(result));
	});

	// freeze(x)
	f = Intrinsic::Create("freeze");
	f.AddParam("x");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return ctx.vm.RaiseUncaughtError(v);
		Value::freeze_value(v);
		return IntrinsicResult(Value::Null);
	});

	// isFrozen(x)
	f = Intrinsic::Create("isFrozen");
	f.AddParam("x");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		return IntrinsicResult(Value::Truth(Value::is_frozen(v)));
	});

	// frozenCopy(x)
	f = Intrinsic::Create("frozenCopy");
	f.AddParam("x");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		return IntrinsicResult(Value::frozen_copy(v));
	});

	// abs(x=0)
	f = Intrinsic::Create("abs");
	f.AddParam("x", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		double x;
		Value e = RequireNumber(v, &x);
		if (!e.IsNull()) return IntrinsicResult(e);
		return IntrinsicResult(Value(Math::Abs(x)));
	});

	// acos(x=0)
	f = Intrinsic::Create("acos");
	f.AddParam("x", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		double x;
		Value e = RequireNumber(v, &x);
		if (!e.IsNull()) return IntrinsicResult(e);
		return IntrinsicResult(Value(Math::Acos(x)));
	});

	// asin(x=0)
	f = Intrinsic::Create("asin");
	f.AddParam("x", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		double x;
		Value e = RequireNumber(v, &x);
		if (!e.IsNull()) return IntrinsicResult(e);
		return IntrinsicResult(Value(Math::Asin(x)));
	});

	// atan(y=0, x=1)
	f = Intrinsic::Create("atan");
	f.AddParam("y", Value::zero);
	f.AddParam("x", Value::one);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value vy = ctx.GetArg(0);
		if (vy.IsError()) return IntrinsicResult(vy);
		Value vx = ctx.GetArg(1);
		if (vx.IsError()) return IntrinsicResult(vx);
		double y, x;
		Value e = RequireNumber(vy, &y);
		if (!e.IsNull()) return IntrinsicResult(e);
		e = RequireNumber(vx, &x);
		if (!e.IsNull()) return IntrinsicResult(e);
		if (x == 1.0) return IntrinsicResult(Value(Math::Atan(y)));
		return IntrinsicResult(Value(Math::Atan2(y, x)));
	});

	// ceil(x=0)
	f = Intrinsic::Create("ceil");
	f.AddParam("x", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		double x;
		Value e = RequireNumber(v, &x);
		if (!e.IsNull()) return IntrinsicResult(e);
		return IntrinsicResult(Value(Math::Ceiling(x)));
	});

	// cos(radians=0)
	f = Intrinsic::Create("cos");
	f.AddParam("radians", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		double x;
		Value e = RequireNumber(v, &x);
		if (!e.IsNull()) return IntrinsicResult(e);
		return IntrinsicResult(Value(Math::Cos(x)));
	});

	// floor(x=0)
	f = Intrinsic::Create("floor");
	f.AddParam("x", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		double x;
		Value e = RequireNumber(v, &x);
		if (!e.IsNull()) return IntrinsicResult(e);
		return IntrinsicResult(Value(Math::Floor(x)));
	});

	// log(x=0, base=10)
	f = Intrinsic::Create("log");
	f.AddParam("x", Value::zero);
	f.AddParam("base", Value(10));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value vx = ctx.GetArg(0);
		if (vx.IsError()) return IntrinsicResult(vx);
		Value vb = ctx.GetArg(1);
		if (vb.IsError()) return IntrinsicResult(vb);
		double x, b;
		Value e = RequireNumber(vx, &x);
		if (!e.IsNull()) return IntrinsicResult(e);
		e = RequireNumber(vb, &b);
		if (!e.IsNull()) return IntrinsicResult(e);
		double result;
		if (Math::Abs(b - 2.718282) < 0.000001) result = Math::Log(x);
		else result = Math::Log(x) / Math::Log(b);
		return IntrinsicResult(Value(result));
	});

	// pi
	f = Intrinsic::Create("pi");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(Value(Math::PI));
	});

	// round(x=0, decimalPlaces=0)
	f = Intrinsic::Create("round");
	f.AddParam("x", Value::zero);
	f.AddParam("decimalPlaces", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value vx = ctx.GetArg(0);
		if (vx.IsError()) return IntrinsicResult(vx);
		Value vd = ctx.GetArg(1);
		if (vd.IsError()) return IntrinsicResult(vd);
		double num, decimals;
		Value e = RequireNumber(vx, &num);
		if (!e.IsNull()) return IntrinsicResult(e);
		e = RequireNumber(vd, &decimals);
		if (!e.IsNull()) return IntrinsicResult(e);
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
		return IntrinsicResult(Value(num));
	});

	// rnd(seed)
	f = Intrinsic::Create("rnd");
	f.AddParam("seed");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return ctx.vm.RaiseUncaughtError(v);
		// If a seed is supplied, reseed the generator before drawing.  null
		// means "no seed"; a number (or numeric string) reseeds; any other
		// type is a parameter error.
		if (!v.IsNull()) {
			double seed;
			Value e = RequireNumber(v, &seed);
			if (!e.IsNull()) return IntrinsicResult(e);
			PRNG::Seed((UInt64)(Int64)seed);
		}
		return IntrinsicResult(Value(PRNG::Next()));
	});

	// sign(x=0)
	f = Intrinsic::Create("sign");
	f.AddParam("x", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		double x;
		Value e = RequireNumber(v, &x);
		if (!e.IsNull()) return IntrinsicResult(e);
		return IntrinsicResult(Value(Math::Sign(x)));
	});

	// sin(radians=0)
	f = Intrinsic::Create("sin");
	f.AddParam("radians", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		double x;
		Value e = RequireNumber(v, &x);
		if (!e.IsNull()) return IntrinsicResult(e);
		return IntrinsicResult(Value(Math::Sin(x)));
	});

	// sqrt(x=0)
	f = Intrinsic::Create("sqrt");
	f.AddParam("x", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		double x;
		Value e = RequireNumber(v, &x);
		if (!e.IsNull()) return IntrinsicResult(e);
		return IntrinsicResult(Value(Math::Sqrt(x)));
	});

	// tan(radians=0)
	f = Intrinsic::Create("tan");
	f.AddParam("radians", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		if (v.IsError()) return IntrinsicResult(v);
		double x;
		Value e = RequireNumber(v, &x);
		if (!e.IsNull()) return IntrinsicResult(e);
		return IntrinsicResult(Value(Math::Tan(x)));
	});
	// push(self, value)
	f = Intrinsic::Create("push");
	f.AddParam("self");
	f.AddParam("value");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (self.IsError()) return ctx.vm.RaiseUncaughtError(self);
		Value value = ctx.GetArg(1);
		if (self.IsList()) {
			Value::list_push(self, value);
			return IntrinsicResult(self);
		} else if (self.IsMap()) {
			Value::map_set(self, value, Value::one);
			return IntrinsicResult(self);
		}
		return IntrinsicResult(ErrorTypes::TypeError("list or map", self));
	});

	// pop(self)
	f = Intrinsic::Create("pop");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (self.IsError()) return ctx.vm.RaiseUncaughtError(self);
		Value result = Value::Null;
		if (self.IsList()) {
			result = Value::list_pop(self);
		} else if (self.IsMap()) {
			if (Value::map_count(self) == 0) return IntrinsicResult(Value::Null);
			MapIterator iter = Value::map_iterator(self);
			if (map_iterator_next(&iter, &result, nullptr)) {
				// remove key that was found
				Value::map_remove(self, result);
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
		if (self.IsError()) return ctx.vm.RaiseUncaughtError(self);
		Value result = Value::Null;
		if (self.IsList()) {
			result = Value::list_pull(self);
		} else if (self.IsMap()) {
			if (Value::map_count(self) == 0) return IntrinsicResult(Value::Null);
			MapIterator iter = Value::map_iterator(self);
			if (map_iterator_next(&iter, &result, nullptr)) {
				// remove key that was found
				Value::map_remove(self, result);
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
		if (self.IsError()) return ctx.vm.RaiseUncaughtError(self);
		int index = (int)Value::numeric_val(ctx.GetArg(1));
		Value value = ctx.GetArg(2);
		if (self.IsList()) {
			Value::list_insert(self, index, value);
			return IntrinsicResult(self);
		} else if (self.IsString()) {
			return IntrinsicResult(Value::string_insert(self, index, value, ctx.vm));
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
		if (self.IsError()) return IntrinsicResult(self);
		Value value = ctx.GetArg(1);
		Value after = ctx.GetArg(2);
		Value result = Value::Null;
		Value iterKey, iterVal;
		if (self.IsList()) {
			int afterIdx = -1;
			if (!after.IsNull()) {
				afterIdx = (int)Value::numeric_val(after);
				if (afterIdx < -1) afterIdx += Value::list_count(self);
			}
			int idx = Value::list_indexOf(self, value, afterIdx);
			if (idx >= 0) result = Value(idx);
		} else if (self.IsString()) {
			if (!value.IsString()) return IntrinsicResult(Value::Null);
			int afterIdx = -1;
			if (!after.IsNull()) {
				afterIdx = (int)Value::numeric_val(after);
				if (afterIdx < -1) afterIdx += Value::string_length(self);
			}
			int idx = Value::string_indexOf(self, value, afterIdx + 1);
			if (idx >= 0) result = Value(idx);
		} else if (self.IsMap()) {
			// Find key where value matches
			bool pastAfter = after.IsNull();
			MapIterator iter = Value::map_iterator(self);
			while (map_iterator_next(&iter, &iterKey, &iterVal)) {
				if (!pastAfter) {
					if (iterKey == after) {
						pastAfter = Boolean(true);
					}
					continue;
				}
				if (iterVal == value) {
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
	f.AddParam("ascending", Value::one);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (self.IsError()) return ctx.vm.RaiseUncaughtError(self);
		Value byKey = ctx.GetArg(1);
		bool ascending = ctx.GetArg(2).BoolValue();
		if (!self.IsList()) return IntrinsicResult(ErrorTypes::TypeError("list", self));
		if (Value::list_count(self) < 2) return IntrinsicResult(self);
		if (byKey.IsNull()) {
			Value::list_sort(self, ascending);
		} else {
			Value::list_sort_by_key(self, byKey, ascending);
		}
		return IntrinsicResult(self);
	});

	// shuffle(self)
	f = Intrinsic::Create("shuffle");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (self.IsError()) return ctx.vm.RaiseUncaughtError(self);
		Value temp;
		Value iterKey, iterVal;
		if (self.IsList()) {
			if (Value::is_frozen(self)) { ctx.vm.RaiseRuntimeError("Attempt to modify a frozen list"); return IntrinsicResult(Value::Null); }
			int count = Value::list_count(self);
			for (int i = count - 1; i > 0; i--) {
				int j = (int)(PRNG::Next() * (i + 1));
				temp = Value::list_get(self, i);
				Value::list_set(self, i, Value::list_get(self, j));
				Value::list_set(self, j, temp);
			}
		} else if (self.IsMap()) {
			if (Value::is_frozen(self)) { ctx.vm.RaiseRuntimeError("Attempt to modify a frozen map"); return IntrinsicResult(Value::Null); }
			// Collect keys and values
			int count = Value::map_count(self);
			List<Value> keys =  List<Value>::New(count);
			List<Value> vals =  List<Value>::New(count);
			MapIterator iter = Value::map_iterator(self);
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
				Value::map_set(self, keys[i], vals[i]);
			}
		} else {
			return IntrinsicResult(ErrorTypes::TypeError("list or map", self));
		}
		return IntrinsicResult(Value::Null);
	});

	// join(self, delimiter=" ")
	f = Intrinsic::Create("join");
	f.AddParam("self");
	f.AddParam("delimiter", Value::make_string(" "));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (self.IsError()) return IntrinsicResult(self);
		if (!self.IsList()) return IntrinsicResult(ErrorTypes::TypeError("list", self));
		Value delim = ctx.GetArg(1);
		String delimStr = delim.IsNull() ? " " : Value::to_String(delim);
		int count = Value::list_count(self);
		List<String> parts =  List<String>::New(count);
		for (int i = 0; i < count; i++) {
			parts.Add(Value::to_String(Value::list_get(self, i)));
		}
		return IntrinsicResult(Value::make_string(String::Join(delimStr, parts)));
	});

	// split(self, delimiter=" ", maxCount=-1)
	f = Intrinsic::Create("split");
	f.AddParam("self");
	f.AddParam("delimiter", Value::make_string(" "));
	f.AddParam("maxCount", Value(-1));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (self.IsError()) return IntrinsicResult(self);
		if (!self.IsString()) return IntrinsicResult(ErrorTypes::TypeError("string", self));
		Value delim = ctx.GetArg(1);
		int maxCount = (int)Value::numeric_val(ctx.GetArg(2));
		return IntrinsicResult(Value::string_split_max(self, delim, maxCount));
	});

	// replace(self, oldval, newval, maxCount=null)
	f = Intrinsic::Create("replace");
	f.AddParam("self");
	f.AddParam("oldval");
	f.AddParam("newval");
	f.AddParam("maxCount");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (self.IsError()) return ctx.vm.RaiseUncaughtError(self);
		Value oldVal = ctx.GetArg(1);
		Value newVal = ctx.GetArg(2);
		Value maxCountVal = ctx.GetArg(3);
		Value iterKey, iterVal;
		int maxCount = maxCountVal.IsNull() ? -1 : (int)Value::numeric_val(maxCountVal);
		if (self.IsList()) {
			int count = Value::list_count(self);
			int found = 0;
			for (int i = 0; i < count; i++) {
				if (Value::list_get(self, i) == oldVal) {
					Value::list_set(self, i, newVal);
					found++;
					if (maxCount > 0 && found >= maxCount) break;
				}
			}
			return IntrinsicResult(self);
		} else if (self.IsMap()) {
			// Collect keys whose values match
			List<Value> keysToChange =  List<Value>::New();
			MapIterator iter = Value::map_iterator(self);
			while (map_iterator_next(&iter, &iterKey, &iterVal)) {
				if (iterVal == oldVal) {
					keysToChange.Add(iterKey);
					if (maxCount > 0 && keysToChange.Count() >= maxCount) break;
				}
			}
			for (int i = 0; i < keysToChange.Count(); i++) {
				Value::map_set(self, keysToChange[i], newVal);
			}
			return IntrinsicResult(self);
		} else if (self.IsString()) {
			return IntrinsicResult(Value::string_replace_max(self, oldVal, newVal, maxCount));
		}
		return IntrinsicResult(ErrorTypes::TypeError("list, map, or string", self));
	});

	// sum(self)
	f = Intrinsic::Create("sum");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (self.IsError()) return IntrinsicResult(self);
		Value iterVal;
		double total = 0;
		if (self.IsList()) {
			int count = Value::list_count(self);
			for (int i = 0; i < count; i++) {
				total += Value::numeric_val(Value::list_get(self, i));
			}
		} else if (self.IsMap()) {
			MapIterator iter = Value::map_iterator(self);
			while (map_iterator_next(&iter, nullptr, &iterVal)) {
				total += Value::numeric_val(iterVal);
			}
		} else {
			return IntrinsicResult(Value::zero);
		}
		if (total == (int)total && total >= Int32MinValue && total <= Int32MaxValue) {
			return IntrinsicResult(Value((int)total));
		}
		return IntrinsicResult(Value(total));
	});

	// slice(seq, from=0, to=null)
	f = Intrinsic::Create("slice");
	f.AddParam("seq");
	f.AddParam("from", Value::zero);
	f.AddParam("to");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value seq = ctx.GetArg(0);
		if (seq.IsError()) return IntrinsicResult(seq);
		int fromIdx = (int)Value::numeric_val(ctx.GetArg(1));
		if (seq.IsList()) {
			int count = Value::list_count(seq);
			int toIdx = ctx.GetArg(2).IsNull() ? count : (int)Value::numeric_val(ctx.GetArg(2));
			return IntrinsicResult(Value::list_slice(seq, fromIdx, toIdx));
		} else if (seq.IsString()) {
			int slen = Value::string_length(seq);
			int toIdx = ctx.GetArg(2).IsNull() ? slen : (int)Value::numeric_val(ctx.GetArg(2));
			return IntrinsicResult(Value::string_slice(seq, fromIdx, toIdx));
		}
		return IntrinsicResult(ErrorTypes::TypeError("list or string", seq));
	});

	// indexes(self)
	f = Intrinsic::Create("indexes");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (self.IsError()) return IntrinsicResult(self);
		Value result = Value::Null;
		Value iterKey;
		if (self.IsList()) {
			int count = Value::list_count(self);
			result = Value::make_list(count);
			for (int i = 0; i < count; i++) {
				Value::list_push(result, Value(i));
			}
			return IntrinsicResult(result);
		} else if (self.IsString()) {
			int slen = Value::string_length(self);
			result = Value::make_list(slen);
			for (int i = 0; i < slen; i++) {
				Value::list_push(result, Value(i));
			}
			return IntrinsicResult(result);
		} else if (self.IsMap()) {
			result = Value::make_list(Value::map_count(self));
			MapIterator iter = Value::map_iterator(self);
			while (map_iterator_next(&iter, &iterKey, nullptr)) {
				Value::list_push(result, iterKey);
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
		if (self.IsError()) return IntrinsicResult(self);
		Value index = ctx.GetArg(1);
		if (self.IsList()) {
			if (!index.IsNumber()) return IntrinsicResult(Value::zero);
			int i = (int)Value::numeric_val(index);
			int count = Value::list_count(self);
			return IntrinsicResult(Value::Truth(i >= -count && i < count));
		} else if (self.IsString()) {
			if (!index.IsNumber()) return IntrinsicResult(Value::zero);
			int i = (int)Value::numeric_val(index);
			int slen = Value::string_length(self);
			return IntrinsicResult(Value::Truth(i >= -slen && i < slen));
		} else if (self.IsMap()) {
			return IntrinsicResult(Value::Truth(Value::map_has_key(self, index)));
		}
		return IntrinsicResult(ErrorTypes::TypeError("list, string, or map", self));
	});

	// values(self)
	f = Intrinsic::Create("values");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value self = ctx.GetArg(0);
		if (self.IsError()) return IntrinsicResult(self);
		Value result = self;
		Value iterVal;
		if (self.IsMap()) {
			result = Value::make_list(Value::map_count(self));
			MapIterator iter = Value::map_iterator(self);
			while (map_iterator_next(&iter, nullptr, &iterVal)) {
				Value::list_push(result, iterVal);
			}
		} else if (self.IsString()) {
			int slen = Value::string_length(self);
			result = Value::make_list(slen);
			for (int i = 0; i < slen; i++) {
				Value::list_push(result, Value::string_substring(self, i, 1));
			}
		} else if (!self.IsList()) {
			// A list returns itself (its values are its elements); any other
			// non-container type is an error.
			return IntrinsicResult(ErrorTypes::TypeError("list, string, or map", self));
		}
		return IntrinsicResult(result);
	});

	// range(from=0, to=0, step=null)
	f = Intrinsic::Create("range");
	f.AddParam("from", Value::zero);
	f.AddParam("to", Value::zero);
	f.AddParam("step");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value vFrom = ctx.GetArg(0);
		if (vFrom.IsError()) return IntrinsicResult(vFrom);
		Value vTo = ctx.GetArg(1);
		if (vTo.IsError()) return IntrinsicResult(vTo);
		Value vStep = ctx.GetArg(2);
		if (vStep.IsError()) return IntrinsicResult(vStep);
		double fromVal, toVal;
		Value e = RequireNumber(vFrom, &fromVal);
		if (!e.IsNull()) return IntrinsicResult(e);
		e = RequireNumber(vTo, &toVal);
		if (!e.IsNull()) return IntrinsicResult(e);
		double step;
		if (vStep.IsNull()) {
			step = (toVal >= fromVal) ? 1 : -1;
		} else {
			e = RequireNumber(vStep, &step);
			if (!e.IsNull()) return IntrinsicResult(e);
		}
		if (step == 0) {
			return IntrinsicResult(ErrorTypes::RuntimeError("range() step may not be zero"));
		}
		double rawCount = (toVal - fromVal) / step + 1.0;
		if (StringUtils::IsNaN(rawCount) || rawCount > Value::MAX_COLLECTION_SIZE) {
			return IntrinsicResult(ErrorTypes::RuntimeError(
				"range() result too large (exceeds maximum list size)"));
		}
		int count = (int)((toVal - fromVal) / step) + 1;
		if (count < 0) count = 0;
		// Build a computed list: element i is fromVal + step*i.  This is O(1)
		// to construct and materializes lazily only if the list is mutated.
		Value result = GCManager::NewComputedList(Value(fromVal), Value(step), count);
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
		return IntrinsicResult(Value(ctx.vm.ElapsedTime()));
	});

	// wait(seconds=1)
	//    Pause execution of this script for some amount of time.
	// seconds (default 1.0): how many seconds to wait
	// See also: time, yield
	f = Intrinsic::Create("wait");
	f.AddParam("seconds", Value::one);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		double now = ctx.vm.ElapsedTime();
		Value vSeconds;
		if (partialResult.done) {
			// Fresh call: calculate end time and return as partial result
			vSeconds = ctx.GetArg(0);
			if (vSeconds.IsError()) return ctx.vm.RaiseUncaughtError(vSeconds);
			double interval = Value::numeric_val(vSeconds);
			return IntrinsicResult(Value(now + interval), Boolean(false));
		} else {
			// Continuation: check if we've waited long enough
			if (now > Value::numeric_val(partialResult.result)) return IntrinsicResult::Null;
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
		replInList = Value::make_list(0);
		replOutList = Value::make_list(0);
		Interpreter interp = ctx.vm.GetInterpreter();
		if (!IsNull(interp)) interp.ResetReplGlobals();
		GCManager::FullCollectGarbage();
		return IntrinsicResult::Null;
	});

	// bitAnd(i=0, j=0)
	f = Intrinsic::Create("bitAnd");
	f.AddParam("i", Value::zero);
	f.AddParam("j", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value ai = ctx.GetArg(0);
		if (ai.IsError()) return IntrinsicResult(ai);
		Value aj = ctx.GetArg(1);
		if (aj.IsError()) return IntrinsicResult(aj);
		Double vi, vj;
		Value e = RequireNumber(ai, &vi);
		if (!e.IsNull()) return IntrinsicResult(e);
		e = RequireNumber(aj, &vj);
		if (!e.IsNull()) return IntrinsicResult(e);
		UInt64 ui = (UInt64)Math::Abs(vi);
		UInt64 uj = (UInt64)Math::Abs(vj);
		Int32 si = vi < 0 ? 1 : 0;
		Int32 sj = vj < 0 ? 1 : 0;
		Int32 sign = si & sj;
		Double result = (Double)(ui & uj);
		return IntrinsicResult(Value(sign != 0 ? -result : result));
	});

	// bitOr(i=0, j=0)
	f = Intrinsic::Create("bitOr");
	f.AddParam("i", Value::zero);
	f.AddParam("j", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value ai = ctx.GetArg(0);
		if (ai.IsError()) return IntrinsicResult(ai);
		Value aj = ctx.GetArg(1);
		if (aj.IsError()) return IntrinsicResult(aj);
		Double vi, vj;
		Value e = RequireNumber(ai, &vi);
		if (!e.IsNull()) return IntrinsicResult(e);
		e = RequireNumber(aj, &vj);
		if (!e.IsNull()) return IntrinsicResult(e);
		UInt64 ui = (UInt64)Math::Abs(vi);
		UInt64 uj = (UInt64)Math::Abs(vj);
		Int32 si = vi < 0 ? 1 : 0;
		Int32 sj = vj < 0 ? 1 : 0;
		Int32 sign = si | sj;
		Double result = (Double)(ui | uj);
		return IntrinsicResult(Value(sign != 0 ? -result : result));
	});

	// bitXor(i=0, j=0)
	f = Intrinsic::Create("bitXor");
	f.AddParam("i", Value::zero);
	f.AddParam("j", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value ai = ctx.GetArg(0);
		if (ai.IsError()) return IntrinsicResult(ai);
		Value aj = ctx.GetArg(1);
		if (aj.IsError()) return IntrinsicResult(aj);
		Double vi, vj;
		Value e = RequireNumber(ai, &vi);
		if (!e.IsNull()) return IntrinsicResult(e);
		e = RequireNumber(aj, &vj);
		if (!e.IsNull()) return IntrinsicResult(e);
		UInt64 ui = (UInt64)Math::Abs(vi);
		UInt64 uj = (UInt64)Math::Abs(vj);
		Int32 si = vi < 0 ? 1 : 0;
		Int32 sj = vj < 0 ? 1 : 0;
		Int32 sign = si ^ sj;
		Double result = (Double)(ui ^ uj);
		return IntrinsicResult(Value(sign != 0 ? -result : result));
	});

	// hash(obj)
	f = Intrinsic::Create("hash");
	f.AddParam("obj");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value v = ctx.GetArg(0);
		return IntrinsicResult(Value(v.Hash()));
	});

	// refEquals(a, b)
	f = Intrinsic::Create("refEquals");
	f.AddParam("a");
	f.AddParam("b");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value a = ctx.GetArg(0);
		Value b = ctx.GetArg(1);
		return IntrinsicResult(Value::Truth(Value::value_identical(a, b)));
	});

	// version
	f = Intrinsic::Create("version");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		if (_versionMap.IsNull()) {
			_versionMap = Value::make_map(6);
			Value::map_set(_versionMap, "miniscript", "2.0");
			Value::map_set(_versionMap, "buildDate", BuildDate());
			Value::map_set(_versionMap, "platform", PlatformName());
			Value::map_set(_versionMap, "host", hostVersion);
			Value::map_set(_versionMap, "hostName", hostName);
			Value::map_set(_versionMap, "hostInfo", hostInfo);
			Value::freeze_value(_versionMap);
		}
		return IntrinsicResult(_versionMap);
	});

	// gc.collect(full=false)  — underlying implementation for gc.collect
	_gcCollectIntr = Intrinsic::Create("");
	f = _gcCollectIntr;
	f.AddParam("full", Value::zero);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		Value vFull = ctx.GetArg(0);
		if (vFull.BoolValue()) {
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
		Value result = Value::make_map(8);
		int bigStrings     = GCManager::BigStrings.LiveCount();
		int internedStrings = GCManager::InternedStrings.LiveCount();
		int lists          = GCManager::Lists.LiveCount();
		int maps           = GCManager::Maps.LiveCount();
		int errors         = GCManager::Errors.LiveCount();
		int functions      = GCManager::Functions.LiveCount();
		int total = bigStrings + internedStrings + lists + maps + errors + functions;
		Value::map_set(result, "bigStrings",      Value(bigStrings));
		Value::map_set(result, "internedStrings", Value(internedStrings));
		Value::map_set(result, "lists",           Value(lists));
		Value::map_set(result, "maps",            Value(maps));
		Value::map_set(result, "errors",          Value(errors));
		Value::map_set(result, "functions",       Value(functions));
		Value::map_set(result, "total",           Value(total));
		Value::freeze_value(result);
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
	if (_intrinsicsMap.IsNull()) {
		int count = Intrinsic::Count();
		_intrinsicsMap = Value::make_map(count);
		for (Int32 i = 0; i < count; i++) {
			Intrinsic intr = Intrinsic::GetByIndex(i);
			if (IsNull(intr) || IsNull(intr.Name()) || intr.Name().Length() == 0) continue;
			Value::map_set(_intrinsicsMap, intr.Name(), intr.GetFunc());
		}
		Value::freeze_value(_intrinsicsMap);
	}
	return _intrinsicsMap;
}
Value CoreIntrinsics::_intrinsicsMap = Value::Null;
Value CoreIntrinsics::GCMap() {
	if (_gcMap.IsNull()) {
		_gcMap = Value::make_map(2);
		if (!IsNull(_gcCollectIntr)) Value::map_set(_gcMap, "collect", _gcCollectIntr.GetFunc());
		if (!IsNull(_gcStatsIntr)) Value::map_set(_gcMap, "stats", _gcStatsIntr.GetFunc());
		Value::freeze_value(_gcMap);
	}
	return _gcMap;
}
Value CoreIntrinsics::_gcMap = Value::Null;
Intrinsic CoreIntrinsics::_gcCollectIntr = nullptr;
Intrinsic CoreIntrinsics::_gcStatsIntr = nullptr;
Value CoreIntrinsics::_versionMap = Value::Null;
List<VoidCallback> CoreIntrinsics::_invalidateCallbacks = nullptr;
void CoreIntrinsics::RegisterInvalidateCallback(VoidCallback callback) {
	if (IsNull(_invalidateCallbacks)) _invalidateCallbacks =  List<VoidCallback>::New();
	_invalidateCallbacks.Add(callback);
}
void CoreIntrinsics::InvalidateTypeMaps() {
	_listType = Value::Null;
	_stringType = Value::Null;
	_mapType = Value::Null;
	_numberType = Value::Null;
	_functionType = Value::Null;
	_errorType = Value::Null;
	_gcMap = Value::Null;
	_versionMap = Value::Null;
	_intrinsicsMap = Value::Null;
	if (!IsNull(_invalidateCallbacks)) {
		for (Int32 i = 0; i < _invalidateCallbacks.Count(); i++) _invalidateCallbacks[i]();
	}
	Intrinsic::ClearShortNames();
}

} // end of namespace MiniScript
