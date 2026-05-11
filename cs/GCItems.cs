//*** BEGIN CS_ONLY ***
using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;

namespace MiniScript {

// ── GCString ─────────────────────────────────────────────────────────────────

public struct GCString : IGCItem {
	public String Data;

	public void MarkChildren(GCManager gc) { }  // strings have no child Values

	public void OnSweep() { Data = null; }
}

// ── GCList ───────────────────────────────────────────────────────────────────

public struct GCList : IGCItem {
	public List<Value> Items;
	public Boolean Frozen;

	[MethodImpl(AggressiveInlining)]
	public void Init(Int32 capacity = 8) {
		Items  = new List<Value>(Math.Max(capacity, 4));
		Frozen = false;
	}

	[MethodImpl(AggressiveInlining)]
	public void Push(Value v) {
		if (Items == null) Init();
		Items.Add(v);
	}

	[MethodImpl(AggressiveInlining)]
	public Value Get(Int32 i) {
		if (i < 0) i += Items.Count;
		return (UInt32)i < (UInt32)Items.Count ? Items[i] : Value.Null();
	}

	[MethodImpl(AggressiveInlining)]
	public void Set(Int32 i, Value v) {
		if (i < 0) i += Items.Count;
		if ((UInt32)i < (UInt32)Items.Count) Items[i] = v;
	}

	[MethodImpl(AggressiveInlining)]
	public Boolean Remove(Int32 index) {
		if (Items == null) Init();
		if (index < 0) index += Items.Count;
		if (index < 0 || index >= Items.Count) return false;
		Items.RemoveAt(index);
		return true;
	}

	public void Insert(Int32 index, Value v) {
		if (Items == null) Init();
		if (index < 0) index += Items.Count + 1;
		if (index < 0) index = 0;  // ToDo: this should raise a runtime error
		if (index > Items.Count) index = Items.Count;
		Items.Insert(index, v);
	}

	public Value Pop() {
		if (Items == null || Items.Count == 0) return Value.Null(); // ToDo: error
		Value result = Items[Items.Count - 1];
		Items.RemoveAt(Items.Count - 1);
		return result;
	}

	public Value Pull() {
		if (Items == null || Items.Count == 0) return Value.Null(); // ToDo: error
		Value result = Items[0];
		Items.RemoveAt(0);
		return result;
	}

	public Int32 IndexOf(Value item, Int32 afterIdx) {
		if (Items == null) return -1;
		for (Int32 i = afterIdx + 1; i < Items.Count; i++) {
			if (Value.Equal(Items[i], item)) return i;
		}
		return -1;
	}

	[MethodImpl(AggressiveInlining)]
	public void MarkChildren(GCManager gc) {
		if (Items == null) return;
		for (Int32 i = 0; i < Items.Count; i++) gc.Mark(Items[i]);
	}

	[MethodImpl(AggressiveInlining)]
	public void OnSweep() {
		Items = null;
		Frozen = false;
	}
}

// ── GCMap ─────────────────────────────────────────────────────────────────────
// "Compact dict" layout: a dense entries array in insertion order, plus a sparse
// indices table that maps hash slot → entry index. Iteration walks the dense
// entries (skipping tombstones), so insertion order is preserved naturally.
//
// Hash and equality are content-based for strings so that tiny strings and
// GCStrings with equal content behave as equal keys.
//
// An optional _vmb field provides VarMap register-binding behaviour.
//
// (Why not just use a standard System.Collections.Dictionary?  Mostly because
// the VM needs to be able to iterate over key/value pairs by an index, via
// NextEntry.  Dicitonary doesn't provide any way to do that.)

public struct GCMap : IGCItem {
	// Dense entries, in insertion order. _entryHashes[i] == DELETED marks a tombstone.
	private Value[]  _entryKeys;
	private Value[]  _entryVals;
	private Int32[]  _entryHashes;
	private Int32    _entryCount;   // total entries appended (incl. tombstones)
	private Int32    _liveCount;    // entries minus tombstones

	// Sparse indices table: slot → entry index, or EMPTY_SLOT / TOMBSTONE_SLOT.
	private Int32[]  _indices;
	private Int32    _cap;          // power-of-2 size of _indices

	public  Boolean  Frozen;

	// Non-null for VarMap-backed maps (closures / REPL globals).
	internal VarMapBacking _vmb;

	private const Int32 EMPTY_SLOT     = -1;
	private const Int32 TOMBSTONE_SLOT = -2;
	private const Int32 DELETED        = Int32.MinValue;  // tombstone marker in _entryHashes

	public Int32 Count() {
		Int32 n = _liveCount;
		if (_vmb != null) n += _vmb.RegEntryCount;
		return n;
	}

	public void Init(Int32 capacity = 8) {
		_cap         = NextPow2(Math.Max(capacity, 8));
		_entryKeys   = new Value[_cap];
		_entryVals   = new Value[_cap];
		_entryHashes = new Int32[_cap];
		_indices     = new Int32[_cap];
		Array.Fill(_indices, EMPTY_SLOT);
		_entryCount  = 0;
		_liveCount   = 0;
		Frozen       = false;
		_vmb         = null;
	}

