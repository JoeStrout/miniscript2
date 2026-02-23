// CoreIntrinsics.cs - Definitions of all built-in intrinsic functions.

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// H: #include "value.h"
// H: #include "Intrinsic.g.h"
// CPP: #include "gc.h"
// CPP: #include "value_list.h"
// CPP: #include "value_string.h"
// CPP: #include "value_map.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "VM.g.h"
// CPP: #include "CS_Math.h"
// CPP: #include <random>

namespace MiniScript {

public static class CoreIntrinsics {

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

	public static void Init() {
		Intrinsic f;

		// print(s="")
		f = Intrinsic.Create("print");
		f.AddParam("s", make_string(""));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			String output = StringUtils.Format("{0}", stk[bi + 1]);
			VM vm = VM.ActiveVM();
			if (vm.GetPrintCallback() != null) {  // CPP: if (VMStorage::sPrintCallback) {
				vm.GetPrintCallback()(output);    // CPP: VMStorage::sPrintCallback(output);
			} else {
				IOHelper.Print(output);
			}
			return make_null();
		};

		// input(prompt=null)
		f = Intrinsic.Create("input");
		f.AddParam("prompt");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			String prompt = new String("");
			if (!is_null(stk[bi + 1])) {
				prompt = StringUtils.Format("{0}", stk[bi + 1]);
			}
			String result = IOHelper.Input(prompt);
			return make_string(result);
		};

		// val(x)
		f = Intrinsic.Create("val");
		f.AddParam("x");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return to_number(stk[bi + 1]);
		};

		// str(x)
		f = Intrinsic.Create("str");
		f.AddParam("x");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_string(StringUtils.Format("{0}", stk[bi + 1]));
		};

		// len(x)
		f = Intrinsic.Create("len");
		f.AddParam("x");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value container = stk[bi + 1];
			Value result = make_null();
			if (is_list(container)) {
				result = make_int(list_count(container));
			} else if (is_string(container)) {
				result = make_int(string_length(container));
			} else if (is_map(container)) {
				result = make_int(map_count(container));
			}
			return result;
		};

		// remove(self, index)
		f = Intrinsic.Create("remove");
		f.AddParam("self");
		f.AddParam("index");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value container = stk[bi + 1];
			int result = 0;
			if (is_list(container)) {
				result = list_remove(container, as_int(stk[bi + 2])) ? 1 : 0;
			} else if (is_map(container)) {
				result = map_remove(container, stk[bi + 2]) ? 1 : 0;
			} else {
				IOHelper.Print("ERROR: `remove` must be called on list or map");
			}
			return make_int(result);
		};

		// freeze(x)
		f = Intrinsic.Create("freeze");
		f.AddParam("x");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			freeze_value(stk[bi + 1]);
			return make_null();
		};

		// isFrozen(x)
		f = Intrinsic.Create("isFrozen");
		f.AddParam("x");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_int(is_frozen(stk[bi + 1]));
		};

		// frozenCopy(x)
		f = Intrinsic.Create("frozenCopy");
		f.AddParam("x");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return frozen_copy(stk[bi + 1]);
		};

		// abs(x=0)
		f = Intrinsic.Create("abs");
		f.AddParam("x", make_int(0));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_double(Math.Abs(numeric_val(stk[bi + 1])));
		};

		// acos(x=0)
		f = Intrinsic.Create("acos");
		f.AddParam("x", make_int(0));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_double(Math.Acos(numeric_val(stk[bi + 1])));
		};

		// asin(x=0)
		f = Intrinsic.Create("asin");
		f.AddParam("x", make_int(0));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_double(Math.Asin(numeric_val(stk[bi + 1])));
		};

		// atan(y=0, x=1)
		f = Intrinsic.Create("atan");
		f.AddParam("y", make_int(0));
		f.AddParam("x", make_int(1));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			double y = numeric_val(stk[bi + 1]);
			double x = numeric_val(stk[bi + 2]);
			if (x == 1.0) return make_double(Math.Atan(y));
			return make_double(Math.Atan2(y, x));
		};

		// ceil(x=0)
		f = Intrinsic.Create("ceil");
		f.AddParam("x", make_int(0));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_double(Math.Ceiling(numeric_val(stk[bi + 1])));
		};

		// cos(radians=0)
		f = Intrinsic.Create("cos");
		f.AddParam("radians", make_int(0));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_double(Math.Cos(numeric_val(stk[bi + 1])));
		};

		// floor(x=0)
		f = Intrinsic.Create("floor");
		f.AddParam("x", make_int(0));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_double(Math.Floor(numeric_val(stk[bi + 1])));
		};

		// log(x=0, base=10)
		f = Intrinsic.Create("log");
		f.AddParam("x", make_int(0));
		f.AddParam("base", make_int(10));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			double x = numeric_val(stk[bi + 1]);
			double b = numeric_val(stk[bi + 2]);
			double result;
			if (Math.Abs(b - 2.718282) < 0.000001) result = Math.Log(x);
			else result = Math.Log(x) / Math.Log(b);
			return make_double(result);
		};

		// pi
		f = Intrinsic.Create("pi");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_double(Math.PI);
		};

		// round(x=0, decimalPlaces=0)
		f = Intrinsic.Create("round");
		f.AddParam("x", make_int(0));
		f.AddParam("decimalPlaces", make_int(0));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			double num = numeric_val(stk[bi + 1]);
			int decimalPlaces = (int)numeric_val(stk[bi + 2]);
			if (decimalPlaces >= 0) {
				if (decimalPlaces > 15) decimalPlaces = 15;
				num = Math.Round(num, decimalPlaces, MidpointRounding.AwayFromZero);
			} else {
				double pow10 = Math.Pow(10, -decimalPlaces);
				num = Math.Round(num / pow10, MidpointRounding.AwayFromZero) * pow10;
			}
			return make_double(num);
		};

		// rnd(seed)
		f = Intrinsic.Create("rnd");
		f.AddParam("seed");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			int seed = is_null(stk[bi + 1]) ? 0 : (int)numeric_val(stk[bi + 1]);
			return make_double(GetNextRandom(seed));
		};

		// sign(x=0)
		f = Intrinsic.Create("sign");
		f.AddParam("x", make_int(0));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_int(Math.Sign(numeric_val(stk[bi + 1])));
		};

		// sin(radians=0)
		f = Intrinsic.Create("sin");
		f.AddParam("radians", make_int(0));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_double(Math.Sin(numeric_val(stk[bi + 1])));
		};

		// sqrt(x=0)
		f = Intrinsic.Create("sqrt");
		f.AddParam("x", make_int(0));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_double(Math.Sqrt(numeric_val(stk[bi + 1])));
		};

		// tan(radians=0)
		f = Intrinsic.Create("tan");
		f.AddParam("radians", make_int(0));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_double(Math.Tan(numeric_val(stk[bi + 1])));
		};
	}

	static Random _random;  // CPP: // (omit)
}

}
