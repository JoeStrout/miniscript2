// value.h
//
// NaN-boxed 8-byte dynamic Value type. All non-immediate types (strings,
// lists, maps, errors, funcrefs) carry a (gcSet, itemIndex) pair packed
// into the lower 35 bits, dispatched by GCManager (see GCManager.h).
//
// Bit layout (matches cs/Value.cs and cs/GCManager.cs):
//   valid double  : top 13 bits not all 1s (standard IEEE 754)
//   null          : 0xFFF1_0000_0000_0000
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

#define CORE_LAYER_2A

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t Value;

// ── Tag bits ────────────────────────────────────────────────────────────
#define NANISH_MASK       0xFFFF000000000000ULL
#define NULL_VALUE        0xFFF1000000000000ULL
#define GC_TAG            0xFFFE000000000000ULL
#define TINY_STRING_TAG   0xFFFF000000000000ULL
// Mask covering top-16 tag bits plus the 3-bit GCSet field (bits 34-32).
#define GC_TYPE_MASK      (NANISH_MASK | 0x0000000700000000ULL)

#define TINY_STRING_MAX_LEN 5

// ── GCSet indices (must match cs/GCManager.cs constants) ────────────────
#define STRING_SET   0
#define LIST_SET     1
#define MAP_SET      2
#define ERROR_SET    3
#define FUNCREF_SET  4

// Composite tag patterns (top-16 + 3-bit set). Useful for legacy code.
#define STRING_TAG_PATTERN  (GC_TAG | ((uint64_t)STRING_SET  << 32))
#define LIST_TAG_PATTERN    (GC_TAG | ((uint64_t)LIST_SET    << 32))
#define MAP_TAG_PATTERN     (GC_TAG | ((uint64_t)MAP_SET     << 32))
#define ERROR_TAG_PATTERN   (GC_TAG | ((uint64_t)ERROR_SET   << 32))
#define FUNCREF_TAG_PATTERN (GC_TAG | ((uint64_t)FUNCREF_SET << 32))

// ── Common constant values ──────────────────────────────────────────────
#define val_null         ((Value)NULL_VALUE)
#define val_zero         ((Value)0x0000000000000000ULL)
#define val_one          ((Value)0x3FF0000000000000ULL)
#define val_empty_string ((Value)TINY_STRING_TAG)

extern Value val_isa_key;   // "__isa"
extern Value val_self;      // "self"
extern Value val_super;     // "super"
void value_init_constants(void);

// ── Accessors used by hot paths ─────────────────────────────────────────
static inline int value_gc_set_index(Value v)  { return (int)((v >> 32) & 0x7); }
static inline int value_item_index(Value v)    { return (int)(uint32_t)v; }

// ── Type predicates ─────────────────────────────────────────────────────
static inline bool value_identical(Value a, Value b) { return a == b; }

static inline bool is_null(Value v) {
    return v == val_null;
}

static inline bool is_int(Value v) {
    (void)v;
    return false;  // legacy stub: int is no longer a separate type
}

static inline bool is_tiny_string(Value v) {
    return (v & NANISH_MASK) == TINY_STRING_TAG;
}

static inline bool is_gc_object(Value v) {
    return (v & NANISH_MASK) == GC_TAG;
}

static inline bool is_heap_string(Value v) {
    return (v & GC_TYPE_MASK) == STRING_TAG_PATTERN;
}

static inline bool is_string(Value v) {
    return is_tiny_string(v) || is_heap_string(v);
}

static inline bool is_funcref(Value v) {
    return (v & GC_TYPE_MASK) == FUNCREF_TAG_PATTERN;
}

static inline bool is_list(Value v) {
    return (v & GC_TYPE_MASK) == LIST_TAG_PATTERN;
}

static inline bool is_map(Value v) {
    return (v & GC_TYPE_MASK) == MAP_TAG_PATTERN;
}

static inline bool is_error(Value v) {
    return (v & GC_TYPE_MASK) == ERROR_TAG_PATTERN;
}

static inline bool is_double(Value v) {
    return (v & NANISH_MASK) < NULL_VALUE;
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
static inline Value make_null(void) { return val_null; }

static inline Value make_double(double d) {
    Value v;
    memcpy(&v, &d, sizeof v);
    return v;
}

static inline Value make_int(int32_t i) { return make_double((double)i); }

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

Value   make_funcref(int32_t funcIndex, Value outerVars);
int32_t funcref_index(Value v);
Value   funcref_outer_vars(Value v);

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
// short name, or val_null if no match. Registered by the VM at init so we
// can avoid a hard dependency from value.cpp on the VM layer.
typedef Value (*ShortNameLookupFn)(void* vm, Value v);
void set_short_name_lookup(ShortNameLookupFn fn);

// ── Arithmetic ──────────────────────────────────────────────────────────
static inline Value value_add(Value a, Value b, void* vm) {
    if (is_error(a)) return a;
    if (is_error(b)) return b;
    if (is_double(a) && is_double(b)) {
        return make_double(as_double(a) + as_double(b));
    }
    if (is_string(a)) {
        if (is_null(b)) return a;
        if (is_string(b)) return string_concat(a, b);
        return string_concat(a, to_string(b, vm));
    } else if (is_string(b)) {
        if (is_null(a)) return b;
        return string_concat(to_string(a, vm), b);
    }
    if (is_list(a) && is_list(b)) return list_concat(a, b);
    if (is_map(a)  && is_map(b))  return map_concat(a, b);
    return val_null;
}

static inline Value value_sub(Value a, Value b) {
    if (is_error(a)) return a;
    if (is_error(b)) return b;
    if (is_double(a) && is_double(b)) {
        return make_double(as_double(a) - as_double(b));
    }
    if (is_string(a) && is_string(b)) return string_sub(a, b);
    return val_null;
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
    return val_null;
}

static inline Value value_mod(Value a, Value b) {
    if (is_error(a)) return a;
    if (is_error(b)) return b;
    if (is_double(a) && is_double(b)) return make_double(fmod(as_double(a), as_double(b)));
    return val_null;
}

static inline Value value_pow(Value a, Value b) {
    if (is_error(a)) return a;
    if (is_error(b)) return b;
    if (is_double(a) && is_double(b)) return make_double(pow(as_double(a), as_double(b)));
    return val_null;
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

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NANBOX_H
