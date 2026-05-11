// GCManager.h — central GC coordinator for the new index-based GC system.
//
// Owns the five typed GCSets (Strings, Lists, Maps, Errors, FuncRefs),
// an explicit root list, a string intern side-table, and a list of
// VM-supplied mark callbacks. CollectGarbage() runs the standard
// mark/sweep cycle.
//
// Mirrors the C# GCManager in cs/GCManager.cs. Slot indices 0-4 here
// match the C# constants exactly, so the on-disk Value bit layout is
// portable between the two runtimes.

#ifndef GCMANAGER_H
#define GCMANAGER_H

#ifdef __cplusplus

#include "value.h"
#include "IGCSet.h"
#include "GCSet.h"
#include "GCItems.h"

#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>

namespace MiniScript {

// GCSet count: 5 in use (Strings/Lists/Maps/Errors/FuncRefs); slot 5
// reserved for future GCHandle.
static const int NUM_SETS = 5;

// View into a fixed-length char buffer, used as the key type for the
// intern table. The view points into a GCString's StringStorage->data
// buffer (heap-allocated by us via malloc, so its address is stable for
// the life of the slot — std::vector reallocations move only the
// GCString struct, not the StringStorage it owns).
struct StringRef {
    const char* data;
    int         length;

    bool operator==(const StringRef& o) const {
        if (length != o.length) return false;
        for (int i = 0; i < length; i++)
            if (data[i] != o.data[i]) return false;
        return true;
    }
};

struct StringRefHash {
    std::size_t operator()(const StringRef& s) const {
        // FNV-1a 32-bit, returned as size_t.
        uint32_t h = 0x811C9DC5u;
        for (int i = 0; i < s.length; i++) {
            h ^= uint8_t(s.data[i]);
            h *= 0x01000193u;
        }
        return std::size_t(h);
    }
};

// Strings shorter than this are interned; longer ones bypass the table.
// Tiny strings (≤5 chars) never reach this code — they're encoded inline
// in the Value bits.
constexpr int INTERN_MAX_LEN = 127;

// Mark callback: VM (or any other root provider) registers one of these
// in its constructor. Called once per CollectGarbage cycle. The callback
// must invoke gc.Mark(v) on every root Value it owns.
typedef void (*MarkCallback)(void* userData, GCManager& gc);

// Specialised GCSet<GCString> that removes interned slots from the
// intern table just before they're swept.
class StringsGCSet : public GCSet<GCString> {
public:
    void BeforeSlotFreed(int idx, GCManager& gc) override;
};

class GCManager {
public:
    StringsGCSet      Strings;
    GCSet<GCList>     Lists;
    GCSet<GCMap>      Maps;
    GCSet<GCError>    Errors;
    GCSet<GCFuncRef>  FuncRefs;

    GCManager();
    ~GCManager() = default;

    // Non-copyable/movable: _sets points into our own members.
    GCManager(const GCManager&) = delete;
    GCManager& operator=(const GCManager&) = delete;

    // ── Singleton accessor ──────────────────────────────────────────────
    // Most code paths use a single process-wide manager; expose it for
    // C-side shims and OnSweep callbacks that need to reach the intern
    // table without an explicit GCManager& parameter.
    static GCManager& Instance();

    // ── Value factories ─────────────────────────────────────────────────
    // make_string equivalent: chooses tiny / interned / fresh based on length.
    Value NewString(const char* data, int length);

    // Adopt a malloc-allocated StringStorage (e.g. the result of an ss_*
    // operation). Length is read from storage->lenB. If interning would
    // collapse to an existing slot, the passed-in storage is freed and the
    // existing Value is returned. Pass NULL for the empty-string case;
    // it returns val_empty_string.
    Value AdoptString(StringStorage* storage);

    Value NewList(int capacity = 8);
    Value NewMap(int capacity = 8);
    Value NewError(Value message, Value inner, Value stack, Value isa);
    Value NewFuncRef(int funcIndex, Value outerVars);

    // ── Retain / Release ────────────────────────────────────────────────
    void RetainValue(Value v);
    void ReleaseValue(Value v);

    // ── Root set ────────────────────────────────────────────────────────
    void AddRoot(Value v);
    void RemoveRoot(Value v);

    // ── Mark callbacks ──────────────────────────────────────────────────
    void RegisterMarkCallback(MarkCallback fn, void* userData);
    void UnregisterMarkCallback(MarkCallback fn, void* userData);

    // ── GC cycle ────────────────────────────────────────────────────────
    void Mark(Value v) {
        if (!IsGCObject(v)) return;
        _sets[GCSetIndex(v)]->Mark(ItemIndex(v), *this);
    }
    void CollectGarbage();

    // ── Intern table access (used by StringsGCSet::BeforeSlotFreed) ─────
    void UninternString(const GCString& s);

    // ── Diagnostics ─────────────────────────────────────────────────────
    void PrintStats() const;

    // ── Convenience ref-accessors ───────────────────────────────────────
    GCString&  GetString(Value v)  { return Strings.Get(ItemIndex(v)); }
    GCList&    GetList(Value v)    { return Lists.Get(ItemIndex(v)); }
    GCMap&     GetMap(Value v)     { return Maps.Get(ItemIndex(v)); }
    GCError&   GetError(Value v)   { return Errors.Get(ItemIndex(v)); }
    GCFuncRef& GetFuncRef(Value v) { return FuncRefs.Get(ItemIndex(v)); }

private:
    IGCSet*            _sets[NUM_SETS];
    std::vector<Value> _roots;

    // Intern table: content view → string-set index. Views point into the
    // matching GCString's Data buffer (stable across vector resizes).
    std::unordered_map<StringRef, int, StringRefHash> _internTable;

    struct Callback { MarkCallback fn; void* userData; };
    std::vector<Callback> _markCallbacks;

    Value InternOrAllocateString(const char* data, int length);
};

} // namespace MiniScript

#endif // __cplusplus
#endif // GCMANAGER_H
