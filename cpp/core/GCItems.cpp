// GCItems.cpp — out-of-line method bodies that depend on GCManager.

#include "GCManager.h"
#include "VarMapBacking.h"
#include <cstdlib>
#include <cstring>

namespace MiniScript {

// ── GCString ────────────────────────────────────────────────────────────

void GCString::OnSweep() {
    if (Storage) { std::free(Storage); Storage = nullptr; }
    Interned = false;
}

// ── GCList ──────────────────────────────────────────────────────────────

void GCList::MarkChildren(GCManager& gc) {
    for (Value v : Items) gc.Mark(v);
}

// ── GCMap ───────────────────────────────────────────────────────────────

const int GCMap::DELETED;

int GCMap::Count() const {
    int n = _liveCount;
    if (_vmb) n += _vmb->RegEntryCount();
    return n;
}

void GCMap::Init(int capacity) {
    _cap = NextPow2(capacity > 8 ? capacity : 8);
    _entryKeys.assign(size_t(_cap), MakeNull());
    _entryVals.assign(size_t(_cap), MakeNull());
    _entryHashes.assign(size_t(_cap), 0);
    _indices.assign(size_t(_cap), int(EMPTY_SLOT));
    _entryCount = 0;
    _liveCount  = 0;
    Frozen = false;
    _vmb   = nullptr;
}

bool GCMap::TryGet(Value key, Value& outVal) const {
    if (_vmb && _vmb->TryGet(key, outVal)) return true;

    if (_indices.empty()) { outVal = MakeNull(); return false; }
    int h    = Hash(key);
    int slot = h & (_cap - 1);
    for (int probe = 0; probe < _cap; probe++) {
        int idx = _indices[size_t(slot)];
        if (idx == EMPTY_SLOT) { outVal = MakeNull(); return false; }
        if (idx != TOMBSTONE_SLOT
            && _entryHashes[size_t(idx)] == h
            && KeyEquals(_entryKeys[size_t(idx)], key)) {
            outVal = _entryVals[size_t(idx)];
            return true;
        }
        slot = (slot + 1) & (_cap - 1);
    }
    outVal = MakeNull();
    return false;
}

void GCMap::Set(Value key, Value value) {
    if (_vmb && _vmb->TrySet(key, value)) return;

    if (_indices.empty()) Init();
    if ((_liveCount + 1) * 2 >= _cap) Resize(_cap * 2);
    int h         = Hash(key);
    int slot      = h & (_cap - 1);
    int firstTomb = -1;
    for (int probe = 0; probe < _cap; probe++) {
        int idx = _indices[size_t(slot)];
        if (idx == EMPTY_SLOT) {
            int insertSlot = firstTomb >= 0 ? firstTomb : slot;
            if (_entryCount == int(_entryKeys.size())) GrowEntries();
            _entryKeys[size_t(_entryCount)]   = key;
            _entryVals[size_t(_entryCount)]   = value;
            _entryHashes[size_t(_entryCount)] = h;
            _indices[size_t(insertSlot)]      = _entryCount;
            _entryCount++;
            _liveCount++;
            return;
        }
        if (idx == TOMBSTONE_SLOT) {
            if (firstTomb < 0) firstTomb = slot;
        } else if (_entryHashes[size_t(idx)] == h
                   && KeyEquals(_entryKeys[size_t(idx)], key)) {
            _entryVals[size_t(idx)] = value;
            return;
        }
        slot = (slot + 1) & (_cap - 1);
    }
    // Should be unreachable: the resize-at-50%-load policy guarantees a slot.
}

bool GCMap::Remove(Value key) {
    if (_vmb && _vmb->TryRemove(key)) return true;

    if (_indices.empty()) return false;
    int h    = Hash(key);
    int slot = h & (_cap - 1);
    for (int probe = 0; probe < _cap; probe++) {
        int idx = _indices[size_t(slot)];
        if (idx == EMPTY_SLOT) return false;
        if (idx != TOMBSTONE_SLOT
            && _entryHashes[size_t(idx)] == h
            && KeyEquals(_entryKeys[size_t(idx)], key)) {
            _entryHashes[size_t(idx)] = DELETED;
            _entryKeys[size_t(idx)]   = MakeNull();
            _entryVals[size_t(idx)]   = MakeNull();
            _indices[size_t(slot)]    = int(TOMBSTONE_SLOT);
            _liveCount--;
            return true;
        }
        slot = (slot + 1) & (_cap - 1);
    }
    return false;
}

bool GCMap::HasKey(Value key) const {
    Value v;
    return TryGet(key, v);
}

int GCMap::NextEntry(int after) const {
    // Phase 1: VarMap register entries (iter values <= -1 from start; <= -2 ongoing).
    if (_vmb && after <= -1) {
        int startRegIdx = (after == -1) ? 0 : -after - 2 + 1;
        int found = _vmb->NextAssignedRegEntry(startRegIdx);
        if (found >= 0) return -(found + 2);
        after = -1;  // fall through to phase 2 from the start
    }
    // Phase 2: dense entries in insertion order (skip tombstones).
    int i = (after < 0) ? 0 : after + 1;
    while (i < _entryCount) {
        if (_entryHashes[size_t(i)] != DELETED) return i;
        i++;
    }
    return -1;
}

Value GCMap::KeyAt(int i) const {
    if (i < -1 && _vmb) return _vmb->GetRegEntryKey(-i - 2);
    return _entryKeys[size_t(i)];
}

Value GCMap::ValueAt(int i) const {
    if (i < -1 && _vmb) return _vmb->GetRegEntryValue(-i - 2);
    return _entryVals[size_t(i)];
}

void GCMap::OnSweep() {
    if (_vmb) { delete _vmb; _vmb = nullptr; }
    _entryKeys.clear();   _entryKeys.shrink_to_fit();
    _entryVals.clear();   _entryVals.shrink_to_fit();
    _entryHashes.clear(); _entryHashes.shrink_to_fit();
    _indices.clear();     _indices.shrink_to_fit();
    _entryCount = 0; _liveCount = 0; _cap = 0; Frozen = false;
}

void GCMap::GrowEntries() {
    int newLen = int(_entryKeys.size()) * 2;
    _entryKeys.resize(size_t(newLen), MakeNull());
    _entryVals.resize(size_t(newLen), MakeNull());
    _entryHashes.resize(size_t(newLen), 0);
}

// Compact entries (drop tombstones), then rebuild indices table at newCap.
void GCMap::Resize(int newCap) {
    // Compact in place, preserving insertion order.
    int writeIdx = 0;
    for (int i = 0; i < _entryCount; i++) {
        if (_entryHashes[size_t(i)] != DELETED) {
            if (writeIdx != i) {
                _entryKeys[size_t(writeIdx)]   = _entryKeys[size_t(i)];
                _entryVals[size_t(writeIdx)]   = _entryVals[size_t(i)];
                _entryHashes[size_t(writeIdx)] = _entryHashes[size_t(i)];
            }
            writeIdx++;
        }
    }
    for (int i = writeIdx; i < _entryCount; i++) {
        _entryKeys[size_t(i)]   = MakeNull();
        _entryVals[size_t(i)]   = MakeNull();
        _entryHashes[size_t(i)] = 0;
    }
    _entryCount = writeIdx;
    _liveCount  = writeIdx;

    _cap = newCap;
    _indices.assign(size_t(_cap), int(EMPTY_SLOT));
    if (int(_entryKeys.size()) < _cap) {
        _entryKeys.resize(size_t(_cap), MakeNull());
        _entryVals.resize(size_t(_cap), MakeNull());
        _entryHashes.resize(size_t(_cap), 0);
    }
    for (int i = 0; i < _entryCount; i++) {
        int h    = _entryHashes[size_t(i)];
        int slot = h & (_cap - 1);
        while (_indices[size_t(slot)] != EMPTY_SLOT)
            slot = (slot + 1) & (_cap - 1);
        _indices[size_t(slot)] = i;
    }
}

void GCMap::MarkChildren(GCManager& gc) {
    for (int i = 0; i < _entryCount; i++) {
        if (_entryHashes[size_t(i)] != DELETED) {
            gc.Mark(_entryKeys[size_t(i)]);
            gc.Mark(_entryVals[size_t(i)]);
        }
    }
    if (_vmb) _vmb->MarkChildren(gc);
}

// Hash by content for strings (so tiny strings and interned heap strings
// with equal content collide); by bits for everything else.
int GCMap::Hash(Value v) {
    int h;
    if (IsString(v)) {
        if (IsTinyStr(v)) {
            // Tiny chars live in bits 8..47 of the Value.
            uint32_t hh = 0x811C9DC5u;
            int n = TinyLen(v);
            for (int i = 0; i < n; i++) {
                uint8_t b = uint8_t((v >> (8 * (i + 1))) & 0xFF);
                hh ^= b;
                hh *= 0x01000193u;
            }
            h = int(hh);
        } else {
            // Use StringStorage's cached hash (computed by ss_hash on first call).
            GCString& s = GCManager::Instance().Strings.Get(ItemIndex(v));
            h = int(ss_hash(s.Storage));
        }
    } else {
        uint64_t bits = uint64_t(v);
        h = int(bits ^ (bits >> 32));
    }
    // Avoid the lone sentinel value used in _entryHashes for tombstones.
    if (h == DELETED) h = 0;
    return h;
}

bool GCMap::KeyEquals(Value a, Value b) {
    if (a == b) return true;
    if (IsString(a) && IsString(b)) {
        // Pull length+data from each side (tiny or heap).
        char tinyBufA[8]; char tinyBufB[8];
        const char* da; int la;
        const char* db; int lb;
        if (IsTinyStr(a)) {
            la = TinyLen(a);
            for (int i = 0; i < la; i++)
                tinyBufA[i] = char((a >> (8 * (i + 1))) & 0xFF);
            da = tinyBufA;
        } else {
            const GCString& s = GCManager::Instance().Strings.Get(ItemIndex(a));
            da = s.Storage ? s.Storage->data : "";
            la = s.Storage ? s.Storage->lenB : 0;
        }
        if (IsTinyStr(b)) {
            lb = TinyLen(b);
            for (int i = 0; i < lb; i++)
                tinyBufB[i] = char((b >> (8 * (i + 1))) & 0xFF);
            db = tinyBufB;
        } else {
            const GCString& s = GCManager::Instance().Strings.Get(ItemIndex(b));
            db = s.Storage ? s.Storage->data : "";
            lb = s.Storage ? s.Storage->lenB : 0;
        }
        if (la != lb) return false;
        for (int i = 0; i < la; i++) if (da[i] != db[i]) return false;
        return true;
    }
    return false;
}

// ── GCError / GCFuncRef ─────────────────────────────────────────────────

void GCError::MarkChildren(GCManager& gc) {
    gc.Mark(Message);
    gc.Mark(Inner);
    gc.Mark(Stack);
    gc.Mark(Isa);
}

void GCFuncRef::MarkChildren(GCManager& gc) {
    gc.Mark(OuterVars);
}

} // namespace MiniScript
