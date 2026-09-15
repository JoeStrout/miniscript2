# Memory Systems in MiniScript2

This document describes the memory management systems used in MiniScript2 and how they interact.

## Overview

MiniScript2 uses **two distinct memory systems**, plus an intern table serving the first:

1. **Custom GC for Value objects** — typed object pools (`GCSet<T>`) coordinated by a single `GCManager`, identical in design on the C# and C++ sides because both are transpiled from the same source.
2. **String intern table** — content-addressed lookup into a dedicated `InternedStrings` `GCSet`, transparent to callers.
3. **Standard memory management for host code** — `std::shared_ptr` in C++ (`CS_String`, `CS_List`, `CS_Dictionary`), regular GC in C#.

For the rationale behind the custom GC, see [adr/0004-GC-system.md](adr/0004-GC-system.md).

## 1. Custom GC for Value objects

**Source:** `cs/GCManager.cs`, `cs/GCSet.cs`, `cs/GCItems.cs`, `cs/GCInterfaces.cs` — all transpiled, so the C++ side is `generated/GCManager.g.{h,cpp}` and friends rather than anything hand-written in `cpp/core/`.

A single `GCManager` owns seven typed object pools:

| Slot | GCSet             | Item type   | Notes |
|------|-------------------|-------------|-------|
| 0    | BigStrings        | `GCString`  | heap strings of 128 bytes or more |
| 1    | Lists             | `GCList`    | |
| 2    | Maps              | `GCMap`     | |
| 3    | Errors            | `GCError`   | |
| 4    | Functions         | `GCFuncRef` | |
| 5    | InternedStrings   | `GCString`  | shorter heap strings; semi-immortal, see §2 |
| 6    | Handles           | `GCHandle`  | native objects with a dispose hook on sweep |

Each `GCSet<T>` is a struct-of-arrays: the items themselves in one vector, and per-slot GC metadata (in-use, marked, retain count) in tight parallel arrays. Slots are recycled via a free-list stack; the high-water mark grows monotonically.

### Value encoding

A `Value` is a 64-bit NaN-boxed word. For GC-managed types, the lower 35 bits carry `(gcSet, itemIndex)`:

```
GC object:  0xFFFE_0000_000G_IIII_IIII
              bits 34-32 = GCSet index (0-6)
              bits 31-0  = item index within that GCSet
```

The same bit layout is used on both platforms, so the index in a Value's payload always means the same thing regardless of which side allocated it. Immediate doubles and tiny strings are encoded inline in the Value bits and do not touch any GCSet. A tiny string is up to 5 UTF-8 *bytes*, which is not the same as 5 characters — a 4-byte character such as an emoji is still a tiny string.

### Collection cycle

`GCManager.CollectGarbage()` runs a textbook mark/sweep:

1. **Prepare** — clear all mark bits across every GCSet.
2. **Mark roots** — walk the explicit root list (`AddRoot` / `RemoveRoot`).
3. **Mark retained** — walk each GCSet for slots with `retainCount > 0`.
4. **Mark via callbacks** — invoke each registered `MarkCallback`. The VM registers one of these in its constructor to mark its register stack, names, and intrinsics table.
5. **Sweep** — every slot in every GCSet that wasn't marked and has no positive retain count is freed (`OnSweep` runs first, then the slot returns to the free list).

`Mark(Value)` is branchless: `_sets[v.GCSetIndex()]->Mark(v.ItemIndex(), *this)`. No switch statement, no virtual dispatch beyond the per-set call.

### When does collection happen?

Collection never happens *behind* an allocation: `make_list` and friends never collect, so no local Value can be swept out from under an expression in progress. This is what removes the need to protect every local during a function body, and what let the old shadow-stack scaffolding go.

Instead, `GCManager.MaybeCollect(encouraged)` is called at well-defined boundary times and decides there whether to run a cycle. Two independent triggers, whichever fires first:

- **Allocation volume.** Slots in use have grown by `GCAllocGrowthFactor` (1.0, so roughly a doubling) times the number that survived the last collection, with a floor of `GCMinAllocsBeforeCollect` (100000 slots, about 10 MB of small objects) so a small heap is left alone.
- **Elapsed ticks.** A tick is one ordinary `MaybeCollect(false)` call; the caller defines what a tick means, and a host calling once per frame makes `GCIntervalTicks` (60) read as "about once a second". Live handles scale that interval down toward `GCMinIntervalTicks`, since a handle's cost is its finalizer, not its footprint. An `encouraged` call — a moment the caller knows we were about to idle anyway — uses a shorter interval.

`CollectGarbage()` forces a cycle regardless, and resets the tick clock. All the tuning constants are public fields, so a host can retune them without a rebuild.

