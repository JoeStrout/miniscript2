// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Intrinsic.cs

#include "Intrinsic.g.h"
#include "CoreIntrinsics.g.h"

namespace MiniScript {

	List<Intrinsic> IntrinsicStorage::_all;
	Dictionary<String, Intrinsic> IntrinsicStorage::_byName;
	Boolean IntrinsicStorage::_initialized;
	List<Value> IntrinsicStorage::_shortNameKeys;
	List<String> IntrinsicStorage::_shortNameVals;
void IntrinsicStorage::AddShortName(Value v,String name) {
	_shortNameKeys.Add(v);
	_shortNameVals.Add(name);
}
void IntrinsicStorage::ClearShortNames() {
	_shortNameKeys.Clear();
	_shortNameVals.Clear();
}
String IntrinsicStorage::GetShortName(Value v) {
	for (Int32 i = 0; i < _shortNameKeys.Count(); i++) {
		if (value_identical(_shortNameKeys[i], v)) return _shortNameVals[i];
	}
	return nullptr;
}
Int32 IntrinsicStorage::Count() {
	if (!_initialized) {
		CoreIntrinsics::Init();
		_initialized = Boolean(true);
	}
	return _all.Count();
}
Intrinsic IntrinsicStorage::Create(String name) {
	Intrinsic result =  Intrinsic::New();
	result.set_Name(name);
	result.set__paramNames( List<String>::New());
	result.set__paramDefaults( List<Value>::New());
	_all.Add(result);
	_byName[name] = result;
	return result;
}
void IntrinsicStorage::AddParam(String name) {
	_paramNames.Add(name);
	_paramDefaults.Add(val_null);
}
void IntrinsicStorage::AddParam(String name,Value defaultValue) {
	_paramNames.Add(name);
	_paramDefaults.Add(defaultValue);
}
Intrinsic IntrinsicStorage::GetByName(String name) {
	Intrinsic result;
	if (_byName.TryGetValue(name, &result)) return result;
	return nullptr;
}
Intrinsic IntrinsicStorage::GetByIndex(Int32 i) {
	return _all[i];
}
void IntrinsicStorage::EnsureBuilt() {
	if (IsNull(_funcDef)) {
		_funcDef = BuildFuncDef();
		_funcRef = make_funcref(_funcDef, val_null);
		GCManager::AddRoot(_funcRef);
	}
}
Value IntrinsicStorage::GetFunc() {
	EnsureBuilt();
	return _funcRef;
}
FuncDef IntrinsicStorage::BuildFuncDef() {
	FuncDef def =  FuncDef::New();
	def.set_Name(Name);
	for (Int32 i = 0; i < _paramNames.Count(); i++) {
		def.ParamNames().Add(make_string(_paramNames[i]));
		def.ParamDefaults().Add(_paramDefaults[i]);
	}
	def.set_MaxRegs((UInt16)(_paramNames.Count() + 1)); // r0 + params
	def.set_NativeCallback(Code);
	return def;
}
void IntrinsicStorage::RegisterAll(Dictionary<String, Value> intrinsics) {
	if (!_initialized) {
		CoreIntrinsics::Init();
		_initialized = Boolean(true);
	}
	intrinsics.Clear();
	for (Int32 i = 0; i < _all.Count(); i++) {
		Intrinsic intr = _all[i];
		intr.EnsureBuilt();
		intrinsics[intr.Name()] = intr._funcRef();
	}
	// Rebuild cached type maps (they are GC objects that may have been swept).
	CoreIntrinsics::InvalidateTypeMaps();
}

} // end of namespace MiniScript
