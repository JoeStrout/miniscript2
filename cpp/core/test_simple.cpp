// Simple test program to verify basic functionality
#include "core_includes.h"
#include <cstdio>

int main() {
    printf("Simple test starting...\n");
    printf("MEM_DEBUG is %s\n", MEM_DEBUG ? "ENABLED" : "DISABLED");
    
    // Test basic string creation
    String s1("Hello");
    printf("Created string: '%s'\n", s1.c_str());
    
    printf("Test completed successfully\n");
    return 0;
}