// Shadow stack garbage collector for NaN-boxed Values (defined in 
// value.h/.c).  Garbage collection may happen on allocation attempts
// (unless disabled with gc_disable()), or explicitly via gc_collect().
// To keep objects referenced by local Value variables from being
// prematurely collected, use the GC_PROTECT macro, within a block
// bracketed by GC_PUSH_SCOPE/GC_POP_SCOPE.  See GC_USAGE.md for details.

#ifndef GC_H
#define GC_H

#include "value.h"
#include <stddef.h>

// This module is part of Layer 2A (Runtime Value System + GC)
#define CORE_LAYER_2A

#ifdef __cplusplus
extern "C" {
#endif


// Initialize/shutdown the garbage collector
void gc_init(void);
void gc_shutdown(void);

// Memory allocation (returns GC-managed memory)
void* gc_allocate(size_t size);

// Manual garbage collection control
void gc_collect(void);
void gc_disable(void);
void gc_enable(void);

// Root set management for local variables
void gc_protect_value(Value* val_ptr);
void gc_unprotect_value(void);

// Mark callback registration - allows subsystems to participate in GC marking
// Callbacks are invoked during mark phase and should call gc_mark_value() on any
// Values they are responsible for (e.g., VM stack, global variables, etc.)
typedef void (*gc_mark_callback_t)(void* user_data);
void gc_register_mark_callback(gc_mark_callback_t callback, void* user_data);
void gc_unregister_mark_callback(gc_mark_callback_t callback, void* user_data);

// Mark a single value (for use by mark callbacks)
void gc_mark_value(Value v);

// Scope management macros for automatic root tracking
#define GC_PUSH_SCOPE() gc_push_scope()
#define GC_POP_SCOPE() gc_pop_scope()

// Protect multiple local variables at once (up to 8)
// These macros both declare and protect the variables
#define GC_LOCALS_1(v1) \
    Value v1 = make_null(); gc_protect_value(&v1)

#define GC_LOCALS_2(v1, v2) \
    Value v1 = make_null(), v2 = make_null(); \
    gc_protect_value(&v1); gc_protect_value(&v2)

#define GC_LOCALS_3(v1, v2, v3) \
    Value v1 = make_null(), v2 = make_null(), v3 = make_null(); \
    gc_protect_value(&v1); gc_protect_value(&v2); gc_protect_value(&v3)

#define GC_LOCALS_4(v1, v2, v3, v4) \
    Value v1 = make_null(), v2 = make_null(), v3 = make_null(), v4 = make_null(); \
    gc_protect_value(&v1); gc_protect_value(&v2); gc_protect_value(&v3); gc_protect_value(&v4)

#define GC_LOCALS_5(v1, v2, v3, v4, v5) \
    Value v1 = make_null(), v2 = make_null(), v3 = make_null(), v4 = make_null(), v5 = make_null(); \
    gc_protect_value(&v1); gc_protect_value(&v2); gc_protect_value(&v3); gc_protect_value(&v4); \
    gc_protect_value(&v5)

#define GC_LOCALS_6(v1, v2, v3, v4, v5, v6) \
    Value v1 = make_null(), v2 = make_null(), v3 = make_null(), v4 = make_null(), v5 = make_null(), \
    v6 = make_null(); \
    gc_protect_value(&v1); gc_protect_value(&v2); gc_protect_value(&v3); gc_protect_value(&v4); \
    gc_protect_value(&v5); gc_protect_value(&v6)

#define GC_LOCALS_7(v1, v2, v3, v4, v5, v6, v7) \
    Value v1 = make_null(), v2 = make_null(), v3 = make_null(), v4 = make_null(), v5 = make_null(), \
    v6 = make_null(), v7 = make_null(); \
    gc_protect_value(&v1); gc_protect_value(&v2); gc_protect_value(&v3); gc_protect_value(&v4); \
    gc_protect_value(&v5); gc_protect_value(&v6); gc_protect_value(&v7)

#define GC_LOCALS_8(v1, v2, v3, v4, v5, v6, v7, v8) \
    Value v1 = make_null(), v2 = make_null(), v3 = make_null(), v4 = make_null(), v5 = make_null(), \
    v6 = make_null(), v7 = make_null(), v8 = make_null(); \
    gc_protect_value(&v1); gc_protect_value(&v2); gc_protect_value(&v3); gc_protect_value(&v4); \
    gc_protect_value(&v5); gc_protect_value(&v6); gc_protect_value(&v7); gc_protect_value(&v8)

// Helper macro to count arguments and dispatch to the right GC_LOCALS_N
#define GC_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,NAME,...) NAME

#define GC_LOCALS(...) \
    GC_GET_MACRO(__VA_ARGS__, GC_LOCALS_8, GC_LOCALS_7, GC_LOCALS_6, GC_LOCALS_5, \
                 GC_LOCALS_4, GC_LOCALS_3, GC_LOCALS_2, GC_LOCALS_1)(__VA_ARGS__)

// Individual variable protection
#define GC_PROTECT(var_ptr) gc_protect_value(var_ptr)

// Internal scope management functions
void gc_push_scope(void);
void gc_pop_scope(void);

// GC statistics and debugging
typedef struct {
    size_t bytes_allocated;
    size_t gc_threshold;
    int collections_count;
    int root_count;
    int scope_depth;      // Current GC scope stack depth
    int max_scope_depth;  // Maximum allowed scope depth
    bool is_enabled;
} GCStats;

GCStats gc_get_stats(void);

// Get current scope depth (useful for debugging scope leaks)
int gc_get_scope_depth(void);

// Accessor functions for debug output (used by gc_debug_output.c)
// These allow traversing internal GC structures without exposing implementation details
void* gc_get_all_objects(void);
void* gc_get_next_object(void* obj);
size_t gc_get_object_size(void* obj);
bool gc_is_object_marked(void* obj);
void gc_mark_phase(void);  // Exposed for gc_mark_and_report

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GC_H