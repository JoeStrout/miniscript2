// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: GCManager.cs

#include "GCManager.g.h"
#include "value.h"

namespace MiniScript {

const Int32 GCManager::StringSet = 0;
const Int32 GCManager::ListSet = 1;
const Int32 GCManager::MapSet = 2;
const Int32 GCManager::ErrorSet = 3;
const Int32 GCManager::FunctionSet = 4;
GCStringSet GCManager::Strings = nullptr;
GCListSet GCManager::Lists = nullptr;
GCMapSet GCManager::Maps = nullptr;
GCErrorSet GCManager::Errors = nullptr;
GCFuncRefSet GCManager::Functions = nullptr;
List<Value> GCManager::_roots = nullptr;
List<MarkCallback> GCManager::_markCallbackFns = nullptr;
List<object> GCManager::_markCallbackData = nullptr;
void GCManager::Init() {
	if (!IsNull(_roots)) return;	// already initialized
	Strings  =  GCStringSet::New();
	Lists    =  GCListSet::New();
	Maps     =  GCMapSet::New();
	Errors   =  GCErrorSet::New();
	Functions =  GCFuncRefSet::New();
	_roots   =  List<Value>::New();
	_markCallbackFns  =  List<MarkCallback>::New();
	_markCallbackData =  List<object>::New();
}
Value GCManager::NewString(String s) {
	Int32 idx = Strings.AllocItem();
	Strings.SetData(idx, s);
	return make_gc(StringSet, idx);
}
Value GCManager::NewList(Int32 capacity ) {
	Int32 idx = Lists.AllocItem();
	Lists.Init(idx, capacity);
	return make_gc(ListSet, idx);
}
Value GCManager::NewMap(Int32 capacity ) {
	Int32 idx = Maps.AllocItem();
	Maps.Init(idx, capacity);
	return make_gc(MapSet, idx);
}
Value GCManager::NewError(Value message,Value inner,Value stack,Value isa) {
	Int32 idx = Errors.AllocItem();
	Errors.SetFields(idx, message, inner, stack, isa);
	return make_gc(ErrorSet, idx);
}
Value GCManager::NewFuncRef(Int32 funcIndex,Value outerVars) {
	Int32 idx = Functions.AllocItem();
	Functions.SetFields(idx, funcIndex, outerVars);
	return make_gc(FunctionSet, idx);
}
void GCManager::Retain(Value v) {
	if (!is_gc_object(v)) return;
	DispatchMark(value_gc_set_index(v), value_item_index(v));
}
void GCManager::RetainValue(Value v) {
	if (!is_gc_object(v)) return;
	DispatchMark(value_gc_set_index(v), value_item_index(v));
}
void GCManager::RegisterMarkCallback(MarkCallback fn,object userData) {
	_markCallbackFns.Add(fn);
	_markCallbackData.Add(userData);
}
void GCManager::UnregisterMarkCallback(MarkCallback fn,object userData) {
	for (Int32 i = 0; i < _markCallbackFns.Count(); i++) {
		if (_markCallbackFns[i] == fn && _markCallbackData[i] == userData) {
			_markCallbackFns.RemoveAt(i);
			_markCallbackData.RemoveAt(i);
			return;
		}
	}
}
void GCManager::DispatchMark(Int32 setIdx,Int32 itemIdx) {
	switch (setIdx) {
		case StringSet:  Strings.Mark(itemIdx);  break;
		case ListSet:    Lists.Mark(itemIdx);    break;
		case MapSet:     Maps.Mark(itemIdx);     break;
		case ErrorSet:   Errors.Mark(itemIdx);   break;
		case FunctionSet: Functions.Mark(itemIdx); break;
	}
}
void GCManager::CollectGarbage() {
	// 1. Clear all mark bits.
	Strings.PrepareForGC();
	Lists.PrepareForGC();
	Maps.PrepareForGC();
	Errors.PrepareForGC();
	Functions.PrepareForGC();

	// 2. Mark from explicit roots.
	for (Int32 i = 0; i < _roots.Count(); i++) Mark(_roots[i]);

	// 2b. Run mark callbacks so VMs (and other providers) can mark their
	// current roots without having to add/remove them on every change.
	for (Int32 i = 0; i < _markCallbackFns.Count(); i++) {
		_markCallbackFns[i](_markCallbackData[i]);
	}

	// 3. Mark retained items (and their children).
	Strings.MarkRetained();
	Lists.MarkRetained();
	Maps.MarkRetained();
	Errors.MarkRetained();
	Functions.MarkRetained();

	// 4. Sweep: free everything still unmarked.
	Strings.Sweep();
	Lists.Sweep();
	Maps.Sweep();
	Errors.Sweep();
	Functions.Sweep();
}
GCFunction GCManager::GetFuncRef(Value v) {
	return Functions.Get(value_item_index(v));
}

} // end of namespace MiniScript
