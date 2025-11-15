#include "gc.h"
#include "value.h"
#include "value_string.h"
#include "value_list.h"
#include "value_map.h"
#include "StringStorage.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "layer_defs.h"
#if LAYER_2A_HIGHER
#error "gc.c (Layer 2A) cannot depend on higher layers (3A, 4)"
#endif
#if LAYER_2A_BSIDE
#error "gc.c (Layer 2A - runtime) cannot depend on B-side layers (2B, 3B)"
#endif

// #define GC_DEBUG 1
// #define GC_AGGRESSIVE 1  // Collect on every allocation (for testing)

// GC Object header - minimal overhead
typedef struct GCObject {
    struct GCObject* next;  // Linked list of all objects
    bool marked;           // Mark bit for GC
    size_t size;          // Size for sweep phase
} GCObject;

// Scope management for RAII-style protection
typedef struct GCScope {
    int start_index;      // Where this scope starts in the root stack
} GCScope;

// Root set management - shadow stack of pointers to local Values
typedef struct GCRootSet {
    Value** roots;        // Array of pointers to Values (shadow stack)
    int count;
    int capacity;
} GCRootSet;

// GC state
typedef struct GC {
    GCObject* all_objects;    // Linked list of all allocated objects
    GCRootSet root_set;       // Stack of root values
    GCScope scope_stack[64];  // Stack of scopes for RAII-style protection
    int scope_count;          // Number of active scopes
    size_t bytes_allocated;   // Total allocated memory
    size_t gc_threshold;      // Trigger collection when exceeded
    int disable_count;        // Counter for nested disable/enable calls
    int collections_count;    // Number of collections performed
} GC;

// Global GC instance
GC gc = {0};

// Forward declarations for marking functions
void gc_mark_string(StringStorage* str);
void gc_mark_list(ValueList* list);
void gc_mark_map(ValueMap* map);

void gc_init(void) {
    gc.all_objects = NULL;
    gc.root_set.roots = malloc(sizeof(Value*) * 64);  // Array of Value* (shadow stack)
    gc.root_set.count = 0;
    gc.root_set.capacity = 64;
    gc.scope_count = 0;
    gc.bytes_allocated = 0;
    gc.gc_threshold = 1024 * 1024;  // 1MB initial threshold
    gc.disable_count = 0;
    gc.collections_count = 0;
}

void gc_shutdown(void) {
    // Force final collection to clean up everything
    gc_collect();
    
    // Free any remaining objects (shouldn't be any)
    GCObject* obj = gc.all_objects;
    while (obj) {
        GCObject* next = obj->next;
        free(obj);
        obj = next;
    }
    
    // Free root set
    free(gc.root_set.roots);
    memset(&gc, 0, sizeof(gc));
}

void gc_protect_value(Value* val_ptr) {
	assert(gc.root_set.capacity > 0);	// if this fails, it means we forgot to call gc_init()
    if (gc.root_set.count >= gc.root_set.capacity) {
        gc.root_set.capacity *= 2;
        gc.root_set.roots = realloc(gc.root_set.roots, 
                                   sizeof(Value*) * gc.root_set.capacity);
    }
    gc.root_set.roots[gc.root_set.count++] = val_ptr;
}

void gc_unprotect_value(void) {
    assert(gc.root_set.count > 0);
    gc.root_set.count--;
}

void gc_push_scope(void) {
	assert(gc.root_set.capacity > 0);	// if this fails, it means we forgot to call gc_init()
    assert(gc.scope_count < 64);
    gc.scope_stack[gc.scope_count].start_index = gc.root_set.count;
    gc.scope_count++;
}

void gc_pop_scope(void) {
    assert(gc.scope_count > 0);
    gc.scope_count--;
    int start = gc.scope_stack[gc.scope_count].start_index;
    
    // Unprotect everything added in this scope - just reset count directly
    gc.root_set.count = start;
}

void gc_disable(void) {
    gc.disable_count++;
}

void gc_enable(void) {
    assert(gc.disable_count > 0);
    gc.disable_count--;
}

void* gc_allocate(size_t size) {
    // Trigger collection BEFORE allocation
#ifdef GC_AGGRESSIVE
    // Aggressive mode: collect on every allocation (for testing)
    if (gc.disable_count == 0) {
        gc_collect();
    }
#else
    // Normal mode: collect when threshold exceeded
    if (gc.disable_count == 0 && gc.bytes_allocated > gc.gc_threshold) {
        gc_collect();
    }
#endif
    
    // Add space for GC header
    size_t total_size = sizeof(GCObject) + size;
    GCObject* obj = malloc(total_size);
    
    if (!obj) {
        // Try collecting garbage and retry
        gc_collect();
        obj = malloc(total_size);
        if (!obj) {
            fprintf(stderr, "Out of memory!\n");
            exit(1);
        }
    }
    
    // Initialize GC header
    obj->next = gc.all_objects;
    obj->marked = false;
    obj->size = total_size;
    gc.all_objects = obj;
    
    gc.bytes_allocated += total_size;
    
    // Return pointer to data area (after header)
    return (char*)obj + sizeof(GCObject);
}

