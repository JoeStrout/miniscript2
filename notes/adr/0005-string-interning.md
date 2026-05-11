Title: Semi-Immortal Interned Strings

Status: Proposed

Context:
MiniScript currently represents strings in three ways:

- Tiny strings, up to 5 bytes, are encoded directly in the `Value`.
- Short heap strings, currently 6–127 bytes, are interned through an intern table but still participate in normal GC.
- Longer heap strings are allocated normally and are not interned.

The current weak-interning approach deduplicates live short strings, but interned strings are swept like any other GC object. This means interning no longer provides some of the strongest benefits of traditional string interning: long-lived canonical identity, avoiding normal GC work, and reuse across separate allocation lifetimes.

In practice, most strings in MiniScript programs are expected to be small and repeated: identifiers, keywords, map keys, property names, function names, and other symbolic values. Strings are also immutable and contain no child `Value` references, making them safe candidates for special lifetime handling.

Decision:
Interned strings will become semi-immortal.

Interned heap strings will bypass ordinary GC cycles. They will remain canonical and reusable across normal collections, and ordinary GC will not mark or sweep the interned string set.

Interned strings will still be reclaimable during explicit or exceptional full-collection events, such as:

- user-initiated `reset`
- memory-pressure collection
- VM teardown
- other explicit full-GC requests

This may be implemented as a separate `GCSet<GCString>` for interned strings, distinct from the ordinary string set. Normal GC will skip this set entirely. Full GC will mark and sweep it along with the rest of the heap, removing swept entries from the intern table before freeing their storage.

The intern table remains the content-addressed lookup structure mapping string content to interned string slots. Its entries are valid for the lifetime of the corresponding semi-immortal string and are removed when that string is reclaimed during full collection.

Alternatives Considered:
- Keep current weak interning: Simple and memory-safe, but provides only live-object deduplication and still requires interned strings to participate in normal GC.
- Restore fully immortal interning: Maximizes canonicalization and GC performance, but permits unbounded permanent growth if user code generates many unique short strings.
- Remove interning entirely: Simplifies GC and allocation, but loses deduplication, canonical identity, and fast equality for repeated short strings.
- Keep one string set with an `Interned` flag and make normal sweep skip interned entries:  Avoids adding a new `Value` string-set kind, but may still require scanning over interned slots unless the set is internally partitioned.

Consequences:
- Normal GC should become faster when many live objects are interned strings, because interned strings need not be marked or swept.
- Repeated short strings retain canonical identity across ordinary GC cycles, improving equality checks and map-key behavior.
- Deduplication becomes stronger than weak interning: a short string can be reused even if all ordinary references to it disappear between normal GC cycles.
- Interned string memory can still be reclaimed during explicit full-GC events, avoiding the worst unbounded-growth behavior of fully immortal interning.
- The implementation must ensure that all string operations recognize both ordinary heap strings and semi-immortal interned strings as strings.
- If implemented as a separate `GCSet`, the `Value` representation, string predicates, accessors, hashing, equality, printing, and serialization code must account for the additional string set.
- Full GC must remove reclaimed interned strings from the intern table before freeing their storage.
- Since `GCString` objects have no child references, skipping interned strings during normal GC does not hide references to other GC-managed objects.