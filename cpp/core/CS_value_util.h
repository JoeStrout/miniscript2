// CS_value_util.h - Conversion utilities between host (B-side) and runtime (A-side) types
// This is Layer 3, which can depend on both A-side (runtime/VM) and B-side (host/compiler)

#pragma once

// This module is part of Layer 3 (Host-Value Utilities)
// Define this BEFORE including other headers so they know we're in Layer 3
#define CORE_LAYER_3

#include "value.h"
#include "value_string.h"
#include "CS_String.h"

#ifdef __cplusplus
extern "C" {
#endif

// Convert CS_String (host string) to Value (runtime string)
// This is inline for efficiency but we keep the .cpp file for future utilities
static inline Value make_string(String s) {
    return make_string(s.c_str());
}

// Convert Value to CS_String (host string)
static inline String to_String(Value v) {
    return String(as_cstring(to_string(v)));
}

#ifdef __cplusplus
} // extern "C"
#endif
