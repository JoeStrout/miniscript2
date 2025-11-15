#include "core_includes.h"
#include "CS_String.h"
#include "StringPool.h"
#include "MemPool.h"
#include <cstdio>
#include <cstring>

static int testCount = 0;
static int passCount = 0;

#define TEST(condition, name) do { \
    testCount++; \
    if (condition) { \
        printf("✓ PASS: %s\n", name); \
        passCount++; \
    } else { \
        printf("✗ FAIL: %s\n", name); \
    } \
} while(0)

void testBasicConstructors() {
    printf("\n=== Testing Basic Constructors ===\n");

    // Default constructor
    String s1;
    TEST(strcmp(s1.c_str(), "") == 0, "Default constructor creates empty string");
    TEST(s1.Length() == 0, "Default constructor length is 0");
    TEST(s1.Empty(), "Default constructor isEmpty");

    // C string constructor
    String s2("Hello");
    TEST(strcmp(s2.c_str(), "Hello") == 0, "C string constructor");
    TEST(s2.Length() == 5, "C string constructor length");
    TEST(!s2.Empty(), "C string constructor not empty");

    // C string constructor with pool
    String s3("World", 2);
    TEST(strcmp(s3.c_str(), "World") == 0, "C string constructor with pool");
    TEST(s3.getPoolNum() == 2, "C string constructor pool number");

    // Copy constructor
    String s4(s2);
    TEST(strcmp(s4.c_str(), "Hello") == 0, "Copy constructor");
    TEST(s4.Length() == s2.Length(), "Copy constructor length");

    // Assignment operator
    String s5;
    s5 = s2;
    TEST(strcmp(s5.c_str(), "Hello") == 0, "Assignment operator");

    // C string assignment
    String s6;
    s6 = "Assignment";
    TEST(strcmp(s6.c_str(), "Assignment") == 0, "C string assignment");
}

void testStringOperations() {
    printf("\n=== Testing String Operations ===\n");

    String s1("Hello");
    String s2("World");
    String empty;

    // Length operations
    TEST(s1.lengthB() == 5, "lengthB()");
    TEST(s1.lengthC() == 5, "lengthC()");
    TEST(s1.Length() == 5, "Length()");

    // Character access
    TEST(s1[0] == 'H', "Character access [0]");
    TEST(s1[4] == 'o', "Character access [4]");

    // c_str()
    TEST(strcmp(s1.c_str(), "Hello") == 0, "c_str()");
    TEST(strcmp(empty.c_str(), "") == 0, "Empty string c_str()");
}

void testConcatenation() {
    printf("\n=== Testing Concatenation ===\n");

    String s1("Hello");
    String s2(" ");
    String s3("World");

    // operator+
    String result1 = s1 + s2;
    TEST(strcmp(result1.c_str(), "Hello ") == 0, "String + String");

    String result2 = s1 + s2 + s3;
    TEST(strcmp(result2.c_str(), "Hello World") == 0, "Chained concatenation");

    // operator+=
    String s4("Hello");
    s4 += s2;
    TEST(strcmp(s4.c_str(), "Hello ") == 0, "String += String");

    s4 += s3;
    TEST(strcmp(s4.c_str(), "Hello World") == 0, "Chained += operator");

    // C string + String
    String result3 = "Prefix: " + s1;
    TEST(strcmp(result3.c_str(), "Prefix: Hello") == 0, "C string + String");
}

void testComparison() {
    printf("\n=== Testing Comparison ===\n");

    String s1("Hello");
    String s2("Hello");
    String s3("World");
    String s4("ABC");

    // Equality
    TEST(s1 == s2, "Equal strings ==");
    TEST(!(s1 == s3), "Different strings == false");
    TEST(s1 != s3, "Different strings !=");
    TEST(!(s1 != s2), "Equal strings != false");

    // Ordering
    TEST(s4 < s1, "ABC < Hello");
    TEST(s1 < s3, "Hello < World");
    TEST(s4 <= s1, "ABC <= Hello");
    TEST(s1 <= s2, "Hello <= Hello");
    TEST(s3 > s1, "World > Hello");
    TEST(s1 > s4, "Hello > ABC");
    TEST(s3 >= s1, "World >= Hello");
    TEST(s1 >= s2, "Hello >= Hello");
}

