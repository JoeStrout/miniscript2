//*** BEGIN CS_ONLY ***
using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

using static MiniScript.ValueHelpers;

namespace MiniScript {

	/// <summary>
	/// Custom equality comparer for Value that uses semantic equality instead of bit equality
	/// </summary>
	public class ValueEqualityComparer : IEqualityComparer<Value> {
		public bool Equals(Value x, Value y) => Value.Equal(x, y);

		public int GetHashCode(Value obj) {
			// For strings, hash the string content rather than the handle
			if (obj.IsString) {
				string str = obj.ToString();
				return str.GetHashCode();
			}
			// For other types, use the bits as hash
			return obj.Bits.GetHashCode();
		}
	}

	/// <summary>
	/// VarMap is a specialized map that bridges between register-based variables
	/// and traditional map operations. It maps some string keys to VM stack slots,
	/// allowing fast register access while maintaining map semantics.
	/// </summary>
	public class VarMap : ValueMap {
		private Dictionary<Value, int> _regMap = new Dictionary<Value, int>(new ValueEqualityComparer());
		private List<Value> _registers;  // Reference to VM's register array
		private List<Value> _names;      // Reference to VM's register names array

		public VarMap(List<Value> registers, List<Value> names, int firstIdx, int lastIdx) {
			_registers = registers;
			_names = names;
			for (int i = firstIdx; i <= lastIdx; i++) {
				if (!_names[i].IsNull) _regMap[_names[i]] = i;
			}
		}

		/// <summary>
		/// Map a variable name to a specific register index.
		/// This creates the special register-backed behavior for this key.
		/// </summary>
		public void MapToRegister(Value varName, int regIndex) {
			_regMap[varName] = regIndex;
		}

		/// <summary>
		/// Get a value by key. If the key maps to a register, return the register
		/// value (if assigned) or perform outer/global lookup. Otherwise, use
		/// standard map behavior.
		/// </summary>
		public override Value Get(Value key) {
			if (is_string(key) && _regMap.TryGetValue(key, out int regIndex)) {
				// Check if register is assigned (has non-null name)
				if (!_names[regIndex].IsNull) return _registers[regIndex];
			}

			// Fall back to standard map behavior
			return base.Get(key);
		}

		/// <summary>
		/// Set a value by key. If the key maps to a register, store directly
		/// in the register and mark as assigned. Otherwise, use standard map storage.
		/// </summary>
		public override bool Set(Value key, Value value) {
			if (is_string(key) && _regMap.TryGetValue(key, out int regIndex)) {
				// Store in register and mark as assigned
				_registers[regIndex] = value;
				_names[regIndex] = key;
				return true;
			}

			// Fall back to standard map behavior
			return base.Set(key, value);
		}

		/// <summary>
		/// Remove a key. If it maps to a register, clear the assignment.
		/// Otherwise, use standard map removal.
		/// </summary>
		public override bool Remove(Value key) {
			if (is_string(key) && _regMap.TryGetValue(key, out int regIndex)) {
				// Clear assignment by setting name to null
				_names[regIndex] = make_null();
				return true;
			}

			// Fall back to standard map behavior
			return base.Remove(key);
		}

		/// <summary>
		/// Check if a key exists. For register-mapped keys, check if assigned.
		/// </summary>
		public override bool HasKey(Value key) {
			if (is_string(key) && _regMap.TryGetValue(key, out int regIndex)) {
				// Key exists if register is assigned
				return !_names[regIndex].IsNull;
			}

			// Fall back to standard map behavior
			return base.HasKey(key);
		}

		/// <summary>
		/// Gather all currently assigned register variables into the regular map
		/// storage, then clear the register mappings. After this operation,
		/// the VarMap behaves like a regular map.
		/// </summary>
		public void Gather() {
			foreach (var kvp in _regMap) {
				Value varName = kvp.Key;
				int regIndex = kvp.Value;

				// If register is assigned, copy to regular map storage
				if (!_names[regIndex].IsNull) {
					Value value = _registers[regIndex];
					base.Set(varName, value);
				}
			}

			// Clear all register mappings
			_regMap.Clear();			
		}

		/// <summary>
		/// Get the total count including both register-mapped and regular entries.
		/// </summary>
		public override int Count {
			get {
				int regCount = 0;
				foreach (var kvp in _regMap) {
					if (!_names[kvp.Value].IsNull) regCount++;
				}
				return base.Count + regCount;
			}
		}

		/// <summary>
		/// Enumerate over both register-mapped variables and regular map entries.
		/// </summary>
		public override IEnumerable<KeyValuePair<Value, Value>> Items {
			get {
				// First, yield all assigned register variables
				foreach (var kvp in _regMap) {
					Value varName = kvp.Key;
					int regIndex = kvp.Value;
					if (!_names[regIndex].IsNull) {
						yield return new KeyValuePair<Value, Value>(varName, _registers[regIndex]);
					}
				}

				// Then, yield all regular map entries
				foreach (var kvp in base.Items) {
					yield return kvp;
				}
			}
		}
	}
}
//*** END CS_ONLY ***