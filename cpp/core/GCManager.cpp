// GCManager.cpp — implementation of the central GC coordinator,
// string interning, and the mark/sweep cycle.

#include "GCManager.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace MiniScript {

// ── Singleton ───────────────────────────────────────────────────────────

GCManager& GCManager::Instance() {
    static GCManager instance;
    return instance;
}

GCManager::GCManager() {
    _sets[STRING_SET]  = &Strings;
    _sets[LIST_SET]    = &Lists;
    _sets[MAP_SET]     = &Maps;
    _sets[ERROR_SET]   = &Errors;
    _sets[FUNCREF_SET] = &FuncRefs;
}

// ── String interning ────────────────────────────────────────────────────

void StringsGCSet::BeforeSlotFreed(int idx, GCManager& gc) {
    GCString& s = _items[size_t(idx)];
    if (s.Interned) gc.UninternString(s);
}

void GCManager::UninternString(const GCString& s) {
    if (!s.Storage) return;
    StringRef key{ s.Storage->data, s.Storage->lenB };
    _internTable.erase(key);
}

Value GCManager::InternOrAllocateString(const char* data, int length) {
    bool intern = (length <= INTERN_MAX_LEN);
    if (intern) {
        StringRef key{ data, length };
        std::unordered_map<StringRef, int, StringRefHash>::iterator it = _internTable.find(key);
        if (it != _internTable.end()) {
            return MakeGC(STRING_SET, it->second);
        }
    }
    StringStorage* storage = ss_createWithLength(length, std::malloc);
    if (storage) {
        std::memcpy(storage->data, data, size_t(length));
    }
    int idx = Strings.New();
    GCString& s = Strings.Get(idx);
    s.Storage  = storage;
    s.Interned = intern;
    if (intern && storage) {
        StringRef storedKey{ storage->data, storage->lenB };
        _internTable.insert(std::make_pair(storedKey, idx));
    }
    return MakeGC(STRING_SET, idx);
}

Value GCManager::AdoptString(StringStorage* storage) {
    if (!storage || storage->lenB == 0) {
        if (storage) std::free(storage);
        return val_empty_string;
    }
    int length = storage->lenB;
    if (length <= 5) {
        // Result fits a tiny string; emit one and free the storage.
        Value v = TINY_STRING_TAG | uint64_t(uint8_t(length));
        for (int i = 0; i < length; i++)
            v |= uint64_t(uint8_t(storage->data[i])) << (8 * (i + 1));
        std::free(storage);
        return v;
    }
    bool intern = (length <= INTERN_MAX_LEN);
    if (intern) {
        StringRef key{ storage->data, storage->lenB };
        std::unordered_map<StringRef, int, StringRefHash>::iterator it = _internTable.find(key);
        if (it != _internTable.end()) {
            std::free(storage);
            return MakeGC(STRING_SET, it->second);
        }
    }
    int idx = Strings.New();
    GCString& s = Strings.Get(idx);
    s.Storage  = storage;
    s.Interned = intern;
    if (intern) {
        StringRef storedKey{ storage->data, storage->lenB };
        _internTable.insert(std::make_pair(storedKey, idx));
    }
    return MakeGC(STRING_SET, idx);
}

// ── Value factories ─────────────────────────────────────────────────────

Value GCManager::NewString(const char* data, int length) {
    if (length <= 5) {
        // Encode tiny string inline.
        Value v = TINY_STRING_TAG | uint64_t(uint8_t(length));
        for (int i = 0; i < length; i++)
            v |= uint64_t(uint8_t(data[i])) << (8 * (i + 1));
        return v;
    }
    return InternOrAllocateString(data, length);
}

Value GCManager::NewList(int capacity) {
    int idx = Lists.New();
    Lists.Get(idx).Init(capacity);
    return MakeGC(LIST_SET, idx);
}

Value GCManager::NewMap(int capacity) {
    int idx = Maps.New();
    Maps.Get(idx).Init(capacity);
    return MakeGC(MAP_SET, idx);
}

Value GCManager::NewError(Value message, Value inner, Value stack, Value isa) {
    int idx = Errors.New();
    GCError& e = Errors.Get(idx);
    e.Message = message;
    e.Inner   = inner;
    e.Stack   = stack;
    e.Isa     = isa;
    return MakeGC(ERROR_SET, idx);
}