void testSearchMethods() {
    printf("\n=== Testing Search Methods ===\n");

    String text("Hello World Hello");
    String needle("Hello");
    String notFound("XYZ");

    // IndexOf
    TEST(text.IndexOf(needle) == 0, "IndexOf first occurrence");
    TEST(text.IndexOf(needle, 1) == 12, "IndexOf from index");
    TEST(text.IndexOf(notFound) == -1, "IndexOf not found");
    TEST(text.IndexOf('o') == 4, "IndexOf char");
    TEST(text.IndexOf('o', 5) == 7, "IndexOf char from index");

    // LastIndexOf
    TEST(text.LastIndexOf(needle) == 12, "LastIndexOf");
    TEST(text.LastIndexOf('o') == 15, "LastIndexOf char");
    TEST(text.LastIndexOf(notFound) == -1, "LastIndexOf not found");

    // Contains
    TEST(text.Contains(needle), "Contains found");
    TEST(!text.Contains(notFound), "Contains not found");

    // StartsWith / EndsWith
    TEST(text.StartsWith(String("Hello")), "StartsWith true");
    TEST(!text.StartsWith(String("World")), "StartsWith false");
    TEST(text.EndsWith(String("Hello")), "EndsWith true");
    TEST(!text.EndsWith(String("World")), "EndsWith false");
}

void testSubstringMethods() {
    printf("\n=== Testing Substring Methods ===\n");

    String text("Hello World");

    // Substring
    String sub1 = text.Substring(6);
    TEST(strcmp(sub1.c_str(), "World") == 0, "Substring from index");

    String sub2 = text.Substring(0, 5);
    TEST(strcmp(sub2.c_str(), "Hello") == 0, "Substring with length");

    String sub3 = text.Substring(6, 5);
    TEST(strcmp(sub3.c_str(), "World") == 0, "Substring middle portion");

    // Left / Right
    String left = text.Left(5);
    TEST(strcmp(left.c_str(), "Hello") == 0, "Left substring");

    String right = text.Right(5);
    TEST(strcmp(right.c_str(), "World") == 0, "Right substring");

    String rightTooLong = text.Right(20);
    TEST(strcmp(rightTooLong.c_str(), "Hello World") == 0, "Right too long returns whole string");
}

void testManipulationMethods() {
    printf("\n=== Testing Manipulation Methods ===\n");

    String base("Hello World");

    // Insert
    String inserted = base.Insert(5, String(" Beautiful"));
    TEST(strcmp(inserted.c_str(), "Hello Beautiful World") == 0, "Insert string");

    // Remove
    String removed1 = base.Remove(5);
    TEST(strcmp(removed1.c_str(), "Hello") == 0, "Remove from index to end");

    String removed2 = base.Remove(5, 6);
    TEST(strcmp(removed2.c_str(), "Hello") == 0, "Remove count characters");

    // Replace
    String replaced1 = base.Replace(String("World"), String("Universe"));
    TEST(strcmp(replaced1.c_str(), "Hello Universe") == 0, "Replace string");

    String replaced2 = base.Replace('o', 'a');
    TEST(strcmp(replaced2.c_str(), "Hella Warld") == 0, "Replace character");
}

void testCaseConversion() {
    printf("\n=== Testing Case Conversion ===\n");

    String mixed("Hello WORLD");

    String lower = mixed.ToLower();
    TEST(strcmp(lower.c_str(), "hello world") == 0, "ToLower");

    String upper = mixed.ToUpper();
    TEST(strcmp(upper.c_str(), "HELLO WORLD") == 0, "ToUpper");
}

void testTrimming() {
    printf("\n=== Testing Trimming ===\n");

    String padded("  Hello World  ");
    String leftPad("  Hello World");
    String rightPad("Hello World  ");

    String trimmed = padded.Trim();
    TEST(strcmp(trimmed.c_str(), "Hello World") == 0, "Trim both sides");

    String trimStart = padded.TrimStart();
    TEST(strcmp(trimStart.c_str(), "Hello World  ") == 0, "TrimStart");

    String trimEnd = padded.TrimEnd();
    TEST(strcmp(trimEnd.c_str(), "  Hello World") == 0, "TrimEnd");
}

