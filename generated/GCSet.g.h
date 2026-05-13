// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: GCSet.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
#include "GCInterfaces.g.h"
#include "GCItems.g.h"

namespace MiniScript {

// DECLARATIONS

// Non-generic abstract base for all GC item pools.
// Manages bookkeeping metadata (InUse, Marked, RetainCount, free-list)
// and implements IGCSet.  Subclasses supply the typed item list and
// the three abstract item operations.
struct GCSetBase : public IGCSet {
	friend class GCSetBaseStorage;
	protected: std::shared_ptr<GCSetBaseStorage> storage;
  public:
	GCSetBase(std::shared_ptr<GCSetBaseStorage> stor) : storage(stor) {}
	GCSetBase() : storage(nullptr) {}
	GCSetBase(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const GCSetBase& inst) { return inst.storage == nullptr; }
	private: GCSetBaseStorage* get() const;

	protected: List<Boolean> _inUse();
	protected: void set__inUse(List<Boolean> _v);
	protected: List<Boolean> _marked();
	protected: void set__marked(List<Boolean> _v);
	protected: List<Byte> _retainCounts();
	protected: void set__retainCounts(List<Byte> _v);
	protected: List<Int32> _free();
	protected: void set__free(List<Int32> _v);
	protected: void CallMarkChildren(Int32 idx);
	protected: void CallOnSweep(Int32 idx);
	protected: void AppendItem();

	// Subclass calls item[idx].MarkChildren().

	// Subclass calls item[idx].OnSweep().

	// Subclass appends a default-constructed item to its items list.

	// ── Allocation ───────────────────────────────────────────────────────────

	public: inline Int32 AllocItem();

	// ── Retain / Release ─────────────────────────────────────────────────────

	public: inline void Retain(Int32 idx);

	public: inline void Release(Int32 idx);

	// ── IGCSet implementation ─────────────────────────────────────────────────

	public: inline void PrepareForGC();

	public: inline void Mark(Int32 idx);

	public: inline void MarkRetained();

	public: inline void Sweep();

	public: inline Int32 LiveCount();
}; // end of struct GCSetBase

template<typename WrapperType, typename StorageType> WrapperType As(GCSetBase inst);

class GCSetBaseStorage : public std::enable_shared_from_this<GCSetBaseStorage>, public IGCSet {
	friend struct GCSetBase;
	protected: List<Boolean> _inUse = List<Boolean>::New();
	protected: List<Boolean> _marked = List<Boolean>::New();
	protected: List<Byte> _retainCounts = List<Byte>::New();
	protected: List<Int32> _free = List<Int32>::New();
	protected: virtual void CallMarkChildren(Int32 idx) = 0;
	protected: virtual void CallOnSweep(Int32 idx) = 0;
	protected: virtual void AppendItem() = 0;

	// Subclass calls item[idx].MarkChildren().

	// Subclass calls item[idx].OnSweep().

	// Subclass appends a default-constructed item to its items list.

	// ── Allocation ───────────────────────────────────────────────────────────

	public: Int32 AllocItem();

	// ── Retain / Release ─────────────────────────────────────────────────────

	public: void Retain(Int32 idx);

	public: void Release(Int32 idx);

	// ── IGCSet implementation ─────────────────────────────────────────────────

	public: void PrepareForGC();

	public: void Mark(Int32 idx);

	public: void MarkRetained();

	public: void Sweep();

	public: Int32 LiveCount();
}; // end of class GCSetBaseStorage

class GCStringSetStorage : public GCSetBaseStorage {
	friend struct GCStringSet;
	private: List<GCString> _items;

	public: GCStringSetStorage(Int32 initialCapacity = 64);

	protected: void CallMarkChildren(Int32 idx);
	protected: void CallOnSweep(Int32 idx);
	protected: void AppendItem();

	public: GCString Get(Int32 idx);

	public: void SetData(Int32 idx, String s);
}; // end of class GCStringSetStorage

class GCListSetStorage : public GCSetBaseStorage {
	friend struct GCListSet;
	private: List<GCList> _items;

	public: GCListSetStorage(Int32 initialCapacity = 64);

	protected: void CallMarkChildren(Int32 idx);
	protected: void CallOnSweep(Int32 idx);
	protected: void AppendItem();

	public: GCList Get(Int32 idx);

	public: void Init(Int32 idx, Int32 capacity);

	public: void SetFrozen(Int32 idx, Boolean frozen);
}; // end of class GCListSetStorage

class GCMapSetStorage : public GCSetBaseStorage {
	friend struct GCMapSet;
	private: List<GCMap> _items;

	public: GCMapSetStorage(Int32 initialCapacity = 64);

	protected: void CallMarkChildren(Int32 idx);
	protected: void CallOnSweep(Int32 idx);
	protected: void AppendItem();

