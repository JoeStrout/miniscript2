// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: GCInterfaces.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"

namespace MiniScript {

// DECLARATIONS

// Interface for items managed by a GCSet.
// Must be implemented by every GC-managed struct type.
class IGCItem {
  public:
	virtual ~IGCItem() = default;
	virtual void MarkChildren() = 0;
	virtual void OnSweep() = 0;
	// Called during the Mark phase: mark every child Value this item holds.

	// Called during the Sweep phase before the slot is recycled.
}; // end of interface IGCItem

// Non-generic interface so GCManager can dispatch over all five GCSets
// without a switch statement.
class IGCSet {
  public:
	virtual ~IGCSet() = default;
	virtual void PrepareForGC() = 0;
	virtual void Mark(Int32 idx) = 0;
	virtual void MarkRetained() = 0;
	virtual void Sweep() = 0;
	virtual Int32 LiveCount() = 0;
	// Clear all mark bits before the Mark phase.

	// Mark the item at idx and recurse into its children.

	// Mark all items with retain count > 0 (and their children).

	// Free every live, unmarked, unretained item.

	// Count of live slots (O(n); for diagnostics only).
}; // end of interface IGCSet

// INLINE METHODS

} // end of namespace MiniScript
