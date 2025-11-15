// Basic test for string pool memory management
#include "core_includes.h"
#include <cstring>
#include <cstdio>

int main() {
    printf("String Pool Memory Management Debug Test\n");
    printf("MEM_DEBUG is %s\n", MEM_DEBUG ? "ENABLED" : "DISABLED");
    printf("==========================================\n");
    
    printf("\n=== Testing Basic String Creation ===\n");
    
    // Create strings in different pools
    String s1("Hello", 0);
    String s2("World", 1);
    String s3("Test", 2);
    
    printf("Created strings:\n");
    printf("s1 (pool %d): '%s'\n", s1.getPoolNum(), s1.c_str());
    printf("s2 (pool %d): '%s'\n", s2.getPoolNum(), s2.c_str());
    printf("s3 (pool %d): '%s'\n", s3.getPoolNum(), s3.c_str());
    
    printf("\n=== Testing Pool Draining ===\n");
    printf("Draining pool 1...\n");
    StringPool::clearPool(1);
    
    printf("Accessing strings after draining pool 1:\n");
    printf("s1 (pool %d): (should still work)\n", s1.getPoolNum());
    const char* s1_str = s1.c_str();
    if (s1_str) printf("  s1 content: '%s'\n", s1_str);
    
    printf("s2 (pool %d): (may show debug warnings)\n", s2.getPoolNum());
    const char* s2_str = s2.c_str();
    if (s2_str) printf("  s2 content: '%s'\n", s2_str);
    
    printf("s3 (pool %d): (should still work)\n", s3.getPoolNum());
    const char* s3_str = s3.c_str();
    if (s3_str) printf("  s3 content: '%s'\n", s3_str);
    
    printf("\n=== Cleanup ===\n");
    MemPoolManager::destroyAllPools();
    
    printf("Test completed\n");
    return 0;
}