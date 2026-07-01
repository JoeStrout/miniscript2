// value.h
//
// NaN-boxed 8-byte dynamic Value type. All non-immediate types (strings,
// lists, maps, errors, funcrefs) carry a (gcSet, itemIndex) pair packed
// into the lower 35 bits, dispatched by GCManager (see GCManager.h).
//
// Bit layout (matches cs/Value.cs and cs/GCManager.cs):
//   valid double  : top 16 bits < 0xFFF9 (includes ±inf and ±NaN)
//   null          : 0xFFF9_0000_0000_0000
//   GC object     : 0xFFFE_0000_000G_IIII_IIII
//                     bits 34-32 = GCSet index (0-5)
//                     bits 31-0  = item index within that GCSet
//                     bits 47-35 = reserved/unused
//   tiny string   : 0xFFFF_C4C3_C2C1_C0LL
//                     bits 47-8  = up to 5 ASCII chars
//                     bits  7-0  = length (0-5)

#ifndef NANBOX_H
#define NANBOX_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "hashing.h"
#include <type_traits>
// NOTE: This pulls the host-side String class (Layer 2B) into this VM-side
// (Layer 2A) header so that Value can expose String-typed methods (to_String,
// make_string(String), map_set(String), value_type_name) matching cs/Value.cs.
// This deliberately inverts the documented 2A/2B layering; layer_defs.h checks
// are currently disabled.  CS_String.h does not include value.h, so no cycle.
#include "CS_String.h"

#define CORE_LAYER_2A

namespace MiniScript { struct FuncDef; }
struct MapIterator;  // defined in value_map.h; returned by Value::map_iterator

