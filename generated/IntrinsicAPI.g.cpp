// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IntrinsicAPI.cs

#include "IntrinsicAPI.g.h"

namespace MiniScript {

Context::Context(VM vm,List<Value> stack,Int32 baseIndex,Int32 argCount) {
	this->vm = vm;
	this->stack = stack;
	this->baseIndex = baseIndex;
	this->argCount = argCount;
}
Value Context::GetVar(String variableName) {
	return vm.LookupParamByName(variableName);
}

} // end of namespace MiniScript
