// value.h
//
// Purpose: this module defines the Value type, an 8-byte NaN-box representation
// of our dynamic type.  This Value will always be either a valid Double, or
// an actual numeric NaN, or a representation of some other type (possibly pointing
// to additional data in the heap, in the case of strings, lists, and maps).
//
// Because a Value may point to garbage-collected memory allocations, care must be
// taken to protect local Value variables (see GC_USAGE.md).

#ifndef NANBOX_H
#define NANBOX_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "hashing.h"

// This module is part of Layer 2A (Runtime Value System + GC)
#define CORE_LAYER_2A

#ifdef __cplusplus
extern "C" {
#endif

// Core NaN-boxing dynamic type system
// Based on: https://piotrduperas.com/posts/nan-boxing

typedef uint64_t Value;

// Forward declarations for map structures
typedef struct MapEntry {
    Value key;
    Value value;
    uint32_t hash;      // Cached hash of the key
    bool occupied;      // Whether this slot is occupied
} MapEntry;

// VarMap-specific data (only allocated when needed)
typedef struct VarMapData {
    Value* registers;        // Pointer to VM register array
    Value* names;           // Pointer to VM names array
    Value* reg_map_keys;    // Variable names mapped to registers
    int* reg_map_indices;   // Corresponding register indices
    int reg_map_count;      // Number of register mappings
    int reg_map_capacity;   // Capacity of the mapping arrays
} VarMapData;

typedef struct ValueMap {
    int count;          // Number of key-value pairs
    int capacity;       // Number of slots in the hash table
    MapEntry* entries;  // Pointer to separately-allocated entries array
    VarMapData* varmap_data; // NULL for regular maps, non-NULL for VarMaps
} ValueMap;

// NaN-boxing masks and constants
#define NANISH_MASK       0xffff000000000000ULL
#define NULL_VALUE        0xfff1000000000000ULL  // our lowest reserved NaN pattern
#define INTEGER_TAG       0xfffa000000000000ULL
#define FUNCREF_TAG       0xfffb000000000000ULL
#define MAP_TAG           0xfffc000000000000ULL
#define LIST_TAG          0xfffd000000000000ULL
#define STRING_TAG        0xfffe000000000000ULL  // (specifically, heap string)
#define TINY_STRING_TAG   0xffff000000000000ULL

#define TINY_STRING_MAX_LEN 5                     // Max 5 chars in 40 bits (bits 8-47)

// Common constant values (initialized by value_init_constants in value.c)
#define val_null ((Value)NULL_VALUE)
#define val_zero ((Value)INTEGER_TAG)  
#define val_one ((Value)(INTEGER_TAG | 1))
#define val_empty_string ((Value)TINY_STRING_TAG)

static inline bool value_identical(Value a, Value b) {
	return a == b;
}

// Core type checking functions
static inline bool is_null(Value v) {
    return v == val_null;
}

static inline bool is_int(Value v) {
    return (v & NANISH_MASK) == INTEGER_TAG;
}

static inline bool is_tiny_string(Value v) {
    return (v & NANISH_MASK) == TINY_STRING_TAG;
}

static inline bool is_heap_string(Value v) {
    return (v & NANISH_MASK) == STRING_TAG;
}

static inline bool is_string(Value v) {
	// Because of the particular bit patterns chosen, instead of:
    //return is_tiny_string(v) || is_heap_string(v);
    // ...we can do just:
    return (v & STRING_TAG) == STRING_TAG;
}

static inline bool is_funcref(Value v) {
    return (v & NANISH_MASK) == FUNCREF_TAG;
}

static inline bool is_list(Value v) {
    return (v & NANISH_MASK) == LIST_TAG;
}

static inline bool is_map(Value v) {
    return (v & NANISH_MASK) == MAP_TAG;
}

static inline bool is_double(Value v) {
	uint64_t top16 = v & NANISH_MASK;
	// Check if it's outside our reserved NaN range
	return top16 < val_null;  // Since val_null is your lowest boxed type
}

static inline bool is_number(Value v) {
    return is_int(v) || is_double(v);
}

bool is_truthy(Value v);

// Core value creation functions
static inline Value make_null(void) {
    return val_null;
}

static inline Value make_int(int32_t i) {
    return INTEGER_TAG | (uint64_t)(uint32_t)i;
}

static inline Value make_double(double d) {
	// This looks expensive, but it's the only 100% portable way, and modern
	// compilers will recognize it as a bit-cast and emit a single move instruction.
    Value v;
    memcpy(&v, &d, sizeof v);   // bitwise copy; aliasing-safe
    return v;
}

// ValueFuncRef represents a function reference with closure support
typedef struct {
    int32_t funcIndex;    // Index into the VM's functions array identifying the FuncDef
    Value outerVars;      // VarMap containing captured outer variables, or null if none
} ValueFuncRef;

// Forward declare GC allocation function (implemented in gc.h/.c)
extern void* gc_allocate(size_t size);

// FuncRef accessor functions
static inline ValueFuncRef* as_funcref(Value v) {
    if (!is_funcref(v)) return NULL;
    return (ValueFuncRef*)(uintptr_t)(v & 0xFFFFFFFFFFFFULL);
}

static inline int32_t funcref_index(Value v) {
    ValueFuncRef* funcRefObj = as_funcref(v);
    return funcRefObj ? funcRefObj->funcIndex : -1;
}

static inline Value funcref_outer_vars(Value v) {
    ValueFuncRef* funcRefObj = as_funcref(v);
    return funcRefObj ? funcRefObj->outerVars : make_null();
}

// FuncRef creation function
static inline Value make_funcref(int32_t funcIndex, Value outerVars) {
    ValueFuncRef* funcRefObj = (ValueFuncRef*)gc_allocate(sizeof(ValueFuncRef));
    funcRefObj->funcIndex = funcIndex;
    funcRefObj->outerVars = outerVars;
    return FUNCREF_TAG | ((uintptr_t)funcRefObj & 0xFFFFFFFFFFFFULL);
}

// Map creation functions (forward declarations for value_map.h)
extern Value make_map(int initial_capacity);
extern Value make_empty_map(void);

// Core value extraction functions
static inline int32_t as_int(Value v) {
    return (int32_t)v;
}

static inline double as_double(Value v) {
	// See comments in make_double.
    double d;
    memcpy(&d, &v, sizeof d);   // aliasing-safe bit copy
    return d;
}

// Map extraction function (forward declaration for value_map.h)
extern ValueMap* as_map(Value v);

// Utility functions for accessing tiny string data within Value
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define GET_VALUE_DATA_PTR(v_ptr) ((char*)(v_ptr))
    #define GET_VALUE_DATA_PTR_CONST(v_ptr) ((const char*)(v_ptr))
#else
    #define GET_VALUE_DATA_PTR(v_ptr) (((char*)(v_ptr)) + 2)
    #define GET_VALUE_DATA_PTR_CONST(v_ptr) (((const char*)(v_ptr)) + 2)
#endif

// Forward declarations for string functions (implemented in value_string.h/c)
extern Value string_concat(Value a, Value b);
extern Value make_string(const char* str);
extern const char* get_string_data_zerocopy(const Value* v_ptr, int* out_len);
extern int string_compare(Value a, Value b);

// Conversion functions

Value to_string(Value v);
Value value_repr(Value v);  // Quoted representation for literals
Value to_number(Value v);

// Arithmetic operations (inlined for performance)
static inline Value value_add(Value a, Value b) {
    // Handle integer + integer case
    if (is_int(a) && is_int(b)) {
        // Use int64_t to detect overflow
        int64_t result = (int64_t)as_int(a) + (int64_t)as_int(b);
        if (result >= INT32_MIN && result <= INT32_MAX) {
            return make_int((int32_t)result);
        } else {
            // Overflow to double
            return make_double((double)result);
        }
    }
    
    // Handle mixed integer/double or double/double cases
    if (is_number(a) && is_number(b)) {
        double da = is_int(a) ? (double)as_int(a) : as_double(a);
        double db = is_int(b) ? (double)as_int(b) : as_double(b);
        return make_double(da + db);
    }
    
    // Handle string concatenation

    if (is_string(a)) {
        if (is_string(b)) return string_concat(a, b);
        if (is_int(b) || is_double(b)) return string_concat(a, to_string(b));
	} else if (is_string(b)) {
        if (is_int(a) || is_double(a)) return string_concat(to_string(a), b);
    }
    
    // For now, return nil for unsupported operations
    return make_null();
}

static inline Value value_sub(Value a, Value b) {
    // Handle integer - integer case
    if (is_int(a) && is_int(b)) {
        // Use int64_t to detect overflow/underflow
        int64_t result = (int64_t)as_int(a) - (int64_t)as_int(b);
        if (result >= INT32_MIN && result <= INT32_MAX) {
            return make_int((int32_t)result);
        } else {
            // Overflow/underflow to double
            return make_double((double)result);
        }
    }
    
    // Handle mixed integer/double or double/double cases
    if (is_number(a) && is_number(b)) {
        double da = is_int(a) ? (double)as_int(a) : as_double(a);
        double db = is_int(b) ? (double)as_int(b) : as_double(b);
        return make_double(da - db);
    }
    
    // Return nil for unsupported operations
    return make_null();
}

// Forward declaration for string equality function
extern bool string_equals(Value a, Value b);

// Most critical comparison function (inlined for performance)
static inline bool value_lt(Value a, Value b) {
    // Handle numeric comparisons
    if (is_number(a) && is_number(b)) {
        double da = is_int(a) ? (double)as_int(a) : as_double(a);
        double db = is_int(b) ? (double)as_int(b) : as_double(b);
        return da < db;
    }
    
    // Handle string comparisons (Unicode-aware)
    if (is_string(a) && is_string(b)) {
        return string_compare(a, b) < 0;
    }
    
    // For now, return false for unsupported comparisons
    return false;
}

extern Value value_mult_nonnumeric(Value a, Value b);
static inline Value value_mult(Value a, Value b) {
    // Handle integer * integer case
    if (is_int(a) && is_int(b)) {
        // Use int64_t to detect overflow
        int64_t result = (int64_t)as_int(a) * (int64_t)as_int(b);
        if (result >= INT32_MIN && result <= INT32_MAX) {
            return make_int((int32_t)result);
        } else {
            // Overflow to double
            return make_double((double)result);
        }
    }
    
    // Handle mixed integer/double or double/double cases
    if (is_number(a) && is_number(b)) {
        double da = is_int(a) ? (double)as_int(a) : as_double(a);
        double db = is_int(b) ? (double)as_int(b) : as_double(b);
        return make_double(da * db);
    }
    
    // Everything else, go to the non-numeric handler
    return value_mult_nonnumeric(a, b);
}

static inline Value value_div(Value a, Value b) {
    // Handle integer / integer case
    if (is_int(a) && is_int(b)) {
        // Use int64_t to detect overflow
        int64_t result = (int64_t)as_int(a) / (int64_t)as_int(b);
        if (result >= INT32_MIN && result <= INT32_MAX) {
            return make_int((int32_t)result);
        } else {
            // Overflow to double
            return make_double((double)result);
        }
    }
    
    // Handle mixed integer/double or double/double cases
    if (is_number(a) && is_number(b)) {
        double da = is_int(a) ? (double)as_int(a) : as_double(a);
        double db = is_int(b) ? (double)as_int(b) : as_double(b);
        return make_double(da / db);
    // Handle string / number
    } else if (is_string(a) && is_number(b)) {
    	// We'll just call through to value_mult for this, with a factor of 1/b.
    	return value_mult_nonnumeric(a, value_div(make_double(1), b));
    }
    return make_null();
}

static inline Value value_mod(Value a, Value b) {
    // Handle integer % integer case
    if (is_int(a) && is_int(b)) {
        // Use int64_t to detect overflow
        int64_t result = (int64_t)as_int(a) % (int64_t)as_int(b);
        if (result >= INT32_MIN && result <= INT32_MAX) {
            return make_int((int32_t)result);
        } else {
            // Overflow to double
            return make_double((double)result);
        }
    }
    
    // Handle mixed integer/double or double/double cases
    if (is_number(a) && is_number(b)) {
        double da = is_int(a) ? (double)as_int(a) : as_double(a);
        double db = is_int(b) ? (double)as_int(b) : as_double(b);
        return make_double(fmod(da, db));
    }
    return make_null();
}

// Value comparison (most critical ones inlined above, others implemented in value.c)
bool value_equal(Value a, Value b);
bool value_le(Value a, Value b);
static inline bool value_gt(Value a, Value b) { return !value_le(a, b); }
static inline bool value_ge(Value a, Value b) { return !value_lt(a, b); }


// Bit-wise operations
Value value_and(Value a, Value b);
Value value_or(Value a, Value b);
Value value_xor(Value a, Value b);
Value value_unary(Value a);  // ~ (not)
Value value_shr(Value v, int shift);
Value value_shl(Value v, int shift);

// Hash function for Values
uint32_t value_hash(Value v);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NANBOX_H