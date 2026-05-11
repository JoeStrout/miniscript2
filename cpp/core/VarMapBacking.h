// VarMapBacking.h — register-binding metadata attached to a GCMap.
//
// Direct C++ port of cs/VarMap.cs. Bridges register-based local variables
// (stored in the VM's stack/names arrays) and map semantics: a VarMap-backed
// GCMap routes string-keyed Get/Set/Remove through the register window,
// falling back to the regular hash table for other keys.
//
// The C# version stores List<Value> references; the C++ side uses raw
// Value* pointers because that's what the generated VM emits. The bridge
// function varmap_rebind() rebinds these pointers when the VM's stack is
// reallocated.

#ifndef VARMAP_BACKING_H
#define VARMAP_BACKING_H

#ifdef __cplusplus

#include "value.h"
#include "value_string.h"
#include <unordered_map>
#include <vector>
#include <cstddef>

namespace MiniScript {

class GCManager;
struct GCMap;

// Content-based Value hashing (strings hash by content, everything else
// by bits). Used by VarMapBacking's _regMap so tiny strings and interned
// heap strings with equal content collide.
struct ValueContentHash {
    std::size_t operator()(Value v) const {
        if (is_string(v)) return std::size_t(get_string_hash(v));
        return std::size_t(v ^ (v >> 32));
    }
};

struct ValueContentEqual {
    bool operator()(Value a, Value b) const {
        if (a == b) return true;
        if (is_string(a) && is_string(b)) return string_equals(a, b);
        return false;
    }
};

class VarMapBacking {
public:
    VarMapBacking(Value* registers, Value* names, int firstIdx, int lastIdx);

    // ── Register-binding checks (used by GCMap.TryGet/Set/Remove) ───────
    bool TryGet(Value key, Value& outValue) const;
    bool TrySet(Value key, Value value);
    bool TryRemove(Value key);
    bool HasKey(Value key) const;

    // ── Iteration support ───────────────────────────────────────────────
    int   RegEntryCount() const;
    int   NextAssignedRegEntry(int startIdx) const;
    Value GetRegEntryKey(int i) const;
    Value GetRegEntryValue(int i) const;

    // ── GC ──────────────────────────────────────────────────────────────
    void MarkChildren(GCManager& gc);

    // ── VarMap operations ───────────────────────────────────────────────
    void Gather(GCMap& map);
    void Rebind(GCMap& map, Value* registers, Value* names);
    void MapToRegister(GCMap& map, Value varName, Value* registers, int regIndex);
    void Clear();

private:
    std::unordered_map<Value, int, ValueContentHash, ValueContentEqual> _regMap;
    Value*             _registers;
    Value*             _names;
    std::vector<Value> _regOrder;  // insertion-order list of names

    bool nameLive(int regIdx) const { return !is_null(_names[regIdx]); }
};

} // namespace MiniScript

#endif // __cplusplus
#endif // VARMAP_BACKING_H
