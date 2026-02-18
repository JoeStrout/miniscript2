// value.c
//
// Core NaN-boxing implementation utilities
// Most functionality is in value.h as inline functions

#include "value.h"
#include "value_string.h"
#include "value_list.h"
#include "value_map.h"
#include "vm_error.h"
#include "gc.h"
#include "StringStorage.h"
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "layer_defs.h"
#if LAYER_2A_HIGHER
#error "value.c (Layer 2A) cannot depend on higher layers (3A, 4)"
#endif
#if LAYER_2A_BSIDE
#error "value.c (Layer 2A - runtime) cannot depend on B-side layers (2B, 3B)"
#endif
#include <string.h>

// Global constant values, initialized by value_init_constants()
Value val_isa_key = 0;
Value val_self = 0;
Value val_super = 0;

void value_init_constants(void) {
	val_isa_key = make_string("__isa");
	val_self = make_string("self");
	val_super = make_string("super");
}

// Debug utilities for Value inspection
// Arithmetic operations for VM support
// Note: value_add() and value_sub() are now inlined in value.h

Value value_mult_nonnumeric(Value a, Value b) {
    // Handle string repetition: string * int or int * string
    if (is_string(a) && is_int(b)) {
        int count = as_int(b);
        if (count <= 0) return val_empty_string;
        if (count == 1) return a;
        
        // Build repeated string
        Value result = a;
        for (int i = 1; i < count; i++) {
            result = string_concat(result, a);
        }
        return result;
    } else if (is_string(a) && is_double(b)) {
        int repeats = 0;
        int extraChars = 0;
        double factor = as_double(b);
        int factorClass = fpclassify(factor);
        if (factorClass == FP_NAN || factorClass == FP_INFINITE) return make_null();
        if (factorClass <= 0) return val_empty_string;

        repeats = (int)factor;
        Value result = val_empty_string;
        for (int i = 0; i < repeats; i++) {
            result = string_concat(result, a);
        }
        extraChars = (int)(string_length(a) * (factor - repeats));
        if (extraChars > 0) result = string_concat(result, string_substring(a, 0, extraChars));
        return result;
    }
    
    // Handle list replication: list * number
    if (is_list(a) && is_number(b)) {
        double factor = is_int(b) ? (double)as_int(b) : as_double(b);
        int factorClass = fpclassify(factor);
        if (factorClass == FP_NAN || factorClass == FP_INFINITE) return make_null();
        int len = list_count(a);
        if (factor <= 0 || len == 0) return make_list(0);
        int fullCopies = (int)factor;
        int extraItems = (int)(len * (factor - fullCopies));
        Value result = make_list(fullCopies * len + extraItems);
        for (int c = 0; c < fullCopies; c++) {
            for (int i = 0; i < len; i++) {
                list_push(result, list_get(a, i));
            }
        }
        for (int i = 0; i < extraItems; i++) {
            list_push(result, list_get(a, i));
        }
        return result;
    }

    return make_null();
}

// Note: value_lt() is now inlined in value.h

// ToDo: inline the following too.
bool value_le(Value a, Value b) {
    // Handle numeric comparisons
    if (is_number(a) && is_number(b)) {
        double da = is_int(a) ? (double)as_int(a) : as_double(a);
        double db = is_int(b) ? (double)as_int(b) : as_double(b);
        return da <= db;
    }
    
    // Handle string comparisons (Unicode-aware)
    if (is_string(a) && is_string(b)) {
        return string_compare(a, b) <= 0;
    }
    
    // For now, return false for unsupported comparisons
    return false;
}

bool value_equal(Value a, Value b) {
	bool sameType = ((a & NANISH_MASK) == (b & NANISH_MASK));
	
    if (is_int(a) && sameType) {
        return as_int(a) == as_int(b);
    }
    if (is_double(a) && sameType) {
        return as_double(a) == as_double(b);
    }
    if (is_string(a) && sameType) {
        return string_equals(a, b);
    }
    // Mixed int/double comparison
    if (is_number(a) && is_number(b)) {
        double da = is_int(a) ? (double)as_int(a) : as_double(a);
        double db = is_int(b) ? (double)as_int(b) : as_double(b);
        return da == db;
    }
    // Nulls
    if (is_null(a) && sameType) {
    	return true;
    }
    // Lists: compare by content
	// (ToDo: limit recursion in case of circular references)
    if (is_list(a) && sameType) {
        if (a == b) return true;  // identity shortcut
        int countA = list_count(a);
        if (countA != list_count(b)) return false;
        for (int i = 0; i < countA; i++) {
            if (!value_equal(list_get(a, i), list_get(b, i))) return false;
        }
        return true;
    }
    // Maps: compare by content
	// (ToDo: limit recursion in case of circular references)
    if (is_map(a) && sameType) {
        if (a == b) return true;  // identity shortcut
        ValueMap* mapA = as_map(a);
        ValueMap* mapB = as_map(b);
        if (!mapA || !mapB) return false;
        if (mapA->count != mapB->count) return false;
        for (int i = 0; i < mapA->capacity; i++) {
            if (!mapA->entries[i].occupied) continue;
            Value val;
            if (!map_try_get(b, mapA->entries[i].key, &val)) return false;
            if (!value_equal(mapA->entries[i].value, val)) return false;
        }
        return true;
    }
    // Other same-type reference values (funcrefs): identity comparison
    if (sameType) return a == b;
    // Different types
    return false;
}

