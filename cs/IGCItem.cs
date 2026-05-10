//*** BEGIN CS_ONLY ***
namespace MiniScript {

/// <summary>
/// Interface for items managed by a GCSet.
/// Must be implemented by every GC-managed struct type.
/// </summary>
public interface IGCItem {
	/// <summary>Called during the Mark phase: mark every child Value this item holds.</summary>
	void MarkChildren(GCManager gc);

	/// <summary>Called during the Sweep phase before the slot is recycled.</summary>
	void OnSweep();
}

}
//*** END CS_ONLY ***
