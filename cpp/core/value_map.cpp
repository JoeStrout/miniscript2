// value_map.cpp — thin wrappers over GCManager.Maps.

#include "value_map.h"
#include "value_string.h"
#include "value_list.h"
#include "vm_error.h"
#include "GCManager.g.h"
#include "VarMap.g.h"
#include "hashing.h"
#include <cstring>

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

int Value::map_count(Value map_val) {
    if (!map_val.IsMap()) return 0;
    GCMap m = GCManager::Maps.Get(Value::value_item_index(map_val));
    return m.Count();
}

int map_capacity(Value map_val) {
    if (!map_val.IsMap()) return 0;
    GCMap m = GCManager::Maps.Get(Value::value_item_index(map_val));
    // Best-effort: GCMap exposes size only via Count(). Return Count*2 as a
    // proxy since the underlying table maintains 50% load.
    return m.Count() * 2;
}

Value Value::map_get(Value map_val, Value key) {
    if (!map_val.IsMap()) return Value::null;
    GCMap m = GCManager::Maps.Get(Value::value_item_index(map_val));
    Value out;
    if (m.TryGet(key, &out)) return out;
    return Value::null;
}

bool Value::map_try_get(Value map_val, Value key, Value* out_value) {
    if (!map_val.IsMap()) { if (out_value) *out_value = Value::null; return false; }
    GCMap m = GCManager::Maps.Get(Value::value_item_index(map_val));
    Value out;
    bool ok = m.TryGet(key, &out);
    if (out_value) *out_value = ok ? out : Value::null;
    return ok;
}

// Walks the __isa chain.
bool Value::map_lookup(Value map_val, Value key, Value* out_value) {
    Value current = map_val;
    for (int depth = 0; depth < 256; depth++) {
        if (!current.IsMap()) { if (out_value) *out_value = Value::null; return false; }
        GCMap m = GCManager::Maps.Get(Value::value_item_index(current));
        Value v;
        if (m.TryGet(key, &v)) { if (out_value) *out_value = v; return true; }
        Value isa;
        if (!m.TryGet(Value::magicIsA, &isa)) { if (out_value) *out_value = Value::null; return false; }
        current = isa;
    }
    if (out_value) *out_value = Value::null;
    return false;
}

