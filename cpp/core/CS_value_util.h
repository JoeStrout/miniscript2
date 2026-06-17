// CS_value_util.h - Conversion utilities between host (B-side) and runtime (A-side) types
// This is Layer 3, which can depend on both A-side (runtime/VM) and B-side (host/compiler)

#pragma once

// This module is part of Layer 3 (Host-Value Utilities)
// Define this BEFORE including other headers so they know we're in Layer 3
#define CORE_LAYER_3

#include "value.h"
#include "value_string.h"
#include "value_map.h"
#include "CS_String.h"

#ifdef __cplusplus

// Convert CS_String (host string) to Value (runtime string)
static inline Value make_string(String s) {
    return make_string(s.c_str());
}

// map_set overloads: accept String keys (and optionally String values) directly.
static inline bool map_set(Value map_val, const String& key, Value value)         { return map_set(map_val, make_string(key), value); }
static inline bool map_set(Value map_val, const String& key, const String& value) { return map_set(map_val, make_string(key), make_string(value)); }

// Convert Value to CS_String (host string)
static inline String to_String(Value v) {
	// ToDo: look for a way to do this that doesn't go through as_cstring
	// (which copies the buffer); in most cases we should be able to
	// directly reference the StringStorage to which v already refers.
    return String(as_cstring(to_string(v, NULL)));
}

#endif // __cplusplus
