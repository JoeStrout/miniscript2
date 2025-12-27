// layer_defs.h - Layer enforcement for C/C++ core code
//
// This file defines macros to enforce the layer architecture described in
// C_CORE_LAYERS.md. Each header includes this file and uses the appropriate
// macros to check for layer violations at compile time.
//
// Layer architecture:
//   Layer 0: Foundation utilities (hashing, unicode, dispatch macros)
//   Layer 1: String infrastructure (StringStorage)
//   Layer 2A: Runtime value system (value, value_string, value_list, value_map, gc) - VM side
//   Layer 2B: Host C# compatibility (CS_List, CS_String, CS_Dictionary, CS_Math) - Host side
//   Layer 3: Host-value utilities (conversion functions between A and B sides)
//
// Rules:
//   - Lower layers cannot depend on higher layers
//   - A-side (Layer 2A) cannot depend on B-side (Layer 2B) and vice versa

#ifndef LAYER_DEFS_H
#define LAYER_DEFS_H

// First, convert defined symbols to 0/1 flags to avoid "defined in macro expansion" warnings
#ifdef CORE_LAYER_1
#define HAS_LAYER_1 1
#else
#define HAS_LAYER_1 0
#endif

#ifdef CORE_LAYER_2A
#define HAS_LAYER_2A 1
#else
#define HAS_LAYER_2A 0
#endif

#ifdef CORE_LAYER_2B
#define HAS_LAYER_2B 1
#else
#define HAS_LAYER_2B 0
#endif

#ifdef CORE_LAYER_3
#define HAS_LAYER_3 1
#else
#define HAS_LAYER_3 0
#endif

// ============================================================================
// Layer 0: Foundation Utilities
// Cannot depend on: anything (this is the foundation)
// ============================================================================

#define LAYER_0_VIOLATIONS \
    (HAS_LAYER_1 || HAS_LAYER_2A || HAS_LAYER_2B || HAS_LAYER_3)

// ============================================================================
// Layer 1: String Infrastructure
// Cannot depend on: 2A, 2B, 3
// Can depend on: Layer 0
// ============================================================================

#define LAYER_1_VIOLATIONS \
    (HAS_LAYER_2A || HAS_LAYER_2B || HAS_LAYER_3)

// ============================================================================
// Layer 2A: Runtime Value System (A-side: VM/Runtime)
// Includes: value.h/.c, value_string.h/.c, value_list.h/.c, value_map.h/.c, gc.h/.c
// Cannot depend on higher layers: 3
// Cannot depend on B-side: 2B
// Can depend on: Layer 0, Layer 1
// Note: This is a cohesive subsystem - GC and Values are mutually interdependent peers
// ============================================================================

#define LAYER_2A_HIGHER \
    (HAS_LAYER_3)

#define LAYER_2A_BSIDE \
    (HAS_LAYER_2B)

// ============================================================================
// Layer 2B: Host C# Compatibility Layer (B-side: Host/Compiler)
// Includes: CS_List.h, CS_String.h, CS_Dictionary.h, CS_Math.h, core_includes.h
// Cannot depend on higher layers: 3
// Cannot depend on A-side: 2A
// Can depend on: Layer 0, Layer 1
// ============================================================================

#define LAYER_2B_HIGHER \
    (HAS_LAYER_3)

#define LAYER_2B_ASIDE \
    (HAS_LAYER_2A)

// ============================================================================
// Layer 3: Host-Value Utilities
// No restrictions - this is the top layer and can depend on both A and B sides
// ============================================================================

// (No violation macros needed for Layer 3 - it's the top)

#endif // LAYER_DEFS_H
