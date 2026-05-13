// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VarMap.cs

#include "VarMap.g.h"
#include "GCManager.g.h"

namespace MiniScript {

VarMapBackingStorage::VarMapBackingStorage(List<Value> registers,List<Value> names,Int32 firstIdx,Int32 lastIdx) {
	_registers  = registers;
	_names      = names;
	_regOrder   =  List<Value>::New();
	_regIndices =  List<Int32>::New();

	for (Int32 i = firstIdx; i <= lastIdx; i++) {
		if (!is_null(_names[i])) {
			Int32 orderIdx = FindOrderIdx(_names[i]);
			if (orderIdx < 0) {
				_regOrder.Add(_names[i]);
				_regIndices.Add(i);
			} else {
				_regIndices[orderIdx] = i;
			}
		}
	}
}
Boolean VarMapBackingStorage::TryGet(Value key,Value* value) {
	if (is_string(key)) {
		Int32 orderIdx = FindOrderIdx(key);
		if (orderIdx >= 0) {
			Int32 regIdx = _regIndices[orderIdx];
			if (!is_null(_names[regIdx])) {
				*value = _registers[regIdx];
				return Boolean(true);
			}
		}
	}
	*value = val_null;
	return Boolean(false);
}
Boolean VarMapBackingStorage::TrySet(Value key,Value value) {
	if (is_string(key)) {
		Int32 orderIdx = FindOrderIdx(key);
		if (orderIdx >= 0) {
			Int32 regIdx = _regIndices[orderIdx];
			_registers[regIdx] = value;
			_names[regIdx]     = key;
			return Boolean(true);
		}
	}
	return Boolean(false);
}
Boolean VarMapBackingStorage::TryRemove(Value key) {
	if (is_string(key)) {
		Int32 orderIdx = FindOrderIdx(key);
		if (orderIdx >= 0) {
			Int32 regIdx = _regIndices[orderIdx];
			_names[regIdx] = val_null;
			return Boolean(true);
		}
	}
	return Boolean(false);
}
Boolean VarMapBackingStorage::HasKey(Value key) {
	if (is_string(key)) {
		Int32 orderIdx = FindOrderIdx(key);
		Int32 regIdx = _regIndices[orderIdx];
		if (orderIdx >= 0) return !is_null(_names[orderIdx]);
	}
	return Boolean(false);
}
Int32 VarMapBackingStorage::RegEntryCount() {
	Int32 n = 0;
	for (Int32 i = 0; i < _regOrder.Count(); i++) {
		Int32 regIdx = _regIndices[i];
		if (!is_null(_names[regIdx])) n++;
	}
	return n;
}
Int32 VarMapBackingStorage::NextAssignedRegEntry(Int32 startIdx) {
	for (Int32 i = startIdx; i < _regOrder.Count(); i++) {
		Int32 regIdx = _regIndices[i];
		if (!is_null(_names[regIdx])) return i;
	}
	return -1;
}
Value VarMapBackingStorage::GetRegEntryKey(Int32 i) {
	if (i < 0 || i >= _regOrder.Count()) return val_null;
	return _regOrder[i];
}
Value VarMapBackingStorage::GetRegEntryValue(Int32 i) {
	if (i < 0 || i >= _regOrder.Count()) return val_null;
	Int32 regIdx = _regIndices[i];
	return _registers[regIdx];
}
void VarMapBackingStorage::MarkChildren() {
	for (Int32 i = 0; i < _regOrder.Count(); i++) {
		Int32 regIdx = _regIndices[i];
		if (regIdx < _names.Count() && !is_null(_names[regIdx])) {
			GCManager::Mark(_regOrder[i]);
			GCManager::Mark(_registers[regIdx]);
		}
	}
}
void VarMapBackingStorage::Gather(Int32 mapIdx) {
	// Temporarily detach to prevent recursion inside map.Set().
	GCManager::Maps.SetVmb(mapIdx, nullptr);
	GCMap map = GCManager::Maps.Get(mapIdx);
	for (Int32 i = 0; i < _regOrder.Count(); i++) {
		Int32 regIdx = _regIndices[i];
		if (regIdx < _names.Count() && !is_null(_names[regIdx])) {
			map.Set(_regOrder[i], _registers[regIdx]);
		}
	}
	// Leave _vmb = null (gathered; no more register backing).
}
void VarMapBackingStorage::Rebind(Int32 mapIdx,List<Value> registers,List<Value> names) {
	VarMapBacking _this(std::static_pointer_cast<VarMapBackingStorage>(shared_from_this()));
	Gather(mapIdx);
	// Re-attach with new arrays (Gather set _vmb = null; we have to restore it).
	_registers = registers;
	_names     = names;
	_regOrder.Clear();
	_regIndices.Clear();
	GCManager::Maps.SetVmb(mapIdx, _this);
}
void VarMapBackingStorage::MapToRegister(Int32 mapIdx,Value varName,List<Value> registers,Int32 regIndex) {
	Int32 orderIdx = FindOrderIdx(varName);
	if (orderIdx < 0) {
		_regOrder.Add(varName);
		_regIndices.Add(regIndex);
	} else {
		_regIndices[orderIdx] = regIndex;
	}
	// If there was an existing plain map entry, move its value to the register.
	// Bypass _vmb during this lookup so we read from the hash table, not the
	// (just-mapped) register, which may be empty/null on a fresh stack.
	GCMap map = GCManager::Maps.Get(mapIdx);
	VarMapBacking saved = map._vmb;
	GCManager::Maps.SetVmb(mapIdx, nullptr);
	map = GCManager::Maps.Get(mapIdx);  // refresh after SetVmb
	Value existingVal;
	if (map.TryGet(varName, &existingVal)) {
		registers[regIndex] = existingVal;
		_names[regIndex]    = varName;   // mark register as live so TryGet can find it
		map.Remove(varName);
	}
	GCManager::Maps.SetVmb(mapIdx, saved);
}
void VarMapBackingStorage::Clear() {
	for (Int32 i = 0; i < _regOrder.Count(); i++) {
		Int32 regIdx = _regIndices[i];
		if (regIdx < _names.Count()) _names[regIdx] = val_null;
	}
}
Value VarMapBackingStorage::NewVarMap(List<Value> registers,List<Value> names,Int32 firstIdx,Int32 lastIdx) {
	VarMapBacking vmb =  VarMapBacking::New(registers, names, firstIdx, lastIdx);
	Int32 idx = GCManager::Maps.AllocItem();
	GCManager::Maps.Init(idx, 4);
	GCManager::Maps.SetVmb(idx, vmb);
	return make_gc(GCManager::MapSet, idx);
}

} // end of namespace MiniScript
