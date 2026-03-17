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
#include "IntrinsicAPI.g.h"
#include "CS_Math.h"
#include "CS_value_util.h"
#include <random>

namespace MiniScript {

	
double CoreIntrinsics::GetNextRandom(int seed) {
	static std::mt19937 gen(std::random_device{}());
	static std::uniform_real_distribution<double> dist(0.0, 1.0);
	if (seed != 0) gen.seed(static_cast<unsigned int>(seed));
	return dist(gen);
}
void CoreIntrinsics::AddIntrinsicToMap(Value map,String methodName) {
	Intrinsic intr = Intrinsic::GetByName(methodName);
	if (!IsNull(intr)) {
		map_set(map, make_string(methodName), intr.GetFunc());
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
	}
	return _mapType;
}
Value CoreIntrinsics::_mapType = val_null;
Value CoreIntrinsics::NumberType() {
	if (is_null(_numberType)) {
		_numberType = make_map(4);
	}
	return _numberType;
}
Value CoreIntrinsics::_numberType = val_null;
Value CoreIntrinsics::FunctionType() {
	if (is_null(_functionType)) {
		_functionType = make_map(4);
	}
	return _functionType;
}
Value CoreIntrinsics::_functionType = val_null;
void CoreIntrinsics::Init() {
	gc_register_mark_callback(CoreIntrinsics::MarkRoots, nullptr);

	Intrinsic f;

	// Garbace collection (GC) note:
	// The transpiler sees a bunch of Values below and figures that it
	// needs to do a GC_PUSH_SCOPE... but in fact those are all inside
	// lambda functions; there is no Value usage here in Init() itself.
	// The transpiler will emit a GC_POP_SCOPE at the end of this method,
	// and we can't easily prevent that.  But we can balance it with:
	GC_PUSH_SCOPE();
	// ...and yes, this is a bit of a hack.  TODO: make transpiler smarter.

	// print(s="")
	f = Intrinsic::Create("print");
	f.AddParam("s", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		String output = StringUtils::Format("{0}", ctx.GetArg(0));
		if (VMStorage::sPrintCallback) {
			VMStorage::sPrintCallback(output);
		} else {
			IOHelper::Print(output);
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

	// val(self=0)
	f = Intrinsic::Create("val");
	f.AddParam("self", make_int(0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value v = ctx.GetArg(0); GC_PROTECT(&v);
		if (is_number(v))  {
			GC_POP_SCOPE();
			return IntrinsicResult(v);
		}
		if (is_string(v))  {
			GC_POP_SCOPE();
			return IntrinsicResult(to_number(v));
		}
		GC_POP_SCOPE();
		return IntrinsicResult(val_null);
	});

	// str(x="")
	f = Intrinsic::Create("str");
	f.AddParam("x", make_string(""));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value v = ctx.GetArg(0); GC_PROTECT(&v);
		if (is_null(v))  {
			GC_POP_SCOPE();
			return IntrinsicResult(make_string(""));
		}
		GC_POP_SCOPE();
		return IntrinsicResult(make_string(StringUtils::Format("{0}", v)));
	});

	// upper(self)
	f = Intrinsic::Create("upper");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(string_upper(ctx.GetArg(0)));
	});

	// lower(self)
	f = Intrinsic::Create("lower");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(string_lower(ctx.GetArg(0)));
	});

	// char(codePoint=65)
	f = Intrinsic::Create("char");
	f.AddParam("codePoint", make_int(65));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		int codePoint = (int)numeric_val(ctx.GetArg(0));
		return IntrinsicResult(string_from_code_point(codePoint));
	});

	// code(self)
	f = Intrinsic::Create("code");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_int(string_code_point(ctx.GetArg(0))));
	});

	// len(x)
	f = Intrinsic::Create("len");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value container = ctx.GetArg(0); GC_PROTECT(&container);
		Value result = val_null; GC_PROTECT(&result);
		if (is_list(container)) {
			result = make_int(list_count(container));
		} else if (is_string(container)) {
			result = make_int(string_length(container));
		} else if (is_map(container)) {
			result = make_int(map_count(container));
		}
		GC_POP_SCOPE();
		return IntrinsicResult(result);
	});

	// remove(self, index)
	f = Intrinsic::Create("remove");
	f.AddParam("self");
	f.AddParam("index");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value container = ctx.GetArg(0); GC_PROTECT(&container);
		int result = 0;
		if (is_list(container)) {
			result = list_remove(container, as_int(ctx.GetArg(1))) ? 1 : 0;
		} else if (is_map(container)) {
			result = map_remove(container, ctx.GetArg(1)) ? 1 : 0;
		} else {
			IOHelper::Print("ERROR: `remove` must be called on list or map");
		}
		GC_POP_SCOPE();
		return IntrinsicResult(make_int(result));
	});

	// freeze(x)
	f = Intrinsic::Create("freeze");
	f.AddParam("x");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		freeze_value(ctx.GetArg(0));
		return IntrinsicResult(val_null);
	});

	// isFrozen(x)
	f = Intrinsic::Create("isFrozen");
	f.AddParam("x");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_int(is_frozen(ctx.GetArg(0))));
	});

	// frozenCopy(x)
	f = Intrinsic::Create("frozenCopy");
	f.AddParam("x");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(frozen_copy(ctx.GetArg(0)));
	});

	// abs(x=0)
	f = Intrinsic::Create("abs");
	f.AddParam("x", make_int(0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(Math::Abs(numeric_val(ctx.GetArg(0)))));
	});

	// acos(x=0)
	f = Intrinsic::Create("acos");
	f.AddParam("x", make_int(0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(Math::Acos(numeric_val(ctx.GetArg(0)))));
	});

	// asin(x=0)
	f = Intrinsic::Create("asin");
	f.AddParam("x", make_int(0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(Math::Asin(numeric_val(ctx.GetArg(0)))));
	});

	// atan(y=0, x=1)
	f = Intrinsic::Create("atan");
	f.AddParam("y", make_int(0));
	f.AddParam("x", make_int(1));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		double y = numeric_val(ctx.GetArg(0));
		double x = numeric_val(ctx.GetArg(1));
		if (x == 1.0) return IntrinsicResult(make_double(Math::Atan(y)));
		return IntrinsicResult(make_double(Math::Atan2(y, x)));
	});

	// ceil(x=0)
	f = Intrinsic::Create("ceil");
	f.AddParam("x", make_int(0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(Math::Ceiling(numeric_val(ctx.GetArg(0)))));
	});

	// cos(radians=0)
	f = Intrinsic::Create("cos");
	f.AddParam("radians", make_int(0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(Math::Cos(numeric_val(ctx.GetArg(0)))));
	});

	// floor(x=0)
	f = Intrinsic::Create("floor");
	f.AddParam("x", make_int(0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(Math::Floor(numeric_val(ctx.GetArg(0)))));
	});

	// log(x=0, base=10)
	f = Intrinsic::Create("log");
	f.AddParam("x", make_int(0));
	f.AddParam("base", make_int(10));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		double x = numeric_val(ctx.GetArg(0));
		double b = numeric_val(ctx.GetArg(1));
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
	f.AddParam("x", make_int(0));
	f.AddParam("decimalPlaces", make_int(0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		double num = numeric_val(ctx.GetArg(0));
		int decimalPlaces = (int)numeric_val(ctx.GetArg(1));
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
		int seed = is_null(ctx.GetArg(0)) ? 0 : (int)numeric_val(ctx.GetArg(0));
		return IntrinsicResult(make_double(GetNextRandom(seed)));
	});

	// sign(x=0)
	f = Intrinsic::Create("sign");
	f.AddParam("x", make_int(0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_int(Math::Sign(numeric_val(ctx.GetArg(0)))));
	});

	// sin(radians=0)
	f = Intrinsic::Create("sin");
	f.AddParam("radians", make_int(0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(Math::Sin(numeric_val(ctx.GetArg(0)))));
	});

	// sqrt(x=0)
	f = Intrinsic::Create("sqrt");
	f.AddParam("x", make_int(0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(Math::Sqrt(numeric_val(ctx.GetArg(0)))));
	});

	// tan(radians=0)
	f = Intrinsic::Create("tan");
	f.AddParam("radians", make_int(0));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(make_double(Math::Tan(numeric_val(ctx.GetArg(0)))));
	});
	// push(self, value)
	f = Intrinsic::Create("push");
	f.AddParam("self");
	f.AddParam("value");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		Value value = ctx.GetArg(1); GC_PROTECT(&value);
		if (is_list(self)) {
			list_push(self, value);
			GC_POP_SCOPE();
			return IntrinsicResult(self);
		} else if (is_map(self)) {
			map_set(self, value, make_int(1));
			GC_POP_SCOPE();
			return IntrinsicResult(self);
		}
		GC_POP_SCOPE();
		return IntrinsicResult(val_null);
	});

	// pop(self)
	f = Intrinsic::Create("pop");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		Value result = val_null; GC_PROTECT(&result);
		if (is_list(self)) {
			result = list_pop(self);
		} else if (is_map(self)) {
			if (map_count(self) == 0)  {
				GC_POP_SCOPE();
				return IntrinsicResult(val_null);
			}
			MapIterator iter = map_iterator(self);
			if (map_iterator_next(&iter, &result, nullptr)) {
				// remove key that was found
				map_remove(self, result);
			}
		}
		GC_POP_SCOPE();
		return IntrinsicResult(result);
	});

	// pull(self)
	f = Intrinsic::Create("pull");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		Value result = val_null; GC_PROTECT(&result);
		if (is_list(self)) {
			result = list_pull(self);
		} else if (is_map(self)) {
			if (map_count(self) == 0)  {
				GC_POP_SCOPE();
				return IntrinsicResult(val_null);
			}
			MapIterator iter = map_iterator(self);
			if (map_iterator_next(&iter, &result, nullptr)) {
				// remove key that was found
				map_remove(self, result);
			}
		}
		GC_POP_SCOPE();
		return IntrinsicResult(result);
	});

	// insert(self, index, value)
	f = Intrinsic::Create("insert");
	f.AddParam("self");
	f.AddParam("index");
	f.AddParam("value");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		int index = (int)numeric_val(ctx.GetArg(1));
		Value value = ctx.GetArg(2); GC_PROTECT(&value);
		if (is_list(self)) {
			list_insert(self, index, value);
			GC_POP_SCOPE();
			return IntrinsicResult(self);
		} else if (is_string(self)) {
			GC_POP_SCOPE();
			return IntrinsicResult(string_insert(self, index, value));
		}
		GC_POP_SCOPE();
		return IntrinsicResult(val_null);
	});

	// indexOf(self, value, after=null)
	f = Intrinsic::Create("indexOf");
	f.AddParam("self");
	f.AddParam("value");
	f.AddParam("after");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		Value value = ctx.GetArg(1); GC_PROTECT(&value);
		Value after = ctx.GetArg(2); GC_PROTECT(&after);
		Value result = val_null; GC_PROTECT(&result);
		Value iterKey, iterVal; GC_PROTECT(&iterKey); GC_PROTECT(&iterVal);
		if (is_list(self)) {
			int afterIdx = -1;
			if (!is_null(after)) {
				afterIdx = (int)numeric_val(after);
				if (afterIdx < -1) afterIdx += list_count(self);
			}
			int idx = list_indexOf(self, value, afterIdx);
			if (idx >= 0) result = make_int(idx);
		} else if (is_string(self)) {
			if (!is_string(value))  {
				GC_POP_SCOPE();
				return IntrinsicResult(val_null);
			}
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
		}
		GC_POP_SCOPE();
		return IntrinsicResult(result);
	});

	// sort(self, byKey=null, ascending=1)
	f = Intrinsic::Create("sort");
	f.AddParam("self");
	f.AddParam("byKey");
	f.AddParam("ascending", make_int(1));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		Value byKey = ctx.GetArg(1); GC_PROTECT(&byKey);
		bool ascending = is_truthy(ctx.GetArg(2));
		if (!is_list(self))  {
			GC_POP_SCOPE();
			return IntrinsicResult(self);
		}
		if (list_count(self) < 2)  {
			GC_POP_SCOPE();
			return IntrinsicResult(self);
		}
		if (is_null(byKey)) {
			list_sort(self, ascending);
		} else {
			list_sort_by_key(self, byKey, ascending);
		}
		GC_POP_SCOPE();
		return IntrinsicResult(self);
	});

	// shuffle(self)
	f = Intrinsic::Create("shuffle");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		Value temp; GC_PROTECT(&temp);
		Value iterKey, iterVal; GC_PROTECT(&iterKey); GC_PROTECT(&iterVal);
		if (is_list(self)) {
			if (is_frozen(self)) { ctx.vm.RaiseRuntimeError("Attempt to modify a frozen list");  {
				GC_POP_SCOPE();
				return IntrinsicResult(val_null); }
			}
			int count = list_count(self);
			for (int i = count - 1; i > 0; i--) {
				int j = (int)(GetNextRandom() * (i + 1));
				temp = list_get(self, i);
				list_set(self, i, list_get(self, j));
				list_set(self, j, temp);
			}
		} else if (is_map(self)) {
			if (is_frozen(self)) { ctx.vm.RaiseRuntimeError("Attempt to modify a frozen map");  {
				GC_POP_SCOPE();
				return IntrinsicResult(val_null); }
			}
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
				int j = (int)(GetNextRandom() * (i + 1));
				temp = vals[i];
				vals[i] = vals[j];
				vals[j] = temp;
			}
			for (int i = 0; i < count; i++) {
				map_set(self, keys[i], vals[i]);
			}
		}
		GC_POP_SCOPE();
		return IntrinsicResult(val_null);
	});

	// join(self, delimiter=" ")
	f = Intrinsic::Create("join");
	f.AddParam("self");
	f.AddParam("delimiter", make_string(" "));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		if (!is_list(self))  {
			GC_POP_SCOPE();
			return IntrinsicResult(self);
		}
		Value delim = ctx.GetArg(1); GC_PROTECT(&delim);
		String delimStr = is_null(delim) ? " " : to_String(delim);
		int count = list_count(self);
		List<String> parts =  List<String>::New(count);
		for (int i = 0; i < count; i++) {
			parts.Add(to_String(list_get(self, i)));
		}
		GC_POP_SCOPE();
		return IntrinsicResult(make_string(String::Join(delimStr, parts)));
	});

	// split(self, delimiter=" ", maxCount=-1)
	f = Intrinsic::Create("split");
	f.AddParam("self");
	f.AddParam("delimiter", make_string(" "));
	f.AddParam("maxCount", make_int(-1));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		if (!is_string(self))  {
			GC_POP_SCOPE();
			return IntrinsicResult(val_null);
		}
		Value delim = ctx.GetArg(1); GC_PROTECT(&delim);
		int maxCount = (int)numeric_val(ctx.GetArg(2));
		GC_POP_SCOPE();
		return IntrinsicResult(string_split_max(self, delim, maxCount));
	});

	// replace(self, oldval, newval, maxCount=null)
	f = Intrinsic::Create("replace");
	f.AddParam("self");
	f.AddParam("oldval");
	f.AddParam("newval");
	f.AddParam("maxCount");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		Value oldVal = ctx.GetArg(1); GC_PROTECT(&oldVal);
		Value newVal = ctx.GetArg(2); GC_PROTECT(&newVal);
		Value maxCountVal = ctx.GetArg(3); GC_PROTECT(&maxCountVal);
		Value iterKey, iterVal; GC_PROTECT(&iterKey); GC_PROTECT(&iterVal);
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
			GC_POP_SCOPE();
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
			GC_POP_SCOPE();
			return IntrinsicResult(self);
		} else if (is_string(self)) {
			GC_POP_SCOPE();
			return IntrinsicResult(string_replace_max(self, oldVal, newVal, maxCount));
		}
		GC_POP_SCOPE();
		return IntrinsicResult(val_null);
	});

	// sum(self)
	f = Intrinsic::Create("sum");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		Value iterVal; GC_PROTECT(&iterVal);
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
			GC_POP_SCOPE();
			return IntrinsicResult(make_int(0));
		}
		if (total == (int)total && total >= Int32MinValue && total <= Int32MaxValue) {
			GC_POP_SCOPE();
			return IntrinsicResult(make_int((int)total));
		}
		GC_POP_SCOPE();
		return IntrinsicResult(make_double(total));
	});

	// slice(seq, from=0, to=null)
	f = Intrinsic::Create("slice");
	f.AddParam("seq");
	f.AddParam("from", make_int(0));
	f.AddParam("to");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value seq = ctx.GetArg(0); GC_PROTECT(&seq);
		int fromIdx = (int)numeric_val(ctx.GetArg(1));
		if (is_list(seq)) {
			int count = list_count(seq);
			int toIdx = is_null(ctx.GetArg(2)) ? count : (int)numeric_val(ctx.GetArg(2));
			GC_POP_SCOPE();
			return IntrinsicResult(list_slice(seq, fromIdx, toIdx));
		} else if (is_string(seq)) {
			int slen = string_length(seq);
			int toIdx = is_null(ctx.GetArg(2)) ? slen : (int)numeric_val(ctx.GetArg(2));
			GC_POP_SCOPE();
			return IntrinsicResult(string_slice(seq, fromIdx, toIdx));
		}
		GC_POP_SCOPE();
		return IntrinsicResult(val_null);
	});

	// indexes(self)
	f = Intrinsic::Create("indexes");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		Value result = val_null; GC_PROTECT(&result);
		Value iterKey; GC_PROTECT(&iterKey);
		if (is_list(self)) {
			int count = list_count(self);
			result = make_list(count);
			for (int i = 0; i < count; i++) {
				list_push(result, make_int(i));
			}
			GC_POP_SCOPE();
			return IntrinsicResult(result);
		} else if (is_string(self)) {
			int slen = string_length(self);
			result = make_list(slen);
			for (int i = 0; i < slen; i++) {
				list_push(result, make_int(i));
			}
			GC_POP_SCOPE();
			return IntrinsicResult(result);
		} else if (is_map(self)) {
			result = make_list(map_count(self));
			MapIterator iter = map_iterator(self);
			while (map_iterator_next(&iter, &iterKey, nullptr)) {
				list_push(result, iterKey);
			}
		}
		GC_POP_SCOPE();
		return IntrinsicResult(result);
	});

	// hasIndex(self, index)
	f = Intrinsic::Create("hasIndex");
	f.AddParam("self");
	f.AddParam("index");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		Value index = ctx.GetArg(1); GC_PROTECT(&index);
		if (is_list(self)) {
			if (!is_number(index))  {
				GC_POP_SCOPE();
				return IntrinsicResult(make_int(0));
			}
			int i = (int)numeric_val(index);
			int count = list_count(self);
			GC_POP_SCOPE();
			return IntrinsicResult(make_int((i >= -count && i < count) ? 1 : 0));
		} else if (is_string(self)) {
			if (!is_number(index))  {
				GC_POP_SCOPE();
				return IntrinsicResult(make_int(0));
			}
			int i = (int)numeric_val(index);
			int slen = string_length(self);
			GC_POP_SCOPE();
			return IntrinsicResult(make_int((i >= -slen && i < slen) ? 1 : 0));
		} else if (is_map(self)) {
			GC_POP_SCOPE();
			return IntrinsicResult(make_int(map_has_key(self, index) ? 1 : 0));
		}
		GC_POP_SCOPE();
		return IntrinsicResult(val_null);
	});

	// values(self)
	f = Intrinsic::Create("values");
	f.AddParam("self");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		GC_PUSH_SCOPE();
		Value self = ctx.GetArg(0); GC_PROTECT(&self);
		Value result = self; GC_PROTECT(&result);
		Value iterVal; GC_PROTECT(&iterVal);
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
		}
		GC_POP_SCOPE();
		return IntrinsicResult(result);
	});

	// range(from=0, to=0, step=null)
	f = Intrinsic::Create("range");
	f.AddParam("from", make_int(0));
	f.AddParam("to", make_int(0));
	f.AddParam("step");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		double fromVal = numeric_val(ctx.GetArg(0));
		double toVal = numeric_val(ctx.GetArg(1));
		double step;
		if (is_null(ctx.GetArg(2)) || !is_number(ctx.GetArg(2))) {
			step = (toVal >= fromVal) ? 1 : -1;
		} else {
			step = numeric_val(ctx.GetArg(2));
		}
		if (step == 0) {
			IOHelper::Print("ERROR: range() step must not be 0");
			return IntrinsicResult(val_null);
		}
		int count = (int)((toVal - fromVal) / step) + 1;
		if (count < 0) count = 0;
		if (count > 1000000) count = 1000000;  // safety limit
		GC_PUSH_SCOPE();
		Value result = make_list(count); GC_PROTECT(&result);
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
		GC_POP_SCOPE();
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
	f.AddParam("seconds", make_int(1));
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		double now = ctx.vm.ElapsedTime();
		if (partialResult.done) {
			// Fresh call: calculate end time and return as partial result
			double interval = numeric_val(ctx.GetArg(0));
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

}
void CoreIntrinsics::InvalidateTypeMaps() {
	_listType = val_null;
	_stringType = val_null;
	_mapType = val_null;
	_numberType = val_null;
	_functionType = val_null;
}
// GC mark callback to protect our static type maps from collection.
void CoreIntrinsics::MarkRoots(void* user_data) {
	(void)user_data;
	gc_mark_value(_listType);
	gc_mark_value(_stringType);
	gc_mark_value(_mapType);
	gc_mark_value(_numberType);
	gc_mark_value(_functionType);
}

} // end of namespace MiniScript
