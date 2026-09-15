// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: GCItems.cs

#include "GCItems.g.h"
#include "GCManager.g.h"

namespace MiniScript {

void GCString::MarkChildren() {
	// strings have no child Values
}
void GCString::OnSweep() {
	Data = nullptr;
}

void GCList::InitComputed(Value baseVal,Value increment,Int32 length) {
	Items =  List<Value>::New(3);
	Items.Add(baseVal);
	Items.Add(increment);
	Items.Add(Value(length));
	Frozen   = Boolean(false);
	Computed = Boolean(true);
}
void GCList::Materialize() {
	if (!Computed) return;
	Int32 len = (Int32)Items[2].NumericVal();
	List<Value> real =  List<Value>::New(Math::Max(len, 4));
	for (Int32 i = 0; i < len; i++) real.Add(Get(i));  // Get still reads meta
	Items    = real;
	Computed = Boolean(false);
}
void GCList::Insert(Int32 index,Value v) {
	if (Computed) Materialize();
	if (IsNull(Items)) Init();
	if (index < 0) index += Items.Count() + 1;
	if (index < 0) index = 0;  // ToDo: this should raise a runtime error
	if (index > Items.Count()) index = Items.Count();
	Items.Insert(index, v);
}
Value GCList::Pop() {
	if (Computed) {
		Int32 len = (Int32)Items[2].NumericVal();
		if (len == 0) return Value::Null;
		Value last = Get(len - 1);
		Items[2] = Value(len - 1);
		return last;
	}
	if (IsNull(Items) || Items.Count() == 0) return Value::Null; // ToDo: error
	Value result = Items[Items.Count() - 1];
	Items.RemoveAt(Items.Count() - 1);
	return result;
}
Value GCList::Pull() {
	if (Computed) Materialize();
	if (IsNull(Items) || Items.Count() == 0) return Value::Null; // ToDo: error
	Value result = Items[0];
	Items.RemoveAt(0);
	return result;
}
Int32 GCList::IndexOf(Value item,Int32 afterIdx) {
	Int32 n = Count();
	for (Int32 i = afterIdx + 1; i < n; i++) {
		if (Get(i) == item) return i;
	}
	return -1;
}
void GCList::MarkChildren() {
	// For a computed list this marks [base, increment, length]; the base may
	// be a heap value (e.g. `[someList] * n`) and must be kept alive, while
	// the numeric increment/length mark as no-ops.
	if (IsNull(Items)) return;
	for (Int32 i = 0; i < Items.Count(); i++) GCManager::Mark(Items[i]);
}

Int32 GCMap::Count() {
	if (!IsNull(_gb)) return _gb.Count();
	Int32 n = (IsNull(Items)) ? 0 : Items.Count();
	if (!IsNull(_vmb)) n += _vmb.RegEntryCount();
	return n;
}
void GCMap::Init(Int32 capacity ) {
	Items  =  Dictionary<Value, Value>::New(Math::Max(capacity, 4));
	_order =  List<Value>::New(Math::Max(capacity, 4));
	_pos   = nullptr;
	Frozen = Boolean(false);
	_vmb   = nullptr;
	_gb    = nullptr;
}
void GCMap::InitAsGlobals(Globals g) {
	Items  = nullptr;
	_order = nullptr;
	_pos   = nullptr;
	Frozen = Boolean(false);
	_vmb   = nullptr;
	_gb    = g;
}
Boolean GCMap::TryGet(Value key,Value* value) {
	if (!IsNull(_gb)) return _gb.TryGet(key, &*value);

	// Check VarMap register bindings first.
	if (!IsNull(_vmb) && _vmb.TryGet(key, &*value)) return Boolean(true);

	if (IsNull(Items)) { *value = Value::Null; return Boolean(false); }
	if (Items.TryGetValue(key, &*value)) return Boolean(true);
	*value = Value::Null;
	return Boolean(false);
}
void GCMap::Set(Value key,Value value) {
	if (!IsNull(_gb)) { _gb.Set(key, value); return; }

	// Store in register if VarMap-backed and key is register-mapped.
	if (!IsNull(_vmb) && _vmb.TrySet(key, value)) return;

	if (IsNull(Items)) Init();
	// Whether the key is new is read off Count rather than asked for
	// separately, so an insert still costs one hash lookup.  Assigning to an
	// existing key must not move it in the order.
	Int32 before = Items.Count();
	Items[key] = value;
	if (Items.Count() != before && !IsNull(_order)) {
		if (!IsNull(_pos)) _pos[key] = _order.Count();
		_order.Add(key);
	}
}
Boolean GCMap::Remove(Value key) {
	if (!IsNull(_gb)) return _gb.Remove(key);
	if (!IsNull(_vmb) && _vmb.TryRemove(key)) return Boolean(true);
	if (IsNull(Items)) return Boolean(false);
	if (!Items.Remove(key)) return Boolean(false);
	OrderRemove(key);
	return Boolean(true);
}
void GCMap::Clear() {
	if (!IsNull(_gb)) { _gb.Clear(); return; }
	if (!IsNull(Items)) Items.Clear();
	if (!IsNull(_order)) _order.Clear();
	if (!IsNull(_pos)) _pos.Clear();
	if (!IsNull(_vmb)) _vmb.Clear();
}
void GCMap::OrderRemove(Value key) {
	if (IsNull(_order)) return;
	if (IsNull(_pos)) BuildPos();
	Int32 slot = 0;
	if (!_pos.TryGetValue(key, &slot)) return;
	_order[slot] = Value::Unassigned;
	_pos.Remove(key);
	// Compact once tombstones outnumber live entries, so that a map churned
	// through many times does not grow without bound and NextEntry does not
	// walk ever-longer runs of holes.  Amortized O(1) per removal.
	if (_order.Count() > 2 * Items.Count() && _order.Count() > 8) CompactOrder();
}
void GCMap::BuildPos() {
	_pos =  Dictionary<Value, Int32>::New(Math::Max(_order.Count(), 4));
	for (Int32 i = 0; i < _order.Count(); i++) {
		Value ord = _order[i];
		if (!ord.IsUnassigned()) _pos[ord] = i;
	}
}
void GCMap::CompactOrder() {
	Int32 w = 0;
	for (Int32 r = 0; r < _order.Count(); r++) {
		if (_order[r].IsUnassigned()) continue;
		_order[w] = _order[r];
		w++;
	}
	if (w < _order.Count()) _order.RemoveRange(w, _order.Count() - w);
	// Slots moved, so every recorded position is stale.  Cleared and
	// refilled in place: replacing the dictionary would be a struct write.
	if (!IsNull(_pos)) {
		_pos.Clear();
		for (Int32 i = 0; i < _order.Count(); i++) {
			Value ord = _order[i];
			_pos[ord] = i;
		}
	}
}
void GCMap::SeedOrder() {
	_order =  List<Value>::New(IsNull(Items) ? 4 : Math::Max(Items.Count(), 4));
	_pos   = nullptr;
	if (IsNull(Items)) return;
	for (Value k : Items.Keys()) _order.Add(k);
}
void GCMap::EnsureOrder() {
	if (IsNull(_order) || IsNull(Items)) return;
	if (_order.Count() - CountTombstones() == Items.Count()) return;
	_order.Clear();
	for (Value k : Items.Keys()) _order.Add(k);
	if (!IsNull(_pos)) {
		_pos.Clear();
		for (Int32 i = 0; i < _order.Count(); i++) {
			Value ord = _order[i];
			_pos[ord] = i;
		}
	}
}
Int32 GCMap::CountTombstones() {
	// Only walked when the cheap check above is inconclusive, which for a
	// map with no removals is never.
	if (_order.Count() == Items.Count()) return 0;
	Int32 n = 0;
	for (Int32 i = 0; i < _order.Count(); i++) {
		if (_order[i].IsUnassigned()) n++;
	}
	return n;
}
Int32 GCMap::NextEntry(Int32 after) {
	// Globals: iter is the slot index directly.  Unassigned slots are
	// skipped, so this walks exactly the bound globals, O(1) per step.
	if (!IsNull(_gb)) return _gb.NextAssignedSlot((after < 0) ? 0 : after + 1);

	// Phase 1: VarMap register entries (negative iter)
	if (!IsNull(_vmb) && after <= -1) {
		Int32 startRegIdx = (after == -1) ? 0 : -(after) - 2 + 1;
		Int32 found = _vmb.NextAssignedRegEntry(startRegIdx);
		if (found >= 0) return -(found + 2);
		// Fall through to the Items phase
	}

	if (IsNull(_order)) return -1;
	// Entering phase 2 is the one place it is safe to resync, since no
	// iterator is holding a slot index yet.
	if (after < 0) EnsureOrder();
	Int32 i = (after < 0) ? 0 : after + 1;
	while (i < _order.Count()) {
		if (!_order[i].IsUnassigned()) return i;
		i++;
	}
	return -1;
}
Value GCMap::KeyAt(Int32 i) {
	if (!IsNull(_gb)) return _gb.NameAtSlot(i);
	if (i < -1 && !IsNull(_vmb)) {
		Int32 regIdx = -(i) - 2;
		return _vmb.GetRegEntryKey(regIdx);
	}
	if (IsNull(_order) || i < 0 || i >= _order.Count()) return Value::Null;
	return _order[i];
}
Value GCMap::ValueAt(Int32 i) {
	if (!IsNull(_gb)) return _gb.ValueAtSlot(i);
	if (i < -1 && !IsNull(_vmb)) {
		Int32 regIdx = -(i) - 2;
		return _vmb.GetRegEntryValue(regIdx);
	}
	if (IsNull(_order) || i < 0 || i >= _order.Count()) return Value::Null;
	Value v = Value::Null;
	Items.TryGetValue(_order[i], &v);
	return v;
}
void GCMap::MarkChildren() {
	if (!IsNull(_gb)) { _gb.MarkChildren(); return; }
	if (!IsNull(Items)) {
		for (Value k : Items.Keys()) {
			GCManager::Mark(k);
		}
		for (Value v : Items.Values()) {
			GCManager::Mark(v);
		}
	}
	if (!IsNull(_vmb)) _vmb.MarkChildren();
}
void GCMap::OnSweep() {
	Items  = nullptr;
	_order = nullptr;
	_pos   = nullptr;
	Frozen = Boolean(false);
	_vmb   = nullptr;
	_gb    = nullptr;
}

void GCError::MarkChildren() {
	GCManager::Mark(Message);
	GCManager::Mark(Inner);
	GCManager::Mark(Stack);
	GCManager::Mark(Isa);
}

void GCFunction::MarkChildren() {
	GCManager::Mark(OuterVars);
	// Mark the function's compile-time constants so that nested-function
	// templates (and any interned strings) remain reachable.  This is what
	// roots the whole FuncDef graph now that the VM keeps no functions list.
	if (!IsNull(Func)) {
		List<Value> consts = Func.Constants();
		for (Int32 i = 0; i < consts.Count(); i++) GCManager::Mark(consts[i]);
		List<Value> pnames = Func.ParamNames();
		for (Int32 i = 0; i < pnames.Count(); i++) GCManager::Mark(pnames[i]);
		List<Value> pdefs = Func.ParamDefaults();
		for (Int32 i = 0; i < pdefs.Count(); i++) GCManager::Mark(pdefs[i]);
		// Global-reference names live outside the constant pool; see
		// VM.MarkFuncConstants.
		List<Value> gnames = Func.GlobalNames();
		for (Int32 i = 0; i < gnames.Count(); i++) GCManager::Mark(gnames[i]);
	}
}

void GCHandle::MarkChildren() {
	// handles have no child Values
}

} // end of namespace MiniScript