// NaN-boxed dynamic Value.  The sole data member `bits` holds the 64-bit
// payload: for numbers it is the IEEE-754 double bit pattern; for everything
// else it is a tagged encoding (see the layout notes at the top of this file).
//
// Value used to be a bare `typedef uint64_t`.  It is now a one-member struct so
// that host code (and, eventually, our own code) can hang methods and operators
// on it, the way cs/Value.cs and the MiniScript 1.x C++ API do.  It is
// deliberately kept trivially copyable and standard-layout, so it is still
// passed in a register exactly like the raw uint64_t it replaces (enforced by
// the static_asserts below).  Core code reaches the payload through `.bits`;
// everything else goes through the make_*/as_*/is_* free functions.
typedef struct Value {
    uint64_t bits;

    // MiniScript 1.x-compatible constructors.  The numeric and string ctors are
    // defined out-of-line below, where the NaN-box constants and the make_*
    // helpers are visible.  These mirror MiniScript 1.x: a default Value is
    // null, Value(1.0) is a number, Value("x") is a string.
    Value() noexcept;                 // null
    Value(double number) noexcept;    // a number
    Value(int i) noexcept;            // integer (delegates to double; prevents 0→null-ptr ambiguity)
    Value(unsigned int u) noexcept;   // unsigned integer (delegates to double)
    // The string ctor is explicit: a bare const char* literal must not silently
    // become a Value, or calls like f("x") where f is overloaded on both String
    // and Value (e.g. RaiseRuntimeError) become ambiguous.  Value("x") still works.
    explicit Value(const char* cString); // a string (copies the C string)

    // Build a Value straight from its raw NaN-boxed payload.  This is the
    // internal escape hatch used by make_* and the Value:: static constants.
    // It is a named factory rather than a uint64_t constructor on purpose:
    // that keeps Value(0), Value(1.0), etc. unambiguous and meaning what they
    // do in 1.x.
    static constexpr Value fromBits(uint64_t rawBits) noexcept { return Value(rawBits, RawTag{}); }

    // MiniScript-semantic equality: a deep value comparison (numbers by value,
    // strings/lists/maps by content; see RecursiveEqual).  Defined out-of-line
    // below.  For a simple bitwise/reference identity check (same NaN-boxed
    // payload) use RefEquals instead.
    bool operator==(Value other) const;
    bool operator!=(Value other) const;

    // Deep MiniScript equality (the implementation behind operator==): numbers by
    // value, strings/lists/maps by content, other reference types by identity.
    // Walks lists/maps iteratively with a visited set so reference cycles
    // terminate instead of overflowing the stack.  Mirrors MS1 Value::RecursiveEqual.
    bool RecursiveEqual(Value rhs) const;

    // Simple bitwise/reference identity: true iff this and rhs have the exact
    // same NaN-boxed payload.  This is the fast pointer-identity comparison; it
    // does NOT do MiniScript-semantic (deep) equality -- use operator== for that.
    // Instance method, mirroring MiniScript 1.x.
    bool RefEquals(const Value& rhs) const { return bits == rhs.bits; }

    // MiniScript 1.x-style convenience accessors.  These are thin, zero-overhead
    // wrappers over the make_*/as_*/is_* free functions; they add no storage and
    // do not change how Value is passed.  Defined out-of-line at the bottom of
    // this header, where those free functions are visible.  (Heavier 1.x methods
    // such as GetString/GetList/GetDict, ToString, and SetElem/GetElem are
    // intentionally omitted here, to avoid pulling the String/List/Map/VM layers
    // into this low-level header.)
    inline double       DoubleValue() const noexcept;
    inline float        FloatValue()  const noexcept;
    inline int32_t      IntValue()    const noexcept;
    inline uint32_t     UIntValue()   const noexcept;
    inline bool         BoolValue()   const noexcept;
    inline bool         IsNull()      const noexcept;
    inline bool         IsNumber()    const noexcept;
    inline bool         IsString()    const noexcept;
    inline bool         IsList()      const noexcept;
    inline bool         IsMap()       const noexcept;
    inline bool         IsError()     const noexcept;
    inline bool         IsFuncRef()   const noexcept;
    inline bool         IsHandle()    const noexcept;
    inline unsigned int Hash()        const noexcept;

    // ── Accessors & predicates (instance form, mirroring cs/Value.cs) ────────
    inline uint64_t     Bits()        const noexcept;
    inline int          GCSetIndex()  const noexcept;
    inline int          ItemIndex()   const noexcept;
    inline int          TinyLen()     const noexcept;
    inline bool         IsTinyString()const noexcept;
    inline bool         IsGCObject()  const noexcept;
    inline bool         IsHeapString()const noexcept;
    inline bool         IsInt()       const noexcept;
    inline int          AsInt()       const noexcept;
    inline double       AsDouble()    const noexcept;
    inline double       NumericVal()  const noexcept;
    String              TypeName()    const;

    // ── Static methods mirroring the C# Value API (cs/Value.cs) ──────────────
    // Transpiled code calls these as Value::make_string(...), etc.  These are
    // the canonical definitions; the matching free functions have been removed.
    static Value  make_gc(int gcSet, int itemIdx);
    static bool   value_identical(Value a, Value b);
    static double AbsClamp01(double d);

    static Value  value_add(Value a, Value b, void* vm);
    static Value  value_pow(Value a, Value b);
    static Value  value_and(Value a, Value b);
    static Value  value_or(Value a, Value b);

    // Strings (instance form, mirroring cs/Value.cs)
    static Value  make_string(const char* str);
    static Value  make_string(const String& s);
    String        ToString(void* vm = nullptr) const;
    const char*   AsCString() const;
    int           Length() const;
    int           StringIndexOf(Value needle, int start_pos) const;
    Value         Substring(int startIndex, int len) const;
    Value         StringSlice(int start, int end) const;
    Value         StringConcat(Value b) const;
    int           Compare(Value b) const;
    Value         Split(Value delimiter) const;
    Value         Replace(Value from, Value to) const;
    Value         StringInsert(int index, Value value, void* vm = nullptr) const;
    Value         Upper() const;
    Value         Lower() const;
    static Value  string_from_code_point(int codePoint);
    int           CodePoint() const;
    Value         SplitMax(Value delimiter, int maxCount) const;
    Value         ReplaceMax(Value search, Value replacement, int maxCount) const;
    Value         ToStringValue(void* vm = nullptr) const;
    Value         ToNumber() const;
    Value         Repr(void* vm = nullptr) const;
    static Value  value_current_stack_trace();

    // Lists (instance form, mirroring cs/Value.cs)
    static Value  make_list(int initial_capacity);
    int           ListCount() const;
    Value         ListGet(int index) const;
    void          ListSet(int index, Value item) const;
    void          Push(Value item) const;
    Value         Pop() const;
    Value         Pull() const;
    void          ListInsert(int index, Value item) const;
    bool          ListRemove(int index) const;
    int           ListIndexOf(Value item, int afterIdx) const;
    Value         ListSlice(int start, int end) const;
    Value         ListConcat(Value b) const;
    void          Sort(bool ascending) const;
    void          SortByKey(Value byKey, bool ascending) const;

    // Maps
    static Value  make_map(int initial_capacity);
    static Value  make_empty_map();
    static int    map_count(Value map_val);
    static Value  map_get(Value map_val, Value key);
    static bool   map_set(Value map_val, Value key, Value value);
    static bool   map_set(Value map_val, const String& key, Value value);
    static bool   map_set(Value map_val, const String& key, const String& value);
    static bool   map_has_key(Value map_val, Value key);
    static bool   map_try_get(Value map_val, Value key, Value* out_value);
    static bool   map_remove(Value map_val, Value key);
    static void   map_clear(Value map_val);
    static bool   map_lookup(Value map_val, Value key, Value* out_value);
    static bool   map_lookup_with_origin(Value map_val, Value key, Value* out_value, Value* out_super);
    static int    map_iter_next(Value map_val, int iter);
    static Value  map_iter_entry(Value map_val, int iter);
    static MapIterator map_iterator(Value map_val);

    // VarMaps (take List<Value>, visible via CS_String.h -> CS_List.h)
    static Value  make_varmap(List<Value> registers, List<Value> names, int firstIndex, int count);
    static void   varmap_map_to_register(Value map_val, Value var_name, List<Value> registers, int reg_index);
    static void   varmap_gather(Value map_val);
    static void   varmap_rebind(Value map_val, List<Value> registers, List<Value> names);

    // Errors / funcrefs / freeze
    static Value  make_error(Value message, Value inner, Value stack, Value isa);
    static Value  error_message(Value error);
    static Value  error_inner(Value error);
    static Value  error_stack(Value error);
    static Value  error_isa(Value error);
    static bool   error_isa_contains(Value error, Value base);
    static Value  make_funcref(MiniScript::FuncDef func, Value outerVars);
    static MiniScript::FuncDef funcref_funcdef(Value v);
    static Value  funcref_outer_vars(Value v);
    static bool   is_frozen(Value v);
    static void   freeze_value(Value v);
    static Value  frozen_copy(Value v);

    // Truth factory: returns Value::one for truthy, Value::zero for falsy.
    // A single overload on double covers all numeric types (bool, int, float, double).
    static Value Truth(double number);

    static Value null;           // DEPRECATED: used in MS 1.x; prefer Null for new code.
    static Value Null;           // null value (C#-compatible capitalization)
    static Value zero;           // 0
    static Value one;            // 1
    static Value emptyString;    // ""
    static Value magicIsA;       // "__isa"
    static Value keyString;      // "key"
    static Value valueString;    // "value"
    static Value implicitResult; // "_"
    static Value selfString;     // "self"
    static Value superString;    // "super"

	// Maximum number of elements a list (or characters a string) may hold.
	// Mirrors Value.MAX_COLLECTION_SIZE in cs/Value.cs.  Operations whose
	// result would exceed this raise a runtime error instead of attempting a
	// hopeless allocation.
	static int MAX_COLLECTION_SIZE;

	// Sentinel returned by map_iter_next when iteration is exhausted.
	// Mirrors Value.MAP_ITER_DONE in cs/Value.cs (Int32.MinValue).
	static int MAP_ITER_DONE;

  private:
    // Tag type that selects the raw-payload constructor (used only by fromBits).
    struct RawTag {};
    constexpr Value(uint64_t rawBits, RawTag) noexcept : bits(rawBits) {}
} Value;

