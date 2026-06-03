// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Intrinsic.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// Intrinsic.cs - The Intrinsic class: a built-in function registered as a callable FuncRef.
// Each intrinsic is defined with a builder-style API:
//   f = Intrinsic.Create("name");
//   f.AddParam("paramName", defaultValue);
//   f.Code = (stk, bi, ac) => { ... };

#include "value.h"
#include "FuncDef.g.h"

namespace MiniScript {

// DECLARATIONS

class IntrinsicStorage : public std::enable_shared_from_this<IntrinsicStorage> {
	friend struct Intrinsic;
	public: String Name;
	public: NativeCallbackDelegate Code;
	private: List<String> _paramNames;
	private: List<Value> _paramDefaults;
	private: FuncDef _funcDef = null;
	private: Value _funcRef = val_null;
	private: static List<Intrinsic> _all;
	private: static Dictionary<String, Intrinsic> _byName;
	private: static Boolean _initialized;
	private: static List<Value> _shortNameKeys;
	private: static List<String> _shortNameVals;

	// Short-name registry: maps known Values (e.g. type maps) to display names.

	public: static void AddShortName(Value v, String name);

	public: static void ClearShortNames();

	public: static String GetShortName(Value v);
	public: IntrinsicStorage() {}

	// Return the number of intrinsics (initializing them if needed).
	public: static Int32 Count();

	public: static Intrinsic Create(String name);

	public: void AddParam(String name);

	public: void AddParam(String name, Value defaultValue);

	public: static Intrinsic GetByName(String name);

	public: static Intrinsic GetByIndex(Int32 i);

	// Build (once) this intrinsic's FuncDef and a stable funcref Value.
	// The funcref is added as a permanent GC root: intrinsics live for the
	// lifetime of the process and are shared across VMs and resets.
	private: void EnsureBuilt();

	public: Value GetFunc();

	// Build a FuncDef from this intrinsic's definition.
	public: FuncDef BuildFuncDef();

	// Populate the VM's intrinsics name->funcref table.  Intrinsic FuncDefs and
	// their funcref Values are built once (lazily) and shared across all VMs.
	public: static void RegisterAll(Dictionary<String, Value> intrinsics);
}; // end of class IntrinsicStorage

struct Intrinsic {
	friend class IntrinsicStorage;
	protected: std::shared_ptr<IntrinsicStorage> storage;
  public:
	Intrinsic(std::shared_ptr<IntrinsicStorage> stor) : storage(stor) {}
	Intrinsic() : storage(nullptr) {}
	Intrinsic(std::nullptr_t) : storage(nullptr) {}
	friend bool IsNull(const Intrinsic& inst) { return inst.storage == nullptr; }
	private: IntrinsicStorage* get() const;

	public: String Name();
	public: void set_Name(String _v);
	public: NativeCallbackDelegate Code();
	public: void set_Code(NativeCallbackDelegate _v);
	private: List<String> _paramNames();
	private: void set__paramNames(List<String> _v);
	private: List<Value> _paramDefaults();
	private: void set__paramDefaults(List<Value> _v);
	private: FuncDef _funcDef();
	private: void set__funcDef(FuncDef _v);
	private: Value _funcRef();
	private: void set__funcRef(Value _v);
	private: List<Intrinsic> _all();
	private: void set__all(List<Intrinsic> _v);
	private: Dictionary<String, Intrinsic> _byName();
	private: void set__byName(Dictionary<String, Intrinsic> _v);
	private: Boolean _initialized();
	private: void set__initialized(Boolean _v);
	private: List<Value> _shortNameKeys();
	private: void set__shortNameKeys(List<Value> _v);
	private: List<String> _shortNameVals();
	private: void set__shortNameVals(List<String> _v);

	// Short-name registry: maps known Values (e.g. type maps) to display names.

	public: static void AddShortName(Value v, String name) { return IntrinsicStorage::AddShortName(v, name); }

	public: static void ClearShortNames() { return IntrinsicStorage::ClearShortNames(); }

