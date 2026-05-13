using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
// H: #include "GCInterfaces.g.h"
// H: #include "GCItems.g.h"

namespace MiniScript {

//
// Non-generic abstract base for all GC item pools.
// Manages bookkeeping metadata (InUse, Marked, RetainCount, free-list)
// and implements IGCSet.  Subclasses supply the typed item list and
// the three abstract item operations.
//
public abstract class GCSetBase : IGCSet {
	protected List<Boolean> _inUse = new List<Boolean>();
	protected List<Boolean> _marked = new List<Boolean>();
	protected List<Byte> _retainCounts = new List<Byte>();
	protected List<Int32> _free = new List<Int32>();

	// Subclass calls item[idx].MarkChildren().
	protected abstract void CallMarkChildren(Int32 idx);

	// Subclass calls item[idx].OnSweep().
	protected abstract void CallOnSweep(Int32 idx);

	// Subclass appends a default-constructed item to its items list.
	protected abstract void AppendItem();

	// ── Allocation ───────────────────────────────────────────────────────────

	public Int32 AllocItem() {
		Int32 idx;
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

	public void Sweep() {
		for (Int32 i = 0; i < _inUse.Count; i++) {
			if (_inUse[i] && !_marked[i] && _retainCounts[i] == 0) {
				CallOnSweep(i);
				_inUse[i]        = false;
				_retainCounts[i] = 0;
				_free.Add(i);
			}
		}
	}

	// True if slot idx is currently in use and will survive the next Sweep
	// (either it was marked this cycle, or it has a non-zero retain count).
	public Boolean IsLiveSlot(Int32 idx) {
		return _inUse[idx] && (_marked[idx] || _retainCounts[idx] > 0);
	}

	public Int32 LiveCount() {
		Int32 n = 0;
		for (Int32 i = 0; i < _inUse.Count; i++) {
			if (_inUse[i]) n++;
		}
		return n;
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

	[MethodImpl(AggressiveInlining)]
	public GCString Get(Int32 idx) {
		return _items[idx];
	}

	[MethodImpl(AggressiveInlining)]
	public void SetData(Int32 idx, String s) {
		GCString item = _items[idx];
		item.Data = s;
		_items[idx] = item;
	}
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

	[MethodImpl(AggressiveInlining)]
	public GCMap Get(Int32 idx) {
		return _items[idx];
	}

	public void Init(Int32 idx, Int32 capacity) {
		GCMap item = _items[idx];
		item.Init(capacity);
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

	[MethodImpl(AggressiveInlining)]
	public GCFunction Get(Int32 idx) {
		return _items[idx];
	}

	[MethodImpl(AggressiveInlining)]
	public void SetFields(Int32 idx, Int32 funcIndex, Value outerVars) {
		GCFunction item = _items[idx];
		item.FuncIndex = funcIndex;
		item.OuterVars = outerVars;
		_items[idx] = item;
	}
}

}
