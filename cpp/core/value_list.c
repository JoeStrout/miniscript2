#include "value_list.h"
#include "value.h"
#include "gc.h"
#include "value_string.h"
#include "value_map.h"
#include "vm_error.h"
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

    // Allocate the ValueList struct (fixed size)
    ValueList* list = (ValueList*)gc_allocate(sizeof(ValueList));
    list->count = 0;
    list->capacity = initial_capacity;
    list->frozen = false;
    list->items = NULL;

    // Protect the list Value so the struct survives the second gc_allocate
    Value list_val = LIST_TAG | ((uintptr_t)list & 0xFFFFFFFFFFFFULL);
    GC_PUSH_SCOPE();
    GC_PROTECT(&list_val);

    // Allocate the items array separately (may trigger GC)
    list->items = (Value*)gc_allocate(initial_capacity * sizeof(Value));

    GC_POP_SCOPE();
    return list_val;
}

Value make_empty_list(void) {
    return make_list(8);  // Default capacity
}

// Ensure the list has room for at least one more item.
// If at capacity, allocates a new (doubled) items array via gc_allocate
// and copies existing elements over.  The old array becomes GC garbage.
static void list_ensure_capacity(ValueList* list) {
    if (list->count < list->capacity) return;

    int new_capacity = list->capacity * 2;
    if (new_capacity < 8) new_capacity = 8;

    // Allocate new items array (GC-managed; old one becomes garbage)
    Value* new_items = (Value*)gc_allocate(new_capacity * sizeof(Value));
    for (int i = 0; i < list->count; i++) {
        new_items[i] = list->items[i];
    }
    list->items = new_items;
    list->capacity = new_capacity;
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
    if (!list) return val_null;
    if (index < 0) index += list->count;
    if (index >= 0 && index < list->count) {
        return list->items[index];
    }
    return val_null;
}

