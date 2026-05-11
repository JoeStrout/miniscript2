//*** BEGIN CS_ONLY ***
using System;

namespace MiniScript {

// 
// Manages a pool of GC-tracked objects of type T using a struct-of-arrays layout
// so GC metadata (Marked, InUse, RetainCount) occupies its own tight arrays,
// separate from item data.
// 
public sealed class GCSet<T> : IGCSet where T : struct, IGCItem {

	private T[]       _items;
	private Boolean[] _inUse;
	private Boolean[] _marked;
	private Byte[]   _retainCounts;

	private Int32     _hwm;       // high-water mark: indices 0.._hwm-1 have been used
	private Int32[]   _free;      // stack of recycled indices
	private Int32     _freeTop;

	public GCSet(Int32 initialCapacity = 64) {
		_items        = new T[initialCapacity];
		_inUse        = new Boolean[initialCapacity];
		_marked       = new Boolean[initialCapacity];
		_retainCounts = new Byte[initialCapacity];
		_free         = new Int32[initialCapacity];
		_freeTop      = 0;
		_hwm          = 0;
	}

	// ── Allocation & Access ──────────────────────────────────────────────────

	// 
	// Reserve a slot and return its index.
	// Caller must initialise the item via Get(idx) before use.
	// 
	public Int32 New() {
		Int32 idx;
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

	// Returns a ref to the item at idx for in-place initialisation or mutation.
	public ref T Get(Int32 idx) { return ref _items[idx]; }

	// ── Retain / Release ────────────────────────────────────────────────────

	public void Retain(Int32 idx)  {
		if (_retainCounts[idx] == 255) {
			throw new InvalidOperationException("GCSet value retained > 255 times");
		}
		_retainCounts[idx]++;
	}
	public void Release(Int32 idx) {
		if (_retainCounts[idx] == 0) {
			throw new InvalidOperationException("GCSet value released more than retained");
		}
		_retainCounts[idx]--;
	}

	// ── IGCSet implementation ────────────────────────────────────────────────

	public void PrepareForGC() {
		Array.Clear(_marked, 0, _hwm);
	}

	public void Mark(Int32 idx, GCManager gc) {
		if (_marked[idx]) return;
		_marked[idx] = true;
		_items[idx].MarkChildren(gc);
	}

	public void MarkRetained(GCManager gc) {
		for (Int32 i = 0; i < _hwm; i++) {
			if (_inUse[i] && _retainCounts[i] > 0) Mark(i, gc);
		}
	}

	public void Sweep() {
		for (Int32 i = 0; i < _hwm; i++) {
			if (_inUse[i] && !_marked[i] && _retainCounts[i] == 0) {
				_items[i].OnSweep();
				_items[i]        = default;
				_inUse[i]        = false;
				_retainCounts[i] = 0;
				if (_freeTop >= _free.Length) Array.Resize(ref _free, _free.Length * 2);
				_free[_freeTop++] = i;
			}
		}
	}

	public Int32 LiveCount() {
		Int32 n = 0;
		for (Int32 i = 0; i < _hwm; i++) {
			if (_inUse[i]) n++;
		}
		return n;
	}

	// ── Internal ─────────────────────────────────────────────────────────────

	private void Grow() {
		Int32 newLen = _items.Length * 2;
		Array.Resize(ref _items,        newLen);
		Array.Resize(ref _inUse,        newLen);
		Array.Resize(ref _marked,       newLen);
		Array.Resize(ref _retainCounts, newLen);
		Array.Resize(ref _free,         newLen);
	}
}

}
//*** END CS_ONLY ***
