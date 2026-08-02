// Globals.cs
//
// The global namespace: a first-class object with its own storage, owned by an
// Interpreter rather than by any compiled program.  See notes/GLOBALS.md for
// the reasoning; the short version is that globals are NOT a call frame.  They
// outlive every compilation (each REPL line is a new @main), they grow at run
// time (`globals.foo = 42` from ten frames down), and there must be exactly one
// place a given global can live -- otherwise "where is it?" depends on whether
// the current @main happened to mention its name, which is how the old
// register-window-plus-hash-table arrangement leaked compilation artifacts into
// semantics.
//
// A Globals is a flat table of *slots*:
//
//     slot     0        1        2        3
//     _names   "x"      "print"  "total"  "i"
//     _values  42       <fn>     Unassign 7
//
// plus a name -> slot index.  Two properties make the whole design work:
//
//   * Slots are STABLE.  Once "foo" is slot 37 it is slot 37 for the life of
//     this object, even if removed and re-added.  Anything may therefore cache
//     a slot index -- which is what the planned GETGLOBAL/SETGLOBAL opcodes do
//     (stage 4), turning a global access into an array index.
//   * The table GROWS.  A new name appends.  No ceiling, no MaxRegs, nothing to
//     gather or rebind when a new program is compiled.
//
// A slot whose name is not currently bound holds Value.Unassigned, which is
// distinct from null -- null is a perfectly good value for a global to hold.
//
// This class doubles as the backing for the `globals` map (GCMap._gb), so the
// map view and compiled code reach the very same slots.  That is what makes
// `globals.foo = 42` and a top-level `foo = 42` the same operation by
// construction rather than by synchronization.

using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
// H: #include "value.h"
// H: #include "FuncDef.g.h"
// CPP: #include "GCManager.g.h"
// CPP: #include "IntrinsicAPI.g.h"
// CPP: #include "VM.g.h"
//
// Context and IntrinsicResult are only named inside a method body (the sentinel
// callback), and FuncDef.g.h already forward-declares both, so those two stay
// out of the header -- GCItems.g.h includes this one, and dragging the VM in
// there would be a cycle.
//
// That cycle is also what decides which methods here may be inlined: an inlined
// body lands in Globals.g.h, so it may only touch things visible there.  The
// slot primitives qualify (they need nothing beyond value.h, now that the
// Unassigned sentinel lives on Value rather than on GCManager); anything
// calling GCManager -- Create, AttachMap, Release, MarkChildren -- must stay
// out of line.

namespace MiniScript {

public class Globals {

	private List<Value> _names;               // slot -> name (set once, never cleared)
	private List<Value> _values;              // slot -> value, or Value.Unassigned
	private Dictionary<Value, Int32> _index;  // name -> slot
	private Int32 _id;                        // generation; see Id()
	private Int32 _assignedCount;             // slots currently holding a value
	private Value _mapValue;                  // the `globals` map viewing this table

	// Generation counter.  Every Globals gets a distinct Id, and an Id is never
	// reused.  Compiled code caches (name -> slot) resolutions and validates
	// them with a single integer compare against this; adding a slot never
	// invalidates an existing one, so the only thing that changes an Id is
	// building a whole new namespace.  See notes/GLOBALS.md section 4.3.
	//
	// Pre-incremented, so the first Globals gets Id 1 and 0 is never a valid Id
	// -- which lets a not-yet-resolved slot cache use 0 as its "never resolved"
	// marker.
	private static Int32 _lastId = 0;

	//
	// Make a new, empty global namespace.  Use this rather than `new Globals()`
	// directly: a Globals is paired 1:1 with the map that views it, and only a
	// fully constructed one is usable.
	//
	// (The pairing is done out here, from a local, rather than in the
	// constructor.  Handing `this` to something that wants a Globals would mean
	// converting a storage pointer back to a wrapper on the C++ side, which the
	// transpiler does not do; a local of wrapper type passes straight through.)
	//
	public static Globals Create() {
		Globals g = new Globals();
		g.AttachMap(GCManager.NewGlobalsMap(g));
		return g;
	}

