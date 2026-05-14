using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
using static MiniScript.ValueHelpers;
// H: #include "value.h"
// CPP: #include "GCManager.g.h"

namespace MiniScript {

//
// VarMapBacking holds the register-binding metadata for a VarMap-backed GCMap.
// It bridges between register-based local variables and map semantics, allowing
// fast register access while maintaining map iteration / closure-capture semantics.
//
public class VarMapBacking {
	private List<Value> _regOrder;    // variable names, in insertion order
	private List<Int32> _regIndices;  // register index for each name (parallel to _regOrder)
	private List<Value> _registers;   // reference to the VM's register array
	private List<Value> _names;       // reference to the VM's names array

	public VarMapBacking(List<Value> registers, List<Value> names, Int32 firstIdx, Int32 lastIdx) {
		_registers  = registers;
		_names      = names;
		_regOrder   = new List<Value>();
		_regIndices = new List<Int32>();

		for (Int32 i = firstIdx; i <= lastIdx; i++) {
			if (!is_null(_names[i])) {
				Int32 orderIdx = FindOrderIdx(_names[i]);
				if (orderIdx < 0) {
					_regOrder.Add(_names[i]);
					_regIndices.Add(i);
				} else {
					_regIndices[orderIdx] = i;
				}
			}
		}
	}

	// ── Register-binding checks (used by GCMap.TryGet/Set/Remove) ────────────

	// Try to get a value via register binding. Returns false if key is not register-mapped or register is unassigned.
	public Boolean TryGet(Value key, out Value value) {
		if (is_string(key)) {
			Int32 orderIdx = FindOrderIdx(key);
			if (orderIdx >= 0) {
				Int32 regIdx = _regIndices[orderIdx];
				if (!is_null(_names[regIdx])) {
					value = _registers[regIdx];
					return true;
				}
			}
		}
		value = val_null;
		return false;
	}

	// Try to set a value via register binding. Returns true if key was register-mapped (and value was stored).
	public Boolean TrySet(Value key, Value value) {
		if (is_string(key)) {
			Int32 orderIdx = FindOrderIdx(key);
			if (orderIdx >= 0) {
				Int32 regIdx = _regIndices[orderIdx];
				_registers[regIdx] = value;
				_names[regIdx]     = key;
				return true;
			}
		}
		return false;
	}

	// Try to remove a key via register binding. Returns true if key was register-mapped.
	public Boolean TryRemove(Value key) {
		if (is_string(key)) {
			Int32 orderIdx = FindOrderIdx(key);
			if (orderIdx >= 0) {
				Int32 regIdx = _regIndices[orderIdx];
				_names[regIdx] = val_null;
				return true;
			}
		}
		return false;
	}

	// Returns true if the key is register-mapped and the register is assigned.
	public Boolean HasKey(Value key) {
		if (is_string(key)) {
			Int32 orderIdx = FindOrderIdx(key);
			if (orderIdx >= 0) {
				Int32 regIdx = _regIndices[orderIdx];
				return !is_null(_names[regIdx]);
			}
		}
		return false;
	}

	// ── Iteration support ─────────────────────────────────────────────────────

	// Number of register-mapped entries that are currently assigned.
	public Int32 RegEntryCount() {
		Int32 n = 0;
		for (Int32 i = 0; i < _regOrder.Count; i++) {
			Int32 regIdx = _regIndices[i];
			if (!is_null(_names[regIdx])) n++;
		}
		return n;
	}

	//
	// Find the next assigned register entry index >= startIdx.
	// Returns the index into _regOrder, or -1 if none found.
	//
	public Int32 NextAssignedRegEntry(Int32 startIdx) {
		for (Int32 i = startIdx; i < _regOrder.Count; i++) {
			Int32 regIdx = _regIndices[i];
			if (!is_null(_names[regIdx])) return i;
		}
		return -1;
	}

	// Get the variable name (key) for the ith register entry in insertion order.
	public Value GetRegEntryKey(Int32 i) {
		if (i < 0 || i >= _regOrder.Count) return val_null;
		return _regOrder[i];
	}

	// Get the register value for the ith register entry in insertion order.
	public Value GetRegEntryValue(Int32 i) {
		if (i < 0 || i >= _regOrder.Count) return val_null;
		Int32 regIdx = _regIndices[i];
		return _registers[regIdx];
	}

