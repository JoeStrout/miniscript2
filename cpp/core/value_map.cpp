// value_map.cpp — thin wrappers over GCManager.Maps.

#include "value_map.h"
#include "value_string.h"
#include "value_list.h"
#include "vm_error.h"
#include "GCManager.g.h"
#include "VarMap.g.h"
#include "hashing.h"
#include <cstring>

namespace MiniScript {

using MiniScript::GCManager;
using MiniScript::GCMap;
using MiniScript::VarMapBacking;


// ── Creation ────────────────────────────────────────────────────────────

Value Value::make_map(int initial_capacity) {
    return GCManager::NewMap(initial_capacity);
}

Value Value::make_empty_map(void) {
    return GCManager::NewMap(8);
}

// ── Access ──────────────────────────────────────────────────────────────

int Value::MapCount() const {
    Value map_val = *this;
    if (!map_val.IsMap()) return 0;
    GCMap m = GCManager::Maps.Get(map_val.ItemIndex());
    return m.Count();
}

int map_capacity(Value map_val) {
    if (!map_val.IsMap()) return 0;
    GCMap m = GCManager::Maps.Get(map_val.ItemIndex());
    // Best-effort: GCMap exposes size only via Count(). Return Count*2 as a
    // proxy since the underlying table maintains 50% load.
    return m.Count() * 2;
}

Value Value::MapGet(Value key) const {
    Value map_val = *this;
    if (!map_val.IsMap()) return Value::null;
    GCMap m = GCManager::Maps.Get(map_val.ItemIndex());
    Value out;
    if (m.TryGet(key, &out)) return out;
    return Value::null;
}

bool Value::TryGet(Value key, Value* out_value) const {
    Value map_val = *this;
    if (!map_val.IsMap()) { if (out_value) *out_value = Value::null; return false; }
    GCMap m = GCManager::Maps.Get(map_val.ItemIndex());
    Value out;
    bool ok = m.TryGet(key, &out);
    if (out_value) *out_value = ok ? out : Value::null;
    return ok;
}

// Walks the __isa chain.
bool Value::Lookup(Value key, Value* out_value) const {
    Value map_val = *this;
    Value current = map_val;
    for (int depth = 0; depth < 256; depth++) {
        if (!current.IsMap()) { if (out_value) *out_value = Value::null; return false; }
        GCMap m = GCManager::Maps.Get(current.ItemIndex());
        Value v;
        if (m.TryGet(key, &v)) { if (out_value) *out_value = v; return true; }
        Value isa;
        if (!m.TryGet(Value::magicIsA, &isa)) { if (out_value) *out_value = Value::null; return false; }
        current = isa;
    }
    if (out_value) *out_value = Value::null;
    return false;
}

bool Value::LookupWithOrigin(Value key, Value* out_value, Value* out_super) const {
    Value map_val = *this;
    Value current = map_val;
    for (int depth = 0; depth < 256; depth++) {
        if (!current.IsMap()) {
            if (out_value) *out_value = Value::null;
            if (out_super) *out_super = Value::null;
            return false;
        }
        GCMap m = GCManager::Maps.Get(current.ItemIndex());
        Value v;
        if (m.TryGet(key, &v)) {
            Value isa;
            m.TryGet(Value::magicIsA, &isa);
            if (out_value) *out_value = v;
            if (out_super) *out_super = isa;
            return true;
        }
        Value isa;
        if (!m.TryGet(Value::magicIsA, &isa)) {
            if (out_value) *out_value = Value::null;
            if (out_super) *out_super = Value::null;
            return false;
        }
        current = isa;
    }
    if (out_value) *out_value = Value::null;
    if (out_super) *out_super = Value::null;
    return false;
}

bool Value::MapSet(Value key, Value value) const {
    Value map_val = *this;
    if (!map_val.IsMap()) return false;
    GCMap m = GCManager::Maps.Get(map_val.ItemIndex());
    if (m.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen map"); return false; }
    m.Set(key, value);
    return true;
}

bool Value::MapRemove(Value key) const {
    Value map_val = *this;
    if (!map_val.IsMap()) return false;
    GCMap m = GCManager::Maps.Get(map_val.ItemIndex());
    if (m.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen map"); return false; }
    return m.Remove(key);
}

bool Value::HasKey(Value key) const {
    Value map_val = *this;
    if (!map_val.IsMap()) return false;
    GCMap m = GCManager::Maps.Get(map_val.ItemIndex());
    Value v;
    return m.TryGet(key, &v);
}

// ── Utilities ───────────────────────────────────────────────────────────

void Value::Clear() const {
    Value map_val = *this;
    if (!map_val.IsMap()) return;
    GCMap m = GCManager::Maps.Get(map_val.ItemIndex());
    if (m.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen map"); return; }
    m.Clear();
}

Value map_copy(Value map_val) {
    if (!map_val.IsMap()) return Value::null;
    GCMap src = GCManager::Maps.Get(map_val.ItemIndex());
    Value newMap = Value::make_map(src.Count());
    GCMap dst = GCManager::Maps.Get(newMap.ItemIndex());
    for (int i = src.NextEntry(-1); i != -1; i = src.NextEntry(i))
        dst.Set(src.KeyAt(i), src.ValueAt(i));
    return newMap;
}

Value map_concat(Value a, Value b) {
    Value result = Value::make_empty_map();
    GCMap dst = GCManager::Maps.Get(result.ItemIndex());
    if (a.IsMap()) {
        GCMap la = GCManager::Maps.Get(a.ItemIndex());
        for (int i = la.NextEntry(-1); i != -1; i = la.NextEntry(i))
            dst.Set(la.KeyAt(i), la.ValueAt(i));
    }
    if (b.IsMap()) {
        GCMap lb = GCManager::Maps.Get(b.ItemIndex());
        for (int i = lb.NextEntry(-1); i != -1; i = lb.NextEntry(i))
            dst.Set(lb.KeyAt(i), lb.ValueAt(i));
    }
    return result;
}

// Instance wrapper mirroring cs/Value.cs; the free map_concat above remains the
// shared implementation used by operator+ (value_add).
Value Value::MapConcat(Value b) const { return map_concat(*this, b); }

bool  map_needs_expansion(Value /*map_val*/) { return false; }
bool  map_expand_capacity(Value /*map_val*/) { return true; }
Value map_with_expanded_capacity(Value m)    { return m; }

// ── Iteration (legacy struct-based) ─────────────────────────────────────

MapIterator Value::Iterator() const {
    Value map_val = *this;
    MapIterator it;
    if (map_val.IsMap()) {
        it.map_idx = map_val.ItemIndex();
        it.iter    = -1;
    } else {
        it.map_idx = -1;
        it.iter    = Value::MAP_ITER_DONE;
    }
    return it;
}

bool map_iterator_next(MapIterator* iter, Value* out_key, Value* out_value) {
    if (!iter || iter->map_idx < 0 || iter->iter == Value::MAP_ITER_DONE) {
        if (out_key)   *out_key   = Value::null;
        if (out_value) *out_value = Value::null;
        return false;
    }
    GCMap m = GCManager::Maps.Get(iter->map_idx);
    int next = m.NextEntry(iter->iter);
    if (next < 0) { iter->iter = Value::MAP_ITER_DONE; return false; }
    iter->iter = next;
    if (out_key)   *out_key   = m.KeyAt(next);
    if (out_value) *out_value = m.ValueAt(next);
    return true;
}

Value Value::NthEntry(int n) const {
    Value map_val = *this;
    if (!map_val.IsMap()) return Value::null;
    GCMap m = GCManager::Maps.Get(map_val.ItemIndex());
    int count = 0;
    for (int i = m.NextEntry(-1); i != -1; i = m.NextEntry(i)) {
        if (count == n) {
            Value entry = Value::make_map(4);
            entry.MapSet(Value::make_string("key"),   m.KeyAt(i));
            entry.MapSet(Value::make_string("value"), m.ValueAt(i));
            return entry;
        }
        count++;
    }
    return Value::null;
}

// ── New iterator interface ──────────────────────────────────────────────

int Value::IterNext(int iter) const {
    Value map_val = *this;
    if (!map_val.IsMap() || iter == Value::MAP_ITER_DONE) return Value::MAP_ITER_DONE;
    GCMap m = GCManager::Maps.Get(map_val.ItemIndex());
    int next = m.NextEntry(iter);
    return next < 0 ? Value::MAP_ITER_DONE : next;
}

Value Value::IterEntry(int iter) const {
    Value map_val = *this;
    if (!map_val.IsMap() || iter == Value::MAP_ITER_DONE) return Value::null;
    GCMap m = GCManager::Maps.Get(map_val.ItemIndex());
    Value entry = Value::make_map(4);
    entry.MapSet(Value::make_string("key"),   m.KeyAt(iter));
    entry.MapSet(Value::make_string("value"), m.ValueAt(iter));
    return entry;
}

// ── Hash & display ──────────────────────────────────────────────────────

uint32_t map_hash(Value map_val) {
    if (!map_val.IsMap()) return 0;
    return uint64_hash(map_val.bits);
}

Value map_to_string(Value map_val, void* vm) {
    return code_form(map_val, vm, 3);
}


// ── VarMap operations (now Value:: static methods) ──────────────────────

// IsNull(VarMapBacking) is an ADL-only hidden friend; called from a free
// function here so ordinary lookup doesn't shadow it with Value::IsNull().
static inline bool vmb_is_null(const VarMapBacking& v) { return IsNull(v); }

Value Value::make_varmap(List<Value> registers, List<Value> names, int firstIndex, int count) {
    return VarMapBacking::NewVarMap(registers, names, firstIndex, firstIndex + count - 1);
}

void Value::MapToRegister(Value var_name, List<Value> registers, int reg_index) const {
    Value map_val = *this;
    if (!map_val.IsMap()) return;
    int32_t idx = map_val.ItemIndex();
    GCMap m = GCManager::Maps.Get(idx);
    if (!vmb_is_null(m._vmb)) m._vmb.MapToRegister(idx, var_name, registers, reg_index);
}

void Value::Gather() const {
    Value map_val = *this;
    if (!map_val.IsMap()) return;
    int32_t idx = map_val.ItemIndex();
    GCMap m = GCManager::Maps.Get(idx);
    if (!vmb_is_null(m._vmb)) m._vmb.Gather(idx);
}

void Value::Rebind(List<Value> registers, List<Value> names) const {
    Value map_val = *this;
    if (!map_val.IsMap()) return;
    int32_t idx = map_val.ItemIndex();
    GCMap m = GCManager::Maps.Get(idx);
    if (!vmb_is_null(m._vmb)) m._vmb.Rebind(idx, registers, names);
}

}  // namespace MiniScript
