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

Value make_list(int initial_capacity) {
    return GCManager::NewList(initial_capacity);
}

Value make_empty_list(void) {
    return GCManager::NewList(0);
}

// ── Access ──────────────────────────────────────────────────────────────

int list_count(Value list_val) {
    if (!is_list(list_val)) return 0;
    GCList l = GCManager::Lists.Get(value_item_index(list_val));
    return l.Count();
}

int list_capacity(Value list_val) {
    if (!is_list(list_val)) return 0;
    GCList l = GCManager::Lists.Get(value_item_index(list_val));
    // No capacity accessor through the public API; report the element count.
    return l.Count();
}

Value list_get(Value list_val, int index) {
    if (!is_list(list_val)) return Value::null;
    GCList l = GCManager::Lists.Get(value_item_index(list_val));
    return l.Get(index);
}

void list_set(Value list_val, int index, Value item) {
    if (!is_list(list_val)) return;
    int idx = value_item_index(list_val);
    GCList l = GCManager::Lists.Get(idx);
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    bool wasComputed = l.Computed;
    l.Set(index, item);
    if (wasComputed) GCManager::Lists.Set(idx, l);  // write back materialization
}

void list_push(Value list_val, Value item) {
    if (!is_list(list_val)) return;
    int idx = value_item_index(list_val);
    GCList l = GCManager::Lists.Get(idx);
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    bool wasComputed = l.Computed;
    l.Push(item);
    if (wasComputed) GCManager::Lists.Set(idx, l);  // write back materialization
}

Value list_pop(Value list_val) {
    if (!is_list(list_val)) return Value::null;
    int idx = value_item_index(list_val);
    GCList l = GCManager::Lists.Get(idx);
    if (l.Count() == 0) return Value::null;
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return Value::null; }
    bool wasComputed = l.Computed;
    Value result = l.Pop();
    if (wasComputed) GCManager::Lists.Set(idx, l);  // write back length/materialization
    return result;
}

Value list_pull(Value list_val) {
    if (!is_list(list_val)) return Value::null;
    int idx = value_item_index(list_val);
    GCList l = GCManager::Lists.Get(idx);
    if (l.Count() == 0) return Value::null;
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return Value::null; }
    bool wasComputed = l.Computed;
    Value result = l.Pull();
    if (wasComputed) GCManager::Lists.Set(idx, l);  // write back materialization
    return result;
}

void list_insert(Value list_val, int index, Value item) {
    if (!is_list(list_val)) return;
    int idx = value_item_index(list_val);
    GCList l = GCManager::Lists.Get(idx);
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    bool wasComputed = l.Computed;
    l.Insert(index, item);
    if (wasComputed) GCManager::Lists.Set(idx, l);  // write back materialization
}

bool list_remove(Value list_val, int index) {
    if (!is_list(list_val)) return false;
    int idx = value_item_index(list_val);
    GCList l = GCManager::Lists.Get(idx);
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return false; }
    bool wasComputed = l.Computed;
    bool result = l.Remove(index);
    if (wasComputed) GCManager::Lists.Set(idx, l);  // write back materialization
    return result;
}

// ── Search ──────────────────────────────────────────────────────────────

int list_indexOf(Value list_val, Value item, int afterIdx) {
    if (!is_list(list_val)) return -1;
    GCList l = GCManager::Lists.Get(value_item_index(list_val));
    return l.IndexOf(item, afterIdx);
}

bool list_contains(Value list_val, Value item) {
    return list_indexOf(list_val, item, -1) >= 0;
}

// ── Utilities ───────────────────────────────────────────────────────────

void list_clear(Value list_val) {
    if (!is_list(list_val)) return;
    int idx = value_item_index(list_val);
    GCList l = GCManager::Lists.Get(idx);
    if (l.Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    l.Init();                       // replace with a fresh, empty (materialized) list
    GCManager::Lists.Set(idx, l);   // write back the new Items / cleared Computed flag
}

Value list_copy(Value list_val) {
    if (!is_list(list_val)) return Value::null;
    GCList src = GCManager::Lists.Get(value_item_index(list_val));
    int n = src.Count();
    Value newList = make_list(n);
    GCList dst = GCManager::Lists.Get(value_item_index(newList));
    for (int i = 0; i < n; i++) dst.Push(src.Get(i));
    return newList;
}

Value list_slice(Value list_val, int start, int end) {
    if (!is_list(list_val)) return make_list(0);
    GCList src = GCManager::Lists.Get(value_item_index(list_val));
    int n = src.Count();
    if (start < 0) start += n;
    if (end   < 0) end   += n;
    if (start < 0) start = 0;
    if (end > n) end = n;
    if (start >= end) return make_list(0);
    Value newList = make_list(end - start);
    GCList dst = GCManager::Lists.Get(value_item_index(newList));
    for (int i = start; i < end; i++) dst.Push(src.Get(i));
    return newList;
}

Value list_concat(Value a, Value b) {
    int na = is_list(a) ? GCManager::Lists.Get(value_item_index(a)).Count() : 0;
    int nb = is_list(b) ? GCManager::Lists.Get(value_item_index(b)).Count() : 0;
    Value result = make_list(na + nb);
    GCList dst = GCManager::Lists.Get(value_item_index(result));
    if (is_list(a)) {
        GCList la = GCManager::Lists.Get(value_item_index(a));
        for (int i = 0; i < na; i++) dst.Push(la.Get(i));
    }
    if (is_list(b)) {
        GCList lb = GCManager::Lists.Get(value_item_index(b));
        for (int i = 0; i < nb; i++) dst.Push(lb.Get(i));
    }
    return result;
}

// ── Sorting ─────────────────────────────────────────────────────────────

namespace {

int compare_values(Value a, Value b) {
    if (is_number(a) && is_number(b)) {
        double da = numeric_val(a), db = numeric_val(b);
        if (da < db) return -1;
        if (da > db) return 1;
        return 0;
    }
    if (is_string(a) && is_string(b)) return string_compare(a, b);
    int ta = is_number(a) ? 0 : is_string(a) ? 1 : 2;
    int tb = is_number(b) ? 0 : is_string(b) ? 1 : 2;
    return (ta < tb) ? -1 : (ta > tb) ? 1 : 0;
}

} // namespace

void list_sort(Value list_val, bool ascending) {
    if (!is_list(list_val)) return;
    int lidx = value_item_index(list_val);
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

void list_sort_by_key(Value list_val, Value byKey, bool ascending) {
    if (!is_list(list_val)) return;
    int lidx = value_item_index(list_val);
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
        if (is_map(e)) keys[i] = map_get(e, byKey);
        else if (is_list(e) && is_number(byKey)) keys[i] = list_get(e, (int)numeric_val(byKey));
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
    if (!is_list(list_val)) return 0;
    // Identity hash — content hashing risks O(n²) recursion on cycles.
    return uint64_hash(list_val.bits);
}

Value list_to_string(Value list_val, void* vm) {
    return code_form(list_val, vm, 3);
}

