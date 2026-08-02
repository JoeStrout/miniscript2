// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Globals.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// Globals.cs
// The global namespace: a first-class object with its own storage, owned by an
// Interpreter rather than by any compiled program.  See notes/GLOBALS.md for
// the reasoning; the short version is that globals are NOT a call frame.  They
// outlive every compilation (each REPL line is a new @main), they grow at run
// time (`globals.foo = 42` from ten frames down), and there must be exactly one
// place a given global can live -- otherwise "where is it?" depends on whether
// the current @main happened to mention its name, which is how the old
// register-window-plus-hash-table arrangement leaked compilation artifacts into
// semantics.
// A Globals is a flat table of *slots*:
//     slot     0        1        2        3
//     _names   "x"      "print"  "total"  "i"
//     _values  42       <fn>     Unassign 7
// plus a name -> slot index.  Two properties make the whole design work:
//   * Slots are STABLE.  Once "foo" is slot 37 it is slot 37 for the life of
//     this object, even if removed and re-added.  Anything may therefore cache
//     a slot index -- which is what the planned GETGLOBAL/SETGLOBAL opcodes do
//     (stage 4), turning a global access into an array index.
//   * The table GROWS.  A new name appends.  No ceiling, no MaxRegs, nothing to
//     gather or rebind when a new program is compiled.
// A slot whose name is not currently bound holds Value.Unassigned, which is
// distinct from null -- null is a perfectly good value for a global to hold.
// This class doubles as the backing for the `globals` map (GCMap._gb), so the
// map view and compiled code reach the very same slots.  That is what makes
// `globals.foo = 42` and a top-level `foo = 42` the same operation by
// construction rather than by synchronization.

#include "value.h"
#include "FuncDef.g.h"
// Context and IntrinsicResult are only named inside a method body (the sentinel
// callback), and FuncDef.g.h already forward-declares both, so those two stay
// out of the header -- GCItems.g.h includes this one, and dragging the VM in
// there would be a cycle.
// That cycle is also what decides which methods here may be inlined: an inlined
// body lands in Globals.g.h, so it may only touch things visible there.  The
// slot primitives qualify (they need nothing beyond value.h, now that the
// Unassigned sentinel lives on Value rather than on GCManager); anything
// calling GCManager -- Create, AttachMap, Release, MarkChildren -- must stay
// out of line.

namespace MiniScript {

// DECLARATIONS

class GlobalsStorage : public std::enable_shared_from_this<GlobalsStorage> {
	friend struct Globals;
	private: List<Value> _names; // slot -> name (set once, never cleared)
	private: List<Value> _values; // slot -> value, or Value.Unassigned
	private: Dictionary<Value, Int32> _index; // name -> slot
	private: Int32 _id; // generation; see Id()
	private: Int32 _assignedCount; // slots currently holding a value
	private: Value _mapValue; // the `globals` map viewing this table
	private: static Int32 _lastId;

	// Generation counter.  Every Globals gets a distinct Id, and an Id is never
	// reused.  Compiled code caches (name -> slot) resolutions and validates
	// them with a single integer compare against this; adding a slot never
	// invalidates an existing one, so the only thing that changes an Id is
	// building a whole new namespace.  See notes/GLOBALS.md section 4.3.
	// Pre-incremented, so the first Globals gets Id 1 and 0 is never a valid Id
	// -- which lets a not-yet-resolved slot cache use 0 as its "never resolved"
	// marker.

	// Make a new, empty global namespace.  Use this rather than `new Globals()`
	// directly: a Globals is paired 1:1 with the map that views it, and only a
	// fully constructed one is usable.
	// (The pairing is done out here, from a local, rather than in the
	// constructor.  Handing `this` to something that wants a Globals would mean
	// converting a storage pointer back to a wrapper on the C++ side, which the
	// transpiler does not do; a local of wrapper type passes straight through.)
	public: static Globals Create();

	// Every field is assigned here rather than at its declaration: the
	// transpiler drops field initializers too, so anything not set in the
	// constructor would be left default-constructed on the C++ side.
	public: GlobalsStorage();

