//*** BEGIN CS_ONLY ***
// (This entire file is only for C#; the C++ code uses value.h/.c instead.)

using System;
using System.Collections.Generic;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace MiniScript {

public struct MapIterator {
	public int MapIndex;
	public int Iter;
	public Value Key;
	public Value Val;
}

// NOTE: Align the TAG MASK constants below with your C value.h.
// The layout mirrors a NaN-box: 64-bit payload that is either:
// - a real double (no matching tag), OR
// - an encoded immediate (null, tiny string), OR
// - a GC-managed object: GC_TAG | (gcSet << 32) | itemIndex
//
// GCSet assignments (must match GCManager constants):
//   STRING_SET  = 0  → 0xFFFE_0000_0000_XXXX
//   Value.list_SET    = 1  → 0xFFFE_0001_0000_XXXX
//   MAP_SET     = 2  → 0xFFFE_0002_0000_XXXX
//   ERROR_SET   = 3  → 0xFFFE_0003_0000_XXXX
//   FUNCREF_SET = 4  → 0xFFFE_0004_0000_XXXX
//
// Keep Value at 8 bytes, blittable, and aggressively inlined.

[StructLayout(LayoutKind.Explicit, Size = 8)]
public readonly struct Value {
	[FieldOffset(0)] internal readonly ulong _u;
	[FieldOffset(0)] internal readonly double _d;

	private Value(ulong u, bool _fromBits) { _d = 0; _u = u; }
	private static Value FromBits(ulong u) => new Value(u, true);

	// MiniScript 1.x-compatible public constructors.
	public Value(double d) { _u = 0; _d = d; }
	public Value(string s) { var tmp = make_string(s); _d = 0; _u = tmp._u; }

	// ==== TAGS & MASKS =======================================================
	internal const ulong NANISH_MASK     = 0xFFFF_0000_0000_0000UL;
	// NULL_VALUE doubles as the is_double threshold: any value whose top 16 bits
	// are below it is a double.  It sits at 0xFFF9 (rather than hugging the
	// double range) so that the negative infinity (0xFFF0) and negative quiet
	// NaN (0xFFF8) bit patterns still classify as doubles; only 0xFFF9-0xFFFF is
	// reserved for tags.  This lets make_double stay a branch-free bit copy.
	internal const ulong NULL_VALUE      = 0xFFF9_0000_0000_0000UL;
	public  const ulong GC_TAG          = 0xFFFE_0000_0000_0000UL;
	internal const ulong TINY_STRING_TAG = 0xFFFF_0000_0000_0000UL;
	internal const ulong GC_TYPE_MASK    = NANISH_MASK | 0x0000_0007_0000_0000UL;

	// ==== LIMITS =============================================================
	// Maximum number of elements a list (or characters a string) may hold.
	// Operations whose result would exceed this raise a runtime error.
	public const int MAX_COLLECTION_SIZE = Int32.MaxValue;
	public const int MAP_ITER_DONE       = Int32.MinValue;

	// ==== SHARED CONSTANTS ===================================================
	// PascalCase: MiniScript 1.x-style public API.
	public static readonly Value Null           = FromBits(NULL_VALUE);
	public static readonly Value zero           = FromBits(0x0000_0000_0000_0000UL);
	public static readonly Value one            = FromBits(0x3FF0_0000_0000_0000UL);
	public static readonly Value emptyString    = FromBits(TINY_STRING_TAG);
	public static readonly Value magicIsA       = make_string("__isa");
	public static readonly Value keyString      = make_string("key");
	public static readonly Value valueString    = make_string("value");
	public static readonly Value implicitResult = make_string("_");
	public static readonly Value selfString     = make_string("self");
	public static readonly Value superString    = make_string("super");

	// ==== MS1 INSTANCE PREDICATES ============================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)] public bool IsNull()    => _u == NULL_VALUE;
	[MethodImpl(MethodImplOptions.AggressiveInlining)] public bool IsNumber()  => (_u & NANISH_MASK) < NULL_VALUE;
	[MethodImpl(MethodImplOptions.AggressiveInlining)] public bool IsString()  => is_tiny_string(this) || is_heap_string(this);
	[MethodImpl(MethodImplOptions.AggressiveInlining)] public bool IsList()    => (_u & GC_TYPE_MASK) == (GC_TAG | ((ulong)GCManager.ListSet     << 32));
	[MethodImpl(MethodImplOptions.AggressiveInlining)] public bool IsMap()     => (_u & GC_TYPE_MASK) == (GC_TAG | ((ulong)GCManager.MapSet      << 32));
	[MethodImpl(MethodImplOptions.AggressiveInlining)] public bool IsError()   => (_u & GC_TYPE_MASK) == (GC_TAG | ((ulong)GCManager.ErrorSet    << 32));
	[MethodImpl(MethodImplOptions.AggressiveInlining)] public bool IsFuncRef() => (_u & GC_TYPE_MASK) == (GC_TAG | ((ulong)GCManager.FunctionSet << 32));
	[MethodImpl(MethodImplOptions.AggressiveInlining)] public bool IsHandle()  => (_u & GC_TYPE_MASK) == (GC_TAG | ((ulong)GCManager.HandleSet   << 32));

	// ==== MS1 INSTANCE ACCESSORS =============================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)] public double DoubleValue() => (_u & NANISH_MASK) < NULL_VALUE ? _d : 0.0;
	[MethodImpl(MethodImplOptions.AggressiveInlining)] public float  FloatValue()  => (float)DoubleValue();
	[MethodImpl(MethodImplOptions.AggressiveInlining)] public int    IntValue()    => (int)DoubleValue();
	[MethodImpl(MethodImplOptions.AggressiveInlining)] public uint   UIntValue()   => (uint)DoubleValue();
	public bool BoolValue() {
		if (IsNull()) return false;
		if (IsNumber()) return _d != 0.0;
		if (IsString()) return string_length(this) != 0;
		if (IsList()) return Value.list_count(this) != 0;
		if (IsMap()) return map_count(this) != 0;
		return true;
	}

	// ==== MS1 TRUTH FACTORIES ================================================
	public static Value Truth(bool b)   => b ? one : zero;
	public static Value Truth(double d) => new Value(d);

	// ==== RAW BIT ACCESS =====================================================
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

	// ==== VALUE CREATION =====================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_gc(int gcSet, int itemIdx) =>
		FromBits(GC_TAG | ((ulong)gcSet << 32) | (uint)itemIdx);

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
		ulong u = TINY_STRING_TAG | (ulong)((uint)len & 0xFFU);
		for (int i = 0; i < len; i++) u |= (ulong)utf8[i] << (8 * (i + 1));
		return FromBits(u);
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
	public static Value make_funcref(FuncDef func, Value outerVars) => GCManager.NewFuncRef(func, outerVars);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static FuncDef funcref_funcdef(Value v) {
		if (!v.IsFuncRef()) return null;
		return GCManager.Functions.Get(value_item_index(v)).Func;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value funcref_outer_vars(Value v) {
		if (!v.IsFuncRef()) return Value.Null;
		return GCManager.Functions.Get(value_item_index(v)).OuterVars;
	}

	// ==== ERROR OPERATIONS ===================================================

	public static Value make_error(Value message, Value inner, Value stack, Value isa) {
		if (stack.IsNull()) {
			stack = make_list(0);
			freeze_value(stack);
		}
		return GCManager.NewError(message, inner, stack, isa);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value error_message(Value v) {
		if (!v.IsError()) return Value.Null;
		return GCManager.Errors.Get(value_item_index(v)).Message;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value error_inner(Value v) {
		if (!v.IsError()) return Value.Null;
		return GCManager.Errors.Get(value_item_index(v)).Inner;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value error_stack(Value v) {
		if (!v.IsError()) return Value.Null;
		return GCManager.Errors.Get(value_item_index(v)).Stack;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value error_isa(Value v) {
		if (!v.IsError()) return Value.Null;
		return GCManager.Errors.Get(value_item_index(v)).Isa;
	}

	public static bool error_isa_contains(Value err, Value target) {
		if (!err.IsError()) return false;
		Value current = GCManager.Errors.Get(value_item_index(err)).Isa;
		for (int depth = 0; depth < 256; depth++) {
			if (current.IsNull()) return false;
			if (value_identical(current, target)) return true;
			if (!current.IsError()) return false;
			current = GCManager.Errors.Get(value_item_index(current)).Isa;
		}
		return false;
	}

	// ==== TYPE PREDICATES ====================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_int(Value v) => false;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_tiny_string(Value v) => (v._u & NANISH_MASK) == TINY_STRING_TAG;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_gc_object(Value v) => (v._u & NANISH_MASK) == GC_TAG;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_heap_string(Value v) {
		ulong masked = v._u & GC_TYPE_MASK;
		return masked == (GC_TAG | ((ulong)GCManager.BigStringSet      << 32))
		    || masked == (GC_TAG | ((ulong)GCManager.InternedStringSet << 32));
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_interned_string(Value v) =>
		(v._u & GC_TYPE_MASK) == (GC_TAG | ((ulong)GCManager.InternedStringSet << 32));

	// ==== NUMERIC ACCESSORS ==================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int as_int(Value v) => (int)as_double(v);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static double as_double(Value v) => v._d;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static double numeric_val(Value v) {
		if (v.IsNumber()) return as_double(v);
		if (v.IsError()) VM.ActiveVM().RaiseUncaughtError(v);
		return 0.0;
	}

	// Return a human-readable name for the type of v, matching the names used
	// by the `info` intrinsic and the core type maps.
	public static String value_type_name(Value v) {
		if (v.IsNumber()) return "number";
		if (v.IsString()) return "string";
		if (v.IsList()) return "list";
		if (v.IsMap()) return "map";
		if (v.IsFuncRef()) return "funcRef";
		if (v.IsError()) return "error";
		if (v.IsHandle()) return "handle";
		if (v.IsNull()) return "null";
		return "unknown";
	}

	// ==== STRING ACCESSORS ===================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static String as_cstring(Value v) {
		if (!v.IsString()) return "";
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

	// ==== CONVERSION =========================================================

	// Returns the string representation of v as a C# string.
	// For a Value that is already a string this returns its content.
	// For other types this calls code_form with a recursion limit of 3.
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static String to_String(Value v, VM vm = null) {
		if (v.IsString()) return as_cstring(v);
		return code_form(v, vm, 3);
	}

	// Returns v converted to a string Value.
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value to_string(Value a, VM vm = null) {
		if (a.IsString()) return a;
		return make_string(code_form(a, vm, 3));
	}

	public static Value value_repr(Value v, VM vm = null) => make_string(code_form(v, vm));

	// Format a double as the shortest decimal string in scientific notation that
	// round-trips back to the same value, with the exponent normalized to at
	// least two digits (e.g. "1.7014118346046924E+30", "1.234567E-07").  Used for
	// very large / very small magnitudes, and for whole numbers too large to be
	// represented exactly.  The C++ runtime mirrors this in format_shortest_sci.
	private static string ShortestSci(double value) {
		string s = value.ToString("E16", CultureInfo.InvariantCulture);
		for (int sig = 1; sig <= 17; sig++) {
			string candidate = value.ToString("E" + (sig - 1).ToString(), CultureInfo.InvariantCulture);
			if (double.Parse(candidate, CultureInfo.InvariantCulture) == value) {
				s = candidate;
				break;
			}
		}
		// Normalize exponent to at least 2 digits (e.g. E+030 → E+30, E-8 → E-08).
		int eIdx = s.IndexOf('E');
		if (eIdx >= 0) {
			string mantissa = s.Substring(0, eIdx + 2); // includes E and sign
			string expDigits = s.Substring(eIdx + 2).TrimStart('0');
			if (expDigits.Length == 0) expDigits = "0";
			while (expDigits.Length < 2) expDigits = "0" + expDigits;
			s = mantissa + expDigits;
		}
		return s;
	}

	// Returns the printable/code representation of v as a C# string.
	public static string code_form(Value v, VM vm = null, int recursionLimit = -1) {
		if (v.IsNull()) return "null";
		if (v.IsNumber()) {
			double value = as_double(v);
			if (double.IsNaN(value)) return "NaN";
			if (double.IsPositiveInfinity(value)) return "Inf";
			if (double.IsNegativeInfinity(value)) return "-Inf";
			if (value % 1.0 == 0.0) {
				// Integers up to 2^53 are represented exactly, so print them in
				// plain form.  Cast to long (exact here, since 2^53 < 2^63) rather
				// than ToString("0"), which would round to 15 significant digits.
				// Larger whole values have lost precision, so fall through to the
				// shortest round-trip scientific form.
				if (value <= 9007199254740992.0 && value >= -9007199254740992.0) {
					return ((long)value).ToString(CultureInfo.InvariantCulture);
				}
				return ShortestSci(value);
			} else if (value > 1E10 || value < -1E10 || (value < 1E-6 && value > -1E-6)) {
				return ShortestSci(value);
			} else {
				string result = value.ToString("0.0#####", CultureInfo.InvariantCulture);
				if (result == "-0") result = "0";
				return result;
			}
		}
		if (v.IsString()) {
			String s = as_cstring(v);
			s = s.Replace("\"", "\"\"");
			return "\"" + s + "\"";
		}
		if (v.IsList()) {
			if (recursionLimit == 0) return "[...]";
			if (recursionLimit > 0 && recursionLimit < 3 && vm != null) {
				string shortName = vm.FindShortName(v);
				if (shortName != null) return shortName;
			}
			GCList list = GCManager.Lists.Get(value_item_index(v));
			int listCount = list.Count();
			var strs = new string[listCount];
			for (int i = 0; i < listCount; i++) {
				Value val_i = list.Get(i);
				strs[i] = val_i.IsNull() ? "null" : code_form(val_i, vm, recursionLimit - 1);
			}
			return "[" + string.Join(", ", strs) + "]";
		}
		if (v.IsMap()) {
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
				if (key == Value.magicIsA) nextRecurLimit = 1;
				strs.Add(string.Format("{0}: {1}", code_form(key, vm, nextRecurLimit),
					val.IsNull() ? "null" : code_form(val, vm, nextRecurLimit)));
			}
			return "{" + String.Join(", ", strs) + "}";
		}
		if (v.IsFuncRef()) {
			GCFunction fr = GCManager.Functions.Get(value_item_index(v));
			if (fr.Func == null) return "<funcref?>";
			return fr.OuterVars.IsNull()
				? StringUtils.Format("FuncRef({0})", fr.Func.Name)
				: StringUtils.Format("FuncRef({0}, closure)", fr.Func.Name);
		}
		if (v.IsError()) {
			GCError err = GCManager.Errors.Get(value_item_index(v));
			string msg = err.Message.IsString() ? as_cstring(err.Message) : to_String(err.Message, vm);
			return "error: " + msg;
		}
		return "<value>";
	}

	public static Value to_number(Value a) {
		try { return new Value(double.Parse(to_String(a), CultureInfo.InvariantCulture)); }
		catch { return Value.zero; }
	}

	// ==== RUNTIME ERROR / STACK TRACE ========================================

	// Construct a complete runtime error value (runtime __isa + stack trace) from
	// core value code, which cannot see the VM layer directly.  In C++ this routes
	// through a hook registered by the VM (value_make_runtime_error in value.cpp);
	// in C# we can reach the active VM directly.  Falls back to a bare error if no
	// VM is active (e.g. unit tests exercising value ops in isolation).
	public static Value value_make_runtime_error(String message) { // CPP: // (defined in value.cpp)
		VM vm = VM.ActiveVM();
		if (vm == null) return make_error(make_string(message), Value.Null, Value.Null, Value.Null);
		return vm.MakeRuntimeError(message);
	}

	// Return the active VM's current call stack as a Value (frozen list of
	// strings), or Value.Null if no VM is running.
	public static Value value_current_stack_trace() { // CPP: // (defined in value.cpp)
		VM vm = VM.ActiveVM();
		if (vm == null) return Value.Null;
		return vm.CurrentStackTrace();
	}

	// ==== ARITHMETIC =========================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_add(Value a, Value b, VM vm = null) {
		if (a.IsError()) return a;
		if (b.IsError()) return b;
		if (a.IsNumber() && b.IsNumber()) return new Value(as_double(a) + as_double(b));
		if (a.IsString()) {
			if (b.IsNull()) return a;
			Value bStr = b.IsString() ? b : make_string(to_String(b, vm));
			// Overflow-safe check that the concatenation won't exceed the limit.
			if (string_length(a) > MAX_COLLECTION_SIZE - string_length(bStr)) {
				return value_make_runtime_error("string too large (exceeds maximum size)");
			}
			return string_concat(a, bStr);
		} else if (b.IsString()) {
			if (a.IsNull()) return b;
			Value aStr = make_string(to_String(a, vm));
			if (string_length(aStr) > MAX_COLLECTION_SIZE - string_length(b)) {
				return value_make_runtime_error("string too large (exceeds maximum size)");
			}
			return string_concat(aStr, b);
		}
		if (a.IsList() && b.IsList()) {
			if (Value.list_count(a) > MAX_COLLECTION_SIZE - Value.list_count(b)) {
				return value_make_runtime_error("list too large (exceeds maximum size)");
			}
			return Value.list_concat(a, b);
		}
		if (a.IsMap()  && b.IsMap())  return map_concat(a, b);
		return Value.Null;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value operator *(Value a, Value b) {
		if (a.IsError()) return a;
		if (b.IsError()) return b;
		if (a.IsNumber() && b.IsNumber()) return new Value(as_double(a) * as_double(b));
		if (a.IsString() && b.IsNumber()) {
			double factor = as_double(b);
			if (double.IsNaN(factor) || double.IsInfinity(factor)) return Value.Null;
			if (factor <= 0) return Value.emptyString;
			if (string_length(a) * factor > MAX_COLLECTION_SIZE) {
				return value_make_runtime_error("string too large (exceeds maximum size)");
			}
			int repeats = (int)factor;
			Value result = Value.emptyString;
			for (int i = 0; i < repeats; i++) result = string_concat(result, a);
			int extraChars = (int)(string_length(a) * (factor - repeats));
			if (extraChars > 0) result = string_concat(result, string_substring(a, 0, extraChars));
			return result;
		}
		if (a.IsList() && b.IsNumber()) {
			double factor = as_double(b);
			if (double.IsNaN(factor) || double.IsInfinity(factor)) return Value.Null;
			int len = Value.list_count(a);
			if (factor <= 0 || len == 0) return make_list(0);
			if (len * factor > MAX_COLLECTION_SIZE) {
				return value_make_runtime_error("list too large (exceeds maximum size)");
			}
			int fullCopies = (int)factor;
			int extraItems = (int)(len * (factor - fullCopies));
			// Fast path: a single immutable element repeated a whole number of
			// times becomes a computed list (null increment => repeat the base).
			if (len == 1 && extraItems == 0) {
				Value elem = Value.list_get(a, 0);
				if (elem.IsNumber() || elem.IsString() || elem.IsNull() || is_frozen(elem)) {
					return GCManager.NewComputedList(elem, Value.Null, fullCopies);
				}
			}
			Value result = make_list(fullCopies * len + extraItems);
			for (int c = 0; c < fullCopies; c++)
				for (int i = 0; i < len; i++) Value.list_push(result, Value.list_get(a, i));
			for (int i = 0; i < extraItems; i++) Value.list_push(result, Value.list_get(a, i));
			return result;
		}
		return Value.Null;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value operator /(Value a, Value b) {
		if (a.IsError()) return a;
		if (b.IsError()) return b;
		if (a.IsNumber() && b.IsNumber()) return new Value(as_double(a) / as_double(b));
		if (a.IsString() && b.IsNumber()) return a * (new Value(1.0) / b);
		if (a.IsList() && b.IsNumber()) {
			double db = as_double(b);
			if (db == 0 || double.IsNaN(db) || double.IsInfinity(db)) return Value.Null;
			return a * (new Value(1.0) / b);
		}
		return Value.Null;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value operator %(Value a, Value b) {
		if (a.IsError()) return a;
		if (b.IsError()) return b;
		if (a.IsNumber() && b.IsNumber()) return new Value(as_double(a) % as_double(b));
		return Value.Null;
	}

	public static Value value_pow(Value a, Value b) {
		if (a.IsError()) return a;
		if (b.IsError()) return b;
		if (a.IsNumber() && b.IsNumber()) return new Value(Math.Pow(as_double(a), as_double(b)));
		return Value.Null;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value operator -(Value a, Value b) {
		if (a.IsError()) return a;
		if (b.IsError()) return b;
		if (a.IsNumber() && b.IsNumber()) return new Value(as_double(a) - as_double(b));
		if (a.IsString() && b.IsString()) {
			string sa = as_cstring(a);
			string sb = as_cstring(b);
			if (sb.Length > 0 && sa.EndsWith(sb))
				return make_string(sa.Substring(0, sa.Length - sb.Length));
			return a;
		}
		return Value.Null;
	}

	// ==== FUZZY LOGIC ========================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Double ToFuzzyBool(Value v) {
		if (v.IsNumber()) return as_double(v);
		return v.BoolValue() ? 1.0 : 0.0;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Double AbsClamp01(Double d) {
		if (d < 0) d = -d;
		return d > 1 ? 1 : d;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_and(Value a, Value b) {
		if (a.IsError()) return a;
		if (b.IsError()) return b;
		return new Value(AbsClamp01(ToFuzzyBool(a) * ToFuzzyBool(b)));
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_or(Value a, Value b) {
		if (a.IsError()) return b;
		if (b.IsError()) return b;
		double fA = ToFuzzyBool(a), fB = ToFuzzyBool(b);
		return new Value(AbsClamp01(fA + fB - fA * fB));
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value operator !(Value a) {
		if (a.IsError()) return a;
		return new Value(1.0 - AbsClamp01(ToFuzzyBool(a)));
	}

	// ==== COMPARISON =========================================================
	// Simple bitwise/reference identity: true iff the two Values have the exact
	// same NaN-boxed payload.  This is the fast pointer-identity comparison; it
	// does NOT do MiniScript-semantic (deep) equality -- use operator== for that.
	// Instance method, mirroring MiniScript 1.x.
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public bool RefEquals(Value rhs) => _u == rhs._u;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_identical(Value a, Value b) => a.RefEquals(b);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool operator <(Value a, Value b) {
		if (a.IsNumber() && b.IsNumber()) return as_double(a) < as_double(b);
		if (a.IsString() && b.IsString()) return string_compare(a, b) < 0;
		return false;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool operator >(Value a, Value b) => !(a <= b);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool operator <=(Value a, Value b) {
		if (a.IsNumber() && b.IsNumber()) return as_double(a) <= as_double(b);
		if (a.IsString() && b.IsString()) return string_compare(a, b) <= 0;
		return false;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool operator >=(Value a, Value b) => !(a < b);

	// Deep MiniScript equality -- delegates to RecursiveEqual.
	public static bool operator ==(Value a, Value b) => a.RecursiveEqual(b);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool operator !=(Value a, Value b) => !a.RecursiveEqual(b);

	// Scalar (non-collection) equality: numbers by value, strings by content,
	// null with null, and everything else (funcRef/error/handle) by reference
	// identity.  Lists and maps are NOT handled here -- RecursiveEqual walks them.
	private static bool ScalarEqual(Value a, Value b) {
		if (a.RefEquals(b)) return true;
		if (a.IsNumber() && b.IsNumber()) return as_double(a) == as_double(b);
		// Two tiny strings with differing bits already differ in content.
		if (is_tiny_string(a) && is_tiny_string(b)) return false;
		if (a.IsString() && b.IsString())
			return string.Equals(as_cstring(a), as_cstring(b), StringComparison.Ordinal);
		if (a.IsNull() && b.IsNull()) return true;
		// Same-type non-container reference values compare by identity; RefEquals
		// already failed above, so two distinct such values are not equal.
		return false;
	}

	// A pair of Values awaiting comparison in RecursiveEqual's work-list.
	private struct ValuePair {
		public Value a;
		public Value b;
		// Careful: compare with RefEquals (reference identity), not == (deep), so
		// the visited set can detect reference loops without recursing back here.
		public override bool Equals(object obj) {
			if (obj is ValuePair other) return a.RefEquals(other.a) && b.RefEquals(other.b);
			return false;
		}
		public override int GetHashCode() {
			unchecked { return (int)((a._u * 397) ^ b._u); }
		}
	}

	// Deep MiniScript equality (the implementation behind operator==): numbers by
	// value, strings/lists/maps by content, other reference types by identity.
	// Scalars are compared directly; lists and maps are walked with an explicit
	// work-list (toDo) and a visited set, mirroring MS1's Value.RecursiveEqual.
	// The visited set (compared by reference, not deep) makes cyclic structures
	// terminate instead of overflowing the stack.
	public bool RecursiveEqual(Value rhs) {
		Value a = this, b = rhs;
		if (a.RefEquals(b)) return true;
		// Hot path: if neither side is a collection, no work-list is needed.
		if (!(a.IsList() || a.IsMap()) && !(b.IsList() || b.IsMap())) return ScalarEqual(a, b);

		var toDo = new Stack<ValuePair>();
		var visited = new HashSet<ValuePair>();
		toDo.Push(new ValuePair { a = a, b = b });
		while (toDo.Count > 0) {
			ValuePair pair = toDo.Pop();
			visited.Add(pair);
			Value pa = pair.a, pb = pair.b;
			if (pa.IsList()) {
				if (!pb.IsList()) return false;
				int aCount = list_count(pa);
				if (list_count(pb) != aCount) return false;
				if (pa.RefEquals(pb)) continue;  // same list object: nothing to do
				for (int i = 0; i < aCount; i++) {
					var np = new ValuePair { a = list_get(pa, i), b = list_get(pb, i) };
					if (!visited.Contains(np)) toDo.Push(np);
				}
			} else if (pa.IsMap()) {
				if (!pb.IsMap()) return false;
				GCMap mapA = GCManager.Maps.Get(value_item_index(pa));
				GCMap mapB = GCManager.Maps.Get(value_item_index(pb));
				if (mapA.Count() != mapB.Count()) return false;
				if (pa.RefEquals(pb)) continue;  // same map object: nothing to do
				for (int iter = mapA.NextEntry(-1); iter != -1; iter = mapA.NextEntry(iter)) {
					Value key = mapA.KeyAt(iter);
					if (!mapB.TryGet(key, out Value bVal)) return false;
					var np = new ValuePair { a = mapA.ValueAt(iter), b = bVal };
					if (!visited.Contains(np)) toDo.Push(np);
				}
			} else {
				// No other types can recurse, so compare them directly.
				if (!ScalarEqual(pa, pb)) return false;
			}
		}
		// Drained the work-list without finding inequality: the values are equal.
		return true;
	}


	// ==== LIST OPERATIONS ====================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int list_count(Value list_val) {
		if (!list_val.IsList()) return 0;
		return GCManager.Lists.Get(value_item_index(list_val)).Count();
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value list_get(Value list_val, int index) {
		if (!list_val.IsList()) return Value.Null;
		return GCManager.Lists.Get(value_item_index(list_val)).Get(index);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static void list_set(Value list_val, int index, Value item) {
		if (!list_val.IsList()) return;
		int idx = value_item_index(list_val);
		GCList list = GCManager.Lists.Get(idx);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		bool wasComputed = list.Computed;
		list.Set(index, item);
		if (wasComputed) GCManager.Lists.Set(idx, list);  // write back materialization
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static void list_push(Value list_val, Value item) {
		if (!list_val.IsList()) return;
		int idx = value_item_index(list_val);
		GCList list = GCManager.Lists.Get(idx);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		bool wasComputed = list.Computed;
		list.Push(item);
		if (wasComputed) GCManager.Lists.Set(idx, list);  // write back materialization
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool list_remove(Value list_val, int index) {
		if (!list_val.IsList()) return false;
		int idx = value_item_index(list_val);
		GCList list = GCManager.Lists.Get(idx);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return false; }
		bool wasComputed = list.Computed;
		bool result = list.Remove(index);
		if (wasComputed) GCManager.Lists.Set(idx, list);  // write back materialization
		return result;
	}

	public static void list_insert(Value list_val, int index, Value item) {
		if (!list_val.IsList()) return;
		int idx = value_item_index(list_val);
		GCList list = GCManager.Lists.Get(idx);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		bool wasComputed = list.Computed;
		list.Insert(index, item);
		if (wasComputed) GCManager.Lists.Set(idx, list);  // write back materialization
	}

	public static Value list_pop(Value list_val) {
		if (!list_val.IsList()) return Value.Null;
		int idx = value_item_index(list_val);
		GCList list = GCManager.Lists.Get(idx);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return Value.Null; }
		bool wasComputed = list.Computed;
		Value result = list.Pop();
		if (wasComputed) GCManager.Lists.Set(idx, list);  // write back length/materialization
		return result;
	}

	public static Value list_pull(Value list_val) {
		if (!list_val.IsList()) return Value.Null;
		int idx = value_item_index(list_val);
		GCList list = GCManager.Lists.Get(idx);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return Value.Null; }
		bool wasComputed = list.Computed;
		Value result = list.Pull();
		if (wasComputed) GCManager.Lists.Set(idx, list);  // write back materialization
		return result;
	}

	public static int list_indexOf(Value list_val, Value item, int afterIdx) {
		if (!list_val.IsList()) return -1;
		return GCManager.Lists.Get(value_item_index(list_val)).IndexOf(item, afterIdx);
	}

	public static void list_sort(Value list_val, bool ascending) {
		if (!list_val.IsList()) return;
		int idx = value_item_index(list_val);
		GCList list = GCManager.Lists.Get(idx);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		bool wasComputed = list.Computed;
		list.Materialize();
		SortList(list, ascending);
		if (wasComputed) GCManager.Lists.Set(idx, list);  // write back materialization
	}

	public static void list_sort_by_key(Value list_val, Value byKey, bool ascending) {
		if (!list_val.IsList()) return;
		int idx = value_item_index(list_val);
		GCList list = GCManager.Lists.Get(idx);
		if (list.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		bool wasComputed = list.Computed;
		list.Materialize();
		SortListByKey(list, byKey, ascending);
		if (wasComputed) GCManager.Lists.Set(idx, list);  // write back materialization
	}

	public static Value list_slice(Value list_val, int start, int end) {
		int len = Value.list_count(list_val);
		if (start < 0) start += len;
		if (end < 0)   end   += len;
		if (start < 0) start = 0;
		if (end > len) end   = len;
		if (start >= end) return make_list(0);
		Value result = make_list(end - start);
		for (int i = start; i < end; i++) Value.list_push(result, Value.list_get(list_val, i));
		return result;
	}

	public static Value list_concat(Value a, Value b) {
		int lenA = Value.list_count(a);
		int lenB = Value.list_count(b);
		Value result = make_list(lenA + lenB);
		for (int i = 0; i < lenA; i++) Value.list_push(result, Value.list_get(a, i));
		for (int i = 0; i < lenB; i++) Value.list_push(result, Value.list_get(b, i));
		return result;
	}

	private static void SortList(GCList list, bool ascending) {
		// Caller must have materialized the list first.
		int n = list.Count();
		if (n < 2) return;
		Value[] tmp = new Value[n];
		for (int i = 0; i < n; i++) tmp[i] = list.Get(i);
		Array.Sort(tmp, (a, b) => {
			int cmp;
			if (a.IsNumber() && b.IsNumber()) {
				cmp = numeric_val(a).CompareTo(numeric_val(b));
			} else if (a.IsString() && b.IsString()) {
				cmp = string_compare(a, b);
			} else {
				int ta = a.IsNumber() ? 0 : a.IsString() ? 1 : 2;
				int tb = b.IsNumber() ? 0 : b.IsString() ? 1 : 2;
				cmp = ta.CompareTo(tb);
			}
			return ascending ? cmp : -cmp;
		});
		for (int i = 0; i < n; i++) list.Set(i, tmp[i]);
	}

	private static void SortListByKey(GCList list, Value byKey, bool ascending) {
		// Caller must have materialized the list first.
		int count = list.Count();
		if (count < 2) return;
		Value[] keys = new Value[count];
		for (int i = 0; i < count; i++) {
			Value elem = list.Get(i);
			if (elem.IsMap()) {
				keys[i] = map_get(elem, byKey);
			} else if (elem.IsList() && byKey.IsNumber()) {
				keys[i] = Value.list_get(elem, (int)numeric_val(byKey));
			} else {
				keys[i] = Value.Null;
			}
		}
		int[] indices = new int[count];
		for (int i = 0; i < count; i++) indices[i] = i;
		Array.Sort(indices, (ia, ib) => {
			Value ka = keys[ia]; Value kb = keys[ib];
			int cmp;
			if (ka.IsNumber() && kb.IsNumber()) {
				cmp = numeric_val(ka).CompareTo(numeric_val(kb));
			} else if (ka.IsString() && kb.IsString()) {
				cmp = string_compare(ka, kb);
			} else {
				int ta = ka.IsNumber() ? 0 : ka.IsString() ? 1 : 2;
				int tb = kb.IsNumber() ? 0 : kb.IsString() ? 1 : 2;
				cmp = ta.CompareTo(tb);
			}
			return ascending ? cmp : -cmp;
		});
		Value[] sorted = new Value[count];
		for (int i = 0; i < count; i++) sorted[i] = list.Get(indices[i]);
		for (int i = 0; i < count; i++) list.Set(i, sorted[i]);
	}

	// ==== MAP OPERATIONS =====================================================
	public static int map_count(Value map_val) {
		if (!map_val.IsMap()) return 0;
		return GCManager.Maps.Get(value_item_index(map_val)).Count();
	}

	public static Value map_get(Value map_val, Value key) {
		if (!map_val.IsMap()) return Value.Null;
		GCManager.Maps.Get(value_item_index(map_val)).TryGet(key, out Value result);
		return result;
	}

	public static bool map_try_get(Value map_val, Value key, out Value value) {
		value = Value.Null;
		if (!map_val.IsMap()) return false;
		return GCManager.Maps.Get(value_item_index(map_val)).TryGet(key, out value);
	}

	public static bool map_lookup(Value map_val, Value key, out Value value) {
		value = Value.Null;
		Value isaKey = Value.magicIsA;
		Value current = map_val;
		for (Int32 depth = 0; depth < 256; depth++) {
			if (!current.IsMap()) return false;
			GCMap m = GCManager.Maps.Get(value_item_index(current));
			if (m.TryGet(key, out value)) return true;
			if (!m.TryGet(isaKey, out Value isa)) return false;
			current = isa;
		}
		return false;
	}

	public static bool map_lookup_with_origin(Value map_val, Value key, out Value value, out Value superVal) {
		value    = Value.Null;
		superVal = Value.Null;
		Value isaKey = Value.magicIsA;
		Value current = map_val;
		for (Int32 depth = 0; depth < 256; depth++) {
			if (!current.IsMap()) return false;
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
		if (!map_val.IsMap()) return false;
		GCMap m = GCManager.Maps.Get(value_item_index(map_val));
		if (m.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return false; }
		m.Set(key, value);
		return true;
	}
	public static bool map_set(Value map_val, string key, Value value) => map_set(map_val, make_string(key), value);
	public static bool map_set(Value map_val, string key, string value) => map_set(map_val, make_string(key), make_string(value));

	public static bool map_remove(Value map_val, Value key) {
		if (!map_val.IsMap()) return false;
		GCMap m = GCManager.Maps.Get(value_item_index(map_val));
		if (m.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return false; }
		return m.Remove(key);
	}

	public static bool map_has_key(Value map_val, Value key) {
		if (!map_val.IsMap()) return false;
		return GCManager.Maps.Get(value_item_index(map_val)).HasKey(key);
	}

	public static void map_clear(Value map_val) {
		if (!map_val.IsMap()) return;
		GCMap m = GCManager.Maps.Get(value_item_index(map_val));
		if (m.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return; }
		m.Clear();
	}

	public static Value map_nth_entry(Value map_val, int n) {
		if (!map_val.IsMap()) return Value.Null;
		GCMap m = GCManager.Maps.Get(value_item_index(map_val));
		int count = 0;
		for (int iter = m.NextEntry(-1); iter != -1; iter = m.NextEntry(iter)) {
			if (count == n) {
				Value result = make_map(4);
				map_set(result, "key",   m.KeyAt(iter));
				map_set(result, "value", m.ValueAt(iter));
				return result;
			}
			count++;
		}
		return Value.Null;
	}

	// ==== MAP ITERATION ======================================================
	// iter = -1: not started.  iter < -1: VarMap register entry index (see GCMap).
	// iter >= 0: hash-table slot.  MAP_ITER_DONE: exhausted.

	public static int map_iter_next(Value map_val, int iter) {
		if (!map_val.IsMap() || iter == MAP_ITER_DONE) return MAP_ITER_DONE;
		int next = GCManager.Maps.Get(value_item_index(map_val)).NextEntry(iter);
		return next == -1 ? MAP_ITER_DONE : next;
	}

	public static Value map_iter_entry(Value map_val, int iter) {
		if (!map_val.IsMap() || iter == MAP_ITER_DONE) return Value.Null;
		GCMap m = GCManager.Maps.Get(value_item_index(map_val));
		Value key = m.KeyAt(iter);
		Value val = m.ValueAt(iter);
		Value result = make_map(4);
		map_set(result, "key",   key);
		map_set(result, "value", val);
		return result;
	}

	// ==== VARMAP OPERATIONS ==================================================
	public static Value make_varmap(List<Value> registers, List<Value> names, int baseIdx, int count) {
		return VarMapBacking.NewVarMap(registers, names, baseIdx, baseIdx + count - 1);
	}

	public static void varmap_gather(Value map_val) {
		if (!map_val.IsMap()) return;
		Int32 idx = value_item_index(map_val);
		VarMapBacking vmb = GCManager.Maps.Get(idx)._vmb;
		if (vmb != null) vmb.Gather(idx);
	}

	public static void varmap_rebind(Value map_val, List<Value> registers, List<Value> names) {
		if (!map_val.IsMap()) return;
		Int32 idx = value_item_index(map_val);
		VarMapBacking vmb = GCManager.Maps.Get(idx)._vmb;
		if (vmb != null) vmb.Rebind(idx, registers, names);
	}

	public static void varmap_map_to_register(Value map_val, Value varName, List<Value> registers, int regIndex) {
		if (!map_val.IsMap()) return;
		Int32 idx = value_item_index(map_val);
		GCMap m = GCManager.Maps.Get(idx);
		if (m._vmb != null) m._vmb.MapToRegister(idx, varName, registers, regIndex);
	}

	// ==== FREEZE OPERATIONS ==================================================
	public static bool is_frozen(Value v) {
		if (v.IsList()) return GCManager.Lists.Get(value_item_index(v)).Frozen;
		if (v.IsMap())  return GCManager.Maps.Get(value_item_index(v)).Frozen;
		return false;
	}

	public static void freeze_value(Value v) {
		if (v.IsList()) {
			Int32 idx = value_item_index(v);
			GCList list = GCManager.Lists.Get(idx);
			if (list.Frozen) return;
			GCManager.Lists.SetFrozen(idx, true);
			if (list.Computed) {
				// Computed-list elements derive from an immutable base; freezing
				// the base covers them all without materializing the list.
				freeze_value(list.Get(0));
			} else {
				int n = list.Count();
				for (int i = 0; i < n; i++) freeze_value(list.Get(i));
			}
		} else if (v.IsMap()) {
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
		if (v.IsList()) {
			GCList src = GCManager.Lists.Get(value_item_index(v));
			if (src.Frozen) return v;
			int srcCount = src.Count();
			Value newList = make_list(srcCount);
			Int32 dstIdx = value_item_index(newList);
			GCList dst = GCManager.Lists.Get(dstIdx);
			GCManager.Lists.SetFrozen(dstIdx, true);
			for (int i = 0; i < srcCount; i++) dst.Push(frozen_copy(src.Get(i)));
			return newList;
		}
		if (v.IsMap()) {
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

	// ==== MAP CONCAT =========================================================
	public static Value map_concat(Value a, Value b) {
		Value result = make_map(0);
		if (a.IsMap()) {
			GCMap mapA = GCManager.Maps.Get(value_item_index(a));
			for (int iter = mapA.NextEntry(-1); iter != -1; iter = mapA.NextEntry(iter))
				map_set(result, mapA.KeyAt(iter), mapA.ValueAt(iter));
		}
		if (b.IsMap()) {
			GCMap mapB = GCManager.Maps.Get(value_item_index(b));
			for (int iter = mapB.NextEntry(-1); iter != -1; iter = mapB.NextEntry(iter))
				map_set(result, mapB.KeyAt(iter), mapB.ValueAt(iter));
		}
		return result;
	}

	// ==== STRING OPERATIONS ==================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int string_length(Value v) {
		if (!v.IsString()) return 0;
		return GetStringValue(v).Length;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int string_indexOf(Value haystack, Value needle, int start_pos) {
		if (!haystack.IsString() || !needle.IsString()) return -1;
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
		if (start >= end) return Value.emptyString;
		return make_string(s.Substring(start, end - start));
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value string_concat(Value a, Value b) {
		if (!a.IsString() || !b.IsString()) return Value.Null;
		return make_string(GetStringValue(a) + GetStringValue(b));
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int string_compare(Value a, Value b) {
		if (!a.IsString() || !b.IsString()) return 0;
		return string.Compare(GetStringValue(a), GetStringValue(b), StringComparison.Ordinal);
	}

	public static Value string_split(Value str, Value delimiter) {
		if (!str.IsString() || !delimiter.IsString()) return Value.Null;
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
		foreach (string part in parts) Value.list_push(list, make_string(part));
		return list;
	}

	public static Value string_replace(Value str, Value from, Value to) {
		if (!str.IsString() || !from.IsString() || !to.IsString()) return Value.Null;
		string s = GetStringValue(str);
		string fromStr = GetStringValue(from);
		if (fromStr == "" || !s.Contains(fromStr)) return str;
		return make_string(s.Replace(fromStr, GetStringValue(to)));
	}

	public static Value string_insert(Value str, int index, Value value, VM vm = null) {
		if (!str.IsString()) return str;
		string s = GetStringValue(str);
		string insertStr = to_String(value, vm);
		if (index < 0) index += s.Length + 1;
		if (index < 0) index = 0;
		if (index > s.Length) index = s.Length;
		return make_string(s.Insert(index, insertStr));
	}

	public static Value string_split_max(Value str, Value delimiter, int maxCount) {
		if (!str.IsString() || !delimiter.IsString()) return Value.Null;
		string s = GetStringValue(str);
		string delim = GetStringValue(delimiter);
		Value list = make_list(8);
		if (delim == "") {
			int count = 0;
			for (int i = 0; i < s.Length; i++) {
				if (maxCount > 0 && count >= maxCount - 1) {
					Value.list_push(list, make_string(s.Substring(i)));
					return list;
				}
				Value.list_push(list, make_string(s[i].ToString()));
				count++;
			}
			return list;
		}
		int pos = 0; int found = 0;
		while (pos <= s.Length) {
			int next = s.IndexOf(delim, pos, StringComparison.Ordinal);
			if (next < 0 || (maxCount > 0 && found >= maxCount - 1)) {
				Value.list_push(list, make_string(s.Substring(pos)));
				break;
			}
			Value.list_push(list, make_string(s.Substring(pos, next - pos)));
			pos = next + delim.Length;
			found++;
			if (pos > s.Length) break;
			if (pos == s.Length) { Value.list_push(list, make_string("")); break; }
		}
		return list;
	}

	public static Value string_replace_max(Value str, Value from, Value to, int maxCount) {
		if (!str.IsString() || !from.IsString() || !to.IsString()) return Value.Null;
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
		if (!str.IsString()) return str;
		return make_string(GetStringValue(str).ToUpper());
	}

	public static Value string_lower(Value str) {
		if (!str.IsString()) return str;
		return make_string(GetStringValue(str).ToLower());
	}

	public static Value string_from_code_point(int codePoint) =>
		make_string(char.ConvertFromUtf32(codePoint));

	public static int string_code_point(Value str) {
		if (!str.IsString()) return 0;
		string s = GetStringValue(str);
		if (s.Length == 0) return 0;
		return char.ConvertToUtf32(s, 0);
	}

	// ── Map iterator (struct-based) ───────────────────────────────────────────
	public static MapIterator map_iterator(Value map_val) {
		MapIterator it = new MapIterator();
		it.Key = Value.Null;
		it.Val = Value.Null;
		if (map_val.IsMap()) {
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

	// ==== OVERRIDES ==========================================================
	[Obsolete("Use to_String(v, vm) instead")]
	public override string ToString() => to_String(this);

	// Equals/GetHashCode embody the same semantics as MiniScript ==, so that
	// Dictionary<Value, Value> hashes/compares keys correctly. In particular,
	// strings compare by content (not pointer/index).
	public override bool Equals(object obj) {
		if (obj is Value v) return this == v;
		return false;
	}

	public override int GetHashCode() {
		if (this.IsString()) {
			string s = GCManager.GetStringContent(this);
			return s.GetHashCode(System.StringComparison.Ordinal);
		}
		return (int)(_u ^ (_u >> 32));
	}
	
	// And Hash, provided for compatibility with 1.0.
	public int Hash() { return GetHashCode(); }
}


}
//*** END CS_ONLY ***
