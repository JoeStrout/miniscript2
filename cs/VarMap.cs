//*** BEGIN CS_ONLY ***
using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

using static MiniScript.ValueHelpers;

namespace MiniScript {

/// <summary>
/// Custom equality comparer for Value that uses semantic (content-based) equality.
/// Used as the comparer for VarMapBacking._regMap.
/// </summary>
public class ValueEqualityComparer : IEqualityComparer<Value> {
	public bool Equals(Value x, Value y) => Value.Equal(x, y);

	public int GetHashCode(Value obj) {
		if (obj.IsString) {
			return GCManager.GetStringContent(obj).GetHashCode(StringComparison.Ordinal);
		}
		return obj.Bits.GetHashCode();
	}
}

/// <summary>
/// VarMapBacking holds the register-binding metadata for a VarMap-backed GCMap.
/// It bridges between register-based local variables and map semantics, allowing
/// fast register access while maintaining map iteration / closure-capture semantics.
///
/// This class is C#-only; it is not transpilable.
/// </summary>
public class VarMapBacking {
	private Dictionary<Value, int> _regMap;   // varName → register index
	private List<Value> _registers;           // reference to the VM's register array
	private List<Value> _names;               // reference to the VM's names array

	// For deterministic iteration, keep an ordered list of reg-entry keys.
	private List<Value> _regOrder;

	public VarMapBacking(List<Value> registers, List<Value> names, int firstIdx, int lastIdx) {
		_registers = registers;
		_names     = names;
		_regMap    = new Dictionary<Value, int>(new ValueEqualityComparer());
		_regOrder  = new List<Value>();

		for (int i = firstIdx; i <= lastIdx; i++) {
			if (!_names[i].IsNull) {
				if (!_regMap.ContainsKey(_names[i])) {
					_regOrder.Add(_names[i]);
				}
				_regMap[_names[i]] = i;
			}
		}
	}

	// ── Register-binding checks (used by GCMap.TryGet/Set/Remove) ────────────

	/// <summary>Try to get a value via register binding. Returns false if key is not register-mapped or register is unassigned.</summary>
	public bool TryGet(Value key, out Value value) {
		if (is_string(key) && _regMap.TryGetValue(key, out int regIndex)) {
			if (!_names[regIndex].IsNull) {
				value = _registers[regIndex];
				return true;
			}
		}
		value = Value.Null();
		return false;
	}

	/// <summary>Try to set a value via register binding. Returns true if key was register-mapped (and value was stored).</summary>
	public bool TrySet(Value key, Value value) {
		if (is_string(key) && _regMap.TryGetValue(key, out int regIndex)) {
			_registers[regIndex] = value;
			_names[regIndex]     = key;
			return true;
		}
		return false;
	}

	/// <summary>Try to remove a key via register binding. Returns true if key was register-mapped.</summary>
	public bool TryRemove(Value key) {
		if (is_string(key) && _regMap.TryGetValue(key, out int regIndex)) {
			_names[regIndex] = val_null;
			return true;
		}
		return false;
	}

	/// <summary>Returns true if the key is register-mapped and the register is assigned.</summary>
	public bool HasKey(Value key) {
		if (is_string(key) && _regMap.TryGetValue(key, out int regIndex)) {
			return !_names[regIndex].IsNull;
		}
		return false;
	}

	// ── Iteration support ─────────────────────────────────────────────────────

	/// <summary>Number of register-mapped entries that are currently assigned.</summary>
	public int RegEntryCount {
		get {
			int n = 0;
			foreach (var kvp in _regMap) {
				if (!_names[kvp.Value].IsNull) n++;
			}
			return n;
		}
	}

	/// <summary>
	/// Find the next assigned register entry index >= startIdx.
	/// Returns the index into _regOrder, or -1 if none found.
	/// </summary>
	public int NextAssignedRegEntry(int startIdx) {
		for (int i = startIdx; i < _regOrder.Count; i++) {
			Value key = _regOrder[i];
			if (_regMap.TryGetValue(key, out int regIdx) && !_names[regIdx].IsNull) {
				return i;
			}
		}
		return -1;
	}

