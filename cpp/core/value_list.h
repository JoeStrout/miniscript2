// value_list.h — list operations on NaN-boxed Values.
// All lists live as GCList slots inside GCManager.Lists.

#ifndef LISTS_H
#define LISTS_H

#include "value.h"
#include <stdbool.h>

#define CORE_LAYER_2A

#ifdef __cplusplus
extern "C" {
#endif

// Creation
Value make_list(int initial_capacity);
Value make_empty_list(void);

// Access
int   list_count(Value list_val);
int   list_capacity(Value list_val);
Value list_get(Value list_val, int index);
void  list_set(Value list_val, int index, Value item);
void  list_push(Value list_val, Value item);
Value list_pop(Value list_val);
Value list_pull(Value list_val);
void  list_insert(Value list_val, int index, Value item);
bool  list_remove(Value list_val, int index);

// Search
int  list_indexOf(Value list_val, Value item, int afterIdx);
bool list_contains(Value list_val, Value item);

// Utilities
void  list_clear(Value list_val);
Value list_copy(Value list_val);
Value list_slice(Value list_val, int start, int end);
Value list_concat(Value a, Value b);

// Sorting
void list_sort(Value list_val, bool ascending);
void list_sort_by_key(Value list_val, Value byKey, bool ascending);

// Hash & display
uint32_t list_hash(Value list_val);
Value    list_to_string(Value list_val, void* vm);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LISTS_H
