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

// Scope management macros for automatic root tracking
#define GC_PUSH_SCOPE() gc_push_scope()
#define GC_POP_SCOPE() gc_pop_scope()

// Protect multiple local variables at once (up to 16)
#define GC_LOCALS(...) \
    do { \
        Value* _locals[] = {__VA_ARGS__}; \
        for (int _i = 0; _i < sizeof(_locals)/sizeof(_locals[0]); _i++) { \
            gc_protect_value(_locals[_i]); \
        } \
    } while(0)

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
    bool is_enabled;
} GCStats;

GCStats gc_get_stats(void);

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