	// ── GC ────────────────────────────────────────────────────────────────────

	// Mark all assigned register-backed values during the GC mark phase.
	public void MarkChildren() {
		for (Int32 i = 0; i < _regOrder.Count; i++) {
			Int32 regIdx = _regIndices[i];
			if (regIdx < _names.Count && !is_null(_names[regIdx])) {
				GCManager.Mark(_regOrder[i]);
				GCManager.Mark(_registers[regIdx]);
			}
		}
	}

	// ── VarMap operations ─────────────────────────────────────────────────────

	//
	// Copy all register-backed values into the GCMap's hash table, then detach
	// this VarMapBacking (setting map._vmb = null).
	//
	public void Gather(Int32 mapIdx) {
		// Temporarily detach to prevent recursion inside map.Set().
		GCManager.Maps.SetVmb(mapIdx, null);
		GCMap map = GCManager.Maps.Get(mapIdx);
		for (Int32 i = 0; i < _regOrder.Count; i++) {
			Int32 regIdx = _regIndices[i];
			if (regIdx < _names.Count && !is_null(_names[regIdx])) {
				map.Set(_regOrder[i], _registers[regIdx]);
			}
		}
		// Leave _vmb = null (gathered; no more register backing).
	}

	//
	// Gather existing register values into the map, then bind to a new set of
	// registers and names arrays. Used by REPL mode when @main is re-compiled.
	// After Rebind, new bindings are added as NAME/ASSIGN opcodes execute.
	//
	public void Rebind(Int32 mapIdx, List<Value> registers, List<Value> names) {
		Gather(mapIdx);
		// Re-attach with new arrays (Gather set _vmb = null; we have to restore it).
		_registers = registers;
		_names     = names;
		_regOrder.Clear();
		_regIndices.Clear();
		GCManager.Maps.SetVmb(mapIdx, this);
	}

	//
	// Map a variable name to a specific register index.
	// If the name already exists as a plain map entry, copy the value into
	// the register and remove it from the map.
	//
	public void MapToRegister(Int32 mapIdx, Value varName, List<Value> registers, Int32 regIndex) {
		Int32 orderIdx = FindOrderIdx(varName);
		if (orderIdx < 0) {
			_regOrder.Add(varName);
			_regIndices.Add(regIndex);
		} else {
			_regIndices[orderIdx] = regIndex;
		}
		// If there was an existing plain map entry, move its value to the register.
		// Bypass _vmb during this lookup so we read from the hash table, not the
		// (just-mapped) register, which may be empty/null on a fresh stack.
		GCMap map = GCManager.Maps.Get(mapIdx);
		VarMapBacking saved = map._vmb;
		GCManager.Maps.SetVmb(mapIdx, null);
		map = GCManager.Maps.Get(mapIdx);  // refresh after SetVmb
		Value existingVal;
		if (map.TryGet(varName, out existingVal)) {
			registers[regIndex] = existingVal;
			_names[regIndex]    = varName;   // mark register as live so TryGet can find it
			map.Remove(varName);
		}
		GCManager.Maps.SetVmb(mapIdx, saved);
	}

	// Clear all register assignments (sets names to null for all mapped registers).
	public void Clear() {
		for (Int32 i = 0; i < _regOrder.Count; i++) {
			Int32 regIdx = _regIndices[i];
			if (regIdx < _names.Count) _names[regIdx] = val_null;
		}
	}

	// ── Factory ──────────────────────────────────────────────────────────────

	// Allocate a new VarMap-backed GCMap and return it as a Value.
	public static Value NewVarMap(List<Value> registers, List<Value> names, Int32 firstIdx, Int32 lastIdx) {
		VarMapBacking vmb = new VarMapBacking(registers, names, firstIdx, lastIdx);
		Int32 idx = GCManager.Maps.AllocItem();
		GCManager.Maps.Init(idx, 4);
		GCManager.Maps.SetVmb(idx, vmb);
		return make_gc(GCManager.MapSet, idx);
	}

	// ── Internal ──────────────────────────────────────────────────────────────

	// Returns the index into _regOrder/_regIndices for the given key, or -1 if not found.
	[MethodImpl(AggressiveInlining)]
	private Int32 FindOrderIdx(Value key) {
		for (Int32 i = 0; i < _regOrder.Count; i++) {
			if (value_equal(_regOrder[i], key)) return i;
		}
		return -1;
	}
}

}
