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

#define CORE_LAYER_2A

namespace MiniScript { struct FuncDef; }

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

    constexpr bool operator==(Value other) const { return bits == other.bits; }
    constexpr bool operator!=(Value other) const { return bits != other.bits; }

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
    inline unsigned int Hash()        const noexcept;

    // Truth factories. Note: Truth(double) is just the number ctor: our Value 
    // is light enough that there is no benefit to MiniScript 1.x's special-casing
    // of 0 and 1.
    static Value Truth(bool b);
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

// Maximum number of elements a list (or characters a string) may hold.
// Mirrors ValueHelpers.MAX_COLLECTION_SIZE in cs/Value.cs.  Operations whose
// result would exceed this raise a runtime error instead of attempting a
// hopeless allocation.
#define MAX_COLLECTION_SIZE 0x7FFFFFFF

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
static inline uint64_t value_bits(Value v)     { return v.bits; }
static inline int value_tiny_len(Value v)      { return (int)(v.bits & 0xFF); }
static inline int value_gc_set_index(Value v)  { return (int)((v.bits >> 32) & 0x7); }
static inline int value_item_index(Value v)    { return (int)(uint32_t)v.bits; }

// ── Type predicates ─────────────────────────────────────────────────────
static inline bool value_identical(Value a, Value b) { return a == b; }

static inline bool is_null(Value v) {
    return v == Value::null;
}

static inline bool is_int(Value v) {
    (void)v;
    return false;  // legacy stub: int is no longer a separate type
}

static inline bool is_tiny_string(Value v) {
    return (v.bits & NANISH_MASK) == TINY_STRING_TAG;
}

static inline bool is_gc_object(Value v) {
    return (v.bits & NANISH_MASK) == GC_TAG;
}

static inline bool is_heap_string(Value v) {
    return (v.bits & GC_TYPE_MASK) == STRING_TAG_PATTERN;
}

static inline bool is_string(Value v) {
    return is_tiny_string(v) || is_heap_string(v);
}

static inline bool is_funcref(Value v) {
    return (v.bits & GC_TYPE_MASK) == FUNCREF_TAG_PATTERN;
}

static inline bool is_list(Value v) {
    return (v.bits & GC_TYPE_MASK) == LIST_TAG_PATTERN;
}

static inline bool is_map(Value v) {
    return (v.bits & GC_TYPE_MASK) == MAP_TAG_PATTERN;
}

static inline bool is_error(Value v) {
    return (v.bits & GC_TYPE_MASK) == ERROR_TAG_PATTERN;
}
static inline bool is_handle(Value v) {
    return (v.bits & GC_TYPE_MASK) == HANDLE_TAG_PATTERN;
}

static inline bool is_double(Value v) {
    return (v.bits & NANISH_MASK) < NULL_VALUE;
}

static inline bool is_number(Value v) { return is_double(v); }

bool is_truthy(Value v);

// ── Forward declarations for runtime functions ──────────────────────────
extern Value string_sub(Value a, Value b);
extern Value string_concat(Value a, Value b);
extern Value make_string(const char* str);
extern const char* get_string_data_zerocopy(const Value* v_ptr, int* out_len);
extern int  string_compare(Value a, Value b);
extern bool string_equals(Value a, Value b);
extern int  string_length(Value v);
extern Value string_substring(Value str, int startIndex, int len);
extern Value list_concat(Value a, Value b);
extern Value map_concat(Value a, Value b);
extern Value make_map(int initial_capacity);
extern Value make_empty_map(void);
extern int   list_count(Value list_val);
extern Value list_get(Value list_val, int index);
extern void  list_push(Value list_val, Value item);
extern Value make_list(int initial_capacity);

// ── Numeric & null factories (immediate Values, no GC) ──────────────────
static inline Value make_null(void) { return Value::null; }

static inline Value make_double(double d) {
    Value v;
    memcpy(&v, &d, sizeof v);
    return v;
}

static inline Value make_int(int32_t i) { return make_double((double)i); }

static inline Value make_gc(int gcSet, int itemIdx) {
    return Value::fromBits(GC_TAG | ((uint64_t)gcSet << 32) | (uint64_t)(uint32_t)itemIdx);
}

static inline double as_double(Value v) {
    double d;
    memcpy(&d, &v, sizeof d);
    return d;
}

static inline int32_t as_int(Value v) { return (int32_t)as_double(v); }

