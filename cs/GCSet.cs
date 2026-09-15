using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
// H: #include "GCInterfaces.g.h"
// H: #include "GCItems.g.h"

namespace MiniScript {

//
// Non-generic abstract base for all GC item pools.
// Manages bookkeeping metadata (InUse, Marked, RetainCount, free-list).
// Subclasses supply the typed item list and the three abstract item operations.
// Satisfies the IGCSet conceptual interface (see GCInterfaces.cs).
//
public abstract class GCSetBase {
	protected List<Boolean> _inUse = new List<Boolean>();
	protected List<Boolean> _marked = new List<Boolean>();
	protected List<Byte> _retainCounts = new List<Byte>();
	protected List<Int32> _free = new List<Int32>();

	// Slots currently in use.  Maintained incrementally rather than counted on
	// demand: LiveCount() is called on every MaybeCollect tick, and scanning
	// _inUse there would be O(slots ever allocated) once per frame.  _inUse is
	// written in exactly three places, all in this class (both AllocItem
	// branches, and Sweep), so the tally cannot drift.
	protected Int32 _liveCount = 0;

	// Subclass calls item[idx].MarkChildren().
	protected abstract void CallMarkChildren(Int32 idx);

	// Subclass calls item[idx].OnSweep().
	protected abstract void CallOnSweep(Int32 idx);

	// Subclass appends a default-constructed item to its items list.
	protected abstract void AppendItem();

	// Subclass drops its items from newCount on, and releases their spare
	// capacity if trimCapacity.
	protected abstract void TruncateItems(Int32 newCount, Boolean trimCapacity);

	// Sweep releases spare capacity only when at least this many slots came off
	// the end, so small tables are not reallocated over a handful of slots.
	private const Int32 TrimCapacityMinSlots = 1024;

	// ── Allocation ───────────────────────────────────────────────────────────

	public Int32 AllocItem() {
		Int32 idx;
		_liveCount++;
		if (_free.Count > 0) {
			idx = _free[_free.Count - 1];
			_free.RemoveAt(_free.Count - 1);
			_inUse[idx]        = true;
			_marked[idx]       = false;
			_retainCounts[idx] = 0;
		} else {
			idx = _inUse.Count;
			_inUse.Add(true);
			_marked.Add(false);
			_retainCounts.Add(0);
			AppendItem();
		}
		return idx;
	}

	// ── Retain / Release ─────────────────────────────────────────────────────

	public void Retain(Int32 idx) {
		if (_retainCounts[idx] == 255) throw new InvalidOperationException("GCSet retained > 255 times"); // CPP: 
		_retainCounts[idx]++;
	}

	public void Release(Int32 idx) {
		if (_retainCounts[idx] == 0) throw new InvalidOperationException("GCSet released more than retained"); // CPP: 
		_retainCounts[idx]--;
	}

	// ── IGCSet implementation ─────────────────────────────────────────────────

	public void PrepareForGC() {
		for (Int32 i = 0; i < _marked.Count; i++) _marked[i] = false;
	}

	public void Mark(Int32 idx) {
		if (_marked[idx]) return;
		_marked[idx] = true;
		CallMarkChildren(idx);
	}

	public void MarkRetained() {
		for (Int32 i = 0; i < _inUse.Count; i++) {
			if (_inUse[i] && _retainCounts[i] > 0) Mark(i);
		}
	}

