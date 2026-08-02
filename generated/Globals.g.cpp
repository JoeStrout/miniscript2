// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Globals.cs

#include "Globals.g.h"
#include "GCManager.g.h"
#include "IntrinsicAPI.g.h"
#include "VM.g.h"

namespace MiniScript {

	Int32 GlobalsStorage::_lastId = 0;
Globals GlobalsStorage::Create() {
	Globals g =  Globals::New();
	g.AttachMap(GCManager::NewGlobalsMap(g));
	return g;
}
GlobalsStorage::GlobalsStorage() {
	EnsureUnassignedSentinel();
	_names         =  List<Value>::New();
	_values        =  List<Value>::New();
	_index         =  Dictionary<Value, Int32>::New();
	_assignedCount = 0;
	_mapValue      = Value::Null;
	_lastId++;
	_id            = _lastId;
}
void GlobalsStorage::AttachMap(Value mapValue) {
	_mapValue = mapValue;
	GCManager::AddRoot(_mapValue);
}
void GlobalsStorage::Release() {
	if (!_mapValue.IsNull()) {
		GCManager::RemoveRoot(_mapValue);
		_mapValue = Value::Null;
	}
}
Int32 GlobalsStorage::Resolve(Value name) {
	Int32 slot;
	if (_index.TryGetValue(name, &slot)) return slot;
	slot = _names.Count();
	_names.Add(name);
	_values.Add(Value::Unassigned);
	_index[name] = slot;
	return slot;
}
Boolean GlobalsStorage::TryGet(Value key,Value* value) {
	Int32 slot = Find(key);
	if (slot >= 0) {
		Value v = _values[slot];
		if (!v.IsUnassigned()) {
			*value = v;
			return Boolean(true);
		}
	}
	*value = Value::Null;
	return Boolean(false);
}
void GlobalsStorage::Set(Value key,Value value) {
	SetSlot(Resolve(key), value);
}
Boolean GlobalsStorage::Remove(Value key) {
	Int32 slot = Find(key);
	if (slot < 0) return Boolean(false);
	if (_values[slot].IsUnassigned()) return Boolean(false);
	SetSlot(slot, Value::Unassigned);
	return Boolean(true);
}
Boolean GlobalsStorage::HasKey(Value key) {
	return SlotIsAssigned(Find(key));
}
void GlobalsStorage::Clear() {
	for (Int32 i = 0; i < _values.Count(); i++) _values[i] = Value::Unassigned;
	_assignedCount = 0;
}
Int32 GlobalsStorage::NextAssignedSlot(Int32 startSlot) {
	Int32 i = startSlot;
	if (i < 0) i = 0;
	while (i < _values.Count()) {
		if (!_values[i].IsUnassigned()) return i;
		i++;
	}
	return -1;
}
void GlobalsStorage::MarkChildren() {
	for (Int32 i = 0; i < _names.Count(); i++) {
		GCManager::Mark(_names[i]);
		GCManager::Mark(_values[i]);
	}
}
void GlobalsStorage::EnsureUnassignedSentinel() {
	FuncDef f = Value::Unassigned.FunctionDef();
	if (IsNull(f)) return;
	if (!IsNull(f.NativeCallback())) return;   // already installed
	f.set_NativeCallback([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		ctx.vm.RaiseRuntimeError(
			"internal error: the unassigned-global sentinel escaped into user code");
		return IntrinsicResult::Null;
	});
}

} // end of namespace MiniScript
