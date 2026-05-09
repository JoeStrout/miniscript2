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
    // Handle string repetition: string * number
    if (is_string(a) && is_double(b)) {
        int repeats = 0;
        int extraChars = 0;
        double factor = as_double(b);
        int factorClass = fpclassify(factor);
        if (factorClass == FP_NAN || factorClass == FP_INFINITE) return val_null;
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
    if (is_list(a) && is_double(b)) {
        double factor = as_double(b);
        int factorClass = fpclassify(factor);
        if (factorClass == FP_NAN || factorClass == FP_INFINITE) return val_null;
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

    return val_null;
}

// Note: value_lt() is now inlined in value.h

// ToDo: inline the following too.
bool value_le(Value a, Value b) {
    if (is_double(a) && is_double(b)) return as_double(a) <= as_double(b);
    if (is_string(a) && is_string(b)) return string_compare(a, b) <= 0;
    return false;
}

bool value_equal(Value a, Value b) {
	bool sameType = ((a & NANISH_MASK) == (b & NANISH_MASK));

    if (is_double(a) && is_double(b)) {
        return as_double(a) == as_double(b);
    }
    if (is_string(a) && sameType) {
        return string_equals(a, b);
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
        if (map_count(a) != map_count(b)) return false;
        for (int i = 0; i < mapA->count; i++) {
            if (mapA->entries[i].next == MAP_ENTRY_REMOVED) continue;
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

// General-purpose comparison: returns <0 if a<b, 0 if a==b, >0 if a>b.
// Numbers sort before strings; strings sort before other types.
int value_compare(Value a, Value b) {
	if (is_number(a) && is_number(b)) {
		double da = numeric_val(a);
		double db = numeric_val(b);
		if (da < db) return -1;
		if (da > db) return 1;
		return 0;
	}
	if (is_string(a) && is_string(b)) {
		return string_compare(a, b);
	}
	// Mixed types: numbers < strings < others
	int ta = is_number(a) ? 0 : is_string(a) ? 1 : 2;
	int tb = is_number(b) ? 0 : is_string(b) ? 1 : 2;
	return (ta < tb) ? -1 : (ta > tb) ? 1 : 0;
}

// ToDo: make all the bitwise ops work with doubles, too (as in MiniScript)
// (or really todo: eliminate these, they should be intrinsics)

// Bitwise XOR
Value value_xor(Value a, Value b) {
    if (is_int(a) && is_int(b)) {
        return make_int(as_int(a) ^ as_int(b));
    }
    return val_null;
}

// Bitwise NOT (unary)
Value value_unary(Value a) {
    if (is_int(a)) {
        return make_int(~as_int(a));
    }
    return val_null;
}

// Shift Right
Value value_shr(Value v, int shift) {
    if (!is_int(v)) {
        return val_null;
    }

    // Logical shift for unsigned behavior
    return make_int((uint32_t)as_int(v) >> shift);
}

Value value_shl(Value v, int shift) {
    if (!is_int(v)) {
        // Unsupported type for shift-left
        return val_null;
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

// Stub for VM short-name lookup; wired up when VM.FindShortName is implemented.
// Returns a borrowed C string (valid until next GC), or NULL if none found.
static const char* find_short_name(void* vm, Value v) {
    (void)vm; (void)v;
    return NULL; // ToDo: call VM callback
}

// Format a double into buf (size >= 32) using MiniScript number rules.
static void format_double(double value, char* buf) {
    if (fmod(value, 1.0) == 0.0) {
        snprintf(buf, 32, "%.0f", value);
        if (strcmp(buf, "-0") == 0) { buf[0] = '0'; buf[1] = '\0'; }
    } else if (value > 1E10 || value < -1E10 || (value < 1E-6 && value > -1E-6)) {
        snprintf(buf, 32, "%.6E", value);
        char* e = strchr(buf, 'E');
        if (e && (e[1] == '+' || e[1] == '-') && e[2] == '0' && e[3] == '0' && e[4] != '\0')
            memmove(e + 3, e + 4, strlen(e + 4) + 1);
    } else {
        size_t i;
        snprintf(buf, 32, "%.6f", value);
        i = strlen(buf) - 1;
        while (i > 1 && buf[i] == '0' && buf[i-1] != '.') i--;
        if (i + 1 < strlen(buf)) buf[i + 1] = '\0';
    }
}

// Return a quoted string Value with internal double-quotes escaped ("" style).
static Value quote_string(Value v) {
    const char* content = as_cstring(v);
    if (!content) content = "";
    int quote_count = 0;
    for (const char* p = content; *p; p++) if (*p == '"') quote_count++;
    int orig_len = (int)strlen(content);
    int new_len = orig_len + 2 + quote_count;
    char* escaped = (char*)malloc(new_len + 1);
    escaped[0] = '"';
    char* out = escaped + 1;
    for (const char* p = content; *p; p++) {
        if (*p == '"') { *out++ = '"'; *out++ = '"'; }
        else *out++ = *p;
    }
    *out++ = '"'; *out = '\0';
    Value result = make_string(escaped);
    free(escaped);
    return result;
}

// Source-code form of a value: strings quoted, containers recursion-limited.
// recursion_limit: -1 = unlimited (no short-name, no truncation)
//                   0 = truncate (emit "[...]" / "{...}")
//                  >0 = render; try short-name when limit < 3
Value code_form(Value v, void* vm, int recursion_limit) {
    char buf[32];

    if (is_null(v)) return make_string("null");

    if (is_double(v)) {
        format_double(as_double(v), buf);
        return make_string(buf);
    }

    if (is_string(v)) return quote_string(v);

    if (is_list(v)) {
        if (recursion_limit == 0) return make_string("[...]");
        if (recursion_limit > 0 && recursion_limit < 3 && vm != NULL) {
            const char* sn = find_short_name(vm, v);
            if (sn) return make_string(sn);
        }
        ValueList* list = as_list(v);
        if (!list) return make_string("[???]");
        Value result = make_string("[");
        Value item_str = val_null;
        for (int i = 0; i < list->count; i++) {
            if (i > 0) result = string_concat(result, make_string(", "));
            Value item = list->items[i];
            item_str = is_null(item) ? make_string("null")
                                     : code_form(item, vm, recursion_limit - 1);
            result = string_concat(result, item_str);
        }
        return string_concat(result, make_string("]"));
    }

    if (is_map(v)) {
        if (recursion_limit == 0) return make_string("{...}");
        if (recursion_limit > 0 && recursion_limit < 3 && vm != NULL) {
            const char* sn = find_short_name(vm, v);
            if (sn) return make_string(sn);
        }
        if (map_count(v) == 0) return make_string("{}");
        Value result = make_string("{");
        MapIterator iter = map_iterator(v);
        Value key = val_null, val = val_null;
        Value key_str = val_null, val_str = val_null;
        bool first = true;
        while (map_iterator_next(&iter, &key, &val)) {
            if (!first) result = string_concat(result, make_string(", "));
            first = false;
            int next_limit = recursion_limit - 1;
            if (value_equal(key, val_isa_key)) next_limit = 1;
            key_str = code_form(key, vm, next_limit);
            val_str = is_null(val) ? make_string("null") : code_form(val, vm, next_limit);
            result = string_concat(result, key_str);
            result = string_concat(result, make_string(": "));
            result = string_concat(result, val_str);
        }
        return string_concat(result, make_string("}"));
    }

    if (is_funcref(v)) {
        int32_t idx = funcref_index(v);
        Value outer = funcref_outer_vars(v);
        if (!is_null(outer))
            snprintf(buf, sizeof buf, "FuncRef(#%d, closure)", idx);
        else
            snprintf(buf, sizeof buf, "FuncRef(#%d)", idx);
        return make_string(buf);
    }

    if (is_error(v)) {
        Value msg = error_message(v);
        if (is_string(msg)) return string_concat(make_string("error: "), msg);
        return make_string("error");
    }

    return make_string("<value>");
}

// to_string: raw string for string values; code_form(3) for everything else.
Value to_string(Value v, void* vm) {
    if (is_string(v)) return v;
    return code_form(v, vm, 3);
}

// value_repr: source-code form with full depth (no truncation or short-name substitution).
Value value_repr(Value v, void* vm) {
    return code_form(v, vm, -1);
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

	return make_double(result);
}

// Inspection
// ToDo: get this into a header somewhere, so it can be inlined
bool is_truthy(Value v) {
    return (!is_null(v) &&
                ((is_double(v) && as_double(v) != 0.0) ||
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
        for (int i = 0; i < map->count; i++) {
            if (map->entries[i].next != MAP_ENTRY_REMOVED) {
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
        // Copy map contents in insertion order, then freeze
        for (int i = 0; i < map->count; i++) {
            if (map->entries[i].next != MAP_ENTRY_REMOVED) {
                map_set(new_map, frozen_copy(map->entries[i].key), frozen_copy(map->entries[i].value));
            }
        }
        dst->frozen = true;
        return new_map;
    }
    return v;
}