void list_set(Value list_val, int index, Value item) {
    ValueList* list = as_list(list_val);
    if (!list) return;
    if (list->frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    if (index < 0) index += list->count;
    if (index >= 0 && index < list->count) {
        list->items[index] = item;
    }
}

void list_push(Value list_val, Value item) {
    ValueList* list = as_list(list_val);
    if (!list) return;
    if (list->frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }

    list_ensure_capacity(list);
    list->items[list->count++] = item;
}

Value list_pop(Value list_val) {
    ValueList* list = as_list(list_val);
    if (!list || list->count <= 0) return val_null;
    if (list->frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return val_null; }

    return list->items[--list->count];
}

Value list_pull(Value list_val) {
    ValueList* list = as_list(list_val);
    if (!list || list->count <= 0) return val_null;
    if (list->frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return val_null; }

    Value result = list->items[0];
    for (int i = 0; i < list->count - 1; i++) {
        list->items[i] = list->items[i + 1];
    }
    list->count--;
    return result;
}

void list_insert(Value list_val, int index, Value item) {
    ValueList* list = as_list(list_val);
    if (!list) return;
    if (list->frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    if (index < 0) index += list->count + 1;
    if (index < 0) index = 0;
    if (index > list->count) index = list->count;

    list_ensure_capacity(list);

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
    if (list->frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return false; }
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
// Note: afterIdx semantics — search starts at afterIdx + 1
int list_indexOf(Value list_val, Value item, int afterIdx) {
    ValueList* list = as_list(list_val);
    if (!list) return -1;

    for (int i = afterIdx + 1; i < list->count; i++) {
        if (value_equal(list->items[i], item)) {
            return i;
        }
    }
    return -1;
}

bool list_contains(Value list_val, Value item) {
    return list_indexOf(list_val, item, -1) != -1;
}

// List utilities
void list_clear(Value list_val) {
    ValueList* list = as_list(list_val);
    if (!list) return;
    if (list->frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    list->count = 0;
}

Value list_slice(Value list_val, int start, int end) {
    GC_PUSH_SCOPE();

    Value result = val_null;
    GC_PROTECT(&list_val);
    GC_PROTECT(&result);

    ValueList* src = as_list(list_val);
    if (!src) {
        GC_POP_SCOPE();
        return make_list(0);
    }

    int len = src->count;
    if (start < 0) start += len;
    if (end < 0) end += len;
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) {
        result = make_list(0);
        GC_POP_SCOPE();
        return result;
    }

    int slice_len = end - start;
    result = make_list(slice_len);
    // Re-fetch src after allocation (GC may have moved it)
    src = as_list(list_val);
    ValueList* dst = as_list(result);
    for (int i = 0; i < slice_len; i++) {
        dst->items[i] = src->items[start + i];
    }
    dst->count = slice_len;

    GC_POP_SCOPE();
    return result;
}

Value list_concat(Value a, Value b) {
    GC_PUSH_SCOPE();

    Value result = val_null;
    GC_PROTECT(&a);
    GC_PROTECT(&b);
    GC_PROTECT(&result);

    ValueList* la = as_list(a);
    ValueList* lb = as_list(b);
    int lenA = la ? la->count : 0;
    int lenB = lb ? lb->count : 0;

    result = make_list(lenA + lenB);
    // Re-fetch after allocation (GC may have moved them)
    la = as_list(a);
    lb = as_list(b);
    ValueList* dst = as_list(result);
    for (int i = 0; i < lenA; i++) {
        dst->items[i] = la->items[i];
    }
    for (int i = 0; i < lenB; i++) {
        dst->items[lenA + i] = lb->items[i];
    }
    dst->count = lenA + lenB;

    GC_POP_SCOPE();
    return result;
}

Value list_copy(Value list_val) {
    ValueList* src = as_list(list_val);
    if (!src) return val_null;
    
    Value new_list = make_list(src->capacity);
    ValueList* dst = as_list(new_list);
    
    dst->count = src->count;
    for (int i = 0; i < src->count; i++) {
        dst->items[i] = src->items[i];
    }
    
    return new_list;
}


// Quicksort helper for list_sort
static int sort_ascending;

static void list_qsort(Value* items, int lo, int hi) {
    if (lo >= hi) return;
    Value pivot = items[(lo + hi) / 2];
    int i = lo, j = hi;
    while (i <= j) {
        while (value_compare(items[i], pivot) * sort_ascending < 0) i++;
        while (value_compare(items[j], pivot) * sort_ascending > 0) j--;
        if (i <= j) {
            Value tmp = items[i];
            items[i] = items[j];
            items[j] = tmp;
            i++; j--;
        }
    }
    if (lo < j) list_qsort(items, lo, j);
    if (i < hi) list_qsort(items, i, hi);
}

void list_sort(Value list_val, bool ascending) {
    ValueList* list = as_list(list_val);
    if (!list || list->count <= 1) return;
    if (list->frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    sort_ascending = ascending ? 1 : -1;
    list_qsort(list->items, 0, list->count - 1);
}

// Quicksort by key helper
static Value* sort_keys;

static void list_qsort_by_key(Value* items, int* indices, int lo, int hi) {
    if (lo >= hi) return;
    int pivotIdx = (lo + hi) / 2;
    Value pivotKey = sort_keys[indices[pivotIdx]];
    int i = lo, j = hi;
    while (i <= j) {
        while (value_compare(sort_keys[indices[i]], pivotKey) * sort_ascending < 0) i++;
        while (value_compare(sort_keys[indices[j]], pivotKey) * sort_ascending > 0) j--;
        if (i <= j) {
            int tmp = indices[i];
            indices[i] = indices[j];
            indices[j] = tmp;
            i++; j--;
        }
    }
    if (lo < j) list_qsort_by_key(items, indices, lo, j);
    if (i < hi) list_qsort_by_key(items, indices, i, hi);
}

void list_sort_by_key(Value list_val, Value byKey, bool ascending) {
    ValueList* list = as_list(list_val);
    if (!list || list->count <= 1) return;
    if (list->frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }

    int count = list->count;

    // Build keys array
    Value* keys = (Value*)malloc(count * sizeof(Value));
    for (int i = 0; i < count; i++) {
        Value elem = list->items[i];
        if (is_map(elem)) {
            keys[i] = map_get(elem, byKey);
        } else if (is_list(elem) && is_number(byKey)) {
            int ki = (int)numeric_val(byKey);
            keys[i] = list_get(elem, ki);
        } else {
            keys[i] = val_null;
        }
    }

    // Build index array
    int* indices = (int*)malloc(count * sizeof(int));
    for (int i = 0; i < count; i++) indices[i] = i;

    // Sort indices by keys
    sort_ascending = ascending ? 1 : -1;
    sort_keys = keys;
    list_qsort_by_key(list->items, indices, 0, count - 1);

    // Reorder items according to sorted indices
    Value* temp = (Value*)malloc(count * sizeof(Value));
    for (int i = 0; i < count; i++) {
        temp[i] = list->items[indices[i]];
    }
    for (int i = 0; i < count; i++) {
        list->items[i] = temp[i];
    }

    free(temp);
    free(indices);
    free(keys);
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

        // Get repr of item (quotes strings, may call recursively for nested lists)
        Value item_str = value_repr(list->items[i]);
        result = string_concat(result, item_str);
    }

    Value close = make_string("]");
    result = string_concat(result, close);

    return result;
}