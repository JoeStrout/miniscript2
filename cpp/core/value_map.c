// Hash table implementation for MiniScript runtime maps (NaN-boxed Values).
//
// Uses chained hashing with a separate buckets[] array and an append-only
// entries[] array.  New entries are always appended at entries[count++],
// so iteration over entries[0..count-1] yields insertion order.  Each
// bucket holds the index of the first entry in its chain; entries are
// linked via their `next` field (-1 = end of chain, -2 = removed/hole).
//
// On resize, entries are copied in index order and holes are compacted,
// preserving insertion order.  On remove, the entry is unlinked from its
// bucket chain and marked as a hole (next = -2); no neighbor rehashing
// is needed (unlike open-addressing).
//
// Memory management is via the GC (gc_allocate for both arrays).
// See also: CS_Dictionary.h, which uses the same algorithm for the
// host-side C++ Dictionary template (with std::vector storage instead).

#include "value_map.h"
#include "value.h"
#include "value_string.h"
#include "gc.h"
#include "gc_debug_output.h"
#include "vm_error.h"
#include "hashing.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>  // HACK for testing

#include "layer_defs.h"
#if LAYER_2A_HIGHER
#error "value_map.c (Layer 2A) cannot depend on higher layers (3A, 4)"
#endif
#if LAYER_2A_BSIDE
#error "value_map.c (Layer 2A - runtime) cannot depend on B-side layers (2B, 3B)"
#endif

// Helper: initialize a freshly-allocated map's storage arrays.
// Called after both entries and buckets have been gc_allocate'd.
static void init_storage(ValueMap* map) {
    for (int i = 0; i < map->capacity; i++) {
        map->buckets[i] = -1;
        map->entries[i].key = val_null;
        map->entries[i].value = val_null;
        map->entries[i].hash = 0;
        map->entries[i].next = MAP_ENTRY_END;
    }
}

// Map creation and management
Value make_map(int initial_capacity) {
    GC_PUSH_SCOPE();

    if (initial_capacity <= 0) initial_capacity = 8; // Default capacity

    // Allocate the ValueMap structure
    ValueMap* map = (ValueMap*)gc_allocate(sizeof(ValueMap));
    map->count = 0;
    map->freeCount = 0;
    map->capacity = initial_capacity;
    map->varmap_data = NULL;
    map->frozen = false;

    // Create and protect the Value immediately so subsequent allocations don't collect it
    Value map_val = MAP_TAG | ((uintptr_t)map & 0xFFFFFFFFFFFFULL);
    GC_PROTECT(&map_val);

    // Allocate storage arrays (may trigger GC)
    map->buckets = (int*)gc_allocate(initial_capacity * sizeof(int));
    map->entries = (MapEntry*)gc_allocate(initial_capacity * sizeof(MapEntry));
    init_storage(map);

    GC_POP_SCOPE();
    return map_val;
}

Value make_empty_map(void) {
    return make_map(8);
}

// Map access
ValueMap* as_map(Value v) {
    if (!is_map(v)) return NULL;
    return (ValueMap*)(uintptr_t)(v & 0xFFFFFFFFFFFFULL);
}

int map_count(Value map_val) {
    ValueMap* map = as_map(map_val);
    if (!map) return 0;

    int active = map->count - map->freeCount;

    // VarMap - include assigned register count
    if (map->varmap_data != NULL) {
        VarMapData* vdata = map->varmap_data;
        for (int i = 0; i < vdata->reg_map_count; i++) {
            int reg_index = vdata->reg_map_indices[i];
            if (!is_null(vdata->names[reg_index])) {
                active++;
            }
        }
    }

    return active;
}

int map_capacity(Value map_val) {
    ValueMap* map = as_map(map_val);
    return map ? map->capacity : 0;
}

