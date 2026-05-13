// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: GCManager.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
#include "GCInterfaces.g.h"
#include "GCSet.g.h"

namespace MiniScript {
typedef void (*MarkCallback)(void* userData);

// DECLARATIONS

// Central GC coordinator.  Owns the five typed GCSets and an explicit root list.
// Mark(Value) dispatches to the right GCSet using the GCSet index baked into
// the Value bits — no switch statement, just array indexing.
class GCManager {
	public: static const Int32 BigStringSet;
	public: static const Int32 ListSet;
	public: static const Int32 MapSet;
	public: static const Int32 ErrorSet;
	public: static const Int32 FunctionSet;
	public: static const Int32 InternedStringSet;
	public: static const Int32 InternThreshold;
	public: static GCStringSet BigStrings;
	public: static GCStringSet InternedStrings;
	public: static GCListSet Lists;
	public: static GCMapSet Maps;
	public: static GCErrorSet Errors;
	public: static GCFuncRefSet Functions;
	private: static Dictionary<String, Int32> _internTable;
	private: static Boolean _fullCollection;
	private: static List<Value> _roots;
	private: static List<MarkCallback> _markCallbackFns;
	private: static List<object> _markCallbackData;

	// GCSet indices — these constants define the encoding baked into every GC Value.

	// Length boundary for interning: heap strings with Length < InternThreshold
	// are placed in the InternedStrings set and deduplicated via _internTable.
	// Strings of Length >= InternThreshold go into the ordinary BigStrings set.

	// Typed accessors; use these to allocate new objects.

	// Content-addressed intern table for short heap strings.
	// Maps string content → InternedStrings slot index.

	// When true, the current GC pass is a full collection that also marks
	// and sweeps the InternedStrings set.  Normal cycles leave it untouched.

	// ── Mark callbacks ───────────────────────────────────────────────────────
	// Callback registered by a VM (or any other root provider) and invoked once
	// per CollectGarbage cycle.  The callback must call GCManager.Mark(v) on
	// every root Value it owns.

	public: static void Init();

	// ── Value factories ──────────────────────────────────────────────────────

	public: static Value NewString(String s);

	// Look up s in the intern table; on miss, allocate a slot in the
	// semi-immortal InternedStrings set and record the mapping.
	public: static Value InternString(String s);

	public: static Value NewList(Int32 capacity = 8);

	public: static Value NewMap(Int32 capacity = 8);

	public: static Value NewError(Value message, Value inner, Value stack, Value isa);

	public: static Value NewFuncRef(Int32 funcIndex, Value outerVars);

	// ── Retain / Release ─────────────────────────────────────────────────────

	public: static void Retain(Value v);

	public: static void RetainValue(Value v);

	// ── Root set ─────────────────────────────────────────────────────────────

	public: static void RegisterMarkCallback(MarkCallback fn, object userData);

	public: static void UnregisterMarkCallback(MarkCallback fn, object userData);

	// ── GC cycle ─────────────────────────────────────────────────────────────

	public: static void Mark(Value v);

	private: static void DispatchMark(Int32 setIdx, Int32 itemIdx);

	// Run a full mark-sweep cycle, including the InternedStrings set.
	// Interned strings unreachable from roots (and not retained) are removed
	// from the intern table and then swept.  Use this for explicit resets,
	// memory-pressure events, or VM teardown.
	public: static void FullCollectGarbage();

	// Run a full mark-sweep cycle.
	public: static void CollectGarbage();

	private: static void CollectGarbageInternal(Boolean includeInterned);

	private: static void SweepInternTable();

	// ── Convenience accessors ─────────────────────────────────────────────────

	public: static GCString GetString(Value v);
	public: static GCList GetList(Value v);
	public: static GCMap GetMap(Value v);
	public: static GCError GetError(Value v);
	public: static GCFunction GetFuncRef(Value v);

	// ── Static helper for content-based string access ─────────────────────────
	// Used by GCMap for content-based key hashing and equality.
	// (Or is it?  ToDo: see if this is still needed.)
	
	public: static String GetStringContent(Value v);
}; // end of struct GCManager

// INLINE METHODS

inline void GCManager::Mark(Value v) {
	if (!is_gc_object(v)) return;
	DispatchMark(value_gc_set_index(v), value_item_index(v));
}
inline String GCManager::GetStringContent(Value v) {
	if (is_tiny_string(v)) {
		Int32 len = value_tiny_len(v);
		char chars[len];
		for (Int32 i = 0; i < len; i++) chars[i] = (char)((value_bits(v) >> (8 * (i + 1))) & 0xFF);
		return  String::New(chars);
	}
	if (is_heap_string(v)) {
		GCStringSet set;
		set = (value_gc_set_index(v) == InternedStringSet) ? InternedStrings : BigStrings;
		String data = set.Get(value_item_index(v)).Data;
		return !IsNull(data) ? data : "";
	}
	return "";
}

} // end of namespace MiniScript
