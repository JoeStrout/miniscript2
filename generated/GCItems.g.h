// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: GCItems.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
#include "GCInterfaces.g.h"
#include "value.h"
#include "VarMap.g.h"
#include "Globals.g.h"
#include "FuncDef.g.h"
#include "CS_Math.h"

namespace MiniScript {

// DECLARATIONS

// ── GCString ─────────────────────────────────────────────────────────────────

struct GCString {
	public: String Data;

	public: void MarkChildren();

	public: void OnSweep();
}; // end of struct GCString

// ── GCList ───────────────────────────────────────────────────────────────────

struct GCList {
	public: List<Value> Items;
	public: Boolean Frozen;
	public: Boolean Computed;
	// Items is public (paralleling GCMap.Items) so host bridges such as
	// Value::GetList() can reach the backing list, but treat it with care: a list
	// may be either "materialized" (Computed == false, Items holds the actual
	// elements) or "computed" (Computed == true, Items holds exactly three
	// meta-values: [0]=base, [1]=increment, [2]=length).  Prefer the methods
	// below, which hide the difference; a computed list with a null increment
	// repeats the base value (used for `[x] * n`), otherwise element i is
	// base + increment * i.  Call Materialize() first if you need the real
	// elements from a possibly-computed list.
	// IMPORTANT: a mutation may materialize a computed list, which replaces the
	// Items reference and clears the Computed flag.  Since GCList is a struct,
	// callers MUST write the value back (GCManager.Lists.Set / SetFrozen style)
	// after any operation that can mutate, or the change is lost.

	public: void Init(Int32 capacity = 8);

	// Construct a computed list.  increment may be Value.Null to repeat baseVal.
	public: void InitComputed(Value baseVal, Value increment, Int32 length);

	// Replace a computed list with the equivalent materialized list.  No-op for
	// an already-materialized list.  Mutates this struct; caller must write back.
	public: void Materialize();

	public: Int32 Count();

	public: void Push(Value v);

	public: Value Get(Int32 i);

	public: void Set(Int32 i, Value v);

	public: Boolean Remove(Int32 index);

	public: void Insert(Int32 index, Value v);

	public: Value Pop();

	public: Value Pull();

	public: Int32 IndexOf(Value item, Int32 afterIdx);

	public: void MarkChildren();

	public: void OnSweep();
}; // end of struct GCList

// ── GCMap ─────────────────────────────────────────────────────────────────────
// A simple wrapper around Dictionary<Value, Value>, plus a Frozen flag and an
// optional backing. Hash/equality of keys follows MiniScript == semantics via
// Value.Equals/GetHashCode (see Value.cs).
// A map has at most one backing, and the two kinds are mutually exclusive:
//   _vmb  a call frame's locals: name -> register bindings layered OVER Items,
//         so a key resolves to a register if bound and to a hash entry if not.
//   _gb   the `globals` table: the SOLE storage, with Items left null.  There
//         is no second tier and nothing to keep in sync.  See cs/Globals.cs.
// These are two fields rather than one polymorphic backing on purpose:
// cs/GCInterfaces.cs explains why GC-managed types here avoid vtables (a
// static-constructor-written vtable pointer can be zero in BSS on some
// platforms, segfaulting on the first virtual call).  A null check is also
// cheaper than virtual dispatch, and these paths are not the hot ones -- a
// frame's locals are normally reached as registers, never through this map.

struct GCMap {
	public: Dictionary<Value, Value> Items;
	public: Boolean Frozen;
	public: VarMapBacking _vmb;
	public: Globals _gb;

	// Non-null for VarMap-backed maps (call-frame locals, closure contexts).

	// Non-null for the `globals` map; then Items is null and _vmb is null.

	public: Int32 Count();

	public: void Init(Int32 capacity = 8);

	// Initialize this slot as the view onto a global slot table.  Items stays
	// null: the table is the storage, so an empty dictionary would be dead
	// weight that Count/iteration would then have to skip past.
	public: void InitAsGlobals(Globals g);