void testSplitting() {
    printf("\n=== Testing Splitting ===\n");

    String csv("apple,banana,cherry");
    String spaced("one two three four");

    // Split by character
    int count1 = 0;
    String* parts1 = csv.Split(',', &count1);
    TEST(count1 == 3, "Split by comma count");
    if (parts1 && count1 >= 3) {
        TEST(strcmp(parts1[0].c_str(), "apple") == 0, "Split part 0");
        TEST(strcmp(parts1[1].c_str(), "banana") == 0, "Split part 1");
        TEST(strcmp(parts1[2].c_str(), "cherry") == 0, "Split part 2");
    }
    free(parts1);

    // Split by string
    int count2 = 0;
    String* parts2 = spaced.Split(String(" "), &count2);
    TEST(count2 == 4, "Split by space string count");
    if (parts2 && count2 >= 4) {
        TEST(strcmp(parts2[0].c_str(), "one") == 0, "Split by string part 0");
        TEST(strcmp(parts2[3].c_str(), "four") == 0, "Split by string part 3");
    }
    free(parts2);

    // SplitToList
    auto list1 = csv.SplitToList(',');
    TEST(list1.Count() == 3, "SplitToList count");
    if (list1.Count() >= 3) {
        TEST(strcmp(list1[0].c_str(), "apple") == 0, "SplitToList part 0");
        TEST(strcmp(list1[2].c_str(), "cherry") == 0, "SplitToList part 2");
    }
}

void testStaticMethods() {
    printf("\n=== Testing Static Methods ===\n");

    String empty;
    String notEmpty("Hello");
    String nullStr; // Simulating null

    // IsNullOrEmpty
    TEST(String::IsNullOrEmpty(empty), "IsNullOrEmpty with empty");
    TEST(!String::IsNullOrEmpty(notEmpty), "IsNullOrEmpty with content");

    // Join
    String parts[] = {String("apple"), String("banana"), String("cherry")};
    String joined = String::Join(String(", "), parts, 3);
    TEST(strcmp(joined.c_str(), "apple, banana, cherry") == 0, "Join array");
}

void testEdgeCases() {
    printf("\n=== Testing Edge Cases ===\n");

    String empty;
    String single("A");
    String normal("Hello");

    // Operations with empty strings
    String emptyConcat = empty + normal;
    TEST(strcmp(emptyConcat.c_str(), "Hello") == 0, "Empty + normal");

    String normalEmpty = normal + empty;
    TEST(strcmp(normalEmpty.c_str(), "Hello") == 0, "Normal + empty");

    // Boundary substring operations
    String subEmpty = normal.Substring(5);
    TEST(strcmp(subEmpty.c_str(), "") == 0, "Substring at end");

    String leftZero = normal.Left(0);
    TEST(strcmp(leftZero.c_str(), "") == 0, "Left(0)");

    String rightZero = normal.Right(0);
    TEST(strcmp(rightZero.c_str(), "") == 0, "Right(0)");

    // Search in empty string
    TEST(empty.IndexOf(String("test")) == -1, "IndexOf in empty string");
    TEST(empty.Contains(String("test")) == false, "Contains in empty string");
}

void testPoolManagement() {
    printf("\n=== Testing Pool Management ===\n");

    // Test different pools
    String s1("Pool0", 0);
    String s2("Pool1", 1);
    String s3("Pool2", 2);

    TEST(s1.getPoolNum() == 0, "String in pool 0");
    TEST(s2.getPoolNum() == 1, "String in pool 1");
    TEST(s3.getPoolNum() == 2, "String in pool 2");

    // Test pool clearing
    StringPool::clearPool(1);

    // After clearing pool 1, s2 should still be accessible but might show warnings
    const char* s2_after = s2.c_str();
    TEST(s2_after != nullptr, "String accessible after pool clear");

    // Create new string in cleared pool
    String s4("NewInPool1", 1);
    TEST(strcmp(s4.c_str(), "NewInPool1") == 0, "New string in cleared pool");
    TEST(s4.getPoolNum() == 1, "New string pool number");
}

int main() {
    printf("CS_String Comprehensive Test Suite\n");
    printf("MEM_DEBUG is %s\n", MEM_DEBUG ? "ENABLED" : "DISABLED");
    printf("=====================================\n");

    testBasicConstructors();
    testStringOperations();
    testConcatenation();
    testComparison();
    testSearchMethods();
    testSubstringMethods();
    testManipulationMethods();
    testCaseConversion();
    testTrimming();
    testSplitting();
    testStaticMethods();
    testEdgeCases();
    testPoolManagement();

    printf("\n=== Test Results ===\n");
    printf("Total tests: %d\n", testCount);
    printf("Passed: %d\n", passCount);
    printf("Failed: %d\n", testCount - passCount);
    printf("Success rate: %.1f%%\n", testCount > 0 ? (100.0 * passCount / testCount) : 0.0);

    if (passCount == testCount) {
        printf("🎉 ALL TESTS PASSED!\n");
    } else {
        printf("❌ Some tests failed.\n");
    }

    printf("\n--- Final Cleanup ---\n");
    MemPoolManager::destroyAllPools();

    return (passCount == testCount) ? 0 : 1;
}