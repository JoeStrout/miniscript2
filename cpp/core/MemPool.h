// MemPool -- a memory management system for "host" code, e.g. the
// compiler, assembler, disassembler, main program, and anything else
// outside of the actual MiniScript runtime and VM.
//
// The basic idea is: allocations happen within a "pool" (0-255), and are
// never released, until the whole pool is explicitly drained (released)
// at once.  So for example, when we go to compile a file, we might switch
// to a pool for that process, and then drain it after compilation is 
// complete.

#pragma once
#include <cstdint>
#include <cstdlib>

// This module is part of Layer 2B (Host Memory Management)
#define CORE_LAYER_2B

// MemPool reference - trivially copyable handle
struct __attribute__((packed)) MemRef {
    uint8_t poolNum;    // Pool number (0-255)
    uint32_t index;     // Index within the pool (32-bit for larger pools)
    
    MemRef() : poolNum(0), index(0) {}
    MemRef(uint8_t pool, uint32_t idx) : poolNum(pool), index(idx) {}
    
    bool isNull() const { return index == 0; }
    
    // Comparison operators for containers
    // ToDo: replace this with `= default`?
    bool operator==(const MemRef& other) const {
        return poolNum == other.poolNum && index == other.index;
    }
    bool operator!=(const MemRef& other) const {
        return !(*this == other);
    }
	
	static MemRef Null;
};

// MemPool - manages memory blocks within a single pool
class MemPool {
private:
    struct Block {
        void* ptr;
        size_t size;
        bool inUse;
    };
    
    static const uint32_t MAX_BLOCKS = 65536;  // at most 64K blocks per pool
    Block* blocks;			// dynamically-sized array of blocks
    uint32_t blockCount;	// how much of that array is actually in use
    uint32_t capacity;		// number of blocks allocated in that array
    
    uint32_t allocateBlockSlot();
    
public:
    MemPool();
    ~MemPool();
    
    // Allocate a block and return its index
    uint32_t alloc(size_t size);
    
    // Reallocate a block (similar to realloc)
    uint32_t realloc(uint32_t index, size_t newSize);
    
    // Free a specific block
    void free(uint32_t index);
    
	// Adopt a block of memory allocated externally by malloc; return its index
	uint32_t adopt(void* ptr, size_t size);
	
    // Get pointer from index (returns nullptr if invalid)
    void* getPtr(uint32_t index) const;
    
    // Get size of block
    size_t getSize(uint32_t index) const;
    
    // Clear all allocations (bulk free)
    void clear();
    
    // Statistics
    uint32_t getBlockCount() const { return blockCount; }
    size_t getTotalMemory() const;
};

// Global MemPool manager
class MemPoolManager {
private:
    static MemPool* pools[256];
    
public:
    // Create a new pool (or return existing one)
    static MemPool* getPool(uint8_t poolNum);
    
    // Destroy a pool and all its allocations
    static void destroyPool(uint8_t poolNum);
    
    // Global allocation/deallocation functions
    static MemRef alloc(size_t size, uint8_t poolNum = 0);
    static MemRef realloc(MemRef ref, size_t newSize);
    static void free(MemRef ref);
//	static void free(void* ptr);	// (assumes ptr was allocated by MemPool)
	
	// Inspectors
    static void* getPtr(MemRef ref);
    static size_t getSize(MemRef ref);
//	static MemRef getMemRef(void* ptr);	// (for ptr allocated by MemPool)

    // Utility functions
    static void clearPool(uint8_t poolNum);
    static void destroyAllPools();

    // Find an unused pool number (returns 0 if none available)
    static uint8_t findUnusedPool();
};