	public: GCMap Get(Int32 idx);

	public: void Init(Int32 idx, Int32 capacity);

	public: void SetFrozen(Int32 idx, Boolean frozen);

	public: void SetVmb(Int32 idx, VarMapBacking vmb);
}; // end of class GCMapSetStorage

class GCErrorSetStorage : public GCSetBaseStorage {
	friend struct GCErrorSet;
	private: List<GCError> _items;

	public: GCErrorSetStorage(Int32 initialCapacity = 64);

	protected: void CallMarkChildren(Int32 idx);
	protected: void CallOnSweep(Int32 idx);
	protected: void AppendItem();

	public: GCError Get(Int32 idx);

	public: void SetFields(Int32 idx, Value message, Value inner, Value stack, Value isa);
}; // end of class GCErrorSetStorage

class GCFuncRefSetStorage : public GCSetBaseStorage {
	friend struct GCFuncRefSet;
	private: List<GCFunction> _items;

	public: GCFuncRefSetStorage(Int32 initialCapacity = 64);

	protected: void CallMarkChildren(Int32 idx);
	protected: void CallOnSweep(Int32 idx);
	protected: void AppendItem();

	public: GCFunction Get(Int32 idx);

	public: void SetFields(Int32 idx, Int32 funcIndex, Value outerVars);
}; // end of class GCFuncRefSetStorage

// ── GCStringSet ───────────────────────────────────────────────────────────────

struct GCStringSet : public GCSetBase {
	friend class GCStringSetStorage;
	GCStringSet(std::shared_ptr<GCStringSetStorage> stor);
	GCStringSet() : GCSetBase() {}
	GCStringSet(std::nullptr_t) : GCSetBase(nullptr) {}
	private: GCStringSetStorage* get() const;

	private: List<GCString> _items();
	private: void set__items(List<GCString> _v);

	public: static GCStringSet New(Int32 initialCapacity = 64) {
		return GCStringSet(std::make_shared<GCStringSetStorage>());
	}

	protected: void CallMarkChildren(Int32 idx) { return get()->CallMarkChildren(idx); }
	protected: void CallOnSweep(Int32 idx) { return get()->CallOnSweep(idx); }
	protected: void AppendItem() { return get()->AppendItem(); }

	public: inline GCString Get(Int32 idx);

	public: inline void SetData(Int32 idx, String s);
}; // end of struct GCStringSet

// ── GCListSet ─────────────────────────────────────────────────────────────────

struct GCListSet : public GCSetBase {
	friend class GCListSetStorage;
	GCListSet(std::shared_ptr<GCListSetStorage> stor);
	GCListSet() : GCSetBase() {}
	GCListSet(std::nullptr_t) : GCSetBase(nullptr) {}
	private: GCListSetStorage* get() const;

	private: List<GCList> _items();
	private: void set__items(List<GCList> _v);

	public: static GCListSet New(Int32 initialCapacity = 64) {
		return GCListSet(std::make_shared<GCListSetStorage>());
	}

	protected: void CallMarkChildren(Int32 idx) { return get()->CallMarkChildren(idx); }
	protected: void CallOnSweep(Int32 idx) { return get()->CallOnSweep(idx); }
	protected: void AppendItem() { return get()->AppendItem(); }

	public: inline GCList Get(Int32 idx);

	public: inline void Init(Int32 idx, Int32 capacity);

	public: inline void SetFrozen(Int32 idx, Boolean frozen);
}; // end of struct GCListSet

// ── GCMapSet ──────────────────────────────────────────────────────────────────

struct GCMapSet : public GCSetBase {
	friend class GCMapSetStorage;
	GCMapSet(std::shared_ptr<GCMapSetStorage> stor);
	GCMapSet() : GCSetBase() {}
	GCMapSet(std::nullptr_t) : GCSetBase(nullptr) {}
	private: GCMapSetStorage* get() const;

	private: List<GCMap> _items();
	private: void set__items(List<GCMap> _v);

	public: static GCMapSet New(Int32 initialCapacity = 64) {
		return GCMapSet(std::make_shared<GCMapSetStorage>());
	}

	protected: void CallMarkChildren(Int32 idx) { return get()->CallMarkChildren(idx); }
	protected: void CallOnSweep(Int32 idx) { return get()->CallOnSweep(idx); }
	protected: void AppendItem() { return get()->AppendItem(); }

	public: inline GCMap Get(Int32 idx);

	public: inline void Init(Int32 idx, Int32 capacity);

	public: inline void SetFrozen(Int32 idx, Boolean frozen);