// Find entry by key using bucket chains.
// Returns the index of the matching entry, or -1 if not found.
static int find_entry(ValueMap* map, Value key, uint32_t hash) {
    if (!map || map->capacity == 0) return -1;

    int bucket = hash % map->capacity;
    for (int i = map->buckets[bucket]; i >= 0; i = map->entries[i].next) {
        if (map->entries[i].hash == hash && value_equal(map->entries[i].key, key)) {
            return i;
        }
    }
    return -1;
}

// Map operations
Value map_get(Value map_val, Value key) {
    ValueMap* map = as_map(map_val);
    if (!map) return val_null;

    // VarMap check - zero overhead for regular maps
    if (map->varmap_data != NULL) {
        VarMapData* vdata = map->varmap_data;
        for (int i = 0; i < vdata->reg_map_count; i++) {
            if (value_equal(vdata->reg_map_keys[i], key)) {
                int reg_index = vdata->reg_map_indices[i];
                if (!is_null(vdata->names[reg_index])) {
                    return vdata->registers[reg_index];
                }
                return val_null;
            }
        }
    }

    uint32_t hash = value_hash(key);
    int index = find_entry(map, key, hash);
    if (index >= 0) {
        return map->entries[index].value;
    }
    return val_null;
}

bool map_try_get(Value map_val, Value key, Value* out_value) {
    ValueMap* map = as_map(map_val);
    if (!map) {
        if (out_value) *out_value = val_null;
        return false;
    }

    // VarMap check
    if (map->varmap_data != NULL) {
        VarMapData* vdata = map->varmap_data;
        for (int i = 0; i < vdata->reg_map_count; i++) {
            if (value_equal(vdata->reg_map_keys[i], key)) {
                int reg_index = vdata->reg_map_indices[i];
                if (!is_null(vdata->names[reg_index])) {
                    if (out_value) *out_value = vdata->registers[reg_index];
                    return true;
                }
                if (out_value) *out_value = val_null;
                return false;
            }
        }
    }

    uint32_t hash = value_hash(key);
    int index = find_entry(map, key, hash);
    if (index >= 0) {
        if (out_value) *out_value = map->entries[index].value;
        return true;
    }

    if (out_value) *out_value = val_null;
    return false;
}

bool map_lookup(Value map_val, Value key, Value* out_value) {
    Value current = map_val;
    for (int depth = 0; depth < 256; depth++) {
        if (!is_map(current)) {
            if (out_value) *out_value = val_null;
            return false;
        }
        if (map_try_get(current, key, out_value)) {
            return true;
        }
        Value isa;
        if (!map_try_get(current, val_isa_key, &isa)) {
            if (out_value) *out_value = val_null;
            return false;
        }
        current = isa;
    }
    if (out_value) *out_value = val_null;
    return false;
}

// Look up a key in a map walking the __isa chain, and also return the __isa
// of the map where the key was found (for computing 'super').
// Returns true if found, false otherwise.
bool map_lookup_with_origin(Value map_val, Value key, Value* out_value, Value* out_super) {
    if (out_value) *out_value = val_null;
    if (out_super) *out_super = val_null;
    Value current = map_val;
    Value isa;
    for (int depth = 0; depth < 256; depth++) {
        if (!is_map(current)) return false;
        if (map_try_get(current, key, out_value)) {
            if (map_try_get(current, val_isa_key, &isa)) {
                if (out_super) *out_super = isa;
            }
            return true;
        }
        if (!map_try_get(current, val_isa_key, &isa)) return false;
        current = isa;
    }
    return false;
}

// Internal set that bypasses VarMap and frozen checks.
static bool base_map_set(Value map_val, Value key, Value value);

