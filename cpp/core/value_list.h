// value_list.h — list operations on NaN-boxed Values.
// All lists live as GCList slots inside GCManager.Lists.

#ifndef LISTS_H
#define LISTS_H

#include "value.h"
#include <stdbool.h>

#define CORE_LAYER_2A


// Creation
// (make_list / list_count / list_get / list_set / list_push / list_pop /
//  list_pull / list_insert / list_remove / list_indexOf / list_slice /
//  list_sort / list_sort_by_key are now Value:: static methods.)
Value make_empty_list(void);

// Access
int   list_capacity(Value list_val);

// Search
bool list_contains(Value list_val, Value item);

// Utilities
void  list_clear(Value list_val);
Value list_copy(Value list_val);
Value list_concat(Value a, Value b);

// Hash & display
uint32_t list_hash(Value list_val);
Value    list_to_string(Value list_val, void* vm);


#endif // LISTS_H