	// Free every in-use slot that is neither marked nor retained, then give
	// back the free slots at the end of the table.
	//
	// Slots cannot be moved -- every Value that refers to one bakes in its
	// index -- so only the tail can be trimmed.  To keep the tail trimmable, the
	// free list is rebuilt so that AllocItem hands out the lowest free index
	// first; live objects then settle toward the front of the table instead of
	// staying scattered up to its high-water mark.  The rebuild is one more
	// pass over a table that Sweep is already walking.
	public void Sweep() {
		Int32 count = _inUse.Count;
		Int32 lastInUse = -1;
		for (Int32 i = 0; i < count; i++) {
			if (!_inUse[i]) continue;
			if (!_marked[i] && _retainCounts[i] == 0) {
				CallOnSweep(i);
				_inUse[i]        = false;
				_retainCounts[i] = 0;
				_liveCount--;
			} else {
				lastInUse = i;
			}
		}

		// Truncating is cheap, since the removed slots are already empty.
		// Returning their memory means reallocating, so do that only when the
		// table has shrunk a lot; otherwise a heap that swings around a steady
		// size would reallocate on every cycle.
		Int32 newCount = lastInUse + 1;
		Boolean trimCapacity = false;
		if (newCount < count) {
			Int32 removed = count - newCount;
			trimCapacity = (removed >= TrimCapacityMinSlots && newCount < count / 4);
			_inUse.RemoveRange(newCount, removed);
			_marked.RemoveRange(newCount, removed);
			_retainCounts.RemoveRange(newCount, removed);
			TruncateItems(newCount, trimCapacity);
			if (trimCapacity) {
				_inUse.TrimExcess();
				_marked.TrimExcess();
				_retainCounts.TrimExcess();
			}
		}

		// Rebuild the free list highest index first, so that AllocItem (which
		// pops from the end) reuses the lowest free slot.
		_free.Clear();
		for (Int32 i = newCount - 1; i >= 0; i--) {
			if (!_inUse[i]) _free.Add(i);
		}
		if (trimCapacity) _free.TrimExcess();
	}

	// True if slot idx is currently in use and will survive the next Sweep
	// (either it was marked this cycle, or it has a non-zero retain count).
	public Boolean IsLiveSlot(Int32 idx) {
		return _inUse[idx] && (_marked[idx] || _retainCounts[idx] > 0);
	}

	public Int32 LiveCount() {
		return _liveCount;
	}

	// Length of the slot table, live or free.  Sweep trims free slots off the
	// end, so this is the high-water mark only since the last collection.
	public Int32 SlotCount() {
		return _inUse.Count;
	}
}

// ── GCStringSet ───────────────────────────────────────────────────────────────

public class GCStringSet : GCSetBase {
	private List<GCString> _items;

	public GCStringSet(Int32 initialCapacity = 64) {
		_items = new List<GCString>(initialCapacity);
	}

	protected override void CallMarkChildren(Int32 idx) {
		_items[idx].MarkChildren();
	}
	protected override void CallOnSweep(Int32 idx) {
		GCString item = _items[idx];
		item.OnSweep();
		_items[idx] = item;
	}
	protected override void AppendItem() {
		_items.Add(new GCString());
	}
	protected override void TruncateItems(Int32 newCount, Boolean trimCapacity) {
		_items.RemoveRange(newCount, _items.Count - newCount);
		if (trimCapacity) _items.TrimExcess();
	}

	[MethodImpl(AggressiveInlining)]
	public GCString Get(Int32 idx) {
		return _items[idx];
	}

	[MethodImpl(AggressiveInlining)]
	public void SetData(Int32 idx, String s) {
		GCString item = _items[idx];
		item.Data = s;
		//*** BEGIN CS_ONLY ***
		item.CpLen = -1;      // not yet measured; see GCString
		item.CursorChar = 0;
		item.CursorUnit = 0;
		//*** END CS_ONLY ***
		_items[idx] = item;
	}

	//*** BEGIN CS_ONLY ***
	// Character-index cache, for cs/Value.cs; see GCString for what it holds.
	// These live here because GCString is a struct, so a caller holding one
	// fetched with Get() would be updating a copy of it.

	[MethodImpl(AggressiveInlining)]
	public Int32 GetCpLen(Int32 idx) {
		return _items[idx].CpLen;
	}

	[MethodImpl(AggressiveInlining)]
	public void SetCpLen(Int32 idx, Int32 cpLen) {
		GCString item = _items[idx];
		item.CpLen = cpLen;
		_items[idx] = item;
	}

	[MethodImpl(AggressiveInlining)]
	public void GetCursor(Int32 idx, out Int32 cursorChar, out Int32 cursorUnit) {
		GCString item = _items[idx];
		cursorChar = item.CursorChar;
		cursorUnit = item.CursorUnit;
	}