void gc_mark_value(Value v) {
    if (is_string(v)) {
        StringStorage* str = as_string(v);
        if (str) gc_mark_string(str);
    } else if (is_list(v)) {
        ValueList* list = as_list(v);
        if (list) gc_mark_list(list);
    } else if (is_map(v)) {
        ValueMap* map = as_map(v);
        if (map) gc_mark_map(map);
    }
    // Numbers, ints, nil don't need marking
}

void gc_mark_string(StringStorage* str) {
    if (!str) return;
    
    // Get GC object header (it's right before the String data)
    GCObject* obj = (GCObject*)((char*)str - sizeof(GCObject));
    
    if (!obj->marked) {
        obj->marked = true;
    }
    // Strings don't contain other Values, so we're done
}

void gc_mark_list(ValueList* list) {
    if (!list) return;

    // Get GC object header
    GCObject* obj = (GCObject*)((char*)list - sizeof(GCObject));

    if (obj->marked) return;  // Already marked

    obj->marked = true;

    // Mark all items in the list
    for (int i = 0; i < list->count; i++) {
        gc_mark_value(list->items[i]);
    }
}

void gc_mark_map(ValueMap* map) {
    if (!map) return;

    // Get GC object header for the ValueMap structure
    GCObject* map_obj = (GCObject*)((char*)map - sizeof(GCObject));

    if (map_obj->marked) return;  // Already marked

    map_obj->marked = true;

    // Mark the entries array (if it exists)
    if (map->entries) {
        GCObject* entries_obj = (GCObject*)((char*)map->entries - sizeof(GCObject));
        if (!entries_obj->marked) {
            entries_obj->marked = true;
        }

        // Mark all keys and values in the map
        for (int i = 0; i < map->capacity; i++) {
            if (map->entries[i].occupied) {
                gc_mark_value(map->entries[i].key);
                gc_mark_value(map->entries[i].value);
            }
        }
    }
}

void gc_mark_phase(void) {
    // Mark all objects reachable from roots (shadow stack)
    for (int i = 0; i < gc.root_set.count; i++) {
        Value* root_ptr = gc.root_set.roots[i];
        if (root_ptr) {
            Value root = *root_ptr;
            gc_mark_value(root);
        }
    }
    
    // Note: Interned strings are allocated with malloc() and are immortal,
    // so they don't need to be marked during GC
}

static void gc_sweep_phase(void) {
    GCObject** obj_ptr = &gc.all_objects;
    
    while (*obj_ptr) {
        GCObject* obj = *obj_ptr;
        
        if (obj->marked) {
            // Object is live, clear mark for next collection
            obj->marked = false;
            obj_ptr = &obj->next;
        } else {
            // Object is garbage, remove from list and free
            *obj_ptr = obj->next;
            gc.bytes_allocated -= obj->size;
            
#ifdef GC_DEBUG
            // Overwrite the object with garbage to catch stale pointer usage
            memset(obj, 0xDEADBEEF, obj->size);
#endif
            free(obj);
        }
    }
}

void gc_collect(void) {
#ifdef GC_DEBUG
	printf("gc_collect: disable_count=%d, bytes_allocated=%ld\n", gc.disable_count, gc.bytes_allocated);
#endif
    if (gc.disable_count > 0) return;
    
    gc.collections_count++;
    size_t before = gc.bytes_allocated;
    
    // Mark & Sweep
    gc_mark_phase();
    gc_sweep_phase();
    
    // Adjust threshold based on how much we freed
    size_t freed = before - gc.bytes_allocated;
    if (freed < gc.gc_threshold / 4) {
        // Didn't free much, increase threshold
        gc.gc_threshold *= 2;
    }
    
#ifdef GC_DEBUG
    printf("GC: freed %zu bytes, %zu bytes remaining, threshold now %zu\n", 
           freed, gc.bytes_allocated, gc.gc_threshold);
#endif
}

// GC statistics
GCStats gc_get_stats(void) {
    GCStats stats = {
        .bytes_allocated = gc.bytes_allocated,
        .gc_threshold = gc.gc_threshold,
        .collections_count = gc.collections_count,
        .root_count = gc.root_set.count,
        .is_enabled = (gc.disable_count == 0)
    };
    return stats;
}

// Accessor functions for debug output (used by gc_debug_output.c)
// These allow traversing internal GC structures without exposing implementation details
void* gc_get_all_objects(void) {
    return gc.all_objects;
}

void* gc_get_next_object(void* obj) {
    if (!obj) return NULL;
    return ((GCObject*)obj)->next;
}

size_t gc_get_object_size(void* obj) {
    if (!obj) return 0;
    return ((GCObject*)obj)->size;
}

bool gc_is_object_marked(void* obj) {
    if (!obj) return false;
    return ((GCObject*)obj)->marked;
}
