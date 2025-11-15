// layer_defs.h - Layer enforcement for C/C++ core code
//
// This file defines macros to enforce the layer architecture described in
// C_CORE_LAYERS.md. Each header includes this file and uses the appropriate
// macros to check for layer violations at compile time.
//
// Layer architecture:
//   Layer 0: Foundation utilities (hashing, unicode, dispatch macros)
//   Layer 1: String infrastructure (StringStorage)
//   Layer 2A: Runtime value system (value, value_string, value_list, value_map, gc) - Runtime/VM side
//   Layer 2B: Host memory management (MemPool, StringPool) - Host/compiler side
//   Layer 3A: (Reserved for future runtime features)
//   Layer 3B: Host C# compatibility (CS_List, CS_String, CS_Math) - Host
//   Layer 4: Host-value utilities (conversion functions between A and B sides)
//
// Rules:
//   - Lower layers cannot depend on higher layers
//   - A-side cannot depend on B-side (and vice versa)

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

#ifdef CORE_LAYER_3A
#define HAS_LAYER_3A 1
#else
#define HAS_LAYER_3A 0
#endif

#ifdef CORE_LAYER_3B
#define HAS_LAYER_3B 1
#else
#define HAS_LAYER_3B 0
#endif

#ifdef CORE_LAYER_4
#define HAS_LAYER_4 1
#else
#define HAS_LAYER_4 0
#endif

// ============================================================================
// Layer 0: Foundation Utilities
// Cannot depend on: anything (this is the foundation)
// ============================================================================

#define LAYER_0_VIOLATIONS \
    (HAS_LAYER_1 || HAS_LAYER_2A || HAS_LAYER_2B || \
     HAS_LAYER_3A || HAS_LAYER_3B || HAS_LAYER_4)

// ============================================================================
// Layer 1: String Infrastructure
// Cannot depend on: 2A, 2B, 3A, 3B, 4
// Can depend on: Layer 0
// ============================================================================

#define LAYER_1_VIOLATIONS \
    (HAS_LAYER_2A || HAS_LAYER_2B || \
     HAS_LAYER_3A || HAS_LAYER_3B || HAS_LAYER_4)

// ============================================================================
// Layer 2A: Runtime Value System (A-side: Runtime/VM)
// Includes: value.h/.c, value_string.h/.c, value_list.h/.c, value_map.h/.c, gc.h/.c
// Cannot depend on higher layers: 3A, 4
// Cannot depend on B-side: 2B, 3B
// Can depend on: Layer 0, Layer 1
// Note: This is a cohesive subsystem - GC and Values are mutually interdependent peers
// ============================================================================

#define LAYER_2A_HIGHER \
    (HAS_LAYER_3A || HAS_LAYER_4)

#define LAYER_2A_BSIDE \
    (HAS_LAYER_2B || HAS_LAYER_3B)

// ============================================================================
// Layer 2B: Host Memory Management (B-side: Host/Compiler)
// Cannot depend on higher layers: 3B, 4
// Cannot depend on A-side: 2A, 3A
// Can depend on: Layer 0, Layer 1
// ============================================================================

#define LAYER_2B_HIGHER \
    (HAS_LAYER_3B || HAS_LAYER_4)

#define LAYER_2B_ASIDE \
    (HAS_LAYER_2A || HAS_LAYER_3A)

// ============================================================================
// Layer 3A: (Reserved for future runtime features)
// Currently unused - reserved for runtime features that genuinely build on
// top of the Value+GC foundation
// ============================================================================

#define LAYER_3A_HIGHER \
    (HAS_LAYER_4)

#define LAYER_3A_BSIDE \
    (HAS_LAYER_2B || HAS_LAYER_3B)

// ============================================================================
// Layer 3B: Host C# Compatibility Layer (B-side: Host/Compiler)
// Cannot depend on higher layers: 4
// Cannot depend on A-side: 2A, 3A
// Can depend on: Layer 0, Layer 1, Layer 2B
// ============================================================================

#define LAYER_3B_HIGHER \
    (HAS_LAYER_4)

#define LAYER_3B_ASIDE \
    (HAS_LAYER_2A || HAS_LAYER_3A)

// ============================================================================
// Layer 4: Host-Value Utilities
// No restrictions - this is the top layer and can depend on both A and B sides
// ============================================================================

// (No violation macros needed for Layer 4 - it's the top)

#endif // LAYER_DEFS_H