	// Adopt the map that views this table, and root it.  The map holds a
	// reference back here (GCMap._gb), so if it were swept the table would be
	// orphaned from the map's side.  Release() drops the root.
	// TODO(stage 3): the owning VM should mark this instead, so that discarding
	// a Globals (as the `reset` intrinsic will) reclaims it.
	public: void AttachMap(Value mapValue);

	// Drop the GC root on the map view.  After this the Globals must not be
	// used again unless something else keeps the map alive.
	public: void Release();

	// ── Identity and size ─────────────────────────────────────────────────────

	public: Int32 Id();

	// The `globals` map: an ordinary MiniScript map whose entire storage is this
	// table.  There is no second tier -- every entry is a slot.
	public: Value AsMap();

	// Total slots ever created, including unassigned ones.  This is the bound
	// for slot iteration, NOT the number of globals; use Count() for that.
	public: Int32 SlotCount();

	// Number of globals currently bound (what `globals.len` reports).
	public: Int32 Count();

	// ── Slot access ───────────────────────────────────────────────────────────

	// The slot for the given name, creating one if this name has never been
	// seen.  A newly created slot is Unassigned, so resolving a name does not
	// bring it into existence as a global -- reading it still fails.  This is
	// what lets compiled code resolve all of a function's global references up
	// front, including names it may never reach.
	public: Int32 Resolve(Value name);

	// The slot for the given name, or -1 if the name has never been seen.
	// Does not create anything.
	public: Int32 Find(Value name);

	// The value in a slot, or Unassigned.  Callers on the read path must test
	// IsUnassigned() and treat it as "not found" -- it must never be handed to
	// user code.
	public: Value ValueAtSlot(Int32 slot);

	// The name a slot is bound to.  Always valid for a slot that exists, even
	// when the slot is unassigned (names are set once and never cleared).
	public: Value NameAtSlot(Int32 slot);

	// Store into a slot, keeping the assigned-count in step.  Storing
	// Unassigned here is how a global is removed.
	public: void SetSlot(Int32 slot, Value value);

	// True if the slot exists and currently holds a value.
	public: Boolean SlotIsAssigned(Int32 slot);

	// Resolve a function's global-reference table against this namespace, so its
	// GLOADC/GLOADV/GSTORE operands become direct slot indices.  Every name the
	// function mentions gets a slot, including ones it may never reach; those
	// stay Unassigned, so resolving a reference does not bring a global into
	// existence -- reading it still reports Undefined Identifier, and iteration
	// still skips it.
	// Rare enough not to matter: adding a slot never invalidates an existing one,
	// so Id does not change and this runs once per function per namespace.  It
	// lives here rather than on FuncDef because Globals.g.h already sees
	// FuncDef.g.h, and the reverse would be a cycle.
	public: void ResolveRefs(FuncDef func);

	// ── Map backing hooks (called by GCMap; see cs/GCItems.cs) ────────────────

	public: Boolean TryGet(Value key, Value* value);

	public: void Set(Value key, Value value);

	// Unbind a name.  The slot survives -- cached slot indices stay valid and a
	// later re-assignment reuses it -- so only distinct names ever used consume
	// slots, and remove/re-add is free.
	public: Boolean Remove(Value key);

	public: Boolean HasKey(Value key);

	// Unbind everything, keeping the slots (and therefore every cached slot
	// index, and this object's Id).
	public: void Clear();

	// Find the next assigned slot at or after startSlot, or -1 if there is
	// none.  This is the iteration primitive behind `for k in globals`; unlike
	// the Dictionary walk it replaces, it is O(1) per step.
	public: Int32 NextAssignedSlot(Int32 startSlot);

	// ── GC ────────────────────────────────────────────────────────────────────

	// Mark every name and value held here.  Reached from GCMap.MarkChildren on
	// the map view, which the owner marks (see the constructor's note).
	public: void MarkChildren();

	// ── The Unassigned sentinel ───────────────────────────────────────────────

