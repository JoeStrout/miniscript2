// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: FuncDef.cs

#include "FuncDef.g.h"
#include "StringUtils.g.h"
#include "IntrinsicAPI.g.h"
#include "gc.h"

namespace MiniScript {

void FuncDefStorage::AddInstruction(UInt32 instruction,Int32 lineNumber) {
	Code.Add(instruction);
	Int32 count = _lineRLELine.Count();
	if (count == 0 || _lineRLELine[count - 1] != lineNumber) {
		_lineRLEPC.Add(Code.Count() - 1);
		_lineRLELine.Add(lineNumber);
	}
}
Int32 FuncDefStorage::GetLineNumber(Int32 pc) {
	Int32 result = 0;
	for (Int32 i = 0; i < _lineRLEPC.Count(); i++) {
		if (_lineRLEPC[i] > pc) break;
		result = _lineRLELine[i];
	}
	return result;
}
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
