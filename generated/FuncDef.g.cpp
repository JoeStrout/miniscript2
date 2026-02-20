// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: FuncDef.cs

#include "FuncDef.g.h"
#include "gc.h"

namespace MiniScript {

FuncDefStorage::FuncDefStorage() {
}
void FuncDefStorage::ReserveRegister(Int32 registerNumber) {
	UInt16 impliedCount = (UInt16)(registerNumber + 1);
	if (MaxRegs < impliedCount) MaxRegs = impliedCount;
}
String FuncDefStorage::ToString() {
	String result = Name + "(";
	GC_PUSH_SCOPE();
	Value defaultVal; GC_PROTECT(&defaultVal);
	for (Int32 i = 0; i < ParamNames.Count(); i++) {
		if (i > 0) result += ", ";
		result += as_cstring(ParamNames[i]);
		defaultVal = ParamDefaults[i];
		if (!is_null(defaultVal)) {
			result += "=";
			result += as_cstring(value_repr(defaultVal));
		}
	}
	result += ")";
	GC_POP_SCOPE();
	return result;
}

} // end of namespace MiniScript
