// GCItems.h — five GC-managed item types backing MiniScript Values.
//
//   GCString  — heap-allocated string (raw char* + length + cached hash).
//   GCList    — vector of Values.
//   GCMap     — open-addressing hash map of Value→Value.
//   GCError   — message/inner/stack/isa quartet.
//   GCFuncRef — funcIndex + outerVars (closure capture).
//
// Each type provides MarkChildren(GCManager&) and OnSweep() to satisfy
// GCSet<T>'s duck-typed contract. Method bodies that reach into GCManager
// are defined out-of-line in GCItems.cpp.

#ifndef GCITEMS_H
#define GCITEMS_H

#ifdef __cplusplus

#include "value.h"
#include "StringStorage.h"
#include <vector>
#include <cstdint>
#include <cstddef>

namespace MiniScript {

using Value = ::Value;

// C++-friendly inline helpers wrapping the C-macro / static-inline API
// in value.h, so the new-GC code can write idiomatic C++ without
// macro-name pollution leaking out of value.h.
inline Value MakeNull()                        { return val_null; }
inline Value MakeGC(int gcSet, int idx)        {
    return GC_TAG | (uint64_t(gcSet) << 32) | uint32_t(idx);
}
inline bool  IsNull(Value v)     { return is_null(v); }
inline bool  IsGCObject(Value v) { return is_gc_object(v); }
inline bool  IsTinyStr(Value v)  { return is_tiny_string(v); }
inline bool  IsString(Value v)   { return is_string(v); }
inline bool  IsList(Value v)     { return is_list(v); }
inline bool  IsMap(Value v)      { return is_map(v); }
inline bool  IsError(Value v)    { return is_error(v); }
inline bool  IsFuncRef(Value v)  { return is_funcref(v); }
inline int   GCSetIndex(Value v) { return value_gc_set_index(v); }
inline int   ItemIndex(Value v)  { return value_item_index(v); }
inline int   TinyLen(Value v)    { return int(v & 0xFF); }

class GCManager;       // forward
class VarMapBacking;   // forward

// ── GCString ────────────────────────────────────────────────────────────
//
// Owns a heap-allocated StringStorage (the same struct used by CS_String,
// the C# host-string compatibility layer). Sharing the storage struct
// guarantees identical behaviour across both string layers — substring,
// case-conversion, hashing, comparison, etc. all dispatch through the
// same ss_* API.
//
// Storage may be NULL (the StringStorage convention for empty strings),
// though Value strings normally don't carry empty heap strings — those
// are encoded as tiny strings.
//
// Interned tells GCManager whether to remove this slot from the intern
// side-table on sweep.

struct GCString {
    StringStorage* Storage  = nullptr;  // malloc'd; freed in OnSweep
    bool           Interned = false;

    void MarkChildren(GCManager&) {}    // strings hold no child Values
    void OnSweep();                      // frees Storage; defined in GCItems.cpp
};

// ── GCList ──────────────────────────────────────────────────────────────

struct GCList {
    std::vector<Value> Items;
    bool Frozen = false;

    int  Count() const { return int(Items.size()); }
    void Init(int capacity = 8) {
        Items.clear();
        Items.reserve(capacity > 4 ? capacity : 4);
        Frozen = false;
    }

    void  Push(Value v) { Items.push_back(v); }
    Value Get(int i) const {
        if (i < 0) i += int(Items.size());
        return (i >= 0 && i < int(Items.size())) ? Items[size_t(i)] : MakeNull();
    }
    void Set(int i, Value v) {
        if (i < 0) i += int(Items.size());
        if (i >= 0 && i < int(Items.size())) Items[size_t(i)] = v;
    }

    void MarkChildren(GCManager& gc);
    void OnSweep() { Items.clear(); Items.shrink_to_fit(); Frozen = false; }
};

// ── GCMap ───────────────────────────────────────────────────────────────
// "Compact dict" layout: a dense entries array in insertion order, plus
// a sparse indices table that maps hash slot → entry index. Iteration walks
// the dense entries (skipping tombstones), so insertion order is preserved
// naturally. Mirrors the C# GCMap layout.

struct GCMap {
    bool Frozen = false;

    // Optional register-binding overlay; null for plain maps. When non-null,
    // string-keyed Get/Set/Remove route through registers first, falling
    // back to the hash table for misses. Iteration walks register entries
    // (encoded as iter values <= -2) before dense entries (iter >= 0).
    VarMapBacking* _vmb = nullptr;

    int Count() const;   // live entries (dense + register-bound)

    void  Init(int capacity = 8);
    bool  TryGet(Value key, Value& outVal) const;
    void  Set(Value key, Value value);
    bool  Remove(Value key);
    bool  HasKey(Value key) const;

    // Iteration: for (int i = NextEntry(-1); i != -1; i = NextEntry(i))
    int   NextEntry(int after) const;
    Value KeyAt(int i)   const;
    Value ValueAt(int i) const;

    void MarkChildren(GCManager& gc);
    void OnSweep();

private:
    // Dense entries, in insertion order. _entryHashes[i] == DELETED → tombstone.
    std::vector<Value> _entryKeys;
    std::vector<Value> _entryVals;
    std::vector<int>   _entryHashes;
    int _entryCount = 0;   // total appended entries (incl. tombstones)
    int _liveCount  = 0;   // entries minus tombstones

    // Sparse indices table: slot → entry index, or EMPTY_SLOT / TOMBSTONE_SLOT.
    std::vector<int> _indices;
    int _cap = 0;          // power-of-2 size of _indices

    enum : int { EMPTY_SLOT = -1, TOMBSTONE_SLOT = -2 };
    static const int DELETED = INT32_MIN;  // tombstone marker in _entryHashes

    void GrowEntries();
    void Resize(int newCap);
    static int  Hash(Value v);
    static bool KeyEquals(Value a, Value b);
    static int  NextPow2(int n) { int p = 1; while (p < n) p <<= 1; return p; }
};

// ── GCError ─────────────────────────────────────────────────────────────

struct GCError {
    Value Message = MakeNull();
    Value Inner   = MakeNull();
    Value Stack   = MakeNull();
    Value Isa     = MakeNull();

    void MarkChildren(GCManager& gc);
    void OnSweep() {
        Message = MakeNull(); Inner = MakeNull();
        Stack   = MakeNull(); Isa   = MakeNull();
    }
};

// ── GCFuncRef ───────────────────────────────────────────────────────────

struct GCFuncRef {
    int   FuncIndex = -1;
    Value OuterVars = MakeNull();

    void MarkChildren(GCManager& gc);
    void OnSweep() { FuncIndex = -1; OuterVars = MakeNull(); }
};

} // namespace MiniScript

#endif // __cplusplus
#endif // GCITEMS_H