	/// <summary>Get the variable name (key) for the ith register entry in insertion order.</summary>
	public Value GetRegEntryKey(int i) {
		if (i < 0 || i >= _regOrder.Count) return val_null;
		return _regOrder[i];
	}

	/// <summary>Get the register value for the ith register entry in insertion order.</summary>
	public Value GetRegEntryValue(int i) {
		if (i < 0 || i >= _regOrder.Count) return val_null;
		Value key = _regOrder[i];
		if (_regMap.TryGetValue(key, out int regIdx)) return _registers[regIdx];
		return val_null;
	}

	// ── GC ────────────────────────────────────────────────────────────────────

	/// <summary>Mark all assigned register-backed values during the GC mark phase.</summary>
	public void MarkChildren(GCManager gc) {
		foreach (var kvp in _regMap) {
			int regIdx = kvp.Value;
			if (regIdx < _names.Count && !_names[regIdx].IsNull) {
				gc.Mark(kvp.Key);              // mark the variable name (a string value)
				gc.Mark(_registers[regIdx]);   // mark the variable value
			}
		}
	}

	// ── VarMap operations ─────────────────────────────────────────────────────

	/// <summary>
	/// Copy all register-backed values into the GCMap's hash table, then detach
	/// this VarMapBacking (setting map._vmb = null).
	/// </summary>
	public void Gather(ref GCMap map) {
		// Temporarily detach to prevent recursion inside map.Set().
		map._vmb = null;
		foreach (var kvp in _regMap) {
			int regIdx = kvp.Value;
			if (regIdx < _names.Count && !_names[regIdx].IsNull) {
				map.Set(kvp.Key, _registers[regIdx]);
			}
		}
		// Leave _vmb = null (gathered; no more register backing).
	}

	/// <summary>
	/// Gather existing register values into the map, then bind to a new set of
	/// registers and names arrays. Used by REPL mode when @main is re-compiled.
	/// After Rebind, new bindings are added as NAME/ASSIGN opcodes execute.
	/// </summary>
	public void Rebind(ref GCMap map, List<Value> registers, List<Value> names) {
		Gather(ref map);
		// Re-attach with new arrays (Gather set _vmb = null; we have to restore it).
		_registers = registers;
		_names     = names;
		_regMap.Clear();
		_regOrder.Clear();
		map._vmb = this;
	}

	/// <summary>
	/// Map a variable name to a specific register index.
	/// If the name already exists as a plain map entry, copy the value into
	/// the register and remove it from the map.
	/// </summary>
	public void MapToRegister(ref GCMap map, Value varName, List<Value> registers, int regIndex) {
		if (!_regMap.ContainsKey(varName)) _regOrder.Add(varName);
		_regMap[varName] = regIndex;
		// If there was an existing plain map entry, move its value to the register.
		// Bypass _vmb during this lookup so we read from the hash table, not the
		// (just-mapped) register, which may be empty/null on a fresh stack.
		var saved = map._vmb;
		map._vmb = null;
		if (map.TryGet(varName, out Value existingVal)) {
			registers[regIndex] = existingVal;
			_names[regIndex]    = varName;   // mark register as live so TryGet can find it
			map.Remove(varName);
		}
		map._vmb = saved;
	}

	/// <summary>Clear all register assignments (sets names to null for all mapped registers).</summary>
	public void Clear() {
		foreach (var kvp in _regMap) {
			if (kvp.Value < _names.Count) _names[kvp.Value] = val_null;
		}
	}

	// ── Enumeration (for map_count and for_in iteration) ──────────────────────

	/// <summary>Enumerate all assigned register entries as key-value pairs.</summary>
	public IEnumerable<(Value key, Value val)> AssignedEntries() {
		foreach (Value key in _regOrder) {
			if (_regMap.TryGetValue(key, out int regIdx) && !_names[regIdx].IsNull) {
				yield return (key, _registers[regIdx]);
			}
		}
	}
}

}
//*** END CS_ONLY ***