static_assert(sizeof(Value) == 8, "Value must remain exactly 8 bytes");
static_assert(std::is_trivially_copyable<Value>::value,
    "Value must stay trivially copyable so it is still passed in a register");
static_assert(std::is_standard_layout<Value>::value,
    "Value must stay standard-layout");

// ── Tag bits ────────────────────────────────────────────────────────────
#define NANISH_MASK       0xFFFF000000000000ULL
// NULL_VALUE doubles as the is_double threshold (top 16 bits below it == double).
// It sits at 0xFFF9 so the negative infinity (0xFFF0) and negative quiet NaN
// (0xFFF8) bit patterns still classify as doubles; only 0xFFF9-0xFFFF is
// reserved for tags.  This lets make_double stay a branch-free bit copy.
#define NULL_VALUE        0xFFF9000000000000ULL
#define GC_TAG            0xFFFE000000000000ULL
#define TINY_STRING_TAG   0xFFFF000000000000ULL
// Mask covering top-16 tag bits plus the 3-bit GCSet field (bits 34-32).
#define GC_TYPE_MASK      (NANISH_MASK | 0x0000000700000000ULL)

#define TINY_STRING_MAX_LEN 5

// ── GCSet indices (IMPORTANT: must match cs/GCManager.cs constants) ────────────
#define STRING_SET   0
#define LIST_SET     1
#define MAP_SET      2
#define ERROR_SET    3
#define FUNCREF_SET  4
// InternedStringSet = 5 (semi-immortal; referenced via GCManager::InternedStringSet)
#define HANDLE_SET   6