// Resize the map: double capacity, rehash in index order (preserves insertion order).
static void map_resize(Value map_val) {
    GC_PUSH_SCOPE();
    GC_PROTECT(&map_val);

    ValueMap* map = as_map(map_val);
    int old_count = map->count;
    int new_capacity = map->capacity * 2;
    if (new_capacity < 8) new_capacity = 8;

    MapEntry* old_entries = map->entries;

    // Allocate new arrays (may trigger GC, so protect map_val)
    int* new_buckets = (int*)gc_allocate(new_capacity * sizeof(int));
    map = as_map(map_val);  // re-fetch after potential GC
    MapEntry* new_entries = (MapEntry*)gc_allocate(new_capacity * sizeof(MapEntry));
    map = as_map(map_val);  // re-fetch after potential GC
    old_entries = map->entries;  // re-fetch in case map was relocated

    // Initialize new arrays
    for (int i = 0; i < new_capacity; i++) {
        new_buckets[i] = -1;
        new_entries[i].key = val_null;
        new_entries[i].value = val_null;
        new_entries[i].hash = 0;
        new_entries[i].next = MAP_ENTRY_END;
    }

    // Copy entries in index order, compacting holes
    int new_index = 0;
    for (int i = 0; i < old_count; i++) {
        if (old_entries[i].next == MAP_ENTRY_REMOVED) continue;
        int bucket = old_entries[i].hash % new_capacity;
        new_entries[new_index].key = old_entries[i].key;
        new_entries[new_index].value = old_entries[i].value;
        new_entries[new_index].hash = old_entries[i].hash;
        new_entries[new_index].next = new_buckets[bucket];
        new_buckets[bucket] = new_index;
        new_index++;
    }

    map->entries = new_entries;
    map->buckets = new_buckets;
    map->capacity = new_capacity;
    map->count = new_index;
    map->freeCount = 0;

    GC_POP_SCOPE();
}

static bool base_map_set(Value map_val, Value key, Value value) {
    GC_PUSH_SCOPE();

    ValueMap* map = as_map(map_val);
    if (!map) {
        GC_POP_SCOPE();
        return false;
    }

    GC_PROTECT(&map_val);
    GC_PROTECT(&key);
    GC_PROTECT(&value);

    uint32_t hash = value_hash(key);

    // Check if key already exists
    int index = find_entry(map, key, hash);
    if (index >= 0) {
        map->entries[index].value = value;
        GC_POP_SCOPE();
        return true;
    }

    // Need to add new entry - resize if high-water mark is at 3/4 capacity
    if (map->count * 4 >= map->capacity * 3) {
        map_resize(map_val);
        map = as_map(map_val);  // re-fetch after resize
    }

    // Append new entry at entries[count]
    int new_index = map->count;
    int bucket = hash % map->capacity;
    map->entries[new_index].key = key;
    map->entries[new_index].value = value;
    map->entries[new_index].hash = hash;
    map->entries[new_index].next = map->buckets[bucket];
    map->buckets[bucket] = new_index;
    map->count++;

    GC_POP_SCOPE();
    return true;
}

bool map_set(Value map_val, Value key, Value value) {
    ValueMap* map = as_map(map_val);
    if (!map) return false;
    if (map->frozen) { vm_raise_runtime_error("Attempt to modify a frozen map"); return false; }

    // VarMap check - handle register assignment
    if (map->varmap_data != NULL) {
        VarMapData* vdata = map->varmap_data;
        for (int i = 0; i < vdata->reg_map_count; i++) {
            if (value_equal(vdata->reg_map_keys[i], key)) {
                int reg_index = vdata->reg_map_indices[i];
                vdata->registers[reg_index] = value;
                vdata->names[reg_index] = key;
                return true;
            }
        }
    }

	return base_map_set(map_val, key, value);
}

