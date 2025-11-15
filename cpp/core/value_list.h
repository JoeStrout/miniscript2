// List implementation for NaN-boxed Values.  I.e., this is the underlying
// implementation of MiniScript lists.  All memory management is done via
// the gc module.

#ifndef LISTS_H
#define LISTS_H

#include "value.h"
#include <stdbool.h>

// This module is part of Layer 2A (Runtime Value System + GC)
#define CORE_LAYER_2A

#ifdef __cplusplus
extern "C" {
#endif


// List structure
typedef struct {
    int count;       // Number of elements
    int capacity;    // Allocated capacity
    Value items[];   // Array of Values
} ValueList;

// List creation and management
Value make_list(int initial_capacity);
Value make_empty_list(void);

// List access
ValueList* as_list(Value v);
int list_count(Value list_val);
int list_capacity(Value list_val);

// List element operations
Value list_get(Value list_val, int index);
void list_set(Value list_val, int index, Value item);
void list_push(Value list_val, Value item);
Value list_pop(Value list_val);
void list_insert(Value list_val, int index, Value item);
bool list_remove(Value list_val, int index);

// List searching
int list_indexOf(Value list_val, Value item, int start_pos);
bool list_contains(Value list_val, Value item);

// List utilities
void list_clear(Value list_val);
Value list_copy(Value list_val);
void list_resize(Value list_val, int new_capacity);

// Capacity management utilities
bool list_needs_expansion(Value list_val);
Value list_with_expanded_capacity(Value list_val);

// Hash function for lists
uint32_t list_hash(Value list_val);

// String conversion for runtime (returns GC-managed Value)
Value list_to_string(Value list_val);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LISTS_H