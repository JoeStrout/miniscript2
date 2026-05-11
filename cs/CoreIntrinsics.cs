// CoreIntrinsics.cs - Definitions of all built-in intrinsic functions.

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// H: #include "value.h"
// H: #include "ErrorTypes.g.h"
// CPP: #include "Intrinsic.g.h"
// H: #include "GCManager.h"
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
// CPP: #include <random>

namespace MiniScript {

public static class CoreIntrinsics {

	static Random _random;  // CPP: 
	
	// If given a nonzero seed, seed our PRNG accordingly.
	// Then (in either case), return the next random number drawn
	// from the range [0, 1) with a uniform distribution.
	private static double GetNextRandom(int seed=0) {
		//*** BEGIN CS_ONLY ***
		if (seed != 0) {
			_random = new Random(seed);
		} else if (_random == null) {
			_random = new Random();
		}
		return _random.NextDouble();
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		static std::mt19937 gen(std::random_device{}());
		static std::uniform_real_distribution<double> dist(0.0, 1.0);
		if (seed != 0) gen.seed(static_cast<unsigned int>(seed));
		return dist(gen);
		*** END CPP_ONLY ***/
	}


	private static void AddIntrinsicToMap(Value map, String methodName) {
		Intrinsic intr = Intrinsic.GetByName(methodName);
		if (intr != null) {
			map_set(map, make_string(methodName), intr.GetFunc());
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
	private static Value _listType = val_null;

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
	private static Value _stringType = val_null;

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
	private static Value _mapType = val_null;
	
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
	private static Value _numberType = val_null;	

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
	private static Value _functionType = val_null;

	// 
	// ErrorType: a static map that represents the `error` type, and provides
	// intrinsic methods that can be invoked on an error via dot syntax
	// (notably `err` for creating a specialization).
	// 
	public static Value ErrorType() {
		if (is_null(_errorType)) {
			_errorType = make_map(4);
			// The method-form of err (with self) is registered under "__error_err"
			// but exposed in this map as "err".
			Intrinsic errMethod = Intrinsic.GetByName("__error_err");
			if (errMethod != null) {
				map_set(_errorType, make_string("err"), errMethod.GetFunc());
			}
			Intrinsic.AddShortName(_errorType, "error");
		}
		return _errorType;
	}
	private static Value _errorType = val_null;

	private static Value _EOL = make_string("\n");

	// REPL history lists, set by App.RunREPL at startup and by the reset intrinsic.
	public static Value replInList = val_null;
	public static Value replOutList = val_null;

	// H: static void MarkRoots(void* user_data, GCManager& gc);

	public static void Init() {
		// CPP: GCManager::Instance().RegisterMarkCallback(CoreIntrinsics::MarkRoots, nullptr);

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
			return new IntrinsicResult(val_null);
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
			return new IntrinsicResult(make_error(msg, inner, ctx.vm.BuildStackTrace(), val_null));
		};

		// err method on ErrorType: se.err(msg, inner=null) creates a new error
		// whose __isa is se.  Terminates if this would create an __isa cycle.
		f = Intrinsic.Create("__error_err");
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
			if (is_null(arg)) {
				// ToDo: return an error
				return IntrinsicResult.Null;
			}
			Value result = make_map(8);
			Value parameters = val_null;
			Value pinfo = val_null;
			if (is_funcref(arg)) {
				map_set(result, make_string("type"), make_string("funcRef"));
				Int32 funcIndex = funcref_index(arg);
				map_set(result, make_string("__idx"), make_int(funcIndex));
				FuncDef func = ctx.vm.GetFuncDef(funcIndex);
				map_set(result, make_string("name"), make_string(func.Name));
				map_set(result, make_string("note"), make_string(func.Note));
				parameters = make_list(func.ParamNames.Count);
				for (int i=0; i < func.ParamNames.Count; i++) {
					pinfo = make_map(2);
					map_set(pinfo, make_string("name"), func.ParamNames[i]);
					map_set(pinfo, make_string("default"), func.ParamDefaults[i]);
					list_push(parameters, pinfo);
				}
				map_set(result, make_string("params"), parameters);
				if (is_null(funcref_outer_vars(arg))) {
					map_set(result, make_string("closure"), val_zero);
				} else {
					map_set(result, make_string("closure"), val_one);
				}
			} else if (is_string(arg)) {
				map_set(result, make_string("type"), make_string("string"));
			} else if (is_number(arg)) {
				map_set(result, make_string("type"), make_string("number"));
			} else if (is_list(arg)) {
				map_set(result, make_string("type"), make_string("list"));
			} else if (is_map(arg)) {
				map_set(result, make_string("type"), make_string("map"));
			} else if (is_error(arg)) {
				map_set(result, make_string("type"), make_string("error"));
				map_set(result, make_string("message"), error_message(arg));
				map_set(result, make_string("inner"), error_inner(arg));
				map_set(result, make_string("stack"), error_stack(arg));
				map_set(result, make_string("isa"), error_isa(arg));
			} else if (is_null(arg)) {
				map_set(result, make_string("type"), make_string("null"));
			} else {
				map_set(result, make_string("type"), make_string("unknown"));
			}
			freeze_value(result);
			return new IntrinsicResult(result);
		};

		// val(self=0)
		f = Intrinsic.Create("val");
		f.AddParam("self", val_zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			if (is_number(v)) return new IntrinsicResult(v);
			if (is_string(v)) return new IntrinsicResult(to_number(v));
			return new IntrinsicResult(val_null);
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
			int codePoint = (int)numeric_val(v);
			return new IntrinsicResult(string_from_code_point(codePoint));
		};

		// code(self)
		f = Intrinsic.Create("code");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(make_int(string_code_point(v)));
		};

