// VarMapBacking.cpp — implementation; mirrors cs/VarMap.cs method-for-method.

#include "VarMapBacking.h"
#include "GCItems.h"
#include "GCManager.h"

namespace MiniScript {

VarMapBacking::VarMapBacking(Value* registers, Value* names, int firstIdx, int lastIdx)
    : _registers(registers), _names(names) {
    for (int i = firstIdx; i <= lastIdx; i++) {
        if (!is_null(names[i])) {
            if (_regMap.find(names[i]) == _regMap.end()) {
                _regOrder.push_back(names[i]);
            }
            _regMap[names[i]] = i;
        }
    }
}

// ── Register-binding checks ─────────────────────────────────────────────

bool VarMapBacking::TryGet(Value key, Value& outValue) const {
    if (!is_string(key)) { outValue = val_null; return false; }
    auto it = _regMap.find(key);
    if (it == _regMap.end()) { outValue = val_null; return false; }
    int regIdx = it->second;
    if (!nameLive(regIdx)) { outValue = val_null; return false; }
    outValue = _registers[regIdx];
    return true;
}

bool VarMapBacking::TrySet(Value key, Value value) {
    if (!is_string(key)) return false;
    auto it = _regMap.find(key);
    if (it == _regMap.end()) return false;
    int regIdx = it->second;
    _registers[regIdx] = value;
    _names[regIdx]     = key;
    return true;
}

bool VarMapBacking::TryRemove(Value key) {
    if (!is_string(key)) return false;
    auto it = _regMap.find(key);
    if (it == _regMap.end()) return false;
    _names[it->second] = val_null;
    return true;
}

bool VarMapBacking::HasKey(Value key) const {
    if (!is_string(key)) return false;
    auto it = _regMap.find(key);
    if (it == _regMap.end()) return false;
    return nameLive(it->second);
}

// ── Iteration ───────────────────────────────────────────────────────────

int VarMapBacking::RegEntryCount() const {
    int n = 0;
    for (auto& kvp : _regMap) {
        if (nameLive(kvp.second)) n++;
    }
    return n;
}

int VarMapBacking::NextAssignedRegEntry(int startIdx) const {
    for (int i = startIdx; i < (int)_regOrder.size(); i++) {
        auto it = _regMap.find(_regOrder[i]);
        if (it != _regMap.end() && nameLive(it->second)) return i;
    }
    return -1;
}

Value VarMapBacking::GetRegEntryKey(int i) const {
    if (i < 0 || i >= (int)_regOrder.size()) return val_null;
    return _regOrder[i];
}

Value VarMapBacking::GetRegEntryValue(int i) const {
    if (i < 0 || i >= (int)_regOrder.size()) return val_null;
    auto it = _regMap.find(_regOrder[i]);
    if (it == _regMap.end()) return val_null;
    return _registers[it->second];
}

// ── GC ──────────────────────────────────────────────────────────────────

void VarMapBacking::MarkChildren(GCManager& gc) {
    for (auto& kvp : _regMap) {
        int regIdx = kvp.second;
        if (nameLive(regIdx)) {
            gc.Mark(kvp.first);            // variable name (string Value)
            gc.Mark(_registers[regIdx]);   // current value
        }
    }
}

// ── VarMap operations ───────────────────────────────────────────────────

void VarMapBacking::Gather(GCMap& map) {
    // Detach during the Set() loop so the assignments hit the hash table,
    // not the registers we're about to clear. Walk _regOrder (not _regMap)
    // so the hash-table entries land in original insertion order.
    map._vmb = nullptr;
    for (size_t i = 0; i < _regOrder.size(); i++) {
        auto it = _regMap.find(_regOrder[i]);
        if (it == _regMap.end()) continue;
        int regIdx = it->second;
        if (nameLive(regIdx)) {
            map.Set(_regOrder[i], _registers[regIdx]);
        }
    }
    // Leave _vmb null on the map; the caller (Rebind, etc.) decides what next.
}

void VarMapBacking::Rebind(GCMap& map, Value* registers, Value* names) {
    Gather(map);
    _registers = registers;
    _names     = names;
    _regMap.clear();
    _regOrder.clear();
    map._vmb = this;
}

void VarMapBacking::MapToRegister(GCMap& map, Value varName, Value* registers, int regIndex) {
    if (_regMap.find(varName) == _regMap.end()) {
        _regOrder.push_back(varName);
    }
    _regMap[varName] = regIndex;

    // Hydrate the register from any existing plain map entry. Detach _vmb
    // during the lookup so TryGet/Remove hit the hash table, not the
    // newly-mapped register (which may still be null on a fresh stack).
    VarMapBacking* saved = map._vmb;
    map._vmb = nullptr;
    Value existing;
    if (map.TryGet(varName, existing)) {
        registers[regIndex] = existing;
        _names[regIndex]    = varName;
        map.Remove(varName);
    }
    map._vmb = saved;
}

void VarMapBacking::Clear() {
    for (auto& kvp : _regMap) {
        _names[kvp.second] = val_null;
    }
}

} // namespace MiniScript
