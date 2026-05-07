Title: NaN-boxed values
Status: Accepted (*)

Context:
MiniScript is a dynamically typed language, which means that any value (local variable/register entry, map key or value, list entry, etc.) can contain any valid MiniScript type.  In MS1, a Value was an abstract base class, with subclasses for number, string, list, funcRef, etc.  This made values regrettably "heavy" and had measuable performance impact.  A key reason for the clean rewrite in MS2 was to enable a more efficient value representation.

Decision:
Use "NaN boxing", where every value is stored as a Double (8-byte IEEE 754 value), but some NaN values are special in that they encode other types.  For heap-allocated objects, we will box a 48-bit pointer (C++) or index into a handle pool (C#) that points to the GC-managed data.  Strings are represented two ways: "tiny strings" (5 bytes or less) are stored right in the NaN box, while larger strings are on the heap.

Alternatives Considered:
- Class hierarchy: what we did before; not performant.
- Union struct: better, but still requires a separate "type" byte, which makes the overall struct an awkward size (9 bytes in order to hold a double value).

Consequences:
- Uniform 8-byte value size, making for efficient value arrays, etc.

In C++:
- 48-bit pointer assumption: works on current x86-64 and ARM64, but could break on some systems.  If that becomes a problem in the future, we may have to switch to a HandlePool approach there too.
- Our custom GC system must not relocate objects in the heap, since Values contain pointers to them.  (Or if it ever does, we will have to update all Values.)

In C#:
- Extra indirection: every dereference requires an extra lookup through HandlePool.
- Dysfunctional GC: our HandlePool holds a (strong) reference to every object ever created, effectively disabling GC for those objects.  We could drain the pool after a run or at special times (e.g. `reset`), but until we do, all objects essentially leak.

(*) This last consequence is a harsh trade-off in favor of the C++ code, at the cost of the C# code, which was not fully realized until after the decision was made.  We may need to revisit this design choice, perhaps makinga different choice in the C# code than in the C++ code, in order to get good performance and normal GC behavior for both.
