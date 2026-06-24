using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
using static MiniScript.ValueHelpers;
// H: #include "GCInterfaces.g.h"
// H: #include "value.h"
// H: #include "VarMap.g.h"
// H: #include "FuncDef.g.h"
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
	// Items is private: a list may be either "materialized" (Computed == false,
	// Items holds the actual elements) or "computed" (Computed == true, Items
	// holds exactly three meta-values: [0]=base, [1]=increment, [2]=length).
	// All access goes through the methods below so callers never see the
	// difference.  A computed list with a null increment repeats the base value
	// (used for `[x] * n`); otherwise element i is base + increment * i.
	//
	// IMPORTANT: a mutation may materialize a computed list, which replaces the
	// Items reference and clears the Computed flag.  Since GCList is a struct,
	// callers MUST write the value back (GCManager.Lists.Set / SetFrozen style)
	// after any operation that can mutate, or the change is lost.
	private List<Value> Items;
	public Boolean Frozen;
	public Boolean Computed;

	[MethodImpl(AggressiveInlining)]
	public void Init(Int32 capacity = 8) {
		Items    = new List<Value>(Math.Max(capacity, 4));
		Frozen   = false;
		Computed = false;
	}

	// Construct a computed list.  increment may be val_null to repeat baseVal.
	public void InitComputed(Value baseVal, Value increment, Int32 length) {
		Items = new List<Value>(3);
		Items.Add(baseVal);
		Items.Add(increment);
		Items.Add(make_int(length));
		Frozen   = false;
		Computed = true;
	}

	// Replace a computed list with the equivalent materialized list.  No-op for
	// an already-materialized list.  Mutates this struct; caller must write back.
	public void Materialize() {
		if (!Computed) return;
		Int32 len = (Int32)numeric_val(Items[2]);
		List<Value> real = new List<Value>(Math.Max(len, 4));
		for (Int32 i = 0; i < len; i++) real.Add(Get(i));  // Get still reads meta
		Items    = real;
		Computed = false;
	}

	[MethodImpl(AggressiveInlining)]
	public Int32 Count() {
		if (Computed) return (Int32)numeric_val(Items[2]);
		return (Items == null) ? 0 : Items.Count;
	}

	[MethodImpl(AggressiveInlining)]
	public void Push(Value v) {
		if (Computed) Materialize();
		if (Items == null) Init();
		Items.Add(v);
	}

	[MethodImpl(AggressiveInlining)]
	public Value Get(Int32 i) {
		if (Computed) {
			Int32 len = (Int32)numeric_val(Items[2]);
			if (i < 0) i += len;
			if ((UInt32)i >= (UInt32)len) return val_null;
			Value incr = Items[1];
			if (is_null(incr)) return Items[0];
			Double d = numeric_val(Items[0]) + numeric_val(incr) * i;
			return (d == (Int32)d) ? make_int((Int32)d) : make_double(d);
		}
		if (i < 0) i += Items.Count;
		return (UInt32)i < (UInt32)Items.Count ? Items[i] : val_null;
	}

	[MethodImpl(AggressiveInlining)]
	public void Set(Int32 i, Value v) {
		if (Computed) Materialize();
		if (i < 0) i += Items.Count;
		if ((UInt32)i < (UInt32)Items.Count) Items[i] = v;
	}

	[MethodImpl(AggressiveInlining)]
	public Boolean Remove(Int32 index) {
		if (Computed) Materialize();
		if (Items == null) Init();
		if (index < 0) index += Items.Count;
		if (index < 0 || index >= Items.Count) return false;
		Items.RemoveAt(index);
		return true;
	}

	public void Insert(Int32 index, Value v) {
		if (Computed) Materialize();
		if (Items == null) Init();
		if (index < 0) index += Items.Count + 1;
		if (index < 0) index = 0;  // ToDo: this should raise a runtime error
		if (index > Items.Count) index = Items.Count;
		Items.Insert(index, v);
	}

	public Value Pop() {
		if (Computed) {
			Int32 len = (Int32)numeric_val(Items[2]);
			if (len == 0) return val_null;
			Value last = Get(len - 1);
			Items[2] = make_int(len - 1);
			return last;
		}
		if (Items == null || Items.Count == 0) return val_null; // ToDo: error
		Value result = Items[Items.Count - 1];
		Items.RemoveAt(Items.Count - 1);
		return result;
	}

	public Value Pull() {
		if (Computed) Materialize();
		if (Items == null || Items.Count == 0) return val_null; // ToDo: error
		Value result = Items[0];
		Items.RemoveAt(0);
		return result;
	}

	public Int32 IndexOf(Value item, Int32 afterIdx) {
		Int32 n = Count();
		for (Int32 i = afterIdx + 1; i < n; i++) {
			if (value_equal(Get(i), item)) return i;
		}
		return -1;
	}

	public void MarkChildren() {
		// For a computed list this marks [base, increment, length]; the base may
		// be a heap value (e.g. `[someList] * n`) and must be kept alive, while
		// the numeric increment/length mark as no-ops.
		if (Items == null) return;
		for (Int32 i = 0; i < Items.Count; i++) GCManager.Mark(Items[i]);
	}

	[MethodImpl(AggressiveInlining)]
	public void OnSweep() {
		Items    = null;
		Frozen   = false;
		Computed = false;
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
	public FuncDef Func;
	public Value OuterVars;

	public void MarkChildren() {
		GCManager.Mark(OuterVars);
		// Mark the function's compile-time constants so that nested-function
		// templates (and any interned strings) remain reachable.  This is what
		// roots the whole FuncDef graph now that the VM keeps no functions list.
		if (Func != null) {
			List<Value> consts = Func.Constants;
			for (Int32 i = 0; i < consts.Count; i++) GCManager.Mark(consts[i]);
			List<Value> pnames = Func.ParamNames;
			for (Int32 i = 0; i < pnames.Count; i++) GCManager.Mark(pnames[i]);
			List<Value> pdefs = Func.ParamDefaults;
			for (Int32 i = 0; i < pdefs.Count; i++) GCManager.Mark(pdefs[i]);
		}
	}

	[MethodImpl(AggressiveInlining)]
	public void OnSweep() {
		Func = null;
		OuterVars = val_null;
	}
}

// ── GCHandle ──────────────────────────────────────────────────────────────────
// A leaf GC type wrapping an arbitrary native object (void* in C++, object in C#).
// When swept, invokes Callback(UserData) so the host can free native resources.

public struct GCHandle : IGCItem {
	public object UserData;
	public HandleFinalizer Callback;

	public void MarkChildren() {
		// handles have no child Values
	}

	[MethodImpl(AggressiveInlining)]
	public void OnSweep() {
		if (Callback != null) Callback(UserData);
		UserData = null;
		Callback = null;
	}
}

}
