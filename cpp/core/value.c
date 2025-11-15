// value.c
//
// Core NaN-boxing implementation utilities
// Most functionality is in value.h as inline functions

#include "value.h"
#include "value_string.h"
#include "value_list.h"
#include "value_map.h"
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
    
    // For now, return nil for unsupported operations
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
    // Different types or unsupported types
    return false;
}

// ToDo: make all the bitwise ops work with doubles, too (as in MiniScript)
// Bitwise AND
Value value_and(Value a, Value b) {
    if (is_int(a) && is_int(b)) {
        return make_int(as_int(a) & as_int(b));
    }
    return make_null();
}

// Bitwise OR
Value value_or(Value a, Value b) {
    if (is_int(a) && is_int(b)) {
        return make_int(as_int(a) | as_int(b));
    }
    return make_null();
}

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
            snprintf(buf, sizeof buf, "%.0f", value);
            return make_string(buf);
        } else if (value > 1E10 || value < -1E10 || (value < 1E-6 && value > -1E-6)) {
            // very large/small numbers in exponential form
            snprintf(buf, sizeof buf, "%.6E", value);
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