// Composite tag patterns (top-16 + 3-bit set). Useful for legacy code.
#define STRING_TAG_PATTERN  (GC_TAG | ((uint64_t)STRING_SET  << 32))
#define LIST_TAG_PATTERN    (GC_TAG | ((uint64_t)LIST_SET    << 32))
#define MAP_TAG_PATTERN     (GC_TAG | ((uint64_t)MAP_SET     << 32))
#define ERROR_TAG_PATTERN   (GC_TAG | ((uint64_t)ERROR_SET   << 32))
#define FUNCREF_TAG_PATTERN (GC_TAG | ((uint64_t)FUNCREF_SET << 32))
#define HANDLE_TAG_PATTERN  (GC_TAG | ((uint64_t)HANDLE_SET  << 32))

// ── Common constant values ──────────────────────────────────────────────
// All constants are static members of Value (see struct declaration above).
// Call value_init_constants() once at startup to set the string-valued ones.
void value_init_constants(void);

// ── Accessors used by hot paths ─────────────────────────────────────────
inline uint64_t Value::Bits()       const noexcept { return bits; }
inline int Value::TinyLen()         const noexcept { return (int)(bits & 0xFF); }
inline int Value::GCSetIndex()      const noexcept { return (int)((bits >> 32) & 0x7); }
inline int Value::ItemIndex()       const noexcept { return (int)(uint32_t)bits; }

// ── Type predicates ─────────────────────────────────────────────────────
inline bool Value::value_identical(Value a, Value b) { return a.RefEquals(b); }

inline bool Value::IsInt() const noexcept {
    return false;  // legacy stub: int is no longer a separate type
}

inline bool Value::IsTinyString() const noexcept {
    return (bits & NANISH_MASK) == TINY_STRING_TAG;
}

inline bool Value::IsGCObject() const noexcept {
    return (bits & NANISH_MASK) == GC_TAG;
}

inline bool Value::IsHeapString() const noexcept {
    return (bits & GC_TYPE_MASK) == STRING_TAG_PATTERN;
}

// ── Forward declarations for runtime functions ──────────────────────────
extern Value string_sub(Value a, Value b);
extern Value string_concat(Value a, Value b);
extern const char* get_string_data_zerocopy(const Value* v_ptr, int* out_len);
extern int  string_compare(Value a, Value b);
extern bool string_equals(Value a, Value b);
extern int  string_length(Value v);
extern Value string_substring(Value str, int startIndex, int len);
extern Value list_concat(Value a, Value b);
extern Value map_concat(Value a, Value b);

// ── Numeric factory (immediate Value, no GC) ──────────────────────────
inline Value Value::make_gc(int gcSet, int itemIdx) {
    return Value::fromBits(GC_TAG | ((uint64_t)gcSet << 32) | (uint64_t)(uint32_t)itemIdx);
}

inline double Value::AsDouble() const noexcept {
    double d;
    memcpy(&d, this, sizeof d);
    return d;
}

inline int Value::AsInt() const noexcept { return (int32_t)AsDouble(); }

