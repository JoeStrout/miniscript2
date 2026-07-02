// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IntrinsicAPI.cs

#include "IntrinsicAPI.g.h"
#include "VM.g.h"
#include "Interpreter.g.h"
#include <unordered_map>
namespace MiniScript {
Value StaticMap(ValueDict& d) {
	// Key the cache on the address of the caller's ValueDict.  Because the
	// parameter is a non-const reference, a temporary won't bind, so the caller
	// must pass a stable lvalue (its static ValueDict); the address is valid even
	// when the dictionary is empty, avoiding the null-storage collision we would
	// get from keying on the underlying table pointer.
	static std::unordered_map<const void*, Value> cache;
	const void* key = &d;
	auto it = cache.find(key);
	if (it != cache.end()) return it->second;

	Value m = GCManager::NewMapFromDict(d);   // shares d's storage; no entry copy
	GCManager::AddRoot(m);                     // keep the map (and contents) alive
	cache[key] = m;
	return m;
}
}

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