	// Every field is assigned here rather than at its declaration: the
	// transpiler drops field initializers too, so anything not set in the
	// constructor would be left default-constructed on the C++ side.
	public Globals() {
		EnsureUnassignedSentinel();
		_names         = new List<Value>();
		_values        = new List<Value>();
		_index         = new Dictionary<Value, Int32>();
		_assignedCount = 0;
		_mapValue      = Value.Null;
		_lastId++;
		_id            = _lastId;
	}

	// Adopt the map that views this table, and root it.  The map holds a
	// reference back here (GCMap._gb), so if it were swept the table would be
	// orphaned from the map's side.  Release() drops the root.
	// TODO(stage 3): the owning VM should mark this instead, so that discarding
	// a Globals (as the `reset` intrinsic will) reclaims it.
	public void AttachMap(Value mapValue) {
		_mapValue = mapValue;
		GCManager.AddRoot(_mapValue);
	}

	// Drop the GC root on the map view.  After this the Globals must not be
	// used again unless something else keeps the map alive.
	public void Release() {
		if (!_mapValue.IsNull()) {
			GCManager.RemoveRoot(_mapValue);
			_mapValue = Value.Null;
		}
	}

	// ── Identity and size ─────────────────────────────────────────────────────

	[MethodImpl(AggressiveInlining)]
	public Int32 Id() {
		return _id;
	}

	// The `globals` map: an ordinary MiniScript map whose entire storage is this
	// table.  There is no second tier -- every entry is a slot.
	[MethodImpl(AggressiveInlining)]
	public Value AsMap() {
		return _mapValue;
	}

	// Total slots ever created, including unassigned ones.  This is the bound
	// for slot iteration, NOT the number of globals; use Count() for that.
	[MethodImpl(AggressiveInlining)]
	public Int32 SlotCount() {
		return _names.Count;
	}

	// Number of globals currently bound (what `globals.len` reports).
	[MethodImpl(AggressiveInlining)]
	public Int32 Count() {
		return _assignedCount;
	}

	// ── Slot access ───────────────────────────────────────────────────────────

	// The slot for the given name, creating one if this name has never been
	// seen.  A newly created slot is Unassigned, so resolving a name does not
	// bring it into existence as a global -- reading it still fails.  This is
	// what lets compiled code resolve all of a function's global references up
	// front, including names it may never reach.
	public Int32 Resolve(Value name) {
		Int32 slot;
		if (_index.TryGetValue(name, out slot)) return slot;
		slot = _names.Count;
		_names.Add(name);
		_values.Add(Value.Unassigned);
		_index[name] = slot;
		return slot;
	}

	// The slot for the given name, or -1 if the name has never been seen.
	// Does not create anything.
	[MethodImpl(AggressiveInlining)]
	public Int32 Find(Value name) {
		Int32 slot;
		if (_index.TryGetValue(name, out slot)) return slot;
		return -1;
	}

	// The value in a slot, or Unassigned.  Callers on the read path must test
	// IsUnassigned() and treat it as "not found" -- it must never be handed to
	// user code.
	[MethodImpl(AggressiveInlining)]
	public Value ValueAtSlot(Int32 slot) {
		if (slot < 0 || slot >= _values.Count) return Value.Unassigned;
		return _values[slot];
	}

	// The name a slot is bound to.  Always valid for a slot that exists, even
	// when the slot is unassigned (names are set once and never cleared).
	[MethodImpl(AggressiveInlining)]
	public Value NameAtSlot(Int32 slot) {
		if (slot < 0 || slot >= _names.Count) return Value.Null;
		return _names[slot];
	}

	// Store into a slot, keeping the assigned-count in step.  Storing
	// Unassigned here is how a global is removed.
	[MethodImpl(AggressiveInlining)]
	public void SetSlot(Int32 slot, Value value) {
		if (slot < 0 || slot >= _values.Count) return;
		Boolean wasAssigned = !_values[slot].IsUnassigned();
		Boolean nowAssigned = !value.IsUnassigned();
		_values[slot] = value;
		if (wasAssigned && !nowAssigned) _assignedCount--;
		if (!wasAssigned && nowAssigned) _assignedCount++;
	}

