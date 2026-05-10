//*** BEGIN CS_ONLY ***
namespace MiniScript {

/// <summary>
/// Non-generic interface so GCManager can dispatch over all five GCSets
/// without a switch statement.
/// </summary>
public interface IGCSet {
	/// <summary>Clear all mark bits before the Mark phase.</summary>
	void PrepareForGC();

	/// <summary>Mark the item at idx and recurse into its children.</summary>
	void Mark(int idx, GCManager gc);

	/// <summary>Mark all items with retain count > 0 (and their children).</summary>
	void MarkRetained(GCManager gc);

	/// <summary>Free every live, unmarked, unretained item.</summary>
	void Sweep();

	/// <summary>Count of live slots (O(n); for diagnostics only).</summary>
	int LiveCount { get; }
}

}
//*** END CS_ONLY ***