	public Boolean TryGet(Value key, out Value value) {
		// Check VarMap register bindings first.
		if (_vmb != null && _vmb.TryGet(key, out value)) return true;

		if (_indices == null) { value = Value.Null(); return false; }
		Int32 h = Hash(key);
		Int32 slot = h & (_cap - 1);
		for (Int32 probe = 0; probe < _cap; probe++) {
			Int32 idx = _indices[slot];
			if (idx == EMPTY_SLOT) { value = Value.Null(); return false; }
			if (idx != TOMBSTONE_SLOT && _entryHashes[idx] == h && KeyEquals(_entryKeys[idx], key)) {
				value = _entryVals[idx];
				return true;
			}
			slot = (slot + 1) & (_cap - 1);
		}
		value = Value.Null();
		return false;
	}

	public void Set(Value key, Value value) {
		// Store in register if VarMap-backed and key is register-mapped.
		if (_vmb != null && _vmb.TrySet(key, value)) return;

		if (_indices == null) Init();
		if ((_liveCount + 1) * 2 >= _cap) Resize(_cap * 2);
		Int32 h = Hash(key);
		Int32 slot = h & (_cap - 1);
		Int32 firstTomb = -1;
		for (Int32 probe = 0; probe < _cap; probe++) {
			Int32 idx = _indices[slot];
			if (idx == EMPTY_SLOT) {
				// Append new entry, then point the (preferred) indices slot at it.
				Int32 insertSlot = firstTomb >= 0 ? firstTomb : slot;
				if (_entryCount == _entryKeys.Length) GrowEntries();
				_entryKeys[_entryCount]   = key;
				_entryVals[_entryCount]   = value;
				_entryHashes[_entryCount] = h;
				_indices[insertSlot]      = _entryCount;
				_entryCount++;
				_liveCount++;
				return;
			}
			if (idx == TOMBSTONE_SLOT) {
				if (firstTomb < 0) firstTomb = slot;
			} else if (_entryHashes[idx] == h && KeyEquals(_entryKeys[idx], key)) {
				_entryVals[idx] = value;
				return;
			}
			slot = (slot + 1) & (_cap - 1);
		}
		throw new InvalidOperationException("GCMap: table full");
	}

	public Boolean Remove(Value key) {
		if (_vmb != null && _vmb.TryRemove(key)) return true;

		if (_indices == null) return false;
		Int32 h = Hash(key);
		Int32 slot = h & (_cap - 1);
		for (Int32 probe = 0; probe < _cap; probe++) {
			Int32 idx = _indices[slot];
			if (idx == EMPTY_SLOT) return false;
			if (idx != TOMBSTONE_SLOT && _entryHashes[idx] == h && KeyEquals(_entryKeys[idx], key)) {
				_entryHashes[idx] = DELETED;
				_entryKeys[idx]   = Value.Null();
				_entryVals[idx]   = Value.Null();
				_indices[slot]    = TOMBSTONE_SLOT;
				_liveCount--;
				return true;
			}
			slot = (slot + 1) & (_cap - 1);
		}
		return false;
	}

	[MethodImpl(AggressiveInlining)]
	public Boolean HasKey(Value key) {
		Value ignored;
		return TryGet(key, out ignored);
	}

	public void Clear() {
		if (_indices != null) {
			Array.Fill(_indices, EMPTY_SLOT);
			Array.Clear(_entryKeys,   0, _entryCount);
			Array.Clear(_entryVals,   0, _entryCount);
			Array.Clear(_entryHashes, 0, _entryCount);
		}
		_entryCount = 0;
		_liveCount  = 0;
		_vmb?.Clear();
	}

	// ── Iteration ─────────────────────────────────────────────────────────────
	// iter = -1: start
	// iter < -1: VarMap register entry -(i+2) where i is the reg-entry index
	// iter >= 0: index into _entries (insertion order)

	public Int32 NextEntry(Int32 after) {
		// Phase 1: VarMap register entries (negative iter)
		if (_vmb != null && after <= -1) {
			Int32 startRegIdx = (after == -1) ? 0 : -(after) - 2 + 1;
			Int32 found = _vmb.NextAssignedRegEntry(startRegIdx);
			if (found >= 0) return -(found + 2);
			// Fall through to hash table phase
		}

		// Phase 2: dense entries (skip tombstones)
		Int32 i = (after < 0) ? 0 : after + 1;
		if (_entryHashes == null) return -1;
		while (i < _entryCount) {
			if (_entryHashes[i] != DELETED) return i;
			i++;
		}
		return -1;
	}

	public Value KeyAt(Int32 i) {
		if (i < -1 && _vmb != null) {
			Int32 regIdx = -(i) - 2;
			return _vmb.GetRegEntryKey(regIdx);
		}
		return _entryKeys[i];
	}

	public Value ValueAt(Int32 i) {
		if (i < -1 && _vmb != null) {
			Int32 regIdx = -(i) - 2;
			return _vmb.GetRegEntryValue(regIdx);
		}
		return _entryVals[i];
	}

