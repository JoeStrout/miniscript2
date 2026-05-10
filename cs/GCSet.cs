//*** BEGIN CS_ONLY ***
using System;

namespace MiniScript {

/// <summary>
/// Manages a pool of GC-tracked objects of type T using a struct-of-arrays layout
/// so GC metadata (Marked, InUse, RetainCount) occupies its own tight arrays,
/// separate from item data.
/// </summary>
public sealed class GCSet<T> : IGCSet where T : struct, IGCItem {

	private T[]    _items;
	private bool[] _inUse;
	private bool[] _marked;
	private int[]  _retainCounts;

	private int    _hwm;       // high-water mark: indices 0.._hwm-1 have been used
	private int[]  _free;      // stack of recycled indices
	private int    _freeTop;

	public GCSet(int initialCapacity = 64) {
		_items        = new T[initialCapacity];
		_inUse        = new bool[initialCapacity];
		_marked       = new bool[initialCapacity];
		_retainCounts = new int[initialCapacity];
		_free         = new int[initialCapacity];
		_freeTop      = 0;
		_hwm          = 0;
	}

	// ── Allocation ──────────────────────────────────────────────────────────

	/// <summary>
	/// Reserve a slot and return its index.
	/// Caller must initialise the item via Get(idx) before use.
	/// </summary>
	public int New() {
		int idx;
		if (_freeTop > 0) {
			idx = _free[--_freeTop];
		} else {
			idx = _hwm++;
			if (idx >= _items.Length) Grow();
		}
		_items[idx]        = default;
		_inUse[idx]        = true;
		_marked[idx]       = false;
		_retainCounts[idx] = 0;
		return idx;
	}

	/// <summary>Returns a ref to the item at idx for in-place initialisation or mutation.</summary>
	public ref T Get(int idx) => ref _items[idx];

	// ── Retain / Release ────────────────────────────────────────────────────

	public void Retain(int idx)  => _retainCounts[idx]++;
	public void Release(int idx) => _retainCounts[idx]--;

	// ── IGCSet implementation ────────────────────────────────────────────────

	public void PrepareForGC() {
		Array.Clear(_marked, 0, _hwm);
	}

	public void Mark(int idx, GCManager gc) {
		if (_marked[idx]) return;
		_marked[idx] = true;
		_items[idx].MarkChildren(gc);
	}

	public void MarkRetained(GCManager gc) {
		for (int i = 0; i < _hwm; i++)
			if (_inUse[i] && _retainCounts[i] > 0) Mark(i, gc);
	}

	public void Sweep() {
		for (int i = 0; i < _hwm; i++) {
			if (_inUse[i] && !_marked[i] && _retainCounts[i] <= 0) {
				_items[i].OnSweep();
				_items[i]        = default;
				_inUse[i]        = false;
				_retainCounts[i] = 0;
				if (_freeTop >= _free.Length) Array.Resize(ref _free, _free.Length * 2);
				_free[_freeTop++] = i;
			}
		}
	}

	public int LiveCount {
		get {
			int n = 0;
			for (int i = 0; i < _hwm; i++) if (_inUse[i]) n++;
			return n;
		}
	}

	// ── Internal ─────────────────────────────────────────────────────────────

	private void Grow() {
		int newLen = _items.Length * 2;
		Array.Resize(ref _items,        newLen);
		Array.Resize(ref _inUse,        newLen);
		Array.Resize(ref _marked,       newLen);
		Array.Resize(ref _retainCounts, newLen);
		Array.Resize(ref _free,         newLen);
	}
}

}
//*** END CS_ONLY ***