bool map_remove(Value map_val, Value key) {
    ValueMap* map = as_map(map_val);
    if (!map) return false;
    if (map->frozen) { vm_raise_runtime_error("Attempt to modify a frozen map"); return false; }

    // VarMap check - handle register clearing
    if (map->varmap_data != NULL) {
        VarMapData* vdata = map->varmap_data;
        for (int i = 0; i < vdata->reg_map_count; i++) {
            if (value_equal(vdata->reg_map_keys[i], key)) {
                int reg_index = vdata->reg_map_indices[i];
                vdata->names[reg_index] = val_null;
                return true;
            }
        }
    }

    uint32_t hash = value_hash(key);
    int bucket = hash % map->capacity;

    // Walk the chain, tracking the previous entry
    int prev = -1;
    for (int i = map->buckets[bucket]; i >= 0; prev = i, i = map->entries[i].next) {
        if (map->entries[i].hash == hash && value_equal(map->entries[i].key, key)) {
            // Unlink from bucket chain
            if (prev < 0) {
                map->buckets[bucket] = map->entries[i].next;
            } else {
                map->entries[prev].next = map->entries[i].next;
            }
            // Mark as removed
            map->entries[i].next = MAP_ENTRY_REMOVED;
            map->entries[i].key = val_null;
            map->entries[i].value = val_null;
            map->entries[i].hash = 0;
            map->freeCount++;
            return true;
        }
    }
    return false;
}

bool map_has_key(Value map_val, Value key) {
    ValueMap* map = as_map(map_val);
    if (!map) return false;

    // VarMap check
    if (map->varmap_data != NULL) {
        VarMapData* vdata = map->varmap_data;
        for (int i = 0; i < vdata->reg_map_count; i++) {
            if (value_equal(vdata->reg_map_keys[i], key)) {
                int reg_index = vdata->reg_map_indices[i];
                return !is_null(vdata->names[reg_index]);
            }
        }
    }

    uint32_t hash = value_hash(key);
    return find_entry(map, key, hash) >= 0;
}

// Map utilities
void map_clear(Value map_val) {
    ValueMap* map = as_map(map_val);
    if (!map) return;
    if (map->frozen) { vm_raise_runtime_error("Attempt to modify a frozen map"); return; }

    for (int i = 0; i < map->capacity; i++) {
        map->buckets[i] = -1;
        map->entries[i].key = val_null;
        map->entries[i].value = val_null;
        map->entries[i].hash = 0;
        map->entries[i].next = MAP_ENTRY_END;
    }
    map->count = 0;
    map->freeCount = 0;
}

Value map_copy(Value map_val) {
    ValueMap* src_map = as_map(map_val);
    if (!src_map) return make_empty_map();

    Value new_map = make_map(src_map->capacity);

    // Iterate in index order to preserve insertion order
    for (int i = 0; i < src_map->count; i++) {
        if (src_map->entries[i].next != MAP_ENTRY_REMOVED) {
            map_set(new_map, src_map->entries[i].key, src_map->entries[i].value);
        }
    }

    return new_map;
}

Value map_concat(Value a, Value b) {
    GC_PUSH_SCOPE();
    GC_PROTECT(&a);
    GC_PROTECT(&b);

    int countA = map_count(a);
    int countB = map_count(b);

    Value result = make_map(countA + countB > 0 ? countA + countB : 4);
    GC_PROTECT(&result);

    // Copy entries from a in insertion order
    ValueMap* mapA = as_map(a);
    if (mapA) {
        for (int i = 0; i < mapA->count; i++) {
            if (mapA->entries[i].next != MAP_ENTRY_REMOVED) {
                map_set(result, mapA->entries[i].key, mapA->entries[i].value);
            }
        }
    }

    // Copy entries from b (overriding any matching keys from a)
    ValueMap* mapB = as_map(b);
    if (mapB) {
        for (int i = 0; i < mapB->count; i++) {
            if (mapB->entries[i].next != MAP_ENTRY_REMOVED) {
                map_set(result, mapB->entries[i].key, mapB->entries[i].value);
            }
        }
    }

    GC_POP_SCOPE();
    return result;
}