// ToDo: make all the bitwise ops work with doubles, too (as in MiniScript)
// (or really todo: eliminate these, they should be intrinsics)

// Bitwise XOR
Value value_xor(Value a, Value b) {
    if (is_int(a) && is_int(b)) {
        return make_int(as_int(a) ^ as_int(b));
    }
    return make_null();
}

// Bitwise NOT (unary)
Value value_unary(Value a) {
    if (is_int(a)) {
        return make_int(~as_int(a));
    }
    return make_null();
}

// Shift Right
Value value_shr(Value v, int shift) {
    if (!is_int(v)) {
        return make_null();
    }

    // Logical shift for unsigned behavior
    return make_int((uint32_t)as_int(v) >> shift);
}

Value value_shl(Value v, int shift) {
    if (!is_int(v)) {
        // Unsupported type for shift-left
        return make_null();
    }

    int64_t result = (int64_t)as_int(v) << shift;

    // Check for overflow beyond 32-bit signed integer range
    if (result >= INT32_MIN && result <= INT32_MAX) {
        return make_int((int32_t)result);
    } else {
        // Overflow: represent as double
        return make_double((double)result);
    }
}

// Conversion functions


// TODO: Consider inlining this.
// TODO: Add support for lists and maps
// Using raw C-String
// Convert value to quoted representation (for literals)
Value value_repr(Value v) {
    if (is_string(v)) {
        // For strings, return quoted representation with internal quotes doubled
        const char* content = as_cstring(v);
        if (!content) content = "";

        // Count quotes to determine output size
        int quote_count = 0;
        for (const char* p = content; *p; p++) {
            if (*p == '"') quote_count++;
        }

        // Allocate buffer: original length + 2 (outer quotes) + quote_count (doubled quotes)
        int orig_len = strlen(content);
        int new_len = orig_len + 2 + quote_count;
        char* escaped = malloc(new_len + 1);

        // Build escaped string
        escaped[0] = '"';
        char* out = escaped + 1;
        for (const char* p = content; *p; p++) {
            if (*p == '"') {
                *out++ = '"';  // First quote
                *out++ = '"';  // Doubled quote
            } else {
                *out++ = *p;
            }
        }
        *out++ = '"';
        *out = '\0';

        Value result = make_string(escaped);
        free(escaped);
        return result;
    } else {
        // For everything else, use normal string representation
        return to_string(v);
    }
}

Value to_string(Value v) {
    char buf[32];

    if (is_string(v)) return v;
    if (is_double(v)) {
        double value = as_double(v);
        if (fmod(value, 1.0) == 0.0) {
            // integer values as integers
            snprintf(buf, sizeof buf, "%.0f", value);
            // Handle negative zero
            if (strcmp(buf, "-0") == 0) buf[0] = '0', buf[1] = '\0';
            return make_string(buf);
        } else if (value > 1E10 || value < -1E10 || (value < 1E-6 && value > -1E-6)) {
            // very large/small numbers in exponential form
            snprintf(buf, sizeof buf, "%.6E", value);
            // Clean up 3-digit exponents: E+00N -> E+0N, E-00N -> E-0N
            // (Some platforms produce 3-digit exponents; we want 2-digit minimum)
            char* e = strchr(buf, 'E');
            if (e && (e[1] == '+' || e[1] == '-') && e[2] == '0' && e[3] == '0' && e[4] != '\0') {
                // Found E±00N pattern, remove one leading zero from exponent
                memmove(e + 3, e + 4, strlen(e + 4) + 1);
            }
            return make_string(buf);
        } else {
            // all others in decimal form, with 1-6 digits past the decimal point

            // Old MiniScript 1.0 code:
                //String s = String::Format(value, "%.6f");
                //long i = s.LengthB() - 1;
                //while (i > 1 && s[i] == '0' && s[i-1] != '.') i--;
                //if (i+1 < s.LengthB()) s = s.SubstringB(0, i+1);
                //
            // Converted code:
            size_t i;

            snprintf(buf, sizeof buf, "%.6f", value);
            i = strlen(buf) - 1;
            while (i > 1 && buf[i] == '0' && buf[i-1] != '.') i--;
            if (i+1 < strlen(buf)) buf[i+1] = '\0';
            return make_string(buf);
        }
    }
    else if (is_int(v)) {
        snprintf(buf, sizeof buf, "%d", as_int(v));
        return make_string(buf);
    }
    else if (is_list(v)) {
        return list_to_string(v);
    }
    else if (is_map(v)) {
        return map_to_string(v);
    }
    return val_empty_string;
}