	public: static String GetShortName(Value v) { return IntrinsicStorage::GetShortName(v); }
	public: static Intrinsic New() {
		return Intrinsic(std::make_shared<IntrinsicStorage>());
	}

	// Return the number of intrinsics (initializing them if needed).
	public: static Int32 Count() { return IntrinsicStorage::Count(); }

	public: static Intrinsic Create(String name) { return IntrinsicStorage::Create(name); }

	public: inline void AddParam(String name);

	public: inline void AddParam(String name, Value defaultValue);

	public: static Intrinsic GetByName(String name) { return IntrinsicStorage::GetByName(name); }

	public: static Intrinsic GetByIndex(Int32 i) { return IntrinsicStorage::GetByIndex(i); }

	// Build (once) this intrinsic's FuncDef and a stable funcref Value.
	// The funcref is added as a permanent GC root: intrinsics live for the
	// lifetime of the process and are shared across VMs and resets.
	private: inline void EnsureBuilt();

	public: inline Value GetFunc();

	// Build a FuncDef from this intrinsic's definition.
	public: inline FuncDef BuildFuncDef();

	// Populate the VM's intrinsics name->funcref table.  Intrinsic FuncDefs and
	// their funcref Values are built once (lazily) and shared across all VMs.
	public: static void RegisterAll(Dictionary<String, Value> intrinsics) { return IntrinsicStorage::RegisterAll(intrinsics); }
}; // end of struct Intrinsic

// INLINE METHODS

inline IntrinsicStorage* Intrinsic::get() const { return static_cast<IntrinsicStorage*>(storage.get()); }
inline String Intrinsic::Name() { return get()->Name; }
inline void Intrinsic::set_Name(String _v) { get()->Name = _v; }
inline NativeCallbackDelegate Intrinsic::Code() { return get()->Code; }
inline void Intrinsic::set_Code(NativeCallbackDelegate _v) { get()->Code = _v; }
inline List<String> Intrinsic::_paramNames() { return get()->_paramNames; }
inline void Intrinsic::set__paramNames(List<String> _v) { get()->_paramNames = _v; }
inline List<Value> Intrinsic::_paramDefaults() { return get()->_paramDefaults; }
inline void Intrinsic::set__paramDefaults(List<Value> _v) { get()->_paramDefaults = _v; }
inline FuncDef Intrinsic::_funcDef() { return get()->_funcDef; }
inline void Intrinsic::set__funcDef(FuncDef _v) { get()->_funcDef = _v; }
inline Value Intrinsic::_funcRef() { return get()->_funcRef; }
inline void Intrinsic::set__funcRef(Value _v) { get()->_funcRef = _v; }
inline List<Intrinsic> Intrinsic::_all() { return get()->_all; }
inline void Intrinsic::set__all(List<Intrinsic> _v) { get()->_all = _v; }
inline Dictionary<String, Intrinsic> Intrinsic::_byName() { return get()->_byName; }
inline void Intrinsic::set__byName(Dictionary<String, Intrinsic> _v) { get()->_byName = _v; }
inline Boolean Intrinsic::_initialized() { return get()->_initialized; }
inline void Intrinsic::set__initialized(Boolean _v) { get()->_initialized = _v; }
inline List<Value> Intrinsic::_shortNameKeys() { return get()->_shortNameKeys; }
inline void Intrinsic::set__shortNameKeys(List<Value> _v) { get()->_shortNameKeys = _v; }
inline List<String> Intrinsic::_shortNameVals() { return get()->_shortNameVals; }
inline void Intrinsic::set__shortNameVals(List<String> _v) { get()->_shortNameVals = _v; }
inline void Intrinsic::AddParam(String name) { return get()->AddParam(name); }
inline void Intrinsic::AddParam(String name,Value defaultValue) { return get()->AddParam(name, defaultValue); }
inline void Intrinsic::EnsureBuilt() { return get()->EnsureBuilt(); }
inline Value Intrinsic::GetFunc() { return get()->GetFunc(); }
inline FuncDef Intrinsic::BuildFuncDef() { return get()->BuildFuncDef(); }

} // end of namespace MiniScript
