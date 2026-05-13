using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
using static MiniScript.ValueHelpers;
// H: #include "GCInterfaces.g.h"
// H: #include "GCSet.g.h"
// CPP: #include "value.h"

namespace MiniScript {

// H: typedef void (*MarkCallback)(void* userData);

//
// Central GC coordinator.  Owns the five typed GCSets and an explicit root list.
// Mark(Value) dispatches to the right GCSet using the GCSet index baked into
// the Value bits — no switch statement, just array indexing.
//
public static class GCManager {

	// GCSet indices — these constants define the encoding baked into every GC Value.
	public const Int32 BigStringSet = 0;
	public const Int32 ListSet = 1;
	public const Int32 MapSet = 2;
	public const Int32 ErrorSet = 3;
	public const Int32 FunctionSet = 4;
	public const Int32 InternedStringSet = 5;

	// Length boundary for interning: heap strings with Length < InternThreshold
	// are placed in the InternedStrings set and deduplicated via _internTable.
	// Strings of Length >= InternThreshold go into the ordinary BigStrings set.
	public const Int32 InternThreshold = 128;

	// Typed accessors; use these to allocate new objects.
	public static GCStringSet BigStrings = null;
	public static GCStringSet InternedStrings = null;
	public static GCListSet Lists = null;
	public static GCMapSet Maps = null;
	public static GCErrorSet Errors = null;
	public static GCFuncRefSet Functions = null;

	// Content-addressed intern table for short heap strings.
	// Maps string content → InternedStrings slot index.
	private static Dictionary<String, Int32> _internTable = null;

	// When true, the current GC pass is a full collection that also marks
	// and sweeps the InternedStrings set.  Normal cycles leave it untouched.
	private static Boolean _fullCollection = false;

	private static List<Value> _roots = null;

	// ── Mark callbacks ───────────────────────────────────────────────────────
	// Callback registered by a VM (or any other root provider) and invoked once
	// per CollectGarbage cycle.  The callback must call GCManager.Mark(v) on
	// every root Value it owns.
	public delegate void MarkCallback(object userData);

	private static List<MarkCallback> _markCallbackFns = null;
	private static List<object> _markCallbackData = null;

	public static void Init() {
		if (_roots != null) return;	// already initialized
		BigStrings      = new GCStringSet();
		InternedStrings = new GCStringSet();
		Lists           = new GCListSet();
		Maps            = new GCMapSet();
		Errors          = new GCErrorSet();
		Functions       = new GCFuncRefSet();
		_internTable    = new 
		  Dictionary<String, Int32>(StringComparer.Ordinal); // CPP: Dictionary<String, Int32>();
		_roots          = new List<Value>();
		_markCallbackFns  = new List<MarkCallback>();
		_markCallbackData = new List<object>();
	}

	// ── Value factories ──────────────────────────────────────────────────────

	public static Value NewString(String s) {
		Int32 idx = BigStrings.AllocItem();
		BigStrings.SetData(idx, s);
		return make_gc(BigStringSet, idx);
	}

	// Look up s in the intern table; on miss, allocate a slot in the
	// semi-immortal InternedStrings set and record the mapping.
	public static Value InternString(String s) {
		Int32 idx;
		if (_internTable.TryGetValue(s, out idx)) {
			return make_gc(InternedStringSet, idx);
		}
		idx = InternedStrings.AllocItem();
		InternedStrings.SetData(idx, s);
		_internTable[s] = idx;
		return make_gc(InternedStringSet, idx);
	}

	public static Value NewList(Int32 capacity = 8) {
		Int32 idx = Lists.AllocItem();
		Lists.Init(idx, capacity);
		return make_gc(ListSet, idx);
	}

	public static Value NewMap(Int32 capacity = 8) {
		Int32 idx = Maps.AllocItem();
		Maps.Init(idx, capacity);
		return make_gc(MapSet, idx);
	}

	public static Value NewError(Value message, Value inner, Value stack, Value isa) {
		Int32 idx = Errors.AllocItem();
		Errors.SetFields(idx, message, inner, stack, isa);
		return make_gc(ErrorSet, idx);
	}

	public static Value NewFuncRef(Int32 funcIndex, Value outerVars) {
		Int32 idx = Functions.AllocItem();
		Functions.SetFields(idx, funcIndex, outerVars);
		return make_gc(FunctionSet, idx);
	}

	// ── Retain / Release ─────────────────────────────────────────────────────

	public static void Retain(Value v) {
		if (!is_gc_object(v)) return;
		DispatchMark(value_gc_set_index(v), value_item_index(v));
	}

	public static void RetainValue(Value v) {
		if (!is_gc_object(v)) return;
		DispatchMark(value_gc_set_index(v), value_item_index(v));
	}

	// ── Root set ─────────────────────────────────────────────────────────────

	public static void AddRoot(Value v)    { _roots.Add(v); }
	public static void RemoveRoot(Value v) { _roots.Remove(v); }
	public static void ClearRoots()        { _roots.Clear(); }

	public static void RegisterMarkCallback(MarkCallback fn, object userData) {
		_markCallbackFns.Add(fn);
		_markCallbackData.Add(userData);
	}

