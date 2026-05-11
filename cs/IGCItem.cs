//*** BEGIN CS_ONLY ***
namespace MiniScript {

// Interface for items managed by a GCSet.
// Must be implemented by every GC-managed struct type.
public interface IGCItem {
	// Called during the Mark phase: mark every child Value this item holds.
	void MarkChildren(GCManager gc);

	// Called during the Sweep phase before the slot is recycled.
	void OnSweep();
}

}
//*** END CS_ONLY ***
