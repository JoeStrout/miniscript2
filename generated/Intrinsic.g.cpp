// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Intrinsic.cs

#include "Intrinsic.g.h"
#include "CoreIntrinsics.g.h"

namespace MiniScript {

List<Intrinsic> IntrinsicStorage::_all =  List<Intrinsic>::New();
Dictionary<String, Intrinsic> IntrinsicStorage::_byName =  Dictionary<String, Intrinsic>::New();
Boolean IntrinsicStorage::_initialized = Boolean(false);
Boolean IntrinsicStorage::_markCallbackRegistered = Boolean(false);
List<Value> IntrinsicStorage::_shortNameKeys =  List<Value>::New();
List<String> IntrinsicStorage::_shortNameVals =  List<String>::New();
void IntrinsicStorage::MarkRoots(object user_data) {
	for (Int32 i = 0; i < _all.Count(); i++) {
		List<Value> defaults = _all[i]._paramDefaults();
		for (Int32 j = 0; j < defaults.Count(); j++) GCManager::Mark(defaults[j]);
	}
	for (Int32 i = 0; i < _shortNameKeys.Count(); i++) GCManager::Mark(_shortNameKeys[i]);
}
void IntrinsicStorage::EnsureMarkCallback() {
	if (_markCallbackRegistered) return;
	_markCallbackRegistered = Boolean(true);
	GCManager::RegisterMarkCallback(Intrinsic::MarkRoots, nullptr);
}
void IntrinsicStorage::AddShortName(Value v,String name) {
	EnsureMarkCallback();
	_shortNameKeys.Add(v);
	_shortNameVals.Add(name);
}
void IntrinsicStorage::ClearShortNames() {
	_shortNameKeys.Clear();
	_shortNameVals.Clear();
}
String IntrinsicStorage::GetShortName(Value v) {
	for (Int32 i = 0; i < _shortNameKeys.Count(); i++) {
		if (_shortNameKeys[i].RefEquals(v)) return _shortNameVals[i];
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
	EnsureMarkCallback();
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
	_paramDefaults.Add(Value::Null);
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
		_funcRef = Value::make_funcref(_funcDef, Value::Null);
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
		def.ParamNames().Add(Value::make_string(_paramNames[i]));
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
	// Note: do NOT invalidate the cached type maps here.  They are GC roots
	// (CoreIntrinsics.MarkRoots), so they are never swept out from under us,
	// and they are built lazily -- so on the first call there is nothing to
	// rebuild anyway.  Doing it per VM would discard whatever a script has
	// added to `list`, `string` or `map`, process-wide, since those maps are
	// shared by every VM; it would also clear short names a host registered
	// during its own setup.
}

} // end of namespace MiniScript
