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
	public: static const Int32 StringSet;
	public: static const Int32 ListSet;
	public: static const Int32 MapSet;
	public: static const Int32 ErrorSet;
	public: static const Int32 FunctionSet;
	public: static GCStringSet Strings;
	public: static GCListSet Lists;
	public: static GCMapSet Maps;
	public: static GCErrorSet Errors;
	public: static GCFuncRefSet Functions;
	private: static List<Value> _roots;
	private: static List<MarkCallback> _markCallbackFns;
	private: static List<object> _markCallbackData;

	// GCSet indices — these constants define the encoding baked into every GC Value.

	// Typed accessors; use these to allocate new objects.

	// ── Mark callbacks ───────────────────────────────────────────────────────
	// Callback registered by a VM (or any other root provider) and invoked once
	// per CollectGarbage cycle.  The callback must call GCManager.Mark(v) on
	// every root Value it owns.

	public: static void Init();

	// ── Value factories ──────────────────────────────────────────────────────

	public: static Value NewString(String s);

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

	// Run a full mark-sweep cycle.
	public: static void CollectGarbage();

	// ── Convenience accessors ─────────────────────────────────────────────────

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
		String data = Strings.Get(value_item_index(v)).Data;
		return !IsNull(data) ? data : "";
	}
	return "";
}

} // end of namespace MiniScript
