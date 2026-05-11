//*** BEGIN CS_ONLY ***
// (This entire file is only for C#; the C++ code uses value.h/.c instead.)

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

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
	[FieldOffset(0)] private readonly ulong _u;
	[FieldOffset(0)] private readonly double _d;

	private Value(ulong u) { _d = 0; _u = u; }

	// ==== TAGS & MASKS =======================================================
	private const ulong NANISH_MASK     = 0xFFFF_0000_0000_0000UL;
	private const ulong NULL_VALUE      = 0xFFF1_0000_0000_0000UL;
	// Bits 34-32 carry the GCSet index (0-7); bits 31-0 carry the item index.
	public  const ulong GC_TAG          = 0xFFFE_0000_0000_0000UL;
	private const ulong TINY_STRING_TAG = 0xFFFF_0000_0000_0000UL;
	// Mask that covers the top 16-bit tag plus the 3-bit GCSet field.
	private const ulong GC_TYPE_MASK    = NANISH_MASK | 0x0000_0007_0000_0000UL;

	// ==== CONSTRUCTORS =======================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value Null() => new(NULL_VALUE);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value FromInt(int i) => FromDouble((double)i);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value FromDouble(double d) {
		ulong bits = (ulong)BitConverter.DoubleToInt64Bits(d);
		return new(bits);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value FromTinyAscii(ReadOnlySpan<byte> s) {
		int len = s.Length;
		if ((uint)len > 5u) throw new ArgumentOutOfRangeException(nameof(s), "Tiny string max 5 bytes");
		ulong u = TINY_STRING_TAG | (ulong)((uint)len & 0xFFU);
		for (int i = 0; i < len; i++) u |= (ulong)((byte)s[i]) << (8 * (i + 1));
		return new(u);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value MakeGC(int gcSet, int itemIdx) =>
		new(GC_TAG | ((ulong)gcSet << 32) | (uint)itemIdx);

	// Legacy factory kept for compatibility (used by some C# callers).
	// New code should call make_string / make_list / etc. from ValueHelpers.
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value FromString(string s) {
		if (s.Length <= 5 && IsAllAscii(s)) {
			Span<byte> tmp = stackalloc byte[5];
			for (int i = 0; i < s.Length; i++) tmp[i] = (byte)s[i];
			return FromTinyAscii(tmp[..s.Length]);
		}
		return gc.NewString(s);
	}

	// ==== TYPE PREDICATES ====================================================
	public bool IsNull     { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => _u == NULL_VALUE; }
	public bool IsTiny     { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) == TINY_STRING_TAG; }
	public bool IsGCObject { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) == GC_TAG; }
	public bool IsDouble   { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) < NULL_VALUE; }

	// Per-type GC checks: single AND + compare using GC_TYPE_MASK.
	public bool IsString  { [MethodImpl(MethodImplOptions.AggressiveInlining)] get =>
		IsTiny || (_u & GC_TYPE_MASK) == (GC_TAG | ((ulong)GCManager.STRING_SET  << 32)); }
	public bool IsList    { [MethodImpl(MethodImplOptions.AggressiveInlining)] get =>
		(_u & GC_TYPE_MASK) == (GC_TAG | ((ulong)GCManager.LIST_SET    << 32)); }
	public bool IsMap     { [MethodImpl(MethodImplOptions.AggressiveInlining)] get =>
		(_u & GC_TYPE_MASK) == (GC_TAG | ((ulong)GCManager.MAP_SET     << 32)); }
	public bool IsError   { [MethodImpl(MethodImplOptions.AggressiveInlining)] get =>
		(_u & GC_TYPE_MASK) == (GC_TAG | ((ulong)GCManager.ERROR_SET   << 32)); }
	public bool IsFuncRef { [MethodImpl(MethodImplOptions.AggressiveInlining)] get =>
		(_u & GC_TYPE_MASK) == (GC_TAG | ((ulong)GCManager.FUNCREF_SET << 32)); }

	// Kept for compatibility — true for non-tiny heap strings.
	public bool IsHeapString { [MethodImpl(MethodImplOptions.AggressiveInlining)] get =>
		(_u & GC_TYPE_MASK) == (GC_TAG | ((ulong)GCManager.STRING_SET  << 32)); }

	// ==== ACCESSORS ==========================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public int AsInt() => (int)AsDouble();

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public double AsDouble() {
		long bits = (long)_u;
		return BitConverter.Int64BitsToDouble(bits);
	}

	// GCSet index (0-4); only meaningful when IsGCObject.
	public int GCSetIndex { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (int)((_u >> 32) & 0x7); }

	// Item index within its GCSet; only meaningful when IsGCObject.
	public int ItemIndex  { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (int)_u; }

	// Alias for ItemIndex; kept for call-site compatibility.
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public int Handle() => (int)_u;

	// Tiny-string helpers
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public int TinyLen() => (int)(_u & 0xFF);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public void TinyCopyTo(Span<byte> dst) {
		int len = TinyLen();
		for (int i = 0; i < len; i++) dst[i] = (byte)((_u >> (8 * (i + 1))) & 0xFF);
	}

	public String ToString(VM vm) {
		if (IsString) return as_cstring(this);
		return CodeForm(vm, 3);
	}

	[Obsolete("Pass the VM context: use ToString(vm) instead")]
	public override string ToString() => ToString(null);

	public string CodeForm(VM vm, int recursionLimit = -1) {
		if (IsNull) return "null";
		if (IsDouble) {
			double value = AsDouble();
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
		if (IsString) {
			String s = as_cstring(this);
			s = s.Replace("\"", "\"\"");
			return "\"" + s + "\"";
		}
		if (IsList) {
			if (recursionLimit == 0) return "[...]";
			if (recursionLimit > 0 && recursionLimit < 3 && vm != null) {
				string shortName = vm.FindShortName(this);
				if (shortName != null) return shortName;
			}
			ref GCList list = ref gc.Lists.Get(ItemIndex);
			var strs = new string[list.Items.Count];
			for (int i = 0; i < list.Items.Count; i++) {
				Value val_i = list.Get(i);
				strs[i] = val_i.IsNull ? "null" : val_i.CodeForm(vm, recursionLimit - 1);
			}
			return "[" + string.Join(", ", strs) + "]";
		}
		if (IsMap) {
			if (recursionLimit == 0) return "{...}";
			if (recursionLimit > 0 && recursionLimit < 3 && vm != null) {
				string shortName = vm.FindShortName(this);
				if (shortName != null) return shortName;
			}
			ref GCMap map = ref gc.Maps.Get(ItemIndex);
			var strs = new List<string>(map.Count());
			for (int iter = map.NextEntry(-1); iter != -1; iter = map.NextEntry(iter)) {
				Value key = map.KeyAt(iter);
				Value val = map.ValueAt(iter);
				int nextRecurLimit = recursionLimit - 1;
				if (Equal(key, val_isa_key)) nextRecurLimit = 1;
				strs.Add(string.Format("{0}: {1}", key.CodeForm(vm, nextRecurLimit),
					val.IsNull ? "null" : val.CodeForm(vm, nextRecurLimit)));
			}
			return "{" + String.Join(", ", strs) + "}";
		}
		if (IsFuncRef) {
			ref GCFuncRef fr = ref gc.FuncRefs.Get(ItemIndex);
			if (fr.FuncIndex < 0) return "<funcref?>";
			return is_null(fr.OuterVars)
				? StringUtils.Format("FuncRef(#{0})", fr.FuncIndex)
				: StringUtils.Format("FuncRef(#{0}, closure)", fr.FuncIndex);
		}
		if (IsError) {
			ref GCError err = ref gc.Errors.Get(ItemIndex);
			string msg = is_string(err.Message) ? as_cstring(err.Message) : err.Message.ToString(vm);
			return "error: " + msg;
		}
		return "<value>";
	}

	public ulong Bits => _u;

	// ==== ARITHMETIC & COMPARISON ============================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value Add(Value a, Value b, VM vm = null) {
		if (a.IsError) return a;
		if (b.IsError) return b;
		if (a.IsDouble && b.IsDouble) return FromDouble(a.AsDouble() + b.AsDouble());
		if (a.IsString) {
			if (b.IsNull) return a;
			if (b.IsString) return string_concat(a, b);
			return string_concat(a, make_string(b.ToString(vm)));
		} else if (b.IsString) {
			if (a.IsNull) return b;
			return string_concat(make_string(a.ToString(vm)), b);
		}
		if (a.IsList && b.IsList) return list_concat(a, b);
		if (a.IsMap  && b.IsMap)  return map_concat(a, b);
		return Null();
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value Multiply(Value a, Value b) {
		if (a.IsError) return a;
		if (b.IsError) return b;
		if (a.IsDouble && b.IsDouble) return FromDouble(a.AsDouble() * b.AsDouble());
		if (is_string(a) && is_double(b)) {
			double factor = as_double(b);
			if (double.IsNaN(factor) || double.IsInfinity(factor)) return Null();
			if (factor <= 0) return val_empty_string;
			int repeats = (int)factor;
			Value result = val_empty_string;
			for (int i = 0; i < repeats; i++) result = string_concat(result, a);
			int extraChars = (int)(string_length(a) * (factor - repeats));
			if (extraChars > 0) result = string_concat(result, string_substring(a, 0, extraChars));
			return result;
		}
		if (a.IsList && b.IsDouble) {
			double factor = b.AsDouble();
			if (double.IsNaN(factor) || double.IsInfinity(factor)) return Null();
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
		return Null();
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value Divide(Value a, Value b) {
		if (a.IsError) return a;
		if (b.IsError) return b;
		if (a.IsDouble && b.IsDouble) return FromDouble(a.AsDouble() / b.AsDouble());
		if (is_string(a) && b.IsDouble) return value_mult(a, value_div(make_double(1), b));
		if (a.IsList && b.IsDouble) {
			double db = b.AsDouble();
			if (db == 0 || double.IsNaN(db) || double.IsInfinity(db)) return Null();
			return value_mult(a, value_div(make_double(1), b));
		}
		return Null();
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value Mod(Value a, Value b) {
		if (a.IsError) return a;
		if (b.IsError) return b;
		if (a.IsDouble && b.IsDouble) return FromDouble(a.AsDouble() % b.AsDouble());
		return Null();
	}

	public static Value Pow(Value a, Value b) {
		if (a.IsError) return a;
		if (b.IsError) return b;
		if (a.IsDouble && b.IsDouble) return FromDouble(Math.Pow(a.AsDouble(), b.AsDouble()));
		return Null();
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value Sub(Value a, Value b) {
		if (a.IsError) return a;
		if (b.IsError) return b;
		if (a.IsDouble && b.IsDouble) return FromDouble(a.AsDouble() - b.AsDouble());
		if (a.IsString && b.IsString) {
			string sa = as_cstring(a);
			string sb = as_cstring(b);
			if (sb.Length > 0 && sa.EndsWith(sb))
				return FromString(sa.Substring(0, sa.Length - sb.Length));
			return a;
		}
		return Null();
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool LessThan(Value a, Value b) {
		if (a.IsDouble && b.IsDouble) return a.AsDouble() < b.AsDouble();
		if (a.IsString && b.IsString) return string_compare(a, b) < 0;
		return false;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool LessThanOrEqual(Value a, Value b) {
		if (a.IsDouble && b.IsDouble) return a.AsDouble() <= b.AsDouble();
		if (a.IsString && b.IsString) return string_compare(a, b) <= 0;
		return false;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool Identical(Value a, Value b) => a._u == b._u;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool Equal(Value a, Value b) {
		if (a._u == b._u) return true;
		if (a.IsDouble && b.IsDouble) return a.AsDouble() == b.AsDouble();
		if (a.IsTiny   && b.IsTiny)   return a._u == b._u;
		if (a.IsString && b.IsString) {
			return string.Equals(as_cstring(a), as_cstring(b), StringComparison.Ordinal);
		}
		if (a.IsNull || b.IsNull) return a.IsNull && b.IsNull;
		if (a.IsList && b.IsList) {
			int countA = list_count(a);
			if (countA != list_count(b)) return false;
			for (int i = 0; i < countA; i++) {
				if (!value_equal(list_get(a, i), list_get(b, i))) return false;
			}
			return true;
		}
		if (a.IsMap && b.IsMap) {
			ref GCMap mapA = ref gc.Maps.Get(a.ItemIndex);
			ref GCMap mapB = ref gc.Maps.Get(b.ItemIndex);
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

	// ==== HELPERS ============================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	private static bool IsAllAscii(string s) {
		foreach (char c in s) if (c > 0x7F) return false;
		return true;
	}
}

// =============================================================================
// Global helper functions matching the C++ value.h interface
// =============================================================================
public static class ValueHelpers {

	// The single GC manager for this process.
	public static readonly GCManager gc = new GCManager();

	// Common constant values
	public static Value val_null         = Value.Null();
	public static Value val_zero         = Value.FromDouble(0.0);
	public static Value val_one          = Value.FromDouble(1.0);
	public static Value val_empty_string = Value.FromString("");
	public static Value val_isa_key      = Value.FromString("__isa");
	public static Value val_self         = Value.FromString("self");
	public static Value val_super        = Value.FromString("super");

	// ── Core value creation ──────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_int(int i) => Value.FromDouble((double)i);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_int(bool b) => Value.FromDouble(b ? 1.0 : 0.0);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_double(double d) => Value.FromDouble(d);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_string(string str) => Value.FromString(str);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static String to_String(Value v) => as_cstring(to_string(v));

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_list(int initial_capacity) => gc.NewList(initial_capacity);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_empty_list() => gc.NewList(0);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_empty_map() => gc.NewMap(8);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_map(int initial_capacity) => gc.NewMap(initial_capacity);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_funcref(Int32 funcIndex, Value outerVars) => gc.NewFuncRef(funcIndex, outerVars);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Int32 funcref_index(Value v) {
		if (!v.IsFuncRef) return -1;
		return gc.FuncRefs.Get(v.ItemIndex).FuncIndex;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value funcref_outer_vars(Value v) {
		if (!v.IsFuncRef) return val_null;
		return gc.FuncRefs.Get(v.ItemIndex).OuterVars;
	}

	// ── Error operations ─────────────────────────────────────────────────────

	public static Value make_error(Value message, Value inner, Value stack, Value isa) {
		if (is_null(stack)) {
			stack = make_list(0);
			freeze_value(stack);
		}
		return gc.NewError(message, inner, stack, isa);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value error_message(Value v) {
		if (!v.IsError) return val_null;
		return gc.Errors.Get(v.ItemIndex).Message;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value error_inner(Value v) {
		if (!v.IsError) return val_null;
		return gc.Errors.Get(v.ItemIndex).Inner;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value error_stack(Value v) {
		if (!v.IsError) return val_null;
		return gc.Errors.Get(v.ItemIndex).Stack;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value error_isa(Value v) {
		if (!v.IsError) return val_null;
		return gc.Errors.Get(v.ItemIndex).Isa;
	}

	public static bool error_isa_contains(Value err, Value target) {
		if (!err.IsError) return false;
		Value current = gc.Errors.Get(err.ItemIndex).Isa;
		for (int depth = 0; depth < 256; depth++) {
			if (is_null(current)) return false;
			if (value_identical(current, target)) return true;
			if (!is_error(current)) return false;
			current = gc.Errors.Get(current.ItemIndex).Isa;
		}
		return false;
	}

	// ── Type predicates ──────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_null(Value v) => v.IsNull;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_int(Value v) => false;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_double(Value v) => v.IsDouble;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_string(Value v) => v.IsString;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_map(Value v) => v.IsMap;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_list(Value v) => v.IsList;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_funcref(Value v) => v.IsFuncRef;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_error(Value v) => v.IsError;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_tiny_string(Value v) => v.IsTiny;
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_number(Value v) => v.IsDouble;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_truthy(Value v) => !is_null(v) &&
		((is_double(v) && as_double(v) != 0.0) ||
		 (is_string(v) && string_length(v) != 0));

	// ── Numeric accessors ────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int as_int(Value v) => (int)v.AsDouble();

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static double as_double(Value v) => v.AsDouble();

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static double numeric_val(Value v) {
		if (v.IsDouble) return v.AsDouble();
		if (v.IsError) VM.ActiveVM().RaiseUncaughtError(v);
		return 0.0;
	}

	// ── String accessors ─────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static String as_cstring(Value v) {
		if (!v.IsString) return "";
		return GetStringValue(v);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	private static string GetStringValue(Value val) {
		if (val.IsTiny) {
			int len = val.TinyLen();
			if (len == 0) return "";
			var chars = new char[len];
			for (int i = 0; i < len; i++) chars[i] = (char)((val.Bits >> (8 * (i + 1))) & 0xFF);
			return new string(chars);
		}
		if (val.IsHeapString) return gc.Strings.Get(val.ItemIndex).Data ?? "";
		return "";
	}

	// ── List operations ──────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int list_count(Value list_val) {
		if (!list_val.IsList) return 0;
		return gc.Lists.Get(list_val.ItemIndex).Items.Count;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value list_get(Value list_val, int index) {
		if (!list_val.IsList) return val_null;
		return gc.Lists.Get(list_val.ItemIndex).Get(index);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static void list_set(Value list_val, int index, Value item) {
		if (!list_val.IsList) return;
		ref GCList list = ref gc.Lists.Get(list_val.ItemIndex);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		list.Set(index, item);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static void list_push(Value list_val, Value item) {
		if (!list_val.IsList) return;
		ref GCList list = ref gc.Lists.Get(list_val.ItemIndex);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		list.Push(item);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool list_remove(Value list_val, int index) {
		if (!list_val.IsList) return false;
		ref GCList list = ref gc.Lists.Get(list_val.ItemIndex);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return false; }
		return list.Remove(index);
	}

	public static void list_insert(Value list_val, int index, Value item) {
		if (!list_val.IsList) return;
		ref GCList list = ref gc.Lists.Get(list_val.ItemIndex);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		list.Insert(index, item);
	}

	public static Value list_pop(Value list_val) {
		if (!list_val.IsList) return val_null;
		ref GCList list = ref gc.Lists.Get(list_val.ItemIndex);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return val_null; }
		return list.Pop();
	}

	public static Value list_pull(Value list_val) {
		if (!list_val.IsList) return val_null;
		ref GCList list = ref gc.Lists.Get(list_val.ItemIndex);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return val_null; }
		return list.Pull();
	}

	public static int list_indexOf(Value list_val, Value item, int afterIdx) {
		if (!list_val.IsList) return -1;
		return gc.Lists.Get(list_val.ItemIndex).IndexOf(item, afterIdx);
	}

	public static void list_sort(Value list_val, bool ascending) {
		if (!list_val.IsList) return;
		ref GCList list = ref gc.Lists.Get(list_val.ItemIndex);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		SortList(ref list, ascending);
	}

	public static void list_sort_by_key(Value list_val, Value byKey, bool ascending) {
		if (!list_val.IsList) return;
		ref GCList list = ref gc.Lists.Get(list_val.ItemIndex);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		SortListByKey(ref list, byKey, ascending);
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

	private static void SortList(ref GCList list, bool ascending) {
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

	private static void SortListByKey(ref GCList list, Value byKey, bool ascending) {
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
		if (!map_val.IsMap) return 0;
		return gc.Maps.Get(map_val.ItemIndex).Count();
	}

	public static Value map_get(Value map_val, Value key) {
		if (!map_val.IsMap) return val_null;
		gc.Maps.Get(map_val.ItemIndex).TryGet(key, out Value result);
		return result;
	}

	public static bool map_try_get(Value map_val, Value key, out Value value) {
		value = val_null;
		if (!map_val.IsMap) return false;
		return gc.Maps.Get(map_val.ItemIndex).TryGet(key, out value);
	}

	public static bool map_lookup(Value map_val, Value key, out Value value) {
		value = val_null;
		Value isaKey = val_isa_key;
		Value current = map_val;
		for (Int32 depth = 0; depth < 256; depth++) {
			if (!is_map(current)) return false;
			ref GCMap m = ref gc.Maps.Get(current.ItemIndex);
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
			ref GCMap m = ref gc.Maps.Get(current.ItemIndex);
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
		if (!map_val.IsMap) return false;
		ref GCMap m = ref gc.Maps.Get(map_val.ItemIndex);
		if (m.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return false; }
		m.Set(key, value);
		return true;
	}

	public static bool map_remove(Value map_val, Value key) {
		if (!map_val.IsMap) return false;
		ref GCMap m = ref gc.Maps.Get(map_val.ItemIndex);
		if (m.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return false; }
		return m.Remove(key);
	}

	public static bool map_has_key(Value map_val, Value key) {
		if (!map_val.IsMap) return false;
		return gc.Maps.Get(map_val.ItemIndex).HasKey(key);
	}

	public static void map_clear(Value map_val) {
		if (!map_val.IsMap) return;
		ref GCMap m = ref gc.Maps.Get(map_val.ItemIndex);
		if (m.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return; }
		m.Clear();
	}

	public static Value map_nth_entry(Value map_val, int n) {
		if (!map_val.IsMap) return val_null;
		ref GCMap m = ref gc.Maps.Get(map_val.ItemIndex);
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
		if (!map_val.IsMap || iter == MAP_ITER_DONE) return MAP_ITER_DONE;
		int next = gc.Maps.Get(map_val.ItemIndex).NextEntry(iter);
		return next == -1 ? MAP_ITER_DONE : next;
	}

	public static Value map_iter_entry(Value map_val, int iter) {
		if (!map_val.IsMap || iter == MAP_ITER_DONE) return val_null;
		ref GCMap m = ref gc.Maps.Get(map_val.ItemIndex);
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
		if (map_val.IsMap) {
			it.MapIndex = map_val.ItemIndex;
			it.Iter     = -1;
		} else {
			it.MapIndex = -1;
			it.Iter     = MAP_ITER_DONE;
		}
		return it;
	}

	public static bool map_iterator_next(ref MapIterator iter) {
		if (iter.MapIndex < 0 || iter.Iter == MAP_ITER_DONE) return false;
		int next = gc.Maps.Get(iter.MapIndex).NextEntry(iter.Iter);
		if (next == -1) { iter.Iter = MAP_ITER_DONE; return false; }
		iter.Iter = next;
		ref GCMap m = ref gc.Maps.Get(iter.MapIndex);
		iter.Key = m.KeyAt(next);
		iter.Val = m.ValueAt(next);
		return true;
	}

	// ── VarMap operations ─────────────────────────────────────────────────────
	public static Value make_varmap(List<Value> registers, List<Value> names, int baseIdx, int count) {
		VarMapBacking vmb = new VarMapBacking(registers, names, baseIdx, baseIdx + count - 1);
		return gc.NewVarMap(vmb);
	}

	public static void varmap_gather(Value map_val) {
		if (!map_val.IsMap) return;
		ref GCMap m = ref gc.Maps.Get(map_val.ItemIndex);
		m._vmb?.Gather(ref m);
	}

	public static void varmap_rebind(Value map_val, List<Value> registers, List<Value> names) {
		if (!map_val.IsMap) return;
		ref GCMap m = ref gc.Maps.Get(map_val.ItemIndex);
		m._vmb?.Rebind(ref m, registers, names);
	}

	public static void varmap_map_to_register(Value map_val, Value varName, List<Value> registers, int regIndex) {
		if (!map_val.IsMap) return;
		ref GCMap m = ref gc.Maps.Get(map_val.ItemIndex);
		m._vmb?.MapToRegister(ref m, varName, registers, regIndex);
	}

	// ── Freeze operations ─────────────────────────────────────────────────────
	public static bool is_frozen(Value v) {
		if (v.IsList) return gc.Lists.Get(v.ItemIndex).Frozen;
		if (v.IsMap)  return gc.Maps.Get(v.ItemIndex).Frozen;
		return false;
	}

	public static void freeze_value(Value v) {
		if (v.IsList) {
			ref GCList list = ref gc.Lists.Get(v.ItemIndex);
			if (list.Frozen) return;
			list.Frozen = true;
			for (int i = 0; i < list.Items.Count; i++) freeze_value(list.Get(i));
		} else if (v.IsMap) {
			ref GCMap map = ref gc.Maps.Get(v.ItemIndex);
			if (map.Frozen) return;
			map.Frozen = true;
			for (int iter = map.NextEntry(-1); iter != -1; iter = map.NextEntry(iter)) {
				freeze_value(map.KeyAt(iter));
				freeze_value(map.ValueAt(iter));
			}
		}
	}

	public static Value frozen_copy(Value v) {
		if (v.IsList) {
			ref GCList src = ref gc.Lists.Get(v.ItemIndex);
			if (src.Frozen) return v;
			Value newList = make_list(src.Items.Count);
			ref GCList dst = ref gc.Lists.Get(newList.ItemIndex);
			dst.Frozen = true;
			for (int i = 0; i < src.Items.Count; i++) dst.Push(frozen_copy(src.Get(i)));
			return newList;
		}
		if (v.IsMap) {
			ref GCMap src = ref gc.Maps.Get(v.ItemIndex);
			if (src.Frozen) return v;
			Value newMap = make_map(src.Count());
			ref GCMap dst = ref gc.Maps.Get(newMap.ItemIndex);
			dst.Frozen = true;
			// Iterate src and copy entries into dst.
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
		if (a.IsMap) {
			ref GCMap mapA = ref gc.Maps.Get(a.ItemIndex);
			for (int iter = mapA.NextEntry(-1); iter != -1; iter = mapA.NextEntry(iter))
				map_set(result, mapA.KeyAt(iter), mapA.ValueAt(iter));
		}
		if (b.IsMap) {
			ref GCMap mapB = ref gc.Maps.Get(b.ItemIndex);
			for (int iter = mapB.NextEntry(-1); iter != -1; iter = mapB.NextEntry(iter))
				map_set(result, mapB.KeyAt(iter), mapB.ValueAt(iter));
		}
		return result;
	}

	// ── Arithmetic operations ─────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_add(Value a, Value b, VM vm = null) => Value.Add(a, b, vm);
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_mult(Value a, Value b) => Value.Multiply(a, b);
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_div(Value a, Value b) => Value.Divide(a, b);
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_mod(Value a, Value b) => Value.Mod(a, b);
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_pow(Value a, Value b) => Value.Pow(a, b);
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_sub(Value a, Value b) => Value.Sub(a, b);

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
		if (a.IsError) return a;
		if (b.IsError) return b;
		return make_double(AbsClamp01(ToFuzzyBool(a) * ToFuzzyBool(b)));
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_or(Value a, Value b) {
		if (a.IsError) return b;
		if (b.IsError) return b;
		double fA = ToFuzzyBool(a), fB = ToFuzzyBool(b);
		return make_double(AbsClamp01(fA + fB - fA * fB));
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_not(Value a) {
		if (a.IsError) return a;
		return make_double(1.0 - AbsClamp01(ToFuzzyBool(a)));
	}

	// ── Comparison ────────────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_identical(Value a, Value b) => Value.Identical(a, b);
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_lt(Value a, Value b) => Value.LessThan(a, b);
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_le(Value a, Value b) => Value.LessThanOrEqual(a, b);
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_equal(Value a, Value b) => Value.Equal(a, b);

	// ── Conversion ────────────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value to_string(Value a, VM vm = null) => make_string(a.ToString(vm));

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value to_number(Value a) {
		try { return make_double(double.Parse(a.ToString(null))); }
		catch { return val_zero; }
	}

	public static Value value_repr(Value v, VM vm = null) => make_string(v.CodeForm(vm));

	// ── String operations ─────────────────────────────────────────────────────
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int string_length(Value v) {
		if (!v.IsString) return 0;
		return GetStringValue(v).Length;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int string_indexOf(Value haystack, Value needle, int start_pos) {
		if (!haystack.IsString || !needle.IsString) return -1;
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
		if (!a.IsString || !b.IsString) return val_null;
		return make_string(GetStringValue(a) + GetStringValue(b));
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int string_compare(Value a, Value b) {
		if (!a.IsString || !b.IsString) return 0;
		return string.Compare(GetStringValue(a), GetStringValue(b), StringComparison.Ordinal);
	}

	public static Value string_split(Value str, Value delimiter) {
		if (!str.IsString || !delimiter.IsString) return val_null;
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
		if (!str.IsString || !from.IsString || !to.IsString) return val_null;
		string s = GetStringValue(str);
		string fromStr = GetStringValue(from);
		if (fromStr == "" || !s.Contains(fromStr)) return str;
		return make_string(s.Replace(fromStr, GetStringValue(to)));
	}

	public static Value string_insert(Value str, int index, Value value, VM vm = null) {
		if (!str.IsString) return str;
		string s = GetStringValue(str);
		string insertStr = value.ToString(vm);
		if (index < 0) index += s.Length + 1;
		if (index < 0) index = 0;
		if (index > s.Length) index = s.Length;
		return make_string(s.Insert(index, insertStr));
	}

	public static Value string_split_max(Value str, Value delimiter, int maxCount) {
		if (!str.IsString || !delimiter.IsString) return val_null;
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
		if (!str.IsString || !from.IsString || !to.IsString) return val_null;
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
		if (!str.IsString) return str;
		return make_string(GetStringValue(str).ToUpper());
	}

	public static Value string_lower(Value str) {
		if (!str.IsString) return str;
		return make_string(GetStringValue(str).ToLower());
	}

	public static Value string_from_code_point(int codePoint) =>
		make_string(char.ConvertFromUtf32(codePoint));

	public static int string_code_point(Value str) {
		if (!str.IsString) return 0;
		string s = GetStringValue(str);
		if (s.Length == 0) return 0;
		return char.ConvertToUtf32(s, 0);
	}
}

}
//*** END CS_ONLY ***