	public: inline void SetVmb(Int32 idx, VarMapBacking vmb);
}; // end of struct GCMapSet

// ── GCErrorSet ────────────────────────────────────────────────────────────────

struct GCErrorSet : public GCSetBase {
	friend class GCErrorSetStorage;
	GCErrorSet(std::shared_ptr<GCErrorSetStorage> stor);
	GCErrorSet() : GCSetBase() {}
	GCErrorSet(std::nullptr_t) : GCSetBase(nullptr) {}
	private: GCErrorSetStorage* get() const;

	private: List<GCError> _items();
	private: void set__items(List<GCError> _v);

	public: static GCErrorSet New(Int32 initialCapacity = 64) {
		return GCErrorSet(std::make_shared<GCErrorSetStorage>());
	}

	protected: void CallMarkChildren(Int32 idx) { return get()->CallMarkChildren(idx); }
	protected: void CallOnSweep(Int32 idx) { return get()->CallOnSweep(idx); }
	protected: void AppendItem() { return get()->AppendItem(); }

	public: inline GCError Get(Int32 idx);

	public: inline void SetFields(Int32 idx, Value message, Value inner, Value stack, Value isa);
}; // end of struct GCErrorSet

// ── GCFuncRefSet ──────────────────────────────────────────────────────────────

struct GCFuncRefSet : public GCSetBase {
	friend class GCFuncRefSetStorage;
	GCFuncRefSet(std::shared_ptr<GCFuncRefSetStorage> stor);
	GCFuncRefSet() : GCSetBase() {}
	GCFuncRefSet(std::nullptr_t) : GCSetBase(nullptr) {}
	private: GCFuncRefSetStorage* get() const;

	private: List<GCFunction> _items();
	private: void set__items(List<GCFunction> _v);

	public: static GCFuncRefSet New(Int32 initialCapacity = 64) {
		return GCFuncRefSet(std::make_shared<GCFuncRefSetStorage>());
	}

	protected: void CallMarkChildren(Int32 idx) { return get()->CallMarkChildren(idx); }
	protected: void CallOnSweep(Int32 idx) { return get()->CallOnSweep(idx); }
	protected: void AppendItem() { return get()->AppendItem(); }

	public: inline GCFunction Get(Int32 idx);

