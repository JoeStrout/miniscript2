//*** BEGIN CS_ONLY ***
// (This entire file is only for C#; the C++ code uses value.h/.c instead.)

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

using static MiniScript.ValueHelpers;
using static MiniScript.StringOperations;

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
		private const ulong val_null      = 0xFFF1_0000_0000_0000UL; // null singleton (our lowest reserved NaN pattern)
		private const ulong INTEGER_TAG     = 0xFFFA_0000_0000_0000UL; // Int32 tag
		private const ulong FUNCREF_TAG     = 0xFFFB_0000_0000_0000UL; // function reference tag
		private const ulong MAP_TAG         = 0xFFFC_0000_0000_0000UL; // map tag
		private const ulong LIST_TAG        = 0xFFFD_0000_0000_0000UL; // list tag
		private const ulong STRING_TAG      = 0xFFFE_0000_0000_0000UL; // heap string tag
		private const ulong TINY_STRING_TAG = 0xFFFF_0000_0000_0000UL; // tiny string tag

		// ==== CONSTRUCTORS ====================================================
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Value Null() => new(val_null);

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
		public bool IsNull   { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => _u == val_null; }
		public bool IsInt	{ [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) == INTEGER_TAG; }
		public bool IsTiny   { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & TINY_STRING_TAG) == TINY_STRING_TAG && (_u & NANISH_MASK) != STRING_TAG && (_u & NANISH_MASK) != LIST_TAG && (_u & NANISH_MASK) != MAP_TAG && (_u & NANISH_MASK) != INTEGER_TAG && (_u & NANISH_MASK) != FUNCREF_TAG && _u != val_null; }
		public bool IsHeapString { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) == STRING_TAG; }
		public bool IsString { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & STRING_TAG) == STRING_TAG; }
		public bool IsFuncRef { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) == FUNCREF_TAG; }
		public bool IsList   { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) == LIST_TAG; }
		public bool IsMap	{ [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) == MAP_TAG; }
		public bool IsDouble { [MethodImpl(MethodImplOptions.AggressiveInlining)] get => (_u & NANISH_MASK) < val_null; }

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
			if (IsDouble) return AsDouble().ToString();
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
                if (b.IsString) return StringOperations.StringConcat(a, b);
                if (b.IsInt || b.IsDouble) return StringOperations.StringConcat(a, ValueHelpers.make_string(b.ToString()));
			} else if(b.IsString) {
                if (a.IsInt || a.IsDouble) return StringOperations.StringConcat(ValueHelpers.make_string(a.ToString()), b);
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
                    result = StringOperations.StringConcat(result, a);
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
                    result = StringConcat(result, a);
                }
                extraChars = (int)(StringLength(a) * (factor - repeats));
                if (extraChars > 0) result = StringConcat(result, StringSubstring(a, 0, extraChars));
                return result;
            }
			// string concat, list append, etc. can be added here.
			return Null();
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static Value Divide(Value a, Value b) {
			if (a.IsInt && b.IsInt) {
				long r = (long)a.AsInt() / b.AsInt();
				if ((uint)r == r) return FromInt((int)r);
				return FromDouble((double)r);
			}
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
				return StringOperations.StringCompare(a, b) < 0;
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
				return StringOperations.StringCompare(a, b) <= 0;
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
		public static Value val_null = val_null;
		public static Value val_zero = Value.FromInt(0);
		public static Value val_one = Value.FromInt(1);
		public static Value val_empty_string = Value.FromString("");

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
			valueList?.Set(index, item);
		}
		
		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static void list_push(Value list_val, Value item) {
			if (!list_val.IsList) return;
			var valueList = HandlePool.Get(list_val.Handle()) as ValueList;
			valueList?.Add(item);
		}

		[MethodImpl(MethodImplOptions.AggressiveInlining)]
		public static bool list_remove(Value list_val, int index) {
			if (!list_val.IsList) return false;
			var valueList = HandlePool.Get(list_val.Handle()) as ValueList;
			return valueList != null ? valueList.Remove(index) : false;
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

		public static bool map_set(Value map_val, Value key, Value value) {
			if (!map_val.IsMap) return false;
			var valueMap = HandlePool.Get(map_val.Handle()) as ValueMap;
			return valueMap?.Set(key, value) ?? false;
		}

		public static bool map_remove(Value map_val, Value key) {
			if (!map_val.IsMap) return false;
			var valueMap = HandlePool.Get(map_val.Handle()) as ValueMap;
			return valueMap?.Remove(key) ?? false;
		}

		public static bool map_has_key(Value map_val, Value key) {
			if (!map_val.IsMap) return false;
			var valueMap = HandlePool.Get(map_val.Handle()) as ValueMap;
			return valueMap?.HasKey(key) ?? false;
		}

		public static void map_clear(Value map_val) {
			if (!map_val.IsMap) return;
			var valueMap = HandlePool.Get(map_val.Handle()) as ValueMap;
			valueMap?.Clear();
		}
		
		public static void varmap_gather(Value map_val) {
			if (!map_val.IsMap) return;
			var varMap = HandlePool.Get(map_val.Handle()) as VarMap;
			varMap?.Gather();
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
				(is_string(v) && StringOperations.StringLength(v) != 0)
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
		
		public int Count => _items.Count;
		
		public void Add(Value item) => _items.Add(item);
		
		public Value Get(int index) {
			if (index < 0 || index >= _items.Count) return val_null;
			return _items[index];
		}
		
		public void Set(int index, Value value) {
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
		protected Dictionary<Value, Value> _items = new Dictionary<Value, Value>();

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

	// String operations
	public static class StringOperations {
		public static Value StringSplit(Value str, Value delimiter) {
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
			
			Value list = ValueHelpers.make_list(parts.Length);
			foreach (string part in parts) {
				ValueHelpers.list_push(list, Value.FromString(part)); // Include all parts, even empty ones
			}
			
			return list;
		}
		
		public static Value StringReplace(Value str, Value from, Value to) {
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
			return Value.FromString(result);
		}
		
		public static Value StringIndexOf(Value str, Value needle) {
			if (!str.IsString || !needle.IsString) return Value.FromInt(-1);
			
			string s = GetStringValue(str);
			string needleStr = GetStringValue(needle);
			
			int index = s.IndexOf(needleStr);
			return Value.FromInt(index);
		}
		
		public static Value StringConcat(Value str1, Value str2) {
			if (!str1.IsString || !str2.IsString) return val_null;
			
			string s1 = GetStringValue(str1);
			string s2 = GetStringValue(str2);
			
			return Value.FromString(s1 + s2);
		}
		
		public static int StringLength(Value str) {
			if (!str.IsString) return 0;
			
			return GetStringValue(str).Length;
		}
		
		public static bool StringEquals(Value str1, Value str2) {
			return Value.Equal(str1, str2);
		}

		public static int StringCompare(Value str1, Value str2) {
			if (!str1.IsString || !str2.IsString) return 0;
			
			string sa = str1.IsTiny ? str1.ToString() : HandlePool.Get(str1.Handle()) as string;
			string sb = str2.IsTiny ? str2.ToString() : HandlePool.Get(str2.Handle()) as string;
			
			if (sa == null || sb == null) return 0;
			return string.Compare(sa, sb, StringComparison.Ordinal);
		}

        public static Value StringSubstring(Value str, int index, int length){
            string a = GetStringValue(str);
            return make_string(a.Substring(index, length));
        }
		
		public static string GetStringValue(Value val) {
			if (val.IsTiny) return val.ToString();
			if (val.IsHeapString) return HandlePool.Get(val.Handle()) as string ?? "";
			return "";
		}
	}
	
}