inline double Value::NumericVal() const noexcept {
    if (IsNumber()) return AsDouble();
    return 0.0;
}

// ── Error / FuncRef accessors (defined in value.cpp) ────────────────────
// These previously lived as inline pointer-decode helpers; now they
// dispatch through the GC manager.


// ── Tiny string buffer access ───────────────────────────────────────────
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define GET_VALUE_DATA_PTR(v_ptr) ((char*)(v_ptr))
    #define GET_VALUE_DATA_PTR_CONST(v_ptr) ((const char*)(v_ptr))
#else
    #define GET_VALUE_DATA_PTR(v_ptr) (((char*)(v_ptr)) + 2)
    #define GET_VALUE_DATA_PTR_CONST(v_ptr) (((const char*)(v_ptr)) + 2)
#endif

// ── Conversion ──────────────────────────────────────────────────────────
// (to_string / value_repr / to_number are now Value:: static methods.)
Value code_form(Value v, void* vm, int recursion_limit);

// Short-name lookup hook: called by code_form when printing a map/list that
// might be a known type or named global. Returns a string Value holding the
// short name, or Value::null if no match. Registered by the VM at init so we
// can avoid a hard dependency from value.cpp on the VM layer.
typedef Value (*ShortNameLookupFn)(void* vm, Value v);
void set_short_name_lookup(ShortNameLookupFn fn);

// Runtime-error constructor hook: builds a properly-typed runtime error Value
// (with the ErrorTypes.runtime __isa) from a message.  Registered by the VM at
// init so value.cpp (layer 2A) can produce the same error values as the higher
// ErrorTypes layer without depending on it.  If unset, value_make_runtime_error
// falls back to a plain make_error.
typedef Value (*RuntimeErrorMakerFn)(const char* message);
void set_runtime_error_maker(RuntimeErrorMakerFn fn);
Value value_make_runtime_error(const char* message);

// Stack-trace hook: returns the active VM's current call stack as a Value (a
// frozen list of strings), or Value::null if there is no running VM.  Registered
// by the VM at init so layers below it (ErrorTypes, value ops) can attach an
// accurate stack trace to error values they create, without a hard dependency
// on the VM layer.
typedef Value (*StackTraceFn)();
void set_stack_trace_hook(StackTraceFn fn);
// (value_current_stack_trace is now a Value:: static method.)

// ── Arithmetic ──────────────────────────────────────────────────────────
inline Value Value::value_add(Value a, Value b, void* vm) {
    if (a.IsError()) return a;
    if (b.IsError()) return b;
    if (a.IsNumber() && b.IsNumber()) {
        return Value(a.AsDouble() + b.AsDouble());
    }
    if (a.IsString()) {
        if (b.IsNull()) return a;
        Value bStr = b.IsString() ? b : b.ToStringValue(vm);
        // Overflow-safe check that the concatenation won't exceed the limit.
        if (a.Length() > Value::MAX_COLLECTION_SIZE - bStr.Length()) {
            return value_make_runtime_error("string too large (exceeds maximum size)");
        }
        return string_concat(a, bStr);
    } else if (b.IsString()) {
        if (a.IsNull()) return b;
        Value aStr = a.ToStringValue(vm);
        if (aStr.Length() > Value::MAX_COLLECTION_SIZE - b.Length()) {
            return value_make_runtime_error("string too large (exceeds maximum size)");
        }
        return string_concat(aStr, b);
    }
    if (a.IsList() && b.IsList()) {
        if (a.ListCount() > Value::MAX_COLLECTION_SIZE - b.ListCount()) {
            return value_make_runtime_error("list too large (exceeds maximum size)");
        }
        return list_concat(a, b);
    }
    if (a.IsMap()  && b.IsMap())  return map_concat(a, b);
    return Value::null;
}

inline Value operator-(Value a, Value b) {
    if (a.IsError()) return a;
    if (b.IsError()) return b;
    if (a.IsNumber() && b.IsNumber()) {
        return Value(a.AsDouble() - b.AsDouble());
    }
    if (a.IsString() && b.IsString()) return string_sub(a, b);
    return Value::null;
}

