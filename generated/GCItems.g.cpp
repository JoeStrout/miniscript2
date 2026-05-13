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

void GCList::Insert(Int32 index,Value v) {
	if (IsNull(Items)) Init();
	if (index < 0) index += Items.Count() + 1;
	if (index < 0) index = 0;  // ToDo: this should raise a runtime error
	if (index > Items.Count()) index = Items.Count();
	Items.Insert(index, v);
}
Value GCList::Pop() {
	if (IsNull(Items) || Items.Count() == 0) return val_null; // ToDo: error
	Value result = Items[Items.Count() - 1];
	Items.RemoveAt(Items.Count() - 1);
	return result;
}
Value GCList::Pull() {
	if (IsNull(Items) || Items.Count() == 0) return val_null; // ToDo: error
	Value result = Items[0];
	Items.RemoveAt(0);
	return result;
}
Int32 GCList::IndexOf(Value item,Int32 afterIdx) {
	if (IsNull(Items)) return -1;
	for (Int32 i = afterIdx + 1; i < Items.Count(); i++) {
		if (value_equal(Items[i], item)) return i;
	}
	return -1;
}
void GCList::MarkChildren() {
	if (IsNull(Items)) return;
	for (Int32 i = 0; i < Items.Count(); i++) GCManager::Mark(Items[i]);
}

Int32 GCMap::Count() {
	Int32 n = (IsNull(Items)) ? 0 : Items.Count();
	if (!IsNull(_vmb)) n += _vmb.RegEntryCount();
	return n;
}
void GCMap::Init(Int32 capacity ) {
	Items  =  Dictionary<Value, Value>::New(Math::Max(capacity, 4));
	Frozen = Boolean(false);
	_vmb   = nullptr;
}
Boolean GCMap::TryGet(Value key,Value* value) {
	// Check VarMap register bindings first.
	if (!IsNull(_vmb) && _vmb.TryGet(key, &*value)) return Boolean(true);

	if (IsNull(Items)) { *value = val_null; return Boolean(false); }
	if (Items.TryGetValue(key, &*value)) return Boolean(true);
	*value = val_null;
	return Boolean(false);
}
void GCMap::Set(Value key,Value value) {
	// Store in register if VarMap-backed and key is register-mapped.
	if (!IsNull(_vmb) && _vmb.TrySet(key, value)) return;

	if (IsNull(Items)) Init();
	Items[key] = value;
}
Boolean GCMap::Remove(Value key) {
	if (!IsNull(_vmb) && _vmb.TryRemove(key)) return Boolean(true);
	if (IsNull(Items)) return Boolean(false);
	return Items.Remove(key);
}
void GCMap::Clear() {
	if (!IsNull(Items)) Items.Clear();
	if (!IsNull(_vmb)) _vmb.Clear();
}
Int32 GCMap::NextEntry(Int32 after) {
	// Phase 1: VarMap register entries (negative iter)
	if (!IsNull(_vmb) && after <= -1) {
		Int32 startRegIdx = (after == -1) ? 0 : -(after) - 2 + 1;
		Int32 found = _vmb.NextAssignedRegEntry(startRegIdx);
		if (found >= 0) return -(found + 2);
		// Fall through to Dictionary phase
	}

	if (IsNull(Items)) return -1;
	Int32 i = (after < 0) ? 0 : after + 1;
	if (i < Items.Count()) return i;
	return -1;
}
Value GCMap::KeyAt(Int32 i) {
	if (i < -1 && !IsNull(_vmb)) {
		Int32 regIdx = -(i) - 2;
		return _vmb.GetRegEntryKey(regIdx);
	}
	if (IsNull(Items)) return val_null;
	Int32 j = 0;
	for (Value k : Items.Keys()) {
		if (j == i) return k;
		j++;
	}
	return val_null;
}
Value GCMap::ValueAt(Int32 i) {
	if (i < -1 && !IsNull(_vmb)) {
		Int32 regIdx = -(i) - 2;
		return _vmb.GetRegEntryValue(regIdx);
	}
	if (IsNull(Items)) return val_null;
	Int32 j = 0;
	for (Value v : Items.Values()) {
		if (j == i) return v;
		j++;
	}
	return val_null;
}
void GCMap::MarkChildren() {
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
	Frozen = Boolean(false);
	_vmb   = nullptr;
}

void GCError::MarkChildren() {
	GCManager::Mark(Message);
	GCManager::Mark(Inner);
	GCManager::Mark(Stack);
	GCManager::Mark(Isa);
}

void GCFunction::MarkChildren() {
	GCManager::Mark(OuterVars);
}

} // end of namespace MiniScript
