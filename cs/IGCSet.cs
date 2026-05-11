//*** BEGIN CS_ONLY ***
using System;

namespace MiniScript {

// Non-generic interface so GCManager can dispatch over all five GCSets
// without a switch statement.
// 
public interface IGCSet {
	// Clear all mark bits before the Mark phase.
	void PrepareForGC();

	// Mark the item at idx and recurse into its children.
	void Mark(Int32 idx, GCManager gc);

	// Mark all items with retain count > 0 (and their children).
	void MarkRetained(GCManager gc);

	// Free every live, unmarked, unretained item.
	void Sweep();

	// Count of live slots (O(n); for diagnostics only).
	Int32 LiveCount();
}

}
//*** END CS_ONLY ***
