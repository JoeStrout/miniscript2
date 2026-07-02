// value_map.h — map (dictionary) operations on NaN-boxed Values.
// Maps live as GCMap slots inside GCManager.Maps.

#ifndef VALUE_MAP_H
#define VALUE_MAP_H

#include "value.h"
#include <stdbool.h>
#include <limits.h>

#define CORE_LAYER_2A

namespace MiniScript {

// make_varmap / varmap_map_to_register / varmap_gather / varmap_rebind are now
// Value:: static methods (they take List<Value>, which value.h sees via
// CS_String.h -> CS_List.h).

// Creation / access / mutation: make_map, make_empty_map, map_count, map_get,
// map_set(Value key), map_try_get, map_lookup, map_lookup_with_origin,
// map_remove, map_has_key, map_clear are now Value:: static methods.

int  map_capacity(Value map_val);
Value map_copy(Value map_val);
Value map_concat(Value a, Value b);
bool  map_needs_expansion(Value map_val);
bool  map_expand_capacity(Value map_val);
Value map_with_expanded_capacity(Value map_val);

// Iteration (legacy struct-based iterator).
// Tagged (not an anonymous typedef) so value.h can forward-declare it for the
// Value::map_iterator member.
struct MapIterator {
    int map_idx;          // -1 if not a map
    int iter;             // current iteration cursor; matches map_iter_next encoding
};

// map_iterator is now Value::Iterator(); map_nth_entry is now Value::NthEntry().
bool        map_iterator_next(MapIterator* iter, Value* out_key, Value* out_value);

// New iterator interface (matches C# / generated code).
//   -1 = not started
//   <-1 = VarMap reg-entry encoding (deferred; never returned in stubbed mode)
//   >=0 = entry index into the dense map
// (MAP_ITER_DONE is now a Value:: static member; map_iter_next / map_iter_entry
//  are now Value:: static methods.)

// Hash & display
uint32_t map_hash(Value map_val);
Value    map_to_string(Value map_val, void* vm);


}  // namespace MiniScript

#endif // VALUE_MAP_H