		// len(x)
		f = Intrinsic.Create("len");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value container = ctx.GetArg(0);
			if (is_error(container)) return new IntrinsicResult(container);
			Value result = val_null;
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
				IOHelper.Print("ERROR: `remove` must be called on list or map");
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
			return new IntrinsicResult(val_null);
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
		f.AddParam("x", val_zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(make_double(Math.Abs(numeric_val(v))));
		};

		// acos(x=0)
		f = Intrinsic.Create("acos");
		f.AddParam("x", val_zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(make_double(Math.Acos(numeric_val(v))));
		};

		// asin(x=0)
		f = Intrinsic.Create("asin");
		f.AddParam("x", val_zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(make_double(Math.Asin(numeric_val(v))));
		};

		// atan(y=0, x=1)
		f = Intrinsic.Create("atan");
		f.AddParam("y", val_zero);
		f.AddParam("x", val_one);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value vy = ctx.GetArg(0);
			if (is_error(vy)) return new IntrinsicResult(vy);
			Value vx = ctx.GetArg(1);
			if (is_error(vx)) return new IntrinsicResult(vx);
			double y = numeric_val(vy);
			double x = numeric_val(vx);
			if (x == 1.0) return new IntrinsicResult(make_double(Math.Atan(y)));
			return new IntrinsicResult(make_double(Math.Atan2(y, x)));
		};

		// ceil(x=0)
		f = Intrinsic.Create("ceil");
		f.AddParam("x", val_zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(make_double(Math.Ceiling(numeric_val(v))));
		};

		// cos(radians=0)
		f = Intrinsic.Create("cos");
		f.AddParam("radians", val_zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(make_double(Math.Cos(numeric_val(v))));
		};

		// floor(x=0)
		f = Intrinsic.Create("floor");
		f.AddParam("x", val_zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(make_double(Math.Floor(numeric_val(v))));
		};

		// log(x=0, base=10)
		f = Intrinsic.Create("log");
		f.AddParam("x", val_zero);
		f.AddParam("base", make_int(10));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value vx = ctx.GetArg(0);
			if (is_error(vx)) return new IntrinsicResult(vx);
			Value vb = ctx.GetArg(1);
			if (is_error(vb)) return new IntrinsicResult(vb);
			double x = numeric_val(vx);
			double b = numeric_val(vb);
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
		f.AddParam("x", val_zero);
		f.AddParam("decimalPlaces", val_zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value vx = ctx.GetArg(0);
			if (is_error(vx)) return new IntrinsicResult(vx);
			double num = numeric_val(vx);
			int decimalPlaces = (int)numeric_val(ctx.GetArg(1));
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
			int seed = is_null(v) ? 0 : (int)numeric_val(v);
			return new IntrinsicResult(make_double(GetNextRandom(seed)));
		};

