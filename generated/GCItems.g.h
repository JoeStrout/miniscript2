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
	public: List<Value> _order;
	public: Int32 _setterStatus;
	public: Dictionary<Value, Int32> _pos;

	// Non-null for VarMap-backed maps (call-frame locals, closure contexts).

	// Non-null for the `globals` map; then Items is null and _vmb is null.

	// Keys in insertion order, which is what a `for` loop over this map walks.
	// Items alone cannot supply that: C#'s Dictionary reuses a freed entry slot
	// on the next add, so its enumeration order changes after any Remove, while
	// the transpiled CS_Dictionary never reuses a hole until a resize compacts.
	// The two would (and did) disagree.  Holding the order here makes it exact
	// and identical on both platforms, and makes reaching the i'th entry O(1)
	// instead of a walk from the start -- see NextEntry/KeyAt/ValueAt.
	// A removed key's slot is left holding Value.Unassigned as a tombstone
	// (a poison payload no make_* ever produces, so it cannot collide with a
	// real key -- null can be a key, so null would not do).  Tombstones are
	// compacted away once they outnumber the live entries.  The live keys here
	// are exactly Items' keys, so MarkChildren has nothing extra to mark.
	// Null only for the globals view, whose order comes from the slot table.

	// Cache for "does this map's __isa chain hold any property setter?", which
	// every map member/index assignment has to answer (see SETRFIND in VM.cs).
	//   -1  this map itself holds at least one key ending in "=".  Written when
	//       such a key is stored, not discovered by a search -- which is the
	//       whole point: answering the question by scanning a map's keys would
	//       cost more than the lookup it is trying to avoid.
	//    0  nothing known, except that no setter key has ever been stored here.
	//  else the value GCManager.SetterGeneration had when this map's whole chain
	//       was last walked and found to hold no setter at all.  Equal to the
	//       current generation, it means "clean" and the walk is skipped.
	// Anything that could change the answer bumps the generation, which retires
	// every stamp in one stroke; see GCManager.NoteSetterChange.  A stale stamp
	// is therefore never wrong, only slow, and the next walk re-stamps it.
	// The -1 is deliberately sticky: removing a setter key cannot clear it
	// without scanning for other setter keys, so a map that held one keeps
	// paying for a lookup.  That is the conservative direction, and removing a
	// setter is not something programs do in a loop.

	// key -> its slot in _order, so that Remove does not have to search for it.
	// Built on a map's first removal and null before that, because most maps
	// never see one: an object, a class, or a module map is filled and then only
	// read.  Those pay nothing for this.  Once it exists it is kept in step with
	// _order.  Callers that can create it must go through GCMapSet.Remove, since
	// GCMap is a struct and the assignment would otherwise land in a copy.

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

	// ── Order maintenance ─────────────────────────────────────────────────────

	// Tombstone the slot holding key.  Called only when key was in Items, so it
	// is in _order exactly once, and _pos knows where.
	private: void OrderRemove(Value key);

	// Index the live entries of _order.  Runs once, on a map's first removal.
	private: void BuildPos();

	// Drop tombstones, preserving the order of what is left.  Compacts in place:
	// GCMap is a struct, so replacing the list reference would be lost unless
	// every caller wrote the struct back.
	private: void CompactOrder();

	// Build the order for a map whose Items were attached wholesale rather than
	// inserted one at a time (GCManager.NewMapFromDict).  Runs on a fresh slot,
	// from GCMapSet.SetItems, which writes the struct back.
	public: void SeedOrder();

	// Rebuild the order from Items when the two have drifted apart.  That can
	// only happen to a map wrapping a host-owned dictionary (GCManager
	// .NewMapFromDict shares the caller's storage), where the host may insert
	// behind our back.  The recovered order is Items' own enumeration order,
	// which for a dictionary filled and never pruned is still insertion order.
	private: void EnsureOrder();

	private: Int32 CountTombstones();

	// ── Iteration ─────────────────────────────────────────────────────────────
	// iter = -1: start
	// iter < -1: VarMap register entry -(i+2) where i is the reg-entry index
	// iter >= 0: index into _order -- a slot, not an ordinal, so tombstoned
	//            slots are simply skipped past.  For a globals map, where Items
	//            and _order are null, it is a slot index in the global table.

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
