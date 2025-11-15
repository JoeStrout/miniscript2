// Test program for string pooling memory management with debug features
// This program tests that strings are properly allocated/deallocated across different pools

#include "core_includes.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdio>

// Simple test framework
static int testCount = 0;
static int passCount = 0;

void TEST(const char* name, bool condition) {
    testCount++;
    if (condition) {
        passCount++;
        printf("✓ PASS: %s\n", name);
    } else {
        printf("✗ FAIL: %s\n", name);
    }
}

void testBasicStringCreation() {
    printf("\n=== Testing Basic String Creation ===\n");
    
    // Create strings in different pools
    String s1("Hello", 0);
    String s2("World", 1);
    String s3("Test", 2);
	
    TEST("String creation in pool 0", s1.c_str() != nullptr && strcmp(s1.c_str(), "Hello") == 0);
    TEST("String creation in pool 1", s2.c_str() != nullptr && strcmp(s2.c_str(), "World") == 0);
    TEST("String creation in pool 2", s3.c_str() != nullptr && strcmp(s3.c_str(), "Test") == 0);
    TEST("Pool numbers preserved", s1.getPoolNum() == 0 && s2.getPoolNum() == 1 && s3.getPoolNum() == 2);
}

void testStringPoolDraining() {
    printf("\n=== Testing String Pool Draining ===\n");
    
    // Create strings in pools 3, 4, and 5
    std::vector<String> pool3_strings;
    std::vector<String> pool4_strings;
    std::vector<String> pool5_strings;
    
    // Add several strings to each pool
    for (int i = 0; i < 5; i++) {
        char buffer[32];
        
        snprintf(buffer, sizeof(buffer), "Pool3_String_%d", i);
        pool3_strings.emplace_back(buffer, 3);
        
        snprintf(buffer, sizeof(buffer), "Pool4_String_%d", i);
        pool4_strings.emplace_back(buffer, 4);
        
        snprintf(buffer, sizeof(buffer), "Pool5_String_%d", i);
        pool5_strings.emplace_back(buffer, 5);
    }
    
    printf("Created 5 strings in each of pools 3, 4, and 5\n");
    
    // Verify all strings are accessible
    bool allStringsValid = true;
    for (const auto& s : pool3_strings) {
        if (!s.c_str() || s.Length() == 0) {
            allStringsValid = false;
            break;
        }
    }
    for (const auto& s : pool4_strings) {
        if (!s.c_str() || s.Length() == 0) {
            allStringsValid = false;
            break;
        }
    }
    for (const auto& s : pool5_strings) {
        if (!s.c_str() || s.Length() == 0) {
            allStringsValid = false;
            break;
        }
    }
    TEST("All strings initially valid", allStringsValid);
    
    // Now drain pool 4
    printf("\n--- Draining Pool 4 ---\n");
    StringPool::clearPool(4);
    
    // Verify pool 3 and 5 strings are still valid
    bool pool3_valid = true;
    bool pool5_valid = true;
    
    for (const auto& s : pool3_strings) {
        if (!s.c_str() || s.Length() == 0) {
            pool3_valid = false;
            break;
        }
    }
    for (const auto& s : pool5_strings) {
        if (!s.c_str() || s.Length() == 0) {
            pool5_valid = false;
            break;
        }
    }
    
    TEST("Pool 3 strings still valid after draining pool 4", pool3_valid);
    TEST("Pool 5 strings still valid after draining pool 4", pool5_valid);
    
    printf("Pool 3 sample string: '%s'\n", pool3_strings[0].c_str());
    printf("Pool 5 sample string: '%s'\n", pool5_strings[0].c_str());
    
    // Verify pool 4 strings are no longer accessible (should return empty strings)
    // Note: This is a tricky test since the String objects still exist but their underlying
    // storage has been freed. In a robust implementation, accessing them should either
    // return null/empty or be detected by our debug system.
    
    printf("\n--- Draining Pool 3 ---\n");
    StringPool::clearPool(3);
    
    // Verify pool 5 is still valid
    pool5_valid = true;
    for (const auto& s : pool5_strings) {
        if (!s.c_str() || s.Length() == 0) {
            pool5_valid = false;
            break;
        }
    }
    TEST("Pool 5 strings still valid after draining pool 3", pool5_valid);
    
    printf("\n--- Draining Pool 5 ---\n");
    StringPool::clearPool(5);
    
    printf("All test pools have been drained\n");
}