Note which of the two is actually doing the work today. The only `MaybeCollect` call sites are the `wait` and `yield` intrinsics, and both pass `encouraged: true` — and only an *ordinary* call advances the tick counter. So in the command-line host the tick trigger never fires at all, and collection is driven entirely by allocation volume (plus an explicit `gc.collect`). The tick machinery is there for a host that calls `MaybeCollect(false)` on a regular beat, one frame at a time, the way Mini Micro will.

### Long-lived values

Locals and ephemeral expression results don't need any protection. For values that need to outlive their natural reachability — typically REPL globals, captured closure variables, or values stashed in host-side data structures — there are two options:

- `AddRoot(v)` / `RemoveRoot(v)` — explicit root-set membership.
- `RetainValue(v)` / `ReleaseValue(v)` — refcount-style; a slot with `retainCount > 0` is unconditionally marked during collection. (C#: `gc.Retain(idx)` on the specific GCSet.)

### Mark callbacks

`GCManager.RegisterMarkCallback(fn, userData)` lets a system inject roots without owning them in the explicit root list. The signature is `void(object userData)` in C#, `void(void*)` in C++. The VM registers one in its constructor, to mark its register stack, names array, and intrinsics table; `Intrinsic.MarkRoots` registers another, for intrinsic parameter defaults and registered short names.

There is no `GC_PROTECT`, and no shadow stack. Both are gone from the tree entirely — along with the `cpp/core/gc.h` shim that once provided them as no-ops — so there is nothing left to delete from call sites.

## 2. String intern table

**Location:** `cs/GCManager.cs`, built into `GCManager` and so shared by both platforms.

Interned strings live in their own GCSet (slot 5, `InternedStrings`), separate from the `BigStrings` set that holds everything longer. Two heap strings with identical content end up at the same slot, so map-key equality and hashing for interned strings collapse to a bit-comparison of the Value. `_internTable` is a `Dictionary<String, Int32>` mapping content to that slot.

### Routing rules

`make_string(s)` dispatches on length:

| Length     | Routing                                                            |
|------------|--------------------------------------------------------------------|
| ≤ 5 bytes  | Inline tiny string in the Value bits; no GCSet slot at all.        |
| under 128  | Hash-lookup `_internTable`; reuse the existing `InternedStrings` slot, or allocate one and record it. |
| 128 and up | Fresh `BigStrings` slot; skips the intern table entirely.          |

(The tiny-string cutoff is in UTF-8 bytes on both sides. The intern cutoff is in bytes on C++ but UTF-16 code units on C#, so a heavily non-ASCII string near the boundary can land in a different set on the two platforms. Nothing observable depends on which set a string is in.)

### Lifetime

Interned strings are **semi-immortal**, per [adr/0005-string-interning.md](adr/0005-string-interning.md). An ordinary collection does not mark or sweep the `InternedStrings` set at all — it is skipped in both `PrepareForGC` and the sweep — so a short string keeps its canonical identity across normal cycles even when nothing references it for a while.

Only `FullCollectGarbage()` reclaims them. It sets `_fullCollection`, which turns marking and sweeping back on for that set, and prunes the dead entries from `_internTable` *before* the sweep clears their `Data` fields. That is the path for a `reset`, memory pressure, or VM teardown — the escape valve that keeps a program generating endless unique short strings from growing without bound.

## 3. Shared `StringStorage` between Value strings and host strings

**Location:** `cpp/core/StringStorage.{h,c}`

`StringStorage` is the canonical heap-string struct used by both runtime Values and host code. It carries `lenB` (byte length), `lenC` (cached UTF-8 character count), `hash` (cached, computed lazily), and a flexible array member for the bytes themselves. The `ss_*` API operates on `StringStorage*` and provides substring, concat, indexOf, replace, case conversion, hashing, and so on.

Both layers reach the same code paths:

- **Value strings** (heap): `GCString` owns a `StringStorage*`; `value_string.cpp` operations call `ss_concat`, `ss_substringLen`, `ss_replace`, `ss_toUpper`, `ss_compare`, etc., feeding tiny strings into a small `TempStorage` RAII helper that materialises them as a temporary `StringStorage` when needed.
- **Host strings**: `CS_String` wraps `StringStorage*` in a `std::shared_ptr<StringStorage>` and uses the same `ss_*` family.

Because both layers share the storage struct *and* the operations, semantics like substring boundary handling, hash computation, and case-conversion behaviour are guaranteed identical across the two layers — there's no second implementation to drift.

## 4. Host C++ memory management (`std::shared_ptr`)

**Location:** `cpp/core/CS_String.h`, `cpp/core/CS_List.h`, `cpp/core/CS_Dictionary.h`

Transpiled C# code (compiler, assembler, debugger, host app) uses standard reference-counted memory:

- `String` — `std::shared_ptr<StringStorage>`
- `List<T>` — `std::shared_ptr<std::vector<T>>`
- `Dictionary<K, V>` — `std::shared_ptr` over internal storage

This layer is completely separate from the Value/GC system. Host strings are never seen by the GC; conversion to/from MiniScript strings always copies. See `CS_value_util.h` for the conversion helpers.

**Watch for reference cycles.** Unlike C#'s real GC, `std::shared_ptr` leaks under cycles. `CS_String` can't form a cycle on its own, but `CS_List` and `CS_Dictionary` can. C# code must either avoid creating such cycles or explicitly break them at clean-up time.

## VarMap overlay

**Source:** `cs/VarMap.cs`, transpiled to `generated/VarMap.g.{h,cpp}`.

`VarMapBacking` is a per-map overlay attached to a `GCMap` as `_vmb`. When present, string-keyed `Get`/`Set`/`Remove` route through the VM's register window first, falling back to the regular hash table on misses. Iteration walks register entries (encoded with negative iterator values) before dense hash entries.

This is how closures and REPL globals stay live across compilation cycles: the register array is the canonical store, the map is the view, and `varmap_gather` / `varmap_rebind` move values into the hash table or rebind the overlay to a relocated register array.

The C# version stores `List<Value>` references; the C++ side stores raw `Value*` pointers (matching the existing C++ VM ABI) and relies on `varmap_rebind` to update those pointers when the VM's stack is reallocated.

## String types summary

### Tiny strings (≤ 5 bytes)
- **Storage:** Inline in NaN-boxed Value (no allocation)
- **Examples:** `"a"`, `"x"`, `"__isa"`, `"self"`
- **Lifetime:** Lives as long as the Value exists
- **System:** None (embedded in Value itself)

### Interned heap strings (under 128)
- **Storage:** `GCManager.InternedStrings` slot, recorded in `_internTable`
- **Examples:** identifiers, short literals, common keys
- **Lifetime:** semi-immortal — skipped by ordinary collections; reclaimed only by `FullCollectGarbage`, which prunes the intern table first
- **System:** GC (full cycles only) + intern table

### Non-interned heap strings (128 and up)
- **Storage:** `GCManager.BigStrings` slot, no intern entry
- **Examples:** Long string literals, concatenation results
- **Lifetime:** GC-managed (collected when unreachable)
- **System:** GC

### Host strings (C# `String` class)
- **Storage:** `StringStorage` managed by `std::shared_ptr` (C++) or normal C# GC
- **Examples:** Function names, labels, compiler strings, debug output
- **Lifetime:** Reference-counted (C++) or GC-managed (C#)
- **System:** `std::shared_ptr` (C++) / managed runtime (C#)

## Memory system interactions

### Clear separation
- The Value GC and host memory systems are independent.
- Converting between them requires explicit string copying (see `CS_value_util.h`).
- Host strings are never seen by the Value GC.
- Values don't use `shared_ptr`.

### Same storage, different ownership
Both layers reach the same `StringStorage` struct and `ss_*` operations. The difference is who owns the lifetime: the GC manager (for Values) or a `std::shared_ptr` (for host strings).

## Debugging memory

### Value GC objects
From script, `gc.stats` returns a map of live slot counts per set plus a total:

```
{"bigStrings": 0, "internedStrings": 24, "lists": 2, "maps": 3,
 "errors": 2, "functions": 130, "handles": 0, "total": 161}
```

and `gc.collect` forces a cycle. From C#, the seven named members `BigStrings`, `InternedStrings`, `Lists`, `Maps`, `Errors`, `Functions` and `Handles` are public, each with an O(1) `LiveCount()`, for ad-hoc inspection.

### Host memory
Standard C++ tools:
- Valgrind for leak detection
- AddressSanitizer for memory errors
- Debugger watches on `shared_ptr` reference counts

## Design rationale (summary)

### Why two systems?
- **Same behaviour on both platforms.** A custom GC for Values is the only way to make C# and C++ behave identically — C# can't NaN-box a managed reference, so a managed-only solution doesn't fit, and a C++-only refcount system doesn't fit C#.
- **No per-local protection.** Collection runs only at explicit boundaries, so locals don't need shadow-stack scaffolding or `GC_PROTECT` macros.
- **Predictable cleanup for "handle" types.** The `GCHandle` GCSet gives native objects a deterministic dispose hook on sweep — important for things like file handles. Live handle count is also what pulls the tick-based collection interval earlier, since a handle's cost is its finalizer rather than its footprint.

### Why `std::shared_ptr` for host code?
- Standard C++ pattern: well understood, well tooled, well tested.
- Mirrors C# reference semantics — clean target for the transpiler.
- No custom pool allocators to maintain.
- Works seamlessly with sanitizers, leak detectors, and debuggers.

The runtime is decidedly *not* `std::shared_ptr`-based because shared_ptr can't NaN-box, and would either require a separate heap-allocated wrapper (slow) or a refcount on every Value copy (slower).
