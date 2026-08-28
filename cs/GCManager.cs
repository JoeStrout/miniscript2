using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
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
	public const Int32 HandleSet = 6;

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
	public static GCHandleSet Handles = null;

	// Content-addressed intern table for short heap strings.
	// Maps string content → InternedStrings slot index.
	private static Dictionary<String, Int32> _internTable = null;


	// When true, the current GC pass is a full collection that also marks
	// and sweeps the InternedStrings set.  Normal cycles leave it untouched.
	private static Boolean _fullCollection = false;

	// ── MaybeCollect tuning ──────────────────────────────────────────────────
	// All intervals are counted in ticks, where a tick is one MaybeCollect(false)
	// call.  GCManager has no notion of a frame; the caller defines the tick by
	// how often it calls, and a host game loop calling once per frame makes the
	// defaults below read as "60 frames" etc.  Public so a host (or a future
	// intrinsic) can retune them without a rebuild.

	// No live handles, ordinary tick: collect about once per second at 60fps.
	public static Int32 GCIntervalTicks = 60;

	// Floor for an ordinary tick.  However many handles are live, we never
	// collect more often than this -- a busy handle churn must not turn into a
	// mark-sweep every other frame.
	public static Int32 GCMinIntervalTicks = 30;

	// The same two numbers for an "encouraged" call -- a moment the caller knows
	// is a good one (an explicit yield, the start of a wait), where a collection
	// is cheap in wall-clock terms because we were about to be idle anyway.
	public static Int32 GCEncouragedIntervalTicks = 20;
	public static Int32 GCEncouragedMinIntervalTicks = 5;

	// Live-handle count at which the interval reaches its floor.  Handles are
	// the objects with finalizers -- file descriptors, GPU resources, host
	// buffers -- and their cost is not their memory footprint, so their number
	// (not bytes allocated) is what pulls collection earlier.
	public static Int32 GCHandlesForMinInterval = 64;

	// Ticks since the last collection.  Only an ordinary (non-encouraged) call
	// advances this, so it stays an honest frame count even when a script yields
	// several times per frame.
	private static Int32 _ticksSinceCollect = 0;

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
		Handles         = new GCHandleSet();
		_internTable    = new Dictionary<String, Int32>(StringComparer.Ordinal); // CPP: _internTable    = Dictionary<String, Int32>();
		_roots          = new List<Value>();
		_markCallbackFns  = new List<MarkCallback>();
		_markCallbackData = new List<object>();

		// Install the unassigned-location sentinel.  The Value itself belongs to
		// the value layer (Value.Unassigned, see cs/Value.cs) so that
		// IsUnassigned() has no dependency on this class; we are only the ones
		// who can allocate a funcref and root it.  A bare FuncDef is all we can
		// build without reaching into the VM layer -- Globals installs the
		// reporting callback on it.
		FuncDef unassignedFunc = new FuncDef();
		unassignedFunc.Name = "<unassigned>";
		Value.Unassigned = NewFuncRef(unassignedFunc, Value.Null);
		AddRoot(Value.Unassigned);
	}

	// ── Value factories ──────────────────────────────────────────────────────

	public static Value NewString(String s) {
		Int32 idx = BigStrings.AllocItem();
		BigStrings.SetData(idx, s);
		return Value.make_gc(BigStringSet, idx);
	}

	// Look up s in the intern table; on miss, allocate a slot in the
	// semi-immortal InternedStrings set and record the mapping.
	public static Value InternString(String s) {
		Int32 idx;
		if (_internTable.TryGetValue(s, out idx)) {
			return Value.make_gc(InternedStringSet, idx);
		}
		idx = InternedStrings.AllocItem();
		InternedStrings.SetData(idx, s);
		_internTable[s] = idx;
		return Value.make_gc(InternedStringSet, idx);
	}

	public static Value NewList(Int32 capacity = 8) {
		Int32 idx = Lists.AllocItem();
		Lists.Init(idx, capacity);
		return Value.make_gc(ListSet, idx);
	}

	// Create a computed list: element i is baseVal + increment * i, for `length`
	// elements.  Pass increment = Value.Null to repeat baseVal (for `[x] * n`).
	public static Value NewComputedList(Value baseVal, Value increment, Int32 length) {
		Int32 idx = Lists.AllocItem();
		GCList item = Lists.Get(idx);
		item.InitComputed(baseVal, increment, length);
		Lists.Set(idx, item);
		return Value.make_gc(ListSet, idx);
	}

	public static Value NewMap(Int32 capacity = 8) {
		Int32 idx = Maps.AllocItem();
		Maps.Init(idx, capacity);
		return Value.make_gc(MapSet, idx);
	}

	// Wrap an existing dictionary as a map Value, sharing its storage rather
	// than copying entries.  The resulting map and the source dictionary refer
	// to the same underlying table, so later mutations to either are visible
	// through the other (matching MiniScript 1.x host semantics).
	public static Value NewMapFromDict(Dictionary<Value, Value> items) {
		Int32 idx = Maps.AllocItem();
		Maps.SetItems(idx, items);
		return Value.make_gc(MapSet, idx);
	}

	// The `globals` map: a map whose entire storage is the given global slot
	// table.  Unlike a VarMap-backed map there is no second tier -- Items stays
	// null, and every key lives in a slot -- so there is nothing to gather,
	// rebind, or keep in sync.  See cs/Globals.cs and notes/GLOBALS.md.
	public static Value NewGlobalsMap(Globals g) {
		Int32 idx = Maps.AllocItem();
		Maps.InitAsGlobals(idx, g);
		return Value.make_gc(MapSet, idx);
	}

	public static Value NewError(Value message, Value inner, Value stack, Value isa) {
		Int32 idx = Errors.AllocItem();
		Errors.SetFields(idx, message, inner, stack, isa);
		return Value.make_gc(ErrorSet, idx);
	}

	public static Value NewFuncRef(FuncDef func, Value outerVars) {
		Int32 idx = Functions.AllocItem();
		Functions.SetFields(idx, func, outerVars);
		return Value.make_gc(FunctionSet, idx);
	}

	public static Value NewHandle(object userData, HandleFinalizer callback) {
		Int32 idx = Handles.AllocItem();
		Handles.SetFields(idx, userData, callback);
		return Value.make_gc(HandleSet, idx);
	}

	// ── Retain / Release ─────────────────────────────────────────────────────

	public static void Retain(Value v) {
		if (!v.IsGCObject()) return;
		DispatchMark(v.GCSetIndex(), v.ItemIndex());
	}

	public static void RetainValue(Value v) {
		if (!v.IsGCObject()) return;
		DispatchMark(v.GCSetIndex(), v.ItemIndex());
	}

	// ── Root set ─────────────────────────────────────────────────────────────

	public static void AddRoot(Value v) {
		_roots.Add(v);
	}
	public static void RemoveRoot(Value v) {
		_roots.Remove(v);
	}
	public static void ClearRoots() {
		_roots.Clear();
	}

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
		if (!v.IsGCObject()) return;
		DispatchMark(v.GCSetIndex(), v.ItemIndex());
	}

	private static void DispatchMark(Int32 setIdx, Int32 itemIdx) {
		switch (setIdx) {
			case BigStringSet:  BigStrings.Mark(itemIdx);  break;
			case ListSet:    Lists.Mark(itemIdx);    break;
			case MapSet:     Maps.Mark(itemIdx);     break;
			case ErrorSet:   Errors.Mark(itemIdx);   break;
			case FunctionSet: Functions.Mark(itemIdx); break;
			case HandleSet:   Handles.Mark(itemIdx);   break;
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

	// Collect, but only if enough has happened since the last time to be worth
	// it.  Safe to call often; the host is expected to call it once per frame.
	//
	// Mark-sweep costs O(live set) rather than O(garbage), so an unconditional
	// collection every frame is a tax proportional to heap size, paid whether or
	// not there is anything to reclaim.  Hence the interval.
	//
	// `encouraged` says the caller is at a known-good moment -- an explicit
	// yield, or the start of a wait -- where the pause is hidden by idleness
	// that was going to happen anyway.  Such calls use the lower thresholds and
	// so get first crack at collecting; the ordinary per-frame call is the
	// fallback that guarantees collection happens even in a script that never
	// yields.
	//
	// Every caller must be at a VM safe point.  VM.MarkRoots scans the register
	// stack (including the current frame), every call frame, globals, and
	// pending-call results, so being inside an intrinsic is fine on its own --
	// `yield` and `wait` both call here.  What is NOT safe is holding a GC Value
	// in a host/C++ local that is not also reachable from those roots, since
	// nothing will mark it.  A resumable intrinsic should also call only on its
	// fresh-call branch; a continuation runs on every VM step, and collecting
	// from there would fire far more often than the interval implies.
	public static void MaybeCollect(Boolean encouraged = false) {
		if (!encouraged) _ticksSinceCollect++;

		Int32 interval    = encouraged ? GCEncouragedIntervalTicks : GCIntervalTicks;
		Int32 minInterval = encouraged ? GCEncouragedMinIntervalTicks : GCMinIntervalTicks;

		// Live handles scale the interval down from `interval` to `minInterval`,
		// reaching the floor at GCHandlesForMinInterval.  LiveCount() is an O(1)
		// running tally (see GCSetBase), so this costs nothing per tick.
		Int32 handles = Handles.LiveCount();
		if (handles > 0 && interval > minInterval) {
			if (handles >= GCHandlesForMinInterval) {
				interval = minInterval;
			} else {
				interval = interval - ((interval - minInterval) * handles) / GCHandlesForMinInterval;
			}
		}

		if (_ticksSinceCollect < interval) return;
		CollectGarbage();		// resets _ticksSinceCollect
	}

	// Run a full mark-sweep cycle.
	public static void CollectGarbage() {
		CollectGarbageInternal(false);
	}

	private static void CollectGarbageInternal(Boolean includeInterned) {
		_fullCollection = includeInterned;

		// Any collection restarts MaybeCollect's clock, so an explicit
		// gc.collect is not immediately followed by an automatic one.
		_ticksSinceCollect = 0;

		// 1. Clear all mark bits.
		BigStrings.PrepareForGC();
		Lists.PrepareForGC();
		Maps.PrepareForGC();
		Errors.PrepareForGC();
		Functions.PrepareForGC();
		Handles.PrepareForGC();
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
		Handles.MarkRetained();
		if (includeInterned) InternedStrings.MarkRetained();

		// 4. Sweep: free everything still unmarked.
		BigStrings.Sweep();
		Lists.Sweep();
		Maps.Sweep();
		Errors.Sweep();
		Functions.Sweep();
		Handles.Sweep();

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
		if (v.GCSetIndex() == InternedStringSet) {
			return InternedStrings.Get(v.ItemIndex());
		}
		return BigStrings.Get(v.ItemIndex());
	}
	public static GCList GetList(Value v) {
		return Lists.Get(v.ItemIndex());
	}
	public static GCMap GetMap(Value v) {
		return Maps.Get(v.ItemIndex());
	}
	public static GCError GetError(Value v) {
		return Errors.Get(v.ItemIndex());
	}
	public static GCFunction GetFuncRef(Value v) {
		return Functions.Get(v.ItemIndex());
	}
	public static GCHandle GetHandle(Value v) {
		return Handles.Get(v.ItemIndex());
	}

	//*** BEGIN CS_ONLY ***
	// ── Static helper for content-based string access ─────────────────────────
	// Used by GCMap for content-based key hashing and equality.
	// (Or is it?  ToDo: see if this is still needed.)
	
	[MethodImpl(AggressiveInlining)]
	public static String GetStringContent(Value v) {
		if (v.IsTinyString()) {
			Int32 len = v.TinyLen();
			char[] chars = new Char[len];
			for (Int32 i = 0; i < len; i++) chars[i] = (char)((v.Bits() >> (8 * (i + 1))) & 0xFF);
			return new String(chars);
		}
		if (v.IsHeapString()) {
			GCStringSet set;
			set = (v.GCSetIndex() == InternedStringSet) ? InternedStrings : BigStrings;
			String data = set.Get(v.ItemIndex()).Data;
			return data != null ? data : "";
		}
		return "";
	}
	//*** END CS_ONLY ***
}

}