	// True if the slot exists and currently holds a value.
	[MethodImpl(AggressiveInlining)]
	public Boolean SlotIsAssigned(Int32 slot) {
		if (slot < 0 || slot >= _values.Count) return false;
		return !_values[slot].IsUnassigned();
	}

	// Resolve a function's global-reference table against this namespace, so its
	// GLOADC/GLOADV/GSTORE operands become direct slot indices.  Every name the
	// function mentions gets a slot, including ones it may never reach; those
	// stay Unassigned, so resolving a reference does not bring a global into
	// existence -- reading it still reports Undefined Identifier, and iteration
	// still skips it.
	//
	// Rare enough not to matter: adding a slot never invalidates an existing one,
	// so Id does not change and this runs once per function per namespace.  It
	// lives here rather than on FuncDef because Globals.g.h already sees
	// FuncDef.g.h, and the reverse would be a cycle.
	public void ResolveRefs(FuncDef func) {
		List<Value> names = func.GlobalNames;
		List<Int32> slots = func.GlobalSlots;
		for (Int32 i = 0; i < names.Count; i++) slots[i] = Resolve(names[i]);
		func.GlobalCacheId = _id;
	}

	// ── Map backing hooks (called by GCMap; see cs/GCItems.cs) ────────────────

	public Boolean TryGet(Value key, out Value value) {
		Int32 slot = Find(key);
		if (slot >= 0) {
			Value v = _values[slot];
			if (!v.IsUnassigned()) {
				value = v;
				return true;
			}
		}
		value = Value.Null;
		return false;
	}

	public void Set(Value key, Value value) {
		SetSlot(Resolve(key), value);
	}

	// Unbind a name.  The slot survives -- cached slot indices stay valid and a
	// later re-assignment reuses it -- so only distinct names ever used consume
	// slots, and remove/re-add is free.
	public Boolean Remove(Value key) {
		Int32 slot = Find(key);
		if (slot < 0) return false;
		if (_values[slot].IsUnassigned()) return false;
		SetSlot(slot, Value.Unassigned);
		return true;
	}

	public Boolean HasKey(Value key) {
		return SlotIsAssigned(Find(key));
	}

	// Unbind everything, keeping the slots (and therefore every cached slot
	// index, and this object's Id).
	public void Clear() {
		for (Int32 i = 0; i < _values.Count; i++) _values[i] = Value.Unassigned;
		_assignedCount = 0;
	}

	// Find the next assigned slot at or after startSlot, or -1 if there is
	// none.  This is the iteration primitive behind `for k in globals`; unlike
	// the Dictionary walk it replaces, it is O(1) per step.
	public Int32 NextAssignedSlot(Int32 startSlot) {
		Int32 i = startSlot;
		if (i < 0) i = 0;
		while (i < _values.Count) {
			if (!_values[i].IsUnassigned()) return i;
			i++;
		}
		return -1;
	}

	// ── GC ────────────────────────────────────────────────────────────────────

	// Mark every name and value held here.  Reached from GCMap.MarkChildren on
	// the map view, which the owner marks (see the constructor's note).
	public void MarkChildren() {
		for (Int32 i = 0; i < _names.Count; i++) {
			GCManager.Mark(_names[i]);
			GCManager.Mark(_values[i]);
		}
	}

	// ── The Unassigned sentinel ───────────────────────────────────────────────

	// Value.Unassigned is created by GCManager.Init() as a bare funcref, so
	// that GCManager needs no knowledge of Context or the VM.  Here we install
	// the callback that makes an escape loud: the sentinel must never reach user
	// code, and giving it a body that reports itself turns a silent wrong value
	// into a located error the first time one is touched.  A funcref is safe as
	// a sentinel precisely because funcrefs compare by identity (Value.cs,
	// ScalarEqual) and this one is never handed out, so no value a user can
	// construct is equal to it.
	private static void EnsureUnassignedSentinel() {
		FuncDef f = Value.Unassigned.FunctionDef();
		if (f == null) return;
		if (f.NativeCallback != null) return;   // already installed
		f.NativeCallback = (Context ctx, IntrinsicResult partialResult) => {
			ctx.vm.RaiseRuntimeError(
				"internal error: the unassigned-global sentinel escaped into user code");
			return IntrinsicResult.Null;
		};
	}
}

}
