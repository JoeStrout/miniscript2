# Map Iteration in For Loops

## Overview

`for` loops use two opcodes to iterate over collections:

- **NEXT_rA_rB** — Advances the iterator (integer value) in R[A] to reference the next entry in collection R[B]. Skips the next instruction if there is no next entry (the next instruction is normally a JUMP to exit the loop).
- **ITERGET_rA_rB_rC** — Retrieves the entry at iterator position R[C] from collection R[B], storing the result in R[A]. For maps, the result is a `{"key": k, "value": v}` mini-map.

## Iterator Encoding

The iterator is a single integer, starting at -1 (not started). Its meaning depends on the collection type and, for maps, whether the map is a VarMap.

### Lists and Strings

The iterator is a simple sequential index. NEXT increments it by 1 and compares to the collection length. ITERGET indexes directly.

### Maps (Two-Phase Encoding)

Maps use a two-phase scheme to handle VarMaps, which have both register-backed entries and regular hash entries:

- **Phase 1 (VarMap registers, iter <= -1):** Negative iterator values encode register mapping indices. Register index `i` maps to iterator value `-(i + 2)`. So the first register entry is -2, the second is -3, etc. NEXT scans forward through register mappings, skipping unassigned slots (where `names[regIndex]` is null).

- **Phase 2 (base entries, iter >= 0):** Non-negative iterator values refer to base hash table entries (C++) or key-cache indexes (C#). The transition from phase 1 to phase 2 happens when all register mappings are exhausted; the iterator resets to -1 and then increments to 0 for the first base entry.

For non-VarMap maps, phase 1 is skipped entirely; the iterator goes from -1 directly to 0.

## C++ Implementation

**Files:** `cpp/core/value_map.h`, `cpp/core/value_map.c`

**Functions:** `map_iter_next(Value map_val, int iter)` and `map_iter_entry(Value map_val, int iter)`

In phase 2, the iterator is a **raw index into the `entries[]` array** of the open-addressing hash table. NEXT scans forward from `iter + 1` looking for the next slot where `entries[i].occupied == true`. ITERGET reads `entries[iter].key` and `entries[iter].value` directly.

This gives O(1) amortized cost per iteration step (O(capacity) total), with no auxiliary data structures.

### Mutation During Iteration

Changing the value of an existing key does not affect iteration (the entry stays in the same slot). Adding or removing keys may cause entries to move due to rehashing, leading to skipped or repeated entries. This is undefined behavior, consistent with MiniScript 1.

## C# Implementation

**Files:** `cs/Value.cs` (ValueHelpers static methods + ValueMap class), `cs/VarMap.cs`

**Functions:** `map_iter_next(Value map_val, int iter)` and `map_iter_entry(Value map_val, int iter)`

C#'s `Dictionary<K,V>` does not expose its internal entry array, so phase 2 uses a **key cache** — a `List<Value>` snapshot of `_items.Keys`, stored on the `ValueMap` instance.

- The key cache is **lazily populated** on the first iteration access via `GetKeyCache()`.
- It is **cleared (set to null)** on any mutation: `Set`, `Remove`, or `Clear`.
- In phase 2, the iterator is an index into this key cache. ITERGET looks up the key from the cache, then does an O(1) dictionary lookup for the value.

This gives O(1) per iteration step after the initial O(n) cache build.

### VarMap Phase 1 (C#)

VarMap register entries are iterated by enumerating through the `_regMap` dictionary to position N. This is O(N) per step, but register mappings are typically small (the number of local variables in a function), so this is not a concern in practice.

### Mutation During Iteration

Changing the value of an existing key does **not** clear the key cache (the dictionary key set is unchanged), so iteration is unaffected. This matches C++ behavior.

Setting, or removing keys clears the key cache. On the next NEXT call, the cache is rebuilt from the current dictionary state. If the set only changed the value of an existing key, then (relying on known C# behavior) the new `Keys` list will match the previous one, and iteration will be unaffected.  If we have added a new key, C#'s `Keys` method will return this at the end, and iteration will proceed normally.  If we have removed an existing key, then the key order is likely to change.  The iterator index may now refer to a different key, leading to skipped or repeated entries. This is the same undefined-but-continues behavior as C++ and MiniScript 1.

## Sentinel Value

Both platforms use `MAP_ITER_DONE` (C#: `Int32.MinValue`, C++: `INT32_MIN`) as the return value from `map_iter_next` when iteration is complete.

## VM Code (Both Platforms)

The VM opcode handlers are identical in structure:

```
case NEXT_rA_rB:
    iter = as_int(R[A])
    if is_list:   iter++; hasMore = (iter < count)
    if is_map:    iter = map_iter_next(container, iter); hasMore = (iter != MAP_ITER_DONE)
    if is_string: iter++; hasMore = (iter < length)
    R[A] = make_int(iter)
    if hasMore: skip next instruction

case ITERGET_rA_rB_rC:
    if is_list:   R[A] = list[iter]
    if is_map:    R[A] = map_iter_entry(container, iter)
    if is_string: R[A] = string[iter..iter+1]
```

The per-platform differences are hidden entirely inside `map_iter_next` and `map_iter_entry`.
