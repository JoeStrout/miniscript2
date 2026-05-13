// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: GCSet.cs

#include "GCSet.g.h"

namespace MiniScript {

Int32 GCSetBaseStorage::AllocItem() {
	Int32 idx;
	if (_free.Count() > 0) {
		idx = _free[_free.Count() - 1];
		_free.RemoveAt(_free.Count() - 1);
		_inUse[idx]        = Boolean(true);
		_marked[idx]       = Boolean(false);
		_retainCounts[idx] = 0;
	} else {
		idx = _inUse.Count();
		_inUse.Add(Boolean(true));
		_marked.Add(Boolean(false));
		_retainCounts.Add(0);
		AppendItem();
	}
	return idx;
}
void GCSetBaseStorage::Retain(Int32 idx) {
	_retainCounts[idx]++;
}
void GCSetBaseStorage::Release(Int32 idx) {
	_retainCounts[idx]--;
}
void GCSetBaseStorage::PrepareForGC() {
	for (Int32 i = 0; i < _marked.Count(); i++) _marked[i] = Boolean(false);
}
void GCSetBaseStorage::Mark(Int32 idx) {
	if (_marked[idx]) return;
	_marked[idx] = Boolean(true);
	CallMarkChildren(idx);
}
void GCSetBaseStorage::MarkRetained() {
	for (Int32 i = 0; i < _inUse.Count(); i++) {
		if (_inUse[i] && _retainCounts[i] > 0) Mark(i);
	}
}
void GCSetBaseStorage::Sweep() {
	for (Int32 i = 0; i < _inUse.Count(); i++) {
		if (_inUse[i] && !_marked[i] && _retainCounts[i] == 0) {
			CallOnSweep(i);
			_inUse[i]        = Boolean(false);
			_retainCounts[i] = 0;
			_free.Add(i);
		}
	}
}
Int32 GCSetBaseStorage::LiveCount() {
	Int32 n = 0;
	for (Int32 i = 0; i < _inUse.Count(); i++) {
		if (_inUse[i]) n++;
	}
	return n;
}

GCStringSetStorage::GCStringSetStorage(Int32 initialCapacity ) {
	_items =  List<GCString>::New(initialCapacity);
}
void GCStringSetStorage::CallMarkChildren(Int32 idx) {
	_items[idx].MarkChildren();
}
void GCStringSetStorage::CallOnSweep(Int32 idx) {
	GCString item = _items[idx];
	item.OnSweep();
	_items[idx] = item;
}
void GCStringSetStorage::AppendItem() {
	_items.Add(GCString());
}

GCListSetStorage::GCListSetStorage(Int32 initialCapacity ) {
	_items =  List<GCList>::New(initialCapacity);
}
void GCListSetStorage::CallMarkChildren(Int32 idx) {
	_items[idx].MarkChildren();
}
void GCListSetStorage::CallOnSweep(Int32 idx) {
	GCList item = _items[idx];
	item.OnSweep();
	_items[idx] = item;
}
void GCListSetStorage::AppendItem() {
	_items.Add(GCList());
}
void GCListSetStorage::Init(Int32 idx,Int32 capacity) {
	GCList item = _items[idx];
	item.Init(capacity);
	_items[idx] = item;
}

GCMapSetStorage::GCMapSetStorage(Int32 initialCapacity ) {
	_items =  List<GCMap>::New(initialCapacity);
}
void GCMapSetStorage::CallMarkChildren(Int32 idx) {
	_items[idx].MarkChildren();
}
void GCMapSetStorage::CallOnSweep(Int32 idx) {
	GCMap item = _items[idx];
	item.OnSweep();
	_items[idx] = item;
}
void GCMapSetStorage::AppendItem() {
	_items.Add(GCMap());
}
void GCMapSetStorage::Init(Int32 idx,Int32 capacity) {
	GCMap item = _items[idx];
	item.Init(capacity);
	_items[idx] = item;
}

GCErrorSetStorage::GCErrorSetStorage(Int32 initialCapacity ) {
	_items =  List<GCError>::New(initialCapacity);
}
void GCErrorSetStorage::CallMarkChildren(Int32 idx) {
	_items[idx].MarkChildren();
}
void GCErrorSetStorage::CallOnSweep(Int32 idx) {
	GCError item = _items[idx];
	item.OnSweep();
	_items[idx] = item;
}
void GCErrorSetStorage::AppendItem() {
	_items.Add(GCError());
}

GCFuncRefSetStorage::GCFuncRefSetStorage(Int32 initialCapacity ) {
	_items =  List<GCFunction>::New(initialCapacity);
}
void GCFuncRefSetStorage::CallMarkChildren(Int32 idx) {
	_items[idx].MarkChildren();
}
void GCFuncRefSetStorage::CallOnSweep(Int32 idx) {
	GCFunction item = _items[idx];
	item.OnSweep();
	_items[idx] = item;
}
void GCFuncRefSetStorage::AppendItem() {
	_items.Add(GCFunction());
}

} // end of namespace MiniScript