static inline double numeric_val(Value v) {
    if (is_double(v)) return as_double(v);
    return 0.0;
}

// ── Error / FuncRef accessors (defined in value.cpp) ────────────────────
// These previously lived as inline pointer-decode helpers; now they
// dispatch through the GC manager.
Value make_error(Value message, Value inner, Value stack, Value isa);
Value error_message(Value error);
Value error_inner(Value error);
Value error_stack(Value error);
Value error_isa(Value error);
bool  error_isa_contains(Value error, Value base);

Value funcref_outer_vars(Value v);

// ── Tiny string buffer access ───────────────────────────────────────────
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define GET_VALUE_DATA_PTR(v_ptr) ((char*)(v_ptr))
    #define GET_VALUE_DATA_PTR_CONST(v_ptr) ((const char*)(v_ptr))
#else
    #define GET_VALUE_DATA_PTR(v_ptr) (((char*)(v_ptr)) + 2)
    #define GET_VALUE_DATA_PTR_CONST(v_ptr) (((const char*)(v_ptr)) + 2)
#endif

// ── Conversion ──────────────────────────────────────────────────────────
Value code_form(Value v, void* vm, int recursion_limit);
Value to_string(Value v, void* vm);
Value value_repr(Value v, void* vm);
Value to_number(Value v);

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
Value value_current_stack_trace();

// ── Arithmetic ──────────────────────────────────────────────────────────
static inline Value value_add(Value a, Value b, void* vm) {
    if (is_error(a)) return a;
    if (is_error(b)) return b;
    if (is_double(a) && is_double(b)) {
        return make_double(as_double(a) + as_double(b));
    }
    if (is_string(a)) {
        if (is_null(b)) return a;
        Value bStr = is_string(b) ? b : to_string(b, vm);
        // Overflow-safe check that the concatenation won't exceed the limit.
        if (string_length(a) > MAX_COLLECTION_SIZE - string_length(bStr)) {
            return value_make_runtime_error("string too large (exceeds maximum size)");
        }
        return string_concat(a, bStr);
    } else if (is_string(b)) {
        if (is_null(a)) return b;
        Value aStr = to_string(a, vm);
        if (string_length(aStr) > MAX_COLLECTION_SIZE - string_length(b)) {
            return value_make_runtime_error("string too large (exceeds maximum size)");
        }
        return string_concat(aStr, b);
    }
    if (is_list(a) && is_list(b)) {
        if (list_count(a) > MAX_COLLECTION_SIZE - list_count(b)) {
            return value_make_runtime_error("list too large (exceeds maximum size)");
        }
        return list_concat(a, b);
    }
    if (is_map(a)  && is_map(b))  return map_concat(a, b);
    return Value::null;
}

static inline Value value_sub(Value a, Value b) {
    if (is_error(a)) return a;
    if (is_error(b)) return b;
    if (is_double(a) && is_double(b)) {
        return make_double(as_double(a) - as_double(b));
    }
    if (is_string(a) && is_string(b)) return string_sub(a, b);
    return Value::null;
}

static inline bool value_lt(Value a, Value b) {
    if (is_double(a) && is_double(b)) return as_double(a) < as_double(b);
    if (is_string(a) && is_string(b)) return string_compare(a, b) < 0;
    return false;
}

extern Value value_mult_nonnumeric(Value a, Value b);
static inline Value value_mult(Value a, Value b) {
    if (is_error(a)) return a;
    if (is_error(b)) return b;
    if (is_double(a) && is_double(b)) {
        return make_double(as_double(a) * as_double(b));
    }
    return value_mult_nonnumeric(a, b);
}

static inline Value value_div(Value a, Value b) {
    if (is_error(a)) return a;
    if (is_error(b)) return b;
    if (is_double(b)) {
        if (is_double(a)) return make_double(as_double(a) / as_double(b));
        return value_mult_nonnumeric(a, value_div(make_double(1), b));
    }
    return Value::null;
}

static inline Value value_mod(Value a, Value b) {
    if (is_error(a)) return a;
    if (is_error(b)) return b;
    if (is_double(a) && is_double(b)) return make_double(fmod(as_double(a), as_double(b)));
    return Value::null;
}

static inline Value value_pow(Value a, Value b) {
    if (is_error(a)) return a;
    if (is_error(b)) return b;
    if (is_double(a) && is_double(b)) return make_double(pow(as_double(a), as_double(b)));
    return Value::null;
}