	// Value.Unassigned is created by GCManager.Init() as a bare funcref, so
	// that GCManager needs no knowledge of Context or the VM.  Here we install
	// the callback that makes an escape loud: the sentinel must never reach user
	// code, and giving it a body that reports itself turns a silent wrong value
	// into a located error the first time one is touched.  A funcref is safe as
	// a sentinel precisely because funcrefs compare by identity (Value.cs,
	// ScalarEqual) and this one is never handed out, so no value a user can
	// construct is equal to it.
	private: static void EnsureUnassignedSentinel();
}; // end of class GlobalsStorage

struct Globals {
	friend class GlobalsStorage;
	protected: std::shared_ptr<GlobalsStorage> storage;
  public:
	Globals(std::shared_ptr<GlobalsStorage> stor) : storage(stor) {}
	Globals() : storage(nullptr) {}
	Globals(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const Globals& inst) { return inst.storage == nullptr; }
	private: GlobalsStorage* get() const;

	private: List<Value> _names(); // slot -> name (set once, never cleared)
	private: void set__names(List<Value> _v); // slot -> name (set once, never cleared)
	private: List<Value> _values(); // slot -> value, or Value.Unassigned
	private: void set__values(List<Value> _v); // slot -> value, or Value.Unassigned
	private: Dictionary<Value, Int32> _index(); // name -> slot
	private: void set__index(Dictionary<Value, Int32> _v); // name -> slot
	private: Int32 _id(); // generation; see Id()
	private: void set__id(Int32 _v); // generation; see Id()
	private: Int32 _assignedCount(); // slots currently holding a value
	private: void set__assignedCount(Int32 _v); // slots currently holding a value
	private: Value _mapValue(); // the `globals` map viewing this table
	private: void set__mapValue(Value _v); // the `globals` map viewing this table
	private: Int32 _lastId();
	private: void set__lastId(Int32 _v);

	// Generation counter.  Every Globals gets a distinct Id, and an Id is never
	// reused.  Compiled code caches (name -> slot) resolutions and validates
	// them with a single integer compare against this; adding a slot never
	// invalidates an existing one, so the only thing that changes an Id is
	// building a whole new namespace.  See notes/GLOBALS.md section 4.3.
	// Pre-incremented, so the first Globals gets Id 1 and 0 is never a valid Id
	// -- which lets a not-yet-resolved slot cache use 0 as its "never resolved"
	// marker.

	// Make a new, empty global namespace.  Use this rather than `new Globals()`
	// directly: a Globals is paired 1:1 with the map that views it, and only a
	// fully constructed one is usable.
	// (The pairing is done out here, from a local, rather than in the
	// constructor.  Handing `this` to something that wants a Globals would mean
	// converting a storage pointer back to a wrapper on the C++ side, which the
	// transpiler does not do; a local of wrapper type passes straight through.)
	public: static Globals Create() { return GlobalsStorage::Create(); }

	// Every field is assigned here rather than at its declaration: the
	// transpiler drops field initializers too, so anything not set in the
	// constructor would be left default-constructed on the C++ side.
	public: static Globals New() {
		return Globals(std::make_shared<GlobalsStorage>());
	}

	// Adopt the map that views this table, and root it.  The map holds a
	// reference back here (GCMap._gb), so if it were swept the table would be
	// orphaned from the map's side.  Release() drops the root.
	// TODO(stage 3): the owning VM should mark this instead, so that discarding
	// a Globals (as the `reset` intrinsic will) reclaims it.
	public: inline void AttachMap(Value mapValue);

	// Drop the GC root on the map view.  After this the Globals must not be
	// used again unless something else keeps the map alive.
	public: inline void Release();

	// ── Identity and size ─────────────────────────────────────────────────────

	public: inline Int32 Id();

	// The `globals` map: an ordinary MiniScript map whose entire storage is this
	// table.  There is no second tier -- every entry is a slot.
	public: inline Value AsMap();

	// Total slots ever created, including unassigned ones.  This is the bound
	// for slot iteration, NOT the number of globals; use Count() for that.
	public: inline Int32 SlotCount();

	// Number of globals currently bound (what `globals.len` reports).
	public: inline Int32 Count();

	// ── Slot access ───────────────────────────────────────────────────────────

