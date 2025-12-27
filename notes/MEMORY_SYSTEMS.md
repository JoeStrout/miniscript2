# Memory Systems in MS2Proto3

This document describes the memory management systems used in MS2Proto3 and how they interact.

## Overview

MS2Proto3 uses **three distinct memory systems** for different purposes:

1. **GC (Garbage Collector)** - For runtime MiniScript values
2. **Intern Table** - For frequently-used small runtime strings
3. **std::shared_ptr** - For host/compiler C++ code (CS_String, CS_List, CS_Dictionary)

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

**Purpose:** Deduplicate small, frequently-used runtime strings to save memory and reduce use of the GC system. (Note that *very* small strings are stored directly in the Value, just like numbers, and so don't use heap memory at all.)

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

**Lifetime:** **Immortal** - never freed during program execution. These persist for the lifetime of the app to avoid the cost of reference counting or managing lifetimes. (They're not "leaked" because we can always reach them via the hash table.)

**Key characteristics:**
- NOT managed by GC
- The `StringStorage` structures themselves ARE still reachable and won't be collected
- Provides O(1) string deduplication for small strings

## 3. Host C++ Memory Management (std::shared_ptr)

**Location:** `cpp/core/CS_String.h`, `cpp/core/CS_List.h`, `cpp/core/CS_Dictionary.h`

**Purpose:** Automatic memory management for host code (transpiled C# code, compiler, assembler, debugger).

**Implementation:**
- Uses standard C++ `std::shared_ptr` for reference-counted memory management
- `CS_String`: Wraps `StringStorage*` in `std::shared_ptr<StringStorage>`
- `CS_List<T>`: Wraps `std::vector<T>*` in `std::shared_ptr<std::vector<T>>`
- `CS_Dictionary<K,V>`: Wraps internal storage in `std::shared_ptr`

**Used by:**
- `String` class (used for C# strings transpiled to C++)
- `List<T>` class (C# List<T> transpiled to C++)
- `Dictionary<K,V>` class (C# Dictionary<K,V> transpiled to C++)
- Assembler, compiler, debugger, and any other host code

**Lifetime:**
- Automatic: Memory freed when last reference goes out of scope
- No manual memory management needed
- Thread-safe reference counting

**Key advantages:**
- Standard C++ idiom - well understood, well tested
- No manual pool management needed
- Works with all C++ tooling (debuggers, sanitizers, etc.)
- Clean separation from GC system

**Separation from runtime:** This system is completely separate from the Value/GC system. Host strings are not MiniScript values. Conversion to/from MiniScript strings always means copying the data.

**BEWARE OF REFERENCE CYCLES:** Unlike the C# code, our shared_ptr implementations can leak objects where reference cycles exist.  CS_String is safe (since it cannot reference any other objects), but CS_List and CS_Dictionary could create such cycles.  So, it is important that the C# code either avoid such cycles, or explicitly break them at clean-up time, so that the transpiled code does not leak.

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
- **Storage:** `malloc()` for StringStorage, managed by `std::shared_ptr`
- **Examples:** Function names, labels, compiler strings, debug output
- **Lifetime:** Reference-counted (freed when last reference goes away)
- **System:** std::shared_ptr

## Memory System Interactions

### Clear Separation
- **Runtime (GC + Intern Table)** ↔ **Host (std::shared_ptr)** systems are independent
- Converting between them requires explicit string copying (see `CS_value_util.h`)
- Host strings are never seen by the GC
- Runtime Values don't use shared_ptr

### No Pool Management Needed
Unlike earlier prototypes, host code now uses standard C++ memory management:
- No need to allocate/manage memory pools
- No need to carefully track which pool strings belong to
- No risk of accidentally clearing a pool that's still in use
- Memory automatically freed when no longer referenced

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

### For Host Memory
Use standard C++ tools:
- Valgrind for leak detection
- AddressSanitizer for memory errors
- Debugger watches on shared_ptr reference counts

## Design Rationale

### Why Two Systems?

1. **Performance:** Different allocation patterns optimized differently
   - GC for complex object graphs with cycles
   - Interning for string deduplication
   - shared_ptr for simple reference counting

2. **Lifetime Management:** Different data has different lifetimes
   - Runtime values: GC-managed (complex reachability)
   - Small strings: Immortal (intern table)
   - Compiler data: Reference-counted (std::shared_ptr)

3. **Language Integration:** Clean separation between host and runtime
   - C#-style String/List/Dictionary classes use std::shared_ptr
   - MiniScript strings use GC/intern table
   - No mixing or confusion

4. **Coding Convenience:** GC strings require extra boilerplate in every method that touches them, in order to maintain the shadow stack. See [GC_USAGE.md](GC_USAGE.md) for details. Such code is fiddly to write and easy to screw up, and is especially impractical for code transpiled from C# -- such as all the compiler and assembler code. So, those use the String class with std::shared_ptr, which requires no extra boilerplate.

### Why std::shared_ptr Instead of Custom Pools?

Earlier prototypes used custom MemPool and StringPool systems, but we switched to std::shared_ptr because:

1. **Simplicity:** Standard C++ patterns everyone understands
2. **Safety:** Automatic memory management eliminates pool management bugs
3. **Compatibility:** Works seamlessly with C++ tooling and libraries
4. **Transpiled Code:** Perfect for C#-to-C++ transpilation (mimics C# reference semantics)
5. **Less Code:** No need to maintain custom pool allocators

The custom pool systems were designed for bulk allocation/deallocation, but in practice:
- Pool lifetime management was error-prone
- Thread safety was complex
- Debugging was harder than with standard tools
- The performance benefits didn't outweigh the complexity

For a transpiler targeting C++, using standard C++ memory management is the right choice.
