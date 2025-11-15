#include "MemPool.h"
#include <cstring>
#include <cstdio>

#include "layer_defs.h"
#if LAYER_2B_HIGHER
#error "MemPool.cpp (Layer 2B) cannot depend on higher layers (3B, 4)"
#endif
#if LAYER_2B_ASIDE
#error "MemPool.cpp (Layer 2B - host) cannot depend on A-side layers (2A, 3A)"
#endif

#if MEM_DEBUG
#include <cstdio>

// Memory debugging utilities
static void debugFillMemory(void* ptr, size_t size) {
    if (!ptr || size == 0) return;
    
	return;  // HACK!
	
    // Fill with 0xDEADBEEF pattern
    uint32_t* words = (uint32_t*)ptr;
    size_t wordCount = size / sizeof(uint32_t);
    
    for (size_t i = 0; i < wordCount; i++) {
        words[i] = 0xDEADBEEF;
    }
    
    // Fill any remaining bytes
    uint8_t* bytes = (uint8_t*)ptr + (wordCount * sizeof(uint32_t));
    size_t remainingBytes = size % sizeof(uint32_t);
    for (size_t i = 0; i < remainingBytes; i++) {
        bytes[i] = 0xEF; // Use last byte of 0xDEADBEEF
    }
}

static bool debugValidateMemory(const void* ptr, size_t size) {
    if (!ptr || size == 0) return true;
	return true;  // HACK!!!
	
    // Check if memory contains the 0xDEADBEEF pattern (indicating use-after-free)
    const uint32_t* words = (const uint32_t*)ptr;
    size_t wordCount = size / sizeof(uint32_t);
    
    for (size_t i = 0; i < wordCount; i++) {
        if (words[i] == 0xDEADBEEF) {
            printf("MEM_DEBUG: Detected use-after-free! Memory at %p contains 0xDEADBEEF pattern\n", ptr);
            return false;
        }
    }
    return true;
}

#endif // MEM_DEBUG

MemRef MemRef::Null = MemRef();

// MemPool implementation
MemPool::MemPool() : blocks(nullptr), blockCount(1), capacity(0) {
    // Start with initial capacity
    capacity = 256;
    blocks = (Block*)malloc(capacity * sizeof(Block));
    if (blocks) {
        memset(blocks, 0, capacity * sizeof(Block));
    }
    // And note that blockCount is initialized to 1, because
    // slot 0 is reserved to represent nullptr.
}

MemPool::~MemPool() {
#if MEM_DEBUG
	if (blocks == nullptr) printf("WHOOPS!  blocks is null in ~MemPool; double delete?");
#endif
    clear();
    ::free(blocks);
	blocks = nullptr;
}

uint32_t MemPool::allocateBlockSlot() {
    // Find a free slot
    for (uint32_t i = 1; i < blockCount; i++) {  // Start at 1 (0 is reserved for null)
        if (!blocks[i].inUse) {
            return i;
        }
    }
    
    // Need to grow
    if (blockCount >= capacity) {
        uint32_t newCapacity = capacity * 2;
        if (newCapacity > MAX_BLOCKS) newCapacity = MAX_BLOCKS;
        if (newCapacity <= capacity) return 0; // Out of slots
        
        Block* newBlocks = (Block*)::realloc(blocks, newCapacity * sizeof(Block));
        if (!newBlocks) return 0;
        
        // Initialize new slots
        memset(newBlocks + capacity, 0, (newCapacity - capacity) * sizeof(Block));
        
        blocks = newBlocks;
        capacity = newCapacity;
    }
    
    return blockCount++;
}

uint32_t MemPool::alloc(size_t size) {
    if (size == 0) return 0;
    
    uint32_t index = allocateBlockSlot();
    if (index == 0) return 0;
    
    void* ptr = malloc(size);
    if (!ptr) return 0;

	blocks[index].ptr = ptr;
    blocks[index].size = size;
    blocks[index].inUse = true;
    
    return index;
}

uint32_t MemPool::realloc(uint32_t index, size_t newSize) {
    if (index == 0 || index >= blockCount || !blocks[index].inUse) {
        // Invalid index - allocate new block
        return alloc(newSize);
    }
    
    if (newSize == 0) {
        free(index);
        return 0;
    }
    
    void* newPtr = ::realloc(blocks[index].ptr, newSize);
    if (!newPtr) return 0;  // Realloc failed

    blocks[index].ptr = newPtr;
    blocks[index].size = newSize;
    
    return index;
}