bool Value::map_lookup_with_origin(Value map_val, Value key, Value* out_value, Value* out_super) {
    Value current = map_val;
    for (int depth = 0; depth < 256; depth++) {
        if (!current.IsMap()) {
            if (out_value) *out_value = Value::null;
            if (out_super) *out_super = Value::null;
            return false;
        }
        GCMap m = GCManager::Maps.Get(Value::value_item_index(current));
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

bool Value::map_set(Value map_val, Value key, Value value) {
    if (!map_val.IsMap()) return false;
    GCMap m = GCManager::Maps.Get(Value::value_item_index(map_val));
    if (m.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen map"); return false; }
    m.Set(key, value);
    return true;
}

bool Value::map_remove(Value map_val, Value key) {
    if (!map_val.IsMap()) return false;
    GCMap m = GCManager::Maps.Get(Value::value_item_index(map_val));
    if (m.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen map"); return false; }
    return m.Remove(key);
}

bool Value::map_has_key(Value map_val, Value key) {
    if (!map_val.IsMap()) return false;
    GCMap m = GCManager::Maps.Get(Value::value_item_index(map_val));
    Value v;
    return m.TryGet(key, &v);
}

// ── Utilities ───────────────────────────────────────────────────────────

void Value::map_clear(Value map_val) {
    if (!map_val.IsMap()) return;
    GCMap m = GCManager::Maps.Get(Value::value_item_index(map_val));
    if (m.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen map"); return; }
    m.Clear();
}

Value map_copy(Value map_val) {
    if (!map_val.IsMap()) return Value::null;
    GCMap src = GCManager::Maps.Get(Value::value_item_index(map_val));
    Value newMap = Value::make_map(src.Count());
    GCMap dst = GCManager::Maps.Get(Value::value_item_index(newMap));
    for (int i = src.NextEntry(-1); i != -1; i = src.NextEntry(i))
        dst.Set(src.KeyAt(i), src.ValueAt(i));
    return newMap;
}

Value map_concat(Value a, Value b) {
    Value result = Value::make_empty_map();
    GCMap dst = GCManager::Maps.Get(Value::value_item_index(result));
    if (a.IsMap()) {
        GCMap la = GCManager::Maps.Get(Value::value_item_index(a));
        for (int i = la.NextEntry(-1); i != -1; i = la.NextEntry(i))
            dst.Set(la.KeyAt(i), la.ValueAt(i));
    }
    if (b.IsMap()) {
        GCMap lb = GCManager::Maps.Get(Value::value_item_index(b));
        for (int i = lb.NextEntry(-1); i != -1; i = lb.NextEntry(i))
            dst.Set(lb.KeyAt(i), lb.ValueAt(i));
    }
    return result;
}

bool  map_needs_expansion(Value /*map_val*/) { return false; }
bool  map_expand_capacity(Value /*map_val*/) { return true; }
Value map_with_expanded_capacity(Value m)    { return m; }

// ── Iteration (legacy struct-based) ─────────────────────────────────────

MapIterator Value::map_iterator(Value map_val) {
    MapIterator it;
    if (map_val.IsMap()) {
        it.map_idx = Value::value_item_index(map_val);
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

Value map_nth_entry(Value map_val, int n) {
    if (!map_val.IsMap()) return Value::null;
    GCMap m = GCManager::Maps.Get(Value::value_item_index(map_val));
    int count = 0;
    for (int i = m.NextEntry(-1); i != -1; i = m.NextEntry(i)) {
        if (count == n) {
            Value entry = Value::make_map(4);
            Value::map_set(entry, Value::make_string("key"),   m.KeyAt(i));
            Value::map_set(entry, Value::make_string("value"), m.ValueAt(i));
            return entry;
        }
        count++;
    }
    return Value::null;
}

// ── New iterator interface ──────────────────────────────────────────────

int Value::map_iter_next(Value map_val, int iter) {
    if (!map_val.IsMap() || iter == Value::MAP_ITER_DONE) return Value::MAP_ITER_DONE;
    GCMap m = GCManager::Maps.Get(Value::value_item_index(map_val));
    int next = m.NextEntry(iter);
    return next < 0 ? Value::MAP_ITER_DONE : next;
}

Value Value::map_iter_entry(Value map_val, int iter) {
    if (!map_val.IsMap() || iter == Value::MAP_ITER_DONE) return Value::null;
    GCMap m = GCManager::Maps.Get(Value::value_item_index(map_val));
    Value entry = Value::make_map(4);
    Value::map_set(entry, Value::make_string("key"),   m.KeyAt(iter));
    Value::map_set(entry, Value::make_string("value"), m.ValueAt(iter));
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

void Value::varmap_map_to_register(Value map_val, Value var_name, List<Value> registers, int reg_index) {
    if (!map_val.IsMap()) return;
    int32_t idx = Value::value_item_index(map_val);
    GCMap m = GCManager::Maps.Get(idx);
    if (!vmb_is_null(m._vmb)) m._vmb.MapToRegister(idx, var_name, registers, reg_index);
}

void Value::varmap_gather(Value map_val) {
    if (!map_val.IsMap()) return;
    int32_t idx = Value::value_item_index(map_val);
    GCMap m = GCManager::Maps.Get(idx);
    if (!vmb_is_null(m._vmb)) m._vmb.Gather(idx);
}

void Value::varmap_rebind(Value map_val, List<Value> registers, List<Value> names) {
    if (!map_val.IsMap()) return;
    int32_t idx = Value::value_item_index(map_val);
    GCMap m = GCManager::Maps.Get(idx);
    if (!vmb_is_null(m._vmb)) m._vmb.Rebind(idx, registers, names);
}
