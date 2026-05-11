# Memory Systems in MiniScript2

This document describes the memory management systems used in MiniScript2 and how they interact.

## Overview

MiniScript2 uses **two distinct memory systems**, plus an intern side-table that piggybacks on the first:

1. **Custom GC for Value objects** — typed object pools (`GCSet<T>`) coordinated by a single `GCManager`, identical in design on the C# and C++ sides.
2. **String intern table** — a side-table on the strings `GCSet`, transparent to callers.
3. **Standard memory management for host code** — `std::shared_ptr` in C++ (`CS_String`, `CS_List`, `CS_Dictionary`), regular GC in C#.

For the rationale behind the custom GC, see [adr/0004-GC-system.md](adr/0004-GC-system.md).

## 1. Custom GC for Value objects

**C# source:** `cs/GCManager.cs`, `cs/GCSet.cs`, `cs/GCItems.cs`, `cs/IGCSet.cs`, `cs/IGCItem.cs`
**C++ source:** `cpp/core/GCManager.{h,cpp}`, `cpp/core/GCSet.h`, `cpp/core/GCItems.{h,cpp}`, `cpp/core/IGCSet.h`

A single `GCManager` owns five typed object pools:

| Slot | GCSet              | C# type     | C++ type    |
|------|--------------------|-------------|-------------|
| 0    | Strings            | `GCString`  | `GCString`  |
| 1    | Lists              | `GCList`    | `GCList`    |
| 2    | Maps               | `GCMap`     | `GCMap`     |
| 3    | Errors             | `GCError`   | `GCError`   |
| 4    | FuncRefs           | `GCFuncRef` | `GCFuncRef` |
| 5    | *(reserved for future `GCHandle`)* | — | — |

Each `GCSet<T>` is a struct-of-arrays: the items themselves in one vector, and per-slot GC metadata (in-use, marked, retain count) in tight parallel arrays. Slots are recycled via a free-list stack; the high-water mark grows monotonically.

### Value encoding

A `Value` is a 64-bit NaN-boxed word. For GC-managed types, the lower 35 bits carry `(gcSet, itemIndex)`:

```
GC object:  0xFFFE_0000_000G_IIII_IIII
              bits 34-32 = GCSet index (0-5)
              bits 31-0  = item index within that GCSet
```

The same bit layout is used on both platforms, so the index in a Value's payload always means the same thing regardless of which side allocated it. Tiny strings (≤5 ASCII bytes) and immediate doubles are encoded inline in the Value bits and do not touch any GCSet.

### Collection cycle

`GCManager.CollectGarbage()` runs a textbook mark/sweep:

1. **Prepare** — clear all mark bits across every GCSet.
2. **Mark roots** — walk the explicit root list (`AddRoot` / `RemoveRoot`).
3. **Mark retained** — walk each GCSet for slots with `retainCount > 0`.
4. **Mark via callbacks** — invoke each registered `MarkCallback`. The VM registers one of these in its constructor to mark its register stack, names, and intrinsics table.
5. **Sweep** — every slot in every GCSet that wasn't marked and has no positive retain count is freed (`OnSweep` runs first, then the slot returns to the free list).

`Mark(Value)` is branchless: `_sets[v.GCSetIndex()]->Mark(v.ItemIndex(), *this)`. No switch statement, no virtual dispatch beyond the per-set call.

### When does collection happen?

Collection is **never triggered by allocation**. The new GC runs only when explicitly requested — currently at well-defined boundary times like `yield` and `wait` in the interpreter, or via an intrinsic. This removes the need to protect every local Value during a function body and eliminates the old shadow-stack scaffolding. Code that touches GC objects never has to worry about the value being collected mid-expression.

### Long-lived values

Locals and ephemeral expression results don't need any protection. For values that need to outlive their natural reachability — typically REPL globals, captured closure variables, or values stashed in host-side data structures — there are two options:

