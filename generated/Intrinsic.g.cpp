// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: Intrinsic.cs

#include "Intrinsic.g.h"
#include "CoreIntrinsics.g.h"

namespace MiniScript {

	List<Intrinsic> IntrinsicStorage::_all;
	Boolean IntrinsicStorage::_initialized;
Intrinsic IntrinsicStorage::Create(String name) {
	Intrinsic result =  Intrinsic::New();
	result.set_Name(name);
	result.set__paramNames( List<String>::New());
	result.set__paramDefaults( List<Value>::New());
	_all.Add(result);
	return result;
}
void IntrinsicStorage::AddParam(String name) {
	_paramNames.Add(name);
	_paramDefaults.Add(make_null());
}
void IntrinsicStorage::AddParam(String name,Value defaultValue) {
	_paramNames.Add(name);
	_paramDefaults.Add(defaultValue);
}
Intrinsic IntrinsicStorage::GetByName(String name) {
	for (Int32 i = 0; i < _all.Count(); i++) {
		if (_all[i].Name() == name) return _all[i];
	}
	return nullptr;
}
Value IntrinsicStorage::GetFunc() {
	return make_funcref(_funcIndex, val_null);
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
void IntrinsicStorage::RegisterAll(List<FuncDef> functions,Dictionary<String, Value> intrinsics) {
	if (!_initialized) {
		CoreIntrinsics::Init();
		_initialized = Boolean(true);
	}
	intrinsics.Clear();
	for (Int32 i = 0; i < _all.Count(); i++) {
		Intrinsic intr = _all[i];
		FuncDef def = intr.BuildFuncDef();
		Int32 funcIndex = functions.Count();
		intr.set__funcIndex(funcIndex);
		functions.Add(def);
		intrinsics[intr.Name()] = make_funcref(funcIndex, val_null);
	}
}

} // end of namespace MiniScript
