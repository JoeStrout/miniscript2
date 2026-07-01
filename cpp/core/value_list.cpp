// value_list.cpp — thin wrappers over GCManager.Lists.

#include "value_list.h"
#include "value_string.h"
#include "vm_error.h"
#include "GCManager.g.h"
#include <algorithm>
#include <cstring>
#include <vector>

using MiniScript::GCManager;
using MiniScript::GCList;


// ── Creation ────────────────────────────────────────────────────────────

Value Value::make_list(int initial_capacity) {
    return GCManager::NewList(initial_capacity);
}

Value make_empty_list(void) {
    return GCManager::NewList(0);
}

// ── Access ──────────────────────────────────────────────────────────────

int Value::list_count(Value list_val) {
    if (!list_val.IsList()) return 0;
    GCList l = GCManager::Lists.Get(list_val.ItemIndex());
    return l.Count();
}

int list_capacity(Value list_val) {
    if (!list_val.IsList()) return 0;
    GCList l = GCManager::Lists.Get(list_val.ItemIndex());
    // No capacity accessor through the public API; report the element count.
    return l.Count();
}

Value Value::list_get(Value list_val, int index) {
    if (!list_val.IsList()) return Value::null;
    GCList l = GCManager::Lists.Get(list_val.ItemIndex());
    return l.Get(index);
}

void Value::list_set(Value list_val, int index, Value item) {
    if (!list_val.IsList()) return;
    int idx = list_val.ItemIndex();
    GCList l = GCManager::Lists.Get(idx);
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    bool wasComputed = l.Computed;
    l.Set(index, item);
    if (wasComputed) GCManager::Lists.Set(idx, l);  // write back materialization
}

void Value::list_push(Value list_val, Value item) {
    if (!list_val.IsList()) return;
    int idx = list_val.ItemIndex();
    GCList l = GCManager::Lists.Get(idx);
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    bool wasComputed = l.Computed;
    l.Push(item);
    if (wasComputed) GCManager::Lists.Set(idx, l);  // write back materialization
}

Value Value::list_pop(Value list_val) {
    if (!list_val.IsList()) return Value::null;
    int idx = list_val.ItemIndex();
    GCList l = GCManager::Lists.Get(idx);
    if (l.Count() == 0) return Value::null;
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return Value::null; }
    bool wasComputed = l.Computed;
    Value result = l.Pop();
    if (wasComputed) GCManager::Lists.Set(idx, l);  // write back length/materialization
    return result;
}

Value Value::list_pull(Value list_val) {
    if (!list_val.IsList()) return Value::null;
    int idx = list_val.ItemIndex();
    GCList l = GCManager::Lists.Get(idx);
    if (l.Count() == 0) return Value::null;
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return Value::null; }
    bool wasComputed = l.Computed;
    Value result = l.Pull();
    if (wasComputed) GCManager::Lists.Set(idx, l);  // write back materialization
    return result;
}

void Value::list_insert(Value list_val, int index, Value item) {
    if (!list_val.IsList()) return;
    int idx = list_val.ItemIndex();
    GCList l = GCManager::Lists.Get(idx);
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    bool wasComputed = l.Computed;
    l.Insert(index, item);
    if (wasComputed) GCManager::Lists.Set(idx, l);  // write back materialization
}

bool Value::list_remove(Value list_val, int index) {
    if (!list_val.IsList()) return false;
    int idx = list_val.ItemIndex();
    GCList l = GCManager::Lists.Get(idx);
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return false; }
    bool wasComputed = l.Computed;
    bool result = l.Remove(index);
    if (wasComputed) GCManager::Lists.Set(idx, l);  // write back materialization
    return result;
}

// ── Search ──────────────────────────────────────────────────────────────

int Value::list_indexOf(Value list_val, Value item, int afterIdx) {
    if (!list_val.IsList()) return -1;
    GCList l = GCManager::Lists.Get(list_val.ItemIndex());
    return l.IndexOf(item, afterIdx);
}

bool list_contains(Value list_val, Value item) {
    return Value::list_indexOf(list_val, item, -1) >= 0;
}

// ── Utilities ───────────────────────────────────────────────────────────

void list_clear(Value list_val) {
    if (!list_val.IsList()) return;
    int idx = list_val.ItemIndex();
    GCList l = GCManager::Lists.Get(idx);
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    l.Init();                       // replace with a fresh, empty (materialized) list
    GCManager::Lists.Set(idx, l);   // write back the new Items / cleared Computed flag
}

Value list_copy(Value list_val) {
    if (!list_val.IsList()) return Value::null;
    GCList src = GCManager::Lists.Get(list_val.ItemIndex());
    int n = src.Count();
    Value newList = Value::make_list(n);
    GCList dst = GCManager::Lists.Get(newList.ItemIndex());
    for (int i = 0; i < n; i++) dst.Push(src.Get(i));
    return newList;
}

