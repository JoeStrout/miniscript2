// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: PerProgram.cs

#include "PerProgram.g.h"
#include "VM.g.h"

namespace MiniScript {

List<String> PerProgramStorage::_displayNames =  List<String>::New();
PerProgramStorage::PerProgramStorage(PerProgramBuilder builder,String displayName ) {
	_builder = builder;
	_index = _displayNames.Count();
	_displayNames.Add(displayName);
}
Value PerProgramStorage::Get(Context ctx) {
	return GetFor(ctx.vm.GetGlobals());
}
Value PerProgramStorage::Get() {
	VM vm = VM::ActiveVM();
	if (IsNull(vm)) return _builder();
	return GetFor(vm.GetGlobals());
}
Value PerProgramStorage::GetFor(Globals g) {
	if (IsNull(g)) return _builder();
	Value v = g.PerProgramValue(_index);
	if (v.IsNull()) {
		v = _builder();
		g.SetPerProgramValue(_index, v);
	}
	return v;
}
Value PerProgramStorage::Peek(Context ctx) {
	Globals g = ctx.vm.GetGlobals();
	if (IsNull(g)) return Value::Null;
	return g.PerProgramValue(_index);
}
String PerProgramStorage::DisplayName(Int32 index) {
	if (index < 0 || index >= _displayNames.Count()) return "";
	return _displayNames[index];
}

} // end of namespace MiniScript
