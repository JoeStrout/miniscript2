//*** BEGIN CS_ONLY ***
// (This entire file is only for C#; the C++ code uses value.h/.c instead.)

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

using static MiniScript.ValueHelpers;

namespace MiniScript {

// NOTE: Align the TAG MASK constants below with your C value.h.
// The layout mirrors a NaN-box: 64-bit payload that is either:
// - a real double (no matching tag), OR
// - an encoded immediate (null, tiny string), OR
// - a GC-managed object: GC_TAG | (gcSet << 32) | itemIndex
//
// GCSet assignments (must match GCManager constants):
//   STRING_SET  = 0  → 0xFFFE_0000_0000_XXXX
//   LIST_SET    = 1  → 0xFFFE_0001_0000_XXXX
//   MAP_SET     = 2  → 0xFFFE_0002_0000_XXXX
//   ERROR_SET   = 3  → 0xFFFE_0003_0000_XXXX
//   FUNCREF_SET = 4  → 0xFFFE_0004_0000_XXXX
//
// Keep Value at 8 bytes, blittable, and aggressively inlined.

[StructLayout(LayoutKind.Explicit, Size = 8)]
public readonly struct Value {
	[FieldOffset(0)] internal readonly ulong _u;
	[FieldOffset(0)] internal readonly double _d;

	internal Value(ulong u) { _d = 0; _u = u; }

	// ==== TAGS & MASKS =======================================================
	internal const ulong NANISH_MASK     = 0xFFFF_0000_0000_0000UL;
	internal const ulong NULL_VALUE      = 0xFFF1_0000_0000_0000UL;
	public  const ulong GC_TAG          = 0xFFFE_0000_0000_0000UL;
	internal const ulong TINY_STRING_TAG = 0xFFFF_0000_0000_0000UL;
	internal const ulong GC_TYPE_MASK    = NANISH_MASK | 0x0000_0007_0000_0000UL;

	[Obsolete("Use to_String(v, vm) instead")]
	public override string ToString() => ValueHelpers.to_String(this);

	// Equals/GetHashCode embody the same semantics as MiniScript ==, so that
	// Dictionary<Value, Value> hashes/compares keys correctly. In particular,
	// strings compare by content (not pointer/index).
	public override bool Equals(object obj) {
		if (obj is Value v) return ValueHelpers.value_equal(this, v);
		return false;
	}

	public override int GetHashCode() {
		if (ValueHelpers.is_string(this)) {
			string s = GCManager.GetStringContent(this);
			return s.GetHashCode(System.StringComparison.Ordinal);
		}
		return (int)(_u ^ (_u >> 32));
	}
}

// =============================================================================
// Global helper functions matching the C++ value.h interface
// =============================================================================
public static class ValueHelpers {

	// Common constant values
	public static Value val_null         = make_null();
	public static Value val_zero         = make_double(0.0);
	public static Value val_one          = make_double(1.0);
	public static Value val_empty_string = make_string("");
	public static Value val_isa_key      = make_string("__isa");
	public static Value val_self         = make_string("self");
	public static Value val_super        = make_string("super");

	// ── Raw bit access ───────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static ulong value_bits(Value v) => v._u;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int value_gc_set_index(Value v) => (int)((v._u >> 32) & 0x7);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int value_item_index(Value v) => (int)v._u;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int value_tiny_len(Value v) => (int)(v._u & 0xFF);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static void value_tiny_copy_to(Value v, Span<byte> dst) {
		int len = value_tiny_len(v);
		for (int i = 0; i < len; i++) dst[i] = (byte)((v._u >> (8 * (i + 1))) & 0xFF);
	}

	// ── Core value creation ──────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_null() => new Value(Value.NULL_VALUE);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_double(double d) {
		ulong bits = (ulong)BitConverter.DoubleToInt64Bits(d);
		return new Value(bits);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_int(int i) => make_double((double)i);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_int(bool b) => make_double(b ? 1.0 : 0.0);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_gc(int gcSet, int itemIdx) =>
		new Value(Value.GC_TAG | ((ulong)gcSet << 32) | (uint)itemIdx);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_string(string str) {
		if (str.Length <= 5) {
			// str.Length <= 5 guarantees UTF-8 bytes <= 20, so a 20-byte buffer suffices.
			Span<byte> buf = stackalloc byte[20];
			int byteCount = Encoding.UTF8.GetBytes(str, buf);
			if (byteCount <= 5) return make_tiny_utf8(buf.Slice(0, byteCount));
		}
		if (str.Length < GCManager.InternThreshold) return GCManager.InternString(str);
		return GCManager.NewString(str);
	}

