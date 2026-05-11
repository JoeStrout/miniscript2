// value_map.h — map (dictionary) operations on NaN-boxed Values.
// Maps live as GCMap slots inside GCManager.Maps.

#ifndef VALUE_MAP_H
#define VALUE_MAP_H

#include "value.h"
#include <stdbool.h>
#include <limits.h>

#define CORE_LAYER_2A

#ifdef __cplusplus
extern "C" {
#endif

// Creation
Value make_map(int initial_capacity);
Value make_empty_map(void);

// VarMap: stubbed; full port deferred to Phase 4. Calling these returns
// a plain map (no register binding).
Value make_varmap(Value* registers, Value* names, int firstIndex, int count);
void  varmap_map_to_register(Value map_val, Value var_name, Value* registers, int reg_index);
void  varmap_gather(Value map_val);
void  varmap_rebind(Value map_val, Value* registers, Value* names);

// Access
int  map_count(Value map_val);
int  map_capacity(Value map_val);

Value map_get(Value map_val, Value key);
bool  map_try_get(Value map_val, Value key, Value* out_value);
bool  map_lookup(Value map_val, Value key, Value* out_value);
bool  map_lookup_with_origin(Value map_val, Value key, Value* out_value, Value* out_super);
bool  map_set(Value map_val, Value key, Value value);
bool  map_remove(Value map_val, Value key);
bool  map_has_key(Value map_val, Value key);

void  map_clear(Value map_val);
Value map_copy(Value map_val);
Value map_concat(Value a, Value b);
bool  map_needs_expansion(Value map_val);
bool  map_expand_capacity(Value map_val);
Value map_with_expanded_capacity(Value map_val);

// Iteration (legacy struct-based iterator).
typedef struct {
    int map_idx;          // -1 if not a map
    int iter;             // current iteration cursor; matches map_iter_next encoding
} MapIterator;

MapIterator map_iterator(Value map_val);
bool        map_iterator_next(MapIterator* iter, Value* out_key, Value* out_value);
Value       map_nth_entry(Value map_val, int n);

// New iterator interface (matches C# / generated code).
//   -1 = not started
//   <-1 = VarMap reg-entry encoding (deferred; never returned in stubbed mode)
//   >=0 = entry index into the dense map
#define MAP_ITER_DONE INT_MIN
int   map_iter_next(Value map_val, int iter);
Value map_iter_entry(Value map_val, int iter);

// Hash & display
uint32_t map_hash(Value map_val);
Value    map_to_string(Value map_val, void* vm);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // VALUE_MAP_H
