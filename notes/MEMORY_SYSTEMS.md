# Memory Systems in MS2Proto3

This document describes the multiple memory management systems used in MS2Proto3 and how they interact.

## Overview

MS2Proto3 uses **four distinct memory systems** for different purposes:

1. **GC (Garbage Collector)** - For runtime MiniScript values
2. **Intern Table** - For frequently-used small runtime strings
3. **String Pool** - For host/compiler strings (C# `String` class)
4. **MemPool** - General purpose pooled allocation for host code

## 1. GC System (Garbage Collector)

**Location:** `cpp/core/gc.c`, `cpp/core/gc.h`

**Purpose:** Manages memory for MiniScript runtime values (strings, lists, maps).

**Implementation:**
- Mark-and-sweep garbage collector
- Shadow stack for root set tracking
- Each allocation has a `GCObject` header (next pointer, marked flag, size)
- All objects linked in `gc.all_objects` list
- Allocation: `gc_allocate(size)` - adds GCObject header, returns pointer to data
- Collection triggered when `gc.bytes_allocated > gc.gc_threshold`

**Used for:**
- **Heap strings** (strings > 128 bytes)
- **ValueList** structures and their item arrays
- **ValueMap** structures and their entry arrays
- Any dynamically-sized runtime data structures

**Lifetime:** Objects live until unreachable from root set, then collected during GC sweep.

**Key insight:** The `marked` flag is only valid during a collection cycle - it's set during mark phase and cleared during sweep phase.

## 2. Intern Table System

**Location:** `cpp/core/value_string.c` (lines 16-193)

**Purpose:** Deduplicate small, frequently-used runtime strings to save memory and reduce use of the GC system.  (Note that *very* small strings are stored directly in the Value, just like numbers, and so don't use heap memory at all.)

**Implementation:**
- Static hash table: `InternEntry* intern_table[INTERN_TABLE_SIZE]` (1024 buckets)
- Each `InternEntry` contains:
  - `Value string_value` - the interned string (a heap string Value)
  - `struct InternEntry* next` - collision chain pointer
- `InternEntry` structs allocated with `malloc()` (NOT gc_allocate)
- Hash-based lookup with collision chaining

**Used for:**
- Strings < 128 bytes (INTERN_THRESHOLD)
- Automatically used by `make_string()` for small strings
- Examples: keywords, common identifiers, short literals

**Lifetime:** **Immortal** - never freed during program execution. These persist for the lifetime of the app to avoid the cost of reference counting or managing lifetimes.  (They're not "leaked" because we can always reach them via the hash table.)

**Key characteristics:**
- NOT managed by GC
- The `StringStorage` structures themselves ARE still reachable and won't be collected
- Provides O(1) string deduplication for small strings

## 3. MemPool System

**Location:** `cpp/core/MemPool.h`, `cpp/core/MemPool.cpp`

**Purpose:** Pooled memory allocation for host code data structures.

**Implementation:**
- 256 independent pools (indexed 0-255)
- Each pool maintains array of blocks
- Blocks never freed individually - entire pool cleared at once
- Reference via `MemRef` (pool number + block index)

**Used for:**
- StringPool's internal structures (hash entries, StringStorage)
- Any host code needing pooled allocation
- Temporary data structures that can be bulk-freed

**Lifetime:** Blocks persist until entire pool is cleared with `MemPoolManager::clearPool(poolNum)`

## 4. String Pool System

**Location:** `cpp/core/StringPool.h`, `cpp/core/StringPool.cpp`

**Purpose:** String interning for **host** code (the assembler, compiler, etc., which make use of the C#-style `String` class).

**Implementation:**
- Based on MemPool (above)
- Each pool has hash table with 256 buckets
- Entries stored as `MemRef` (pool number + index into MemPool)
- Uses `MemPool` for underlying allocation

The idea behind the multiple pools is: a particular operation that may need to allocate a bunch of strings can use its own pool for that, and then drain just that pool when it's done, leaving other strings used by other parts of the program untouched.  But care must be taken if a process needs _some_ strings to survive the drain; those strings will need to be allocated in (or copied to) another pool.

**Used by:**
- `String` class (used for C# strings transpiled to C++)
- Assembler (function names, labels, etc.)
- Any host code using the `String` class
- VMVis display strings (uses temporary pool, cleared each frame)

**Lifetime:**
- Strings persist until their pool is explicitly cleared with `StringPool::clearPool(poolNum)`
- Pool 0 is the default and typically long-lived
- Temporary pools can be allocated/cleared for transient strings

**Separation from runtime:** This system is completely separate from the Value/GC system. Host strings are not MiniScript values.  Conversion to/from MiniScript strings always means copying the data.


## String Types Summary

### Tiny Strings (≤ 5 bytes)
- **Storage:** Inline in NaN-boxed Value (no allocation)
- **Examples:** "a", "x", "true", "null"
- **Lifetime:** Lives as long as the Value exists
- **System:** None (embedded in Value itself)

### Interned Strings (< 128 bytes)
- **Storage:** `malloc()` for StringStorage, stored in `intern_table`
- **Examples:** "count", "name", "print", short literals
- **Lifetime:** Immortal (never freed)
- **System:** Intern Table

### GC Heap Strings (≥ 128 bytes)
- **Storage:** `gc_allocate()` for StringStorage
- **Examples:** Long string literals, concatenation results
- **Lifetime:** GC-managed (collected when unreachable)
- **System:** GC

### Host Strings (C# `String` class)
- **Storage:** MemPool allocation, interned in StringPool
- **Examples:** Function names, labels, compiler strings, debug output
- **Lifetime:** Pool lifetime (until explicitly cleared)
- **System:** StringPool + MemPool

## Memory System Interactions

### Clear Separation
- **Runtime (GC + Intern Table)** ↔ **Host (StringPool + MemPool)** systems are independent
- Converting between them requires explicit string copying
- Host strings are never seen by the GC
- Runtime Values don't use StringPool

### Temporary Pool Strategy
Several subsystems use temporary pools to reduce memory pressure:

1. **Assembler**:
   - Allocates temp pool at start of `Assemble()`
   - Function names re-interned into pool 0 before clearing
   - Temp pool cleared after assembly complete

2. **VMVis**:
   - Allocates temp pool in constructor
   - Switches to it during `UpdateDisplay()`
   - Clears after each frame rendered

3. **App Main**:
   - Uses temp pool for file reading and command-line processing
   - Switches back to pool 0 before running VM

## Debugging Memory

### For GC Objects
Use `gc_dump_objects()`:
- Shows all objects in `gc.all_objects` list
- Hex/ASCII dump of object data
- Mark status (if mark phase run first)

### For Interned Strings
Use `dump_intern_table()`:
- Walk `intern_table[1024]` array
- Follow collision chains
- Shows each interned string value

### For StringPool
Use `StringPool::dumpAllPoolState()`:
- Shows all strings in each initialized pool
- Pool statistics (entries, bins, chain lengths)

### For MemPool
Use `MemPoolManager` statistics:
- Per-pool block counts and sizes
- Total memory allocated

## Design Rationale

### Why Multiple Systems?

1. **Performance:** Different allocation patterns optimized differently
   - GC for complex object graphs
   - Interning for deduplication
   - Pools for bulk allocation/deallocation

2. **Lifetime Management:** Different data has different lifetimes
   - Runtime values: GC-managed
   - Small strings: Immortal (intern table)
   - Compiler data: Scoped (pools)

3. **Language Integration:** Clean separation between host and runtime
   - C#-style String class uses StringPool
   - MiniScript strings use GC/intern table
   - No mixing or confusion

4. **Coding Convenience:** GC strings require extra boilerplate in every method that touches them, in order to maintain the shadow stack.  See [GC_USAGE.md](GC_USAGE.md) for details.  Such code is fiddly to write and easy to screw up, and is especially impractical for code transpiled from C# -- such as all the compiler and assembler code.  So, those use the String class and its pool system, which requires no extra boilerplate.

