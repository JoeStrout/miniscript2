To keep our core C code from becoming a tangled mess, we divide the code into layers as follows:

Layer 0: Foundation Utilities (no dependencies)
- hashing.h/.c - Hash functions
- unicodeUtil.h/.c - Unicode/UTF-8 utilities
- dispatch_macros.h - VM dispatch macros (pure preprocessor)

Layer 1: String Infrastructure
  - StringStorage.h/.c - Core string storage (depends on: unicodeUtil)

Layer 2A: Runtime Value System
- value.h/.c - NaN-boxed Value type (depends on: hashing.h)
- value_string.h/.c - String Values (depends on: value.h, StringStorage.h, gc.h)
- value_list.h/.c - List Values (depends on: value.h, gc.h)
- value_map.h/.c - Map Values (depends on: value.h, gc.h)
- gc.h/.c - GC for runtime Values (depends on: value.h, value_string.h, value_list.h, value_map.h)

Layer 3A: (Reserved for future runtime features)

Layer 2B: Host Memory Management
- MemPool.h/.cpp - Memory pool system for host code
- StringPool.h/.cpp - String interning system (depends on: MemPool, StringStorage)

Layer 3B: Host C# Compatibility Layer
- CS_List.h - C# List template (depends on: MemPool)
- CS_String.h - C# String class (depends on: StringStorage, StringPool, CS_List)
- CS_Math.h - Math utilities
- core_includes.h - Aggregates the above for transpiled code

Layer 4: Host-Value Utilities
- (functions to convert between host strings and Value strings, etc.)

A lower-layer file may never include or use a higher-layer one; _and_, nothing in the "A" side may depend on the "B" side or vice versa.