Value to_number(Value v) {
	if (is_number(v)) return v;
	if (!is_string(v)) return val_zero;

	// Get the string data
	int len;
	const char* str = get_string_data_zerocopy(&v, &len);
	if (!str || len == 0) return val_zero;

	// Parse as double using strtod (handles all cases efficiently)
	char* endptr;
	double result = strtod(str, &endptr);

	// Check if parsing was successful
	if (endptr == str) {
		// No conversion performed
		return val_zero;
	}

	// Skip trailing whitespace after the number
	while (endptr < str + len && (*endptr == ' ' || *endptr == '\t' || *endptr == '\n' || *endptr == '\r')) {
		endptr++;
	}

	// Check if we consumed the entire string
	if (endptr != str + len) {
		// Invalid characters after the number
		return val_zero;
	}

	// Check if the result can be represented as int32 and has no fractional part
	if (result >= INT32_MIN && result <= INT32_MAX && result == (double)(int32_t)result) {
		return make_int((int32_t)result);
	}

	return make_double(result);
}

// Inspection
// ToDo: get this into a header somewhere, so it can be inlined
bool is_truthy(Value v) {
    return (!is_null(v) &&
                ((is_int(v) && as_int(v) != 0) ||
                (is_double(v) && as_double(v) != 0.0) ||
                (is_string(v) && string_length(v) != 0)
                ));
    }

// Hash function for Values
uint32_t value_hash(Value v) {
    if (is_heap_string(v)) {
		// For heap strings, use the cached hash from StringStorage
		StringStorage* storage = as_string(v);
		return ss_hash(storage);
    } else if (is_list(v)) {
        // Forward declare list_hash - will be implemented in value_list.c
        extern uint32_t list_hash(Value v);
        return list_hash(v);
    } else if (is_map(v)) {
        // Forward declare map_hash - will be implemented in value_map.c
        extern uint32_t map_hash(Value v);
        return map_hash(v);
    } else {
        // For everything else (int, double, null, tiny strings),
        // hash the raw uint64_t value
        return uint64_hash(v);
    }
}

// Frozen value support

bool is_frozen(Value v) {
    if (is_list(v)) {
        ValueList* list = as_list(v);
        return list != NULL && list->frozen;
    }
    if (is_map(v)) {
        ValueMap* map = as_map(v);
        return map != NULL && map->frozen;
    }
    return false;
}

void freeze_value(Value v) {
    if (is_list(v)) {
        ValueList* list = as_list(v);
        if (!list || list->frozen) return;
        list->frozen = true;
        for (int i = 0; i < list->count; i++) {
            freeze_value(list->items[i]);
        }
    } else if (is_map(v)) {
        ValueMap* map = as_map(v);
        if (!map || map->frozen) return;
        map->frozen = true;
        for (int i = 0; i < map->capacity; i++) {
            if (map->entries[i].occupied) {
                freeze_value(map->entries[i].key);
                freeze_value(map->entries[i].value);
            }
        }
    }
}

Value frozen_copy(Value v) {
    if (is_list(v)) {
        ValueList* list = as_list(v);
        if (!list || list->frozen) return v;
        Value new_list = make_list(list->count > 0 ? list->count : 8);
        ValueList* dst = as_list(new_list);
        dst->frozen = true;
        for (int i = 0; i < list->count; i++) {
            dst->items[i] = frozen_copy(list->items[i]);
        }
        dst->count = list->count;
        return new_list;
    }
    if (is_map(v)) {
        ValueMap* map = as_map(v);
        if (!map || map->frozen) return v;
        Value new_map = make_map(map->capacity);
        ValueMap* dst = as_map(new_map);
        // Copy map contents, then freeze
        for (int i = 0; i < map->capacity; i++) {
            if (map->entries[i].occupied) {
                map_set(new_map, frozen_copy(map->entries[i].key), frozen_copy(map->entries[i].value));
            }
        }
        dst->frozen = true;
        return new_map;
    }
    return v;
}