	public: Boolean TryGet(Value key, Value* value);

	public: void Set(Value key, Value value);

	public: Boolean Remove(Value key);

	public: Boolean HasKey(Value key);

	public: void Clear();

	// ── Iteration ─────────────────────────────────────────────────────────────
	// iter = -1: start
	// iter < -1: VarMap register entry -(i+2) where i is the reg-entry index
	// iter >= 0: index into Items (in enumeration order), or -- for a globals
	//            map, where Items is null -- a slot index in the global table

	public: Int32 NextEntry(Int32 after);

	public: Value KeyAt(Int32 i);

	public: Value ValueAt(Int32 i);

	// ── GC ────────────────────────────────────────────────────────────────────

	public: void MarkChildren();

	public: void OnSweep();
}; // end of struct GCMap

// ── GCError ──────────────────────────────────────────────────────────────────

struct GCError {
	public: Value Message;
	public: Value Inner;
	public: Value Stack;
	public: Value Isa;

	public: void MarkChildren();

	public: void OnSweep();
}; // end of struct GCError

// ── GCFunction ────────────────────────────────────────────────────────────────

struct GCFunction {
	public: FuncDef Func;
	public: Value OuterVars;

	public: void MarkChildren();

	public: void OnSweep();
}; // end of struct GCFunction

// ── GCHandle ──────────────────────────────────────────────────────────────────
// A leaf GC type wrapping an arbitrary native object (void* in C++, object in C#).
// When swept, invokes Callback(UserData) so the host can free native resources.

struct GCHandle {
	public: object UserData;
	public: HandleFinalizer Callback;

	public: void MarkChildren();

	public: void OnSweep();
}; // end of struct GCHandle

// INLINE METHODS

inline void GCList::Init(Int32 capacity ) {
	Items    =  List<Value>::New(Math::Max(capacity, 4));
	Frozen   = Boolean(false);
	Computed = Boolean(false);
}
inline Int32 GCList::Count() {
	if (Computed) return (Int32)Items[2].NumericVal();
	return (IsNull(Items)) ? 0 : Items.Count();
}
inline void GCList::Push(Value v) {
	if (Computed) Materialize();
	if (IsNull(Items)) Init();
	Items.Add(v);
}
inline Value GCList::Get(Int32 i) {
	if (Computed) {
		Int32 len = (Int32)Items[2].NumericVal();
		if (i < 0) i += len;
		if ((UInt32)i >= (UInt32)len) return Value::Null;
		Value incr = Items[1];
		if (incr.IsNull()) return Items[0];
		Double d = Items[0].NumericVal() + incr.NumericVal() * i;
		return Value(d);
	}
	if (i < 0) i += Items.Count();
	return (UInt32)i < (UInt32)Items.Count() ? Items[i] : Value::Null;
}
inline void GCList::Set(Int32 i,Value v) {
	if (Computed) Materialize();
	if (i < 0) i += Items.Count();
	if ((UInt32)i < (UInt32)Items.Count()) Items[i] = v;
}
inline Boolean GCList::Remove(Int32 index) {
	if (Computed) Materialize();
	if (IsNull(Items)) Init();
	if (index < 0) index += Items.Count();
	if (index < 0 || index >= Items.Count()) return Boolean(false);
	Items.RemoveAt(index);
	return Boolean(true);
}
inline void GCList::OnSweep() {
	Items    = nullptr;
	Frozen   = Boolean(false);
	Computed = Boolean(false);
}

inline Boolean GCMap::HasKey(Value key) {
	Value ignored;
	return TryGet(key, &ignored);
}

inline void GCError::OnSweep() {
	Message = Value::Null;
	Inner   = Value::Null;
	Stack   = Value::Null;
	Isa     = Value::Null;
}

inline void GCFunction::OnSweep() {
	Func = nullptr;
	OuterVars = Value::Null;
}

inline void GCHandle::OnSweep() {
	if (!IsNull(Callback)) Callback(UserData);
	UserData = nullptr;
	Callback = nullptr;
}

} // end of namespace MiniScript