Value GCManager::NewFuncRef(int funcIndex, Value outerVars) {
    int idx = FuncRefs.New();
    GCFuncRef& f = FuncRefs.Get(idx);
    f.FuncIndex = funcIndex;
    f.OuterVars = outerVars;
    return MakeGC(FUNCREF_SET, idx);
}

// ── Retain / Release ────────────────────────────────────────────────────

void GCManager::RetainValue(Value v) {
    if (!IsGCObject(v)) return;
    int set = GCSetIndex(v);
    int idx = ItemIndex(v);
    switch (set) {
        case STRING_SET:  Strings.Retain(idx);  break;
        case LIST_SET:    Lists.Retain(idx);    break;
        case MAP_SET:     Maps.Retain(idx);     break;
        case ERROR_SET:   Errors.Retain(idx);   break;
        case FUNCREF_SET: FuncRefs.Retain(idx); break;
        default: break;
    }
}

void GCManager::ReleaseValue(Value v) {
    if (!IsGCObject(v)) return;
    int set = GCSetIndex(v);
    int idx = ItemIndex(v);
    switch (set) {
        case STRING_SET:  Strings.Release(idx);  break;
        case LIST_SET:    Lists.Release(idx);    break;
        case MAP_SET:     Maps.Release(idx);     break;
        case ERROR_SET:   Errors.Release(idx);   break;
        case FUNCREF_SET: FuncRefs.Release(idx); break;
        default: break;
    }
}

// ── Roots & callbacks ───────────────────────────────────────────────────

void GCManager::AddRoot(Value v) { _roots.push_back(v); }

void GCManager::RemoveRoot(Value v) {
    auto it = std::find(_roots.begin(), _roots.end(), v);
    if (it != _roots.end()) _roots.erase(it);
}

void GCManager::RegisterMarkCallback(MarkCallback fn, void* userData) {
    Callback cb; cb.fn = fn; cb.userData = userData;
    _markCallbacks.push_back(cb);
}

void GCManager::UnregisterMarkCallback(MarkCallback fn, void* userData) {
    for (auto it = _markCallbacks.begin(); it != _markCallbacks.end(); ++it) {
        if (it->fn == fn && it->userData == userData) {
            _markCallbacks.erase(it);
            return;
        }
    }
}

// ── Collect ─────────────────────────────────────────────────────────────

void GCManager::CollectGarbage() {
    // 1. Clear mark bits.
    for (int i = 0; i < NUM_SETS; i++) _sets[i]->PrepareForGC();

    // 2. Mark from explicit roots.
    for (size_t i = 0; i < _roots.size(); i++) Mark(_roots[i]);

    // 3. Mark retained items.
    for (int i = 0; i < NUM_SETS; i++) _sets[i]->MarkRetained(*this);

    // 4. Mark via VM-supplied callbacks.
    for (size_t i = 0; i < _markCallbacks.size(); i++)
        _markCallbacks[i].fn(_markCallbacks[i].userData, *this);

    // 5. Sweep unreachable items. Strings sweep last so map/list children
    // (whose entries may be string keys) have already cleared their
    // marked-but-unreachable status. Order within sweep doesn't actually
    // affect correctness here — Sweep only frees items still unmarked
    // after step 4 — but doing strings last is a small hedge against
    // future ordering hazards if MarkChildren ever grows side effects.
    for (int i = 0; i < NUM_SETS; i++) {
        if (i == STRING_SET) continue;
        _sets[i]->Sweep(*this);
    }
    _sets[STRING_SET]->Sweep(*this);
}

// ── Diagnostics ─────────────────────────────────────────────────────────

void GCManager::PrintStats() const {
    std::printf("  Strings:  %d live\n", Strings.LiveCount());
    std::printf("  Lists:    %d live\n", Lists.LiveCount());
    std::printf("  Maps:     %d live\n", Maps.LiveCount());
    std::printf("  Errors:   %d live\n", Errors.LiveCount());
    std::printf("  FuncRefs: %d live\n", FuncRefs.LiveCount());
}

} // namespace MiniScript
