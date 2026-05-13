// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: VarMap.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
#include "value.h"

namespace MiniScript {

// DECLARATIONS

class VarMapBackingStorage : public std::enable_shared_from_this<VarMapBackingStorage> {
	friend struct VarMapBacking;
	private: List<Value> _regOrder; // variable names, in insertion order
	private: List<Int32> _regIndices; // register index for each name (parallel to _regOrder)
	private: List<Value> _registers; // reference to the VM's register array
	private: List<Value> _names; // reference to the VM's names array

	public: VarMapBackingStorage(List<Value> registers, List<Value> names, Int32 firstIdx, Int32 lastIdx);

	// ── Register-binding checks (used by GCMap.TryGet/Set/Remove) ────────────

	// Try to get a value via register binding. Returns false if key is not register-mapped or register is unassigned.
	public: Boolean TryGet(Value key, Value* value);

	// Try to set a value via register binding. Returns true if key was register-mapped (and value was stored).
	public: Boolean TrySet(Value key, Value value);

	// Try to remove a key via register binding. Returns true if key was register-mapped.
	public: Boolean TryRemove(Value key);

	// Returns true if the key is register-mapped and the register is assigned.
	public: Boolean HasKey(Value key);

	// ── Iteration support ─────────────────────────────────────────────────────

	// Number of register-mapped entries that are currently assigned.
	public: Int32 RegEntryCount();

	// Find the next assigned register entry index >= startIdx.
	// Returns the index into _regOrder, or -1 if none found.
	public: Int32 NextAssignedRegEntry(Int32 startIdx);

	// Get the variable name (key) for the ith register entry in insertion order.
	public: Value GetRegEntryKey(Int32 i);

	// Get the register value for the ith register entry in insertion order.
	public: Value GetRegEntryValue(Int32 i);

	// ── GC ────────────────────────────────────────────────────────────────────

	// Mark all assigned register-backed values during the GC mark phase.
	public: void MarkChildren();

	// ── VarMap operations ─────────────────────────────────────────────────────

	// Copy all register-backed values into the GCMap's hash table, then detach
	// this VarMapBacking (setting map._vmb = null).
	public: void Gather(Int32 mapIdx);

	// Gather existing register values into the map, then bind to a new set of
	// registers and names arrays. Used by REPL mode when @main is re-compiled.
	// After Rebind, new bindings are added as NAME/ASSIGN opcodes execute.
	public: void Rebind(Int32 mapIdx, List<Value> registers, List<Value> names);

	// Map a variable name to a specific register index.
	// If the name already exists as a plain map entry, copy the value into
	// the register and remove it from the map.
	public: void MapToRegister(Int32 mapIdx, Value varName, List<Value> registers, Int32 regIndex);

	// Clear all register assignments (sets names to null for all mapped registers).
	public: void Clear();

	// ── Factory ──────────────────────────────────────────────────────────────

	// Allocate a new VarMap-backed GCMap and return it as a Value.
	public: static Value NewVarMap(List<Value> registers, List<Value> names, Int32 firstIdx, Int32 lastIdx);

	// ── Internal ──────────────────────────────────────────────────────────────

	// Returns the index into _regOrder/_regIndices for the given key, or -1 if not found.
	private: Int32 FindOrderIdx(Value key);
}; // end of class VarMapBackingStorage

// VarMapBacking holds the register-binding metadata for a VarMap-backed GCMap.
// It bridges between register-based local variables and map semantics, allowing
// fast register access while maintaining map iteration / closure-capture semantics.
struct VarMapBacking {
	friend class VarMapBackingStorage;
	protected: std::shared_ptr<VarMapBackingStorage> storage;
  public:
	VarMapBacking(std::shared_ptr<VarMapBackingStorage> stor) : storage(stor) {}
	VarMapBacking() : storage(nullptr) {}
	VarMapBacking(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const VarMapBacking& inst) { return inst.storage == nullptr; }
	private: VarMapBackingStorage* get() const;

	private: List<Value> _regOrder(); // variable names, in insertion order
	private: void set__regOrder(List<Value> _v); // variable names, in insertion order
	private: List<Int32> _regIndices(); // register index for each name (parallel to _regOrder)
	private: void set__regIndices(List<Int32> _v); // register index for each name (parallel to _regOrder)
	private: List<Value> _registers(); // reference to the VM's register array
	private: void set__registers(List<Value> _v); // reference to the VM's register array
	private: List<Value> _names(); // reference to the VM's names array
	private: void set__names(List<Value> _v); // reference to the VM's names array

	public: static VarMapBacking New(List<Value> registers, List<Value> names, Int32 firstIdx, Int32 lastIdx) {
		return VarMapBacking(std::make_shared<VarMapBackingStorage>(registers, names, firstIdx, lastIdx));
	}

	// ── Register-binding checks (used by GCMap.TryGet/Set/Remove) ────────────

	// Try to get a value via register binding. Returns false if key is not register-mapped or register is unassigned.
	public: inline Boolean TryGet(Value key, Value* value);

	// Try to set a value via register binding. Returns true if key was register-mapped (and value was stored).
	public: inline Boolean TrySet(Value key, Value value);

	// Try to remove a key via register binding. Returns true if key was register-mapped.
	public: inline Boolean TryRemove(Value key);