		// sign(x=0)
		f = Intrinsic.Create("sign");
		f.AddParam("x", val_zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(make_int(Math.Sign(numeric_val(v))));
		};

		// sin(radians=0)
		f = Intrinsic.Create("sin");
		f.AddParam("radians", val_zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(make_double(Math.Sin(numeric_val(v))));
		};

		// sqrt(x=0)
		f = Intrinsic.Create("sqrt");
		f.AddParam("x", val_zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(make_double(Math.Sqrt(numeric_val(v))));
		};

		// tan(radians=0)
		f = Intrinsic.Create("tan");
		f.AddParam("radians", val_zero);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value v = ctx.GetArg(0);
			if (is_error(v)) return new IntrinsicResult(v);
			return new IntrinsicResult(make_double(Math.Tan(numeric_val(v))));
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
				map_set(self, value, val_one);
				return new IntrinsicResult(self);
			}
			return new IntrinsicResult(val_null);
		};

		// pop(self)
		f = Intrinsic.Create("pop");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
			Value result = val_null;
			if (is_list(self)) {
				result = list_pop(self);
			} else if (is_map(self)) {
				if (map_count(self) == 0) return new IntrinsicResult(val_null);
				MapIterator iter = map_iterator(self);
				if (map_iterator_next(ref iter)) { // CPP: if (map_iterator_next(&iter, &result, nullptr)) {
					result = iter.Key;             // CPP: // remove key that was found
					map_remove(self, result);
				}
			}
			return new IntrinsicResult(result);
		};

		// pull(self)
		f = Intrinsic.Create("pull");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
			Value result = val_null;
			if (is_list(self)) {
				result = list_pull(self);
			} else if (is_map(self)) {
				if (map_count(self) == 0) return new IntrinsicResult(val_null);
				MapIterator iter = map_iterator(self);
				if (map_iterator_next(ref iter)) { // CPP: if (map_iterator_next(&iter, &result, nullptr)) {
					result = iter.Key;             // CPP: // remove key that was found
					map_remove(self, result);
				}
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
			return new IntrinsicResult(val_null);
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
			Value result = val_null;
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
				if (!is_string(value)) return new IntrinsicResult(val_null);
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
			}
			return new IntrinsicResult(result);
		};

		// sort(self, byKey=null, ascending=1)
		f = Intrinsic.Create("sort");
		f.AddParam("self");
		f.AddParam("byKey");
		f.AddParam("ascending", val_one);
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return ctx.vm.RaiseUncaughtError(self);
			Value byKey = ctx.GetArg(1);
			bool ascending = is_truthy(ctx.GetArg(2));
			if (!is_list(self)) return new IntrinsicResult(self);
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
				if (is_frozen(self)) { ctx.vm.RaiseRuntimeError("Attempt to modify a frozen list"); return new IntrinsicResult(val_null); }
				int count = list_count(self);
				for (int i = count - 1; i > 0; i--) {
					int j = (int)(GetNextRandom() * (i + 1));
					temp = list_get(self, i);
					list_set(self, i, list_get(self, j));
					list_set(self, j, temp);
				}
			} else if (is_map(self)) {
				if (is_frozen(self)) { ctx.vm.RaiseRuntimeError("Attempt to modify a frozen map"); return new IntrinsicResult(val_null); }
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
					int j = (int)(GetNextRandom() * (i + 1));
					temp = vals[i];
					vals[i] = vals[j];
					vals[j] = temp;
				}
				for (int i = 0; i < count; i++) {
					map_set(self, keys[i], vals[i]);
				}
			}
			return new IntrinsicResult(val_null);
		};

		// join(self, delimiter=" ")
		f = Intrinsic.Create("join");
		f.AddParam("self");
		f.AddParam("delimiter", make_string(" "));
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return new IntrinsicResult(self);
			if (!is_list(self)) return new IntrinsicResult(self);
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
			if (!is_string(self)) return new IntrinsicResult(val_null);
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
			return new IntrinsicResult(val_null);
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
				return new IntrinsicResult(val_zero);
			}
			if (total == (int)total && total >= Int32.MinValue && total <= Int32.MaxValue) {
				return new IntrinsicResult(make_int((int)total));
			}
			return new IntrinsicResult(make_double(total));
		};

		// slice(seq, from=0, to=null)
		f = Intrinsic.Create("slice");
		f.AddParam("seq");
		f.AddParam("from", val_zero);
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
			return new IntrinsicResult(val_null);
		};

		// indexes(self)
		f = Intrinsic.Create("indexes");
		f.AddParam("self");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value self = ctx.GetArg(0);
			if (is_error(self)) return new IntrinsicResult(self);
			Value result = val_null;
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
				if (!is_number(index)) return new IntrinsicResult(val_zero);
				int i = (int)numeric_val(index);
				int count = list_count(self);
				return new IntrinsicResult(make_int((i >= -count && i < count) ? 1 : 0));
			} else if (is_string(self)) {
				if (!is_number(index)) return new IntrinsicResult(val_zero);
				int i = (int)numeric_val(index);
				int slen = string_length(self);
				return new IntrinsicResult(make_int((i >= -slen && i < slen) ? 1 : 0));
			} else if (is_map(self)) {
				return new IntrinsicResult(make_int(map_has_key(self, index) ? 1 : 0));
			}
			return new IntrinsicResult(val_null);
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
			}
			return new IntrinsicResult(result);
		};

		// range(from=0, to=0, step=null)
		f = Intrinsic.Create("range");
		f.AddParam("from", val_zero);
		f.AddParam("to", val_zero);
		f.AddParam("step");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Value vFrom = ctx.GetArg(0);
			if (is_error(vFrom)) return new IntrinsicResult(vFrom);
			Value vTo = ctx.GetArg(1);
			if (is_error(vTo)) return new IntrinsicResult(vTo);
			Value vStep = ctx.GetArg(2);
			if (is_error(vStep)) return new IntrinsicResult(vStep);
			double fromVal = numeric_val(vFrom);
			double toVal = numeric_val(vTo);
			double step;
			if (is_null(vStep) || !is_number(vStep)) {
				step = (toVal >= fromVal) ? 1 : -1;
			} else {
				step = numeric_val(vStep);
			}
			if (step == 0) {
				IOHelper.Print("ERROR: range() step must not be 0");
				return new IntrinsicResult(val_null);
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
		f.AddParam("seconds", val_one);
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

		// _
		//    Always equals the most recent implicit REPL result, i.e. _out[-1].
		//    Returns null if no implicit output has been produced yet.
		f = Intrinsic.Create("_");
		f.Code = (Context ctx, IntrinsicResult partialResult) => {
			Int32 n = list_count(replOutList);
			if (n == 0) return IntrinsicResult.Null;
			return new IntrinsicResult(list_get(replOutList, n - 1));
		};

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
			return IntrinsicResult.Null;
		};

	}

	public static void InvalidateTypeMaps() {
		_listType = val_null;
		_stringType = val_null;
		_mapType = val_null;
		_numberType = val_null;
		_functionType = val_null;
		_errorType = val_null;
		Intrinsic.ClearShortNames();
	}

	/*** BEGIN CPP_ONLY ***
	// GC mark callback to protect our static type maps from collection.
	void CoreIntrinsics::MarkRoots(void* user_data, GCManager& gc) {
		(void)user_data;
		gc.Mark(_listType);
		gc.Mark(_stringType);
		gc.Mark(_mapType);
		gc.Mark(_numberType);
		gc.Mark(_functionType);
		gc.Mark(_errorType);
		gc.Mark(replInList);
		gc.Mark(replOutList);
	}
	*** END CPP_ONLY ***/

}

}
