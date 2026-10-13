// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: FuncDef.cs

#include "FuncDef.g.h"
#include "StringUtils.g.h"
#include "IntrinsicAPI.g.h"

namespace MiniScript {

Int32 FuncDefStorage::AddGlobalRef(Value name) {
	for (Int32 i = 0; i < GlobalNames.Count(); i++) {
		if (GlobalNames[i] == name) return i;
	}
	GlobalNames.Add(name);
	GlobalSlots.Add(-1);
	return GlobalNames.Count() - 1;
}
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
	List<String> parts =  List<String>::New(ParamNames.Count());
	Value defaultVal;
	for (Int32 i = 0; i < ParamNames.Count(); i++) {
		String part = ParamNames[i].AsCString();
		defaultVal = ParamDefaults[i];
		if (!defaultVal.IsNull()) part = part + "=" + defaultVal.Repr(nullptr).AsCString();
		parts.Add(part);
	}
	return Name + "(" + String::Join(", ", parts) + ")";
}

} // end of namespace MiniScript
