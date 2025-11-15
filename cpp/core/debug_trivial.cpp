// Even more trivial test - just create one string
#include "core_includes.h"
#include <cstdio>

int main() {
    printf("Debug trivial test starting...\n");
    
    // Create a string in default pool (0)
    String s1("Hello");
    printf("Created string in default pool: '%s'\n", s1.c_str());
    
    // Create a string in pool 1
    printf("Creating string in pool 1...\n");
    String s2("World", 1);
    printf("Created string in pool 1: '%s'\n", s2.c_str());
    
    printf("\nDraining pool 1...\n");
    MemPoolManager::clearPool(1);
    
    printf("Accessing strings after draining:\n");
    printf("s1 (pool 0): '%s' (should work)\n", s1.c_str());
    printf("s2 (pool 1): '%s' (may not work)\n", s2.c_str());
    
    printf("Test completed\n");
    return 0;
}