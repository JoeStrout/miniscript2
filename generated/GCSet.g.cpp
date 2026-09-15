// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: GCSet.cs

#include "GCSet.g.h"

namespace MiniScript {

const Int32 GCSetBaseStorage::TrimCapacityMinSlots = 1024;
Int32 GCSetBaseStorage::AllocItem() {
	Int32 idx;
	_liveCount++;
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
	Int32 count = _inUse.Count();
	Int32 lastInUse = -1;
	for (Int32 i = 0; i < count; i++) {
		if (!_inUse[i]) continue;
		if (!_marked[i] && _retainCounts[i] == 0) {
			CallOnSweep(i);
			_inUse[i]        = Boolean(false);
			_retainCounts[i] = 0;
			_liveCount--;
		} else {
			lastInUse = i;
		}
	}

	// Truncating is cheap, since the removed slots are already empty.
	// Returning their memory means reallocating, so do that only when the
	// table has shrunk a lot; otherwise a heap that swings around a steady
	// size would reallocate on every cycle.
	Int32 newCount = lastInUse + 1;
	Boolean trimCapacity = Boolean(false);
	if (newCount < count) {
		Int32 removed = count - newCount;
		trimCapacity = (removed >= TrimCapacityMinSlots && newCount < count / 4);
		_inUse.RemoveRange(newCount, removed);
		_marked.RemoveRange(newCount, removed);
		_retainCounts.RemoveRange(newCount, removed);
		TruncateItems(newCount, trimCapacity);
		if (trimCapacity) {
			_inUse.TrimExcess();
			_marked.TrimExcess();
			_retainCounts.TrimExcess();
		}
	}

	// Rebuild the free list highest index first, so that AllocItem (which
	// pops from the end) reuses the lowest free slot.
	_free.Clear();
	for (Int32 i = newCount - 1; i >= 0; i--) {
		if (!_inUse[i]) _free.Add(i);
	}
	if (trimCapacity) _free.TrimExcess();
}
Boolean GCSetBaseStorage::IsLiveSlot(Int32 idx) {
	return _inUse[idx] && (_marked[idx] || _retainCounts[idx] > 0);
}
Int32 GCSetBaseStorage::LiveCount() {
	return _liveCount;
}
Int32 GCSetBaseStorage::SlotCount() {
	return _inUse.Count();
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
void GCStringSetStorage::TruncateItems(Int32 newCount,Boolean trimCapacity) {
	_items.RemoveRange(newCount, _items.Count() - newCount);
	if (trimCapacity) _items.TrimExcess();
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
void GCListSetStorage::TruncateItems(Int32 newCount,Boolean trimCapacity) {
	_items.RemoveRange(newCount, _items.Count() - newCount);
	if (trimCapacity) _items.TrimExcess();
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
void GCMapSetStorage::TruncateItems(Int32 newCount,Boolean trimCapacity) {
	_items.RemoveRange(newCount, _items.Count() - newCount);
	if (trimCapacity) _items.TrimExcess();
}
void GCMapSetStorage::Init(Int32 idx,Int32 capacity) {
	GCMap item = _items[idx];
	item.Init(capacity);
	_items[idx] = item;
}
Boolean GCMapSetStorage::Remove(Int32 idx,Value key) {
	GCMap item = _items[idx];
	Boolean removed = item.Remove(key);
	_items[idx] = item;
	return removed;
}
void GCMapSetStorage::InitAsGlobals(Int32 idx,Globals g) {
	GCMap item = _items[idx];
	item.InitAsGlobals(g);
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
void GCErrorSetStorage::TruncateItems(Int32 newCount,Boolean trimCapacity) {
	_items.RemoveRange(newCount, _items.Count() - newCount);
	if (trimCapacity) _items.TrimExcess();
}

GCHandleSetStorage::GCHandleSetStorage(Int32 initialCapacity ) {
	_items =  List<GCHandle>::New(initialCapacity);
}
void GCHandleSetStorage::CallMarkChildren(Int32 idx) {
	_items[idx].MarkChildren();
}
void GCHandleSetStorage::CallOnSweep(Int32 idx) {
	GCHandle item = _items[idx];
	item.OnSweep();
	_items[idx] = item;
}
void GCHandleSetStorage::AppendItem() {
	_items.Add(GCHandle());
}
void GCHandleSetStorage::TruncateItems(Int32 newCount,Boolean trimCapacity) {
	_items.RemoveRange(newCount, _items.Count() - newCount);
	if (trimCapacity) _items.TrimExcess();
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
void GCFuncRefSetStorage::TruncateItems(Int32 newCount,Boolean trimCapacity) {
	_items.RemoveRange(newCount, _items.Count() - newCount);
	if (trimCapacity) _items.TrimExcess();
}

} // end of namespace MiniScript
