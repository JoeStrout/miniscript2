// gc.h — shim layer over the new index-based GC system.
//
// The legacy gc system (shadow stack, gc_allocate, GC_PROTECT macros)
// is gone. The new GC (cpp/core/GCManager.{h,cpp}) is index-based and
// does not require local Value protection. This header preserves the
// legacy API surface so existing call sites compile unchanged; Phase 3
// will purge most of these calls from CPP_ONLY blocks in the cs files.

#ifndef GC_H
#define GC_H

#include "value.h"
#include <stddef.h>

#define CORE_LAYER_2A

#ifdef __cplusplus
extern "C" {
#endif

// ── Lifecycle ───────────────────────────────────────────────────────────
void gc_init(void);
void gc_shutdown(void);

// Legacy raw allocator. Forwards to malloc — the new GC does not track
// arbitrary allocations. Allocations made through this entry point leak
// (no Free is provided) and only used by transitional code.
void* gc_allocate(size_t size);

// ── Manual collection ───────────────────────────────────────────────────
void gc_collect(void);
void gc_disable(void);
void gc_enable(void);

// ── Local-variable protection (now no-ops) ──────────────────────────────
// The new GC runs only on explicit triggers (gc_collect, yield, wait),
// so local Values cannot be collected mid-call. These functions remain
// callable as no-ops to keep legacy code paths compiling.
void gc_protect_value(Value* val_ptr);
void gc_unprotect_value(void);

// ── Mark callbacks ──────────────────────────────────────────────────────
typedef void (*gc_mark_callback_t)(void* user_data);
void gc_register_mark_callback(gc_mark_callback_t callback, void* user_data);
void gc_unregister_mark_callback(gc_mark_callback_t callback, void* user_data);
void gc_mark_value(Value v);

// ── Scope macros (no-ops; new GC needs neither shadow stack nor scopes) ─
#define GC_PUSH_SCOPE() ((void)0)
#define GC_POP_SCOPE()  ((void)0)

#define GC_LOCALS_1(v1)             Value v1 = val_null
#define GC_LOCALS_2(v1, v2)         Value v1 = val_null, v2 = val_null
#define GC_LOCALS_3(v1, v2, v3)     Value v1 = val_null, v2 = val_null, v3 = val_null
#define GC_LOCALS_4(v1, v2, v3, v4) Value v1 = val_null, v2 = val_null, v3 = val_null, v4 = val_null
#define GC_LOCALS_5(v1, v2, v3, v4, v5) \
    Value v1 = val_null, v2 = val_null, v3 = val_null, v4 = val_null, v5 = val_null
#define GC_LOCALS_6(v1, v2, v3, v4, v5, v6) \
    Value v1 = val_null, v2 = val_null, v3 = val_null, v4 = val_null, v5 = val_null, v6 = val_null
#define GC_LOCALS_7(v1, v2, v3, v4, v5, v6, v7) \
    Value v1 = val_null, v2 = val_null, v3 = val_null, v4 = val_null, v5 = val_null, \
    v6 = val_null, v7 = val_null
#define GC_LOCALS_8(v1, v2, v3, v4, v5, v6, v7, v8) \
    Value v1 = val_null, v2 = val_null, v3 = val_null, v4 = val_null, v5 = val_null, \
    v6 = val_null, v7 = val_null, v8 = val_null

#define GC_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,NAME,...) NAME
#define GC_LOCALS(...) \
    GC_GET_MACRO(__VA_ARGS__, GC_LOCALS_8, GC_LOCALS_7, GC_LOCALS_6, GC_LOCALS_5, \
                 GC_LOCALS_4, GC_LOCALS_3, GC_LOCALS_2, GC_LOCALS_1)(__VA_ARGS__)

#define GC_PROTECT(var_ptr) ((void)(var_ptr))

// ── Stats ───────────────────────────────────────────────────────────────
typedef struct {
    size_t bytes_allocated;
    size_t gc_threshold;
    int    collections_count;
    int    root_count;
    int    scope_depth;
    int    max_scope_depth;
    bool   is_enabled;
} GCStats;

GCStats gc_get_stats(void);
int     gc_get_scope_depth(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GC_H
