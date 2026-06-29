// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IntrinsicAPI.cs

#include "IntrinsicAPI.g.h"
#include "VM.g.h"
#include "Interpreter.g.h"

namespace MiniScript {

Value Context::GetVar(String variableName) {
	return vm.LookupParamByName(variableName);
}
Interpreter Context::GetInterpreter() {
	return vm.GetInterpreter();
}

IntrinsicResult::IntrinsicResult(Value result,Boolean done ) {
	this->result = result;
	this->done = done;
}
const IntrinsicResult IntrinsicResult::Null = IntrinsicResult(Value::Null);
const IntrinsicResult IntrinsicResult::EmptyString = IntrinsicResult(Value::emptyString);
const IntrinsicResult IntrinsicResult::Zero = IntrinsicResult(Value::zero);
const IntrinsicResult IntrinsicResult::One = IntrinsicResult(Value::one);

} // end of namespace MiniScript