Value Value::list_slice(Value list_val, int start, int end) {
    if (!list_val.IsList()) return Value::make_list(0);
    GCList src = GCManager::Lists.Get(list_val.ItemIndex());
    int n = src.Count();
    if (start < 0) start += n;
    if (end   < 0) end   += n;
    if (start < 0) start = 0;
    if (end > n) end = n;
    if (start >= end) return Value::make_list(0);
    Value newList = Value::make_list(end - start);
    GCList dst = GCManager::Lists.Get(newList.ItemIndex());
    for (int i = start; i < end; i++) dst.Push(src.Get(i));
    return newList;
}

Value list_concat(Value a, Value b) {
    int na = a.IsList() ? GCManager::Lists.Get(a.ItemIndex()).Count() : 0;
    int nb = b.IsList() ? GCManager::Lists.Get(b.ItemIndex()).Count() : 0;
    Value result = Value::make_list(na + nb);
    GCList dst = GCManager::Lists.Get(result.ItemIndex());
    if (a.IsList()) {
        GCList la = GCManager::Lists.Get(a.ItemIndex());
        for (int i = 0; i < na; i++) dst.Push(la.Get(i));
    }
    if (b.IsList()) {
        GCList lb = GCManager::Lists.Get(b.ItemIndex());
        for (int i = 0; i < nb; i++) dst.Push(lb.Get(i));
    }
    return result;
}

// ── Sorting ─────────────────────────────────────────────────────────────

namespace {

int compare_values(Value a, Value b) {
    if (a.IsNumber() && b.IsNumber()) {
        double da = a.NumericVal(), db = b.NumericVal();
        if (da < db) return -1;
        if (da > db) return 1;
        return 0;
    }
    if (a.IsString() && b.IsString()) return string_compare(a, b);
    int ta = a.IsNumber() ? 0 : a.IsString() ? 1 : 2;
    int tb = b.IsNumber() ? 0 : b.IsString() ? 1 : 2;
    return (ta < tb) ? -1 : (ta > tb) ? 1 : 0;
}

} // namespace

void Value::list_sort(Value list_val, bool ascending) {
    if (!list_val.IsList()) return;
    int lidx = list_val.ItemIndex();
    GCList l = GCManager::Lists.Get(lidx);
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    bool wasComputed = l.Computed;
    l.Materialize();
    int n = l.Count();
    if (n >= 2) {
        std::vector<Value> tmp((size_t)n, Value::null);
        for (int i = 0; i < n; i++) tmp[i] = l.Get(i);
        std::sort(tmp.begin(), tmp.end(),
            [ascending](Value a, Value b) {
                int c = compare_values(a, b);
                return ascending ? c < 0 : c > 0;
            });
        for (int i = 0; i < n; i++) l.Set(i, tmp[i]);
    }
    if (wasComputed) GCManager::Lists.Set(lidx, l);  // write back materialization
}

extern Value map_get(Value map_val, Value key);

void Value::list_sort_by_key(Value list_val, Value byKey, bool ascending) {
    if (!list_val.IsList()) return;
    int lidx = list_val.ItemIndex();
    GCList l = GCManager::Lists.Get(lidx);
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    bool wasComputed = l.Computed;
    l.Materialize();
    int n = l.Count();
    if (n < 2) { if (wasComputed) GCManager::Lists.Set(lidx, l); return; }

    // Decorate with sort keys so we don't recompute per comparison.
    std::vector<Value> elems((size_t)n, Value::null);
    std::vector<Value> keys((size_t)n, Value::null);
    for (int i = 0; i < n; i++) {
        Value e = l.Get(i);
        elems[i] = e;
        if (e.IsMap()) keys[i] = Value::map_get(e, byKey);
        else if (e.IsList() && byKey.IsNumber()) keys[i] = Value::list_get(e, (int)byKey.NumericVal());
    }
    std::vector<int> idx((size_t)n, 0);
    for (int i = 0; i < n; i++) idx[i] = i;
    std::sort(idx.begin(), idx.end(),
        [&keys, ascending](int a, int b) {
            int c = compare_values(keys[a], keys[b]);
            return ascending ? c < 0 : c > 0;
        });
    for (int i = 0; i < n; i++) l.Set(i, elems[idx[i]]);
    if (wasComputed) GCManager::Lists.Set(lidx, l);  // write back materialization
}

// ── Hash & display ──────────────────────────────────────────────────────

uint32_t list_hash(Value list_val) {
    if (!list_val.IsList()) return 0;
    // Identity hash — content hashing risks O(n²) recursion on cycles.
    return uint64_hash(list_val.bits);
}

Value list_to_string(Value list_val, void* vm) {
    return code_form(list_val, vm, 3);
}