	// The slot for the given name, creating one if this name has never been
	// seen.  A newly created slot is Unassigned, so resolving a name does not
	// bring it into existence as a global -- reading it still fails.  This is
	// what lets compiled code resolve all of a function's global references up
	// front, including names it may never reach.
	public: inline Int32 Resolve(Value name);

	// The slot for the given name, or -1 if the name has never been seen.
	// Does not create anything.
	public: inline Int32 Find(Value name);

	// The value in a slot, or Unassigned.  Callers on the read path must test
	// IsUnassigned() and treat it as "not found" -- it must never be handed to
	// user code.
	public: inline Value ValueAtSlot(Int32 slot);

	// The name a slot is bound to.  Always valid for a slot that exists, even
	// when the slot is unassigned (names are set once and never cleared).
	public: inline Value NameAtSlot(Int32 slot);

	// Store into a slot, keeping the assigned-count in step.  Storing
	// Unassigned here is how a global is removed.
	public: inline void SetSlot(Int32 slot, Value value);

	// True if the slot exists and currently holds a value.
	public: inline Boolean SlotIsAssigned(Int32 slot);

	// Resolve a function's global-reference table against this namespace, so its
	// GLOADC/GLOADV/GSTORE operands become direct slot indices.  Every name the
	// function mentions gets a slot, including ones it may never reach; those
	// stay Unassigned, so resolving a reference does not bring a global into
	// existence -- reading it still reports Undefined Identifier, and iteration
	// still skips it.
	// Rare enough not to matter: adding a slot never invalidates an existing one,
	// so Id does not change and this runs once per function per namespace.  It
	// lives here rather than on FuncDef because Globals.g.h already sees
	// FuncDef.g.h, and the reverse would be a cycle.
	public: inline void ResolveRefs(FuncDef func);

	// ── Map backing hooks (called by GCMap; see cs/GCItems.cs) ────────────────

	public: inline Boolean TryGet(Value key, Value* value);

	public: inline void Set(Value key, Value value);

	// Unbind a name.  The slot survives -- cached slot indices stay valid and a
	// later re-assignment reuses it -- so only distinct names ever used consume
	// slots, and remove/re-add is free.
	public: inline Boolean Remove(Value key);

	public: inline Boolean HasKey(Value key);

	// Unbind everything, keeping the slots (and therefore every cached slot
	// index, and this object's Id).
	public: inline void Clear();

	// Find the next assigned slot at or after startSlot, or -1 if there is
	// none.  This is the iteration primitive behind `for k in globals`; unlike
	// the Dictionary walk it replaces, it is O(1) per step.
	public: inline Int32 NextAssignedSlot(Int32 startSlot);

	// ── GC ────────────────────────────────────────────────────────────────────

	// Mark every name and value held here.  Reached from GCMap.MarkChildren on
	// the map view, which the owner marks (see the constructor's note).
	public: inline void MarkChildren();

	// ── The Unassigned sentinel ───────────────────────────────────────────────

