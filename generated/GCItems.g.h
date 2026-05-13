// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: GCItems.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
#include "GCInterfaces.g.h"
#include "value.h"
#include "VarMap.g.h"
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

	public: void Init(Int32 capacity = 8);

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
// optional VarMapBacking for register-binding behaviour. Hash/equality of keys
// follows MiniScript == semantics via Value.Equals/GetHashCode (see Value.cs).

struct GCMap {
	public: Dictionary<Value, Value> Items;
	public: Boolean Frozen;
	public: VarMapBacking _vmb;

	// Non-null for VarMap-backed maps (closures / REPL globals).

	public: Int32 Count();

	public: void Init(Int32 capacity = 8);

	public: Boolean TryGet(Value key, Value* value);

	public: void Set(Value key, Value value);

	public: Boolean Remove(Value key);

	public: Boolean HasKey(Value key);

	public: void Clear();

	// ── Iteration ─────────────────────────────────────────────────────────────
	// iter = -1: start
	// iter < -1: VarMap register entry -(i+2) where i is the reg-entry index
	// iter >= 0: index into Items (in enumeration order)

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
	public: Int32 FuncIndex;
	public: Value OuterVars;

	public: void MarkChildren();

	public: void OnSweep();
}; // end of struct GCFunction

// INLINE METHODS

inline void GCList::Init(Int32 capacity ) {
	Items  =  List<Value>::New(Math::Max(capacity, 4));
	Frozen = Boolean(false);
}
inline void GCList::Push(Value v) {
	if (IsNull(Items)) Init();
	Items.Add(v);
}
inline Value GCList::Get(Int32 i) {
	if (i < 0) i += Items.Count();
	return (UInt32)i < (UInt32)Items.Count() ? Items[i] : val_null;
}
inline void GCList::Set(Int32 i,Value v) {
	if (i < 0) i += Items.Count();
	if ((UInt32)i < (UInt32)Items.Count()) Items[i] = v;
}
inline Boolean GCList::Remove(Int32 index) {
	if (IsNull(Items)) Init();
	if (index < 0) index += Items.Count();
	if (index < 0 || index >= Items.Count()) return Boolean(false);
	Items.RemoveAt(index);
	return Boolean(true);
}
inline void GCList::OnSweep() {
	Items = nullptr;
	Frozen = Boolean(false);
}

inline Boolean GCMap::HasKey(Value key) {
	Value ignored;
	return TryGet(key, &ignored);
}

inline void GCError::OnSweep() {
	Message = val_null;
	Inner   = val_null;
	Stack   = val_null;
	Isa     = val_null;
}

inline void GCFunction::OnSweep() {
	FuncIndex = -1;
	OuterVars = val_null;
}

} // end of namespace MiniScript