	private static Value make_tiny_utf8(ReadOnlySpan<byte> utf8) {
		int len = utf8.Length;
		ulong u = Value.TINY_STRING_TAG | (ulong)((uint)len & 0xFFU);
		for (int i = 0; i < len; i++) u |= (ulong)utf8[i] << (8 * (i + 1));
		return new Value(u);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_list(int initial_capacity) => GCManager.NewList(initial_capacity);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_empty_list() => GCManager.NewList(0);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_empty_map() => GCManager.NewMap(8);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_map(int initial_capacity) => GCManager.NewMap(initial_capacity);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_funcref(Int32 funcIndex, Value outerVars) => GCManager.NewFuncRef(funcIndex, outerVars);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Int32 funcref_index(Value v) {
		if (!is_funcref(v)) return -1;
		return GCManager.Functions.Get(value_item_index(v)).FuncIndex;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value funcref_outer_vars(Value v) {
		if (!is_funcref(v)) return val_null;
		return GCManager.Functions.Get(value_item_index(v)).OuterVars;
	}

	// ── Error operations ─────────────────────────────────────────────────────

	public static Value make_error(Value message, Value inner, Value stack, Value isa) {
		if (is_null(stack)) {
			stack = make_list(0);
			freeze_value(stack);
		}
		return GCManager.NewError(message, inner, stack, isa);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value error_message(Value v) {
		if (!is_error(v)) return val_null;
		return GCManager.Errors.Get(value_item_index(v)).Message;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value error_inner(Value v) {
		if (!is_error(v)) return val_null;
		return GCManager.Errors.Get(value_item_index(v)).Inner;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value error_stack(Value v) {
		if (!is_error(v)) return val_null;
		return GCManager.Errors.Get(value_item_index(v)).Stack;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value error_isa(Value v) {
		if (!is_error(v)) return val_null;
		return GCManager.Errors.Get(value_item_index(v)).Isa;
	}

	public static bool error_isa_contains(Value err, Value target) {
		if (!is_error(err)) return false;
		Value current = GCManager.Errors.Get(value_item_index(err)).Isa;
		for (int depth = 0; depth < 256; depth++) {
			if (is_null(current)) return false;
			if (value_identical(current, target)) return true;
			if (!is_error(current)) return false;
			current = GCManager.Errors.Get(value_item_index(current)).Isa;
		}
		return false;
	}

	// ── Type predicates ──────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_null(Value v) => v._u == Value.NULL_VALUE;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_int(Value v) => false;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_double(Value v) => (v._u & Value.NANISH_MASK) < Value.NULL_VALUE;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_tiny_string(Value v) => (v._u & Value.NANISH_MASK) == Value.TINY_STRING_TAG;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_gc_object(Value v) => (v._u & Value.NANISH_MASK) == Value.GC_TAG;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_heap_string(Value v) {
		ulong masked = v._u & Value.GC_TYPE_MASK;
		return masked == (Value.GC_TAG | ((ulong)GCManager.BigStringSet      << 32))
		    || masked == (Value.GC_TAG | ((ulong)GCManager.InternedStringSet << 32));
	}
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_interned_string(Value v) =>
		(v._u & Value.GC_TYPE_MASK) == (Value.GC_TAG | ((ulong)GCManager.InternedStringSet << 32));
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_string(Value v) => is_tiny_string(v) || is_heap_string(v);
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_list(Value v) =>
		(v._u & Value.GC_TYPE_MASK) == (Value.GC_TAG | ((ulong)GCManager.ListSet    << 32));
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_map(Value v) =>
		(v._u & Value.GC_TYPE_MASK) == (Value.GC_TAG | ((ulong)GCManager.MapSet     << 32));
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_error(Value v) =>
		(v._u & Value.GC_TYPE_MASK) == (Value.GC_TAG | ((ulong)GCManager.ErrorSet   << 32));
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_funcref(Value v) =>
		(v._u & Value.GC_TYPE_MASK) == (Value.GC_TAG | ((ulong)GCManager.FunctionSet << 32));
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_number(Value v) => is_double(v);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_truthy(Value v) => !is_null(v) &&
		((is_double(v) && as_double(v) != 0.0) ||
		 (is_string(v) && string_length(v) != 0));

	// ── Numeric accessors ────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int as_int(Value v) => (int)as_double(v);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static double as_double(Value v) => v._d;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static double numeric_val(Value v) {
		if (is_double(v)) return as_double(v);
		if (is_error(v)) VM.ActiveVM().RaiseUncaughtError(v);
		return 0.0;
	}

	// ── String accessors ─────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static String as_cstring(Value v) {
		if (!is_string(v)) return "";
		return GetStringValue(v);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	internal static string GetStringValue(Value val) {
		if (is_tiny_string(val)) {
			int len = value_tiny_len(val);
			if (len == 0) return "";
			Span<byte> bytes = stackalloc byte[5];
			for (int i = 0; i < len; i++) bytes[i] = (byte)((val._u >> (8 * (i + 1))) & 0xFF);
			return Encoding.UTF8.GetString(bytes.Slice(0, len));
		}
		if (is_heap_string(val)) {
			GCStringSet set = (value_gc_set_index(val) == GCManager.InternedStringSet)
				? GCManager.InternedStrings : GCManager.BigStrings;
			return set.Get(value_item_index(val)).Data ?? "";
		}
		return "";
	}

	// ── Conversion ────────────────────────────────────────────────────────────

	// Returns the string representation of v as a C# string.
	// For a Value that is already a string this returns its content.
	// For other types this calls code_form with a recursion limit of 3.
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static String to_String(Value v, VM vm = null) {
		if (is_string(v)) return as_cstring(v);
		return code_form(v, vm, 3);
	}

	// Returns v converted to a string Value.
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value to_string(Value a, VM vm = null) {
		if (is_string(a)) return a;
		return make_string(code_form(a, vm, 3));
	}

	public static Value value_repr(Value v, VM vm = null) => make_string(code_form(v, vm));

	// Returns the printable/code representation of v as a C# string.
	public static string code_form(Value v, VM vm = null, int recursionLimit = -1) {
		if (is_null(v)) return "null";
		if (is_double(v)) {
			double value = as_double(v);
			if (value % 1.0 == 0.0) {
				string result = value.ToString("0", CultureInfo.InvariantCulture);
				if (result == "-0") result = "0";
				return result;
			} else if (value > 1E10 || value < -1E10 || (value < 1E-6 && value > -1E-6)) {
				string s = value.ToString("E6", CultureInfo.InvariantCulture);
				s = s.Replace("E-00", "E-0");
				return s;
			} else {
				string result = value.ToString("0.0#####", CultureInfo.InvariantCulture);
				if (result == "-0") result = "0";
				return result;
			}
		}
		if (is_string(v)) {
			String s = as_cstring(v);
			s = s.Replace("\"", "\"\"");
			return "\"" + s + "\"";
		}
		if (is_list(v)) {
			if (recursionLimit == 0) return "[...]";
			if (recursionLimit > 0 && recursionLimit < 3 && vm != null) {
				string shortName = vm.FindShortName(v);
				if (shortName != null) return shortName;
			}
			GCList list = GCManager.Lists.Get(value_item_index(v));
			var strs = new string[list.Items.Count];
			for (int i = 0; i < list.Items.Count; i++) {
				Value val_i = list.Get(i);
				strs[i] = is_null(val_i) ? "null" : code_form(val_i, vm, recursionLimit - 1);
			}
			return "[" + string.Join(", ", strs) + "]";
		}
		if (is_map(v)) {
			if (recursionLimit == 0) return "{...}";
			if (recursionLimit > 0 && recursionLimit < 3 && vm != null) {
				string shortName = vm.FindShortName(v);
				if (shortName != null) return shortName;
			}
			GCMap map = GCManager.Maps.Get(value_item_index(v));
			var strs = new List<string>(map.Count());
			for (int iter = map.NextEntry(-1); iter != -1; iter = map.NextEntry(iter)) {
				Value key = map.KeyAt(iter);
				Value val = map.ValueAt(iter);
				int nextRecurLimit = recursionLimit - 1;
				if (value_equal(key, val_isa_key)) nextRecurLimit = 1;
				strs.Add(string.Format("{0}: {1}", code_form(key, vm, nextRecurLimit),
					is_null(val) ? "null" : code_form(val, vm, nextRecurLimit)));
			}
			return "{" + String.Join(", ", strs) + "}";
		}
		if (is_funcref(v)) {
			GCFunction fr = GCManager.Functions.Get(value_item_index(v));
			if (fr.FuncIndex < 0) return "<funcref?>";
			return is_null(fr.OuterVars)
				? StringUtils.Format("FuncRef(#{0})", fr.FuncIndex)
				: StringUtils.Format("FuncRef(#{0}, closure)", fr.FuncIndex);
		}
		if (is_error(v)) {
			GCError err = GCManager.Errors.Get(value_item_index(v));
			string msg = is_string(err.Message) ? as_cstring(err.Message) : to_String(err.Message, vm);
			return "error: " + msg;
		}
		return "<value>";
	}

	public static Value to_number(Value a) {
		try { return make_double(double.Parse(to_String(a), CultureInfo.InvariantCulture)); }
		catch { return val_zero; }
	}

	// ── Arithmetic operations ─────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_add(Value a, Value b, VM vm = null) {
		if (is_error(a)) return a;
		if (is_error(b)) return b;
		if (is_double(a) && is_double(b)) return make_double(as_double(a) + as_double(b));
		if (is_string(a)) {
			if (is_null(b)) return a;
			if (is_string(b)) return string_concat(a, b);
			return string_concat(a, make_string(to_String(b, vm)));
		} else if (is_string(b)) {
			if (is_null(a)) return b;
			return string_concat(make_string(to_String(a, vm)), b);
		}
		if (is_list(a) && is_list(b)) return list_concat(a, b);
		if (is_map(a)  && is_map(b))  return map_concat(a, b);
		return val_null;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_mult(Value a, Value b) {
		if (is_error(a)) return a;
		if (is_error(b)) return b;
		if (is_double(a) && is_double(b)) return make_double(as_double(a) * as_double(b));
		if (is_string(a) && is_double(b)) {
			double factor = as_double(b);
			if (double.IsNaN(factor) || double.IsInfinity(factor)) return val_null;
			if (factor <= 0) return val_empty_string;
			int repeats = (int)factor;
			Value result = val_empty_string;
			for (int i = 0; i < repeats; i++) result = string_concat(result, a);
			int extraChars = (int)(string_length(a) * (factor - repeats));
			if (extraChars > 0) result = string_concat(result, string_substring(a, 0, extraChars));
			return result;
		}
		if (is_list(a) && is_double(b)) {
			double factor = as_double(b);
			if (double.IsNaN(factor) || double.IsInfinity(factor)) return val_null;
			int len = list_count(a);
			if (factor <= 0 || len == 0) return make_list(0);
			int fullCopies = (int)factor;
			int extraItems = (int)(len * (factor - fullCopies));
			Value result = make_list(fullCopies * len + extraItems);
			for (int c = 0; c < fullCopies; c++)
				for (int i = 0; i < len; i++) list_push(result, list_get(a, i));
			for (int i = 0; i < extraItems; i++) list_push(result, list_get(a, i));
			return result;
		}
		return val_null;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_div(Value a, Value b) {
		if (is_error(a)) return a;
		if (is_error(b)) return b;
		if (is_double(a) && is_double(b)) return make_double(as_double(a) / as_double(b));
		if (is_string(a) && is_double(b)) return value_mult(a, value_div(make_double(1), b));
		if (is_list(a) && is_double(b)) {
			double db = as_double(b);
			if (db == 0 || double.IsNaN(db) || double.IsInfinity(db)) return val_null;
			return value_mult(a, value_div(make_double(1), b));
		}
		return val_null;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_mod(Value a, Value b) {
		if (is_error(a)) return a;
		if (is_error(b)) return b;
		if (is_double(a) && is_double(b)) return make_double(as_double(a) % as_double(b));
		return val_null;
	}

	public static Value value_pow(Value a, Value b) {
		if (is_error(a)) return a;
		if (is_error(b)) return b;
		if (is_double(a) && is_double(b)) return make_double(Math.Pow(as_double(a), as_double(b)));
		return val_null;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_sub(Value a, Value b) {
		if (is_error(a)) return a;
		if (is_error(b)) return b;
		if (is_double(a) && is_double(b)) return make_double(as_double(a) - as_double(b));
		if (is_string(a) && is_string(b)) {
			string sa = as_cstring(a);
			string sb = as_cstring(b);
			if (sb.Length > 0 && sa.EndsWith(sb))
				return make_string(sa.Substring(0, sa.Length - sb.Length));
			return a;
		}
		return val_null;
	}

	// ── Fuzzy logic ───────────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Double ToFuzzyBool(Value v) {
		if (is_double(v)) return as_double(v);
		return is_truthy(v) ? 1.0 : 0.0;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Double AbsClamp01(Double d) {
		if (d < 0) d = -d;
		return d > 1 ? 1 : d;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_and(Value a, Value b) {
		if (is_error(a)) return a;
		if (is_error(b)) return b;
		return make_double(AbsClamp01(ToFuzzyBool(a) * ToFuzzyBool(b)));
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_or(Value a, Value b) {
		if (is_error(a)) return b;
		if (is_error(b)) return b;
		double fA = ToFuzzyBool(a), fB = ToFuzzyBool(b);
		return make_double(AbsClamp01(fA + fB - fA * fB));
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_not(Value a) {
		if (is_error(a)) return a;
		return make_double(1.0 - AbsClamp01(ToFuzzyBool(a)));
	}

	// ── Comparison ────────────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_identical(Value a, Value b) => a._u == b._u;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_lt(Value a, Value b) {
		if (is_double(a) && is_double(b)) return as_double(a) < as_double(b);
		if (is_string(a) && is_string(b)) return string_compare(a, b) < 0;
		return false;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_le(Value a, Value b) {
		if (is_double(a) && is_double(b)) return as_double(a) <= as_double(b);
		if (is_string(a) && is_string(b)) return string_compare(a, b) <= 0;
		return false;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_equal(Value a, Value b) {
		if (a._u == b._u) return true;
		if (is_double(a) && is_double(b)) return as_double(a) == as_double(b);
		if (is_tiny_string(a) && is_tiny_string(b)) return a._u == b._u;
		if (is_string(a) && is_string(b)) {
			return string.Equals(as_cstring(a), as_cstring(b), StringComparison.Ordinal);
		}
		if (is_null(a) || is_null(b)) return is_null(a) && is_null(b);
		if (is_list(a) && is_list(b)) {
			int countA = list_count(a);
			if (countA != list_count(b)) return false;
			for (int i = 0; i < countA; i++) {
				if (!value_equal(list_get(a, i), list_get(b, i))) return false;
			}
			return true;
		}
		if (is_map(a) && is_map(b)) {
			GCMap mapA = GCManager.Maps.Get(value_item_index(a));
			GCMap mapB = GCManager.Maps.Get(value_item_index(b));
			if (mapA.Count() != mapB.Count()) return false;
			for (int iter = mapA.NextEntry(-1); iter != -1; iter = mapA.NextEntry(iter)) {
				Value key = mapA.KeyAt(iter);
				Value val = mapA.ValueAt(iter);
				if (!mapB.TryGet(key, out Value bVal)) return false;
				if (!value_equal(val, bVal)) return false;
			}
			return true;
		}
		return false;
	}

	// ── List operations ──────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int list_count(Value list_val) {
		if (!is_list(list_val)) return 0;
		return GCManager.Lists.Get(value_item_index(list_val)).Items.Count;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value list_get(Value list_val, int index) {
		if (!is_list(list_val)) return val_null;
		return GCManager.Lists.Get(value_item_index(list_val)).Get(index);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static void list_set(Value list_val, int index, Value item) {
		if (!is_list(list_val)) return;
		GCList list = GCManager.Lists.Get(value_item_index(list_val));
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		list.Set(index, item);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static void list_push(Value list_val, Value item) {
		if (!is_list(list_val)) return;
		GCList list = GCManager.Lists.Get(value_item_index(list_val));
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		list.Push(item);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool list_remove(Value list_val, int index) {
		if (!is_list(list_val)) return false;
		GCList list = GCManager.Lists.Get(value_item_index(list_val));
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return false; }
		return list.Remove(index);
	}

	public static void list_insert(Value list_val, int index, Value item) {
		if (!is_list(list_val)) return;
		GCList list = GCManager.Lists.Get(value_item_index(list_val));
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		list.Insert(index, item);
	}

	public static Value list_pop(Value list_val) {
		if (!is_list(list_val)) return val_null;
		GCList list = GCManager.Lists.Get(value_item_index(list_val));
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return val_null; }
		return list.Pop();
	}

	public static Value list_pull(Value list_val) {
		if (!is_list(list_val)) return val_null;
		GCList list = GCManager.Lists.Get(value_item_index(list_val));
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return val_null; }
		return list.Pull();
	}

	public static int list_indexOf(Value list_val, Value item, int afterIdx) {
		if (!is_list(list_val)) return -1;
		return GCManager.Lists.Get(value_item_index(list_val)).IndexOf(item, afterIdx);
	}

	public static void list_sort(Value list_val, bool ascending) {
		if (!is_list(list_val)) return;
		GCList list = GCManager.Lists.Get(value_item_index(list_val));
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		SortList(list, ascending);
	}

	public static void list_sort_by_key(Value list_val, Value byKey, bool ascending) {
		if (!is_list(list_val)) return;
		GCList list = GCManager.Lists.Get(value_item_index(list_val));
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		SortListByKey(list, byKey, ascending);
	}

	public static Value list_slice(Value list_val, int start, int end) {
		int len = list_count(list_val);
		if (start < 0) start += len;
		if (end < 0)   end   += len;
		if (start < 0) start = 0;
		if (end > len) end   = len;
		if (start >= end) return make_list(0);
		Value result = make_list(end - start);
		for (int i = start; i < end; i++) list_push(result, list_get(list_val, i));
		return result;
	}

	public static Value list_concat(Value a, Value b) {
		int lenA = list_count(a);
		int lenB = list_count(b);
		Value result = make_list(lenA + lenB);
		for (int i = 0; i < lenA; i++) list_push(result, list_get(a, i));
		for (int i = 0; i < lenB; i++) list_push(result, list_get(b, i));
		return result;
	}

	private static void SortList(GCList list, bool ascending) {
		if (list.Items == null || list.Items.Count < 2) return;
		list.Items.Sort(Comparer<Value>.Create((a, b) => {
			int cmp;
			if (is_number(a) && is_number(b)) {
				cmp = numeric_val(a).CompareTo(numeric_val(b));
			} else if (is_string(a) && is_string(b)) {
				cmp = string_compare(a, b);
			} else {
				int ta = is_number(a) ? 0 : is_string(a) ? 1 : 2;
				int tb = is_number(b) ? 0 : is_string(b) ? 1 : 2;
				cmp = ta.CompareTo(tb);
			}
			return ascending ? cmp : -cmp;
		}));
	}

	private static void SortListByKey(GCList list, Value byKey, bool ascending) {
		int count = list.Items.Count;
		if (count < 2) return;
		Value[] keys = new Value[count];
		for (int i = 0; i < count; i++) {
			Value elem = list.Get(i);
			if (is_map(elem)) {
				keys[i] = map_get(elem, byKey);
			} else if (is_list(elem) && is_number(byKey)) {
				keys[i] = list_get(elem, (int)numeric_val(byKey));
			} else {
				keys[i] = val_null;
			}
		}
		int[] indices = new int[count];
		for (int i = 0; i < count; i++) indices[i] = i;
		Array.Sort(indices, (ia, ib) => {
			Value ka = keys[ia]; Value kb = keys[ib];
			int cmp;
			if (is_number(ka) && is_number(kb)) {
				cmp = numeric_val(ka).CompareTo(numeric_val(kb));
			} else if (is_string(ka) && is_string(kb)) {
				cmp = string_compare(ka, kb);
			} else {
				int ta = is_number(ka) ? 0 : is_string(ka) ? 1 : 2;
				int tb = is_number(kb) ? 0 : is_string(kb) ? 1 : 2;
				cmp = ta.CompareTo(tb);
			}
			return ascending ? cmp : -cmp;
		});
		Value[] sorted = new Value[count];
		for (int i = 0; i < count; i++) sorted[i] = list.Get(indices[i]);
		for (int i = 0; i < count; i++) list.Set(i, sorted[i]);
	}

	// ── Map operations ────────────────────────────────────────────────────────
	public static int map_count(Value map_val) {
		if (!is_map(map_val)) return 0;
		return GCManager.Maps.Get(value_item_index(map_val)).Count();
	}

	public static Value map_get(Value map_val, Value key) {
		if (!is_map(map_val)) return val_null;
		GCManager.Maps.Get(value_item_index(map_val)).TryGet(key, out Value result);
		return result;
	}

	public static bool map_try_get(Value map_val, Value key, out Value value) {
		value = val_null;
		if (!is_map(map_val)) return false;
		return GCManager.Maps.Get(value_item_index(map_val)).TryGet(key, out value);
	}

	public static bool map_lookup(Value map_val, Value key, out Value value) {
		value = val_null;
		Value isaKey = val_isa_key;
		Value current = map_val;
		for (Int32 depth = 0; depth < 256; depth++) {
			if (!is_map(current)) return false;
			GCMap m = GCManager.Maps.Get(value_item_index(current));
			if (m.TryGet(key, out value)) return true;
			if (!m.TryGet(isaKey, out Value isa)) return false;
			current = isa;
		}
		return false;
	}

	public static bool map_lookup_with_origin(Value map_val, Value key, out Value value, out Value superVal) {
		value    = val_null;
		superVal = val_null;
		Value isaKey = val_isa_key;
		Value current = map_val;
		for (Int32 depth = 0; depth < 256; depth++) {
			if (!is_map(current)) return false;
			GCMap m = GCManager.Maps.Get(value_item_index(current));
			if (m.TryGet(key, out value)) {
				m.TryGet(isaKey, out superVal);
				return true;
			}
			if (!m.TryGet(isaKey, out Value isa)) return false;
			current = isa;
		}
		return false;
	}

	public static bool map_set(Value map_val, Value key, Value value) {
		if (!is_map(map_val)) return false;
		GCMap m = GCManager.Maps.Get(value_item_index(map_val));
		if (m.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return false; }
		m.Set(key, value);
		return true;
	}

	public static bool map_remove(Value map_val, Value key) {
		if (!is_map(map_val)) return false;
		GCMap m = GCManager.Maps.Get(value_item_index(map_val));
		if (m.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return false; }
		return m.Remove(key);
	}

	public static bool map_has_key(Value map_val, Value key) {
		if (!is_map(map_val)) return false;
		return GCManager.Maps.Get(value_item_index(map_val)).HasKey(key);
	}

	public static void map_clear(Value map_val) {
		if (!is_map(map_val)) return;
		GCMap m = GCManager.Maps.Get(value_item_index(map_val));
		if (m.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return; }
		m.Clear();
	}

	public static Value map_nth_entry(Value map_val, int n) {
		if (!is_map(map_val)) return val_null;
		GCMap m = GCManager.Maps.Get(value_item_index(map_val));
		int count = 0;
		for (int iter = m.NextEntry(-1); iter != -1; iter = m.NextEntry(iter)) {
			if (count == n) {
				Value result = make_map(4);
				map_set(result, make_string("key"),   m.KeyAt(iter));
				map_set(result, make_string("value"), m.ValueAt(iter));
				return result;
			}
			count++;
		}
		return val_null;
	}

	// ── Map iteration ─────────────────────────────────────────────────────────
	// iter = -1: not started.  iter < -1: VarMap register entry index (see GCMap).
	// iter >= 0: hash-table slot.  MAP_ITER_DONE: exhausted.

	public const int MAP_ITER_DONE = Int32.MinValue;

	public static int map_iter_next(Value map_val, int iter) {
		if (!is_map(map_val) || iter == MAP_ITER_DONE) return MAP_ITER_DONE;
		int next = GCManager.Maps.Get(value_item_index(map_val)).NextEntry(iter);
		return next == -1 ? MAP_ITER_DONE : next;
	}

	public static Value map_iter_entry(Value map_val, int iter) {
		if (!is_map(map_val) || iter == MAP_ITER_DONE) return val_null;
		GCMap m = GCManager.Maps.Get(value_item_index(map_val));
		Value key = m.KeyAt(iter);
		Value val = m.ValueAt(iter);
		Value result = make_map(4);
		map_set(result, make_string("key"),   key);
		map_set(result, make_string("value"), val);
		return result;
	}

	// Legacy struct-based iterator (kept for any call sites that use it).
	public struct MapIterator {
		public int MapIndex;
		public int Iter;
		public Value Key;
		public Value Val;
	}

	public static MapIterator map_iterator(Value map_val) {
		MapIterator it = new MapIterator();
		it.Key = val_null;
		it.Val = val_null;
		if (is_map(map_val)) {
			it.MapIndex = value_item_index(map_val);
			it.Iter     = -1;
		} else {
			it.MapIndex = -1;
			it.Iter     = MAP_ITER_DONE;
		}
		return it;
	}

	public static bool map_iterator_next(ref MapIterator iter) {
		if (iter.MapIndex < 0 || iter.Iter == MAP_ITER_DONE) return false;
		int next = GCManager.Maps.Get(iter.MapIndex).NextEntry(iter.Iter);
		if (next == -1) { iter.Iter = MAP_ITER_DONE; return false; }
		iter.Iter = next;
		GCMap m = GCManager.Maps.Get(iter.MapIndex);
		iter.Key = m.KeyAt(next);
		iter.Val = m.ValueAt(next);
		return true;
	}

	// ── VarMap operations ─────────────────────────────────────────────────────
	public static Value make_varmap(List<Value> registers, List<Value> names, int baseIdx, int count) {
		return VarMapBacking.NewVarMap(registers, names, baseIdx, baseIdx + count - 1);
	}

	public static void varmap_gather(Value map_val) {
		if (!is_map(map_val)) return;
		Int32 idx = value_item_index(map_val);
		VarMapBacking vmb = GCManager.Maps.Get(idx)._vmb;
		if (vmb != null) vmb.Gather(idx);
	}

	public static void varmap_rebind(Value map_val, List<Value> registers, List<Value> names) {
		if (!is_map(map_val)) return;
		Int32 idx = value_item_index(map_val);
		VarMapBacking vmb = GCManager.Maps.Get(idx)._vmb;
		if (vmb != null) vmb.Rebind(idx, registers, names);
	}

	public static void varmap_map_to_register(Value map_val, Value varName, List<Value> registers, int regIndex) {
		if (!is_map(map_val)) return;
		Int32 idx = value_item_index(map_val);
		GCMap m = GCManager.Maps.Get(idx);
		if (m._vmb != null) m._vmb.MapToRegister(idx, varName, registers, regIndex);
	}

	// ── Freeze operations ─────────────────────────────────────────────────────
	public static bool is_frozen(Value v) {
		if (is_list(v)) return GCManager.Lists.Get(value_item_index(v)).Frozen;
		if (is_map(v))  return GCManager.Maps.Get(value_item_index(v)).Frozen;
		return false;
	}

	public static void freeze_value(Value v) {
		if (is_list(v)) {
			Int32 idx = value_item_index(v);
			GCList list = GCManager.Lists.Get(idx);
			if (list.Frozen) return;
			GCManager.Lists.SetFrozen(idx, true);
			for (int i = 0; i < list.Items.Count; i++) freeze_value(list.Get(i));
		} else if (is_map(v)) {
			Int32 idx = value_item_index(v);
			GCMap map = GCManager.Maps.Get(idx);
			if (map.Frozen) return;
			GCManager.Maps.SetFrozen(idx, true);
			for (int iter = map.NextEntry(-1); iter != -1; iter = map.NextEntry(iter)) {
				freeze_value(map.KeyAt(iter));
				freeze_value(map.ValueAt(iter));
			}
		}
	}

	public static Value frozen_copy(Value v) {
		if (is_list(v)) {
			GCList src = GCManager.Lists.Get(value_item_index(v));
			if (src.Frozen) return v;
			Value newList = make_list(src.Items.Count);
			Int32 dstIdx = value_item_index(newList);
			GCList dst = GCManager.Lists.Get(dstIdx);
			GCManager.Lists.SetFrozen(dstIdx, true);
			for (int i = 0; i < src.Items.Count; i++) dst.Push(frozen_copy(src.Get(i)));
			return newList;
		}
		if (is_map(v)) {
			GCMap src = GCManager.Maps.Get(value_item_index(v));
			if (src.Frozen) return v;
			Value newMap = make_map(src.Count());
			Int32 dstIdx = value_item_index(newMap);
			GCMap dst = GCManager.Maps.Get(dstIdx);
			GCManager.Maps.SetFrozen(dstIdx, true);
			for (int iter = src.NextEntry(-1); iter != -1; iter = src.NextEntry(iter)) {
				Value key = src.KeyAt(iter);
				Value val = src.ValueAt(iter);
				dst.Set(frozen_copy(key), frozen_copy(val));
			}
			return newMap;
		}
		return v;
	}

	// ── Map concat ────────────────────────────────────────────────────────────
	public static Value map_concat(Value a, Value b) {
		Value result = make_map(0);
		if (is_map(a)) {
			GCMap mapA = GCManager.Maps.Get(value_item_index(a));
			for (int iter = mapA.NextEntry(-1); iter != -1; iter = mapA.NextEntry(iter))
				map_set(result, mapA.KeyAt(iter), mapA.ValueAt(iter));
		}
		if (is_map(b)) {
			GCMap mapB = GCManager.Maps.Get(value_item_index(b));
			for (int iter = mapB.NextEntry(-1); iter != -1; iter = mapB.NextEntry(iter))
				map_set(result, mapB.KeyAt(iter), mapB.ValueAt(iter));
		}
		return result;
	}

	// ── String operations ─────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int string_length(Value v) {
		if (!is_string(v)) return 0;
		return GetStringValue(v).Length;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int string_indexOf(Value haystack, Value needle, int start_pos) {
		if (!is_string(haystack) || !is_string(needle)) return -1;
		return GetStringValue(haystack).IndexOf(GetStringValue(needle), start_pos, StringComparison.Ordinal);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value string_substring(Value str, int startIndex, int len) {
		string s = GetStringValue(str);
		if (startIndex < 0) startIndex += s.Length;
		return make_string(s.Substring(startIndex, len));
	}

	public static Value string_slice(Value str, int start, int end) {
		string s = GetStringValue(str);
		int len = s.Length;
		if (start < 0) start += len;
		if (end   < 0) end   += len;
		if (start < 0) start = 0;
		if (end > len) end   = len;
		if (start >= end) return val_empty_string;
		return make_string(s.Substring(start, end - start));
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value string_concat(Value a, Value b) {
		if (!is_string(a) || !is_string(b)) return val_null;
		return make_string(GetStringValue(a) + GetStringValue(b));
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int string_compare(Value a, Value b) {
		if (!is_string(a) || !is_string(b)) return 0;
		return string.Compare(GetStringValue(a), GetStringValue(b), StringComparison.Ordinal);
	}

	public static Value string_split(Value str, Value delimiter) {
		if (!is_string(str) || !is_string(delimiter)) return val_null;
		string s = GetStringValue(str);
		string delim = GetStringValue(delimiter);
		string[] parts;
		if (delim == "") {
			parts = new string[s.Length];
			for (int i = 0; i < s.Length; i++) parts[i] = s[i].ToString();
		} else {
			parts = s.Split(new string[] { delim }, StringSplitOptions.None);
		}
		Value list = make_list(parts.Length);
		foreach (string part in parts) list_push(list, make_string(part));
		return list;
	}

	public static Value string_replace(Value str, Value from, Value to) {
		if (!is_string(str) || !is_string(from) || !is_string(to)) return val_null;
		string s = GetStringValue(str);
		string fromStr = GetStringValue(from);
		if (fromStr == "" || !s.Contains(fromStr)) return str;
		return make_string(s.Replace(fromStr, GetStringValue(to)));
	}

	public static Value string_insert(Value str, int index, Value value, VM vm = null) {
		if (!is_string(str)) return str;
		string s = GetStringValue(str);
		string insertStr = to_String(value, vm);
		if (index < 0) index += s.Length + 1;
		if (index < 0) index = 0;
		if (index > s.Length) index = s.Length;
		return make_string(s.Insert(index, insertStr));
	}

	public static Value string_split_max(Value str, Value delimiter, int maxCount) {
		if (!is_string(str) || !is_string(delimiter)) return val_null;
		string s = GetStringValue(str);
		string delim = GetStringValue(delimiter);
		Value list = make_list(8);
		if (delim == "") {
			int count = 0;
			for (int i = 0; i < s.Length; i++) {
				if (maxCount > 0 && count >= maxCount - 1) {
					list_push(list, make_string(s.Substring(i)));
					return list;
				}
				list_push(list, make_string(s[i].ToString()));
				count++;
			}
			return list;
		}
		int pos = 0; int found = 0;
		while (pos <= s.Length) {
			int next = s.IndexOf(delim, pos, StringComparison.Ordinal);
			if (next < 0 || (maxCount > 0 && found >= maxCount - 1)) {
				list_push(list, make_string(s.Substring(pos)));
				break;
			}
			list_push(list, make_string(s.Substring(pos, next - pos)));
			pos = next + delim.Length;
			found++;
			if (pos > s.Length) break;
			if (pos == s.Length) { list_push(list, make_string("")); break; }
		}
		return list;
	}

	public static Value string_replace_max(Value str, Value from, Value to, int maxCount) {
		if (!is_string(str) || !is_string(from) || !is_string(to)) return val_null;
		string s = GetStringValue(str);
		string fromStr = GetStringValue(from);
		string toStr   = GetStringValue(to);
		if (fromStr == "") return str;
		int pos = 0; int count = 0;
		var sb = new System.Text.StringBuilder();
		while (pos < s.Length) {
			int next = s.IndexOf(fromStr, pos, StringComparison.Ordinal);
			if (next < 0 || (maxCount > 0 && count >= maxCount)) {
				sb.Append(s.Substring(pos));
				break;
			}
			sb.Append(s.Substring(pos, next - pos));
			sb.Append(toStr);
			pos = next + fromStr.Length;
			count++;
		}
		return make_string(sb.ToString());
	}

	public static Value string_upper(Value str) {
		if (!is_string(str)) return str;
		return make_string(GetStringValue(str).ToUpper());
	}

	public static Value string_lower(Value str) {
		if (!is_string(str)) return str;
		return make_string(GetStringValue(str).ToLower());
	}

	public static Value string_from_code_point(int codePoint) =>
		make_string(char.ConvertFromUtf32(codePoint));

	public static int string_code_point(Value str) {
		if (!is_string(str)) return 0;
		string s = GetStringValue(str);
		if (s.Length == 0) return 0;
		return char.ConvertToUtf32(s, 0);
	}
}

}
//*** END CS_ONLY ***
