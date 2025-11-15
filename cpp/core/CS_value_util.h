// CS_value_util.h - Conversion utilities between host (B-side) and runtime (A-side) types
// This is Layer 4, which can depend on both A-side (runtime/VM) and B-side (host/compiler)

#pragma once

// This module is part of Layer 4 (Host-Value Utilities)
// Define this BEFORE including other headers so they know we're in Layer 4
#define CORE_LAYER_4

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

#ifdef __cplusplus
} // extern "C"
#endif
