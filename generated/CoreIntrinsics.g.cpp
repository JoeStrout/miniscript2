// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CoreIntrinsics.cs

#include "CoreIntrinsics.g.h"
#include "gc.h"
#include "value_list.h"
#include "value_string.h"
#include "value_map.h"
#include "IOHelper.g.h"
#include "StringUtils.g.h"
#include "VM.g.h"
#include "CS_Math.h"
#include <random>

namespace MiniScript {

double CoreIntrinsics::GetNextRandom(int seed) {
	static std::mt19937 gen(std::random_device{}());
	static std::uniform_real_distribution<double> dist(0.0, 1.0);
	if (seed != 0) gen.seed(static_cast<unsigned int>(seed));
	return dist(gen);
}
void CoreIntrinsics::Init() {
	Intrinsic f;

	// print(s="")
	f = Intrinsic::Create("print");
	f.AddParam("s", make_string(""));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		String output = StringUtils::Format("{0}", stk[bi + 1]);
		VM vm = VM::ActiveVM();
		if (VMStorage::sPrintCallback) {
			VMStorage::sPrintCallback(output);
		} else {
			IOHelper::Print(output);
		}
		return make_null();
	});

	// input(prompt=null)
	f = Intrinsic::Create("input");
	f.AddParam("prompt");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		String prompt =  String::New("");
		if (!is_null(stk[bi + 1])) {
			prompt = StringUtils::Format("{0}", stk[bi + 1]);
		}
		String result = IOHelper::Input(prompt);
		return make_string(result);
	});

	// val(x=0)
	f = Intrinsic::Create("val");
	f.AddParam("x", make_int(0));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		GC_PUSH_SCOPE();
		Value v = stk[bi + 1]; GC_PROTECT(&v);
		if (is_number(v))  {
			GC_POP_SCOPE();
			return v;
		}
		if (is_string(v))  {
			GC_POP_SCOPE();
			return to_number(v);
		}
		GC_POP_SCOPE();
		return make_null();
	});

	// str(x="")
	f = Intrinsic::Create("str");
	f.AddParam("x", make_string(""));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		GC_PUSH_SCOPE();
		Value v = stk[bi + 1]; GC_PROTECT(&v);
		if (is_null(v))  {
			GC_POP_SCOPE();
			return make_string("");
		}
		GC_POP_SCOPE();
		return make_string(StringUtils::Format("{0}", v));
	});

	// upper(self)
	f = Intrinsic::Create("upper");
	f.AddParam("self");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return string_upper(stk[bi + 1]);
	});

	// lower(self)
	f = Intrinsic::Create("lower");
	f.AddParam("self");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return string_lower(stk[bi + 1]);
	});

	// char(codePoint=65)
	f = Intrinsic::Create("char");
	f.AddParam("codePoint", make_int(65));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		int codePoint = (int)numeric_val(stk[bi + 1]);
		return string_from_code_point(codePoint);
	});

	// code(self)
	f = Intrinsic::Create("code");
	f.AddParam("self");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_int(string_code_point(stk[bi + 1]));
	});

	// len(x)
	f = Intrinsic::Create("len");
	f.AddParam("x");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		GC_PUSH_SCOPE();
		Value container = stk[bi + 1]; GC_PROTECT(&container);
		Value result = make_null(); GC_PROTECT(&result);
		if (is_list(container)) {
			result = make_int(list_count(container));
		} else if (is_string(container)) {
			result = make_int(string_length(container));
		} else if (is_map(container)) {
			result = make_int(map_count(container));
		}
		GC_POP_SCOPE();
		return result;
	});

	// remove(self, index)
	f = Intrinsic::Create("remove");
	f.AddParam("self");
	f.AddParam("index");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		GC_PUSH_SCOPE();
		Value container = stk[bi + 1]; GC_PROTECT(&container);
		int result = 0;
		if (is_list(container)) {
			result = list_remove(container, as_int(stk[bi + 2])) ? 1 : 0;
		} else if (is_map(container)) {
			result = map_remove(container, stk[bi + 2]) ? 1 : 0;
		} else {
			IOHelper::Print("ERROR: `remove` must be called on list or map");
		}
		GC_POP_SCOPE();
		return make_int(result);
	});

	// freeze(x)
	f = Intrinsic::Create("freeze");
	f.AddParam("x");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		freeze_value(stk[bi + 1]);
		return make_null();
	});

	// isFrozen(x)
	f = Intrinsic::Create("isFrozen");
	f.AddParam("x");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_int(is_frozen(stk[bi + 1]));
	});

	// frozenCopy(x)
	f = Intrinsic::Create("frozenCopy");
	f.AddParam("x");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return frozen_copy(stk[bi + 1]);
	});

	// abs(x=0)
	f = Intrinsic::Create("abs");
	f.AddParam("x", make_int(0));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_double(Math::Abs(numeric_val(stk[bi + 1])));
	});

	// acos(x=0)
	f = Intrinsic::Create("acos");
	f.AddParam("x", make_int(0));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_double(Math::Acos(numeric_val(stk[bi + 1])));
	});

	// asin(x=0)
	f = Intrinsic::Create("asin");
	f.AddParam("x", make_int(0));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_double(Math::Asin(numeric_val(stk[bi + 1])));
	});

	// atan(y=0, x=1)
	f = Intrinsic::Create("atan");
	f.AddParam("y", make_int(0));
	f.AddParam("x", make_int(1));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		double y = numeric_val(stk[bi + 1]);
		double x = numeric_val(stk[bi + 2]);
		if (x == 1.0) return make_double(Math::Atan(y));
		return make_double(Math::Atan2(y, x));
	});

	// ceil(x=0)
	f = Intrinsic::Create("ceil");
	f.AddParam("x", make_int(0));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_double(Math::Ceiling(numeric_val(stk[bi + 1])));
	});

	// cos(radians=0)
	f = Intrinsic::Create("cos");
	f.AddParam("radians", make_int(0));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_double(Math::Cos(numeric_val(stk[bi + 1])));
	});

	// floor(x=0)
	f = Intrinsic::Create("floor");
	f.AddParam("x", make_int(0));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_double(Math::Floor(numeric_val(stk[bi + 1])));
	});

	// log(x=0, base=10)
	f = Intrinsic::Create("log");
	f.AddParam("x", make_int(0));
	f.AddParam("base", make_int(10));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		double x = numeric_val(stk[bi + 1]);
		double b = numeric_val(stk[bi + 2]);
		double result;
		if (Math::Abs(b - 2.718282) < 0.000001) result = Math::Log(x);
		else result = Math::Log(x) / Math::Log(b);
		return make_double(result);
	});

	// pi
	f = Intrinsic::Create("pi");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_double(Math::PI);
	});

	// round(x=0, decimalPlaces=0)
	f = Intrinsic::Create("round");
	f.AddParam("x", make_int(0));
	f.AddParam("decimalPlaces", make_int(0));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		double num = numeric_val(stk[bi + 1]);
		int decimalPlaces = (int)numeric_val(stk[bi + 2]);
		if (decimalPlaces >= 0) {
			if (decimalPlaces > 15) decimalPlaces = 15;
			num = Math::Round(num, decimalPlaces);
		} else {
			double pow10 = Math::Pow(10, -decimalPlaces);
			num /= pow10;
			num = Math::Round(num);
			num *= pow10;
		}
		return make_double(num);
	});

	// rnd(seed)
	f = Intrinsic::Create("rnd");
	f.AddParam("seed");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		int seed = is_null(stk[bi + 1]) ? 0 : (int)numeric_val(stk[bi + 1]);
		return make_double(GetNextRandom(seed));
	});

	// sign(x=0)
	f = Intrinsic::Create("sign");
	f.AddParam("x", make_int(0));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_int(Math::Sign(numeric_val(stk[bi + 1])));
	});

	// sin(radians=0)
	f = Intrinsic::Create("sin");
	f.AddParam("radians", make_int(0));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_double(Math::Sin(numeric_val(stk[bi + 1])));
	});

	// sqrt(x=0)
	f = Intrinsic::Create("sqrt");
	f.AddParam("x", make_int(0));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_double(Math::Sqrt(numeric_val(stk[bi + 1])));
	});

	// tan(radians=0)
	f = Intrinsic::Create("tan");
	f.AddParam("radians", make_int(0));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_double(Math::Tan(numeric_val(stk[bi + 1])));
	});
}
	// (omit)

} // end of namespace MiniScript
