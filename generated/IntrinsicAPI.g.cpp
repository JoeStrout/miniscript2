// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IntrinsicAPI.cs

#include "IntrinsicAPI.g.h"
#include "VM.g.h"

namespace MiniScript {

Value Context::GetVar(String variableName) {
	return vm.LookupParamByName(variableName);
}

} // end of namespace MiniScript