inline bool operator<(Value a, Value b) {
    if (a.IsNumber() && b.IsNumber()) return a.AsDouble() < b.AsDouble();
    if (a.IsString() && b.IsString()) return string_compare(a, b) < 0;
    return false;
}
inline bool operator<=(Value a, Value b) {
    if (a.IsNumber() && b.IsNumber()) return a.AsDouble() <= b.AsDouble();
    if (a.IsString() && b.IsString()) return string_compare(a, b) <= 0;
    return false;
}
inline bool operator>(Value a, Value b)  { return !(a <= b); }
inline bool operator>=(Value a, Value b) { return !(a < b); }

extern Value value_mult_nonnumeric(Value a, Value b);
inline Value operator*(Value a, Value b) {
    if (a.IsError()) return a;
    if (b.IsError()) return b;
    if (a.IsNumber() && b.IsNumber()) {
        return Value(a.AsDouble() * b.AsDouble());
    }
    return value_mult_nonnumeric(a, b);
}

inline Value operator/(Value a, Value b) {
    if (a.IsError()) return a;
    if (b.IsError()) return b;
    if (b.IsNumber()) {
        if (a.IsNumber()) return Value(a.AsDouble() / b.AsDouble());
        return value_mult_nonnumeric(a, Value(1.0) / b);
    }
    return Value::null;
}

inline Value operator%(Value a, Value b) {
    if (a.IsError()) return a;
    if (b.IsError()) return b;
    if (a.IsNumber() && b.IsNumber()) return Value(fmod(a.AsDouble(), b.AsDouble()));
    return Value::null;
}

inline Value Value::value_pow(Value a, Value b) {
    if (a.IsError()) return a;
    if (b.IsError()) return b;
    if (a.IsNumber() && b.IsNumber()) return Value(pow(a.AsDouble(), b.AsDouble()));
    return Value::null;
}

int  value_compare(Value a, Value b);

// MiniScript-semantic equality (deep) -- delegates to RecursiveEqual (defined in
// value.cpp).  RefEquals (in the struct) remains the bitwise/reference-identity
// comparison.
inline bool Value::operator==(Value other) const { return RecursiveEqual(other); }
inline bool Value::operator!=(Value other) const { return !RecursiveEqual(other); }

// ── Helpers / fuzzy logic ───────────────────────────────────────────────
static inline double ToFuzzyBool(Value v) {
    if (v.IsNumber()) return v.AsDouble();
    return v.BoolValue() ? 1.0 : 0.0;
}

inline double Value::AbsClamp01(double d) {
    if (d < 0) d = -d;
    if (d > 1) return 1;
    return d;
}

inline Value Value::value_and(Value a, Value b) {
    if (a.IsError()) return a;
    if (b.IsError()) return b;
    return Value(Value::AbsClamp01(ToFuzzyBool(a) * ToFuzzyBool(b)));
}

inline Value Value::value_or(Value a, Value b) {
    if (a.IsError()) return b;
    if (b.IsError()) return b;
    double fA = ToFuzzyBool(a), fB = ToFuzzyBool(b);
    return Value(Value::AbsClamp01(fA + fB - fA * fB));
}

inline Value operator!(Value a) {
    if (a.IsError()) return a;
    return Value(1.0 - Value::AbsClamp01(ToFuzzyBool(a)));
}

// ── Bitwise (legacy stubs; retained for compatibility) ──────────────────
Value value_xor(Value a, Value b);
Value value_unary(Value a);
Value value_shr(Value v, int shift);
Value value_shl(Value v, int shift);

// ── Hashing & frozen ────────────────────────────────────────────────────
uint32_t value_hash(Value v);

// Content-aware Hash and equality overloads for Value, used by
// Dictionary<Value, Value>. Without these, Dictionary would fall back to
// bitwise hash/equality (via Hash(int) narrowing and uint64_t ==), which
// fails for heap-allocated strings/lists/maps whose bits differ across
// allocations even when their content is equal.
inline int Hash(Value v) {
    return (int)(value_hash(v) & 0x7FFFFFFFU);
}
inline bool DictKeyEqual(Value a, Value b) {
    return a.RecursiveEqual(b);
}

