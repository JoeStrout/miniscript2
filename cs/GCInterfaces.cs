using System;

namespace MiniScript {

// Interface for items managed by a GCSet.
// Must be implemented by every GC-managed struct type.
public interface IGCItem {
	// Called during the Mark phase: mark every child Value this item holds.
	void MarkChildren();

	// Called during the Sweep phase before the slot is recycled.
	void OnSweep();
}


// Non-generic interface so GCManager can dispatch over all five GCSets
// without a switch statement.
//
public interface IGCSet {
	// Clear all mark bits before the Mark phase.
	void PrepareForGC();

	// Mark the item at idx and recurse into its children.
	void Mark(Int32 idx);

	// Mark all items with retain count > 0 (and their children).
	void MarkRetained();

	// Free every live, unmarked, unretained item.
	void Sweep();

	// Count of live slots (O(n); for diagnostics only).
	Int32 LiveCount();
}

}
