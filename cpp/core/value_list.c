#include "value_list.h"
#include "value.h"
#include "gc.h"
#include "value_string.h"
#include "hashing.h"
#include <stdlib.h>
#include <assert.h>

#include "layer_defs.h"
#if LAYER_2A_HIGHER
#error "value_list.c (Layer 2A) cannot depend on higher layers (3A, 4)"
#endif
#if LAYER_2A_BSIDE
#error "value_list.c (Layer 2A - runtime) cannot depend on B-side layers (2B, 3B)"
#endif

// List creation and management
Value make_list(int initial_capacity) {
    if (initial_capacity <= 0) initial_capacity = 8; // Default capacity
    ValueList* list = (ValueList*)gc_allocate(sizeof(ValueList) + initial_capacity * sizeof(Value));
    list->count = 0;
    list->capacity = initial_capacity;
    return LIST_TAG | ((uintptr_t)list & 0xFFFFFFFFFFFFULL);
}

Value make_empty_list(void) {
    return make_list(8);  // Default capacity
}

// List access
ValueList* as_list(Value v) {
    if (!is_list(v)) return NULL;
    return (ValueList*)(uintptr_t)(v & 0xFFFFFFFFFFFFULL);
}

int list_count(Value list_val) {
    ValueList* list = as_list(list_val);
    return list ? list->count : 0;
}

int list_capacity(Value list_val) {
    ValueList* list = as_list(list_val);
    return list ? list->capacity : 0;
}

// List element operations
Value list_get(Value list_val, int index) {
    ValueList* list = as_list(list_val);
    if (!list) return make_null();
    if (index < 0) index += list->count;
    if (index >= 0 && index < list->count) {
        return list->items[index];
    }
    return make_null();
}

void list_set(Value list_val, int index, Value item) {
    ValueList* list = as_list(list_val);
    if (!list) return;
    if (index < 0) index += list->count;
    if (index >= 0 && index < list->count) {
        list->items[index] = item;
    }
}

void list_push(Value list_val, Value item) {
    ValueList* list = as_list(list_val);
    if (!list) return;
    
    // Add item if there's space
    if (list->count < list->capacity) {
        list->items[list->count++] = item;
    }
    // NOTE: Capacity expansion requires the caller to manage the Value reference
    // since we cannot modify the passed Value. Use list_needs_expansion() and
    // list_with_expanded_capacity() for manual capacity management.
}

Value list_pop(Value list_val) {
    ValueList* list = as_list(list_val);
    if (!list || list->count <= 0) return make_null();
    
    return list->items[--list->count];
}

void list_insert(Value list_val, int index, Value item) {
    ValueList* list = as_list(list_val);
    if (!list) return;
    if (index < 0) index += list->count;
    if (index < 0 || index > list->count) return;
    
    // Insert item if there's space
    if (list->count >= list->capacity) return;
    
    // Shift elements to the right
    for (int i = list->count; i > index; i--) {
        list->items[i] = list->items[i-1];
    }
    
    list->items[index] = item;
    list->count++;
}

bool list_remove(Value list_val, int index) {
    ValueList* list = as_list(list_val);
    if (!list) return false;
    if (index < 0) index += list->count;
    if (index < 0 || index > list->count) return false;
    
    // Shift elements to the left
    for (int i = index; i < list->count - 1; i++) {
        list->items[i] = list->items[i+1];
    }
    
    list->count--;
    return true;
}

// List searching
int list_indexOf(Value list_val, Value item, int start_pos) {
    ValueList* list = as_list(list_val);
    if (!list) return -1;
    
    if (start_pos < 0) start_pos = 0;
    
    for (int i = start_pos; i < list->count; i++) {
        if (value_equal(list->items[i], item)) {
            return i;
        }
    }
    return -1;
}

bool list_contains(Value list_val, Value item) {
    return list_indexOf(list_val, item, 0) != -1;
}

// List utilities
void list_clear(Value list_val) {
    ValueList* list = as_list(list_val);
    if (list) {
        list->count = 0;
    }
}

Value list_copy(Value list_val) {
    ValueList* src = as_list(list_val);
    if (!src) return make_null();
    
    Value new_list = make_list(src->capacity);
    ValueList* dst = as_list(new_list);
    
    dst->count = src->count;
    for (int i = 0; i < src->count; i++) {
        dst->items[i] = src->items[i];
    }
    
    return new_list;
}


// Check if list needs capacity expansion
bool list_needs_expansion(Value list_val) {
    ValueList* list = as_list(list_val);
    return list && (list->count >= list->capacity);
}

// Create a new list with expanded capacity, copying all elements
Value list_with_expanded_capacity(Value list_val) {
    ValueList* old_list = as_list(list_val);
    if (!old_list) return make_null();
    
    // Double the capacity (minimum of 2)
    int new_capacity = old_list->capacity * 2;
    if (new_capacity < 2) new_capacity = 2;
    
    // Allocate new list directly to avoid make_list's minimum capacity constraint
    ValueList* new_list = (ValueList*)gc_allocate(sizeof(ValueList) + new_capacity * sizeof(Value));
    new_list->count = old_list->count;
    new_list->capacity = new_capacity;
    
    // Copy all existing elements
    for (int i = 0; i < old_list->count; i++) {
        new_list->items[i] = old_list->items[i];
    }
    
    return LIST_TAG | ((uintptr_t)new_list & 0xFFFFFFFFFFFFULL);
}

void list_resize(Value list_val, int new_capacity) {
    // Note: This function cannot modify the original Value since it's passed by value
    // Use list_with_expanded_capacity() for capacity expansion instead
    (void)list_val;
    (void)new_capacity;
}

// Hash function for lists
uint32_t list_hash(Value list_val) {
    ValueList* list = as_list(list_val);
    if (!list) return 0;

    // Use a simple hash algorithm: combine the hashes of all elements
    // Using FNV-1a constants for consistency with our other hash functions.
    // ToDo: avoid getting stuck in a loop if list structure is recursive.
    const uint32_t FNV_PRIME = 0x01000193;
    uint32_t hash = 0x811c9dc5; // FNV-1a offset basis

    for (int i = 0; i < list->count; i++) {
        uint32_t element_hash = value_hash(list->items[i]);
        hash ^= element_hash;
        hash *= FNV_PRIME;
    }

    // Ensure hash is never 0 (reserved for "not computed")
    return hash == 0 ? 1 : hash;
}

// Convert list to string representation for runtime (returns GC-managed Value)
Value list_to_string(Value list_val) {
    ValueList* list = as_list(list_val);
    if (!list) return make_string("[]");

    if (list->count == 0) return make_string("[]");

    // Build string: [item1, item2, ...]
    // For now: a simple approach: build each part and concatenate.
    // ToDo: a more efficient approach using Join
    Value result = make_string("[");

    for (int i = 0; i < list->count; i++) {
        if (i > 0) {
            Value comma = make_string(", ");
            result = string_concat(result, comma);
        }

        // Get string representation of item (may call to_string recursively)
        Value item_str = to_string(list->items[i]);
        result = string_concat(result, item_str);
    }

    Value close = make_string("]");
    result = string_concat(result, close);

    return result;
}