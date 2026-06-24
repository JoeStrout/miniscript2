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
	Items.Add(make_int(length));
	Frozen   = Boolean(false);
	Computed = Boolean(true);
}
void GCList::Materialize() {
	if (!Computed) return;
	Int32 len = (Int32)numeric_val(Items[2]);
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
		Int32 len = (Int32)numeric_val(Items[2]);
		if (len == 0) return val_null;
		Value last = Get(len - 1);
		Items[2] = make_int(len - 1);
		return last;
	}
	if (IsNull(Items) || Items.Count() == 0) return val_null; // ToDo: error
	Value result = Items[Items.Count() - 1];
	Items.RemoveAt(Items.Count() - 1);
	return result;
}
Value GCList::Pull() {
	if (Computed) Materialize();
	if (IsNull(Items) || Items.Count() == 0) return val_null; // ToDo: error
	Value result = Items[0];
	Items.RemoveAt(0);
	return result;
}
Int32 GCList::IndexOf(Value item,Int32 afterIdx) {
	Int32 n = Count();
	for (Int32 i = afterIdx + 1; i < n; i++) {
		if (value_equal(Get(i), item)) return i;
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
	}
}

void GCHandle::MarkChildren() {
	// handles have no child Values
}

} // end of namespace MiniScript