void testStringOperationsAfterDrain() {
    printf("\n=== Testing String Operations After Pool Drain ===\n");
    
    // Create strings in pool 6
    String s1("Hello", 6);
	String::defaultPool = 6;
    String s2("World");
	String s3 = s1 + " " + s2;  // Should create in pool 6
	const char* s3str = s3.c_str();
	
    printf("Created strings: '%s', '%s', '%s'\n", s1.c_str(), s2.c_str(), s3str);
    TEST("String concatenation works", s3.c_str() != nullptr && strcmp(s3.c_str(), "Hello World") == 0);
    
    // Now drain the pool
    printf("\n--- Draining Pool 6 ---\n");
    StringPool::clearPool(6);
    
    // Try to use the strings - this should trigger debug warnings if MEM_DEBUG is on
    printf("Attempting to access drained strings (should return empty strings):\n");
    
    // These accesses should be detected by our debug system
	const char* c1 = s1.c_str();
	const char* c2 = s2.c_str();
	const char* c3 = s3.c_str();
    
    printf("Access results: s1='%s', s2='%s', s3='%s'\n", 
           c1 ? c1 : "NULL", c2 ? c2 : "NULL", c3 ? c3 : "NULL");
    
    // Create new strings in the same pool to verify it can be reused
    String s4("Reused", 6);
    String s5("Pool", 6);
    TEST("Pool can be reused after draining", s4.c_str() != nullptr && s5.c_str() != nullptr);
    printf("New strings in reused pool: '%s', '%s'\n", s4.c_str(), s5.c_str());
}

void testMultiplePoolOperations() {
    printf("\n=== Testing Multiple Pool Operations ===\n");
    
    // Create a mix of operations across multiple pools
    std::vector<String> strings;
    
    // Pool 10: Basic strings
    strings.emplace_back("Base1", 10);
    strings.emplace_back("Base2", 10); 
    strings.emplace_back("Base3", 10);
    
    // Pool 11: Concatenated strings
    String temp1("Concat", 11);
    String temp2("Test", 11);
    strings.push_back(temp1 + temp2);
    strings.push_back(temp1 + " " + temp2);
    
    // Pool 12: Substring operations
    String source("This is a test string for substrings", 12);
    strings.push_back(source.Substring(5, 4)); // "is a"
    strings.push_back(source.Substring(10));   // "test string for substrings"
    
    printf("Created strings across pools 10, 11, 12:\n");
    for (size_t i = 0; i < strings.size(); i++) {
        printf("  [%zu] Pool %d: '%s'\n", i, strings[i].getPoolNum(), strings[i].c_str());
    }
    
    // Drain pool 11
    printf("\n--- Draining Pool 11 ---\n");
    StringPool::clearPool(11);
    
    // Verify pools 10 and 12 are still intact
    bool pools_10_12_valid = true;
    for (const auto& s : strings) {
        if (s.getPoolNum() == 10 || s.getPoolNum() == 12) {
            if (!s.c_str() || s.Length() == 0) {
                pools_10_12_valid = false;
                printf("String in pool %d became invalid: '%s'\n", s.getPoolNum(), s.c_str());
            }
        }
    }
    TEST("Pools 10 and 12 remain valid after draining pool 11", pools_10_12_valid);
    
    // Clean up remaining pools
    StringPool::clearPool(10);
    StringPool::clearPool(12);
}

int main() {
    printf("String Pool Memory Management Debug Test\n");
    printf("MEM_DEBUG is %s\n", MEM_DEBUG ? "ENABLED" : "DISABLED");
    printf("==========================================\n");
    
    testBasicStringCreation();
    testStringPoolDraining();
    testStringOperationsAfterDrain();
    testMultiplePoolOperations();
    
    printf("\n=== Test Results ===\n");
    printf("Total tests: %d\n", testCount);
    printf("Passed: %d\n", passCount);
    printf("Failed: %d\n", testCount - passCount);
    printf("Success rate: %.1f%%\n", testCount > 0 ? (100.0 * passCount / testCount) : 0.0);
    
    // Clean up any remaining pools
    printf("\n--- Final Cleanup ---\n");
    MemPoolManager::destroyAllPools();
    
    return (passCount == testCount) ? 0 : 1;
}
