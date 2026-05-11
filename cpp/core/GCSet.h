// GCSet.h — typed pool of GC-managed objects.
//
// Struct-of-arrays layout: items live in their own vector; per-slot GC
// metadata (in-use, marked, retain count) lives in tight parallel arrays.
// Slots are recycled via a free-list stack; high-water mark grows monotonically.
//
// T must satisfy the duck-typed contract:
//   void MarkChildren(GCManager&)
//   void OnSweep()
//
// A virtual hook BeforeSlotFreed(idx, gc) lets specialised subclasses
// (e.g. StringsGCSet) clean up side state — like the intern table — just
// before a slot's OnSweep runs.

#ifndef GCSET_H
#define GCSET_H

#ifdef __cplusplus

#include "IGCSet.h"
#include <vector>
#include <cstdint>

namespace MiniScript {

template<typename T>
class GCSet : public IGCSet {
protected:
    std::vector<T>       _items;
    std::vector<uint8_t> _inUse;
    std::vector<uint8_t> _marked;
    std::vector<int>     _retainCounts;
    std::vector<int>     _free;     // stack of recycled indices
    int                  _hwm = 0;  // high-water mark

public:
    explicit GCSet(int initialCapacity = 64) {
        _items.resize(initialCapacity);
        _inUse.resize(initialCapacity, 0);
        _marked.resize(initialCapacity, 0);
        _retainCounts.resize(initialCapacity, 0);
        _free.reserve(initialCapacity);
    }

    // ── Allocation ──────────────────────────────────────────────────────
    int New() {
        int idx;
        if (!_free.empty()) {
            idx = _free.back();
            _free.pop_back();
        } else {
            idx = _hwm++;
            if (idx >= int(_items.size())) Grow();
        }
        _items[idx]        = T{};
        _inUse[idx]        = 1;
        _marked[idx]       = 0;
        _retainCounts[idx] = 0;
        return idx;
    }

    T&       Get(int idx)       { return _items[idx]; }
    const T& Get(int idx) const { return _items[idx]; }

    // ── Retain / Release ────────────────────────────────────────────────
    void Retain(int idx)  { _retainCounts[idx]++; }
    void Release(int idx) { _retainCounts[idx]--; }

    // ── IGCSet implementation ───────────────────────────────────────────
    void PrepareForGC() override {
        for (int i = 0; i < _hwm; i++) _marked[i] = 0;
    }

    void Mark(int idx, GCManager& gc) override {
        if (_marked[idx]) return;
        _marked[idx] = 1;
        _items[idx].MarkChildren(gc);
    }

    void MarkRetained(GCManager& gc) override {
        for (int i = 0; i < _hwm; i++)
            if (_inUse[i] && _retainCounts[i] > 0) Mark(i, gc);
    }

    void Sweep(GCManager& gc) override {
        for (int i = 0; i < _hwm; i++) {
            if (_inUse[i] && !_marked[i] && _retainCounts[i] <= 0) {
                BeforeSlotFreed(i, gc);
                _items[i].OnSweep();
                _items[i]        = T{};
                _inUse[i]        = 0;
                _retainCounts[i] = 0;
                _free.push_back(i);
            }
        }
    }

    int LiveCount() const override {
        int n = 0;
        for (int i = 0; i < _hwm; i++) if (_inUse[i]) n++;
        return n;
    }

    // Subclasses can override to clean up side state (e.g. intern tables).
    // Default: no-op.
    virtual void BeforeSlotFreed(int /*idx*/, GCManager& /*gc*/) {}

private:
    void Grow() {
        int newLen = int(_items.size()) * 2;
        _items.resize(newLen);
        _inUse.resize(newLen, 0);
        _marked.resize(newLen, 0);
        _retainCounts.resize(newLen, 0);
    }
};

} // namespace MiniScript

#endif // __cplusplus
#endif // GCSET_H
