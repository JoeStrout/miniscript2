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
// CPP: #include "CS_value_util.h"
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

	private static Value _listType = val_null;

	private static void AddIntrinsicToMap(Value map, String methodName) {
		Intrinsic intr = Intrinsic.GetByName(methodName);
		if (intr != null) {
			map_set(_listType, make_string(methodName), intr.GetFunc());
		} else {
			IOHelper.Print(StringUtils.Format("Intrinsic not found: {0}", methodName));
		}
	}

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
		}
		return _listType;
	}

	public static void Init() {
		Intrinsic f;

		// Garbace collection (GC) note:
		// The transpiler sees a bunch of Values below and figures that it
		// needs to do a GC_PUSH_SCOPE... but in fact those are all inside
		// lambda functions; there is no Value usage here in Init() itself.
		// The transpiler will emit a GC_POP_SCOPE at the end of this method,
		// and we can't easily prevent that.  But we can balance it with:
		// CPP: GC_PUSH_SCOPE();
		// ...and yes, this is a bit of a hack.  TODO: make transpiler smarter.

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

		// val(x=0)
		f = Intrinsic.Create("val");
		f.AddParam("x", make_int(0));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value v = stk[bi + 1];
			if (is_number(v)) return v;
			if (is_string(v)) return to_number(v);
			return make_null();
		};

		// str(x="")
		f = Intrinsic.Create("str");
		f.AddParam("x", make_string(""));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value v = stk[bi + 1];
			if (is_null(v)) return make_string("");
			return make_string(StringUtils.Format("{0}", v));
		};

		// upper(self)
		f = Intrinsic.Create("upper");
		f.AddParam("self");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return string_upper(stk[bi + 1]);
		};

		// lower(self)
		f = Intrinsic.Create("lower");
		f.AddParam("self");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return string_lower(stk[bi + 1]);
		};

		// char(codePoint=65)
		f = Intrinsic.Create("char");
		f.AddParam("codePoint", make_int(65));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			int codePoint = (int)numeric_val(stk[bi + 1]);
			return string_from_code_point(codePoint);
		};

		// code(self)
		f = Intrinsic.Create("code");
		f.AddParam("self");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_int(string_code_point(stk[bi + 1]));
		};

		// len(x)
		f = Intrinsic.Create("len");
		f.AddParam("self");
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
				num = Math.Round(num, decimalPlaces, MidpointRounding.AwayFromZero); // CPP: num = Math::Round(num, decimalPlaces);
			} else {
				double pow10 = Math.Pow(10, -decimalPlaces);
				num /= pow10;
				num = Math.Round(num, MidpointRounding.AwayFromZero); // CPP: num = Math::Round(num);
				num *= pow10;
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
		// push(self, value)
		f = Intrinsic.Create("push");
		f.AddParam("self");
		f.AddParam("value");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			Value value = stk[bi + 2];
			if (is_list(self)) {
				list_push(self, value);
				return self;
			} else if (is_map(self)) {
				map_set(self, value, make_int(1));
				return self;
			}
			return make_null();
		};

		// pop(self)
		f = Intrinsic.Create("pop");
		f.AddParam("self");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			Value result = make_null();
			if (is_list(self)) {
				result = list_pop(self);
			} else if (is_map(self)) {
				if (map_count(self) == 0) return make_null();
				MapIterator iter = map_iterator(self);
				if (map_iterator_next(ref iter)) { // CPP: if (map_iterator_next(&iter, &result, nullptr)) {
					result = iter.Key;             // CPP: // remove key that was found
					map_remove(self, result);
				}
			}
			return result;
		};

		// pull(self)
		f = Intrinsic.Create("pull");
		f.AddParam("self");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			Value result = make_null();
			if (is_list(self)) {
				result = list_pull(self);
			} else if (is_map(self)) {
				if (map_count(self) == 0) return make_null();
				MapIterator iter = map_iterator(self);
				if (map_iterator_next(ref iter)) { // CPP: if (map_iterator_next(&iter, &result, nullptr)) {
					result = iter.Key;             // CPP: // remove key that was found
					map_remove(self, result);
				}
			}
			return result;
		};

		// insert(self, index, value)
		f = Intrinsic.Create("insert");
		f.AddParam("self");
		f.AddParam("index");
		f.AddParam("value");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			int index = (int)numeric_val(stk[bi + 2]);
			Value value = stk[bi + 3];
			if (is_list(self)) {
				list_insert(self, index, value);
				return self;
			} else if (is_string(self)) {
				return string_insert(self, index, value);
			}
			return make_null();
		};

		// indexOf(self, value, after=null)
		f = Intrinsic.Create("indexOf");
		f.AddParam("self");
		f.AddParam("value");
		f.AddParam("after");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			Value value = stk[bi + 2];
			Value after = stk[bi + 3];
			Value result = make_null();
			// CPP: Value iterKey, iterVal; GC_PROTECT(&iterKey); GC_PROTECT(&iterVal);
			if (is_list(self)) {
				int afterIdx = -1;
				if (!is_null(after)) {
					afterIdx = (int)numeric_val(after);
					if (afterIdx < -1) afterIdx += list_count(self);
				}
				int idx = list_indexOf(self, value, afterIdx);
				if (idx >= 0) result = make_int(idx);
			} else if (is_string(self)) {
				if (!is_string(value)) return make_null();
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
			return result;
		};

		// sort(self, byKey=null, ascending=1)
		f = Intrinsic.Create("sort");
		f.AddParam("self");
		f.AddParam("byKey");
		f.AddParam("ascending", make_int(1));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			Value byKey = stk[bi + 2];
			bool ascending = numeric_val(stk[bi + 3]) != 0;
			if (!is_list(self)) return self;
			if (list_count(self) < 2) return self;
			if (is_null(byKey)) {
				list_sort(self, ascending);
			} else {
				list_sort_by_key(self, byKey, ascending);
			}
			return self;
		};

		// shuffle(self)
		f = Intrinsic.Create("shuffle");
		f.AddParam("self");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			Value temp;
			// CPP: Value iterKey, iterVal; GC_PROTECT(&iterKey); GC_PROTECT(&iterVal);
			if (is_list(self)) {
				if (is_frozen(self)) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return make_null(); }
				int count = list_count(self);
				for (int i = count - 1; i > 0; i--) {
					int j = (int)(GetNextRandom() * (i + 1));
					temp = list_get(self, i);
					list_set(self, i, list_get(self, j));
					list_set(self, j, temp);
				}
			} else if (is_map(self)) {
				if (is_frozen(self)) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return make_null(); }
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
			return make_null();
		};

		// join(self, delimiter=" ")
		f = Intrinsic.Create("join");
		f.AddParam("self");
		f.AddParam("delimiter", make_string(" "));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			if (!is_list(self)) return self;
			Value delim = stk[bi + 2];
			String delimStr = is_null(delim) ? " " : to_String(delim);
			int count = list_count(self);
			List<String> parts = new List<String>(count);
			for (int i = 0; i < count; i++) {
				parts.Add(to_String(list_get(self, i)));
			}
			return make_string(String.Join(delimStr, parts));
		};

		// split(self, delimiter=" ", maxCount=-1)
		f = Intrinsic.Create("split");
		f.AddParam("self");
		f.AddParam("delimiter", make_string(" "));
		f.AddParam("maxCount", make_int(-1));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			if (!is_string(self)) return make_null();
			Value delim = stk[bi + 2];
			int maxCount = (int)numeric_val(stk[bi + 3]);
			return string_split_max(self, delim, maxCount);
		};

		// replace(self, oldval, newval, maxCount=null)
		f = Intrinsic.Create("replace");
		f.AddParam("self");
		f.AddParam("oldval");
		f.AddParam("newval");
		f.AddParam("maxCount");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			Value oldVal = stk[bi + 2];
			Value newVal = stk[bi + 3];
			// CPP: Value iterKey, iterVal; GC_PROTECT(&iterKey); GC_PROTECT(&iterVal);
			int maxCount = is_null(stk[bi + 4]) ? -1 : (int)numeric_val(stk[bi + 4]);
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
				return self;
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
				return self;
			} else if (is_string(self)) {
				return string_replace_max(self, oldVal, newVal, maxCount);
			}
			return make_null();
		};

		// sum(self)
		f = Intrinsic.Create("sum");
		f.AddParam("self");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			// CPP: Value iterVal; GC_PROTECT(&iterVal);
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
				return make_int(0);
			}
			if (total == (int)total && total >= Int32.MinValue && total <= Int32.MaxValue) {
				return make_int((int)total);
			}
			return make_double(total);
		};

		// slice(seq, from=0, to=null)
		f = Intrinsic.Create("slice");
		f.AddParam("seq");
		f.AddParam("from", make_int(0));
		f.AddParam("to");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value seq = stk[bi + 1];
			int fromIdx = (int)numeric_val(stk[bi + 2]);
			if (is_list(seq)) {
				int count = list_count(seq);
				int toIdx = is_null(stk[bi + 3]) ? count : (int)numeric_val(stk[bi + 3]);
				return list_slice(seq, fromIdx, toIdx);
			} else if (is_string(seq)) {
				int slen = string_length(seq);
				int toIdx = is_null(stk[bi + 3]) ? slen : (int)numeric_val(stk[bi + 3]);
				return string_slice(seq, fromIdx, toIdx);
			}
			return make_null();
		};

		// indexes(self)
		f = Intrinsic.Create("indexes");
		f.AddParam("self");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			Value result = make_null();
			// CPP: Value iterKey; GC_PROTECT(&iterKey);
			if (is_list(self)) {
				int count = list_count(self);
				result = make_list(count);
				for (int i = 0; i < count; i++) {
					list_push(result, make_int(i));
				}
				return result;
			} else if (is_string(self)) {
				int slen = string_length(self);
				result = make_list(slen);
				for (int i = 0; i < slen; i++) {
					list_push(result, make_int(i));
				}
				return result;
			} else if (is_map(self)) {
				result = make_list(map_count(self));
				MapIterator iter = map_iterator(self);
				while (map_iterator_next(ref iter)) { // CPP: while (map_iterator_next(&iter, &iterKey, nullptr)) {
					list_push(result, iter.Key);  // CPP: list_push(result, iterKey);
				}
			}
			return result;
		};

		// hasIndex(self, index)
		f = Intrinsic.Create("hasIndex");
		f.AddParam("self");
		f.AddParam("index");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			Value index = stk[bi + 2];
			if (is_list(self)) {
				if (!is_number(index)) return make_int(0);
				int i = (int)numeric_val(index);
				int count = list_count(self);
				return make_int((i >= -count && i < count) ? 1 : 0);
			} else if (is_string(self)) {
				if (!is_number(index)) return make_int(0);
				int i = (int)numeric_val(index);
				int slen = string_length(self);
				return make_int((i >= -slen && i < slen) ? 1 : 0);
			} else if (is_map(self)) {
				return make_int(map_has_key(self, index) ? 1 : 0);
			}
			return make_null();
		};

		// values(self)
		f = Intrinsic.Create("values");
		f.AddParam("self");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value self = stk[bi + 1];
			Value result = self;
			// CPP: Value iterVal; GC_PROTECT(&iterVal);
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
			return result;
		};

		// range(from=0, to=0, step=null)
		f = Intrinsic.Create("range");
		f.AddParam("from", make_int(0));
		f.AddParam("to", make_int(0));
		f.AddParam("step");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			double fromVal = numeric_val(stk[bi + 1]);
			double toVal = numeric_val(stk[bi + 2]);
			double step;
			if (is_null(stk[bi + 3]) || !is_number(stk[bi + 3])) {
				step = (toVal >= fromVal) ? 1 : -1;
			} else {
				step = numeric_val(stk[bi + 3]);
			}
			if (step == 0) {
				IOHelper.Print("ERROR: range() step must not be 0");
				return make_null();
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
			return result;
		};

		// list
		//    Returns a map that represents the list datatype in
		//    MiniScript's core type system.  This can be used with `isa`
		//    to check whether a variable refers to a list.  You can also
		//    assign new methods here to make them available to all lists.
		f = Intrinsic.Create("list");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return ListType();
		};
	}

}

}