	// ── GC ────────────────────────────────────────────────────────────────────

	public void MarkChildren(GCManager gc) {
		if (_entryHashes != null) {
			for (Int32 i = 0; i < _entryCount; i++) {
				if (_entryHashes[i] != DELETED) {
					gc.Mark(_entryKeys[i]);
					gc.Mark(_entryVals[i]);
				}
			}
		}
		_vmb?.MarkChildren(gc);
	}

	public void OnSweep() {
		_entryKeys   = null;
		_entryVals   = null;
		_entryHashes = null;
		_indices     = null;
		_entryCount  = 0;
		_liveCount   = 0;
		_cap         = 0;
		Frozen       = false;
		_vmb         = null;
	}

	// ── Internals ─────────────────────────────────────────────────────────────

	private void GrowEntries() {
		Int32 newLen = _entryKeys.Length * 2;
		Array.Resize(ref _entryKeys,   newLen);
		Array.Resize(ref _entryVals,   newLen);
		Array.Resize(ref _entryHashes, newLen);
	}

	// Compact entries (drop tombstones), then rebuild the indices table at newCap.
	private void Resize(Int32 newCap) {
		// Compact entries in place, preserving insertion order.
		Int32 writeIdx = 0;
		for (Int32 i = 0; i < _entryCount; i++) {
			if (_entryHashes[i] != DELETED) {
				if (writeIdx != i) {
					_entryKeys[writeIdx]   = _entryKeys[i];
					_entryVals[writeIdx]   = _entryVals[i];
					_entryHashes[writeIdx] = _entryHashes[i];
				}
				writeIdx++;
			}
		}
		// Clear the now-unused tail.
		for (Int32 i = writeIdx; i < _entryCount; i++) {
			_entryKeys[i]   = Value.Null();
			_entryVals[i]   = Value.Null();
			_entryHashes[i] = 0;
		}
		_entryCount = writeIdx;
		_liveCount  = writeIdx;

		// Allocate new indices table and rebuild from the compact entries.
		_cap     = newCap;
		_indices = new Int32[_cap];
		Array.Fill(_indices, EMPTY_SLOT);
		// Entries array must have room for at least _liveCount; grow if newCap demands more.
		if (_entryKeys.Length < _cap) {
			Array.Resize(ref _entryKeys,   _cap);
			Array.Resize(ref _entryVals,   _cap);
			Array.Resize(ref _entryHashes, _cap);
		}
		for (Int32 i = 0; i < _entryCount; i++) {
			Int32 h = _entryHashes[i];
			Int32 slot = h & (_cap - 1);
			while (_indices[slot] != EMPTY_SLOT) slot = (slot + 1) & (_cap - 1);
			_indices[slot] = i;
		}
	}

	// Hash by content for strings; by bits for everything else.
	// Ensures tiny strings and GCStrings with equal content map to the same bucket.
	private static Int32 Hash(Value v) {
		Int32 h;
		if (v.IsString) {
			String s = GCManager.GetStringContent(v);
			h = s.GetHashCode(System.StringComparison.Ordinal);
		} else {
			h = (Int32)(v.Bits ^ (v.Bits >> 32));
		}
		if (h == DELETED) h = 0;  // reserved for tombstone marker
		return h;
	}

	// Content equality for strings; bit equality for everything else.
	private static Boolean KeyEquals(Value a, Value b) {
		if (Value.Identical(a, b)) return true;
		if (a.IsString && b.IsString) {
			return String.Equals(GCManager.GetStringContent(a), GCManager.GetStringContent(b),
				System.StringComparison.Ordinal);
		}
		return false;
	}

	private static Int32 NextPow2(Int32 n) {
		Int32 p = 8;  // (minimum size)
		while (p < n) p <<= 1;
		return p;
	}
}

// ── GCError ──────────────────────────────────────────────────────────────────

public struct GCError : IGCItem {
	public Value Message;
	public Value Inner;
	public Value Stack;
	public Value Isa;

	[MethodImpl(AggressiveInlining)]
	public void MarkChildren(GCManager gc) {
		gc.Mark(Message);
		gc.Mark(Inner);
		gc.Mark(Stack);
		gc.Mark(Isa);
	}

	[MethodImpl(AggressiveInlining)]
	public void OnSweep() {
		Message = Value.Null();
		Inner   = Value.Null();
		Stack   = Value.Null();
		Isa     = Value.Null();
	}
}

// ── GCFuncRef ────────────────────────────────────────────────────────────────

public struct GCFuncRef : IGCItem {
	public Int32 FuncIndex;
	public Value OuterVars;

	[MethodImpl(AggressiveInlining)]
	public void MarkChildren(GCManager gc) {
		gc.Mark(OuterVars);
	}

	[MethodImpl(AggressiveInlining)]
	public void OnSweep() {
		FuncIndex = -1;
		OuterVars = Value.Null();
	}
}

}
//*** END CS_ONLY ***