	[MethodImpl(AggressiveInlining)]
	public void SetCursor(Int32 idx, Int32 cursorChar, Int32 cursorUnit) {
		GCString item = _items[idx];
		item.CursorChar = cursorChar;
		item.CursorUnit = cursorUnit;
		_items[idx] = item;
	}
	//*** END CS_ONLY ***
}

// ── GCListSet ─────────────────────────────────────────────────────────────────

public class GCListSet : GCSetBase {
	private List<GCList> _items;

	public GCListSet(Int32 initialCapacity = 64) {
		_items = new List<GCList>(initialCapacity);
	}

	protected override void CallMarkChildren(Int32 idx) {
		_items[idx].MarkChildren();
	}
	protected override void CallOnSweep(Int32 idx) {
		GCList item = _items[idx];
		item.OnSweep();
		_items[idx] = item;
	}
	protected override void AppendItem() {
		_items.Add(new GCList());
	}
	protected override void TruncateItems(Int32 newCount, Boolean trimCapacity) {
		_items.RemoveRange(newCount, _items.Count - newCount);
		if (trimCapacity) _items.TrimExcess();
	}

	[MethodImpl(AggressiveInlining)]
	public GCList Get(Int32 idx) {
		return _items[idx];
	}

	public void Init(Int32 idx, Int32 capacity) {
		GCList item = _items[idx];
		item.Init(capacity);
		_items[idx] = item;
	}

	[MethodImpl(AggressiveInlining)]
	public void SetFrozen(Int32 idx, Boolean frozen) {
		GCList item = _items[idx];
		item.Frozen = frozen;
		_items[idx] = item;
	}

	// Write back a (possibly mutated/materialized) GCList value.  Mutating
	// operations must call this so a materialized list's new Items reference and
	// cleared Computed flag are not lost to struct-copy semantics.
	[MethodImpl(AggressiveInlining)]
	public void Set(Int32 idx, GCList item) {
		_items[idx] = item;
	}
}

// ── GCMapSet ──────────────────────────────────────────────────────────────────

public class GCMapSet : GCSetBase {
	private List<GCMap> _items;

	public GCMapSet(Int32 initialCapacity = 64) {
		_items = new List<GCMap>(initialCapacity);
	}

	protected override void CallMarkChildren(Int32 idx) {
		_items[idx].MarkChildren();
	}
	protected override void CallOnSweep(Int32 idx) {
		GCMap item = _items[idx];
		item.OnSweep();
		_items[idx] = item;
	}
	protected override void AppendItem() {
		_items.Add(new GCMap());
	}
	protected override void TruncateItems(Int32 newCount, Boolean trimCapacity) {
		_items.RemoveRange(newCount, _items.Count - newCount);
		if (trimCapacity) _items.TrimExcess();
	}

	[MethodImpl(AggressiveInlining)]
	public GCMap Get(Int32 idx) {
		return _items[idx];
	}

	public void Init(Int32 idx, Int32 capacity) {
		GCMap item = _items[idx];
		item.Init(capacity);
		_items[idx] = item;
	}

	// Remove a key, writing the struct back afterwards: GCMap.Remove may build
	// the map's position index on its first removal, and that assignment would
	// otherwise land in the copy Get() returned.
	public Boolean Remove(Int32 idx, Value key) {
		GCMap item = _items[idx];
		Boolean removed = item.Remove(key);
		_items[idx] = item;
		return removed;
	}

	// Initialize a slot as the `globals` map view; see GCManager.NewGlobalsMap.
	public void InitAsGlobals(Int32 idx, Globals g) {
		GCMap item = _items[idx];
		item.InitAsGlobals(g);
		_items[idx] = item;
	}

	[MethodImpl(AggressiveInlining)]
	public void SetFrozen(Int32 idx, Boolean frozen) {
		GCMap item = _items[idx];
		item.Frozen = frozen;
		_items[idx] = item;
	}

	[MethodImpl(AggressiveInlining)]
	public void SetVmb(Int32 idx, VarMapBacking vmb) {
		GCMap item = _items[idx];
		item._vmb = vmb;
		_items[idx] = item;
	}