bool value_equal(Value a, Value b);
bool value_le(Value a, Value b);
static inline bool value_gt(Value a, Value b) { return !value_le(a, b); }
static inline bool value_ge(Value a, Value b) { return !value_lt(a, b); }
int  value_compare(Value a, Value b);

// ── Helpers / fuzzy logic ───────────────────────────────────────────────
static inline double ToFuzzyBool(Value v) {
    if (is_double(v)) return as_double(v);
    return is_truthy(v) ? 1.0 : 0.0;
}

static inline double AbsClamp01(double d) {
    if (d < 0) d = -d;
    if (d > 1) return 1;
    return d;
}

static inline Value value_and(Value a, Value b) {
    if (is_error(a)) return a;
    if (is_error(b)) return b;
    return make_double(AbsClamp01(ToFuzzyBool(a) * ToFuzzyBool(b)));
}

static inline Value value_or(Value a, Value b) {
    if (is_error(a)) return b;
    if (is_error(b)) return b;
    double fA = ToFuzzyBool(a), fB = ToFuzzyBool(b);
    return make_double(AbsClamp01(fA + fB - fA * fB));
}

static inline Value value_not(Value a) {
    if (is_error(a)) return a;
    return make_double(1.0 - AbsClamp01(ToFuzzyBool(a)));
}

// ── Bitwise (legacy stubs; retained for compatibility) ──────────────────
Value value_xor(Value a, Value b);
Value value_unary(Value a);
Value value_shr(Value v, int shift);
Value value_shl(Value v, int shift);

// ── Hashing & frozen ────────────────────────────────────────────────────
uint32_t value_hash(Value v);
bool     is_frozen(Value v);
void     freeze_value(Value v);
Value    frozen_copy(Value v);

// Content-aware Hash and equality overloads for Value, used by
// Dictionary<Value, Value>. Without these, Dictionary would fall back to
// bitwise hash/equality (via Hash(int) narrowing and uint64_t ==), which
// fails for heap-allocated strings/lists/maps whose bits differ across
// allocations even when their content is equal.
inline int Hash(Value v) {
    return (int)(value_hash(v) & 0x7FFFFFFFU);
}
inline bool DictKeyEqual(Value a, Value b) {
    return value_equal(a, b);
}

// ── MiniScript 1.x-compatible constructors (declared in struct Value) ──────
// Defined here, where NULL_VALUE and the make_* helpers are visible.  A default
// Value is null; Value(1.0) is a number; Value("x") copies the C string.
inline Value::Value() noexcept : bits(NULL_VALUE) {}
inline Value::Value(double number) noexcept { *this = make_double(number); }
inline Value::Value(const char* cString) { *this = make_string(cString); }

// Truthiness factories.  Truth(double) maps 0 -> zero and 1 -> one for free,
// since make_double(0.0)/make_double(1.0) are exactly Value::zero/Value::one.
inline Value Value::Truth(bool b)        { return b ? one : zero; }
inline Value Value::Truth(double number) { return Value(number); }

// ── MiniScript 1.x-style convenience accessors (declared in struct Value) ──
// Out-of-line so they can see is_double/as_double/is_truthy/is_null/value_hash,
// which are declared earlier in this header.  Each compiles to the same code as
// the corresponding free-function call, so it is convenience only.
inline double       Value::DoubleValue() const noexcept { return is_double(*this) ? as_double(*this) : 0.0; }
inline float        Value::FloatValue()  const noexcept { return (float)DoubleValue(); }
inline int32_t      Value::IntValue()    const noexcept { return (int32_t)DoubleValue(); }
inline uint32_t     Value::UIntValue()   const noexcept { return (uint32_t)DoubleValue(); }
inline bool         Value::BoolValue()   const noexcept { return is_truthy(*this); }
inline bool         Value::IsNull()      const noexcept { return is_null(*this); }
inline unsigned int Value::Hash()        const noexcept { return value_hash(*this); }

// A funcref Value pairs a FuncDef with an optional captured-variable map.
// Declared with C++ linkage (FuncDef is a C++ type, defined in FuncDef.g.h).
Value               make_funcref(MiniScript::FuncDef func, Value outerVars);
MiniScript::FuncDef funcref_funcdef(Value v);

#endif // NANBOX_H