	// Returns true if the key is register-mapped and the register is assigned.
	public: inline Boolean HasKey(Value key);

	// ── Iteration support ─────────────────────────────────────────────────────

	// Number of register-mapped entries that are currently assigned.
	public: inline Int32 RegEntryCount();

	// Find the next assigned register entry index >= startIdx.
	// Returns the index into _regOrder, or -1 if none found.
	public: inline Int32 NextAssignedRegEntry(Int32 startIdx);

	// Get the variable name (key) for the ith register entry in insertion order.
	public: inline Value GetRegEntryKey(Int32 i);

	// Get the register value for the ith register entry in insertion order.
	public: inline Value GetRegEntryValue(Int32 i);

	// ── GC ────────────────────────────────────────────────────────────────────

	// Mark all assigned register-backed values during the GC mark phase.
	public: inline void MarkChildren();

	// ── VarMap operations ─────────────────────────────────────────────────────

	// Copy all register-backed values into the GCMap's hash table, then detach
	// this VarMapBacking (setting map._vmb = null).
	public: inline void Gather(Int32 mapIdx);

	// Gather existing register values into the map, then bind to a new set of
	// registers and names arrays. Used by REPL mode when @main is re-compiled.
	// After Rebind, new bindings are added as NAME/ASSIGN opcodes execute.
	public: inline void Rebind(Int32 mapIdx, List<Value> registers, List<Value> names);

	// Map a variable name to a specific register index.
	// If the name already exists as a plain map entry, copy the value into
	// the register and remove it from the map.
	public: inline void MapToRegister(Int32 mapIdx, Value varName, List<Value> registers, Int32 regIndex);

	// Clear all register assignments (sets names to null for all mapped registers).
	public: inline void Clear();

	// ── Factory ──────────────────────────────────────────────────────────────

	// Allocate a new VarMap-backed GCMap and return it as a Value.
	public: static Value NewVarMap(List<Value> registers, List<Value> names, Int32 firstIdx, Int32 lastIdx) { return VarMapBackingStorage::NewVarMap(registers, names, firstIdx, lastIdx); }

	// ── Internal ──────────────────────────────────────────────────────────────

	// Returns the index into _regOrder/_regIndices for the given key, or -1 if not found.
	private: inline Int32 FindOrderIdx(Value key);
}; // end of struct VarMapBacking

// INLINE METHODS

inline VarMapBackingStorage* VarMapBacking::get() const { return static_cast<VarMapBackingStorage*>(storage.get()); }
inline List<Value> VarMapBacking::_regOrder() { return get()->_regOrder; } // variable names, in insertion order
inline void VarMapBacking::set__regOrder(List<Value> _v) { get()->_regOrder = _v; } // variable names, in insertion order
inline List<Int32> VarMapBacking::_regIndices() { return get()->_regIndices; } // register index for each name (parallel to _regOrder)
inline void VarMapBacking::set__regIndices(List<Int32> _v) { get()->_regIndices = _v; } // register index for each name (parallel to _regOrder)
inline List<Value> VarMapBacking::_registers() { return get()->_registers; } // reference to the VM's register array
inline void VarMapBacking::set__registers(List<Value> _v) { get()->_registers = _v; } // reference to the VM's register array
inline List<Value> VarMapBacking::_names() { return get()->_names; } // reference to the VM's names array
inline void VarMapBacking::set__names(List<Value> _v) { get()->_names = _v; } // reference to the VM's names array
inline Boolean VarMapBacking::TryGet(Value key,Value* value) { return get()->TryGet(key, value); }
inline Boolean VarMapBacking::TrySet(Value key,Value value) { return get()->TrySet(key, value); }
inline Boolean VarMapBacking::TryRemove(Value key) { return get()->TryRemove(key); }
inline Boolean VarMapBacking::HasKey(Value key) { return get()->HasKey(key); }
inline Int32 VarMapBacking::RegEntryCount() { return get()->RegEntryCount(); }
inline Int32 VarMapBacking::NextAssignedRegEntry(Int32 startIdx) { return get()->NextAssignedRegEntry(startIdx); }
inline Value VarMapBacking::GetRegEntryKey(Int32 i) { return get()->GetRegEntryKey(i); }
inline Value VarMapBacking::GetRegEntryValue(Int32 i) { return get()->GetRegEntryValue(i); }
inline void VarMapBacking::MarkChildren() { return get()->MarkChildren(); }
inline void VarMapBacking::Gather(Int32 mapIdx) { return get()->Gather(mapIdx); }
inline void VarMapBacking::Rebind(Int32 mapIdx,List<Value> registers,List<Value> names) { return get()->Rebind(mapIdx, registers, names); }
inline void VarMapBacking::MapToRegister(Int32 mapIdx,Value varName,List<Value> registers,Int32 regIndex) { return get()->MapToRegister(mapIdx, varName, registers, regIndex); }
inline void VarMapBacking::Clear() { return get()->Clear(); }
inline Int32 VarMapBacking::FindOrderIdx(Value key) { return get()->FindOrderIdx(key); }
inline Int32 VarMapBackingStorage::FindOrderIdx(Value key) {
	for (Int32 i = 0; i < _regOrder.Count(); i++) {
		if (value_equal(_regOrder[i], key)) return i;
	}
	return -1;
}

} // end of namespace MiniScript