// Return the Nth key-value pair from a map as a {"key":k, "value":v} mini-map.
Value map_nth_entry(Value map_val, int n) {
    ValueMap* map = as_map(map_val);
    if (!map) return val_null;

    GC_PUSH_SCOPE();
    GC_PROTECT(&map_val);

    // Walk entries in index order, skipping holes, to find the Nth active entry
    int active = 0;
    Value key = val_null;
    Value val = val_null;
    for (int i = 0; i < map->count; i++) {
        if (map->entries[i].next != MAP_ENTRY_REMOVED) {
            if (active == n) {
                key = map->entries[i].key;
                val = map->entries[i].value;
                break;
            }
            active++;
        }
    }

    GC_PROTECT(&key);
    GC_PROTECT(&val);
    Value result = make_map(4);
    map_set(result, make_string("key"), key);
    map_set(result, make_string("value"), val);

    GC_POP_SCOPE();
    return result;
}

int map_iter_next(Value map_val, int iter) {
    ValueMap* map = as_map(map_val);
    if (!map) return MAP_ITER_DONE;

    // Phase 1: VarMap register entries (iter <= -1)
    if (map->varmap_data != NULL && iter <= -1) {
        VarMapData* vdata = map->varmap_data;
        int startIdx = (iter == -1) ? 0 : -(iter) - 2 + 1;
        for (int i = startIdx; i < vdata->reg_map_count; i++) {
            int reg_index = vdata->reg_map_indices[i];
            if (!is_null(vdata->names[reg_index])) {
                return -(i + 2);
            }
        }
        // Exhausted register entries; reset for phase 2
        iter = -1;
    }

    // Phase 2: Regular entries in index order, skipping holes
    int start = iter + 1;
    for (int i = start; i < map->count; i++) {
        if (map->entries[i].next != MAP_ENTRY_REMOVED) {
            return i;
        }
    }
    return MAP_ITER_DONE;
}

Value map_iter_entry(Value map_val, int iter) {
    ValueMap* map = as_map(map_val);
    if (!map) return val_null;

    Value key, val;

    if (iter < -1 && map->varmap_data != NULL) {
        // VarMap register entry
        int regMapIdx = -(iter) - 2;
        VarMapData* vdata = map->varmap_data;
        int reg_index = vdata->reg_map_indices[regMapIdx];
        key = vdata->reg_map_keys[regMapIdx];
        val = vdata->registers[reg_index];
    } else if (iter >= 0 && iter < map->count && map->entries[iter].next != MAP_ENTRY_REMOVED) {
        // Regular entry
        key = map->entries[iter].key;
        val = map->entries[iter].value;
    } else {
        return val_null;
    }

    // Build {"key": k, "value": v} mini-map
    GC_PUSH_SCOPE();
    GC_PROTECT(&key);
    GC_PROTECT(&val);
    Value result = make_map(4);
    map_set(result, make_string("key"), key);
    map_set(result, make_string("value"), val);
    GC_POP_SCOPE();
    return result;
}

bool map_needs_expansion(Value map_val) {
    ValueMap* map = as_map(map_val);
    if (!map) return false;
    // Resize when high-water mark exceeds 3/4 of capacity
    return map->count * 4 >= map->capacity * 3;
}

// Expand map capacity in-place (preserves the Value/MemRef)
bool map_expand_capacity(Value map_val) {
    map_resize(map_val);
    return true;
}

// Deprecated: Creates a new map instead of expanding in-place
Value map_with_expanded_capacity(Value map_val) {
    ValueMap* old_map = as_map(map_val);
    if (!old_map) return map_val;

    int new_capacity = old_map->capacity * 2;
    Value new_map = make_map(new_capacity);

    for (int i = 0; i < old_map->count; i++) {
        if (old_map->entries[i].next != MAP_ENTRY_REMOVED) {
            map_set(new_map, old_map->entries[i].key, old_map->entries[i].value);
        }
    }

    return new_map;
}

