// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: GCManager.cs

#include "GCManager.g.h"
#include "value.h"

namespace MiniScript {

const Int32 GCManager::BigStringSet = 0;
const Int32 GCManager::ListSet = 1;
const Int32 GCManager::MapSet = 2;
const Int32 GCManager::ErrorSet = 3;
const Int32 GCManager::FunctionSet = 4;
const Int32 GCManager::InternedStringSet = 5;
const Int32 GCManager::InternThreshold = 128;
GCStringSet GCManager::BigStrings = nullptr;
GCStringSet GCManager::InternedStrings = nullptr;
GCListSet GCManager::Lists = nullptr;
GCMapSet GCManager::Maps = nullptr;
GCErrorSet GCManager::Errors = nullptr;
GCFuncRefSet GCManager::Functions = nullptr;
Dictionary<String, Int32> GCManager::_internTable = nullptr;
Boolean GCManager::_fullCollection = Boolean(false);
List<Value> GCManager::_roots = nullptr;
List<MarkCallback> GCManager::_markCallbackFns = nullptr;
List<object> GCManager::_markCallbackData = nullptr;
void GCManager::Init() {
	if (!IsNull(_roots)) return;	// already initialized
	BigStrings      =  GCStringSet::New();
	InternedStrings =  GCStringSet::New();
	Lists           =  GCListSet::New();
	Maps            =  GCMapSet::New();
	Errors          =  GCErrorSet::New();
	Functions       =  GCFuncRefSet::New();

	  Dictionary<String, Int32>();
	_roots          =  List<Value>::New();
	_markCallbackFns  =  List<MarkCallback>::New();
	_markCallbackData =  List<object>::New();
}
Value GCManager::NewString(String s) {
	Int32 idx = BigStrings.AllocItem();
	BigStrings.SetData(idx, s);
	return make_gc(BigStringSet, idx);
}
Value GCManager::InternString(String s) {
	Int32 idx;
	if (_internTable.TryGetValue(s, &idx)) {
		return make_gc(InternedStringSet, idx);
	}
	idx = InternedStrings.AllocItem();
	InternedStrings.SetData(idx, s);
	_internTable[s] = idx;
	return make_gc(InternedStringSet, idx);
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
		case BigStringSet:  BigStrings.Mark(itemIdx);  break;
		case ListSet:    Lists.Mark(itemIdx);    break;
		case MapSet:     Maps.Mark(itemIdx);     break;
		case ErrorSet:   Errors.Mark(itemIdx);   break;
		case FunctionSet: Functions.Mark(itemIdx); break;
		case InternedStringSet:
			// Skip during normal GC; interned strings are semi-immortal.
			if (_fullCollection) InternedStrings.Mark(itemIdx);
			break;
	}
}
void GCManager::FullCollectGarbage() {
	CollectGarbageInternal(Boolean(true));
}
void GCManager::CollectGarbage() {
	CollectGarbageInternal(Boolean(false));
}
void GCManager::CollectGarbageInternal(Boolean includeInterned) {
	_fullCollection = includeInterned;

	// 1. Clear all mark bits.
	BigStrings.PrepareForGC();
	Lists.PrepareForGC();
	Maps.PrepareForGC();
	Errors.PrepareForGC();
	Functions.PrepareForGC();
	if (includeInterned) InternedStrings.PrepareForGC();

	// 2. Mark from explicit roots.
	for (Int32 i = 0; i < _roots.Count(); i++) Mark(_roots[i]);

	// 2b. Run mark callbacks so VMs (and other providers) can mark their
	// current roots without having to add/remove them on every change.
	for (Int32 i = 0; i < _markCallbackFns.Count(); i++) {
		_markCallbackFns[i](_markCallbackData[i]);
	}

	// 3. Mark retained items (and their children).
	BigStrings.MarkRetained();
	Lists.MarkRetained();
	Maps.MarkRetained();
	Errors.MarkRetained();
	Functions.MarkRetained();
	if (includeInterned) InternedStrings.MarkRetained();

	// 4. Sweep: free everything still unmarked.
	BigStrings.Sweep();
	Lists.Sweep();
	Maps.Sweep();
	Errors.Sweep();
	Functions.Sweep();

	// 5. Full-GC only: remove dead intern-table entries, then sweep.
	// The table is keyed by string content, so we must purge its
	// entries before InternedStrings.Sweep() clears the .Data fields.
	if (includeInterned) SweepInternTable();
}
void GCManager::SweepInternTable() {
	List<String> dead =  List<String>::New();
	for (String key : _internTable.Keys()) {
		Int32 slot = _internTable[key];
		if (!InternedStrings.IsLiveSlot(slot)) dead.Add(key);
	}
	for (Int32 i = 0; i < dead.Count(); i++) _internTable.Remove(dead[i]);
	InternedStrings.Sweep();
}
GCString GCManager::GetString(Value v) {
	if (value_gc_set_index(v) == InternedStringSet) {
		return InternedStrings.Get(value_item_index(v));
	}
	return BigStrings.Get(value_item_index(v));
}
GCList GCManager::GetList(Value v) {
	return Lists.Get(value_item_index(v));
}
GCMap GCManager::GetMap(Value v) {
	return Maps.Get(value_item_index(v));
}
GCError GCManager::GetError(Value v) {
	return Errors.Get(value_item_index(v));
}
GCFunction GCManager::GetFuncRef(Value v) {
	return Functions.Get(value_item_index(v));
}

} // end of namespace MiniScript
