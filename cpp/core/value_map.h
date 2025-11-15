// Map implementation for NaN-boxed Values. I.e., this is the underlying
// implementation of MiniScript maps (dictionaries). All memory management
// is done via the gc module.

#ifndef VALUE_MAP_H
#define VALUE_MAP_H

#include "value.h"
#include <stdbool.h>

// This module is part of Layer 2A (Runtime Value System + GC)
#define CORE_LAYER_2A

#ifdef __cplusplus
extern "C" {
#endif

// Map and MapEntry structures are defined in value.h

// Map creation and management
Value make_map(int initial_capacity);
Value make_empty_map(void);
Value make_varmap(Value* registers, Value* names, int firstIndex, int count);

// Map access
ValueMap* as_map(Value v);
int map_count(Value map_val);
int map_capacity(Value map_val);

// Map operations
Value map_get(Value map_val, Value key);
bool map_try_get(Value map_val, Value key, Value* out_value);
bool map_set(Value map_val, Value key, Value value);
bool map_remove(Value map_val, Value key);
bool map_has_key(Value map_val, Value key);

// Map utilities
void map_clear(Value map_val);
Value map_copy(Value map_val);
bool map_needs_expansion(Value map_val);
bool map_expand_capacity(Value map_val);  // Expands in-place, returns success
Value map_with_expanded_capacity(Value map_val);  // Deprecated - creates new map

// VarMap-specific functions
void varmap_map_to_register(Value map_val, Value var_name, int reg_index);
void varmap_gather(Value map_val);

// Map iteration
typedef struct {
    ValueMap* map;
    int index;
    int varmap_reg_index; // For VarMap: current register mapping index (-1 = not started)
} MapIterator;

MapIterator map_iterator(Value map_val);
bool map_iterator_next(MapIterator* iter, Value* out_key, Value* out_value);

// Hash function for maps
uint32_t map_hash(Value map_val);

// String conversion for runtime (returns GC-managed Value)
Value map_to_string(Value map_val);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // VALUE_MAP_H