- `AddRoot(v)` / `RemoveRoot(v)` — explicit root-set membership.
- `RetainValue(v)` / `ReleaseValue(v)` — refcount-style; a slot with `retainCount > 0` is unconditionally marked during collection. (C#: `gc.Retain(idx)` on the specific GCSet.)

### Mark callbacks

`GCManager.RegisterMarkCallback(fn, userData)` lets a system inject roots without owning them in the explicit root list. The signature is `void(void*, GCManager&)`. The VM uses this to mark its register stack, names array, and intrinsics map on every collection cycle.

A legacy C-compatible shim in `cpp/core/gc.h` provides `gc_register_mark_callback` / `gc_mark_value` / `gc_collect` / etc. that forward to the new API, so older call sites continue to work unmodified. The shim also provides no-op `GC_PROTECT`, `GC_LOCALS_n`, and `GC_PUSH_SCOPE` / `GC_POP_SCOPE` macros for now — they're remnants of the old shadow-stack system and can be deleted from call sites at any time.

## 2. String intern table

**Location:** `cpp/core/GCManager.{h,cpp}` (C++), built into `GCManager` on both platforms.

Interning is a side-table on the strings GCSet rather than a separate system. Two heap strings with identical content end up at the same `GCSet` index, so map-key equality and hashing for interned strings collapse to bit-comparison of the Value.

### Routing rules

`make_string(s)` (or `GCManager.NewString(data, len)`) dispatches based on length:

| Length     | Routing                                                            |
|------------|--------------------------------------------------------------------|
| ≤ 5 bytes  | Inline tiny string in Value bits; no GCSet slot.                   |
| 6 – 127    | Hash-lookup the intern table; reuse existing slot or allocate a new one with `Interned = true`. |
| ≥ 128      | Fresh GCSet slot; `Interned = false`; skips the intern table.      |

### Lifetime

Interned slots are **not immortal** — they're swept like any other slot when nothing references them. `GCString::Interned` tells `GCManager` whether to also remove the slot from the intern side-table during sweep. Truly immortal strings (opcode names, lexer keywords, intrinsic names) should call `Retain` on their slot once at startup; this keeps them alive without polluting the root list.

The intern table is keyed on `StringRef{const char*, int}` views that point into each `GCString`'s `StringStorage->data` buffer. The buffers are `malloc`-allocated and stable across `std::vector` resizes (vector reallocations move the `GCString` struct, not the `StringStorage` it owns).

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

**C# source:** `cs/VarMap.cs`
**C++ source:** `cpp/core/VarMapBacking.{h,cpp}`

`VarMapBacking` is a per-map overlay attached to a `GCMap` as `_vmb`. When present, string-keyed `Get`/`Set`/`Remove` route through the VM's register window first, falling back to the regular hash table on misses. Iteration walks register entries (encoded with negative iterator values) before dense hash entries.

This is how closures and REPL globals stay live across compilation cycles: the register array is the canonical store, the map is the view, and `varmap_gather` / `varmap_rebind` move values into the hash table or rebind the overlay to a relocated register array.

The C# version stores `List<Value>` references; the C++ side stores raw `Value*` pointers (matching the existing C++ VM ABI) and relies on `varmap_rebind` to update those pointers when the VM's stack is reallocated.

## String types summary

### Tiny strings (≤ 5 bytes)
- **Storage:** Inline in NaN-boxed Value (no allocation)
- **Examples:** `"a"`, `"x"`, `"__isa"`, `"self"`
- **Lifetime:** Lives as long as the Value exists
- **System:** None (embedded in Value itself)

### Interned heap strings (6 – 127 bytes)
- **Storage:** `StringStorage` in `GCManager.Strings` slot, registered in intern side-table
- **Examples:** identifiers, short literals, common keys
- **Lifetime:** GC-managed; swept when unreachable (slot is removed from intern table on sweep)
- **System:** GC + intern side-table

### Non-interned heap strings (≥ 128 bytes)
- **Storage:** `StringStorage` in `GCManager.Strings` slot, no intern entry
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
`GCManager.PrintStats()` reports the live slot count for each GCSet. The four (soon five) named members `Strings`, `Lists`, `Maps`, `Errors`, `FuncRefs` are directly accessible for ad-hoc inspection.

### Host memory
Standard C++ tools:
- Valgrind for leak detection
- AddressSanitizer for memory errors
- Debugger watches on `shared_ptr` reference counts

## Design rationale (summary)

### Why two systems?
- **Same behaviour on both platforms.** A custom GC for Values is the only way to make C# and C++ behave identically — C# can't NaN-box a managed reference, so a managed-only solution doesn't fit, and a C++-only refcount system doesn't fit C#.
- **No per-local protection.** Collection runs only at explicit boundaries, so locals don't need shadow-stack scaffolding or `GC_PROTECT` macros.
- **Predictable cleanup for "handle" types.** A planned future `GCHandle` GCSet will give native objects a deterministic dispose hook on sweep — important for things like file handles.

### Why `std::shared_ptr` for host code?
- Standard C++ pattern: well understood, well tooled, well tested.
- Mirrors C# reference semantics — clean target for the transpiler.
- No custom pool allocators to maintain.
- Works seamlessly with sanitizers, leak detectors, and debuggers.

The runtime is decidedly *not* `std::shared_ptr`-based because shared_ptr can't NaN-box, and would either require a separate heap-allocated wrapper (slow) or a refcount on every Value copy (slower).