	// Value.Unassigned is created by GCManager.Init() as a bare funcref, so
	// that GCManager needs no knowledge of Context or the VM.  Here we install
	// the callback that makes an escape loud: the sentinel must never reach user
	// code, and giving it a body that reports itself turns a silent wrong value
	// into a located error the first time one is touched.  A funcref is safe as
	// a sentinel precisely because funcrefs compare by identity (Value.cs,
	// ScalarEqual) and this one is never handed out, so no value a user can
	// construct is equal to it.
	private: static void EnsureUnassignedSentinel() { return GlobalsStorage::EnsureUnassignedSentinel(); }
}; // end of struct Globals

// INLINE METHODS

inline GlobalsStorage* Globals::get() const { return static_cast<GlobalsStorage*>(storage.get()); }
inline List<Value> Globals::_names() { return get()->_names; } // slot -> name (set once, never cleared)
inline void Globals::set__names(List<Value> _v) { get()->_names = _v; } // slot -> name (set once, never cleared)
inline List<Value> Globals::_values() { return get()->_values; } // slot -> value, or Value.Unassigned
inline void Globals::set__values(List<Value> _v) { get()->_values = _v; } // slot -> value, or Value.Unassigned
inline Dictionary<Value, Int32> Globals::_index() { return get()->_index; } // name -> slot
inline void Globals::set__index(Dictionary<Value, Int32> _v) { get()->_index = _v; } // name -> slot
inline Int32 Globals::_id() { return get()->_id; } // generation; see Id()
inline void Globals::set__id(Int32 _v) { get()->_id = _v; } // generation; see Id()
inline Int32 Globals::_assignedCount() { return get()->_assignedCount; } // slots currently holding a value
inline void Globals::set__assignedCount(Int32 _v) { get()->_assignedCount = _v; } // slots currently holding a value
inline Value Globals::_mapValue() { return get()->_mapValue; } // the `globals` map viewing this table
inline void Globals::set__mapValue(Value _v) { get()->_mapValue = _v; } // the `globals` map viewing this table
inline Int32 Globals::_lastId() { return get()->_lastId; }
inline void Globals::set__lastId(Int32 _v) { get()->_lastId = _v; }
inline void Globals::AttachMap(Value mapValue) { return get()->AttachMap(mapValue); }
inline void Globals::Release() { return get()->Release(); }
inline Int32 Globals::Id() { return get()->Id(); }
inline Int32 GlobalsStorage::Id() {
	return _id;
}
inline Value Globals::AsMap() { return get()->AsMap(); }
inline Value GlobalsStorage::AsMap() {
	return _mapValue;
}
inline Int32 Globals::SlotCount() { return get()->SlotCount(); }
inline Int32 GlobalsStorage::SlotCount() {
	return _names.Count();
}
inline Int32 Globals::Count() { return get()->Count(); }
inline Int32 GlobalsStorage::Count() {
	return _assignedCount;
}
inline Int32 Globals::Resolve(Value name) { return get()->Resolve(name); }
inline Int32 Globals::Find(Value name) { return get()->Find(name); }
inline Int32 GlobalsStorage::Find(Value name) {
	Int32 slot;
	if (_index.TryGetValue(name, &slot)) return slot;
	return -1;
}
inline Value Globals::ValueAtSlot(Int32 slot) { return get()->ValueAtSlot(slot); }
inline Value GlobalsStorage::ValueAtSlot(Int32 slot) {
	if (slot < 0 || slot >= _values.Count()) return Value::Unassigned;
	return _values[slot];
}
inline Value Globals::NameAtSlot(Int32 slot) { return get()->NameAtSlot(slot); }
inline Value GlobalsStorage::NameAtSlot(Int32 slot) {
	if (slot < 0 || slot >= _names.Count()) return Value::Null;
	return _names[slot];
}
inline void Globals::SetSlot(Int32 slot,Value value) { return get()->SetSlot(slot, value); }
inline void GlobalsStorage::SetSlot(Int32 slot,Value value) {
	if (slot < 0 || slot >= _values.Count()) return;
	Boolean wasAssigned = !_values[slot].IsUnassigned();
	Boolean nowAssigned = !value.IsUnassigned();
	_values[slot] = value;
	if (wasAssigned && !nowAssigned) _assignedCount--;
	if (!wasAssigned && nowAssigned) _assignedCount++;
}
inline Boolean Globals::SlotIsAssigned(Int32 slot) { return get()->SlotIsAssigned(slot); }
inline Boolean GlobalsStorage::SlotIsAssigned(Int32 slot) {
	if (slot < 0 || slot >= _values.Count()) return Boolean(false);
	return !_values[slot].IsUnassigned();
}
inline void Globals::ResolveRefs(FuncDef func) { return get()->ResolveRefs(func); }
inline Boolean Globals::TryGet(Value key,Value* value) { return get()->TryGet(key, value); }
inline void Globals::Set(Value key,Value value) { return get()->Set(key, value); }
inline Boolean Globals::Remove(Value key) { return get()->Remove(key); }
inline Boolean Globals::HasKey(Value key) { return get()->HasKey(key); }
inline void Globals::Clear() { return get()->Clear(); }
inline Int32 Globals::NextAssignedSlot(Int32 startSlot) { return get()->NextAssignedSlot(startSlot); }
inline void Globals::MarkChildren() { return get()->MarkChildren(); }

} // end of namespace MiniScript
