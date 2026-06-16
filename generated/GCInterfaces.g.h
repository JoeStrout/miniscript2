// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: GCInterfaces.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"

namespace MiniScript {
typedef void (*HandleFinalizer)(void* userData);
inline bool IsNull(HandleFinalizer f) { return f == nullptr; }

// DECLARATIONS

// Callback invoked when a GCHandle is swept (its referent has no more live references).

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

// INLINE METHODS

} // end of namespace MiniScript
