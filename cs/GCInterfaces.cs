using System;

namespace MiniScript {

// Callback invoked when a GCHandle is swept (its referent has no more live references).
// H: typedef void (*HandleFinalizer)(void* userData);
// H: inline bool IsNull(HandleFinalizer f) { return f == nullptr; }
public delegate void HandleFinalizer(object userData);

// Interface for items managed by a GCSet.
// Must be implemented by every GC-managed struct type.
public interface IGCItem {
	// Called during the Mark phase: mark every child Value this item holds.
	void MarkChildren();

	// Called during the Sweep phase before the slot is recycled.
	void OnSweep();
}


// IGCSet: conceptual interface for GC-managed item pools.
// We deliberately do NOT declare this as a real C# interface (or C++ abstract class),
// because doing so forces every GCSet wrapper type to carry a vtable pointer.
// In C++, that vtable is written by a static constructor; on some platforms/kernels,
// static constructors for global objects may not run, leaving the vtable pointer
// zero in BSS, which causes a segfault when any virtual method is called.
// Instead, GCManager dispatches GCSet methods via a switch statement (see
// DispatchMark) or explicit per-set calls, which is also faster than virtual dispatch.
//
// Expected methods on every concrete GCSet subclass:
//   void PrepareForGC()    - clear all mark bits before the Mark phase
//   void Mark(Int32 idx)   - mark item at idx and recurse into its children
//   void MarkRetained()    - mark all items with retain count > 0 (and children)
//   void Sweep()           - free every live, unmarked, unretained item
//   Int32 LiveCount()      - count of live slots (O(n); for diagnostics only)

}
