using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
using static MiniScript.ValueHelpers;
// H: #include "GCInterfaces.g.h"
// H: #include "value.h"
// H: #include "VarMap.g.h"
// H: #include "CS_Math.h"
// CPP: #include "GCManager.g.h"

namespace MiniScript {

// ── GCString ─────────────────────────────────────────────────────────────────

public struct GCString : IGCItem {
	public String Data;

	public void MarkChildren() {
		// strings have no child Values
	}

	public void OnSweep() {
		Data = null;
	}
}

// ── GCList ───────────────────────────────────────────────────────────────────

public struct GCList : IGCItem {
	public List<Value> Items;
	public Boolean Frozen;

	[MethodImpl(AggressiveInlining)]
	public void Init(Int32 capacity = 8) {
		Items  = new List<Value>(Math.Max(capacity, 4));
		Frozen = false;
	}

	[MethodImpl(AggressiveInlining)]
	public void Push(Value v) {
		if (Items == null) Init();
		Items.Add(v);
	}

	[MethodImpl(AggressiveInlining)]
	public Value Get(Int32 i) {
		if (i < 0) i += Items.Count;
		return (UInt32)i < (UInt32)Items.Count ? Items[i] : val_null;
	}

	[MethodImpl(AggressiveInlining)]
	public void Set(Int32 i, Value v) {
		if (i < 0) i += Items.Count;
		if ((UInt32)i < (UInt32)Items.Count) Items[i] = v;
	}

	[MethodImpl(AggressiveInlining)]
	public Boolean Remove(Int32 index) {
		if (Items == null) Init();
		if (index < 0) index += Items.Count;
		if (index < 0 || index >= Items.Count) return false;
		Items.RemoveAt(index);
		return true;
	}

	public void Insert(Int32 index, Value v) {
		if (Items == null) Init();
		if (index < 0) index += Items.Count + 1;
		if (index < 0) index = 0;  // ToDo: this should raise a runtime error
		if (index > Items.Count) index = Items.Count;
		Items.Insert(index, v);
	}

	public Value Pop() {
		if (Items == null || Items.Count == 0) return val_null; // ToDo: error
		Value result = Items[Items.Count - 1];
		Items.RemoveAt(Items.Count - 1);
		return result;
	}

	public Value Pull() {
		if (Items == null || Items.Count == 0) return val_null; // ToDo: error
		Value result = Items[0];
		Items.RemoveAt(0);
		return result;
	}

	public Int32 IndexOf(Value item, Int32 afterIdx) {
		if (Items == null) return -1;
		for (Int32 i = afterIdx + 1; i < Items.Count; i++) {
			if (value_equal(Items[i], item)) return i;
		}
		return -1;
	}

	public void MarkChildren() {
		if (Items == null) return;
		for (Int32 i = 0; i < Items.Count; i++) GCManager.Mark(Items[i]);
	}

	[MethodImpl(AggressiveInlining)]
	public void OnSweep() {
		Items = null;
		Frozen = false;
	}
}

// ── GCMap ─────────────────────────────────────────────────────────────────────
// A simple wrapper around Dictionary<Value, Value>, plus a Frozen flag and an
// optional VarMapBacking for register-binding behaviour. Hash/equality of keys
// follows MiniScript == semantics via Value.Equals/GetHashCode (see Value.cs).

public struct GCMap : IGCItem {
	public Dictionary<Value, Value> Items;
	public Boolean Frozen;

	// Non-null for VarMap-backed maps (closures / REPL globals).
	public VarMapBacking _vmb;

	public Int32 Count() {
		Int32 n = (Items == null) ? 0 : Items.Count;
		if (_vmb != null) n += _vmb.RegEntryCount();
		return n;
	}

	public void Init(Int32 capacity = 8) {
		Items  = new Dictionary<Value, Value>(Math.Max(capacity, 4));
		Frozen = false;
		_vmb   = null;
	}