	public: inline void SetFields(Int32 idx, Int32 funcIndex, Value outerVars);
}; // end of struct GCFuncRefSet

// INLINE METHODS

inline GCSetBaseStorage* GCSetBase::get() const { return static_cast<GCSetBaseStorage*>(storage.get()); }
inline List<Boolean> GCSetBase::_inUse() { return get()->_inUse; }
inline void GCSetBase::set__inUse(List<Boolean> _v) { get()->_inUse = _v; }
inline List<Boolean> GCSetBase::_marked() { return get()->_marked; }
inline void GCSetBase::set__marked(List<Boolean> _v) { get()->_marked = _v; }
inline List<Byte> GCSetBase::_retainCounts() { return get()->_retainCounts; }
inline void GCSetBase::set__retainCounts(List<Byte> _v) { get()->_retainCounts = _v; }
inline List<Int32> GCSetBase::_free() { return get()->_free; }
inline void GCSetBase::set__free(List<Int32> _v) { get()->_free = _v; }
inline void GCSetBase::CallMarkChildren(Int32 idx) { return get()->CallMarkChildren(idx); }
inline void GCSetBase::CallOnSweep(Int32 idx) { return get()->CallOnSweep(idx); }
inline void GCSetBase::AppendItem() { return get()->AppendItem(); }
inline Int32 GCSetBase::AllocItem() { return get()->AllocItem(); }
inline void GCSetBase::Retain(Int32 idx) { return get()->Retain(idx); }
inline void GCSetBase::Release(Int32 idx) { return get()->Release(idx); }
inline void GCSetBase::PrepareForGC() { return get()->PrepareForGC(); }
inline void GCSetBase::Mark(Int32 idx) { return get()->Mark(idx); }
inline void GCSetBase::MarkRetained() { return get()->MarkRetained(); }
inline void GCSetBase::Sweep() { return get()->Sweep(); }
inline Int32 GCSetBase::LiveCount() { return get()->LiveCount(); }

inline GCStringSet::GCStringSet(std::shared_ptr<GCStringSetStorage> stor) : GCSetBase(stor) {}
inline GCStringSetStorage* GCStringSet::get() const { return static_cast<GCStringSetStorage*>(storage.get()); }
inline List<GCString> GCStringSet::_items() { return get()->_items; }
inline void GCStringSet::set__items(List<GCString> _v) { get()->_items = _v; }
inline GCString GCStringSet::Get(Int32 idx) { return get()->Get(idx); }
inline GCString GCStringSetStorage::Get(Int32 idx) {
	return _items[idx];
}
inline void GCStringSet::SetData(Int32 idx,String s) { return get()->SetData(idx, s); }
inline void GCStringSetStorage::SetData(Int32 idx,String s) {
	GCString item = _items[idx];
	item.Data = s;
	_items[idx] = item;
}

inline GCListSet::GCListSet(std::shared_ptr<GCListSetStorage> stor) : GCSetBase(stor) {}
inline GCListSetStorage* GCListSet::get() const { return static_cast<GCListSetStorage*>(storage.get()); }
inline List<GCList> GCListSet::_items() { return get()->_items; }
inline void GCListSet::set__items(List<GCList> _v) { get()->_items = _v; }
inline GCList GCListSet::Get(Int32 idx) { return get()->Get(idx); }
inline GCList GCListSetStorage::Get(Int32 idx) {
	return _items[idx];
}
inline void GCListSet::Init(Int32 idx,Int32 capacity) { return get()->Init(idx, capacity); }
inline void GCListSet::SetFrozen(Int32 idx,Boolean frozen) { return get()->SetFrozen(idx, frozen); }
inline void GCListSetStorage::SetFrozen(Int32 idx,Boolean frozen) {
	GCList item = _items[idx];
	item.Frozen = frozen;
	_items[idx] = item;
}

inline GCMapSet::GCMapSet(std::shared_ptr<GCMapSetStorage> stor) : GCSetBase(stor) {}
inline GCMapSetStorage* GCMapSet::get() const { return static_cast<GCMapSetStorage*>(storage.get()); }
inline List<GCMap> GCMapSet::_items() { return get()->_items; }
inline void GCMapSet::set__items(List<GCMap> _v) { get()->_items = _v; }
inline GCMap GCMapSet::Get(Int32 idx) { return get()->Get(idx); }
inline GCMap GCMapSetStorage::Get(Int32 idx) {
	return _items[idx];
}
inline void GCMapSet::Init(Int32 idx,Int32 capacity) { return get()->Init(idx, capacity); }
inline void GCMapSet::SetFrozen(Int32 idx,Boolean frozen) { return get()->SetFrozen(idx, frozen); }
inline void GCMapSetStorage::SetFrozen(Int32 idx,Boolean frozen) {
	GCMap item = _items[idx];
	item.Frozen = frozen;
	_items[idx] = item;
}
inline void GCMapSet::SetVmb(Int32 idx,VarMapBacking vmb) { return get()->SetVmb(idx, vmb); }
inline void GCMapSetStorage::SetVmb(Int32 idx,VarMapBacking vmb) {
	GCMap item = _items[idx];
	item._vmb = vmb;
	_items[idx] = item;
}

inline GCErrorSet::GCErrorSet(std::shared_ptr<GCErrorSetStorage> stor) : GCSetBase(stor) {}
inline GCErrorSetStorage* GCErrorSet::get() const { return static_cast<GCErrorSetStorage*>(storage.get()); }
inline List<GCError> GCErrorSet::_items() { return get()->_items; }
inline void GCErrorSet::set__items(List<GCError> _v) { get()->_items = _v; }
inline GCError GCErrorSet::Get(Int32 idx) { return get()->Get(idx); }
inline GCError GCErrorSetStorage::Get(Int32 idx) {
	return _items[idx];
}
inline void GCErrorSet::SetFields(Int32 idx,Value message,Value inner,Value stack,Value isa) { return get()->SetFields(idx, message, inner, stack, isa); }
inline void GCErrorSetStorage::SetFields(Int32 idx,Value message,Value inner,Value stack,Value isa) {
	GCError item = _items[idx];
	item.Message = message;
	item.Inner   = inner;
	item.Stack   = stack;
	item.Isa     = isa;
	_items[idx] = item;
}

inline GCFuncRefSet::GCFuncRefSet(std::shared_ptr<GCFuncRefSetStorage> stor) : GCSetBase(stor) {}
inline GCFuncRefSetStorage* GCFuncRefSet::get() const { return static_cast<GCFuncRefSetStorage*>(storage.get()); }
inline List<GCFunction> GCFuncRefSet::_items() { return get()->_items; }
inline void GCFuncRefSet::set__items(List<GCFunction> _v) { get()->_items = _v; }
inline GCFunction GCFuncRefSet::Get(Int32 idx) { return get()->Get(idx); }
inline GCFunction GCFuncRefSetStorage::Get(Int32 idx) {
	return _items[idx];
}
inline void GCFuncRefSet::SetFields(Int32 idx,Int32 funcIndex,Value outerVars) { return get()->SetFields(idx, funcIndex, outerVars); }
inline void GCFuncRefSetStorage::SetFields(Int32 idx,Int32 funcIndex,Value outerVars) {
	GCFunction item = _items[idx];
	item.FuncIndex = funcIndex;
	item.OuterVars = outerVars;
	_items[idx] = item;
}

} // end of namespace MiniScript
