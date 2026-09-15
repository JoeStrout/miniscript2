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

- **Phase 2 (`Items` entries, iter >= 0):** Non-negative iterator values are indexes into the map's own `_order` array (see below) — a slot, not an ordinal over live entries, so a tombstoned slot is skipped past rather than renumbering everything after it. The transition from phase 1 to phase 2 happens when all register mappings are exhausted; the iterator resets to -1 and then advances to the first live `_order` slot.

For non-VarMap maps, phase 1 is skipped entirely; the iterator goes from -1 directly to 0.

## Where the order lives

`GCMap` owns the iteration order, in `_order`: a `List<Value>` of keys in
insertion order, maintained by `Set` and `Remove`.  It is not derived from the
hash table, and that is the whole point.

Neither host language's dictionary can supply it portably.  Both are dense
entry arrays that append on insert, so both give insertion order *until
something is removed* -- and then they diverge.  C#'s `Dictionary` puts the
freed entry on a free list and reuses it for the next add; the transpiled
`CS_Dictionary` marks a hole and never reuses it until a resize compacts.  So
`{a,b,c}` minus `b` plus `d` iterated as `a, d, c` on C# and `a, c, d` on C++,
against a language that promises insertion order on both
(see [LANGUAGE_CHANGES.md](LANGUAGE_CHANGES.md)).  Holding the order ourselves
settles that, and the C++ reading is the one that survived: a re-added key is a
new key and goes to the end.

It also makes iteration linear.  The iterator is an index straight into
`_order`, so `KeyAt` is an array read and `ValueAt` is an array read plus one
hash lookup.  Before, the iterator was a *logical ordinal* and both had to walk
the dictionary's enumeration from the start to find the i'th live entry --
twice per step, since `ITERGET` calls both, which made a full pass O(n^2).

### Removal and tombstones

`Remove` leaves `Value.Unassigned` in the key's slot rather than shifting the
rest down.  That poison payload is never produced by any `make_*`, so it cannot
collide with a real key -- `null` can be a key, so `null` would not do.
`NextEntry` skips tombstones, and `CompactOrder` drops them once they outnumber
the live entries, which keeps a churned map from growing without bound and
bounds the run of holes any single step walks.

Finding the slot to tombstone would be a scan, so `_pos` (`Dictionary<Value,
Int32>`, key to slot) is built on a map's **first** removal and kept in step
afterwards.  Most maps -- objects, classes, module maps -- are filled and then
only read, and never allocate it at all.  Without it, removing every key from a
map was quadratic; with it, that is linear again.

Because `GCMap` is a struct, anything that *assigns* one of these fields has to
write the struct back, or the assignment lands in the copy `GCMapSet.Get`
returned.  Only two things do: `SeedOrder`, from `GCMapSet.SetItems`, and
`BuildPos`, from `Remove` -- which is why removal goes through
`GCMapSet.Remove(idx, key)` rather than `Get(idx).Remove(key)`, on both
platforms.  Mutating the *contents* of `_order` or `_pos` is safe anywhere,
since both are reference types.

### Maps that share a host dictionary

`GCManager.NewMapFromDict` wraps a host-owned `Dictionary` without copying it,
so the host may still insert into it afterwards, where `Set` cannot see it and
`_order` would silently miss those keys.  `EnsureOrder`, called when iteration
enters the `Items` phase, notices that the live count no longer matches and
rebuilds the order from the dictionary's own enumeration -- which, for a
dictionary that was filled and never pruned, is still insertion order.  The
undetectable case is a host that removes one key and adds another between
iterations, leaving the count unchanged; that map iterates in a stale order.

## VarMap and globals

Those two phases are unchanged, and do not use `_order`:

- A **VarMap**-backed map (a call frame's locals, a closure context) walks its
  register entries first, in phase 1, with the negative encoding above.
- The **globals** map has no `Items` and no `_order`; `NextEntry` walks the
  slot table directly via `Globals.NextAssignedSlot`, which already skips
  unassigned slots in order.

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

`map_iter_next` and `map_iter_entry` are `Value::IterNext` / `Value::IterEntry`, which delegate to `GCMap.NextEntry` / `KeyAt` / `ValueAt`.  Those are transpiled from one source now, so the two platforms no longer have per-platform differences to hide -- which is exactly what the order divergence above came from.
