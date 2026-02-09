// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: ErrorPool.cs

#include "ErrorPool.g.h"

namespace MiniScript {


ErrorPool ErrorPool::Create() {
	ErrorPool pool = ErrorPool();
	pool._errors =  List<String>::New();
	return pool;
}
void ErrorPool::EnsureList() {
	if (IsNull(_errors)) _errors =  List<String>::New();
}
void ErrorPool::Add(String message) {
	EnsureList();
	_errors.Add(message);
}
Boolean ErrorPool::HasError() {
	return !IsNull(_errors) && _errors.Count() > 0;
}
String ErrorPool::TopError() {
	if (IsNull(_errors) || _errors.Count() == 0) return nullptr;
	return _errors[0];
}
List<String> ErrorPool::GetErrors() {
	EnsureList();
	return _errors;
}
void ErrorPool::Clear() {
	if (!IsNull(_errors)) _errors.Clear();
	else _errors =  List<String>::New();
}


} // end of namespace MiniScript