	public Boolean TryGet(Value key, out Value value) {
		// Check VarMap register bindings first.
		if (_vmb != null && _vmb.TryGet(key, out value)) return true;

		if (Items == null) { value = val_null; return false; }
		if (Items.TryGetValue(key, out value)) return true;
		value = val_null;
		return false;
	}

	public void Set(Value key, Value value) {
		// Store in register if VarMap-backed and key is register-mapped.
		if (_vmb != null && _vmb.TrySet(key, value)) return;

		if (Items == null) Init();
		Items[key] = value;
	}

	public Boolean Remove(Value key) {
		if (_vmb != null && _vmb.TryRemove(key)) return true;
		if (Items == null) return false;
		return Items.Remove(key);
	}

	[MethodImpl(AggressiveInlining)]
	public Boolean HasKey(Value key) {
		Value ignored;
		return TryGet(key, out ignored);
	}

	public void Clear() {
		if (Items != null) Items.Clear();
		if (_vmb != null) _vmb.Clear();
	}

	// ── Iteration ─────────────────────────────────────────────────────────────
	// iter = -1: start
	// iter < -1: VarMap register entry -(i+2) where i is the reg-entry index
	// iter >= 0: index into Items (in enumeration order)

	public Int32 NextEntry(Int32 after) {
		// Phase 1: VarMap register entries (negative iter)
		if (_vmb != null && after <= -1) {
			Int32 startRegIdx = (after == -1) ? 0 : -(after) - 2 + 1;
			Int32 found = _vmb.NextAssignedRegEntry(startRegIdx);
			if (found >= 0) return -(found + 2);
			// Fall through to Dictionary phase
		}

		if (Items == null) return -1;
		Int32 i = (after < 0) ? 0 : after + 1;
		if (i < Items.Count) return i;
		return -1;
	}

	public Value KeyAt(Int32 i) {
		if (i < -1 && _vmb != null) {
			Int32 regIdx = -(i) - 2;
			return _vmb.GetRegEntryKey(regIdx);
		}
		if (Items == null) return val_null;
		Int32 j = 0;
		foreach (Value k in Items.Keys) {
			if (j == i) return k;
			j++;
		}
		return val_null;
	}

	public Value ValueAt(Int32 i) {
		if (i < -1 && _vmb != null) {
			Int32 regIdx = -(i) - 2;
			return _vmb.GetRegEntryValue(regIdx);
		}
		if (Items == null) return val_null;
		Int32 j = 0;
		foreach (Value v in Items.Values) {
			if (j == i) return v;
			j++;
		}
		return val_null;
	}

	// ── GC ────────────────────────────────────────────────────────────────────

	public void MarkChildren() {
		if (Items != null) {
			foreach (Value k in Items.Keys) {
				GCManager.Mark(k);
			}
			foreach (Value v in Items.Values) {
				GCManager.Mark(v);
			}
		}
		if (_vmb != null) _vmb.MarkChildren();
	}

	public void OnSweep() {
		Items  = null;
		Frozen = false;
		_vmb   = null;
	}
}

// ── GCError ──────────────────────────────────────────────────────────────────

public struct GCError : IGCItem {
	public Value Message;
	public Value Inner;
	public Value Stack;
	public Value Isa;

	public void MarkChildren() {
		GCManager.Mark(Message);
		GCManager.Mark(Inner);
		GCManager.Mark(Stack);
		GCManager.Mark(Isa);
	}

	[MethodImpl(AggressiveInlining)]
	public void OnSweep() {
		Message = val_null;
		Inner   = val_null;
		Stack   = val_null;
		Isa     = val_null;
	}
}

// ── GCFunction ────────────────────────────────────────────────────────────────

public struct GCFunction : IGCItem {
	public Int32 FuncIndex;
	public Value OuterVars;

	public void MarkChildren() {
		GCManager.Mark(OuterVars);
	}

	[MethodImpl(AggressiveInlining)]
	public void OnSweep() {
		FuncIndex = -1;
		OuterVars = val_null;
	}
}

}
