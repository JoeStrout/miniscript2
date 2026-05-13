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
	public const Int32 StringSet = 0;
	public const Int32 ListSet = 1;
	public const Int32 MapSet = 2;
	public const Int32 ErrorSet = 3;
	public const Int32 FunctionSet = 4;

	// Typed accessors; use these to allocate new objects.
	public static GCStringSet Strings = null;
	public static GCListSet Lists = null;
	public static GCMapSet Maps = null;
	public static GCErrorSet Errors = null;
	public static GCFuncRefSet Functions = null;

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
		Strings  = new GCStringSet();
		Lists    = new GCListSet();
		Maps     = new GCMapSet();
		Errors   = new GCErrorSet();
		Functions = new GCFuncRefSet();
		_roots   = new List<Value>();
		_markCallbackFns  = new List<MarkCallback>();
		_markCallbackData = new List<object>();
	}

	// ── Value factories ──────────────────────────────────────────────────────

	public static Value NewString(String s) {
		Int32 idx = Strings.AllocItem();
		Strings.SetData(idx, s);
		return make_gc(StringSet, idx);
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
			case StringSet:  Strings.Mark(itemIdx);  break;
			case ListSet:    Lists.Mark(itemIdx);    break;
			case MapSet:     Maps.Mark(itemIdx);     break;
			case ErrorSet:   Errors.Mark(itemIdx);   break;
			case FunctionSet: Functions.Mark(itemIdx); break;
		}
	}

	// Run a full mark-sweep cycle.
	public static void CollectGarbage() {
		// 1. Clear all mark bits.
		Strings.PrepareForGC();
		Lists.PrepareForGC();
		Maps.PrepareForGC();
		Errors.PrepareForGC();
		Functions.PrepareForGC();

		// 2. Mark from explicit roots.
		for (Int32 i = 0; i < _roots.Count; i++) Mark(_roots[i]);

		// 2b. Run mark callbacks so VMs (and other providers) can mark their
		// current roots without having to add/remove them on every change.
		for (Int32 i = 0; i < _markCallbackFns.Count; i++) {
			_markCallbackFns[i](_markCallbackData[i]);
		}

		// 3. Mark retained items (and their children).
		Strings.MarkRetained();
		Lists.MarkRetained();
		Maps.MarkRetained();
		Errors.MarkRetained();
		Functions.MarkRetained();

		// 4. Sweep: free everything still unmarked.
		Strings.Sweep();
		Lists.Sweep();
		Maps.Sweep();
		Errors.Sweep();
		Functions.Sweep();
	}

	// ── Convenience accessors ─────────────────────────────────────────────────

	public static GCString  GetString(Value v)  {
		return Strings.Get(value_item_index(v));
	}
	public static GCList    GetList(Value v)    {
		return Lists.Get(value_item_index(v));
	}
	public static GCMap     GetMap(Value v)     {
		return Maps.Get(value_item_index(v));
	}
	public static GCError   GetError(Value v)   {
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
			String data = Strings.Get(value_item_index(v)).Data;
			return data != null ? data : "";
		}
		return "";
	}
}

}