// ── MiniScript 1.x-compatible constructors (declared in struct Value) ──────
// Defined here, where NULL_VALUE and the make_* helpers are visible.  A default
// Value is null; Value(1.0) is a number; Value("x") copies the C string.
inline Value::Value() noexcept : bits(NULL_VALUE) {}
inline Value::Value(double number) noexcept { memcpy(&bits, &number, sizeof bits); }
inline Value::Value(int i) noexcept { double d = (double)i; memcpy(&bits, &d, sizeof bits); }
inline Value::Value(unsigned int u) noexcept { double d = (double)u; memcpy(&bits, &d, sizeof bits); }
inline Value::Value(const char* cString) { *this = Value::make_string(cString); }

// Truth factory: any numeric type converts to double via standard conversion.
// 0.0 / false → Value::zero, anything else → a number value.
inline Value Value::Truth(double number) { return Value(number); }
//inline Value Value::Truth(Boolean b) { return Value((double)b); }

// ── MiniScript 1.x-style convenience accessors (declared in struct Value) ──
// Out-of-line so they can see the tag constants and helper predicates declared
// earlier in this header.  Each compiles to the same code as the corresponding
// free-function call, so it is convenience only.
inline bool Value::IsNull()    const noexcept { return bits == NULL_VALUE; }
inline bool Value::IsNumber()  const noexcept { return (bits & NANISH_MASK) < NULL_VALUE; }
inline bool Value::IsString()  const noexcept { return IsTinyString() || IsHeapString(); }
inline bool Value::IsList()    const noexcept { return (bits & GC_TYPE_MASK) == LIST_TAG_PATTERN; }
inline bool Value::IsMap()     const noexcept { return (bits & GC_TYPE_MASK) == MAP_TAG_PATTERN; }
inline bool Value::IsError()   const noexcept { return (bits & GC_TYPE_MASK) == ERROR_TAG_PATTERN; }
inline bool Value::IsFuncRef() const noexcept { return (bits & GC_TYPE_MASK) == FUNCREF_TAG_PATTERN; }
inline bool Value::IsHandle()  const noexcept { return (bits & GC_TYPE_MASK) == HANDLE_TAG_PATTERN; }
inline bool Value::BoolValue() const noexcept {
    if (IsNull()) return false;
    if (IsNumber()) return AsDouble() != 0.0;
    if (IsString()) return Length() != 0;
    if (IsList()) return ListCount() != 0;
    if (IsMap()) return Value::map_count(*this) != 0;
    return true;
}
inline double       Value::DoubleValue() const noexcept { return IsNumber() ? AsDouble() : 0.0; }
inline float        Value::FloatValue()  const noexcept { return (float)DoubleValue(); }
inline int32_t      Value::IntValue()    const noexcept { return (int32_t)DoubleValue(); }
inline uint32_t     Value::UIntValue()   const noexcept { return (uint32_t)DoubleValue(); }
inline unsigned int Value::Hash()        const noexcept { return value_hash(*this); }

// A funcref Value pairs a FuncDef with an optional captured-variable map.
// Declared with C++ linkage (FuncDef is a C++ type, defined in FuncDef.g.h).

// ── String-typed Value methods (need the host String class) ──────────────
// These mirror cs/Value.cs.  Defined inline here since String is now visible.
inline Value Value::make_string(const String& s) { return Value::make_string(s.c_str()); }

inline bool Value::map_set(Value map_val, const String& key, Value value) {
    return Value::map_set(map_val, Value::make_string(key), value);
}
inline bool Value::map_set(Value map_val, const String& key, const String& value) {
    return Value::map_set(map_val, Value::make_string(key), Value::make_string(value));
}

inline String Value::ToString(void* vm) const {
    return String(ToStringValue(vm).AsCString());
}

// Hand-written twin of Value.TypeName (Value.cs is CS_ONLY, so it
// is not transpiled).  Keep in sync with the C# version.
inline String Value::TypeName() const {
    if (IsNumber()) return String("number");
    if (IsString()) return String("string");
    if (IsList()) return String("list");
    if (IsMap()) return String("map");
    if (IsFuncRef()) return String("funcRef");
    if (IsError()) return String("error");
    if (IsHandle()) return String("handle");
    if (IsNull()) return String("null");
    return String("unknown");
}

#endif // NANBOX_H