// Map iteration
MapIterator map_iterator(Value map_val) {
    MapIterator iter;
    iter.map = as_map(map_val);
    iter.index = -1;
    iter.varmap_reg_index = -1;
    return iter;
}

bool map_iterator_next(MapIterator* iter, Value* out_key, Value* out_value) {
    if (!iter || !iter->map) return false;

    // For VarMaps, first iterate through register mappings
    if (iter->map->varmap_data != NULL && iter->varmap_reg_index < iter->map->varmap_data->reg_map_count) {
        VarMapData* vdata = iter->map->varmap_data;

        iter->varmap_reg_index++;
        while (iter->varmap_reg_index < vdata->reg_map_count) {
            int reg_index = vdata->reg_map_indices[iter->varmap_reg_index];
            if (!is_null(vdata->names[reg_index])) {
                if (out_key) *out_key = vdata->reg_map_keys[iter->varmap_reg_index];
                if (out_value) *out_value = vdata->registers[reg_index];
                return true;
            }
            iter->varmap_reg_index++;
        }

        iter->index = -1;
    }

    // Walk entries in index order, skipping holes
    iter->index++;
    while (iter->index < iter->map->count) {
        if (iter->map->entries[iter->index].next != MAP_ENTRY_REMOVED) {
            if (out_key) *out_key = iter->map->entries[iter->index].key;
            if (out_value) *out_value = iter->map->entries[iter->index].value;
            return true;
        }
        iter->index++;
    }

    return false;
}

// Hash function for maps
uint32_t map_hash(Value map_val) {
    ValueMap* map = as_map(map_val);
    if (!map) return 0;

    const uint32_t FNV_PRIME = 0x01000193;
    uint32_t hash = 0x811c9dc5;

    for (int i = 0; i < map->count; i++) {
        if (map->entries[i].next != MAP_ENTRY_REMOVED) {
            uint32_t key_hash = value_hash(map->entries[i].key);
            uint32_t value_hash_val = value_hash(map->entries[i].value);
            uint32_t pair_hash = key_hash ^ value_hash_val;
            hash ^= pair_hash;
            hash *= FNV_PRIME;
        }
    }

    return hash == 0 ? 1 : hash;
}

// Convert map to string representation for runtime (returns GC-managed Value)
Value map_to_string(Value map_val) {
    ValueMap* map = as_map(map_val);
    if (!map) return make_string("{}");

    if (map->count - map->freeCount == 0 && map->varmap_data == NULL) return make_string("{}");

    Value result = make_string("{");

    MapIterator iter = map_iterator(map_val);
    Value key, value;
    bool first = true;

	// ToDo: instead of repeatedly calling string_concat, gather all
	// the items in a list, and use join.
    while (map_iterator_next(&iter, &key, &value)) {
        if (!first) {
            Value comma = make_string(", ");
            result = string_concat(result, comma);
        }
        first = false;

        Value key_str = value_repr(key);
        Value value_str = value_repr(value);

        result = string_concat(result, key_str);
        Value colon = make_string(": ");
        result = string_concat(result, colon);
        result = string_concat(result, value_str);
    }

    Value close = make_string("}");
    result = string_concat(result, close);

    return result;
}

// VarMap creation and management
Value make_varmap(Value* registers, Value* names, int firstIndex, int count) {
    // Create regular map structure
    ValueMap* map = (ValueMap*)gc_allocate(sizeof(ValueMap));
    Value result = MAP_TAG | ((uintptr_t)map & 0xFFFFFFFFFFFFULL);
    map->count = 0;
    map->freeCount = 0;
    map->capacity = 8;
    map->frozen = false;
    map->buckets = (int*)gc_allocate(8 * sizeof(int));
    map->entries = (MapEntry*)gc_allocate(8 * sizeof(MapEntry));
    init_storage(map);

    // Allocate and initialize VarMapData
    VarMapData* vdata = (VarMapData*)gc_allocate(sizeof(VarMapData));
    vdata->registers = registers;
    vdata->names = names;
    vdata->reg_map_keys = (Value*)gc_allocate(5 * sizeof(Value));
    vdata->reg_map_indices = (int*)gc_allocate(5 * sizeof(int));
    vdata->reg_map_count = 0;
    vdata->reg_map_capacity = 5;

    map->varmap_data = vdata;

    for (int i = firstIndex; i < firstIndex + count; i++) {
    	if (!is_null(names[i])) varmap_map_to_register(result, names[i], registers, i);
    }

    return result;
}

