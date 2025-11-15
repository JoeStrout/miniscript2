// StringPool.h provides a string interning service, as well as a general
// StringStorage allocator, for the String class (i.e., host strings).  It
// is built on top of MemPool.

#pragma once
#include "MemPool.h"
#include "StringStorage.h"
#include <cstdint>

// This module is part of Layer 2B (Host Memory Management)
#define CORE_LAYER_2B

namespace StringPool {

struct HashEntry {
	uint32_t hash;	// hash value
	MemRef ssRef; 	// MemRef to StringStorage
	MemRef next;    // MemRef to next HashEntry in the bin
	
	const StringStorage* stringStorage() const { return (const StringStorage*)MemPoolManager::getPtr(ssRef); }
};

struct Pool {
	// Hash table heads are MemRefs to HashEntry (first in the linked list of entries for each bin)
	MemRef   hashTable[256];

	bool     initialized;
};

MemRef internOrAdoptString(uint8_t poolNum, StringStorage *ss);
MemRef internString(uint8_t poolNum, const char* cstr);
MemRef storeInPool(MemRef sRef, uint8_t poolNum, uint32_t hash);
inline const StringStorage* getStorage(MemRef ref) { return (const StringStorage*)MemPoolManager::getPtr(ref); }
inline const char* getCString(MemRef ref) { return getStorage(ref)->data; }

// Pool management
void clearPool(uint8_t poolNum);

StringStorage* defaultStringAllocator(const char* src, int lenB, uint32_t hash);  // ToDo: is this needed?

// Allocator function compatible with StringStorageAllocator typedef
// Uses pool 0 by default
void* poolAllocator(size_t size);

// Pool-specific allocator (can be used with std::bind or similar)
void* poolAllocatorForPool(size_t size, uint8_t poolNum);

// Inspection/debugging support
void dumpPoolState(uint8_t poolNum);
void dumpAllPoolState();

} // namespace StringPool

#ifdef __cplusplus
extern "C" {
#endif

// C-compatible allocator function for use in C code
void* stringpool_allocator(size_t size);

#ifdef __cplusplus
}
#endif
