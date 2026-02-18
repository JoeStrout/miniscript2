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
// - an encoded immediate (int, tiny string), OR
// - a tagged 32-bit handle to heap-managed objects (string/list/map).
//
// Keep Value at 8 bytes, blittable, and aggressively inlined.

[StructLayout(LayoutKind.Explicit, Size = 8)]
public readonly struct Value {
	// Overlaid views enable single-move bit casts on CoreCLR.
	[FieldOffset(0)] private readonly ulong _u;
	[FieldOffset(0)] private readonly double _d;

	private Value(ulong u) { _d = 0; _u = u; }

	// ==== TAGS & MASKS (EDIT TO MATCH YOUR C EXACTLY) =====================
	// High 16 bits used to tag NaN-ish payloads.
	private const ulong NANISH_MASK     = 0xFFFF_0000_0000_0000UL;
	private const ulong NULL_VALUE      = 0xFFF1_0000_0000_0000UL; // null singleton (our lowest reserved NaN pattern)
	private const ulong INTEGER_TAG     = 0xFFFA_0000_0000_0000UL; // Int32 tag
	private const ulong FUNCREF_TAG     = 0xFFFB_0000_0000_0000UL; // function reference tag
	private const ulong MAP_TAG         = 0xFFFC_0000_0000_0000UL; // map tag
	private const ulong LIST_TAG        = 0xFFFD_0000_0000_0000UL; // list tag
	private const ulong STRING_TAG      = 0xFFFE_0000_0000_0000UL; // heap string tag
	private const ulong TINY_STRING_TAG = 0xFFFF_0000_0000_0000UL; // tiny string tag

	// ==== CONSTRUCTORS ====================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value Null() => new(NULL_VALUE);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value FromInt(int i) => new(INTEGER_TAG | (uint)i);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value FromDouble(double d) {
		// Unity/Mono/IL2CPP-safe: use BitConverter, no Unsafe dependency.
		ulong bits = (ulong)BitConverter.DoubleToInt64Bits(d);
		return new(bits);
	}

	// Tiny ASCII string: stores length (low 8 bits) + up to 5 bytes data in bits 8..48.
	// High bits carry TINY_STRING_TAG so the tag check is a single AND/compare.
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value FromTinyAscii(ReadOnlySpan<byte> s) {
		int len = s.Length;
		if ((uint)len > 5u) throw new ArgumentOutOfRangeException(nameof(s), "Tiny string max 5 bytes");
		ulong u = TINY_STRING_TAG | (ulong)((uint)len & 0xFFU);
		for (int i = 0; i < len; i++) {
			u |= (ulong)((byte)s[i]) << (8 * (i + 1));
		}
		return new(u);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value FromString(string s) {
		if (s.Length <= 5 && IsAllAscii(s)) {
			Span<byte> tmp = stackalloc byte[5];
			for (int i = 0; i < s.Length; i++) tmp[i] = (byte)s[i];
			return FromTinyAscii(tmp[..s.Length]);
		}
		int h = HandlePool.Add(s);
		return FromHandle(STRING_TAG, h);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value FromList(object list) { // typically List<Value> or a custom IList
		int h = HandlePool.Add(list);
		return FromHandle(LIST_TAG, h);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value FromMap(object map) {// typically Dictionary<Value,Value> or custom
		int h = HandlePool.Add(map);
		return FromHandle(MAP_TAG, h);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value FromFuncRef(ValueFuncRef funcRefObj) {
		int h = HandlePool.Add(funcRefObj);
		return FromHandle(FUNCREF_TAG, h);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value FromHandle(ulong tagMask, int handle)
		=> new(tagMask | (uint)handle);

	// ==== TYPE PREDICATES =================================================
	public bool IsNull   { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => _u == NULL_VALUE; }
	public bool IsInt	{ [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) == INTEGER_TAG; }
	public bool IsTiny   { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & TINY_STRING_TAG) == TINY_STRING_TAG && (_u & NANISH_MASK) != STRING_TAG && (_u & NANISH_MASK) != LIST_TAG && (_u & NANISH_MASK) != MAP_TAG && (_u & NANISH_MASK) != INTEGER_TAG && (_u & NANISH_MASK) != FUNCREF_TAG && _u != NULL_VALUE; }
	public bool IsHeapString { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) == STRING_TAG; }
	public bool IsString { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & STRING_TAG) == STRING_TAG; }
	public bool IsFuncRef { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) == FUNCREF_TAG; }
	public bool IsList   { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) == LIST_TAG; }
	public bool IsMap	{ [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) == MAP_TAG; }
	public bool IsDouble { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) < NULL_VALUE; }

	// ==== ACCESSORS =======================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public int AsInt() => (int)_u;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public ValueFuncRef AsFuncRefObject() => HandlePool.Get(Handle()) as ValueFuncRef;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public double AsDouble() {
		long bits = (long)_u;
		return BitConverter.Int64BitsToDouble(bits);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public int Handle() => (int)_u;

	// Tiny decode helpers
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public int TinyLen() => (int)(_u & 0xFF);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public void TinyCopyTo(Span<byte> dst) {
		int len = TinyLen();
		for (int i = 0; i < len; i++) dst[i] = (byte)((_u >> (8 * (i + 1))) & 0xFF);
	}

	public override string ToString()  {
		if (IsNull) return "null";
		if (IsInt) return AsInt().ToString();
		if (IsDouble) {
			double value = AsDouble();
			if (value % 1.0 == 0.0) {
				// integer values as integers
				string result = value.ToString("0", CultureInfo.InvariantCulture);
				if (result == "-0") result = "0";
				return result;
			} else if (value > 1E10 || value < -1E10 || (value < 1E-6 && value > -1E-6)) {
				// very large/small numbers in exponential form
				string s = value.ToString("E6", CultureInfo.InvariantCulture);
				s = s.Replace("E-00", "E-0");
				return s;
			} else {
				// all others in decimal form, with 1-6 digits past the decimal point
				string result = value.ToString("0.0#####", CultureInfo.InvariantCulture);
				if (result == "-0") result = "0";
				return result;
			}
		}
		if (IsString) {
			if (IsTiny) {
				Span<byte> b = stackalloc byte[5];
				TinyCopyTo(b);
				return System.Text.Encoding.ASCII.GetString(b[..TinyLen()]);
			}
			return HandlePool.Get(Handle()) as string ?? "<str?>";
		}
		if (IsList) {
			var valueList = HandlePool.Get(Handle()) as ValueList;
			if (valueList == null) return "<list?>";
			
			var items = new string[valueList.Count];
			for (int i = 0; i < valueList.Count; i++) {
				Value item = valueList.Get(i);
				// ToDo: watch out for recursion, or maybe just limit our depth in
				// general.  I think MS1.0 limits nesting to 16 levels deep.  But
				// whatever we do, we shouldn't just crash with a stack overflow.
				items[i] = ValueHelpers.value_repr(item).ToString();
			}
			return "[" + string.Join(", ", items) + "]";
		}
		if (IsMap) {
			var valueMap = HandlePool.Get(Handle()) as ValueMap;
			if (valueMap == null) return "<map?>";

			var items = new string[valueMap.Count];
			int i = 0;
			foreach (var kvp in valueMap.Items) {
				// ToDo: watch out for recursion, or maybe just limit our depth in
				// general.  I think MS1.0 limits nesting to 16 levels deep.  But
				// whatever we do, we shouldn't just crash with a stack overflow.
				string keyStr = ValueHelpers.value_repr(kvp.Key).ToString();
				string valueStr = ValueHelpers.value_repr(kvp.Value).ToString();
				items[i] = keyStr + ": " + valueStr;
				i++;
			}

			return "{" + string.Join(", ", items) + "}";
		}
		if (IsFuncRef) {
			var funcRefObj = AsFuncRefObject();
			if (funcRefObj == null) return "<funcref?>";
			return funcRefObj.ToString();
		}
		return "<value>";
	}

	public ulong Bits => _u; // sometimes useful for hashing/equality

	// ==== ARITHMETIC & COMPARISON (subset; extend as needed) ==============
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value Add(Value a, Value b) {
		if (a.IsInt && b.IsInt) {
			long r = (long)a.AsInt() + b.AsInt();
			if ((uint)r == r) return FromInt((int)r);
			return FromDouble((double)r);
		}
		if ((a.IsInt || a.IsDouble) && (b.IsInt || b.IsDouble)) {
			double da = a.IsInt ? a.AsInt() : a.AsDouble();
			double db = b.IsInt ? b.AsInt() : b.AsDouble();
			return FromDouble(da + db);
		}
		// Handle string concatenation
		if (a.IsString) {
			if (b.IsString) return ValueHelpers.string_concat(a, b);
			if (b.IsInt || b.IsDouble) return ValueHelpers.string_concat(a, ValueHelpers.make_string(b.ToString()));
		} else if(b.IsString) {
			if (a.IsInt || a.IsDouble) return ValueHelpers.string_concat(ValueHelpers.make_string(a.ToString()), b);
		}
		// string concat, list append, etc. can be added here.
		return Null();
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value Multiply(Value a, Value b) {
		if (a.IsInt && b.IsInt) {
			long r = (long)a.AsInt() * b.AsInt();
			if ((uint)r == r) return FromInt((int)r);
			return FromDouble((double)r);
		}
		if (is_number(a) && is_number(b)) {
			double da = a.IsInt ? a.AsInt() : a.AsDouble();
			double db = b.IsInt ? b.AsInt() : b.AsDouble();
			return FromDouble(da * db);
		}
		// TODO: String support not added yet!

		// Handle string repetition: string * int or int * string
		if (a.IsString && b.IsInt) {
			int count = b.AsInt();
			if (count <= 0) return FromString("");
			if (count == 1) return a;
			
			// Build repeated string
			Value result = a;
			for (int i = 1; i < count; i++) {
				result = ValueHelpers.string_concat(result, a);
			}
			return result;
		} else if (is_string(a) && is_double(b)) {
			int repeats = 0;
			int extraChars = 0;
			double factor = as_double(b);
			if (double.IsNaN(factor) || double.IsInfinity(factor)) return Null();
			if (factor <= 0) return val_empty_string;
			repeats = (int)factor;
			// TODO: Do we need to check Max length of a string like in 1.0?

			Value result = val_empty_string;
			for (int i = 0; i < repeats; i++) {
				result = ValueHelpers.string_concat(result, a);
			}
			extraChars = (int)(ValueHelpers.string_length(a) * (factor - repeats));
			if (extraChars > 0) result = ValueHelpers.string_concat(result, ValueHelpers.string_substring(a, 0, extraChars));
			return result;
		}
		// string concat, list append, etc. can be added here.
		return Null();
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value Divide(Value a, Value b) {
		// MiniScript division always returns a float/double
		if ((a.IsInt || a.IsDouble) && (b.IsInt || b.IsDouble)) {
			double da = a.IsInt ? a.AsInt() : a.AsDouble();
			double db = b.IsInt ? b.AsInt() : b.AsDouble();
			return FromDouble(da / db);
		}
		if (is_string(a) && is_number(b)) {
			// We'll just call through to value_mult for this, with a factor of 1/b.
			return value_mult(a, value_div(make_double(1), b));
		}
		// string concat, list append, etc. can be added here.
		return Null();
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value Mod(Value a, Value b) {
		if (a.IsInt && b.IsInt) {
			long r = (long)a.AsInt() % b.AsInt();
			if ((uint)r == r) return FromInt((int)r);
			return FromDouble((double)r);
		}
		if ((a.IsInt || a.IsDouble) && (b.IsInt || b.IsDouble)) {
			double da = a.IsInt ? a.AsInt() : a.AsDouble();
			double db = b.IsInt ? b.AsInt() : b.AsDouble();
			return FromDouble(da % db);
		}
		// TODO: String support not added yet!
		// string concat, list append, etc. can be added here.
		return Null();
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value Sub(Value a, Value b) {
		if (a.IsInt && b.IsInt) {
			long r = (long)a.AsInt() - b.AsInt();
			if (r >= int.MinValue && r <= int.MaxValue) return FromInt((int)r);
			return FromDouble((double)r);
		}
		if ((a.IsInt || a.IsDouble) && (b.IsInt || b.IsDouble)) {
			double da = a.IsInt ? a.AsInt() : a.AsDouble();
			double db = b.IsInt ? b.AsInt() : b.AsDouble();
			return FromDouble(da - db);
		}
		if (a.IsString && b.IsString) {
			string sa = a.IsTiny ? a.ToString() : HandlePool.Get(a.Handle()) as string;
			string sb = b.IsTiny ? b.ToString() : HandlePool.Get(b.Handle()) as string;
			if (sb.Length > 0 && sa.EndsWith(sb)) {
				return FromString(sa.Substring(0, sa.Length - sb.Length));
			}
			return a;
		}
		return Null();
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool LessThan(Value a, Value b) {
		if ((a.IsInt || a.IsDouble) && (b.IsInt || b.IsDouble)) {
			double da = a.IsInt ? a.AsInt() : a.AsDouble();
			double db = b.IsInt ? b.AsInt() : b.AsDouble();
			return da < db;
		}
		// Handle string comparison
		if (a.IsString && b.IsString) {
			return ValueHelpers.string_compare(a, b) < 0;
		}
		return false;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool LessThanOrEqual(Value a, Value b) {
		if ((a.IsInt || a.IsDouble) && (b.IsInt || b.IsDouble)) {
			double da = a.IsInt ? a.AsInt() : a.AsDouble();
			double db = b.IsInt ? b.AsInt() : b.AsDouble();
			return da <= db;
		}
		// Handle string comparison
		if (a.IsString && b.IsString) {
			return ValueHelpers.string_compare(a, b) <= 0;
		}
		return false;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool Identical(Value a, Value b)  {
		return (a._u == b._u);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool Equal(Value a, Value b)  {
		// Fast path: identical bits
		if (a._u == b._u) return true;

		// If both numeric, compare numerically (handles int/double mix)
		if ((a.IsInt || a.IsDouble) && (b.IsInt || b.IsDouble)) {
			double da = a.IsInt ? a.AsInt() : a.AsDouble();
			double db = b.IsInt ? b.AsInt() : b.AsDouble();
			return da == db; // Note: NaN == NaN is false, matching IEEE
		}

		// Both tiny strings => byte compare via bits when lengths equal
		if (a.IsTiny && b.IsTiny) return a._u == b._u;

		// Heap strings via handle indirection
		if (a.IsString && b.IsString) {
			string sa = a.IsTiny ? a.ToString() : HandlePool.Get(a.Handle()) as string;
			string sb = b.IsTiny ? b.ToString() : HandlePool.Get(b.Handle()) as string;
			return string.Equals(sa, sb, StringComparison.Ordinal);
		}

		// Null only equals Null
		if (a.IsNull || b.IsNull) return a.IsNull && b.IsNull;

		// Lists: compare by content
		// (ToDo: limit recursion in case of circular references)
		if (a.IsList && b.IsList) {
			int countA = list_count(a);
			if (countA != list_count(b)) return false;
			for (int i = 0; i < countA; i++) {
				if (!value_equal(list_get(a, i), list_get(b, i))) return false;
			}
			return true;
		}

		// Maps: compare by content
		// (ToDo: limit recursion in case of circular references)
		if (a.IsMap && b.IsMap) {
			var mapA = HandlePool.Get(a.Handle()) as ValueMap;
			var mapB = HandlePool.Get(b.Handle()) as ValueMap;
			if (mapA == null || mapB == null) return false;
			if (mapA.Count != mapB.Count) return false;
			foreach (var kvp in mapA.Items) {
				if (!mapB.HasKey(kvp.Key)) return false;
				if (!value_equal(kvp.Value, mapB.Get(kvp.Key))) return false;
			}
			return true;
		}

		return false;
	}

	// ==== HELPERS =========================================================
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	private static bool IsAllAscii(string s) {
		foreach (char c in s) if (c > 0x7F) return false;
		return true;
	}
}

// Global helper functions to match C++ value.h interface
// ToDo: take out most of the stuff above and have *only* these interfaces,
// so we don't have two ways to do things in the C# code (only one of which
// actually works in any transpiled code).
public static class ValueHelpers {

	// Common constant values (matching value.h)
	public static Value val_null = Value.Null();
	public static Value val_zero = Value.FromInt(0);
	public static Value val_one = Value.FromInt(1);
	public static Value val_empty_string = Value.FromString("");
	public static Value val_isa_key = Value.FromString("__isa");
	public static Value val_self = Value.FromString("self");
	public static Value val_super = Value.FromString("super");

	// Core value creation functions (matching value.h)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_null() => val_null;
	
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_int(int i) => Value.FromInt(i);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_int(bool b) => Value.FromInt(b ? 1 : 0);
	
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_double(double d) => Value.FromDouble(d);
	
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_string(string str) => Value.FromString(str);
	
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_list(int initial_capacity) {
		var list = new ValueList();
		return Value.FromList(list);
	}
	
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_empty_list() => make_list(0);
	
	// List operations (matching value_list.h)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int list_count(Value list_val) {
		if (!list_val.IsList) return 0;
		var valueList = HandlePool.Get(list_val.Handle()) as ValueList;
		return valueList?.Count ?? 0;
	}
	
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value list_get(Value list_val, int index) {
		if (!list_val.IsList) return make_null();
		var valueList = HandlePool.Get(list_val.Handle()) as ValueList;
		return valueList?.Get(index) ?? make_null();
	}
	
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static void list_set(Value list_val, int index, Value item) {
		if (!list_val.IsList) return;
		var valueList = HandlePool.Get(list_val.Handle()) as ValueList;
		if (valueList == null) return;
		if (valueList.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		valueList.Set(index, item);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static void list_push(Value list_val, Value item) {
		if (!list_val.IsList) return;
		var valueList = HandlePool.Get(list_val.Handle()) as ValueList;
		if (valueList == null) return;
		if (valueList.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return; }
		valueList.Add(item);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool list_remove(Value list_val, int index) {
		if (!list_val.IsList) return false;
		var valueList = HandlePool.Get(list_val.Handle()) as ValueList;
		if (valueList == null) return false;
		if (valueList.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen list"); return false; }
		return valueList.Remove(index);
	}

	// Map functions (matching value_map.h)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_map(int initial_capacity) {
		var map = new ValueMap();
		return Value.FromMap(map);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_empty_map() => make_map(8);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_funcref(Int32 funcIndex) => Value.FromFuncRef(new ValueFuncRef(funcIndex));

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value make_funcref(Int32 funcIndex, Value outerVars) => Value.FromFuncRef(new ValueFuncRef(funcIndex, outerVars));

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Int32 funcref_index(Value v) {
		var funcRefObj = v.AsFuncRefObject();
		return funcRefObj?.FuncIndex ?? -1;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value funcref_outer_vars(Value v) {
		var funcRefObj = v.AsFuncRefObject();
		return funcRefObj?.OuterVars ?? make_null();
	}

	public static int map_count(Value map_val) {
		if (!map_val.IsMap) return 0;
		var valueMap = HandlePool.Get(map_val.Handle()) as ValueMap;
		return valueMap?.Count ?? 0;
	}

	public static Value map_get(Value map_val, Value key) {
		if (!map_val.IsMap) return make_null();
		var valueMap = HandlePool.Get(map_val.Handle()) as ValueMap;
		return valueMap?.Get(key) ?? make_null();
	}

	// Look up a key directly in map_val only (not walking the __isa chain).
	// Returns true if found (with value in out parameter), false otherwise.
	public static bool map_try_get(Value map_val, Value key, out Value value) {
		value = make_null();
		if (!map_val.IsMap) return false;
		var valueMap = HandlePool.Get(map_val.Handle()) as ValueMap;
		if (valueMap == null) return false;

		if (valueMap.HasKey(key)) {
			value = valueMap.Get(key);
			return true;
		}
		return false;
	}

	// Look up a key in a map, walking the __isa chain if needed.
	// Returns true if found (with value in out parameter), false otherwise.
	public static bool map_lookup(Value map_val, Value key, out Value value) {
		value = make_null();
		Value isaKey = val_isa_key;
		Value current = map_val;
		for (Int32 depth = 0; depth < 256; depth++) {
			if (!is_map(current)) return false;
			var valueMap = HandlePool.Get(current.Handle()) as ValueMap;
			if (valueMap == null) return false;
			if (valueMap.HasKey(key)) {
				value = valueMap.Get(key);
				return true;
			}
			// Walk up __isa chain
			if (!valueMap.HasKey(isaKey)) return false;
			current = valueMap.Get(isaKey);
		}
		return false;
	}

	// Look up a key in a map walking the __isa chain, and also return the __isa
	// of the map where the key was found (for computing 'super').
	// Returns true if found, false otherwise.
	public static bool map_lookup_with_origin(Value map_val, Value key, out Value value, out Value superVal) {
		value = make_null();
		superVal = make_null();
		Value isaKey = val_isa_key;
		Value current = map_val;
		for (Int32 depth = 0; depth < 256; depth++) {
			if (!is_map(current)) return false;
			var valueMap = HandlePool.Get(current.Handle()) as ValueMap;
			if (valueMap == null) return false;
			if (valueMap.HasKey(key)) {
				value = valueMap.Get(key);
				// super = the __isa of the map where we found it
				if (valueMap.HasKey(isaKey)) {
					superVal = valueMap.Get(isaKey);
				}
				return true;
			}
			// Walk up __isa chain
			if (!valueMap.HasKey(isaKey)) return false;
			current = valueMap.Get(isaKey);
		}
		return false;
	}

	public static bool map_set(Value map_val, Value key, Value value) {
		if (!map_val.IsMap) return false;
		var valueMap = HandlePool.Get(map_val.Handle()) as ValueMap;
		if (valueMap == null) return false;
		if (valueMap.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return false; }
		return valueMap.Set(key, value);
	}

	public static bool map_remove(Value map_val, Value key) {
		if (!map_val.IsMap) return false;
		var valueMap = HandlePool.Get(map_val.Handle()) as ValueMap;
		if (valueMap == null) return false;
		if (valueMap.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return false; }
		return valueMap.Remove(key);
	}

	public static bool map_has_key(Value map_val, Value key) {
		if (!map_val.IsMap) return false;
		var valueMap = HandlePool.Get(map_val.Handle()) as ValueMap;
		return valueMap?.HasKey(key) ?? false;
	}

	public static void map_clear(Value map_val) {
		if (!map_val.IsMap) return;
		var valueMap = HandlePool.Get(map_val.Handle()) as ValueMap;
		if (valueMap == null) return;
		if (valueMap.Frozen) { VM.ActiveVM().RaiseRuntimeError("Attempt to modify a frozen map"); return; }
		valueMap.Clear();
	}
	
	public static void varmap_gather(Value map_val) {
		if (!map_val.IsMap) return;
		var varMap = HandlePool.Get(map_val.Handle()) as VarMap;
		varMap?.Gather();
	}

	// Frozen value helpers
	public static bool is_frozen(Value v) {
		if (v.IsList) {
			var valueList = HandlePool.Get(v.Handle()) as ValueList;
			return valueList != null && valueList.Frozen;
		}
		if (v.IsMap) {
			var valueMap = HandlePool.Get(v.Handle()) as ValueMap;
			return valueMap != null && valueMap.Frozen;
		}
		return false;
	}

	public static void freeze_value(Value v) {
		if (v.IsList) {
			var valueList = HandlePool.Get(v.Handle()) as ValueList;
			if (valueList == null || valueList.Frozen) return;
			valueList.Frozen = true;
			for (int i = 0; i < valueList.Count; i++) {
				freeze_value(valueList.Get(i));
			}
		} else if (v.IsMap) {
			var valueMap = HandlePool.Get(v.Handle()) as ValueMap;
			if (valueMap == null || valueMap.Frozen) return;
			valueMap.Frozen = true;
			foreach (var kvp in valueMap.Items) {
				freeze_value(kvp.Key);
				freeze_value(kvp.Value);
			}
		}
	}

	public static Value frozen_copy(Value v) {
		if (v.IsList) {
			var valueList = HandlePool.Get(v.Handle()) as ValueList;
			if (valueList == null || valueList.Frozen) return v;
			Value newList = make_list(valueList.Count);
			var newValueList = HandlePool.Get(newList.Handle()) as ValueList;
			newValueList.Frozen = true;
			for (int i = 0; i < valueList.Count; i++) {
				newValueList.Add(frozen_copy(valueList.Get(i)));
			}
			return newList;
		}
		if (v.IsMap) {
			var valueMap = HandlePool.Get(v.Handle()) as ValueMap;
			if (valueMap == null || valueMap.Frozen) return v;
			Value newMap = make_map(valueMap.Count);
			var newValueMap = HandlePool.Get(newMap.Handle()) as ValueMap;
			newValueMap.Frozen = true;
			foreach (var kvp in valueMap.Items) {
				newValueMap.Set(frozen_copy(kvp.Key), frozen_copy(kvp.Value));
			}
			return newMap;
		}
		return v;
	}

	// Value representation function (for literal representation)
	public static Value value_repr(Value v) {
		if (v.IsString) {
			// For strings, return quoted representation with internal quotes doubled
			string content = v.ToString();
			string escaped = content.Replace("\"", "\"\"");  // Double internal quotes
			return make_string("\"" + escaped + "\"");
		} else {
			// For everything else, use normal string representation
			return make_string(v.ToString());
		}
	}

	// Core value extraction functions (matching value.h)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int as_int(Value v) => v.AsInt();
	
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static double as_double(Value v) => v.AsDouble();
	
	// Core type checking functions (matching value.h)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_null(Value v) => v.IsNull;
	
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_int(Value v) => v.IsInt;
	
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
	public static bool is_tiny_string(Value v) => v.IsTiny;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static String as_cstring(Value v) {
		if (!v.IsString) return "";
		return GetStringValue(v);
	}
	
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_number(Value v) => v.IsInt || v.IsDouble;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool is_truthy(Value v) => (!is_null(v) &&
			((is_int(v) && as_int(v) != 0) ||
			(is_double(v) && as_double(v) != 0.0) ||
			(is_string(v) && ValueHelpers.string_length(v) != 0)
			));
	
	
	// Arithmetic operations (matching value.h)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_add(Value a, Value b) => Value.Add(a, b);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_mult(Value a, Value b) => Value.Multiply(a, b);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_div(Value a, Value b) => Value.Divide(a, b);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_mod(Value a, Value b) => Value.Mod(a, b);
	
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_sub(Value a, Value b) => Value.Sub(a, b);

	// Fuzzy logic operations
	// Helper: convert a value to a fuzzy truth value (0-1)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Double ToFuzzyBool(Value v) {
		if (is_int(v)) return (Double)as_int(v);
		if (is_double(v)) return as_double(v);
		// For non-numeric values, use boolean truth: truthy = 1, falsey = 0
		return is_truthy(v) ? 1.0 : 0.0;
	}

	// Helper: take the absolute value, and then clamp to [0-1]
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Double AbsClamp01(Double d) {
		if (d < 0) d = -d;
		if (d > 1) return 1;
		return d;
	}


	// AND: AbsClamp01(a * b)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_and(Value a, Value b) {
		Double fA = ToFuzzyBool(a);
		Double fB = ToFuzzyBool(b);
		return make_double(AbsClamp01(fA * fB));
	}

	// OR: AbsClamp01(a + b - a*b)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_or(Value a, Value b) {
		Double fA = ToFuzzyBool(a);
		Double fB = ToFuzzyBool(b);
		return make_double(AbsClamp01(fA + fB - fA * fB));
	}

	// NOT: 1 - AbsClamp01(a)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value value_not(Value a) {
		Double fA = ToFuzzyBool(a);
		return make_double(1.0 - AbsClamp01(fA));
	}

	// Comparison operations (matching value.h)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_identical(Value a, Value b) => Value.Identical(a, b);

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_lt(Value a, Value b) => Value.LessThan(a, b);

	// Comparison operations (matching value.h)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_le(Value a, Value b) => Value.LessThanOrEqual(a, b);

	// Comparison operations (matching value.h)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static bool value_equal(Value a, Value b) => Value.Equal(a, b);		

	// Conversion operations (matching value.h)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value to_string(Value a) => make_string(a.ToString());

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value to_number(Value a) {
		try {
			double result = double.Parse(a.ToString());
			if (result % 1 == 0 && Int32.MinValue <= result 
				&& result <= Int32.MaxValue) return make_int((int)result);
			return make_double(result);
		} catch {
			return val_zero;
		}
	}
	
	public static Value make_varmap(List<Value> registers, List<Value> names, int baseIdx, int count) {
		VarMap varmap = new VarMap(registers, names, baseIdx, baseIdx + count - 1);
		return Value.FromMap(varmap);
	}

	// String helper
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	private static string GetStringValue(Value val) {
		if (val.IsTiny) return val.ToString();
		if (val.IsHeapString) return HandlePool.Get(val.Handle()) as string ?? "";
		return "";
	}

	// String operations (matching value_string.h)
	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int string_length(Value v) {
		if (!v.IsString) return 0;
		return GetStringValue(v).Length;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int string_indexOf(Value haystack, Value needle, int start_pos) {
		if (!haystack.IsString || !needle.IsString) return -1;
		string h = GetStringValue(haystack);
		string n = GetStringValue(needle);
		return h.IndexOf(n, start_pos);
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
		if (end < 0) end += len;
		if (start < 0) start = 0;
		if (end > len) end = len;
		if (start >= end) return val_empty_string;
		return make_string(s.Substring(start, end - start));
	}

	public static Value list_slice(Value list_val, int start, int end) {
		int len = list_count(list_val);
		if (start < 0) start += len;
		if (end < 0) end += len;
		if (start < 0) start = 0;
		if (end > len) end = len;
		if (start >= end) return make_list(0);
		Value result = make_list(end - start);
		for (int i = start; i < end; i++) {
			list_push(result, list_get(list_val, i));
		}
		return result;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static Value string_concat(Value a, Value b) {
		if (!a.IsString || !b.IsString) return val_null;
		string sa = GetStringValue(a);
		string sb = GetStringValue(b);
		return make_string(sa + sb);
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int string_compare(Value a, Value b) {
		if (!a.IsString || !b.IsString) return 0;
		string sa = GetStringValue(a);
		string sb = GetStringValue(b);
		return string.Compare(sa, sb, StringComparison.Ordinal);
	}

	public static Value string_split(Value str, Value delimiter) {
		if (!str.IsString || !delimiter.IsString) return val_null;

		string s = GetStringValue(str);
		string delim = GetStringValue(delimiter);

		string[] parts;
		if (delim == "") {
			// Split into characters
			parts = new string[s.Length];
			for (int i = 0; i < s.Length; i++) {
				parts[i] = s[i].ToString();
			}
		} else {
			parts = s.Split(new string[] { delim }, StringSplitOptions.None);
		}

		Value list = make_list(parts.Length);
		foreach (string part in parts) {
			list_push(list, make_string(part));
		}

		return list;
	}

	public static Value string_replace(Value str, Value from, Value to) {
		if (!str.IsString || !from.IsString || !to.IsString) return val_null;

		string s = GetStringValue(str);
		string fromStr = GetStringValue(from);
		string toStr = GetStringValue(to);

		if (fromStr == "") {
			return str; // Can't replace empty string
		}
		if (!s.Contains(fromStr)) {
			return str; // Return original if no match
		}
		string result = s.Replace(fromStr, toStr);
		return make_string(result);
	}
}

// A minimal, fast handle table. Stores actual C# objects referenced by Value.
// All heap-backed Value variants carry a 32-bit index into this pool.
internal static class HandlePool {
	private static object[] _objs = new object[1024];
	private static int _count;

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static int Add(object o) {
		int idx = _count;
		if ((uint)idx >= (uint)_objs.Length)
			Array.Resize(ref _objs, _objs.Length << 1);
		_objs[idx] = o;
		_count = idx + 1;
		return idx;
	}

	[MethodImpl(MethodImplOptions.AggressiveInlining)]
	public static object Get(int h) => (uint)h < (uint)_count ? _objs[h] : null;
	
	public static int GetCount() => _count;
}

// List implementation for Value lists
public class ValueList {
	private List<Value> _items = new List<Value>();
	public bool Frozen;

	public int Count => _items.Count;
	
	public void Add(Value item) => _items.Add(item);
	
	public Value Get(int index) {
		if (index < 0) index += _items.Count;
		if (index < 0 || index >= _items.Count) return val_null;
		return _items[index];
	}
	
	public void Set(int index, Value value) {
		if (index < 0) index += _items.Count;
		if (index >= 0 && index < _items.Count)
			_items[index] = value;
	}
	
	public int IndexOf(Value item) {
		for (int i = 0; i < _items.Count; i++) {
			if (Value.Equal(_items[i], item)) return i;
		}
		return -1;
	}
	
	public bool Remove(int index) {
		if (index < 0) index += _items.Count;
		if (index < 0 || index >= _items.Count) return false;
		_items.RemoveAt(index);
		return true;
	}
}

public class ValueMap {
	protected Dictionary<Value, Value> _items = new Dictionary<Value, Value>(new ValueEqualityComparer());
	public bool Frozen;

	public virtual int Count => _items.Count;

	public virtual Value Get(Value key) {
		if (_items.TryGetValue(key, out Value value)) {
			return value;
		}
		return val_null;
	}

	public virtual bool Set(Value key, Value value) {
		_items[key] = value;
		return true;
	}

	public virtual bool Remove(Value key) {
		return _items.Remove(key);
	}

	public virtual bool HasKey(Value key) {
		return _items.ContainsKey(key);
	}

	public virtual void Clear() {
		_items.Clear();
	}

	// For iteration support
	public virtual IEnumerable<KeyValuePair<Value, Value>> Items => _items;
}

}