// Helper function to add register mapping.
// If var_name already exists as a plain map entry, hydrate the register
// from that entry and remove the plain entry (the register mapping shadows it).
void varmap_map_to_register(Value map_val, Value var_name, Value* registers, int reg_index) {
    ValueMap* map = as_map(map_val);
    if (!map || !map->varmap_data) return;

    VarMapData* vdata = map->varmap_data;
    if (vdata->reg_map_count < vdata->reg_map_capacity) {
        vdata->reg_map_keys[vdata->reg_map_count] = var_name;
        vdata->reg_map_indices[vdata->reg_map_count] = reg_index;
        vdata->reg_map_count++;
    }
    // ToDo: else grow our capacity!

    // If there's a plain map entry for this name, hydrate the register from it
    uint32_t hash = value_hash(var_name);
    int entry_index = find_entry(map, var_name, hash);
    if (entry_index >= 0) {
        registers[reg_index] = map->entries[entry_index].value;
        // Remove the plain entry (now shadowed by register mapping)
        int bucket = hash % map->capacity;
        int prev = -1;
        for (int i = map->buckets[bucket]; i >= 0; prev = i, i = map->entries[i].next) {
            if (i == entry_index) {
                if (prev < 0) {
                    map->buckets[bucket] = map->entries[i].next;
                } else {
                    map->entries[prev].next = map->entries[i].next;
                }
                map->entries[i].next = MAP_ENTRY_REMOVED;
                map->entries[i].key = val_null;
                map->entries[i].value = val_null;
                map->entries[i].hash = 0;
                map->freeCount++;
                break;
            }
        }
    }
}

static void map_debug_dump(Value map_val) {
    ValueMap* map = as_map(map_val);
	if (!map) {
		printf("Not a map: ");
		debug_print_value(map_val);
		printf("\n");
		return;
	}
	printf("Map "); debug_print_value(map_val); printf(" has %d entries\n", map->count - map->freeCount);
	VarMapData* vdata = map->varmap_data;
	if (vdata) {
		printf("...and varmap data with %d registers\n", vdata->reg_map_count);
	}

}

// Gather all assigned register variables into regular map storage
void varmap_gather(Value map_val) {
    ValueMap* map = as_map(map_val);
    if (!map || !map->varmap_data) return;

    VarMapData* vdata = map->varmap_data;

    // Copy all assigned register variables to regular map storage
    for (int i = 0; i < vdata->reg_map_count; i++) {
        Value var_name = vdata->reg_map_keys[i];
        int reg_index = vdata->reg_map_indices[i];

        if (!is_null(vdata->names[reg_index])) {
            Value value = vdata->registers[reg_index];
            base_map_set(map_val, var_name, value);
        }
    }

    // Clear all register mappings
    vdata->reg_map_count = 0;
}

// Rebind a VarMap to new register and names arrays.
// Gathers all register-mapped values into plain map entries first,
// then updates the register/names pointers for future operations.
void varmap_rebind(Value map_val, Value* registers, Value* names) {
    ValueMap* map = as_map(map_val);
    if (!map || !map->varmap_data) return;

    // Gather current register values into plain map entries
    varmap_gather(map_val);

    // Update pointers to new arrays
    VarMapData* vdata = map->varmap_data;
    vdata->registers = registers;
    vdata->names = names;
}