void MemPool::free(uint32_t index) {
    if (index == 0 || index >= blockCount || !blocks[index].inUse) {
        return;  // Invalid or already freed
    }
    
    ::free(blocks[index].ptr);
    blocks[index].ptr = nullptr;
    blocks[index].size = 0;
    blocks[index].inUse = false;
}

uint32_t MemPool::adopt(void* ptr, size_t size) {
	uint32_t index = allocateBlockSlot();
	blocks[index].ptr = ptr;
	blocks[index].size = size;
	blocks[index].inUse = true;
	return index;
}

void* MemPool::getPtr(uint32_t index) const {
    if (index == 0 || index >= blockCount || !blocks[index].inUse) {
        return nullptr;
    }
    
#if MEM_DEBUG
    // Validate that the memory hasn't been corrupted
    if (!debugValidateMemory(blocks[index].ptr, blocks[index].size)) {
        printf("MEM_DEBUG: Validation failed for block %u at %p\n", index, blocks[index].ptr);
    }
#endif
    
    return blocks[index].ptr;
}

size_t MemPool::getSize(uint32_t index) const {
    if (index == 0 || index >= blockCount || !blocks[index].inUse) {
        return 0;
    }
    return blocks[index].size;
}

void MemPool::clear() {
	if (blockCount <= 1) return;  // Already cleared (only null block remains)
    
    for (uint32_t i = 1; i < blockCount; i++) {
        if (blocks[i].inUse) {
            ::free(blocks[i].ptr);
            blocks[i].ptr = nullptr;
            blocks[i].size = 0;
            blocks[i].inUse = false;
        }
    }
    blockCount = 1;  // Keep slot 0 as null
}

size_t MemPool::getTotalMemory() const {
    size_t total = 0;
    for (uint32_t i = 1; i < blockCount; i++) {
        if (blocks[i].inUse) {
            total += blocks[i].size;
        }
    }
    return total;
}

// MemPoolManager static members
MemPool* MemPoolManager::pools[256] = {nullptr};

MemPool* MemPoolManager::getPool(uint8_t poolNum) {
    if (!pools[poolNum]) {
        pools[poolNum] = new MemPool();
    }
    return pools[poolNum];
}

void MemPoolManager::destroyPool(uint8_t poolNum) {
    if (pools[poolNum]) {
        delete pools[poolNum];
        pools[poolNum] = nullptr;
    }
}

MemRef MemPoolManager::alloc(size_t size, uint8_t poolNum) {
    MemPool* pool = getPool(poolNum);
    if (!pool) return MemRef();
    
    uint32_t index = pool->alloc(size);
    return MemRef(poolNum, index);
}

MemRef MemPoolManager::realloc(MemRef ref, size_t newSize) {
    MemPool* pool = getPool(ref.poolNum);
    if (!pool) return MemRef();
    
    uint32_t newIndex = pool->realloc(ref.index, newSize);
    return MemRef(ref.poolNum, newIndex);
}

void MemPoolManager::free(MemRef ref) {
    if (pools[ref.poolNum]) pools[ref.poolNum]->free(ref.index);
}

void* MemPoolManager::getPtr(MemRef ref) {
    if (!pools[ref.poolNum]) return nullptr;
    return pools[ref.poolNum]->getPtr(ref.index);
}

size_t MemPoolManager::getSize(MemRef ref) {
    if (!pools[ref.poolNum]) return 0;
    return pools[ref.poolNum]->getSize(ref.index);
}

void MemPoolManager::clearPool(uint8_t poolNum) {
    if (pools[poolNum]) pools[poolNum]->clear();
}

void MemPoolManager::destroyAllPools() {
    for (int i = 0; i < 256; i++) {
        destroyPool(i);
    }
}

uint8_t MemPoolManager::findUnusedPool() {
    // Start from 1 (reserve 0 for general use) and find first unused pool
    for (int i = 1; i < 256; i++) {
        if (pools[i] == nullptr) return (uint8_t)i;
    }
    return 0; // All pools in use
}
