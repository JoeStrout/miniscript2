// Trivial test - just include headers and print
#include "core_includes.h"
#include <cstdio>

int main() {
    printf("Trivial test starting...\n");
    printf("MEM_DEBUG is %d\n", MEM_DEBUG);
    
    // Test basic MemPool operations without String
    MemRef ref = MemPoolManager::alloc(100, 0);
    if (ref.isNull()) {
        printf("Memory allocation failed\n");
        return 1;
    }
    printf("Allocated 100 bytes in pool 0\n");
    
    void* ptr = MemPoolManager::getPtr(ref);
    printf("Got pointer: %p\n", ptr);
    
    MemPoolManager::free(ref);
    printf("Freed memory\n");
    
    printf("Test completed\n");
    return 0;
}