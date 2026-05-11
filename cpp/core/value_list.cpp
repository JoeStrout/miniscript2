// value_list.cpp — thin wrappers over GCManager.Lists.

#include "value_list.h"
#include "value_string.h"
#include "vm_error.h"
#include "GCManager.h"
#include <algorithm>
#include <cstring>

using MiniScript::GCManager;
using MiniScript::GCList;
using MiniScript::Value;

namespace {

GCList* as_list_ptr(Value v) {
    if (!is_list(v)) return nullptr;
    return &GCManager::Instance().Lists.Get(value_item_index(v));
}

} // namespace

extern "C" {

// ── Creation ────────────────────────────────────────────────────────────

Value make_list(int initial_capacity) {
    return GCManager::Instance().NewList(initial_capacity);
}

Value make_empty_list(void) {
    return GCManager::Instance().NewList(0);
}

// ── Access ──────────────────────────────────────────────────────────────

int list_count(Value list_val) {
    GCList* l = as_list_ptr(list_val);
    return l ? l->Count() : 0;
}

int list_capacity(Value list_val) {
    GCList* l = as_list_ptr(list_val);
    return l ? (int)l->Items.capacity() : 0;
}

Value list_get(Value list_val, int index) {
    GCList* l = as_list_ptr(list_val);
    return l ? l->Get(index) : val_null;
}

void list_set(Value list_val, int index, Value item) {
    GCList* l = as_list_ptr(list_val);
    if (!l) return;
    if (l->Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    l->Set(index, item);
}

void list_push(Value list_val, Value item) {
    GCList* l = as_list_ptr(list_val);
    if (!l) return;
    if (l->Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    l->Push(item);
}

Value list_pop(Value list_val) {
    GCList* l = as_list_ptr(list_val);
    if (!l || l->Items.empty()) return val_null;
    if (l->Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return val_null; }
    Value v = l->Items.back();
    l->Items.pop_back();
    return v;
}

Value list_pull(Value list_val) {
    GCList* l = as_list_ptr(list_val);
    if (!l || l->Items.empty()) return val_null;
    if (l->Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return val_null; }
    Value v = l->Items.front();
    l->Items.erase(l->Items.begin());
    return v;
}

void list_insert(Value list_val, int index, Value item) {
    GCList* l = as_list_ptr(list_val);
    if (!l) return;
    if (l->Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    int n = (int)l->Items.size();
    if (index < 0) index += n + 1;
    if (index < 0) index = 0;
    if (index > n) index = n;
    l->Items.insert(l->Items.begin() + index, item);
}

bool list_remove(Value list_val, int index) {
    GCList* l = as_list_ptr(list_val);
    if (!l) return false;
    if (l->Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return false; }
    int n = (int)l->Items.size();
    if (index < 0) index += n;
    if (index < 0 || index >= n) return false;
    l->Items.erase(l->Items.begin() + index);
    return true;
}

// ── Search ──────────────────────────────────────────────────────────────

int list_indexOf(Value list_val, Value item, int afterIdx) {
    GCList* l = as_list_ptr(list_val);
    if (!l) return -1;
    int n = (int)l->Items.size();
    for (int i = afterIdx + 1; i < n; i++) {
        if (value_equal(l->Items[i], item)) return i;
    }
    return -1;
}

bool list_contains(Value list_val, Value item) {
    return list_indexOf(list_val, item, -1) >= 0;
}

// ── Utilities ───────────────────────────────────────────────────────────

void list_clear(Value list_val) {
    GCList* l = as_list_ptr(list_val);
    if (!l) return;
    if (l->Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    l->Items.clear();
}

Value list_copy(Value list_val) {
    GCList* src = as_list_ptr(list_val);
    if (!src) return val_null;
    int n = (int)src->Items.size();
    Value newList = make_list(n);
    GCList& dst = GCManager::Instance().Lists.Get(value_item_index(newList));
    dst.Items = src->Items;
    return newList;
}

Value list_slice(Value list_val, int start, int end) {
    GCList* src = as_list_ptr(list_val);
    if (!src) return make_list(0);
    int n = (int)src->Items.size();
    if (start < 0) start += n;
    if (end   < 0) end   += n;
    if (start < 0) start = 0;
    if (end > n) end = n;
    if (start >= end) return make_list(0);
    Value newList = make_list(end - start);
    GCList& dst = GCManager::Instance().Lists.Get(value_item_index(newList));
    dst.Items.assign(src->Items.begin() + start, src->Items.begin() + end);
    return newList;
}

Value list_concat(Value a, Value b) {
    GCList* la = as_list_ptr(a);
    GCList* lb = as_list_ptr(b);
    int na = la ? (int)la->Items.size() : 0;
    int nb = lb ? (int)lb->Items.size() : 0;
    Value result = make_list(na + nb);
    GCList& dst = GCManager::Instance().Lists.Get(value_item_index(result));
    if (la) dst.Items.insert(dst.Items.end(), la->Items.begin(), la->Items.end());
    if (lb) dst.Items.insert(dst.Items.end(), lb->Items.begin(), lb->Items.end());
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
    GCList* l = as_list_ptr(list_val);
    if (!l) return;
    if (l->Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    std::sort(l->Items.begin(), l->Items.end(),
        [ascending](Value a, Value b) {
            int c = compare_values(a, b);
            return ascending ? c < 0 : c > 0;
        });
}

extern Value map_get(Value map_val, Value key);

void list_sort_by_key(Value list_val, Value byKey, bool ascending) {
    GCList* l = as_list_ptr(list_val);
    if (!l) return;
    if (l->Frozen) { vm_raise_runtime_error("Attempt to modify a frozen list"); return; }
    int n = (int)l->Items.size();
    if (n < 2) return;

    // Decorate with sort keys so we don't recompute per comparison.
    std::vector<Value> keys((size_t)n, val_null);
    for (int i = 0; i < n; i++) {
        Value e = l->Items[i];
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
    std::vector<Value> sorted((size_t)n, val_null);
    for (int i = 0; i < n; i++) sorted[i] = l->Items[idx[i]];
    l->Items = std::move(sorted);
}

// ── Hash & display ──────────────────────────────────────────────────────

uint32_t list_hash(Value list_val) {
    GCList* l = as_list_ptr(list_val);
    if (!l) return 0;
    // Identity hash — content hashing risks O(n²) recursion on cycles.
    return uint64_hash((uint64_t)list_val);
}

Value list_to_string(Value list_val, void* vm) {
    return code_form(list_val, vm, 3);
}

} // extern "C"