	public static void UnregisterMarkCallback(MarkCallback fn, object userData) {
		for (Int32 i = 0; i < _markCallbackFns.Count; i++) {
			if (_markCallbackFns[i] == fn && _markCallbackData[i] == userData) {
				_markCallbackFns.RemoveAt(i);
				_markCallbackData.RemoveAt(i);
				return;
			}
		}
	}

	// ── GC cycle ─────────────────────────────────────────────────────────────

	[MethodImpl(AggressiveInlining)]
	public static void Mark(Value v) {
		if (!is_gc_object(v)) return;
		DispatchMark(value_gc_set_index(v), value_item_index(v));
	}

	private static void DispatchMark(Int32 setIdx, Int32 itemIdx) {
		switch (setIdx) {
			case BigStringSet:  BigStrings.Mark(itemIdx);  break;
			case ListSet:    Lists.Mark(itemIdx);    break;
			case MapSet:     Maps.Mark(itemIdx);     break;
			case ErrorSet:   Errors.Mark(itemIdx);   break;
			case FunctionSet: Functions.Mark(itemIdx); break;
			case InternedStringSet:
				// Skip during normal GC; interned strings are semi-immortal.
				if (_fullCollection) InternedStrings.Mark(itemIdx);
				break;
		}
	}

	// Run a full mark-sweep cycle, including the InternedStrings set.
	// Interned strings unreachable from roots (and not retained) are removed
	// from the intern table and then swept.  Use this for explicit resets,
	// memory-pressure events, or VM teardown.
	public static void FullCollectGarbage() {
		CollectGarbageInternal(true);
	}

	// Run a full mark-sweep cycle.
	public static void CollectGarbage() {
		CollectGarbageInternal(false);
	}

	private static void CollectGarbageInternal(Boolean includeInterned) {
		_fullCollection = includeInterned;

		// 1. Clear all mark bits.
		BigStrings.PrepareForGC();
		Lists.PrepareForGC();
		Maps.PrepareForGC();
		Errors.PrepareForGC();
		Functions.PrepareForGC();
		if (includeInterned) InternedStrings.PrepareForGC();

		// 2. Mark from explicit roots.
		for (Int32 i = 0; i < _roots.Count; i++) Mark(_roots[i]);

		// 2b. Run mark callbacks so VMs (and other providers) can mark their
		// current roots without having to add/remove them on every change.
		for (Int32 i = 0; i < _markCallbackFns.Count; i++) {
			_markCallbackFns[i](_markCallbackData[i]);
		}

		// 3. Mark retained items (and their children).
		BigStrings.MarkRetained();
		Lists.MarkRetained();
		Maps.MarkRetained();
		Errors.MarkRetained();
		Functions.MarkRetained();
		if (includeInterned) InternedStrings.MarkRetained();

		// 4. Sweep: free everything still unmarked.
		BigStrings.Sweep();
		Lists.Sweep();
		Maps.Sweep();
		Errors.Sweep();
		Functions.Sweep();

		// 5. Full-GC only: remove dead intern-table entries, then sweep.
		// The table is keyed by string content, so we must purge its
		// entries before InternedStrings.Sweep() clears the .Data fields.
		if (includeInterned) SweepInternTable();
	}

	private static void SweepInternTable() {
		List<String> dead = new List<String>();
		foreach (KeyValuePair<String, Int32> kvp in _internTable) { // CPP: for (String key : _internTable.Keys()) {
			String key = kvp.Key;		// CPP:
			Int32 slot = kvp.Value;		// CPP: Int32 slot = _internTable[key];
			if (!InternedStrings.IsLiveSlot(slot)) dead.Add(key);
		}
		for (Int32 i = 0; i < dead.Count; i++) _internTable.Remove(dead[i]);
		InternedStrings.Sweep();
	}

	// ── Convenience accessors ─────────────────────────────────────────────────

	public static GCString GetString(Value v) {
		if (value_gc_set_index(v) == InternedStringSet) {
			return InternedStrings.Get(value_item_index(v));
		}
		return BigStrings.Get(value_item_index(v));
	}
	public static GCList GetList(Value v) {
		return Lists.Get(value_item_index(v));
	}
	public static GCMap GetMap(Value v) {
		return Maps.Get(value_item_index(v));
	}
	public static GCError GetError(Value v) {
		return Errors.Get(value_item_index(v));
	}
	public static GCFunction GetFuncRef(Value v) {
		return Functions.Get(value_item_index(v));
	}

	// ── Static helper for content-based string access ─────────────────────────
	// Used by GCMap for content-based key hashing and equality.
	// (Or is it?  ToDo: see if this is still needed.)
	
	[MethodImpl(AggressiveInlining)]
	public static String GetStringContent(Value v) {
		if (is_tiny_string(v)) {
			Int32 len = value_tiny_len(v);
			char[] chars = new Char[len];
			for (Int32 i = 0; i < len; i++) chars[i] = (char)((value_bits(v) >> (8 * (i + 1))) & 0xFF);
			return new String(chars);
		}
		if (is_heap_string(v)) {
			GCStringSet set;
			set = (value_gc_set_index(v) == InternedStringSet) ? InternedStrings : BigStrings;
			String data = set.Get(value_item_index(v)).Data;
			return data != null ? data : "";
		}
		return "";
	}
}

}
