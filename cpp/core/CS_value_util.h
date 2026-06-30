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

// make_string(String), map_set(String key) overloads, to_String, and
// value_type_name are now Value:: static methods (declared in value.h, which
// includes CS_String.h).  Nothing host-specific remains here for now.

#endif // __cplusplus