	// Attach an existing dictionary as this slot's contents, sharing its
	// storage rather than copying entries (Dictionary assignment shares the
	// underlying table).  Leaves Frozen and _vmb untouched, so this is meant
	// for a freshly allocated slot.  See GCManager.NewMapFromDict.
	[MethodImpl(AggressiveInlining)]
	public void SetItems(Int32 idx, Dictionary<Value, Value> items) {
		GCMap item = _items[idx];
		item.Items = items;
		// Seed the iteration order from what the dictionary already holds.  The
		// host owns that dictionary and may add to it afterwards, which GCMap
		// cannot see; GCMap.EnsureOrder notices the drift and rebuilds.
		item.SeedOrder();
		_items[idx] = item;
	}
}

// ── GCErrorSet ────────────────────────────────────────────────────────────────

public class GCErrorSet : GCSetBase {
	private List<GCError> _items;

	public GCErrorSet(Int32 initialCapacity = 64) {
		_items = new List<GCError>(initialCapacity);
	}

	protected override void CallMarkChildren(Int32 idx) {
		_items[idx].MarkChildren();
	}
	protected override void CallOnSweep(Int32 idx) {
		GCError item = _items[idx];
		item.OnSweep();
		_items[idx] = item;
	}
	protected override void AppendItem() {
		_items.Add(new GCError());
	}
	protected override void TruncateItems(Int32 newCount, Boolean trimCapacity) {
		_items.RemoveRange(newCount, _items.Count - newCount);
		if (trimCapacity) _items.TrimExcess();
	}

	[MethodImpl(AggressiveInlining)]
	public GCError Get(Int32 idx) {
		return _items[idx];
	}

	[MethodImpl(AggressiveInlining)]
	public void SetFields(Int32 idx, Value message, Value inner, Value stack, Value isa) {
		GCError item = _items[idx];
		item.Message = message;
		item.Inner   = inner;
		item.Stack   = stack;
		item.Isa     = isa;
		_items[idx] = item;
	}
}

// ── GCHandleSet ───────────────────────────────────────────────────────────────

public class GCHandleSet : GCSetBase {
	private List<GCHandle> _items;

	public GCHandleSet(Int32 initialCapacity = 16) {
		_items = new List<GCHandle>(initialCapacity);
	}

	protected override void CallMarkChildren(Int32 idx) {
		_items[idx].MarkChildren();
	}
	protected override void CallOnSweep(Int32 idx) {
		GCHandle item = _items[idx];
		item.OnSweep();
		_items[idx] = item;
	}
	protected override void AppendItem() {
		_items.Add(new GCHandle());
	}
	protected override void TruncateItems(Int32 newCount, Boolean trimCapacity) {
		_items.RemoveRange(newCount, _items.Count - newCount);
		if (trimCapacity) _items.TrimExcess();
	}

	[MethodImpl(AggressiveInlining)]
	public GCHandle Get(Int32 idx) {
		return _items[idx];
	}

	[MethodImpl(AggressiveInlining)]
	public void SetFields(Int32 idx, object userData, HandleFinalizer callback) {
		GCHandle item = _items[idx];
		item.UserData = userData;
		item.Callback = callback;
		_items[idx] = item;
	}
}

// ── GCFuncRefSet ──────────────────────────────────────────────────────────────

public class GCFuncRefSet : GCSetBase {
	private List<GCFunction> _items;

	public GCFuncRefSet(Int32 initialCapacity = 64) {
		_items = new List<GCFunction>(initialCapacity);
	}

	protected override void CallMarkChildren(Int32 idx) {
		_items[idx].MarkChildren();
	}
	protected override void CallOnSweep(Int32 idx) {
		GCFunction item = _items[idx];
		item.OnSweep();
		_items[idx] = item;
	}
	protected override void AppendItem() {
		_items.Add(new GCFunction());
	}
	protected override void TruncateItems(Int32 newCount, Boolean trimCapacity) {
		_items.RemoveRange(newCount, _items.Count - newCount);
		if (trimCapacity) _items.TrimExcess();
	}

	[MethodImpl(AggressiveInlining)]
	public GCFunction Get(Int32 idx) {
		return _items[idx];
	}

	[MethodImpl(AggressiveInlining)]
	public void SetFields(Int32 idx, FuncDef func, Value outerVars) {
		GCFunction item = _items[idx];
		item.Func = func;
		item.OuterVars = outerVars;
		_items[idx] = item;
	}
}

}
